/*
 * AS7341 + Matriz WS2812 5x5 - BitDogLab v7
 * Raspberry Pi Pico (Pico SDK, C nativo)
 *
 * Funcionalidades:
 *   - 9 LEDs centrais da matriz 5x5 acesos em uma cor sólida
 *   - Botão A (GPIO 5): muda a cor dos LEDs
 *   - Botão B (GPIO 6): dispara leitura espectral do AS7341
 *   - Serial: enviar 'a' para ácido, 'n' para neutro, 'b' para base
 *   - Cada medição é impressa em formato CSV para captura
 *
 * Conexões (BitDogLab v7):
 *   Matriz WS2812 -> GPIO 7
 *   Botão A       -> GPIO 5 (ativo em LOW, pull-up)
 *   Botão B       -> GPIO 6 (ativo em LOW, pull-up)
 *   AS7341 SDA    -> GPIO 2 (I2C1)
 *   AS7341 SCL    -> GPIO 3 (I2C1)
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "as7341.h"
#include "ws2812.pio.h"

// ============================================================================
// Pinagem BitDogLab v7
// ============================================================================
#define WS2812_PIN      7     // Matriz de LEDs WS2812
#define BTN_A_PIN       5     // Botão A
#define BTN_B_PIN       6     // Botão B
#define I2C_SDA_PIN     2     // I2C1 SDA
#define I2C_SCL_PIN     3     // I2C1 SCL
#define I2C_BAUDRATE    400000

// ============================================================================
// Configuração da matriz 5x5
// ============================================================================
#define NUM_LEDS        25
#define MATRIX_WIDTH    5
#define MATRIX_HEIGHT   5

// Buffer de cores da matriz (formato GRB para WS2812)
static uint32_t led_buffer[NUM_LEDS];

// PIO e state machine para WS2812
static PIO ws_pio;
static uint ws_sm;

// Instância do sensor AS7341
static as7341_t sensor;

// ============================================================================
// Mapeamento da matriz serpentina (BitDogLab v7)
// ============================================================================
//
// Layout visual (visto de frente):
//
//    Col:  0    1    2    3    4
// Linha 0: 24   23   22   21   20
// Linha 1: 15   16   17   18   19
// Linha 2: 14   13   12   11   10
// Linha 3:  5    6    7    8    9
// Linha 4:  4    3    2    1    0
//
// Os 9 LEDs centrais (linhas 1-3, colunas 1-3):
//   16, 17, 18
//   13, 12, 11
//    6,  7,  8

static const uint8_t center_leds[9] = {
    16, 17, 18,   // Linha 1, colunas 1-3
    13, 12, 11,   // Linha 2, colunas 1-3
     6,  7,  8    // Linha 3, colunas 1-3
};

// ============================================================================
// Paleta de cores para o botão A
// ============================================================================
typedef struct {
    uint8_t r, g, b;
    const char *nome;
} cor_t;

static const cor_t paleta[] = {
    { 255,   0,   0, "Vermelho"  },
    {   0, 255,   0, "Verde"     },
    {   0,   0, 255, "Azul"      },
    { 255, 255,   0, "Amarelo"   },
    { 255,   0, 255, "Magenta"   },
    {   0, 255, 255, "Ciano"     },
    { 255, 255, 255, "Branco"    },
    { 255, 128,   0, "Laranja"   },
    { 128,   0, 255, "Violeta"   },
    { 255,  64, 128, "Rosa"      },
};

#define NUM_PALETA (sizeof(paleta) / sizeof(paleta[0]))

static uint8_t cor_atual = 0;

// ============================================================================
// Tipo de pH (selecionável via serial)
// ============================================================================
typedef enum {
    PH_ACIDO = 0,
    PH_NEUTRO,
    PH_BASE,
    PH_NUM_TIPOS
} ph_tipo_t;

static const char *ph_nomes[] = { "Acido", "Neutro", "Base" };
static ph_tipo_t ph_atual = PH_ACIDO;
static uint16_t medicao_num = 0;  // Contador global de medições

// ============================================================================
// Funções WS2812
// ============================================================================

/**
 * @brief Converte RGB para formato GRB de 24 bits (WS2812)
 */
static inline uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
}

/**
 * @brief Envia um pixel para o PIO
 */
static inline void ws2812_put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(ws_pio, ws_sm, pixel_grb << 8u);
}

/**
 * @brief Atualiza toda a matriz com o conteúdo do buffer
 */
static void ws2812_update(void) {
    for (int i = 0; i < NUM_LEDS; i++) {
        ws2812_put_pixel(led_buffer[i]);
    }
    sleep_us(500);  // Reset time (>280µs)
}

/**
 * @brief Limpa toda a matriz (todos os LEDs apagados)
 */
static void ws2812_clear(void) {
    memset(led_buffer, 0, sizeof(led_buffer));
}

/**
 * @brief Define os 9 LEDs centrais com a cor especificada
 */
static void set_center_color(uint8_t r, uint8_t g, uint8_t b) {
    // Primeiro limpa tudo
    ws2812_clear();

    // Define os 9 LEDs centrais
    uint32_t color = rgb_to_grb(r, g, b);
    for (int i = 0; i < 9; i++) {
        led_buffer[center_leds[i]] = color;
    }

    // Atualiza a matriz
    ws2812_update();
}

/**
 * @brief Inicializa o PIO para WS2812
 */
static void ws2812_init(void) {
    ws_pio = pio0;
    ws_sm = pio_claim_unused_sm(ws_pio, true);
    uint offset = pio_add_program(ws_pio, &ws2812_program);
    ws2812_program_init(ws_pio, ws_sm, offset, WS2812_PIN, 800000, false);

    // Limpa a matriz
    ws2812_clear();
    ws2812_update();
}

// ============================================================================
// Debounce simples para botões
// ============================================================================

static bool btn_pressed(uint gpio) {
    if (!gpio_get(gpio)) {   // Ativo em LOW
        sleep_ms(20);         // Debounce
        if (!gpio_get(gpio)) {
            // Aguarda soltar
            while (!gpio_get(gpio)) {
                sleep_ms(10);
            }
            sleep_ms(20);
            return true;
        }
    }
    return false;
}

// ============================================================================
// Verificação de comandos via serial
// ============================================================================

static void verificar_serial(void) {
    int c = getchar_timeout_us(0);  // Non-blocking read
    if (c == PICO_ERROR_TIMEOUT) return;

    switch (c) {
        case 'a':
        case 'A':
            ph_atual = PH_ACIDO;
            printf("[pH] Tipo alterado para: ACIDO\n");
            break;
        case 'n':
        case 'N':
            ph_atual = PH_NEUTRO;
            printf("[pH] Tipo alterado para: NEUTRO\n");
            break;
        case 'b':
        case 'B':
            ph_atual = PH_BASE;
            printf("[pH] Tipo alterado para: BASE\n");
            break;
        case 'r':
        case 'R':
            medicao_num = 0;
            printf("[Reset] Contador de medicoes zerado.\n");
            break;
        case 'h':
        case 'H':
        case '?':
            printf("\n--- Comandos ---\n");
            printf("  a = Tipo Acido\n");
            printf("  n = Tipo Neutro\n");
            printf("  b = Tipo Base\n");
            printf("  r = Reset contador\n");
            printf("  h = Ajuda\n");
            printf("  Tipo atual: %s | Medicao #%u\n\n",
                   ph_nomes[ph_atual], medicao_num);
            break;
        default:
            break;
    }
}

// ============================================================================
// Leitura espectral com saída CSV
// ============================================================================

static void realizar_leitura(void) {
    uint16_t readings[12];

    printf("\n[Medindo...]\n");

    if (!as7341_read_all_channels(&sensor, readings)) {
        printf("ERRO: Falha ao ler os canais do AS7341!\n\n");
        return;
    }

    medicao_num++;

    // --- Saída legível para o terminal ---
    printf("--- Medicao #%u | pH: %s | LED: %s ---\n",
           medicao_num, ph_nomes[ph_atual], paleta[cor_atual].nome);
    printf("  F1  (415nm): %u\n", readings[0]);
    printf("  F2  (445nm): %u\n", readings[1]);
    printf("  F3  (480nm): %u\n", readings[2]);
    printf("  F4  (515nm): %u\n", readings[3]);
    printf("  F5  (555nm): %u\n", readings[6]);
    printf("  F6  (590nm): %u\n", readings[7]);
    printf("  F7  (630nm): %u\n", readings[8]);
    printf("  F8  (680nm): %u\n", readings[9]);
    printf("  Clear:       %u\n", readings[10]);
    printf("  NIR:         %u\n", readings[11]);

    // --- Saída CSV (prefixada com "CSV," para fácil parsing) ---
    // Formato:
    // CSV,num,tipo_ph,led_cor,led_R,led_G,led_B,F1_415,F2_445,F3_480,F4_515,F5_555,F6_590,F7_630,F8_680,Clear,NIR
    printf("CSV,%u,%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
           medicao_num,
           ph_nomes[ph_atual],
           paleta[cor_atual].nome,
           paleta[cor_atual].r,
           paleta[cor_atual].g,
           paleta[cor_atual].b,
           readings[0],   // F1 415nm
           readings[1],   // F2 445nm
           readings[2],   // F3 480nm
           readings[3],   // F4 515nm
           readings[6],   // F5 555nm
           readings[7],   // F6 590nm
           readings[8],   // F7 630nm
           readings[9],   // F8 680nm
           readings[10],  // Clear
           readings[11]   // NIR
    );

    printf("---\n\n");
}

// ============================================================================
// Main
// ============================================================================

int main() {
    // Inicializa stdio (USB serial)
    stdio_init_all();
    sleep_ms(2000);

    printf("========================================\n");
    printf("  AS7341 + Matriz WS2812 5x5\n");
    printf("  BitDogLab v7 - Modo Coleta pH\n");
    printf("========================================\n\n");

    // --- Inicializa I2C1 ---
    i2c_init(i2c1, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    printf("[I2C] SDA=GPIO%d, SCL=GPIO%d, %d kHz\n\n",
           I2C_SDA_PIN, I2C_SCL_PIN, I2C_BAUDRATE / 1000);

    // --- Inicializa botões ---
    gpio_init(BTN_A_PIN);
    gpio_set_dir(BTN_A_PIN, GPIO_IN);
    gpio_pull_up(BTN_A_PIN);

    gpio_init(BTN_B_PIN);
    gpio_set_dir(BTN_B_PIN, GPIO_IN);
    gpio_pull_up(BTN_B_PIN);
    printf("[Botoes] A=GPIO%d, B=GPIO%d\n\n", BTN_A_PIN, BTN_B_PIN);

    // --- Scanner I2C ---
    printf("[I2C] Escaneando barramento...\n");
    int encontrados = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        uint8_t dummy;
        int ret = i2c_read_blocking(i2c1, addr, &dummy, 1, false);
        if (ret >= 0) {
            printf("  -> Dispositivo encontrado: 0x%02X\n", addr);
            encontrados++;
        }
    }
    printf("[I2C] Total: %d dispositivo(s)\n\n", encontrados);

    // --- Inicializa AS7341 ---
    printf("[AS7341] Inicializando sensor...\n");
    if (!as7341_init(&sensor, i2c1, AS7341_I2CADDR_DEFAULT)) {
        printf("  ERRO: AS7341 nao encontrado no endereco 0x39!\n");
        printf("  Verifique as conexoes I2C1:\n");
        printf("    SDA -> GPIO %d\n", I2C_SDA_PIN);
        printf("    SCL -> GPIO %d\n", I2C_SCL_PIN);
        while (1) tight_loop_contents();
    }
    printf("  AS7341 detectado com sucesso!\n\n");

    // Configuração do sensor
    as7341_set_atime(&sensor, 100);
    as7341_set_astep(&sensor, 999);
    as7341_set_gain(&sensor, AS7341_GAIN_8X);

    float tempo_ms = (100 + 1) * (999 + 1) * 2.78e-3f;
    printf("[Config] ATIME=100, ASTEP=999, Ganho=8x\n");
    printf("         Tempo de integracao ~= %.1f ms\n\n", tempo_ms);

    // --- Inicializa matriz WS2812 ---
    printf("[WS2812] Inicializando matriz 5x5 (GPIO%d)...\n", WS2812_PIN);
    ws2812_init();
    printf("  Matriz inicializada!\n\n");

    // Acende os 9 LEDs centrais com a primeira cor
    set_center_color(paleta[cor_atual].r, paleta[cor_atual].g, paleta[cor_atual].b);

    // --- Instruções ---
    printf("========================================\n");
    printf("  MODO COLETA DE DADOS pH\n");
    printf("========================================\n");
    printf("  Botao A -> Muda a cor do LED\n");
    printf("  Botao B -> Dispara medicao\n");
    printf("\n");
    printf("  Comandos serial:\n");
    printf("    'a' = pH Acido\n");
    printf("    'n' = pH Neutro\n");
    printf("    'b' = pH Base\n");
    printf("    'r' = Reset contador\n");
    printf("    'h' = Ajuda\n");
    printf("\n");
    printf("  Tipo pH atual: %s\n", ph_nomes[ph_atual]);
    printf("  Cor LED atual: %s\n", paleta[cor_atual].nome);
    printf("========================================\n\n");

    // Imprime cabeçalho CSV (para referência)
    printf("CSV_HEADER,Num,Tipo_pH,LED_Cor,LED_R,LED_G,LED_B,F1_415nm,F2_445nm,F3_480nm,F4_515nm,F5_555nm,F6_590nm,F7_630nm,F8_680nm,Clear,NIR\n");

    // --- Loop principal ---
    while (true) {
        // Verifica comandos serial (tipo pH)
        verificar_serial();

        // Botão A: muda a cor
        if (btn_pressed(BTN_A_PIN)) {
            cor_atual = (cor_atual + 1) % NUM_PALETA;
            set_center_color(paleta[cor_atual].r,
                             paleta[cor_atual].g,
                             paleta[cor_atual].b);
            printf("[Cor] %s (R=%d, G=%d, B=%d)\n",
                   paleta[cor_atual].nome,
                   paleta[cor_atual].r,
                   paleta[cor_atual].g,
                   paleta[cor_atual].b);
        }

        // Botão B: leitura espectral
        if (btn_pressed(BTN_B_PIN)) {
            realizar_leitura();
        }

        sleep_ms(10);  // Reduz uso de CPU
    }

    return 0;
}
