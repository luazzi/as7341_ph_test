# Resultados de Calibração e Caracterização Óptica

Este diretório armazena os conjuntos de dados experimentais gerados pelo firmware de caracterização óptica (`caracterizacao_optica.uf2`) e capturados por `scripts/calibracao/capturar_calibracao.py`.

---

## Estrutura das Pastas

Cada experimento é salvo em um subdiretório exclusivo com a seguinte convenção:
```
<AAAAMMDD_HHMMSS>__matriz-<on|off>_led-<off|NNmA>[_<rotulo>]/
├── dados.csv     # Dados brutos das 41 condições × 5 réplicas (205 medições)
└── config.json   # Parâmetros de execução (porta, horários, estado da iluminação)
```

---

## Formato do Arquivo `dados.csv`

| Coluna | Descrição |
|---|---|
| `etapa` | Identificador da etapa (`linear` para canais RGB isolados ou `mistura` para combinações) |
| `nome_cor` | Descritor da condição (ex: `Vermelho_25`, `Amarelo_Pleno`, `Branco_100`, `Escuro`) |
| `r`, `g`, `b` | Níveis PWM aplicados aos LEDs da matriz (0 a 255) |
| `matriz` | `1` se a matriz de LEDs WS2812 estava ativa, `0` se desligada |
| `led_ma` | Corrente aplicada ao LED integrado do AS7341 (mA) |
| `medicao` | Índice da réplica na condição atual (1 a 5) |
| `F1`–`F8` | Contagens fotométricas brutas dos 8 canais espectrais (415 nm a 680 nm) |
| `clear` | Contagem do fotodiodo sem filtro de cor (espectro visível amplo) |
| `nir` | Contagem do fotodiodo sensível a infravermelho próximo (~910 nm) |

---

## Ensaios Realizados

- **`20260928_162903__matriz-on_led-off`**: Dados históricos migrados da versão preliminar do projeto.
- **`20261004_163716__matriz-on_led-off`**: Calibração com matriz WS2812 ativa (9 LEDs centrais) e LED do AS7341 desligado.
