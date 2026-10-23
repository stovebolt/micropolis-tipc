#!/usr/bin/env python3
"""mkplan.py: the keyboard-only test town for step 5.
Writes PLAN (for tools/botrun.c, Linux check) and KEYS (typed by shots.lua in MAME).
Town: coal plant at the west end, one main road, two rows of zones (R/C/I), a wire crossing the road so both rows get power."""
import sys
seed = int(sys.argv[1]) if len(sys.argv) > 1 else 1
out = sys.argv[2] if len(sys.argv) > 2 else '/tmp/town'
steps = []                      # ('tool', key) / ('at', x, y, key) / ('line', key, x0, y0, x1)
def build(key, x, y): steps.append(('build', key, x, y))
def hline(key, x0, y, x1): steps.append(('line', key, x0, y, x1))
build('C', 44, 5)               # coal power plant (4x4), $3000
hline('R', 47, 8, 75)           # main road
build('W', 48, 8)               # power line over the road: joins the two rows
xs = list(range(48, 75, 3))
k = {'R': '1', 'C': '2', 'I': '3'}
for x, z in zip(xs, 'RRRCRRRII'): build(k[z], x, 6)
for x, z in zip(xs, 'RRRRCRRII'): build(k[z], x, 10)
plan = ['n %d' % seed]
keys = []
for s in steps:
    if s[0] == 'build':
        _, key, x, y = s
        plan.append('t %s %d %d' % (key, x, y))
        keys.append('%sg%d %d\n\n' % (key.lower(), x, y))        # tool, go to, Enter = build
    else:
        _, key, x0, y, x1 = s
        for x in range(x0, x1 + 1): plan.append('t %s %d %d' % (key, x, y))
        keys.append('%sg%d %d\nd\n%sd' % (key.lower(), x0, y, '{RIGHT}' * (x1 - x0)))   # drag mode: Enter, then cursor right
plan += ['w 6', 'w 12', 'w 12', 'w 12', 'w 12']
open(out + '_plan.txt', 'w').write('\n'.join(plan) + '\n')
open(out + '_keys.txt', 'w').write(''.join(keys))
print(len(''.join(keys)), 'chars')
