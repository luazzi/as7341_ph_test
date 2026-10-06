# Subsistema de Scripts Python — Aquisição e Visualização de Dados

Este diretório contém os scripts em Python responsáveis pela comunicação serial USB com a placa BitDogLab v7 (RP2350), orquestração dos protocolos experimentais, captura em tempo real dos fluxos CSV e geração de relatórios gráficos.

---

## Estrutura do Diretório

```
scripts/
├── comum.py                  # Módulo base compartilhado (caminhos, portas, JSON)
├── calibracao/               # Protocolo de calibração e caracterização óptica
│   ├── capturar_calibracao.py
│   └── README.md
├── varredura/                # Varredura espectral automatizada de 8 cores
│   ├── varredura_espectral.py
│   └── README.md
├── ph/                       # Medição espectral de soluções com pH ácido, neutro e base
│   ├── captura_dados.py
│   └── README.md
├── legado/                   # Scripts anteriores mantidos para rastreabilidade
│   ├── varredura_espectral_antigo.py
│   └── README.md
└── README.md                 # Este documento
```

---

## Instalação e Pré-requisitos

Os scripts requerem **Python 3.8+** no computador hospedeiro.

Instale as dependências com:

```powershell
pip install pyserial matplotlib numpy
```

---

## Módulo Base: `comum.py`

O arquivo [comum.py](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/scripts/comum.py) unifica as operações de infraestrutura para todos os scripts:

- **Centralização de resultados**: Garante que qualquer execução grave os dados em `resultados/<experimento>/...` na raiz do repositório, independentemente de qual pasta o terminal estiver aberto.
- **Detecção automática de porta**: Identifica a porta serial da Raspberry Pi Pico através do USB VID `0x2E8A` ou pela string de descrição do driver.
- **Formatação de pastas e metadados**: Cria pastas com nomenclatura timestamp padronizada (`AAAAMMDD_HHMMSS__<config>`) e exporta o arquivo `config.json` contendo todos os parâmetros da coleta.

---

## Guia Rápido dos Scripts

| Ensaio | Firmware Necessário | Script | Exemplo de Comando |
|---|---|---|---|
| **Calibração Óptica** | `caracterizacao_optica.uf2` | [capturar_calibracao.py](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/scripts/calibracao/capturar_calibracao.py) | `py scripts/calibracao/capturar_calibracao.py --rotulo teste_cubeta` |
| **Varredura Espectral** | `a734x_test.uf2` | [varredura_espectral.py](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/scripts/varredura/varredura_espectral.py) | `py scripts/varredura/varredura_espectral.py --rotulo cubeta_cheia` |
| **Ensaio de pH** | `a734x_test.uf2` | [captura_dados.py](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/scripts/ph/captura_dados.py) | `py scripts/ph/captura_dados.py acido_neutro_base` |

> [!NOTE]
> Apenas um programa pode utilizar a porta serial de cada vez. Antes de rodar qualquer script, certifique-se de fechar o Serial Monitor do VS Code, PuTTY ou qualquer outro terminal aberto na porta COM da Pico.
