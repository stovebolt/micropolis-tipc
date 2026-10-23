# Build MAME test disks for MPTIPC: A = 360K TI MS-DOS 2.11 boot disk with AUTOEXEC.BAT, B = 720K with the program.
# usage: mkdisks.py OUTDIR CITY "MPTIPC args after the city" [more bat lines...]
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import fat12
out, city, args = sys.argv[1], sys.argv[2], sys.argv[3]
src = fat12.Image(open('/workspace/tipc/gfx/scroll/scroll.img', 'rb').read())
f = src.files()
a = fat12.Image.blank(360, src.d[:512])
for n in ('IO.SYS', 'MSDOS.SYS'): a.add(n, f[n], attr=0x27)
for n in ('COMMAND.COM', 'CHKDSK.COM'): a.add(n, f[n])
bat = 'CHKDSK A: > B:CHK.TXT\r\nB:\r\n%s %s%s\r\n' % ('MPTIPC', '' if city == '-' else 'CITY.CTY ', args)
a.add('AUTOEXEC.BAT', bat.encode())
b = fat12.Image.blank(720, src.d[:512])
here = os.path.dirname(os.path.abspath(__file__))
bd = here + '/../tbuild/'; exe = 'MPTIPC.EXE'
b.add(exe, open(bd + exe, 'rb').read())
for t in ('TILES8.BIN', 'TILES1.BIN') + ('SPR8.BIN', 'SPR1.BIN'): b.add(t, open(bd + t, 'rb').read())
cpath = city if city.lower().endswith('.cty') else here + '/../cities/%s.cty' % city
if city != '-': b.add('CITY.CTY', open(cpath, 'rb').read())
if os.environ.get('CFGFILE'):   # put a prepared MP.CFG on B: (settings persistence test)
    b.add('MP.CFG', open(os.environ['CFGFILE'], 'rb').read())
if os.environ.get('FILLA'):   # leave only FILLA bytes free on A: (disk-full test)
    a.add('FILL.DAT', bytes(max(0, a.free_bytes() - int(os.environ['FILLA']))))
os.makedirs(out, exist_ok=True)
open(out + '/A.img', 'wb').write(a.d); open(out + '/B.img', 'wb').write(b.d)
