"""Captura a caracterizacao optica (calibracao) enviada pela Pico.

Firmware: build/caracterizacao_optica.uf2

Uso (a partir de qualquer pasta):
    py scripts/calibracao/capturar_calibracao.py
    py scripts/calibracao/capturar_calibracao.py --port COM8 --sem-matriz --led-ma 20
    py scripts/calibracao/capturar_calibracao.py --led-ma 10 --rotulo cubeta_vazia

O script configura a iluminacao (matriz WS2812 e LED do AS7341), envia o
comando 'r' e salva as 41 condicoes x 5 replicas = 205 linhas em:

    resultados/calibracao/<data_hora>__matriz-<on|off>_led-<off|NNmA>[_rotulo]/
        dados.csv
        config.json
"""

import argparse
import csv
import re
import sys
import time
from pathlib import Path

import serial

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from comum import (caminho_relativo, criar_pasta_execucao,  # noqa: E402
                   encontrar_porta_pico, salvar_config)


HEADER = [
    "etapa", "nome_cor", "r", "g", "b", "matriz", "led_ma", "medicao",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "clear", "nir",
]
EXPECTED_ROWS = 41 * 5
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
    sys.exit("A placa nao respondeu ao comando de status. "
             "O firmware caracterizacao_optica esta gravado?")


def configurar_iluminacao(device, matriz, led_ma):
    """Ajusta a matriz e o LED do AS7341 e inicia o protocolo."""
    matriz_atual, led_atual = ler_status(device)
    if matriz_atual != matriz:
        device.write(b"m")
    if led_ma > 0:
        device.write(f"i{led_ma}\n".encode())
        time.sleep(0.2)
    if led_atual != (led_ma > 0):
        device.write(b"l")
    time.sleep(0.2)
    device.write(b"r")


def valid_data_row(row):
    if len(row) != len(HEADER):
        return False
    try:
        # Todos os campos exceto etapa/nome_cor devem ser inteiros.
        [int(value) for value in row[2:]]
        return True
    except ValueError:
        return False


def main():
    parser = argparse.ArgumentParser(description="Captura CSV da caracterizacao AS7341.")
    parser.add_argument("--port", default=None, help="porta serial (padrao: detecta a Pico)")
    parser.add_argument("--baud", type=int, default=115200, help="baud rate (padrao: 115200)")
    parser.add_argument("--sem-matriz", action="store_true",
                        help="mantem a matriz de LEDs WS2812 desligada")
    parser.add_argument("--led-ma", type=int, default=0,
                        help="corrente do LED do AS7341 em mA (0 = desligado)")
    parser.add_argument("--rotulo", default="",
                        help="texto livre adicionado ao nome da pasta (ex.: cubeta_vazia)")
    args = parser.parse_args()

    porta = args.port or encontrar_porta_pico()
    if not porta:
        sys.exit("Nenhuma porta da Pico encontrada. Informe com --port COMx.")

    matriz = not args.sem_matriz
    config_nome = [
        f"matriz-{'on' if matriz else 'off'}",
        f"led-{args.led_ma}mA" if args.led_ma > 0 else "led-off",
        args.rotulo,
    ]

    try:
        device = serial.Serial(porta, args.baud, timeout=1)
    except serial.SerialException as exc:
        sys.exit(f"Nao foi possivel abrir {porta}: {exc}\n"
                 "Feche o monitor serial ou outro programa usando a porta.")

    time.sleep(0.5)
    configurar_iluminacao(device, matriz, args.led_ma)

    pasta = criar_pasta_execucao("calibracao", config_nome)
    arquivo_csv = pasta / "dados.csv"
    salvar_config(pasta, {
        "experimento": "calibracao (caracterizacao optica)",
        "firmware": "caracterizacao_optica",
        "porta": porta,
        "matriz_ligada": matriz,
        "led_as7341_mA": args.led_ma if args.led_ma > 0 else 0,
        "rotulo": args.rotulo,
        "sensor": {"atime": 100, "astep": 999, "ganho": "4x"},
        "replicas_por_condicao": 5,
        "condicoes": 41,
    })

    print(f"Iluminacao: matriz={'ligada' if matriz else 'desligada'}, "
          f"LED AS7341={f'{args.led_ma} mA' if args.led_ma > 0 else 'desligado'}")
    print(f"Salvando em: {caminho_relativo(pasta)}")

    saved_rows = 0
    got_header = False
    with open(arquivo_csv, "w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        try:
            while True:
                try:
                    line = device.readline().decode("utf-8", errors="replace").strip()
                except serial.SerialException:
                    print(f"\nConexao com {porta} perdida. Captura encerrada.")
                    break
                if not line:
                    continue

                if line == ",".join(HEADER):
                    if not got_header:
                        writer.writerow(HEADER)
                        got_header = True
                        print("Cabecalho recebido; salvando medidas...")
                    continue

                if line.startswith("# CONCLUIDO"):
                    break
                if line.startswith("# INTERROMPIDO"):
                    print(f"\nA placa informou falha: {line}")
                    break
                if line.startswith("#") or not got_header:
                    continue

                try:
                    row = next(csv.reader([line]))
                except csv.Error:
                    continue
                if valid_data_row(row):
                    writer.writerow(row)
                    file.flush()
                    saved_rows += 1
                    print(f"\rMedidas salvas: {saved_rows}/{EXPECTED_ROWS}", end="", flush=True)
        except KeyboardInterrupt:
            print("\nCaptura interrompida pelo usuario.")
        finally:
            device.close()

    print()
    status = "Concluido" if saved_rows == EXPECTED_ROWS else "Atencao (incompleto)"
    print(f"{status}: {saved_rows}/{EXPECTED_ROWS} medidas em {caminho_relativo(arquivo_csv)}")


if __name__ == "__main__":
    main()
