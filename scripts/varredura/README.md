# Script de Varredura Espectral

Este diretório contém o script para automatização da varredura espectral multicanal e geração de gráficos radiométricos em tempo real.

---

## Arquivo

- [varredura_espectral.py](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/scripts/varredura/varredura_espectral.py) — Captura serial, parsing de CSV, visualização em tempo real e exportação gráfica.

---

## Firmware Necessário

- **`build/a734x_test.uf2`** gravado na BitDogLab v7.

---

## Como Executar

```powershell
# Varredura padrão com cubeta cheia (matriz WS2812 ligada, LED do sensor desligado)
py scripts/varredura/varredura_espectral.py --rotulo cheio

# Cubeta cheia com matriz ligada e LED do sensor em 20 mA
py scripts/varredura/varredura_espectral.py --rotulo cheio --led-ma 20

# Cubeta vazia apenas com iluminação do sensor em 20 mA (sem matriz)
py scripts/varredura/varredura_espectral.py --rotulo vazio --sem-matriz --led-ma 20

# Execução sem abrir janela interativa do matplotlib
py scripts/varredura/varredura_espectral.py --rotulo meia_agua --sem-grafico
```

---

## Parâmetros de Linha de Comando

| Parâmetro | Tipo | Padrão | Descrição |
|---|---|---|---|
| `--rotulo` | String | *Obrigatório/Recomendado* | Descrição do ensaio (ex: `vazio`, `cheio`, `meia_agua`) |
| `--port` | String | *Auto* | Porta serial da Pico (detectada automaticamente via VID `2E8A`) |
| `--baud` | Inteiro | `115200` | Taxa de transmissão serial |
| `--sem-matriz` | Flag | `False` | Desliga a matriz de LEDs WS2812 |
| `--led-ma` | Inteiro | `0` | Corrente do LED do sensor AS7341 em mA (`0` = desligado) |
| `--sem-grafico` | Flag | `False` | Suprime a janela gráfica interativa |

---

## Visualização Gráfica em Tempo Real

Durante o ensaio, uma janela do Matplotlib é aberta exibindo o espectro de resposta da amostra para cada uma das 8 cores testadas (Violeta, Índigo, Azul, Ciano, Verde, Amarelo, Laranja e Vermelho):

- **Eixo X**: Canais espectrais com seus respectivos comprimentos de onda centrais:
  `F1 (415 nm)`, `F2 (445 nm)`, `F3 (480 nm)`, `F4 (515 nm)`, `F5 (555 nm)`, `F6 (590 nm)`, `F7 (630 nm)`, `F8 (680 nm)` e canal `Clear`.
- **Eixo Y**: Contagem bruta dos ADCs (proporcional à irradiância espectral).
- **Linhas coloridas**: Médias e desvios das 5 amostras para cada cor emitida pelos LEDs.

---

## Resultados Gerados

Os dados são salvos em:
```
resultados/varredura/<data_hora>__<rotulo>/
├── dados.csv      # Tabela com as 40 medições completas
├── grafico.png    # Gráfico espectral exportado em alta resolução
└── config.json    # Parâmetros de execução (ganho, ATIME, iluminação, porta)
```
