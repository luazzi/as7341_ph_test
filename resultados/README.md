# Repositório de Resultados Experimentais

Este diretório armazena todos os dados brutos, gráficos gerados e arquivos de metadados produzidos durante as execuções dos experimentos de calibração, varredura espectral e ensaios de pH.

---

## Organização dos Resultados

Os dados são categorizados em três áreas experimentais:

```
resultados/
├── calibracao/               # Ensaios de caracterização óptica e linearidade
│   ├── 20260928_162903__matriz-on_led-off/
│   ├── 20261004_163716__matriz-on_led-off/
│   └── README.md
├── varredura/                # Varreduras espectrais automáticas (8 cores)
│   ├── 20261004_170354__matriz-on_led-20mA_vazio/
│   ├── 20261004_170857__matriz-on_led-20mA_meia-agua/
│   ├── 20261004_171347__matriz-on_led-20mA_cheio/
│   ├── ... (outros ensaios)
│   └── README.md
├── ph/                       # Ensaios espectrais de pH (ácido, neutro, base)
│   ├── 20260916__acido_neutro_base/
│   └── README.md
└── README.md                 # Este documento
```

---

## Padrão de Nomenclatura das Pastas

Cada execução cria automaticamente um subdiretório exclusivo com a seguinte estrutura:

$$\text{resultados/}\langle\text{experimento}\rangle\text{/}\langle\text{data\_hora}\rangle\text{\_\_}\langle\text{configuração}\rangle\text{/}$$

Exemplo:
`resultados/varredura/20261004_170354__matriz-on_led-20mA_vazio/`

### Conteúdo Padrão de Cada Pasta

| Arquivo | Descrição |
|---|---|
| `dados.csv` | Tabela com todas as medições brutas capturadas do sensor |
| `config.json` | Registro de todos os parâmetros empregados (ganho, ATIME, ASTEP, estado da matriz, corrente do LED, rótulo, porta e timestamp) |
| `grafico.png` | Imagem em alta resolução do gráfico gerado (gerado nos experimentos de varredura) |

---

## Histórico de Migração de Dados (28/09)

Durante a refatoração da arquitetura do repositório, os dados históricos foram unificados na pasta `resultados/`:

| Pasta de Destino | Origem Histórica | Descrição |
|---|---|---|
| `calibracao/20260928_162903__matriz-on_led-off` | `data/calibracao/...csv` | Protocolo preliminar de caracterização dos LEDs |
| `varredura/20260928_152204__meia_agua` | `data/experimentos/meia_agua.txt` + gráfico | Ensaio com cubeta com nível intermediário |
| `varredura/20260928_152535__cheio` | `data/experimentos/cheio.txt` + gráfico | Ensaio com cubeta cheia |
| `varredura/20260928_155748__vazio` | `data/experimentos/vazio.txt` + gráfico | Ensaio de referência com cubeta vazia |
| `ph/20260916__acido_neutro_base` | `data/ph/...csv` | Medições espectrais em soluções de pH |
