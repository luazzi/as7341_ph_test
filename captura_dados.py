"""
Captura de dados do AS7341 via serial USB.

Conecta à porta serial da BitDogLab/Pico, captura as linhas CSV
emitidas pelo firmware e salva em um arquivo .csv.

Uso:
    python captura_dados.py              (detecta porta automaticamente)
    python captura_dados.py COM5         (porta específica)

Teclas durante execução (sem precisar apertar Enter):
    a -> pH Ácido
    n -> pH Neutro
    b -> pH Base
    r -> Reset contador
    q -> Sair e salvar

Dependências:
    pip install pyserial
"""

import serial
import serial.tools.list_ports
import sys
import os
import csv
import msvcrt
from datetime import datetime


# ============================================================================
# Configuração
# ============================================================================
BAUD_RATE = 115200
CSV_HEADER = [
    "Num", "Tipo_pH", "LED_Cor", "LED_R", "LED_G", "LED_B",
    "F1_415nm", "F2_445nm", "F3_480nm", "F4_515nm",
    "F5_555nm", "F6_590nm", "F7_630nm", "F8_680nm",
    "Clear", "NIR"
]


def encontrar_porta_pico():
    """Detecta automaticamente a porta serial do Pico."""
    portas = serial.tools.list_ports.comports()
    for p in portas:
        desc = (p.description or "").lower()
        vid_pid = f"{p.vid:04X}:{p.pid:04X}" if p.vid and p.pid else ""
        if "2e8a" in vid_pid.lower() or "pico" in desc or "board in fs mode" in desc:
            return p.device
    if portas:
        print(f"[Aviso] Pico nao detectado automaticamente.")
        print(f"  Portas disponiveis:")
        for p in portas:
            print(f"    {p.device} - {p.description}")
        return portas[0].device
    return None


def main():
    # Determina a porta serial
    if len(sys.argv) > 1:
        porta = sys.argv[1]
    else:
        porta = encontrar_porta_pico()
        if not porta:
            print("ERRO: Nenhuma porta serial encontrada!")
            sys.exit(1)

    # Nome do arquivo CSV com timestamp
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    nome_csv = f"medicoes_ph_{timestamp}.csv"

    print("=" * 50)
    print("  Captura de Dados AS7341 - Teste pH")
    print("=" * 50)
    print(f"  Porta:   {porta}")
    print(f"  Arquivo: {nome_csv}")
    print()
    print("  Teclas (instantaneas, sem Enter):")
    print("    a = pH Acido")
    print("    n = pH Neutro")
    print("    b = pH Base")
    print("    r = Reset contador")
    print("    q = Sair e salvar")
    print("=" * 50)
    print()

    # Abre a porta serial
    try:
        ser = serial.Serial(porta, BAUD_RATE, timeout=0.1)
    except serial.SerialException as e:
        print(f"ERRO: Nao foi possivel abrir {porta}: {e}")
        print("  Feche o Serial Monitor ou outro programa que use essa porta.")
        sys.exit(1)

    # Abre o arquivo CSV
    csvfile = open(nome_csv, 'w', newline='', encoding='utf-8')
    writer = csv.writer(csvfile)
    writer.writerow(CSV_HEADER)
    csvfile.flush()

    medicoes = 0
    running = True

    print("[Aguardando dados... Pressione botao B na BitDogLab para medir]\n")

    try:
        while running:
            # --- Verifica teclado (non-blocking, sem Enter) ---
            if msvcrt.kbhit():
                tecla = msvcrt.getch().decode('utf-8', errors='ignore').lower()
                if tecla == 'q':
                    break
                elif tecla in ('a', 'n', 'b', 'r', 'h'):
                    ser.write(tecla.encode())
                    nomes = {'a': 'ACIDO', 'n': 'NEUTRO', 'b': 'BASE',
                             'r': 'RESET', 'h': 'AJUDA'}
                    print(f"  >> Enviado: {nomes.get(tecla, tecla)}")

            # --- Lê serial (non-blocking, timeout=0.1s) ---
            try:
                raw = ser.readline()
                if not raw:
                    continue

                line = raw.decode('utf-8', errors='replace').strip()
                if not line:
                    continue

                # Linha CSV do firmware: "CSV,1,Acido,Vermelho,255,0,0,..."
                if line.startswith("CSV,") and not line.startswith("CSV_HEADER"):
                    parts = line[4:].split(",")  # Remove o prefixo "CSV,"
                    if len(parts) == len(CSV_HEADER):
                        writer.writerow(parts)
                        csvfile.flush()
                        medicoes += 1
                        print(f"  >> Medicao #{parts[0]} SALVA | "
                              f"pH={parts[1]} | LED={parts[2]} | "
                              f"F1={parts[6]} F8={parts[13]}")
                    else:
                        print(f"  [CSV: {len(parts)} campos, esperado {len(CSV_HEADER)}]")
                else:
                    # Imprime outras linhas normalmente
                    print(line)

            except serial.SerialException:
                print("\n[ERRO] Conexao serial perdida!")
                break

    except KeyboardInterrupt:
        print("\n[Ctrl+C]")

    finally:
        csvfile.close()
        ser.close()
        print(f"\n{'=' * 50}")
        print(f"  Total de medicoes salvas: {medicoes}")
        print(f"  Arquivo: {os.path.abspath(nome_csv)}")
        print(f"{'=' * 50}")


if __name__ == "__main__":
    main()
