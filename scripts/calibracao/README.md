# Script de Captura de Calibração Óptica

Este diretório contém o script de automação para aquisição de dados do ensaio de caracterização fotométrica e calibração dos LEDs.

---

## Arquivo

- [capturar_calibracao.py](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/scripts/calibracao/capturar_calibracao.py) — Script de comunicação serial e gravação dos dados de calibração.

---

## Firmware Necessário

- **`build/caracterizacao_optica.uf2`** gravado na BitDogLab v7.

---

## Como Executar

O script pode ser executado a partir de qualquer diretório:

```powershell
# Execução padrão (matriz WS2812 ligada, LED do sensor desligado)
py scripts/calibracao/capturar_calibracao.py

# Apenas o LED integrado do AS7341 com 20 mA (matriz desligada)
py scripts/calibracao/capturar_calibracao.py --sem-matriz --led-ma 20

# Matriz ligada com LED do sensor em 10 mA e rótulo descritivo
py scripts/calibracao/capturar_calibracao.py --led-ma 10 --rotulo cubeta_vazia

# Especificando porta serial manualmente
py scripts/calibracao/capturar_calibracao.py --port COM8 --led-ma 20
```

---

## Parâmetros de Linha de Comando

| Parâmetro | Tipo | Padrão | Descrição |
|---|---|---|---|
| `--port` | String | *Auto* | Porta serial da Pico (detectada automaticamente via VID `2E8A`) |
| `--baud` | Inteiro | `115200` | Taxa de transmissão serial |
| `--sem-matriz` | Flag | `False` | Mantém a matriz de LEDs WS2812 apagada |
| `--led-ma` | Inteiro | `0` | Corrente do LED integrado do AS7341 em mA (`0` = desligado) |
| `--rotulo` | String | `""` | Rótulo customizado adicionado ao nome da pasta de resultados |

---

## Fluxo de Execução

1. O script abre a porta serial USB.
2. Envia o comando `'s'` para ler o estado da placa e ajusta o estado da matriz e da corrente do LED conforme os argumentos passados.
3. Envia o comando `'r'` para disparar o protocolo na Pico.
4. Aguarda e valida a recepção de **205 linhas de dados** (41 condições ópticas × 5 réplicas).
5. Salva os resultados no diretório padronizado:
   ```
   resultados/calibracao/<data_hora>__matriz-<on|off>_led-<off|NNmA>[_rotulo]/
   ├── dados.csv      # 205 linhas com todos os canais espectrais
   └── config.json    # Metadados e parâmetros da execução
   ```
