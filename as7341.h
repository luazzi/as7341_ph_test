/*
 * AS7341 - Driver para Sensor Espectral de 11 Canais
 * Raspberry Pi Pico (Pico SDK, C nativo)
 *
 * Baseado na biblioteca Adafruit AS7341 (BSD License)
 * Adaptado para uso direto com hardware_i2c do Pico SDK
 */

#ifndef AS7341_H
#define AS7341_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

// ============================================================================
// Endereço I2C e identificação
// ============================================================================
#define AS7341_I2CADDR_DEFAULT  0x39
#define AS7341_CHIP_ID          0x09  // Bits [7:2] do registro WHOAMI

// ============================================================================
// Mapa de registradores
// ============================================================================
#define AS7341_ENABLE       0x80  // PON, SP_EN, SMUXEN, WEN, FDEN
#define AS7341_ATIME        0x81  // Contagem de passos de integração
#define AS7341_WTIME        0x83  // Tempo de espera entre medições
#define AS7341_CONFIG       0x70  // Modo de integração, LED_SEL
#define AS7341_LED          0x74  // Controle do LED integrado
#define AS7341_WHOAMI       0x92  // Chip ID (leitura)
#define AS7341_STATUS       0x93  // Status de interrupção / dados prontos
#define AS7341_STATUS2      0xA3  // Status adicional (data ready)
#define AS7341_CFG0         0xA9  // Configuração 0 (LOW_POWER, REG_BANK)
#define AS7341_CFG1         0xAA  // Ganho (AGAIN) bits [4:0]
#define AS7341_CFG6         0xAF  // Comando SMUX
#define AS7341_ASTEP_L      0xCA  // ASTEP byte baixo
#define AS7341_ASTEP_H      0xCB  // ASTEP byte alto
#define AS7341_CH0_DATA_L   0x95  // Início dos dados dos canais ADC

// ============================================================================
// Bits do registrador ENABLE (0x80)
// ============================================================================
#define AS7341_ENABLE_PON       (1 << 0)  // Power ON
#define AS7341_ENABLE_SP_EN     (1 << 1)  // Spectral measurement enable
#define AS7341_ENABLE_WEN       (1 << 3)  // Wait enable
#define AS7341_ENABLE_SMUXEN    (1 << 4)  // SMUX enable
#define AS7341_ENABLE_FDEN      (1 << 6)  // Flicker detection enable

// ============================================================================
// Valores de ganho (AGAIN) para CFG1
// ============================================================================
typedef enum {
    AS7341_GAIN_0_5X  = 0,   // 0.5x
    AS7341_GAIN_1X    = 1,   // 1x
    AS7341_GAIN_2X    = 2,   // 2x
    AS7341_GAIN_4X    = 3,   // 4x
    AS7341_GAIN_8X    = 4,   // 8x
    AS7341_GAIN_16X   = 5,   // 16x
    AS7341_GAIN_32X   = 6,   // 32x
    AS7341_GAIN_64X   = 7,   // 64x
    AS7341_GAIN_128X  = 8,   // 128x
    AS7341_GAIN_256X  = 9,   // 256x
    AS7341_GAIN_512X  = 10   // 512x
} as7341_gain_t;

// ============================================================================
// Estrutura do driver
// ============================================================================
typedef struct {
    i2c_inst_t *i2c;        // Instância I2C (i2c0 ou i2c1)
    uint8_t     addr;       // Endereço I2C do sensor
} as7341_t;

// ============================================================================
// Funções do driver
// ============================================================================

/**
 * @brief Inicializa o sensor AS7341
 * @param dev Ponteiro para a estrutura do driver
 * @param i2c Instância I2C (i2c0 ou i2c1)
 * @param addr Endereço I2C do sensor (normalmente AS7341_I2CADDR_DEFAULT)
 * @return true se o sensor foi detectado e inicializado com sucesso
 */
bool as7341_init(as7341_t *dev, i2c_inst_t *i2c, uint8_t addr);

/**
 * @brief Configura o tempo de integração ATIME (0-255)
 *        Tempo = (ATIME + 1) * (ASTEP + 1) * 2.78 µs
 */
bool as7341_set_atime(as7341_t *dev, uint8_t atime);

/**
 * @brief Configura o passo de integração ASTEP (0-65534)
 *        Tempo = (ATIME + 1) * (ASTEP + 1) * 2.78 µs
 */
bool as7341_set_astep(as7341_t *dev, uint16_t astep);

/**
 * @brief Configura o ganho analógico
 */
bool as7341_set_gain(as7341_t *dev, as7341_gain_t gain);

/**
 * @brief Lê todos os 10 canais espectrais (F1-F8, Clear, NIR)
 *        O buffer deve ter pelo menos 12 elementos (uint16_t)
 *        Índices: [0]=F1, [1]=F2, [2]=F3, [3]=F4, [4]=Clear_low, [5]=NIR_low
 *                 [6]=F5, [7]=F6, [8]=F7, [9]=F8, [10]=Clear_high, [11]=NIR_high
 * @param readings Buffer de saída com 12 uint16_t
 * @return true se a leitura foi bem-sucedida
 */
bool as7341_read_all_channels(as7341_t *dev, uint16_t *readings);

/**
 * @brief Habilita/desabilita o LED integrado do sensor
 */
bool as7341_enable_led(as7341_t *dev, bool enable);

/**
 * @brief Define a corrente do LED integrado (4-258 mA)
 */
bool as7341_set_led_current(as7341_t *dev, uint16_t current_ma);

#endif // AS7341_H
