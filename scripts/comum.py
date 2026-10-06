"""Funcoes compartilhadas pelos scripts de captura.

Todos os resultados sao salvos em ``resultados/`` na raiz do projeto,
independentemente do diretorio de onde o script foi executado:

    resultados/<experimento>/<AAAAMMDD_HHMMSS>__<configuracao>/
        dados.csv     medicoes brutas
        config.json   configuracao usada na execucao
        *.png         graficos (quando houver)
"""

import json
import re
import unicodedata
from datetime import datetime
from pathlib import Path

RAIZ_PROJETO = Path(__file__).resolve().parent.parent
PASTA_RESULTADOS = RAIZ_PROJETO / "resultados"


def slug(texto):
    """Converte um texto livre em um nome seguro para pastas."""
    texto = unicodedata.normalize("NFKD", str(texto)).encode("ascii", "ignore").decode()
    texto = re.sub(r"[^A-Za-z0-9.\-]+", "_", texto).strip("_")
    return texto or "sem_nome"


def criar_pasta_execucao(experimento, partes_config):
    """Cria e retorna resultados/<experimento>/<data_hora>__<config>/."""
    data_hora = datetime.now().strftime("%Y%m%d_%H%M%S")
    partes = [slug(p) for p in partes_config if p]
    nome = data_hora + ("__" + "_".join(partes) if partes else "")
    pasta = PASTA_RESULTADOS / experimento / nome
    pasta.mkdir(parents=True, exist_ok=False)
    return pasta


def salvar_config(pasta, config):
    """Grava config.json com a configuracao da execucao."""
    config = {"data_hora": datetime.now().isoformat(timespec="seconds"), **config}
    with open(pasta / "config.json", "w", encoding="utf-8") as arquivo:
        json.dump(config, arquivo, indent=2, ensure_ascii=False)


def caminho_relativo(caminho):
    """Caminho relativo a raiz do projeto, para mensagens no terminal."""
    try:
        return Path(caminho).resolve().relative_to(RAIZ_PROJETO)
    except ValueError:
        return Path(caminho)


def encontrar_porta_pico():
    """Detecta a porta serial da Pico (VID 2E8A); retorna None se nao achar."""
    import serial.tools.list_ports

    portas = list(serial.tools.list_ports.comports())
    for porta in portas:
        if porta.vid == 0x2E8A or "pico" in (porta.description or "").lower():
            return porta.device
    return portas[0].device if len(portas) == 1 else None
