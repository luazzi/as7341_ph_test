/*
 * AS7341 - Driver para Sensor Espectral de 11 Canais
 * Raspberry Pi Pico (Pico SDK, C nativo)
 *
 * Implementação baseada na biblioteca Adafruit AS7341 (BSD License)
 * e no datasheet ams OSRAM AS7341 (DS000504)
 *
 * Adaptado para uso direto com hardware_i2c do Pico SDK
 */

#include "as7341.h"
#include <string.h>
#include "pico/stdlib.h"

// ============================================================================
// Funções internas de acesso I2C
// ============================================================================

/**
 * @brief Escreve um byte em um registrador do AS7341
 */
static bool as7341_write_reg(as7341_t *dev, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    int ret = i2c_write_blocking(dev->i2c, dev->addr, buf, 2, false);
    return (ret == 2);
}

/**
 * @brief Lê um byte de um registrador do AS7341
 */
static bool as7341_read_reg(as7341_t *dev, uint8_t reg, uint8_t *value) {
    int ret = i2c_write_blocking(dev->i2c, dev->addr, &reg, 1, true);
    if (ret != 1) return false;
    ret = i2c_read_blocking(dev->i2c, dev->addr, value, 1, false);
    return (ret == 1);
}

/**
 * @brief Lê múltiplos bytes consecutivos a partir de um registrador
 */
static bool as7341_read_regs(as7341_t *dev, uint8_t reg, uint8_t *buf, uint8_t len) {
    int ret = i2c_write_blocking(dev->i2c, dev->addr, &reg, 1, true);
    if (ret != 1) return false;
    ret = i2c_read_blocking(dev->i2c, dev->addr, buf, len, false);
    return (ret == len);
}

/**
 * @brief Modifica bits específicos de um registrador (read-modify-write)
 */
static bool as7341_modify_reg(as7341_t *dev, uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t current;
    if (!as7341_read_reg(dev, reg, &current)) return false;
    current = (current & ~mask) | (value & mask);
    return as7341_write_reg(dev, reg, current);
}

// ============================================================================
// Controle de energia e medição
// ============================================================================

/**
 * @brief Habilita/desabilita o power on (PON)
 */
static bool as7341_power_enable(as7341_t *dev, bool enable) {
    return as7341_modify_reg(dev, AS7341_ENABLE, AS7341_ENABLE_PON,
                             enable ? AS7341_ENABLE_PON : 0);
}

/**
 * @brief Habilita/desabilita a medição espectral (SP_EN)
 */
static bool as7341_enable_spectral_measurement(as7341_t *dev, bool enable) {
    return as7341_modify_reg(dev, AS7341_ENABLE, AS7341_ENABLE_SP_EN,
                             enable ? AS7341_ENABLE_SP_EN : 0);
}

/**
 * @brief Seleciona o banco de registradores (0 ou 1)
 *        Bank 0: registradores >= 0x80 (padrão)
 *        Bank 1: registradores 0x60-0x74 (CONFIG, LED, etc.)
 */
static bool as7341_set_register_bank(as7341_t *dev, bool bank1) {
    // Bit 4 (REG_BANK) do CFG0 (0xA9): 0=banco padrão, 1=banco 0x60-0x74
    return as7341_modify_reg(dev, AS7341_CFG0, 0x10,
                             bank1 ? 0x10 : 0x00);
}

/**
 * @brief Verifica se os dados estão prontos
 */
static bool as7341_is_data_ready(as7341_t *dev) {
    uint8_t status2;
    if (!as7341_read_reg(dev, AS7341_STATUS2, &status2)) return false;
    // Bit 6 = AVALID (spectral data valid)
    return (status2 & 0x40) != 0;
}

/**
 * @brief Aguarda os dados ficarem prontos (com timeout)
 */
static bool as7341_wait_for_data(as7341_t *dev, uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (!as7341_is_data_ready(dev)) {
        if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) {
            return false;  // Timeout
        }
        sleep_ms(1);
    }
    return true;
}

// ============================================================================
// Configuração SMUX
// ============================================================================

/**
 * @brief Configura o SMUX para ler os canais baixos: F1, F2, F3, F4, Clear, NIR
 *
 * Mapeamento (baseado na Adafruit_AS7341):
 *   ADC0 = F1 (415nm)
 *   ADC1 = F2 (445nm)
 *   ADC2 = F3 (480nm)
 *   ADC3 = F4 (515nm)
 *   ADC4 = Clear
 *   ADC5 = NIR
 */
static bool as7341_setup_f1f4_clear_nir(as7341_t *dev) {
    // Sequência de 20 bytes para os registradores SMUX (0x00-0x13)
    // Valores da Adafruit AS7341 setup_F1F4_Clear_NIR
    uint8_t smux_config[20] = {
        0x30,  // 0x00: F3 left -> ADC2
        0x01,  // 0x01: F1 left -> ADC0
        0x00,  // 0x02: (desabilitado)
        0x00,  // 0x03: (desabilitado)
        0x00,  // 0x04: (desabilitado)
        0x42,  // 0x05: F4 left -> ADC3, F2 left -> ADC1
        0x00,  // 0x06: (desabilitado)
        0x00,  // 0x07: (desabilitado)
        0x50,  // 0x08: Clear -> ADC4
        0x00,  // 0x09: (desabilitado)
        0x00,  // 0x0A: (desabilitado)
        0x00,  // 0x0B: (desabilitado)
        0x20,  // 0x0C: F2 right -> ADC1
        0x04,  // 0x0D: F4 right -> ADC3
        0x00,  // 0x0E: (desabilitado)
        0x30,  // 0x0F: F3 right -> ADC2
        0x01,  // 0x10: F1 right -> ADC0
        0x50,  // 0x11: Clear -> ADC4 (duplicado)
        0x00,  // 0x12: (desabilitado)
        0x06   // 0x13: NIR -> ADC5
    };

    // Escreve o comando SMUX_WRITE no CFG6
    if (!as7341_write_reg(dev, AS7341_CFG6, 0x10)) return false;

    // Escreve cada byte de configuração nos registradores SMUX
    for (int i = 0; i < 20; i++) {
        if (!as7341_write_reg(dev, (uint8_t)i, smux_config[i])) return false;
    }

    // Ativa o SMUX (SMUXEN) - bit auto-limpante
    if (!as7341_modify_reg(dev, AS7341_ENABLE, AS7341_ENABLE_SMUXEN, AS7341_ENABLE_SMUXEN))
        return false;

    // Aguarda o SMUXEN limpar (config aplicada)
    for (int i = 0; i < 100; i++) {
        uint8_t enable_val;
        if (!as7341_read_reg(dev, AS7341_ENABLE, &enable_val)) return false;
        if (!(enable_val & AS7341_ENABLE_SMUXEN)) return true;
        sleep_ms(1);
    }
    return false;  // Timeout
}

/**
 * @brief Configura o SMUX para ler os canais altos: F5, F6, F7, F8, Clear, NIR
 *
 * Mapeamento:
 *   ADC0 = F5 (555nm)
 *   ADC1 = F6 (590nm)
 *   ADC2 = F7 (630nm)
 *   ADC3 = F8 (680nm)
 *   ADC4 = Clear
 *   ADC5 = NIR
 */
static bool as7341_setup_f5f8_clear_nir(as7341_t *dev) {
    uint8_t smux_config[20] = {
        0x00,  // 0x00: (desabilitado)
        0x00,  // 0x01: (desabilitado)
        0x00,  // 0x02: (desabilitado)
        0x40,  // 0x03: F8 left -> ADC3
        0x02,  // 0x04: F6 left -> ADC1
        0x00,  // 0x05: (desabilitado)
        0x10,  // 0x06: F7 left -> ADC0 (nota: mapeado como ADC2 em algumas refs)
        0x03,  // 0x07: F5 left -> ADC0
        0x50,  // 0x08: Clear -> ADC4
        0x10,  // 0x09: F5 right -> ADC0
        0x03,  // 0x0A: F7 right -> ADC2
        0x00,  // 0x0B: (desabilitado)
        0x00,  // 0x0C: (desabilitado)
        0x00,  // 0x0D: (desabilitado)
        0x24,  // 0x0E: F8 right -> ADC3, F6 right -> ADC1
        0x00,  // 0x0F: (desabilitado)
        0x00,  // 0x10: (desabilitado)
        0x50,  // 0x11: Clear -> ADC4
        0x00,  // 0x12: (desabilitado)
        0x06   // 0x13: NIR -> ADC5
    };

    // Escreve o comando SMUX_WRITE no CFG6
    if (!as7341_write_reg(dev, AS7341_CFG6, 0x10)) return false;

    // Escreve a configuração SMUX
    for (int i = 0; i < 20; i++) {
        if (!as7341_write_reg(dev, (uint8_t)i, smux_config[i])) return false;
    }

    // Ativa o SMUX
    if (!as7341_modify_reg(dev, AS7341_ENABLE, AS7341_ENABLE_SMUXEN, AS7341_ENABLE_SMUXEN))
        return false;

    // Aguarda SMUXEN limpar
    for (int i = 0; i < 100; i++) {
        uint8_t enable_val;
        if (!as7341_read_reg(dev, AS7341_ENABLE, &enable_val)) return false;
        if (!(enable_val & AS7341_ENABLE_SMUXEN)) return true;
        sleep_ms(1);
    }
    return false;
}

// ============================================================================
// Funções públicas
// ============================================================================

bool as7341_init(as7341_t *dev, i2c_inst_t *i2c, uint8_t addr) {
    dev->i2c = i2c;
    dev->addr = addr;

    // Verifica o chip ID
    uint8_t id;
    if (!as7341_read_reg(dev, AS7341_WHOAMI, &id)) return false;

    // O chip ID é armazenado nos bits [7:2] do registro WHOAMI
    if ((id & 0xFC) != (AS7341_CHIP_ID << 2)) return false;

    // Liga o sensor (PON)
    if (!as7341_power_enable(dev, true)) return false;

    sleep_ms(10);  // Aguarda estabilizar

    return true;
}

bool as7341_set_atime(as7341_t *dev, uint8_t atime) {
    return as7341_write_reg(dev, AS7341_ATIME, atime);
}

bool as7341_set_astep(as7341_t *dev, uint16_t astep) {
    // ASTEP é 16 bits: byte baixo em 0xCA, byte alto em 0xCB
    if (!as7341_write_reg(dev, AS7341_ASTEP_L, (uint8_t)(astep & 0xFF)))
        return false;
    return as7341_write_reg(dev, AS7341_ASTEP_H, (uint8_t)(astep >> 8));
}

bool as7341_set_gain(as7341_t *dev, as7341_gain_t gain) {
    return as7341_write_reg(dev, AS7341_CFG1, (uint8_t)gain);
}

bool as7341_read_all_channels(as7341_t *dev, uint16_t *readings) {
    // Desabilita SP_EN antes de reconfigurar o SMUX
    if (!as7341_enable_spectral_measurement(dev, false)) return false;

    // --- Ciclo 1: F1-F4, Clear, NIR ---
    if (!as7341_setup_f1f4_clear_nir(dev)) return false;
    if (!as7341_enable_spectral_measurement(dev, true)) return false;

    // Aguarda os dados (timeout generoso de 2 segundos)
    if (!as7341_wait_for_data(dev, 2000)) return false;

    // Lê os 6 canais ADC (12 bytes: 6 x 16bit LE)
    uint8_t raw_low[12];
    if (!as7341_read_regs(dev, AS7341_CH0_DATA_L, raw_low, 12)) return false;

    // Converte para uint16_t (little-endian)
    for (int i = 0; i < 6; i++) {
        readings[i] = (uint16_t)raw_low[i * 2] | ((uint16_t)raw_low[i * 2 + 1] << 8);
    }

    // Desabilita SP_EN antes de reconfigurar
    if (!as7341_enable_spectral_measurement(dev, false)) return false;

    // --- Ciclo 2: F5-F8, Clear, NIR ---
    if (!as7341_setup_f5f8_clear_nir(dev)) return false;
    if (!as7341_enable_spectral_measurement(dev, true)) return false;

    if (!as7341_wait_for_data(dev, 2000)) return false;

    uint8_t raw_high[12];
    if (!as7341_read_regs(dev, AS7341_CH0_DATA_L, raw_high, 12)) return false;

    for (int i = 0; i < 6; i++) {
        readings[6 + i] = (uint16_t)raw_high[i * 2] | ((uint16_t)raw_high[i * 2 + 1] << 8);
    }

    return true;
}

bool as7341_enable_led(as7341_t *dev, bool enable) {
    // CONFIG (0x70) e LED (0x74) so sao acessiveis com REG_BANK = 1
    if (!as7341_set_register_bank(dev, true)) return false;

    // Primeiro, habilita o controle de LED via CONFIG (0x70), bit 3 (LED_SEL)
    bool ok = as7341_modify_reg(dev, AS7341_CONFIG, 0x08, enable ? 0x08 : 0x00);

    // Depois, liga/desliga o LED via registro LED (0x74), bit 7
    if (ok) ok = as7341_modify_reg(dev, AS7341_LED, 0x80, enable ? 0x80 : 0x00);

    // Sempre retorna ao banco padrao (necessario para leituras espectrais)
    if (!as7341_set_register_bank(dev, false)) return false;
    return ok;
}

bool as7341_set_led_current(as7341_t *dev, uint16_t current_ma) {
    // O registro LED (0x74) bits [6:0] controlam a corrente
    // Valor = (corrente_mA - 4) / 2, limitado a 0-127
    if (current_ma < 4) current_ma = 4;
    if (current_ma > 258) current_ma = 258;

    uint8_t reg_val = (uint8_t)((current_ma - 4) / 2);

    // Preserva bit 7 (LED on/off), escreve bits [6:0] (requer REG_BANK = 1)
    if (!as7341_set_register_bank(dev, true)) return false;
    bool ok = as7341_modify_reg(dev, AS7341_LED, 0x7F, reg_val);
    if (!as7341_set_register_bank(dev, false)) return false;
    return ok;
}
