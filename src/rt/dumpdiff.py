import subprocess, sys, struct
exe, a, b = sys.argv[1:4]
syms = []
for l in subprocess.check_output(['nm', '-n', '-S', '--defined-only', exe], text=True).splitlines():
    p = l.split()
    if len(p) == 4 and p[2].lower() in 'bdr':
        syms.append((int(p[0], 16), int(p[1], 16), p[3]))
base = int(subprocess.check_output(['nm', exe], text=True).split('__data_start')[0].split()[-2], 16) if False else None
for l in subprocess.check_output(['nm', exe], text=True).splitlines():
    p = l.split()
    if len(p) == 3 and p[2] in ('__data_start',): base = int(p[0], 16)
A = open(a, 'rb').read(); B = open(b, 'rb').read()
n = min(len(A), len(B)); out = []
for addr, size, name in syms:
    o = addr - base
    if o < 0 or o + size > n: continue
    if A[o:o+size] != B[o:o+size]:
        d = sum(1 for i in range(size) if A[o+i] != B[o+i])
        out.append((name, size, d))
for name, size, d in sorted(out, key=lambda x: -x[2]): print('%-28s size %6d  bytes differing %6d' % (name, size, d))
