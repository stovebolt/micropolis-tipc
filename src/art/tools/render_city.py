#!/usr/bin/env python3
"""render_city.py CITY.cty OUT.png [--mono] [--wx N --wy N]: draw the 45x32 tile window the TIPC front end shows,
from the converted tiles (px8.npy / px1.npy in ../build), as a 720x300 image (status row black)."""
import sys, struct, argparse, os
import numpy as np
from PIL import Image
TIPC = [(0,0,0),(0,0,255),(255,0,0),(255,0,255),(0,255,0),(0,255,255),(255,255,0),(255,255,255)]
ap = argparse.ArgumentParser(); ap.add_argument('city'); ap.add_argument('out')
ap.add_argument('--mono', action='store_true'); ap.add_argument('--wx', type=int, default=40); ap.add_argument('--wy', type=int, default=34)
ap.add_argument('--build', default=os.path.join(os.path.dirname(__file__), '..', 'build'))
a = ap.parse_args()
d = open(a.city, 'rb').read()
m = struct.unpack('>%dH' % 12000, d[3120:3120 + 24000])
px = np.load(os.path.join(a.build, 'px1.npy' if a.mono else 'px8.npy'))
pal = [(0,0,0),(255,255,255)] if a.mono else TIPC
th = px.shape[1]
im = Image.new('RGB', (720, 300), (0, 0, 0))
for ty in range(32):
    for tx in range(45):
        t = m[(a.wx + tx) * 100 + a.wy + ty] & 0x3FF
        tile = Image.new('RGB', (16, th))
        for y in range(th):
            for x in range(16): tile.putpixel((x, y), pal[px[t, y, x]])
        im.paste(tile, (tx * 16, ty * th))
im.save(a.out)
