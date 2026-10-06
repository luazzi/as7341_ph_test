# Resultados de Varredura Espectral

Este diretório contém os resultados das varreduras espectrais automatizadas através das 8 cores da matriz de LEDs, capturadas com o script `scripts/varredura/varredura_espectral.py` e firmware `a734x_test.uf2`.

---

## Estrutura das Pastas

Cada ensaio gera um diretório individual:
```
<AAAAMMDD_HHMMSS>__<rotulo>/
├── dados.csv      # Leituras espectrais brutas das 8 cores × 5 amostras
├── grafico.png    # Gráfico espectral dos canais F1-F8 e Clear
└── config.json    # Parâmetros experimentais (ganho, iluminação, data/hora)
```

---

## Variáveis Experimentais Testadas

Os ensaios foram realizados comparando três condições de preenchimento da cubeta:
1. **`vazio`**: Cubeta óptica vazia (referência de ar/transmissão direta).
2. **`meia_agua`**: Cubeta com nível intermediário de água destilada.
3. **`cheio`**: Cubeta completamente preenchida.

Para cada condição física, foram testadas diferentes modalidades de iluminação:
- **`matriz-on_led-off`**: Apenas a matriz de LEDs WS2812B (9 LEDs centrais) acesa.
- **`matriz-off_led-20mA`**: Apenas o LED integrado do sensor AS7341 aceso (corrente regulada em 20 mA).
- **`matriz-on_led-20mA`**: Iluminação combinada da matriz WS2812B com o LED integrado do sensor a 20 mA.

---

## Formato dos Dados

As colunas do arquivo CSV gerado na varredura contêm:
- `ciclo`: Ciclo da varredura.
- `cor_id`: Índice da cor na paleta do firmware (0 a 7).
- `amostra`: Número da amostra na cor atual (1 a 5).
- `medicao`: Sequencial global de medição.
- `ph`: Classificação associada no momento do teste.
- `cor`: Nome da cor emitida pelos LEDs (Violeta, Índigo, Azul, Ciano, Verde, Amarelo, Laranja, Vermelho).
- `r`, `g`, `b`: Valores RGB aplicados aos LEDs da matriz.
- `F1`–`F8`: Leituras dos canais de 415 nm a 680 nm.
- `clear`: Leitura do canal sem filtro óptico.
- `nir`: Leitura do canal no infravermelho próximo (~910 nm).
