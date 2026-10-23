# Build MAME test disks: A = 360K TI MS-DOS 2.11 boot (from the box's TI DOS image), B = 720K with SIMTEST + cities.
# usage: mkdisks.py OUTDIR YEARS city1 city2 ...
import sys, os
sys.path.insert(0, '/workspace/tipc/ports/goldbox/builder')
import fat12
out, years, cities = sys.argv[1], sys.argv[2], sys.argv[3:]
src = fat12.Image(open('/workspace/tipc/gfx/scroll/scroll.img', 'rb').read())
f = src.files()
a = fat12.Image.blank(360, src.d[:512])
for n in ('IO.SYS', 'MSDOS.SYS'): a.add(n, f[n], attr=0x27)
for n in ('COMMAND.COM', 'CHKDSK.COM'): a.add(n, f[n])
bat = 'CHKDSK A: > B:CHK.TXT\r\nB:\r\n'
for i, c in enumerate(cities): bat += 'SIMTEST %s.CTY %s > OUT%d.TXT\r\n' % (c.upper(), years, i)
a.add('AUTOEXEC.BAT', bat.encode())
b = fat12.Image.blank(720, src.d[:512])
here = os.path.dirname(os.path.abspath(__file__))
b.add('SIMTEST.EXE', open(here + '/../SIMTEST.EXE', 'rb').read())
for c in cities: b.add(c.upper() + '.CTY', open(here + '/../cities/%s.cty' % c, 'rb').read())
os.makedirs(out, exist_ok=True)
open(out + '/A.img', 'wb').write(a.d); open(out + '/B.img', 'wb').write(b.d)
