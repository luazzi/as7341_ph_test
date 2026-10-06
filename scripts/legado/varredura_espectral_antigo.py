"""
Varredura espectral automática com LEDs coloridos.

Para cada cor correspondente a um canal espectral do AS7341,
acende os LEDs, realiza 5 leituras e salva os resultados.
No final, plota os espectros em um gráfico.

Uso:
    python varredura_espectral.py              (detecta porta automaticamente)
    python varredura_espectral.py COM8         (porta específica)

Requisitos:
    pip install pyserial matplotlib numpy

IMPORTANTE: Grave o firmware atualizado (build/a734x_test.uf2) na Pico
antes de executar este script.
"""

import serial
import serial.tools.list_ports
import sys
import os
import csv
import time
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime


# ============================================================================
# Configuração
# ============================================================================
BAUD_RATE = 115200
LEITURAS_POR_COR = 5
ESPERA_TROCA_COR = 1.0    # Segundos para estabilizar após trocar cor
ESPERA_ENTRE_LEITURAS = 0.5  # Segundos entre leituras

# Mapeamento: canal espectral → índice na paleta do firmware
# A paleta do firmware é:
#   0=Vermelho, 1=Verde, 2=Azul, 3=Amarelo, 4=Magenta,
#   5=Ciano, 6=Branco, 7=Laranja, 8=Violeta, 9=Rosa
CORES_ESPECTRAIS = [
    {"nome": "Violeta",  "idx": 8, "canal": "F1", "nm": 415, "rgb": (128,   0, 255)},
    {"nome": "Azul",     "idx": 2, "canal": "F2", "nm": 445, "rgb": (  0,   0, 255)},
    {"nome": "Ciano",    "idx": 5, "canal": "F3", "nm": 480, "rgb": (  0, 255, 255)},
    {"nome": "Verde",    "idx": 1, "canal": "F4", "nm": 515, "rgb": (  0, 255,   0)},
    {"nome": "Amarelo",  "idx": 3, "canal": "F6", "nm": 590, "rgb": (255, 255,   0)},
    {"nome": "Laranja",  "idx": 7, "canal": "F7", "nm": 630, "rgb": (255, 128,   0)},
    {"nome": "Vermelho", "idx": 0, "canal": "F8", "nm": 680, "rgb": (255,   0,   0)},
    {"nome": "Branco",   "idx": 6, "canal": "-",  "nm": 0,   "rgb": (255, 255, 255)},
]

# Colunas CSV
CSV_HEADER = [
    "Num", "Tipo_pH", "LED_Cor", "LED_R", "LED_G", "LED_B",
    "F1_415nm", "F2_445nm", "F3_480nm", "F4_515nm",
    "F5_555nm", "F6_590nm", "F7_630nm", "F8_680nm",
    "Clear", "NIR"
]

# Nomes curtos dos canais espectrais para gráficos
CANAIS = ["F1\n415nm", "F2\n445nm", "F3\n480nm", "F4\n515nm",
          "F5\n555nm", "F6\n590nm", "F7\n630nm", "F8\n680nm",
          "Clear", "NIR"]


def encontrar_porta_pico():
    """Detecta automaticamente a porta serial do Pico."""
    portas = serial.tools.list_ports.comports()
    for p in portas:
        desc = (p.description or "").lower()
        vid_pid = f"{p.vid:04X}:{p.pid:04X}" if p.vid and p.pid else ""
        if "2e8a" in vid_pid.lower() or "pico" in desc or "board in fs mode" in desc:
            return p.device
    if portas:
        return portas[0].device
    return None


def ler_linhas(ser, timeout=5.0):
    """Lê todas as linhas disponíveis no serial até timeout."""
    linhas = []
    inicio = time.time()
    while time.time() - inicio < timeout:
        raw = ser.readline()
        if raw:
            line = raw.decode('utf-8', errors='replace').strip()
            if line:
                linhas.append(line)
                inicio = time.time()  # Reset timeout quando recebe dados
        else:
            if linhas:  # Já recebeu algo e parou
                break
    return linhas


def enviar_comando(ser, cmd):
    """Envia um caractere de comando e aguarda resposta."""
    ser.write(cmd.encode())
    time.sleep(0.1)


def trocar_cor(ser, idx):
    """Envia comando para trocar a cor pelo índice."""
    enviar_comando(ser, str(idx))
    time.sleep(ESPERA_TROCA_COR)
    # Limpa buffer
    ler_linhas(ser, timeout=0.5)


def disparar_medicao(ser):
    """Envia comando 'm' e captura a linha CSV resultante."""
    ser.reset_input_buffer()
    enviar_comando(ser, 'm')

    # Aguarda e captura a resposta (inclui a linha CSV)
    linhas = ler_linhas(ser, timeout=8.0)

    # Procura a linha CSV
    for linha in linhas:
        if linha.startswith("CSV,") and not linha.startswith("CSV_HEADER"):
            parts = linha[4:].split(",")
            if len(parts) == len(CSV_HEADER):
                return parts

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

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    nome_csv = f"varredura_espectral_{timestamp}.csv"

    print("=" * 55)
    print("  Varredura Espectral Automatica - AS7341")
    print("=" * 55)
    print(f"  Porta:          {porta}")
    print(f"  Leituras/cor:   {LEITURAS_POR_COR}")
    print(f"  Cores:          {len(CORES_ESPECTRAIS)}")
    print(f"  Total medicoes: {LEITURAS_POR_COR * len(CORES_ESPECTRAIS)}")
    print(f"  Arquivo:        {nome_csv}")
    print("=" * 55)
    print()

    # Conecta ao serial
    try:
        ser = serial.Serial(porta, BAUD_RATE, timeout=0.5)
    except serial.SerialException as e:
        print(f"ERRO: Nao foi possivel abrir {porta}: {e}")
        print("  Feche o Serial Monitor ou outro programa que use essa porta.")
        sys.exit(1)

    # Aguarda o Pico inicializar
    print("[Aguardando Pico inicializar...]")
    time.sleep(2)
    ler_linhas(ser, timeout=2.0)  # Limpa buffer de boot

    # Abre CSV
    csvfile = open(nome_csv, 'w', newline='', encoding='utf-8')
    writer = csv.writer(csvfile)
    writer.writerow(CSV_HEADER)

    # Estrutura para armazenar resultados
    todos_dados = {}  # {nome_cor: [lista de medições]}

    total = LEITURAS_POR_COR * len(CORES_ESPECTRAIS)
    medicao_global = 0

    print("\n[Iniciando varredura...]\n")

    for cor_info in CORES_ESPECTRAIS:
        nome = cor_info["nome"]
        idx = cor_info["idx"]
        nm = cor_info["nm"]

        print(f"--- {nome} (indice {idx}) ---")

        # Troca a cor
        trocar_cor(ser, idx)
        todos_dados[nome] = []

        for i in range(LEITURAS_POR_COR):
            medicao_global += 1
            sys.stdout.write(f"  Leitura {i+1}/{LEITURAS_POR_COR}... ")
            sys.stdout.flush()

            resultado = disparar_medicao(ser)

            if resultado:
                writer.writerow(resultado)
                csvfile.flush()

                # Extrai valores espectrais (índices 6-15 do CSV)
                valores = [int(v) for v in resultado[6:]]
                todos_dados[nome].append(valores)

                print(f"OK  (F1={valores[0]}, F4={valores[3]}, F8={valores[7]})")
            else:
                print("FALHA - sem dados CSV")

            if i < LEITURAS_POR_COR - 1:
                time.sleep(ESPERA_ENTRE_LEITURAS)

        print()

    csvfile.close()
    ser.close()

    print(f"[Concluido] {medicao_global} medicoes salvas em {nome_csv}\n")

    # ========================================================================
    # Gerar gráficos
    # ========================================================================
    print("[Gerando graficos...]\n")

    # Comprimentos de onda dos canais
    wavelengths = [415, 445, 480, 515, 555, 590, 630, 680]
    canal_labels = ["F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8"]

    # Cores para cada LED no gráfico (RGB normalizado)
    cores_plot = {
        "Violeta":  (0.5, 0.0, 1.0),
        "Azul":     (0.0, 0.0, 1.0),
        "Ciano":    (0.0, 0.8, 0.8),
        "Verde":    (0.0, 0.7, 0.0),
        "Amarelo":  (0.9, 0.9, 0.0),
        "Laranja":  (1.0, 0.5, 0.0),
        "Vermelho": (1.0, 0.0, 0.0),
        "Branco":   (0.5, 0.5, 0.5),
    }

    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle("Varredura Espectral AS7341 - Resposta por Cor de Iluminação",
                 fontsize=14, fontweight='bold')

    # --- Gráfico 1: Espectro médio por cor (linhas) ---
    ax1 = axes[0, 0]
    for nome, medicoes in todos_dados.items():
        if not medicoes:
            continue
        arr = np.array(medicoes)[:, :8]  # Só F1-F8
        media = arr.mean(axis=0)
        desvio = arr.std(axis=0)
        cor = cores_plot.get(nome, (0.5, 0.5, 0.5))
        ax1.errorbar(wavelengths, media, yerr=desvio, marker='o',
                     label=nome, color=cor, linewidth=2, capsize=3)

    ax1.set_xlabel("Comprimento de onda (nm)")
    ax1.set_ylabel("Contagem bruta (ADC)")
    ax1.set_title("Espectro médio por cor de iluminação")
    ax1.legend(fontsize=8, loc='upper right')
    ax1.grid(True, alpha=0.3)
    ax1.set_xticks(wavelengths)

    # --- Gráfico 2: Barras agrupadas por canal ---
    ax2 = axes[0, 1]
    nomes_cores = [c["nome"] for c in CORES_ESPECTRAIS if c["nome"] in todos_dados and todos_dados[c["nome"]]]
    n_cores = len(nomes_cores)
    x = np.arange(8)
    largura = 0.8 / max(n_cores, 1)

    for i, nome in enumerate(nomes_cores):
        arr = np.array(todos_dados[nome])[:, :8]
        media = arr.mean(axis=0)
        cor = cores_plot.get(nome, (0.5, 0.5, 0.5))
        offset = (i - n_cores / 2 + 0.5) * largura
        ax2.bar(x + offset, media, largura, label=nome, color=cor, alpha=0.85)

    ax2.set_xlabel("Canal espectral")
    ax2.set_ylabel("Contagem média (ADC)")
    ax2.set_title("Resposta por canal e cor de iluminação")
    ax2.set_xticks(x)
    ax2.set_xticklabels([f"{c}\n{w}nm" for c, w in zip(canal_labels, wavelengths)], fontsize=8)
    ax2.legend(fontsize=7, loc='upper right')
    ax2.grid(True, alpha=0.3, axis='y')

    # --- Gráfico 3: Heatmap normalizado ---
    ax3 = axes[1, 0]
    nomes_heatmap = [c["nome"] for c in CORES_ESPECTRAIS if c["nome"] in todos_dados and todos_dados[c["nome"]]]
    if nomes_heatmap:
        matrix = []
        for nome in nomes_heatmap:
            arr = np.array(todos_dados[nome])[:, :8]
            media = arr.mean(axis=0)
            # Normaliza cada cor pelo seu máximo
            if media.max() > 0:
                matrix.append(media / media.max())
            else:
                matrix.append(media)
        matrix = np.array(matrix)

        im = ax3.imshow(matrix, aspect='auto', cmap='YlOrRd')
        ax3.set_xticks(range(8))
        ax3.set_xticklabels([f"{c}\n{w}nm" for c, w in zip(canal_labels, wavelengths)], fontsize=8)
        ax3.set_yticks(range(len(nomes_heatmap)))
        ax3.set_yticklabels(nomes_heatmap)
        ax3.set_title("Heatmap normalizado (cada cor / seu máximo)")
        fig.colorbar(im, ax=ax3, shrink=0.8)

    # --- Gráfico 4: Clear e NIR ---
    ax4 = axes[1, 1]
    nomes_bar = [c["nome"] for c in CORES_ESPECTRAIS if c["nome"] in todos_dados and todos_dados[c["nome"]]]
    if nomes_bar:
        clear_vals = []
        nir_vals = []
        for nome in nomes_bar:
            arr = np.array(todos_dados[nome])
            clear_vals.append(arr[:, 8].mean())  # Clear
            nir_vals.append(arr[:, 9].mean())     # NIR

        x_bar = np.arange(len(nomes_bar))
        ax4.bar(x_bar - 0.2, clear_vals, 0.35, label='Clear', color='gold', alpha=0.8)
        ax4.bar(x_bar + 0.2, nir_vals, 0.35, label='NIR', color='darkred', alpha=0.8)
        ax4.set_xticks(x_bar)
        ax4.set_xticklabels(nomes_bar, rotation=30, fontsize=9)
        ax4.set_ylabel("Contagem média (ADC)")
        ax4.set_title("Canais Clear e NIR por cor de iluminação")
        ax4.legend()
        ax4.grid(True, alpha=0.3, axis='y')

    plt.tight_layout()

    nome_grafico = f"varredura_espectral_{timestamp}.png"
    plt.savefig(nome_grafico, dpi=150, bbox_inches='tight')
    print(f"  Grafico salvo: {os.path.abspath(nome_grafico)}")

    plt.show()

    print("\n[Finalizado!]")


if __name__ == "__main__":
    main()
