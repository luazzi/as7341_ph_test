# Resultados de Ensaios com Soluções de pH

Este diretório contém os conjuntos de dados adquiridos durante os ensaios espectrais com soluções ácidas, neutras e básicas, utilizando o firmware `a734x_test.uf2` e o script `scripts/ph/captura_dados.py`.

---

## Estrutura das Pastas

```
<AAAAMMDD_HHMMSS>__<rotulo>/
├── dados.csv      # Conjunto de medições espectrais rotuladas por tipo de pH
└── config.json    # Parâmetros de data, firmware, porta e rótulo do ensaio
```

---

## Formato do Arquivo `dados.csv`

| Coluna | Descrição |
|---|---|
| `Num` | Identificador sequencial da leitura |
| `Tipo_pH` | Classe da solução analisada (`Acido`, `Neutro` ou `Base`) |
| `LED_Cor` | Nome da cor da iluminação ativa |
| `LED_R`, `LED_G`, `LED_B` | Componentes RGB aplicados à fonte de luz |
| `F1_415nm`–`F8_680nm` | Respostas dos 8 canais espectrais do AS7341 |
| `Clear` | Resposta do fotodiodo desprovido de filtro |
| `NIR` | Resposta no infravermelho próximo (~910 nm) |

---

## Conjuntos Disponíveis

- **`20260916__acido_neutro_base`**:
  - Ensaio com soluções calibradas ácida, neutra e alcalina, com múltiplas réplicas espectrais para verificação da dispersão e separabilidade ótica das amostras.
