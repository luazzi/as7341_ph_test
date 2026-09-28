# AS7341 — Sensor Espectral de 11 Canais com BitDogLab v7

Projeto de Iniciação Científica para análise espectral de soluções com diferentes pH (ácido, neutro e base) utilizando o sensor AS7341 integrado à placa BitDogLab v7.

---

## Sumário

- [Visão geral](#visão-geral)
- [Hardware utilizado](#hardware-utilizado)
- [Conexões](#conexões)
- [Estrutura do projeto](#estrutura-do-projeto)
- [Descrição dos arquivos de código](#descrição-dos-arquivos-de-código)
  - [as7341.h — Header do driver](#as7341h--header-do-driver)
  - [as7341.c — Implementação do driver](#as7341c--implementação-do-driver)
  - [a734x_test.c — Programa principal](#a734x_testc--programa-principal)
  - [ws2812.pio — Programa PIO para LEDs WS2812](#ws2812pio--programa-pio-para-leds-ws2812)
  - [captura_dados.py — Script de captura serial](#captura_dadospy--script-de-captura-serial)
  - [CMakeLists.txt — Configuração de build](#cmakeliststxt--configuração-de-build)
- [Configuração do sensor](#configuração-do-sensor)
- [Formato dos dados coletados](#formato-dos-dados-coletados)
- [Como compilar e executar](#como-compilar-e-executar)
- [Protocolo de medição](#protocolo-de-medição)

---

## Visão geral

O sistema utiliza o sensor espectral AS7341 (ams OSRAM) para medir a resposta espectral de amostras de soluções com diferentes valores de pH. O sensor é posicionado sobre a matriz de LEDs 5×5 (WS2812B) da placa BitDogLab v7, que funciona como fonte de luz controlada. A comunicação entre o microcontrolador RP2350 (Raspberry Pi Pico 2 W) e o sensor é feita via protocolo I2C, enquanto a matriz de LEDs é controlada via PIO (Programmable I/O).

O fluxo de operação é:

1. Os 9 LEDs centrais da matriz acendem com uma cor sólida controlável (fonte de iluminação).
2. A luz atravessa/reflete na amostra e atinge o sensor AS7341.
3. Ao pressionar o botão B, o sensor realiza uma leitura espectral completa (10 canais).
4. Os dados são transmitidos via USB serial em formato CSV e capturados por um script Python no computador.

---

## Hardware utilizado

| Componente | Descrição |
|---|---|
| **BitDogLab v7** | Placa educacional baseada no Raspberry Pi Pico 2 W (RP2350) |
| **AS7341** | Sensor espectral de 11 canais (ams OSRAM), I2C, endereço 0x39 |
| **Matriz WS2812B** | Matriz 5×5 de LEDs RGB endereçáveis (integrada à BitDogLab) |
| **Botões A e B** | Botões da placa BitDogLab (ativos em LOW com pull-up interno) |

---

## Conexões

| Sinal | GPIO | Descrição |
|---|---|---|
| **I2C1 SDA** | GPIO 2 | Dados I2C do AS7341 |
| **I2C1 SCL** | GPIO 3 | Clock I2C do AS7341 |
| **WS2812** | GPIO 7 | Dados da matriz de LEDs (protocolo WS2812, via PIO) |
| **Botão A** | GPIO 5 | Altera a cor da iluminação (pull-up, ativo LOW) |
| **Botão B** | GPIO 6 | Dispara a leitura espectral (pull-up, ativo LOW) |
| **AS7341 VCC** | 3.3V | Alimentação do sensor |
| **AS7341 GND** | GND | Referência comum |

---

## Estrutura do projeto

```
a734x_test/
├── as7341.h              # Header do driver do sensor AS7341
├── as7341.c              # Implementação do driver (I2C + SMUX)
├── a734x_test.c          # Programa principal (LEDs, botões, medição, CSV)
├── ws2812.pio            # Programa PIO para protocolo WS2812
├── captura_dados.py      # Script Python para captura serial → CSV
├── CMakeLists.txt        # Configuração de build CMake (Pico SDK)
├── pico_sdk_import.cmake # Importação do Pico SDK
├── medicoes_ph_*.csv     # Arquivos de dados coletados
└── build/                # Diretório de compilação (gerado)
    └── a734x_test.uf2    # Firmware compilado para gravar na Pico
```

---

## Descrição dos arquivos de código

### `as7341.h` — Header do driver

Define a interface pública do driver do sensor AS7341 para o Pico SDK:

- **Mapa de registradores**: endereços dos registradores do AS7341 conforme o datasheet DS000504 (ENABLE, ATIME, ASTEP, CFG1, STATUS, etc.).
- **Bits de controle**: máscaras para os bits do registrador ENABLE (PON, SP_EN, SMUXEN, WEN, FDEN).
- **Enum de ganho** (`as7341_gain_t`): valores de 0.5× a 512× para o amplificador analógico interno do sensor.
- **Estrutura do driver** (`as7341_t`): encapsula a instância I2C e o endereço do sensor, permitindo uso de múltiplas instâncias.
- **Protótipos das funções**: `as7341_init`, `as7341_set_atime`, `as7341_set_astep`, `as7341_set_gain`, `as7341_read_all_channels`, `as7341_enable_led`, `as7341_set_led_current`.

### `as7341.c` — Implementação do driver

Implementa o driver completo do AS7341 em C nativo, sem dependências externas além do Pico SDK. Os principais componentes são:

**Funções de acesso I2C** (internas, `static`):
- `as7341_write_reg()`: escreve um byte em um registrador via `i2c_write_blocking()`.
- `as7341_read_reg()`: lê um byte de um registrador (write address + read data).
- `as7341_read_regs()`: leitura em bloco de múltiplos registradores consecutivos.
- `as7341_modify_reg()`: operação read-modify-write para alterar bits específicos sem afetar os demais.

**Controle de energia e medição**:
- `as7341_power_enable()`: controla o bit PON (Power ON) do registrador ENABLE.
- `as7341_enable_spectral_measurement()`: controla o bit SP_EN para iniciar/parar medições.
- `as7341_is_data_ready()`: verifica o bit AVALID no registrador STATUS2.
- `as7341_wait_for_data()`: polling com timeout para aguardar a conclusão da integração.

**Configuração do SMUX (Super Multiplexer)**:

O AS7341 possui 11 fotodiodos, mas apenas 6 ADCs internos. Para ler todos os canais, é necessário configurar o multiplexador interno (SMUX) em dois ciclos:

1. `as7341_setup_f1f4_clear_nir()`: configura o SMUX para mapear F1 (415nm), F2 (445nm), F3 (480nm), F4 (515nm), Clear e NIR aos 6 ADCs.
2. `as7341_setup_f5f8_clear_nir()`: reconfigura para F5 (555nm), F6 (590nm), F7 (630nm), F8 (680nm), Clear e NIR.

A configuração é feita escrevendo 20 bytes (registradores 0x00–0x13) na RAM do SMUX, seguida da ativação via bit SMUXEN no registrador ENABLE. Os valores de mapeamento foram extraídos da biblioteca Adafruit AS7341.

**Função principal de leitura** (`as7341_read_all_channels`):
1. Desabilita SP_EN.
2. Configura SMUX para canais baixos (F1–F4).
3. Habilita SP_EN e aguarda AVALID.
4. Lê 12 bytes (6 canais × 2 bytes, little-endian) a partir de CH0_DATA_L (0x95).
5. Repete para canais altos (F5–F8).
6. Retorna buffer de 12 valores `uint16_t`.

### `a734x_test.c` — Programa principal

Contém toda a lógica de aplicação que integra o sensor, os LEDs e a interface com o usuário:

**Mapeamento da matriz serpentina**: a matriz de LEDs 5×5 da BitDogLab utiliza cabeamento em "serpentina" (boustrophedon). O array `center_leds[9]` mapeia os índices lineares dos 9 LEDs centrais (linhas 1–3, colunas 1–3) usados como fonte de iluminação.

**Paleta de cores**: 10 cores pré-definidas (Vermelho, Verde, Azul, Amarelo, Magenta, Ciano, Branco, Laranja, Violeta, Rosa) que podem ser cicladas com o Botão A. Cada cor é definida por seus componentes RGB e um nome para identificação nos dados.

**Controle WS2812 via PIO**: os LEDs WS2812B utilizam um protocolo de 1 fio com temporização precisa. O Pico SDK utiliza o periférico PIO (Programmable I/O) para gerar os sinais de controle em hardware, liberando a CPU. A função `ws2812_put_pixel()` envia dados no formato GRB (Green-Red-Blue) de 24 bits.

**Debounce de botões**: a função `btn_pressed()` implementa debounce por software com verificação dupla e aguardo de soltura do botão, evitando leituras múltiplas por um único pressionamento.

**Seleção de tipo de pH via serial**: o firmware aceita comandos de um caractere via USB serial:
- `a` → Ácido | `n` → Neutro | `b` → Base | `r` → Reset do contador

**Saída em formato CSV**: cada medição gera uma linha prefixada com `CSV,` contendo todos os dados da medição. Este prefixo permite que o script Python filtre apenas as linhas de dados, ignorando mensagens de diagnóstico.

### `ws2812.pio` — Programa PIO para LEDs WS2812

Programa assembly para o periférico PIO do RP2350 que implementa o protocolo de temporização dos LEDs WS2812B a 800 kHz. Opera via side-set para gerar os pulsos de dados com temporização T1=3, T2=3, T3=4 ciclos. O arquivo é processado pelo `pioasm` durante a compilação para gerar o header `ws2812.pio.h`.

### `captura_dados.py` — Script de captura serial

Script Python que roda no computador e captura os dados transmitidos pela Pico via USB serial:

- **Detecção automática de porta**: identifica a porta serial da Pico.
- **Captura seletiva**: filtra linhas que começam com `CSV,` e extrai os 16 campos de dados.
- **Entrada por tecla única**: utiliza `msvcrt.kbhit()` e `msvcrt.getch()` (Windows) para capturar comandos de teclado instantaneamente, sem necessidade de pressionar Enter.
- **Saída**: arquivo CSV com timestamp no nome (`medicoes_ph_YYYYMMDD_HHMMSS.csv`).

### `CMakeLists.txt` — Configuração de build

Configuração CMake para o Pico SDK que define:
- Arquivos fonte (`a734x_test.c`, `as7341.c`).
- Geração do header PIO (`pico_generate_pio_header` para `ws2812.pio`).
- Bibliotecas linkadas: `pico_stdlib`, `hardware_i2c`, `hardware_pio`.
- Saída USB habilitada (`pico_enable_stdio_usb`), UART desabilitada.

---

## Configuração do sensor

| Parâmetro | Valor | Descrição |
|---|---|---|
| **ATIME** | 100 | Número de passos de integração |
| **ASTEP** | 999 | Duração de cada passo |
| **Ganho (AGAIN)** | 8× | Amplificação analógica |
| **Tempo de integração** | ~280 ms | Calculado: (ATIME+1) × (ASTEP+1) × 2.78 µs |
| **I2C Clock** | 400 kHz | Fast Mode |

### Canais espectrais do AS7341

| Canal | Comprimento de onda | Região do espectro |
|---|---|---|
| F1 | 415 nm | Violeta |
| F2 | 445 nm | Azul (índigo) |
| F3 | 480 nm | Azul |
| F4 | 515 nm | Ciano/Verde |
| F5 | 555 nm | Verde |
| F6 | 590 nm | Amarelo |
| F7 | 630 nm | Laranja |
| F8 | 680 nm | Vermelho |
| Clear | — | Luz visível total (sem filtro) |
| NIR | ~910 nm | Infravermelho próximo |

---

## Formato dos dados coletados

O arquivo CSV gerado contém as seguintes colunas:

| Coluna | Tipo | Descrição |
|---|---|---|
| `Num` | Inteiro | Número sequencial da medição |
| `Tipo_pH` | Texto | Tipo da amostra: Acido, Neutro ou Base |
| `LED_Cor` | Texto | Nome da cor dos LEDs durante a medição |
| `LED_R` | 0–255 | Componente vermelho do LED |
| `LED_G` | 0–255 | Componente verde do LED |
| `LED_B` | 0–255 | Componente azul do LED |
| `F1_415nm` | Inteiro | Contagem bruta do canal F1 (violeta) |
| `F2_445nm` | Inteiro | Contagem bruta do canal F2 (azul) |
| `F3_480nm` | Inteiro | Contagem bruta do canal F3 (azul) |
| `F4_515nm` | Inteiro | Contagem bruta do canal F4 (ciano) |
| `F5_555nm` | Inteiro | Contagem bruta do canal F5 (verde) |
| `F6_590nm` | Inteiro | Contagem bruta do canal F6 (amarelo) |
| `F7_630nm` | Inteiro | Contagem bruta do canal F7 (laranja) |
| `F8_680nm` | Inteiro | Contagem bruta do canal F8 (vermelho) |
| `Clear` | Inteiro | Contagem bruta do canal Clear |
| `NIR` | Inteiro | Contagem bruta do canal NIR |

As contagens brutas são proporcionais à intensidade luminosa em cada faixa espectral, ponderadas pelo tempo de integração e ganho configurados.

---

## Como compilar e executar

### Pré-requisitos

- [Pico SDK](https://github.com/raspberrypi/pico-sdk) (v2.3.0 ou superior)
- Extensão Raspberry Pi Pico para VS Code (opcional, configura toolchain automaticamente)
- Python 3 com `pyserial` instalado (`pip install pyserial`)

### Compilação

```bash
# Configurar o projeto (primeira vez)
cmake -B build -G "Ninja"

# Compilar
cmake --build build
```

O firmware compilado estará em `build/a734x_test.uf2`.

### Gravação na Pico

1. Segure o botão **BOOTSEL** da Pico e conecte via USB.
2. Copie `build/a734x_test.uf2` para a unidade USB que aparecerá.
3. A Pico reiniciará automaticamente.

### Captura de dados

```bash
python captura_dados.py          # Detecta porta automaticamente
python captura_dados.py COM8     # Porta específica
```

---

## Protocolo de medição

1. Conectar a BitDogLab ao computador via USB.
2. Gravar o firmware (`a734x_test.uf2`).
3. Executar o script de captura (`captura_dados.py`).
4. Selecionar a cor dos LEDs desejada com o **Botão A**.
5. Selecionar o tipo de pH no terminal: tecla `a` (ácido), `n` (neutro) ou `b` (base).
6. Posicionar a amostra entre os LEDs e o sensor.
7. Pressionar o **Botão B** para disparar a medição.
8. Repetir passos 6–7 para cada medição (10× por tipo de pH).
9. Tecla `q` para encerrar e salvar o arquivo CSV.
