# Script de Captura de Dados para Testes de pH

Este diretório contém o script para aquisição de dados do ensaio espectral de soluções com diferentes níveis de pH (Ácido, Neutro e Base).

---

## Arquivo

- [captura_dados.py](file:///c:/Users/luana/Documents/IC/a7341/a734x/a734x_test/scripts/ph/captura_dados.py) — Script para captura interativa via serial com atalhos de teclado.

---

## Firmware Necessário

- **`build/a734x_test.uf2`** gravado na BitDogLab v7.

---

## Como Executar

```powershell
# Execução padrão com detecção automática da porta
py scripts/ph/captura_dados.py acido_neutro_base

# Informando porta serial específica e rótulo
py scripts/ph/captura_dados.py COM8 teste_ph_amostra_1
```

---

## Controles Interativos pelo Teclado

O script monitora o teclado de forma não-bloqueante no terminal do Windows (`msvcrt`). Ao pressionar qualquer uma das teclas abaixo, a ação é executada **instantaneamente, sem necessidade de pressionar Enter**:

| Tecla | Ação |
|---|---|
| `a` | Envia comando para a placa marcar a próxima amostra como **Ácido** |
| `n` | Envia comando para a placa marcar a próxima amostra como **Neutro** |
| `b` | Envia comando para a placa marcar a próxima amostra como **Base** |
| `r` | Envia comando para zerar o contador de medições |
| `q` | Encerra a captura, fecha a porta serial e salva os arquivos |

---

## Protocolo Típico de Ensaio

1. Grave `a734x_test.uf2` na BitDogLab v7.
2. Inicie o script `py scripts/ph/captura_dados.py ensaio_ph_01`.
3. Escolha a cor de iluminação desejada na placa usando o **Botão A** (ex: Luz Branca ou Azul).
4. Insira a cubeta com a solução ácida:
   - Pressione `a` no teclado do computador.
   - Pressione o **Botão B** na BitDogLab para coletar a medição (repita 10 vezes para obter réplicas).
5. Troque para a solução neutra:
   - Pressione `n` no teclado.
   - Pressione o **Botão B** 10 vezes.
6. Troque para a solução básica:
   - Pressione `b` no teclado.
   - Pressione o **Botão B** 10 vezes.
7. Pressione `q` no teclado do computador para encerrar o experimento.

---

## Dados Gerados

Os resultados são salvos em:
```
resultados/ph/<data_hora>__<rotulo>/
├── dados.csv      # Linhas CSV com identificador de pH, cor e 10 canais espectrais
└── config.json    # Informações de data, porta e parâmetros do ensaio
```
