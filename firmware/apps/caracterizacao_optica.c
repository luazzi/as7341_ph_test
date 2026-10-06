/*
 * Caracterizacao optica de LED RGB com AS7341
 *
 * Alvo: Raspberry Pi Pico / Pico SDK com matriz WS2812 5x5 da BitDogLab.
 *
 * Ligacoes:
 *   AS7341 SDA -> GPIO 2  (I2C1)
 *   AS7341 SCL -> GPIO 3  (I2C1)
 *   Matriz WS2812 -> GPIO 7
 *
 * Comandos pela serial USB (tecla unica, sem Enter):
 *   m       liga/desliga a matriz de LEDs WS2812
 *   l       liga/desliga o LED acoplado ao AS7341 (pino LDR)
 *   + / -   aumenta/diminui a corrente do LED do AS7341
 *   i<mA>   define a corrente do LED do AS7341 (ex.: i20 + Enter)
 *   r       executa o protocolo completo (41 condicoes x 5 replicas)
 *   u       leitura unica (5 replicas) com o estado atual de iluminacao
 *   s       mostra o estado atual
 *   h       mostra a ajuda
 */

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "as7341.h"
#include "ws2812.pio.h"

// ===== Pinos a ajustar de acordo com a montagem =====
#define I2C_PORT       i2c1
#define I2C_SDA_PIN    2
#define I2C_SCL_PIN    3
#define I2C_BAUDRATE   400000

#define WS2812_PIN     7
#define NUM_LEDS       25
// 1 ilumina toda a matriz; 0 usa apenas os nove LEDs centrais.
#define MATRIX_USE_ALL_LEDS 0

// ===== Protocolo experimental =====
#define ESTABILIZACAO_MS 150
#define REPLICAS          5

// ===== LED acoplado ao AS7341 =====
// O AS7341 aceita 4 a 258 mA, mas o LED da placa do sensor pode aquecer
// muito em correntes altas. Ajuste LED_SENSOR_MAX_MA conforme o LED usado.
#define LED_SENSOR_MIN_MA     4
#define LED_SENSOR_MAX_MA    50
#define LED_SENSOR_PASSO_MA   2
#define LED_SENSOR_INICIAL_MA 10

static as7341_t sensor;
static uint32_t led_buffer[NUM_LEDS];
static PIO ws_pio;
static uint ws_sm;

// Estado de iluminacao controlado pelo usuario
static bool     matriz_ligada  = true;
static bool     led_sensor_on  = false;
static uint16_t led_sensor_ma  = LED_SENSOR_INICIAL_MA;

static const uint8_t center_leds[9] = { 16, 17, 18, 13, 12, 11, 6, 7, 8 };

typedef struct {
    const char *nome;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} condicao_t;

/*
 * A lista fornecida para a etapa 2 possui 11 condicoes:
 * escuro + 6 misturas cromaticas + 4 niveis de branco.
 */
static const condicao_t misturas[] = {
    { "Escuro",          0,   0,   0 },
    { "Amarelo_Medio", 128, 128,   0 },
    { "Amarelo_Pleno", 255, 255,   0 },
    { "Ciano_Medio",     0, 128, 128 },
    { "Ciano_Pleno",     0, 255, 255 },
    { "Magenta_Medio", 128,   0, 128 },
    { "Magenta_Pleno", 255,   0, 255 },
    { "Branco_25",      64,  64,  64 },
    { "Branco_50",     128, 128, 128 },
    { "Branco_75",     192, 192, 192 },
    { "Branco_100",    255, 255, 255 },
};

static const uint8_t niveis_lineares[] = {
    0, 25, 50, 75, 100, 125, 150, 175, 200, 255
};

static uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

static void matrix_update(void) {
    for (uint i = 0; i < NUM_LEDS; i++) {
        pio_sm_put_blocking(ws_pio, ws_sm, led_buffer[i] << 8u);
    }
    sleep_us(500); // tempo de reset do WS2812
}

static void matrix_init(void) {
    ws_pio = pio0;
    ws_sm = pio_claim_unused_sm(ws_pio, true);
    uint offset = pio_add_program(ws_pio, &ws2812_program);
    ws2812_program_init(ws_pio, ws_sm, offset, WS2812_PIN, 800000, false);
    memset(led_buffer, 0, sizeof(led_buffer));
    matrix_update();
}

static void rgb_set(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t cor = rgb_to_grb(r, g, b);
    memset(led_buffer, 0, sizeof(led_buffer));

    // Com a matriz desligada o buffer permanece zerado (LEDs apagados).
    if (matriz_ligada) {
#if MATRIX_USE_ALL_LEDS
        for (uint i = 0; i < NUM_LEDS; i++) {
            led_buffer[i] = cor;
        }
#else
        for (uint i = 0; i < 9; i++) {
            led_buffer[center_leds[i]] = cor;
        }
#endif
    }
    matrix_update();
}

// ===== Controle do LED acoplado ao AS7341 =====

static bool led_sensor_aplicar(void) {
    if (!as7341_set_led_current(&sensor, led_sensor_ma)) return false;
    return as7341_enable_led(&sensor, led_sensor_on);
}

static void led_sensor_definir_corrente(int corrente_ma) {
    if (corrente_ma < LED_SENSOR_MIN_MA) corrente_ma = LED_SENSOR_MIN_MA;
    if (corrente_ma > LED_SENSOR_MAX_MA) corrente_ma = LED_SENSOR_MAX_MA;
    // O registrador so aceita passos de 2 mA a partir de 4 mA.
    corrente_ma = LED_SENSOR_MIN_MA + ((corrente_ma - LED_SENSOR_MIN_MA) / 2) * 2;
    led_sensor_ma = (uint16_t)corrente_ma;

    if (led_sensor_aplicar()) {
        printf("# LED AS7341: corrente = %u mA (%s)\n", led_sensor_ma,
               led_sensor_on ? "ligado" : "desligado");
    } else {
        printf("# ERRO: falha ao configurar o LED do AS7341\n");
    }
}

static void mostrar_status(void) {
    printf("# STATUS: matriz=%s, led_as7341=%s, corrente=%u mA (max %u mA)\n",
           matriz_ligada ? "ligada" : "desligada",
           led_sensor_on ? "ligado" : "desligado",
           led_sensor_ma, LED_SENSOR_MAX_MA);
}

static void mostrar_ajuda(void) {
    printf("# Comandos:\n");
    printf("#   m      liga/desliga a matriz de LEDs\n");
    printf("#   l      liga/desliga o LED do AS7341\n");
    printf("#   + / -  corrente do LED do AS7341 +/- %u mA\n", LED_SENSOR_PASSO_MA);
    printf("#   i<mA>  define a corrente (ex.: i20 + Enter), %u-%u mA\n",
           LED_SENSOR_MIN_MA, LED_SENSOR_MAX_MA);
    printf("#   r      executa o protocolo completo\n");
    printf("#   u      leitura unica (%u replicas) com a iluminacao atual\n", REPLICAS);
    printf("#   s      estado atual\n");
    printf("#   h      ajuda\n");
}

static bool executar_condicao(const char *etapa, const char *nome_cor,
                              uint8_t r, uint8_t g, uint8_t b) {
    uint16_t leitura[12];

    rgb_set(r, g, b);
    sleep_ms(ESTABILIZACAO_MS);

    for (uint8_t medicao = 1; medicao <= REPLICAS; medicao++) {
        if (!as7341_read_all_channels(&sensor, leitura)) {
            // Linha comentada para que leitores de CSV possam ignorar o erro.
            printf("# ERRO,AS7341,%s,%s,%u\n", etapa, nome_cor, medicao);
            return false;
        }

        // clear e nir sao coletados no segundo ciclo SMUX: indices 10 e 11.
        printf("%s,%s,%u,%u,%u,%u,%u,%u,"
               "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
               etapa, nome_cor, r, g, b,
               matriz_ligada ? 1 : 0,
               led_sensor_on ? led_sensor_ma : 0,
               medicao,
               leitura[0], leitura[1], leitura[2], leitura[3],
               leitura[6], leitura[7], leitura[8], leitura[9],
               leitura[10], leitura[11]);
    }
    return true;
}

static bool executar_etapa_linear(void) {
    char nome[32];

    for (uint i = 0; i < sizeof(niveis_lineares); i++) {
        uint8_t valor = niveis_lineares[i];
        snprintf(nome, sizeof(nome), "Vermelho_%u", valor);
        if (!executar_condicao("linear", nome, valor, 0, 0)) return false;
    }
    for (uint i = 0; i < sizeof(niveis_lineares); i++) {
        uint8_t valor = niveis_lineares[i];
        snprintf(nome, sizeof(nome), "Verde_%u", valor);
        if (!executar_condicao("linear", nome, 0, valor, 0)) return false;
    }
    for (uint i = 0; i < sizeof(niveis_lineares); i++) {
        uint8_t valor = niveis_lineares[i];
        snprintf(nome, sizeof(nome), "Azul_%u", valor);
        if (!executar_condicao("linear", nome, 0, 0, valor)) return false;
    }
    return true;
}

static bool executar_etapa_misturas(void) {
    for (uint i = 0; i < sizeof(misturas) / sizeof(misturas[0]); i++) {
        const condicao_t *condicao = &misturas[i];
        if (!executar_condicao("superposicao", condicao->nome,
                               condicao->r, condicao->g, condicao->b)) {
            return false;
        }
    }
    return true;
}

static const char *CSV_HEADER =
    "etapa,nome_cor,r,g,b,matriz,led_ma,medicao,"
    "F1,F2,F3,F4,F5,F6,F7,F8,clear,nir";

static void executar_protocolo(void) {
    mostrar_status();
    // Cabecalho solicitado: as linhas seguintes sao dados CSV.
    printf("%s\n", CSV_HEADER);

    bool sucesso = executar_etapa_linear() && executar_etapa_misturas();
    rgb_set(0, 0, 0);

    if (sucesso) {
        printf("# CONCLUIDO: 41 condicoes, 5 replicas por condicao\n");
    } else {
        printf("# INTERROMPIDO: LEDs desligados apos falha de leitura\n");
    }
}

static void executar_leitura_unica(void) {
    mostrar_status();
    printf("%s\n", CSV_HEADER);
    // Usa branco pleno na matriz (se ligada); com a matriz desligada,
    // apenas o LED do AS7341 (se ligado) ilumina a amostra.
    if (executar_condicao("manual", matriz_ligada ? "Branco_100" : "Matriz_Off",
                          255, 255, 255)) {
        printf("# LEITURA CONCLUIDA\n");
    }
    rgb_set(0, 0, 0);
}

// Le um numero inteiro digitado ate Enter (usado pelo comando 'i').
static int ler_numero_serial(void) {
    int valor = 0;
    bool tem_digito = false;
    while (true) {
        int c = getchar_timeout_us(10000000); // 10 s para digitar
        if (c == PICO_ERROR_TIMEOUT) break;
        if (c >= '0' && c <= '9') {
            valor = valor * 10 + (c - '0');
            tem_digito = true;
            if (valor > 1000) valor = 1000;
        } else if (c == '\r' || c == '\n') {
            if (tem_digito) break;
        } else if (c != ' ') {
            break;
        }
    }
    return tem_digito ? valor : -1;
}

static void processar_comando(int c) {
    switch (c) {
        case 'm': case 'M':
            matriz_ligada = !matriz_ligada;
            rgb_set(0, 0, 0);
            printf("# Matriz de LEDs: %s\n", matriz_ligada ? "LIGADA" : "DESLIGADA");
            break;
        case 'l': case 'L':
            led_sensor_on = !led_sensor_on;
            if (led_sensor_aplicar()) {
                printf("# LED AS7341: %s (%u mA)\n",
                       led_sensor_on ? "LIGADO" : "DESLIGADO", led_sensor_ma);
            } else {
                printf("# ERRO: falha ao configurar o LED do AS7341\n");
            }
            break;
        case '+': case '=':
            led_sensor_definir_corrente(led_sensor_ma + LED_SENSOR_PASSO_MA);
            break;
        case '-': case '_':
            led_sensor_definir_corrente(led_sensor_ma - LED_SENSOR_PASSO_MA);
            break;
        case 'i': case 'I': {
            printf("# Digite a corrente em mA (%u-%u) e Enter:\n",
                   LED_SENSOR_MIN_MA, LED_SENSOR_MAX_MA);
            int valor = ler_numero_serial();
            if (valor < 0) {
                printf("# Valor invalido, corrente mantida em %u mA\n", led_sensor_ma);
            } else {
                led_sensor_definir_corrente(valor);
            }
            break;
        }
        case 'r': case 'R':
            executar_protocolo();
            break;
        case 'u': case 'U':
            executar_leitura_unica();
            break;
        case 's': case 'S':
            mostrar_status();
            break;
        case 'h': case 'H': case '?':
            mostrar_ajuda();
            break;
        default:
            break; // ignora \r, \n e teclas desconhecidas
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(2000); // tempo para a USB serial enumerar

    // Inicializa AS7341 no I2C1.
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    matrix_init();
    rgb_set(0, 0, 0);

    if (!as7341_init(&sensor, I2C_PORT, AS7341_I2CADDR_DEFAULT)) {
        printf("# ERRO: AS7341 nao encontrado no endereco 0x39\n");
        while (true) tight_loop_contents();
    }

    // Com os 25 LEDs ativos, 4x preserva margem antes da saturacao do ADC.
    as7341_set_atime(&sensor, 100);
    as7341_set_astep(&sensor, 999);
    as7341_set_gain(&sensor, AS7341_GAIN_4X);

    // LED do sensor inicia desligado, com a corrente padrao ja configurada.
    led_sensor_aplicar();

    printf("# Caracterizacao optica AS7341 pronta.\n");
    mostrar_ajuda();
    mostrar_status();

    while (true) {
        int c = getchar_timeout_us(100000);
        if (c != PICO_ERROR_TIMEOUT) {
            processar_comando(c);
        }
    }
}
