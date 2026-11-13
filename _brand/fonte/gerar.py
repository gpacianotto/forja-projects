"""Gera os arquivos da marca (Forja Instruments e Matiza) a partir da geometria em forge.py.

Uso (na pasta _brand/fonte):
    pip install shapely fonttools cairosvg
    python gerar.py

Escreve os SVGs em ../forja e ../matiza e, se o cairosvg estiver instalado, os PNGs em
../forja/png e ../matiza/png. Para ajustar uma logo, mude os números em forge.py
(H, S, B, G, CH ou as letras) e rode de novo: nunca edite os SVGs gerados à mão.

Logos aprovadas em 12/11/2026: Forja = conceito A (Bigorna); Matiza = MTZ refinado.
"""
import os
import sys

from shapely.geometry import Polygon, box
from shapely.ops import unary_union

from forge import CH, H, chamfer_box, forja_word, mtz, text_path, to_path, width

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
FONTE_INTER = os.path.join(AQUI, "fonts", "Inter-SemiBold.otf")

# Paleta Têmpera (mesmos valores de Matiza/src/Palette.h)
FERRO = "#14161b"   # fundo
TEXTO = "#e9e4da"   # claro
PALHA = "#d9a441"   # principal
TINTA = "#121317"   # escuro (logo sobre fundo claro)
CINZA_ESCURO = "#55575e"
CINZA_CLARO = "#9a9da6"

# ---------------------------------------------------------------- formas

def bigorna():
    """Símbolo da Forja (conceito A): a bigorna, no mesmo corte em blocos das letras."""
    return unary_union([
        Polygon([(0, 0), (190, 0), (190, 150)]),                 # chifre (bico)
        chamfer_box(180, 0, 720, 150, CH, "tr"),                 # mesa
        box(310, 140, 590, 320),                                 # cintura
        Polygon([(310, 310), (590, 310), (690, 420), (210, 420)]),
        box(160, 420, 740, 520),                                 # base
    ])

# ---------------------------------------------------------------- SVG

def svg(w, h, corpo, fundo=None, raio=0):
    rect = ""
    if fundo:
        rect = f'<rect width="{w:.0f}" height="{h:.0f}" rx="{raio}" fill="{fundo}"/>'
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:.0f} {h:.0f}">'
            f'{rect}{corpo}</svg>\n')


def caminho(geom, cor, dx=0, dy=0, k=1.0):
    return f'<path d="{to_path(geom, dx, dy, k)}" fill="{cor}" fill-rule="evenodd"/>'


def salvar(pasta, nome, conteudo):
    os.makedirs(pasta, exist_ok=True)
    with open(os.path.join(pasta, nome), "w", encoding="utf-8", newline="\n") as f:
        f.write(conteudo)
    return os.path.join(pasta, nome)

# ---------------------------------------------------------------- Forja

def forja_horizontal(cor, cor_sub):
    """Bigorna + FORJA + INSTRUMENTS (Inter SemiBold em curvas, largura igual à de FORJA)."""
    palavra = forja_word()
    pw = width(palavra)
    d0, w0 = text_path("INSTRUMENTS", FONTE_INTER, 150)
    tracking = (pw - w0) / (len("INSTRUMENTS") - 1)
    sub_d, _ = text_path("INSTRUMENTS", FONTE_INTER, 150, tracking)
    bloco = H + 120 + 110
    sym = bigorna()
    x0, y0, x1, y1 = sym.bounds
    k = bloco / (y1 - y0)
    sw = (x1 - x0) * k
    corpo = caminho(sym, cor, dx=-x0 * k, dy=-y0 * k, k=k)
    x = sw + 170
    corpo += caminho(palavra, cor, dx=x)
    corpo += f'<path d="{sub_d}" fill="{cor_sub}" transform="translate({x:.1f},{bloco:.1f})"/>'
    return corpo, x + pw, bloco


def forja_simbolo(cor, tamanho=520, margem=0):
    sym = bigorna()
    x0, y0, x1, y1 = sym.bounds
    util = tamanho - 2 * margem
    k = util / max(x1 - x0, y1 - y0)
    ox = margem + (util - (x1 - x0) * k) / 2
    oy = margem + (util - (y1 - y0) * k) / 2
    return caminho(sym, cor, dx=-x0 * k + ox, dy=-y0 * k + oy, k=k)

# ---------------------------------------------------------------- Matiza

def matiza_assinatura(cor, cor_sub, m):
    _, my0, _, my1 = m.bounds
    mw, mh = width(m), my1 - my0
    nome_d, _ = text_path("MATIZA", FONTE_INTER, 140, 38)
    by_d, by_w = text_path("by Forja Instruments", FONTE_INTER, 100, 1)
    corpo = caminho(m, cor)
    corpo += f'<path d="{nome_d}" fill="{cor}" transform="translate(0,{mh + 220})"/>'
    corpo += f'<path d="{by_d}" fill="{cor_sub}" transform="translate({mw - by_w:.1f},{mh + 220})"/>'
    return corpo, mw, mh + 260


def matiza_icone(m, tamanho=2000):
    _, my0, _, my1 = m.bounds
    mw, mh = width(m), my1 - my0
    k = (tamanho * 0.92) / mw
    y = (tamanho - mh * k) / 2 + tamanho * 0.07
    x = (tamanho - mw * k) / 2
    defs = (f'<defs><linearGradient id="g" x1="0" y1="0" x2="0.12" y2="1">'
            f'<stop offset="0" stop-color="{FERRO}"/><stop offset="0.38" stop-color="{TEXTO}"/>'
            f'<stop offset="0.62" stop-color="#f7ecd6"/><stop offset="1" stop-color="{PALHA}"/>'
            f'</linearGradient></defs>')
    corpo = defs + f'<rect width="{tamanho}" height="{tamanho}" fill="url(#g)"/>' + caminho(m, TINTA, dx=x, dy=y, k=k)
    return corpo

# ---------------------------------------------------------------- PNG

def png(svg_path, png_path, largura):
    try:
        import cairosvg
    except ImportError:
        return False
    os.makedirs(os.path.dirname(png_path), exist_ok=True)
    cairosvg.svg2png(url=svg_path, write_to=png_path, output_width=largura)
    return True


def main():
    forja = os.path.join(RAIZ, "forja")
    matiza = os.path.join(RAIZ, "matiza")
    pad = 120

    # Forja: logo horizontal
    c, w, h = forja_horizontal(TINTA, CINZA_ESCURO)
    a = salvar(forja, "forja-horizontal-escura.svg", svg(w, h, c))                    # para fundo claro
    c2, _, _ = forja_horizontal(TEXTO, CINZA_CLARO)
    b = salvar(forja, "forja-horizontal-clara.svg", svg(w, h, c2))                    # para fundo escuro
    c3 = f'<g transform="translate({pad},{pad})">{c2}</g>'
    salvar(forja, "forja-horizontal-sobre-ferro.svg", svg(w + 2 * pad, h + 2 * pad, c3, FERRO))

    # Forja: símbolo
    for nome, cor in (("escuro", TINTA), ("claro", TEXTO), ("palha", PALHA)):
        salvar(forja, f"forja-simbolo-{nome}.svg", svg(520, 520, forja_simbolo(cor)))
    av = salvar(forja, "forja-avatar.svg", svg(920, 920, forja_simbolo(PALHA, 920, 200), FERRO))

    # Matiza
    m = mtz()
    _, my0, _, my1 = m.bounds
    mw, mh = width(m), my1 - my0
    for nome, cor in (("escuro", TINTA), ("claro", TEXTO), ("palha", PALHA)):
        salvar(matiza, f"mtz-{nome}.svg", svg(mw, mh, caminho(m, cor)))
    pm = 140
    salvar(matiza, "mtz-sobre-ferro.svg", svg(mw + 2 * pm, mh + 2 * pm, caminho(m, PALHA, dx=pm, dy=pm), FERRO))
    ic = salvar(matiza, "matiza-icone.svg", svg(2000, 2000, matiza_icone(m)))
    c, w, h = matiza_assinatura(TINTA, CINZA_ESCURO, m)
    salvar(matiza, "matiza-assinatura-escura.svg", svg(w, h, c))
    c, w, h = matiza_assinatura(TEXTO, CINZA_CLARO, m)
    salvar(matiza, "matiza-assinatura-clara.svg", svg(w, h, c))

    # PNGs
    feitos = True
    for s in (16, 32, 48, 64, 128, 256, 512):
        feitos &= png(av, os.path.join(forja, "png", f"forja-avatar-{s}.png"), s)
        feitos &= png(ic, os.path.join(matiza, "png", f"matiza-icone-{s}.png"), s)
    for largura in (600, 1600):
        feitos &= png(a, os.path.join(forja, "png", f"forja-horizontal-escura-{largura}.png"), largura)
        feitos &= png(b, os.path.join(forja, "png", f"forja-horizontal-clara-{largura}.png"), largura)
        feitos &= png(os.path.join(matiza, "mtz-palha.svg"), os.path.join(matiza, "png", f"mtz-palha-{largura}.png"), largura)
        feitos &= png(os.path.join(matiza, "mtz-escuro.svg"), os.path.join(matiza, "png", f"mtz-escuro-{largura}.png"), largura)
    if not feitos:
        print("cairosvg não instalado: só os SVGs foram gerados.", file=sys.stderr)
    print("ok")


if __name__ == "__main__":
    main()
