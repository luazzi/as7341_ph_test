"""Varredura espectral: captura as 8 cores da matriz e plota ao vivo.

Firmware: build/a734x_test.uf2 (executa uma varredura automatica ao ligar,
5 leituras por cor, nas cores Violeta, Indigo, Azul, Ciano, Verde, Amarelo,
Laranja e Vermelho).

Uso (a partir de qualquer pasta):
    py scripts/varredura/varredura_espectral.py --rotulo cheio
    py scripts/varredura/varredura_espectral.py --port COM8 --rotulo meia_agua

Inicie o script e depois REINICIE a placa (botao RESET ou desconectar/ligar a
USB). Os resultados ficam em:

    resultados/varredura/<data_hora>__<rotulo>/
        dados.csv
        grafico.png
        config.json
"""

import argparse
import csv
import re
import sys
import time
from collections import OrderedDict
from pathlib import Path

import matplotlib.pyplot as plt
import serial

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from comum import (caminho_relativo, criar_pasta_execucao,  # noqa: E402
                   encontrar_porta_pico, salvar_config)


WAVELENGTHS = [415, 445, 480, 515, 555, 590, 630, 680]
CHANNELS = ["F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8"]
PLOT_LABELS = [f"{canal}\n{onda} nm" for canal, onda in zip(CHANNELS, WAVELENGTHS)] + ["Clear"]
PLOT_POSITIONS = list(range(len(PLOT_LABELS)))
SAMPLES_PER_COLOR = 5
COLORS_PER_CYCLE = 8
OUTPUT_HEADER = [
    "ciclo", "cor_id", "amostra", "medicao", "ph", "cor", "r", "g", "b",
    *CHANNELS, "clear", "nir",
]

STATUS_RE = re.compile(r"# STATUS: matriz=(\w+), led_as7341=(\w+), corrente=(\d+) mA")


def ler_status(device, timeout=5.0):
    """Pede o estado atual a placa e retorna (matriz_ligada, led_ligado)."""
    device.reset_input_buffer()
    device.write(b"s")
    limite = time.time() + timeout
    while time.time() < limite:
        line = device.readline().decode("utf-8", errors="replace").strip()
        match = STATUS_RE.match(line)
        if match:
            return match.group(1) == "ligada", match.group(2) == "ligado"
    return None, None


def configurar_iluminacao(device, matriz, led_ma):
    """Ajusta a matriz e o LED do AS7341 e dispara a varredura ('v')."""
    matriz_atual, led_atual = ler_status(device)
    if matriz_atual is not None:
        if matriz_atual != matriz:
            device.write(b"m")
            time.sleep(0.1)
        if led_ma > 0:
            device.write(f"i{led_ma}\n".encode())
            time.sleep(0.2)
        if led_atual != (led_ma > 0):
            device.write(b"l")
            time.sleep(0.1)
    # Dispara a varredura
    time.sleep(0.2)
    device.write(b"v")


def parse_csv_line(line):
    """Retorna a leitura do formato CSV do firmware, ou None."""
    try:
        row = next(csv.reader([line]))
        if len(row) != 17 or row[0] != "CSV":
            return None
        return {
            "medicao": int(row[1]),
            "ph": row[2],
            "cor": row[3],
            "rgb": (int(row[4]), int(row[5]), int(row[6])),
            "canais": [int(value) for value in row[7:15]],
            "clear": int(row[15]),
            "nir": int(row[16]),
        }
    except (ValueError, csv.Error, StopIteration):
        return None


def redraw(ax, espectros, titulo):
    ax.clear()
    for cor, leituras in espectros.items():
        rgb = leituras[0]["rgb"]
        cor_plot = tuple(valor / 255 for valor in rgb)
        valores = [leitura["canais"] + [leitura["clear"]] for leitura in leituras]
        media = [sum(amostras) / len(amostras) for amostras in zip(*valores)]
        minimo = [min(amostras) for amostras in zip(*valores)]
        maximo = [max(amostras) for amostras in zip(*valores)]

        ax.plot(PLOT_POSITIONS, media, marker="o", linewidth=2,
                color=cor_plot, label=f"{cor} (n={len(leituras)})")
        ax.fill_between(PLOT_POSITIONS, minimo, maximo, color=cor_plot, alpha=0.12)

    ax.set_title(titulo)
    ax.set_xlabel("Canal do AS7341")
    ax.set_ylabel("Contagem ADC")
    ax.set_xticks(PLOT_POSITIONS, PLOT_LABELS)
    ax.grid(True, alpha=0.3)
    if espectros:
        ax.legend(loc="upper left", bbox_to_anchor=(1.02, 1), borderaxespad=0)


def main():
    parser = argparse.ArgumentParser(description="Varredura espectral AS7341 com grafico ao vivo.")
    parser.add_argument("--port", default=None, help="porta serial (padrao: detecta a Pico)")
    parser.add_argument("--baud", default=115200, type=int, help="baud rate (padrao: 115200)")
    parser.add_argument("--rotulo", default="",
                        help="descricao da amostra/configuracao (ex.: cheio, vazio, meia_agua)")
    parser.add_argument("--sem-matriz", action="store_true",
                        help="mantem a matriz de LEDs WS2812 desligada durante a varredura")
    parser.add_argument("--led-ma", type=int, default=0,
                        help="corrente do LED integrado do AS7341 em mA (0 = desligado)")
    args = parser.parse_args()

    porta_nome = args.port or encontrar_porta_pico()
    if not porta_nome:
        sys.exit("Nenhuma porta da Pico encontrada. Informe com --port COMx.")

    try:
        porta = serial.Serial(porta_nome, args.baud, timeout=0.5)
    except serial.SerialException as exc:
        sys.exit(f"Nao foi possivel abrir {porta_nome}: {exc}\n"
                 "Feche o monitor serial ou outro programa usando a porta.")

    matriz = not args.sem_matriz
    partes_pasta = [
        f"matriz-{'on' if matriz else 'off'}",
        f"led-{args.led_ma}mA" if args.led_ma > 0 else "led-off",
        args.rotulo,
    ]

    pasta = criar_pasta_execucao("varredura", partes_pasta)
    arquivo_csv = pasta / "dados.csv"
    arquivo_png = pasta / "grafico.png"
    salvar_config(pasta, {
        "experimento": "varredura espectral",
        "firmware": "a734x_test",
        "porta": porta_nome,
        "rotulo": args.rotulo,
        "matriz_ligada": matriz,
        "led_as7341_mA": args.led_ma if args.led_ma > 0 else 0,
        "sensor": {"atime": 100, "astep": 999, "ganho": "8x"},
        "leituras_por_cor": SAMPLES_PER_COLOR,
        "cores": COLORS_PER_CYCLE,
    })

    print(f"Iluminacao: matriz={'ligada' if matriz else 'desligada'}, "
          f"LED AS7341={f'{args.led_ma} mA' if args.led_ma > 0 else 'desligado'}")
    print(f"Lendo {porta_nome}. Salvando em: {caminho_relativo(pasta)}")

    # Configura a iluminação desejada e inicia a varredura via comando serial
    time.sleep(0.5)
    configurar_iluminacao(porta, matriz, args.led_ma)

    titulo_base = f"AS7341 — media de 5 leituras por cor (Matriz={'ON' if matriz else 'OFF'}, LED={args.led_ma}mA)"
    if args.rotulo:
        titulo_base += f" [{args.rotulo}]"

    plt.ion()
    fig, ax = plt.subplots(figsize=(12, 6))
    fig.tight_layout(rect=(0, 0, 0.82, 1))
    espectros = OrderedDict()
    ciclo_atual = None
    lote_atual = None
    concluido = False

    with porta, open(arquivo_csv, "w", newline="", encoding="utf-8") as arquivo:
        gravador = csv.writer(arquivo)
        gravador.writerow(OUTPUT_HEADER)
        print("Varredura iniciada via serial. Aguardando dados... (Ctrl+C para parar)")

        try:
            while plt.fignum_exists(fig.number):
                linha = porta.readline().decode("utf-8", errors="replace").strip()
                if not linha:
                    plt.pause(0.01)
                    continue

                # BATCH,ciclo,cor_id,cor e emitido antes das cinco leituras.
                if linha.startswith("BATCH,"):
                    campos = linha.split(",", 3)
                    if len(campos) == 4:
                        try:
                            ciclo, cor_id = int(campos[1]), int(campos[2])
                        except ValueError:
                            continue
                        if ciclo_atual != ciclo:
                            ciclo_atual = ciclo
                            espectros.clear()
                        lote_atual = {"ciclo": ciclo, "cor_id": cor_id,
                                      "cor": campos[3], "leituras": []}
                    continue

                leitura = parse_csv_line(linha)
                if leitura is None or lote_atual is None:
                    continue

                # Ignora qualquer CSV que nao pertença a cor anunciada pelo BATCH.
                if leitura["cor"] != lote_atual["cor"]:
                    continue

                lote_atual["leituras"].append(leitura)
                amostra = len(lote_atual["leituras"])
                gravador.writerow([
                    lote_atual["ciclo"], lote_atual["cor_id"], amostra,
                    leitura["medicao"], leitura["ph"], leitura["cor"], *leitura["rgb"],
                    *leitura["canais"], leitura["clear"], leitura["nir"],
                ])
                arquivo.flush()

                if amostra == SAMPLES_PER_COLOR:
                    espectros[leitura["cor"]] = lote_atual["leituras"]
                    redraw(ax, espectros, titulo_base)
                    fig.canvas.draw_idle()
                    plt.pause(0.01)
                    print(f"  {leitura['cor']} concluida ({len(espectros)}/{COLORS_PER_CYCLE}).")
                    if len(espectros) == COLORS_PER_CYCLE:
                        concluido = True
                        break
        except KeyboardInterrupt:
            print("\nCaptura interrompida.")

    if espectros:
        fig.savefig(arquivo_png, dpi=160, bbox_inches="tight")
        print(f"Grafico salvo em: {caminho_relativo(arquivo_png)}")
    print(f"Dados salvos em:  {caminho_relativo(arquivo_csv)}")
    if concluido:
        print("Varredura concluida. Feche a janela do grafico para sair.")
        plt.ioff()
        plt.show()


if __name__ == "__main__":
    main()
