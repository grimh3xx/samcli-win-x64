"""
gen_logo.py - converte assets/logo.png em arte de terminal (src/logo_data.h).

Cada caractere do terminal vira 2x2 "subpixels" usando os caracteres
quadrant do Unicode (U+2596..U+259F, U+2580/2584/258C/2590/2588), com
cor RGB 24-bit. Como a celula do terminal e ~2x mais alta que larga,
cada subpixel amostra uma area 1:2 da imagem, mantendo a proporcao.

Uso:
    python tools/gen_logo.py [logo.png] [saida.h] [--preview preview.png]
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
args = [a for a in sys.argv[1:] if not a.startswith('--')]
SRC = Path(args[0]) if len(args) > 0 else ROOT / 'assets' / 'logo.png'
OUT = Path(args[1]) if len(args) > 1 else ROOT / 'src' / 'logo_data.h'
PREVIEW = sys.argv[sys.argv.index('--preview') + 1] if '--preview' in sys.argv else None

img = np.asarray(Image.open(SRC).convert('RGB')).astype(float)
MASK = img.max(axis=2) > 90          # pixel "aceso" (o fundo e azul-marinho escuro)
THRESHOLD = 0.45                     # cobertura minima pra acender um subpixel
QSTEP = 16                           # quantizacao da paleta

# Recortes do arquivo original 1254x1254 (x0, y0, x1, y1)
ICON = (429, 282, 870, 726)
WORD = (213, 752, 1053, 851)


def sample(box, gw, gh):
    x0, y0, x1, y1 = box
    on = np.zeros((gh, gw), bool)
    col = np.zeros((gh, gw, 3))
    for j in range(gh):
        ya = int(y0 + (y1 - y0) * j / gh)
        yb = max(ya + 1, int(y0 + (y1 - y0) * (j + 1) / gh))
        for i in range(gw):
            xa = int(x0 + (x1 - x0) * i / gw)
            xb = max(xa + 1, int(x0 + (x1 - x0) * (i + 1) / gw))
            m = MASK[ya:yb, xa:xb]
            on[j, i] = m.mean() >= THRESHOLD
            if m.any():
                col[j, i] = img[ya:yb, xa:xb][m].mean(axis=0)
    return on, col


def quad_cells(box, cols, rows):
    on, col = sample(box, cols * 2, rows * 2)
    grid = []
    for r in range(rows):
        line = []
        for c in range(cols):
            bits, cs = 0, []
            for k, (dy, dx) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1))):
                if on[r * 2 + dy, c * 2 + dx]:
                    bits |= 1 << k
                    cs.append(col[r * 2 + dy, c * 2 + dx])
            line.append((bits, tuple(np.mean(cs, axis=0)) if cs else None))
        grid.append(line)
    return grid


def word_cols(rows):
    return round((WORD[2] - WORD[0]) / ((WORD[3] - WORD[1]) / (rows * 2)))


IMAGES = {
    'ICON_L': quad_cells(ICON, 32, 16),
    'WORD_L': quad_cells(WORD, word_cols(5), 5),
    'ICON_S': quad_cells(ICON, 28, 14),
    'WORD_S': quad_cells(WORD, word_cols(4), 4),
}

palette = []


def color_index(rgb):
    q = tuple(min(255, int(round(v / QSTEP)) * QSTEP) for v in rgb)
    if q not in palette:
        palette.append(q)
    return palette.index(q)


def c_array(values, width):
    rows = [','.join(str(v) for v in values[i:i + width]) for i in range(0, len(values), width)]
    return ',\n    '.join(rows)


body = []
for name, grid in IMAGES.items():
    R, C = len(grid), len(grid[0])
    bits = [b for line in grid for b, _ in line]
    cols = [color_index(rgb) if rgb else 255 for line in grid for _, rgb in line]
    body += [
        f'static const unsigned char {name}_BITS[{R * C}] = {{\n    {c_array(bits, C)}\n}};',
        f'static const unsigned char {name}_COLOR[{R * C}] = {{\n    {c_array(cols, C)}\n}};',
        f'static const quad_image {name} = {{ {C}, {R}, {name}_BITS, {name}_COLOR }};',
        '',
    ]
assert len(palette) < 255, f'paleta grande demais: {len(palette)}'

out = [
    '/* Gerado por tools/gen_logo.py a partir de assets/logo.png. Nao edite a mao. */',
    '#ifndef LOGO_DATA_H',
    '#define LOGO_DATA_H',
    '',
    '/* Cada celula = 1 caractere quadrant (2x2 subpixels). bits: TL=1 TR=2 BL=4 BR=8.',
    '   color = indice em LOGO_PALETTE (255 = transparente). */',
    'typedef struct { int cols, rows; const unsigned char *bits; const unsigned char *color; } quad_image;',
    '',
    f'static const unsigned char LOGO_PALETTE[{len(palette)}][3] = {{',
    *['    {%d,%d,%d},' % p for p in palette],
    '};',
    '',
    *body,
    '#endif',
    '',
]
OUT.write_text('\n'.join(out), encoding='utf-8')
print(f'{OUT}: paleta {len(palette)} cores,',
      ', '.join(f'{k} {len(v[0])}x{len(v)}' for k, v in IMAGES.items()))

if PREVIEW:
    cw, ch = 9, 18
    blocks = [IMAGES['ICON_L'], IMAGES['WORD_L']]
    W = max(len(b[0]) for b in blocks) + 4
    H = sum(len(b) + 1 for b in blocks) + 1
    pv = Image.new('RGB', (W * cw, H * ch), (12, 12, 12))
    d = ImageDraw.Draw(pv)
    y = 1
    for b in blocks:
        off = (W - len(b[0])) // 2
        for r, line in enumerate(b):
            for c, (bits, rgb) in enumerate(line):
                for k, (dy, dx) in enumerate(((0, 0), (0, 1), (1, 0), (1, 1))):
                    if bits >> k & 1:
                        X, Y = (off + c) * cw + dx * cw // 2, (y + r) * ch + dy * ch // 2
                        d.rectangle([X, Y, X + cw // 2 - 1, Y + ch // 2 - 1],
                                    fill=tuple(int(v) for v in rgb))
        y += len(b) + 1
    pv.save(PREVIEW)
