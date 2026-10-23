#!/usr/bin/env python3
"""mktiles.py: convert the Micropolis GPL tile art to TIPC tiles (build-time converter).

Code by Shurr Nuff (AI assistant), produced by Will (stovebolt), Flying Horse Labs. GPL v3.
Input : src/tiles.xpm (960 tiles, 16x16, 14 colours), src/tilesbw.xpm (original 1-bit set),
        src/tilessm.xpm (960 tiles, 4x3).  All from Micropolis (GPL v3, Don Hopkins' release).
Output: TILES8.BIN  8-colour, 16x9 px per tile, three planes (pixel value = C*4+B*2+A, TIPC default palette)
        TILES1.BIN  mono, 16x9 px per tile, one plane
        TILESM.BIN  4x2 px overview tiles (8-colour nibbles), for the later overview map
        PNG previews (sheet and 4:3 views).
Aspect: a TIPC pixel is 1.8x taller than wide (720x300 on a 4:3 tube).  A square 16x16 tile
is therefore 16x8.9 pixels; 16x9 is within 1.3%.  (--th 8 gives 16x8, --th 10 gives 16x10.)

TILES file layout (little endian):
  char magic[4] 'MPT8' or 'MPT1';  u16 ntiles(960);  u16 nuniq;  u16 th;  u16 planes
  u16 off[ntiles]     byte offset (from the start of the plane-A data) of the tile's data, th words per tile
  plane A data: nuniq*th words, then plane B, then plane C (only plane A for MPT1)
  A row is one word: pixel 0 (leftmost) is bit 15.  This is exactly what a 16-bit store to the
  TIPC graphics RAM wants (bytes are swapped within each word on the board).
The table offsets are relative to the start of the file image in memory minus the header, i.e. the
loader keeps the header+table at the start of the segment and offsets already include it (see 'base').
"""
import sys, os, struct, argparse
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
from xpm import read_xpm

TIPC = [(0,0,0),(0,0,255),(255,0,0),(255,0,255),(0,255,0),(0,255,255),(255,255,0),(255,255,255)]
GAMMA = 2.2

def lin(c):  return (np.asarray(c, dtype=np.float64) / 255.0) ** GAMMA
def unlin(l): return np.clip(l, 0, 1) ** (1 / GAMMA) * 255.0

def lab(l):
    """linear sRGB-ish (gamma 2.2 approx) -> CIE Lab (D65)"""
    M = np.array([[0.4124,0.3576,0.1805],[0.2126,0.7152,0.0722],[0.0193,0.1192,0.9505]])
    xyz = l @ M.T / np.array([0.9505,1.0,1.089])
    f = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16/116)
    return np.stack([116*f[...,1]-16, 500*(f[...,0]-f[...,1]), 200*(f[...,1]-f[...,2])], -1)

def load_tiles(path, tw, th):
    w, h, rows, pal = read_xpm(path)
    a = np.array(rows, dtype=np.int32).reshape(h // th, th, tw)
    return a, pal

def tile_rgb(idx, pal):
    return np.array(pal, dtype=np.float64)[idx]       # (n, th, tw, 3) 0..255

def vresample(lt, th_out):
    """area-average a (n,16,16,3) linear tile stack down to th_out rows"""
    n, th_in, tw, _ = lt.shape
    out = np.zeros((n, th_out, tw, 3))
    for r in range(th_out):
        y0, y1 = r * th_in / th_out, (r + 1) * th_in / th_out
        acc = 0.0; wsum = 0.0
        for y in range(int(np.floor(y0)), int(np.ceil(y1))):
            wgt = min(y + 1, y1) - max(y, y0)
            if wgt > 1e-9: acc = acc + lt[:, y] * wgt; wsum += wgt
        out[:, r] = acc / wsum
    return out

# ---------------------------------------------------------------- 8 colour: pure + 50% checker pairs
def candidates():
    c = []   # (colour at parity0, colour at parity1, mixed linear rgb)
    L = [lin(x) for x in TIPC]
    for i in range(8): c.append((i, i, L[i]))
    for i in range(8):
        for j in range(i + 1, 8):
            c.append((i, j, (L[i] + L[j]) / 2))
    return c
CANDS = candidates()
CAND_LAB = lab(np.array([x[2] for x in CANDS]))
# a mild preference for pure colours (no texture) when the error is about equal
PENALTY = np.array([0.0 if a == b else 3.0 for a, b, _ in CANDS])

def quant8(lin_tile):
    """lin_tile (th,16,3) linear -> (th,16) pixel values using pure colours and 50% checkers"""
    th, tw, _ = lin_tile.shape
    tl = lab(lin_tile.reshape(-1, 3))
    d = ((tl[:, None, :] - CAND_LAB[None, :, :]) ** 2).sum(-1) + PENALTY[None, :] ** 2
    best = d.argmin(1).reshape(th, tw)
    out = np.zeros((th, tw), dtype=np.int32)
    for y in range(th):
        for x in range(tw):
            a, b, _ = CANDS[best[y, x]]
            out[y, x] = a if ((x + y) & 1) == 0 else b
    return out

# ---------------------------------------------------------------- mono: ordered dither of lightness
# Bayer-like 4x4 (x period 4 divides 16).  Rows of a 9-row tile repeat the 4-row matrix; the seam
# between vertically stacked tiles is the same phase jump the checker has (9 is odd).
BAYER4 = np.array([[0,8,2,10],[12,4,14,6],[3,11,1,9],[15,7,13,5]]) / 16.0 + 1/32

def quant1(lin_tile, gain=1.0, bias=0.0, matrix='bayer'):
    th, tw, _ = lin_tile.shape
    Y = lin_tile @ np.array([0.2126, 0.7152, 0.0722])          # linear luminance 0..1
    Y = np.clip(Y * gain + bias, 0, 1)
    # dither in "display" space: 50% black/white checker = linear 0.5.  Threshold on linear value.
    out = np.zeros((th, tw), dtype=np.int32)
    for y in range(th):
        for x in range(tw):
            t = BAYER4[y % 4, x % 4]
            out[y, x] = 1 if Y[y, x] > t else 0
    return out

def pack_words(px, planes):
    """px (n,th,16) values -> per-plane (n,th) uint16, pixel 0 = bit 15"""
    n, th, tw = px.shape
    out = []
    for p in range(planes):
        bits = (px >> p) & 1
        w = np.zeros((n, th), dtype=np.uint32)
        for x in range(tw):
            w |= bits[:, :, x].astype(np.uint32) << (15 - x)
        out.append(w.astype(np.uint16))
    return out

def write_tiles(path, magic, words, th, planes):
    n = words[0].shape[0]
    keys = {}; order = []; idx = []
    for t in range(n):
        k = b''.join(w[t].tobytes() for w in words)
        if k not in keys: keys[k] = len(order); order.append(t)
        idx.append(keys[k])
    nu = len(order)
    hdr = 4 + 2 * 4 + 2 * n
    with open(path, 'wb') as f:
        f.write(magic + struct.pack('<HHHH', n, nu, th, planes))
        f.write(b''.join(struct.pack('<H', hdr + i * th * 2) for i in idx))
        for p in range(planes):
            for t in order: f.write(words[p][t].tobytes())
    return nu, hdr + nu * th * 2 * planes

def sheet(px, pal_rgb, cols=32, sx=1, sy=1, gap=1):
    n, th, tw = px.shape
    rows = (n + cols - 1) // cols
    im = Image.new('RGB', (cols * (tw + gap), rows * (th + gap)), (60, 0, 60))
    for t in range(n):
        tile = Image.new('RGB', (tw, th))
        for y in range(th):
            for x in range(tw): tile.putpixel((x, y), pal_rgb[px[t, y, x]])
        im.paste(tile, ((t % cols) * (tw + gap), (t // cols) * (th + gap)))
    return im

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default=os.path.join(os.path.dirname(__file__), '..', 'src'))
    ap.add_argument('--out', default='.')
    ap.add_argument('--th', type=int, default=9)
    ap.add_argument('--mono-source', choices=['color', 'bw', 'tone'], default='bw')
    ap.add_argument('--sharp', type=float, default=1.5)
    ap.add_argument('--gain', type=float, default=1.0)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    idx, pal = load_tiles(os.path.join(a.src, 'tiles.xpm'), 16, 16)
    n = idx.shape[0]
    lt = lin(tile_rgb(idx, pal) )
    lt = vresample(lt, a.th)
    px8 = np.stack([quant8(lt[t]) for t in range(n)])
    if a.mono_source == 'tone':
        # per-colour display brightness (linear, 0..1), tuned by eye so that dirt, grass, trees, water,
        # roads and buildings stay distinguishable in one bit
        TONE = {(204,127,102): .86, (63,63,63): .08, (0,0,0): 0.0, (102,102,230): .50, (0,0,230): .30,
                (153,127,76): .44, (127,127,127): .45, (0,230,0): .62, (0,127,0): .34, (255,255,255): 1.0,
                (255,0,0): .30, (255,255,0): .75, (255,127,0): .55, (191,191,191): .80}
        tp = [(TONE[tuple(c)],) * 3 for c in pal]
        tl = vresample(np.array(tp, dtype=np.float64)[idx], a.th)
        px1 = np.stack([quant1(np.clip((tl[t] - .5) * a.sharp + .5, 0, 1)) for t in range(n)])
    elif a.mono_source == 'bw':
        bidx, bpal = load_tiles(os.path.join(a.src, 'tilesbw.xpm'), 16, 16)
        blt = vresample(lin(tile_rgb(bidx, bpal)), a.th)
        px1 = np.stack([quant1(np.clip((blt[t] - .5) * a.sharp + .5, 0, 1)) for t in range(n)])
    else:
        px1 = np.stack([quant1(lt[t], a.gain) for t in range(n)])
    w8 = pack_words(px8, 3); w1 = pack_words(px1, 1)
    nu8, sz8 = write_tiles(os.path.join(a.out, 'TILES8.BIN'), b'MPT8', w8, a.th, 3)
    nu1, sz1 = write_tiles(os.path.join(a.out, 'TILES1.BIN'), b'MPT1', w1, a.th, 1)
    print('TILES8.BIN unique %d size %d; TILES1.BIN unique %d size %d' % (nu8, sz8, nu1, sz1))
    sheet(px8, TIPC).save(os.path.join(a.out, 'sheet8.png'))
    sheet(px1, [(0,0,0),(255,255,255)]).save(os.path.join(a.out, 'sheet1.png'))
    np.save(os.path.join(a.out, 'px8.npy'), px8); np.save(os.path.join(a.out, 'px1.npy'), px1)

if __name__ == '__main__':
    main()
