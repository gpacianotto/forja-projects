"""Geometria das logos (Forja Instruments e Matiza).

Tudo é desenhado num módulo comum, para as duas marcas falarem a mesma língua de "blocos forjados":
    H = altura das letras, S = espessura das hastes verticais, B = espessura das barras horizontais,
    G = espaço entre letras.
As formas são unidas com shapely e exportadas como um único <path> por cor.
"""
from shapely.geometry import Polygon, box, MultiPolygon
from shapely.ops import unary_union
from shapely import affinity
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.recordingPen import DecomposingRecordingPen

H, S, B, G = 520, 200, 130, 110

# ---------------------------------------------------------------- utilidades

def to_path(geom, dx=0.0, dy=0.0, scale=1.0):
    """Shapely -> atributo d de um <path> (com furos, regra evenodd)."""
    polys = [geom] if isinstance(geom, Polygon) else list(getattr(geom, "geoms", []))
    out = []
    for p in polys:
        for ring in [p.exterior, *p.interiors]:
            pts = list(ring.coords)[:-1]
            out.append("M" + " L".join(f"{dx + x * scale:.2f},{dy + y * scale:.2f}" for x, y in pts) + " Z")
    return " ".join(out)


def chamfer_box(x0, y0, x1, y1, c, corners="tl tr br bl"):
    """Retângulo com cantos chanfrados (corte em 45°), como uma barra forjada."""
    c = min(c, (x1 - x0) / 2, (y1 - y0) / 2)
    pts = []
    pts += [(x0, y0 + c), (x0 + c, y0)] if "tl" in corners else [(x0, y0)]
    pts += [(x1 - c, y0), (x1, y0 + c)] if "tr" in corners else [(x1, y0)]
    pts += [(x1, y1 - c), (x1 - c, y1)] if "br" in corners else [(x1, y1)]
    pts += [(x0 + c, y1), (x0, y1 - c)] if "bl" in corners else [(x0, y1)]
    return Polygon(pts)


def width(geom):
    x0, _, x1, _ = geom.bounds
    return x1 - x0

# ---------------------------------------------------------------- Matiza: MTZ refinado

def mtz():
    """Sigla MTZ refinada a partir do esboço do Guilherme.

    Mantém a ideia do original (M, T e Z presos numa barra superior contínua, letras pesadas e
    largas, só retas e diagonais) e acerta: hastes com a mesma espessura, barras com a mesma
    espessura, diagonais do M paralelas, e o mesmo espaço entre a haste do T e as vizinhas
    (no esboço o Z quase encostava no T).
    """
    parts = []
    # M (largura 980)
    WM = 980
    a = 255            # largura do topo da haste + começo da diagonal
    v = 370            # ponta do V do M (altura)
    c = 80             # meia largura da base do V
    slope = (WM / 2 - a) / v                      # diagonal externa: dx por dy
    h1 = 520 - ((WM / 2 - c) - S) / slope         # onde a diagonal interna sai da haste (paralela)
    left = Polygon([(0, 0), (a, 0), (WM / 2, v), (WM / 2 - c, H), (WM / 2 - c, H),
                    (S, h1), (S, H), (0, H)])
    left = Polygon([(0, 0), (a, 0), (WM / 2, v), (WM / 2, H), (WM / 2 - c, H), (S, h1), (S, H), (0, H)])
    right = affinity.scale(left, xfact=-1, origin=(WM / 2, 0))
    parts += [left, right]
    # barra superior contínua: do topo da haste direita do M até o fim do Z
    t0 = WM + G
    t1 = t0 + S                                    # haste do T
    z0 = t1 + G
    zw = 600
    z1 = z0 + zw
    parts.append(box(WM - a, 0, z1, B))
    parts.append(box(t0, 0, t1, H))
    # Z: diagonal com a mesma largura horizontal da diagonal do M, barra de baixo
    d = 270
    parts.append(Polygon([(z1 - d, B), (z1, B), (z0 + d, H - B), (z0, H - B)]))
    parts.append(box(z0, H - B, z1, H))
    return unary_union(parts)

# ---------------------------------------------------------------- Forja: letras no mesmo módulo

CH = 120  # chanfro dos cantos externos (o "corte do martelo"), igual em todas as letras


def letter_F():
    W = 500
    return unary_union([
        chamfer_box(0, 0, W, B, CH, "tr"),
        box(0, 0, S, H),
        box(0, 205, W - 90, 205 + B),
    ])


def letter_O():
    W = 600
    outer = chamfer_box(0, 0, W, H, CH)
    inner = box(S, B, W - S, H - B)
    return outer.difference(inner)


def letter_R():
    W = 560
    bowl_h = 330
    bowl = chamfer_box(0, 0, W, bowl_h, CH, "tr").difference(box(S, B, W - S, bowl_h - B))
    stem = box(0, 0, S, H)
    leg = Polygon([(S + 60, bowl_h - B), (S + 260, bowl_h - B), (W, H), (W - 200, H)])
    return unary_union([bowl, stem, leg])


def letter_J():
    W = 500
    return unary_union([
        box(W - S, 0, W, H - B + 1),
        chamfer_box(0, H - B, W, H, CH, "br"),
        box(0, 300, S, H),
    ])


def letter_A():
    W = 600
    outer = chamfer_box(0, 0, W, H, CH, "tl tr")
    counter = box(S, B, W - S, 300)
    legs = box(S, 300 + B, W - S, H + 1)
    return outer.difference(counter).difference(legs)


def word(letters, gap=G):
    out, x = [], 0.0
    for g in letters:
        gx0 = g.bounds[0]
        out.append(affinity.translate(g, x - gx0, 0))
        x += width(g) + gap
    return unary_union(out)


def forja_word():
    return word([letter_F(), letter_O(), letter_R(), letter_J(), letter_A()])

# ---------------------------------------------------------------- texto (Inter, licença OFL) em curvas

def text_path(text, font_path, size, tracking=0.0):
    """Texto convertido em curvas (a logo não depende da fonte instalada). Devolve (d, largura)."""
    font = TTFont(font_path)
    gs = font.getGlyphSet()
    cmap = font.getBestCmap()
    upm = font["head"].unitsPerEm
    scale = size / upm
    hmtx = font["hmtx"]
    pen = SVGPathPen(gs)
    x = 0.0
    for ch in text:
        name = cmap.get(ord(ch))
        if name is None:
            continue
        # y para baixo no SVG: espelha verticalmente
        tp = TransformPen(pen, (scale, 0, 0, -scale, x, 0))
        gs[name].draw(tp)
        x += hmtx[name][0] * scale + tracking
    return pen.getCommands(), x - tracking
