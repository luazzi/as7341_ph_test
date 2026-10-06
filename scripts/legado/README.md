# Scripts Legados — Histórico e Arquivo

Este diretório contém versões anteriores dos scripts de aquisição de dados desenvolvidos nas fases iniciais do projeto de Iniciação Científica.

---

## Arquivos

- [varredura_espectral_antigo.py](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/scripts/legado/varredura_espectral_antigo.py) — Versão inicial do script de varredura com controle manual da placa via temporizações locais no Python.

---

## Motivação e Estado Atual

> [!WARNING]
> Os scripts deste diretório **não devem ser utilizados** para novos ensaios com os firmwares atuais (`a734x_test.uf2` e `caracterizacao_optica.uf2`).

### Diferenças em Relação à Versão Atual

1. **Protocolo Serial**:
   - A versão legada enviava caracteres individuais de troca de cor com esperas rígidas de tempo no Python (`sleep(1.0)`).
   - A versão moderna ([scripts/varredura/varredura_espectral.py](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/scripts/varredura/varredura_espectral.py)) sincroniza diretamente com o comando `'v'` e o handshake de status `'s'`, delegando a temporização ao microcontrolador RP2350.
2. **Organização de Dados**:
   - O script legado gravava arquivos avulsos no diretório de execução ou sob estruturas antigas (`data/experimentos/`).
   - A suíte atual utiliza [scripts/comum.py](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/scripts/comum.py), garantindo que todos os resultados fiquem centralizados em `resultados/` com metadados estruturados (`config.json`), dados brutos (`dados.csv`) e gráficos (`grafico.png`).
