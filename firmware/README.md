# Subsistema de Firmware — BitDogLab v7 (RP2350)

Este diretório contém todo o código embarcado desenvolvido em C nativo com o **Raspberry Pi Pico SDK** para o microcontrolador **RP2350** (Raspberry Pi Pico 2 W) integrado à plataforma educacional **BitDogLab v7**.

---

## Estrutura do Diretório

```
firmware/
├── apps/                     # Aplicações principais de firmware (executáveis)
│   ├── a734x_test.c          # Varredura espectral e medição de soluções de pH
│   ├── caracterizacao_optica.c # Protocolo de 41 condições ópticas dos LEDs
│   └── README.md             # Documentação detalhada dos aplicativos
├── drivers/                  # Drivers de periféricos
│   ├── as7341.h              # Cabeçalho do driver do sensor espectral AS7341
│   ├── as7341.c              # Implementação I2C e SMUX do AS7341
│   └── README.md             # Documentação técnica do driver AS7341
├── pio/                      # Programas para o periférico PIO do RP2350
│   ├── ws2812.pio            # Assembly PIO para controle dos LEDs WS2812B a 800 kHz
│   └── README.md             # Documentação de temporização e arquitetura PIO
└── README.md                 # Este documento
```

---

## Hardware e Conexões (BitDogLab v7)

| Periférico / Sinal | GPIO | Configuração | Descrição |
|---|---|---|---|
| **I2C1 SDA** | GPIO 2 | Pull-up externo / interno | Linha de dados I2C do sensor AS7341 |
| **I2C1 SCL** | GPIO 3 | 400 kHz (Fast Mode) | Linha de clock I2C do sensor AS7341 |
| **Matriz WS2812B** | GPIO 7 | PIO State Machine (800 kHz) | Sinal de dados serial da matriz 5×5 de LEDs |
| **Botão A** | GPIO 5 | Entrada com pull-up (Ativo em LOW) | Alterna as cores da fonte de iluminação |
| **Botão B** | GPIO 6 | Entrada com pull-up (Ativo em LOW) | Dispara leitura espectral manual |
| **VCC** | 3.3V | Barramento de alimentação | Alimentação do sensor AS7341 |
| **GND** | GND | Referência comum | Referência de aterramento comum |

---

## Executáveis Gerados

O projeto gera dois firmwares distintos a partir do mesmo sistema de build CMake:

1. **`build/a734x_test.uf2`**:
   - Firmware para **varredura espectral automatizada** e **ensaios com soluções de pH**.
   - Integra controle da matriz WS2812 (9 LEDs centrais), ciclo de cores, botões A e B, comandos seriais e transmissão contínua de linhas `CSV,` via USB CDC.
   - Veja detalhes em [firmware/apps/README.md](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/firmware/apps/README.md).

2. **`build/caracterizacao_optica.uf2`**:
   - Firmware para **calibração fotométrica e caracterização óptica**.
   - Executa protocolo automatizado de 41 condições ópticas (10 níveis R, 10 níveis G, 10 níveis B, 11 misturas e brancos) com 5 réplicas por condição (205 medições).
   - Veja detalhes em [firmware/apps/README.md](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/firmware/apps/README.md).

---

## Como Compilar o Firmware

### Pré-requisitos
- **Raspberry Pi Pico SDK** (versão 2.3.0 ou mais recente instalada no sistema).
- **CMake** (3.13+) e compilador **ARM GNU Toolchain** (`arm-none-eabi-gcc`).
- Gerador de build **Ninja** ou **Make**.

### Passos de Compilação

Na raiz do repositório:

```powershell
# 1. Configurar o projeto CMake
cmake -B build -G "Ninja"

# 2. Compilar todos os executáveis
cmake --build build
```

Após o término da compilação bem-sucedida, os arquivos `.uf2` estarão disponíveis em:
- `build/a734x_test.uf2`
- `build/caracterizacao_optica.uf2`

---

## Como Gravar na Placa (RP2350)

1. Mantenha pressionado o botão **BOOTSEL** da placa BitDogLab (ou Raspberry Pi Pico 2 W).
2. Conecte o cabo USB ao computador (ou pressione e solte RESET mantendo BOOTSEL).
3. A placa será montada como uma unidade de disco removível (`RPI-RP2` ou similar).
4. Arraste e solte o arquivo `.uf2` desejado para a unidade.
5. A placa reiniciará instantaneamente executando o firmware gravado.
