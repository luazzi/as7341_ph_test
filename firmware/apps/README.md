# Módulo Apps — Aplicações de Firmware

Este diretório contém os códigos-fonte dos dois firmwares executáveis desenvolvidos para a plataforma **Raspberry Pi Pico 2 W (RP2350)** na placa **BitDogLab v7**:

1. [a734x_test.c](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/firmware/apps/a734x_test.c) — Varredura espectral automatizada (8 cores × 5 amostras) e testes de pH.
2. [caracterizacao_optica.c](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/firmware/apps/caracterizacao_optica.c) — Calibração e caracterização espectral de 41 condições ópticas dos LEDs e iluminação do sensor.

---

## 1. `a734x_test.c` — Varredura Espectral e Análise de pH

### Objetivo
Permitir a análise espectral de amostras líquidas com diferentes pHs (Ácido, Neutro, Base) e realizar varreduras espectrais automatizadas através da iluminação controlada pelos LEDs WS2812B e pelo LED integrado do AS7341.

### Disposição Física dos LEDs (Matriz Serpentina)
A matriz 5×5 da BitDogLab é conectada em padrão serpentino (*boustrophedon*):

```
     Col 0   Col 1   Col 2   Col 3   Col 4
L0:   24      23      22      21      20
L1:   15     [16]    [17]    [18]     19
L2:   14     [13]    [12]    [11]     10
L3:    5     [ 6]    [ 7]    [ 8]      9
L4:    4       3       2       1       0
```

Os **9 LEDs centrais** utilizados como fonte de iluminação colimada para as cubetas são os índices:
`[16, 17, 18, 13, 12, 11, 6, 7, 8]`.

### Modos de Operação

#### Modo Varredura Automática (Comando `'v'` ou inicialização)
- Percorre sequencialmente 8 cores da paleta espectral:
  1. Violeta (128, 0, 255)
  2. Índigo (75, 0, 130)
  3. Azul (0, 0, 255)
  4. Ciano (0, 255, 255)
  5. Verde (0, 255, 0)
  6. Amarelo (255, 255, 0)
  7. Laranja (255, 128, 0)
  8. Vermelho (255, 0, 0)
- Para cada cor:
  - Acende os 9 LEDs centrais com a cor.
  - Aguarda estabilização térmica e óptica de 500 ms.
  - Coleta 5 leituras completas do sensor (intervalo de 500 ms entre amostras).
  - Emite cada leitura via USB Serial no formato CSV.

#### Modo pH Interativo (Botões físicos ou comandos de terminal)
- **Botão A (GPIO 5)**: Cicla manualmente as cores da matriz de iluminação.
- **Botão B (GPIO 6)**: Dispara uma medição pontual do sensor.
- Comandos via USB Serial para marcar a classe da amostra:
  - `'a'` / `'A'`: Marca amostra como **Ácido**.
  - `'n'` / `'N'`: Marca amostra como **Neutro**.
  - `'b'` / `'B'`: Marca amostra como **Base**.

### Tabela de Comandos Seriais USB

| Tecla | Função |
|---|---|
| `v` / `V` | Inicia a varredura automática completa de 8 cores |
| `p` / `P` | Dispara uma medição pontual com a cor atual |
| `a` / `A` | Seleciona classificação: **Ácido** |
| `n` / `N` | Seleciona classificação: **Neutro** |
| `b` / `B` | Seleciona classificação: **Base** |
| `m` / `M` | Liga / desliga a matriz de LEDs WS2812 |
| `l` / `L` | Liga / desliga o LED integrado do AS7341 |
| `+` / `=` | Aumenta a corrente do LED do AS7341 (+2 mA) |
| `-` / `_` | Diminui a corrente do LED do AS7341 (-2 mA) |
| `i` / `I` | Solicita digitação de corrente direta em mA (ex: 20) |
| `s` / `S` | Imprime status da iluminação no terminal |
| `r` / `R` | Zera o contador de medições sequencial |
| `0` a `7` | Seleciona diretamente a cor correspondente na paleta |
| `h` / `H` | Exibe menu de ajuda |

### Formato de Saída Serial
Cada linha de medição é prefixada com `CSV,`:
```csv
CSV,Num,Tipo_pH,LED_Cor,LED_R,LED_G,LED_B,F1,F2,F3,F4,F5,F6,F7,F8,Clear,NIR
```

---

## 2. `caracterizacao_optica.c` — Protocolo de Calibração Óptica

### Objetivo
Realizar a calibração radiométrica e caracterização da resposta dos LEDs RGB em relação aos canais fotométricos do sensor AS7341, permitindo mapear linearidade e diafonia espectral (*spectral crosstalk*).

### Protocolo Experimental de 41 Condições

O firmware implementa um protocolo estruturado contendo **41 condições de iluminação**, com **5 réplicas** para cada condição, totalizando **205 medições** por ciclo:

1. **Varredura Linear do Canal Vermelho** (10 níveis de brilho):
   - Intensidades $R \in [0, 25, 50, 75, 100, 125, 150, 175, 200, 255]$ com $G=0, B=0$.
2. **Varredura Linear do Canal Verde** (10 níveis de brilho):
   - Intensidades $G \in [0, 25, 50, 75, 100, 125, 150, 175, 200, 255]$ com $R=0, B=0$.
3. **Varredura Linear do Canal Azul** (10 níveis de brilho):
   - Intensidades $B \in [0, 25, 50, 75, 100, 125, 150, 175, 200, 255]$ com $R=0, G=0$.
4. **Condições de Mistura e Brancos** (11 condições):
   - Escuro (0, 0, 0)
   - Amarelo Médio (128, 128, 0) e Amarelo Pleno (255, 255, 0)
   - Ciano Médio (0, 128, 128) e Ciano Pleno (0, 255, 255)
   - Magenta Médio (128, 0, 128) e Magenta Pleno (255, 0, 255)
   - Branco 25% (64, 64, 64), Branco 50% (128, 128, 128), Branco 75% (192, 192, 192) e Branco 100% (255, 255, 255).

### Comandos Seriais

| Comando | Descrição |
|---|---|
| `r` | Executa o protocolo completo de calibração (41 condições × 5 réplicas) |
| `u` | Executa leitura única (5 réplicas) no estado atual da iluminação |
| `m` | Alterna matriz WS2812 (ligada / desligada) |
| `l` | Alterna LED do sensor AS7341 (ligado / desligado) |
| `+` / `-` | Ajusta a corrente do LED do sensor em ±2 mA |
| `i<mA>` | Ajusta a corrente do LED do sensor para o valor informado |
| `s` | Imprime string com status atual de iluminação |
| `h` | Mostra ajuda |

### Formato de Saída do Protocolo
```csv
etapa,nome_cor,r,g,b,matriz,led_ma,medicao,F1,F2,F3,F4,F5,F6,F7,F8,clear,nir
```
Onde:
- `etapa`: `linear` ou `mistura`.
- `matriz`: `1` para ligada, `0` para desligada.
- `led_ma`: valor da corrente do LED em mA (ou `0` se desligado).
- `medicao`: número da réplica (1 a 5).
