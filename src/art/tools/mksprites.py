#!/usr/bin/env python3
"""mksprites.py: convert the Micropolis GPL sprite art (images/obj<type>-<frame>.xpm) to TIPC sprites.

Code by Shurr Nuff (AI assistant), produced by Will (stovebolt), Flying Horse Labs. GPL v3.
Input : src/sprites/obj{1..7}-N.xpm  (1 train 5 frames 32x32, 2 helicopter 8 frames 32x32, 3 airplane 11 frames 48x48,
        4 ship 8 frames 48x48, 5 monster 16 frames 48x48, 6 tornado 3 frames 48x48, 7 explosion 6 frames 48x48;
        the unused bus, obj8, is left out).  A sprite pixel is 'None' (transparent) or one of a few colours.
Output: SPR8.BIN (8 colours, three planes + mask) and SPR1.BIN (mono, one plane + mask), and previews.
Scaling: the same as the tiles: 16 map pixels -> 16x9 TIPC pixels, so the width stays and the height becomes 9/16
        (32x32 -> 32x18, 48x48 -> 48x27, area-averaged in linear light with the mask as alpha); the colour is then
        reduced exactly as for the tiles (pure colours and 50% checkers; mono = Bayer dither of the luminance).
        A pixel is opaque when at least half of its source area is opaque.

SPRx.BIN layout (little endian):
  char magic[4] 'MPS8'|'MPS1';  u16 planes (3|1)
  7 x { u16 nframes, u16 ww, u16 rows, u16 offset, u16 bytes }   for sprite types 1..7 (offset from the file start)
  per type, at its offset: nframes frames, each = mask array, then one array per plane;
  an array is rows*ww words, row-major, pixel 0 (leftmost) = bit 15 of the first word of the row (like the tile rows).
"""
import sys, os, re, struct, glob, argparse
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
import mktiles as mt

NFR = {1: 5, 2: 8, 3: 11, 4: 8, 5: 16, 6: 3, 7: 6}
NAME = {1: 'train', 2: 'helicopter', 3: 'airplane', 4: 'ship', 5: 'monster', 6: 'tornado', 7: 'explosion'}

def q(l): return l[l.index('"') + 1:l.rindex('"')]

def read_sprite(path):
    L = [l for l in open(path) if l.startswith('"')]
    w, h, nc, cpp = map(int, q(L[0]).split())
    pal = {}
    for l in L[1:1 + nc]:
        s = q(l); k = s[:cpp]
        m = re.search(r'c\s+(#[0-9A-Fa-f]+|None)', s)
        if m.group(1) == 'None': pal[k] = None
        else:
            h6 = m.group(1)[1:]; n = len(h6) // 3
            pal[k] = tuple(int(h6[i * n:i * n + 2], 16) for i in range(3))
    rgb = np.zeros((h, w, 3)); a = np.zeros((h, w))
    for y, l in enumerate(L[1 + nc:1 + nc + h]):
        s = q(l)
        for x in range(w):
            c = pal[s[x * cpp:(x + 1) * cpp]]
            if c is not None: rgb[y, x] = c; a[y, x] = 1.0
    return w, h, rgb, a

def scale_v(rgb, a, th):
    """area-average rows to th rows in linear light, alpha-weighted; returns lin rgb (th,w,3) and coverage (th,w)"""
    h, w, _ = rgb.shape
    lin = mt.lin(rgb) * a[..., None]
    o = np.zeros((th, w, 3)); oa = np.zeros((th, w))
    for r in range(th):
        y0, y1 = r * h / th, (r + 1) * h / th
        for y in range(int(np.floor(y0)), int(np.ceil(y1))):
            wgt = min(y + 1, y1) - max(y, y0)
            if wgt > 1e-9: o[r] += lin[y] * wgt; oa[r] += a[y] * wgt
    oa /= (h / th)
    with np.errstate(invalid='ignore', divide='ignore'):
        col = np.where(oa[..., None] > 1e-9, o / (h / th) / np.maximum(oa[..., None], 1e-9), 0)
    return col, oa

def pack(bits):
    """bits (rows, w) 0/1 -> (rows, ww) uint16, pixel 0 = bit 15"""
    rows, w = bits.shape; ww = (w + 15) // 16
    out = np.zeros((rows, ww), dtype=np.uint32)
    for x in range(w): out[:, x // 16] |= bits[:, x].astype(np.uint32) << (15 - (x % 16))
    return out.astype(np.uint16)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default=os.path.join(os.path.dirname(__file__), '..', 'src', 'sprites'))
    ap.add_argument('--out', default='.')
    ap.add_argument('--mono-gain', type=float, default=1.0)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    data = {8: {}, 1: {}}; prev = {}
    for t in range(1, 8):
        frames8 = []; frames1 = []; pv8 = []; pv1 = []
        for f in range(NFR[t]):
            w, h, rgb, al = read_sprite(os.path.join(a.src, 'obj%d-%d.xpm' % (t, f)))
            th = h * 9 // 16
            col, cov = scale_v(rgb, al, th)
            mask = (cov >= 0.5)
            px8 = mt.quant8(col)
            # mono: lightness, a little contrast; the outline pixels of the art are black, so keep them
            px1 = mt.quant1(np.clip((col - .5) * 1.2 + .5, 0, 1), a.mono_gain)
            px8 = np.where(mask, px8, 0); px1 = np.where(mask, px1, 0)
            m = pack(mask.astype(np.int32))
            p8 = [pack(((px8 >> p) & 1)) for p in range(3)]
            p1 = [pack(px1)]
            frames8.append([m] + p8); frames1.append([m] + p1)
            pv8.append((px8, mask)); pv1.append((px1, mask))
        data[8][t] = (frames8, th, w); data[1][t] = (frames1, th, w); prev[t] = (pv8, pv1)
    for planes, magic, fn in ((3, b'MPS8', 'SPR8.BIN'), (1, b'MPS1', 'SPR1.BIN')):
        hdr = 4 + 2 + 7 * 10; off = hdr; table = b''; body = b''
        for t in range(1, 8):
            frames, th, w = data[8 if planes == 3 else 1][t]
            ww = (w + 15) // 16
            blk = b''.join(arr.astype('<u2').tobytes() for fr in frames for arr in fr)
            table += struct.pack('<HHHHH', len(frames), ww, th, off, len(blk)); body += blk; off += len(blk)
        open(os.path.join(a.out, fn), 'wb').write(magic + struct.pack('<H', planes) + table + body)
        print(fn, off, 'bytes;', ' '.join('%s %d' % (NAME[t], struct.unpack('<HHHHH', table[(t - 1) * 10:t * 10])[4]) for t in range(1, 8)))
    # previews: every frame on a mid-green / grey background, 3x
    for tag, pal, which in (('8', mt.TIPC, 0), ('1', [(0, 0, 0), (255, 255, 255)], 1)):
        cols = 16; sc = 3; cw, ch = 50, 29
        im = Image.new('RGB', (cols * cw * sc, 7 * ch * sc), (0, 100, 0) if tag == '8' else (96, 96, 96))
        for t in range(1, 8):
            for f, (px, mask) in enumerate(prev[t][which]):
                tile = Image.new('RGB', (px.shape[1], px.shape[0]), (0, 100, 0) if tag == '8' else (96, 96, 96))
                for y in range(px.shape[0]):
                    for x in range(px.shape[1]):
                        if mask[y, x]: tile.putpixel((x, y), pal[px[y, x]])
                tile = tile.resize((tile.width * sc, tile.height * sc), Image.NEAREST)
                im.paste(tile, (f * cw * sc, (t - 1) * ch * sc))
        im.save(os.path.join(a.out, 'sprites%s.png' % tag))

if __name__ == '__main__':
    main()
