#!/usr/bin/env python3
"""build_disks.py: floppy disk images for Micropolis for TIPC.
Code by Shurr Nuff (AI assistant), produced by Will (stovebolt), Flying Horse Labs.  GPL v3, see COPYING.

  python3 build_disks.py                  program and cities disks only (no DOS on them; this is what disks/ holds)
  python3 build_disks.py --dos TIDOS.img  also a bootable BOOT360.img made from YOUR TI MS-DOS 2.11 boot disk
  python3 build_disks.py --dos TIDOS.img --imd   also .imd copies with 2:1 sector interleave (see README.TXT)

Output (default folder disks/):
  MICRO360.img   360K  program disk: MPTIPC.EXE, tiles, sprites, COLOR.BAT, MONO.BAT, 2 sample cities
  MICRO720.img   720K  program disk: the same plus 12 sample cities
  CITIES720.img  720K  all 24 sample cities
  BOOT360.img    360K  TI MS-DOS 2.11 + AUTOEXEC.BAT (only with --dos; TI's DOS is not ours to give out)
Nothing from TI is in this package."""
import sys, os, argparse
here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
import fat12

TIME = b'\x00\x60\x57\x5d'   # 12:00, 2026-10-23 in DOS time/date words
SMALL = ['LINECITY', 'RADIAL']
MIDDLE = ['ABOUT', 'LINECITY', 'RADIAL', 'KYOTO', 'HAIGHT', 'KOBE', 'BRUCE', 'FREDS', 'HAPPISLE', 'SENRI', 'SOUTHPAC', 'WETCITY']

def stub_boot(kb):
    """Boot sector of a data disk: a valid BPB, no boot code (these disks are not bootable; BOOT360 is)."""
    b = bytearray(512)
    b[0:3] = bytes([0xEB, 0x3C, 0x90]); b[3:11] = b'MICROPLS'
    b[510:512] = bytes([0x55, 0xAA])
    return bytes(b)

def find(*cands):
    for c in cands:
        if os.path.isdir(c): return c
    sys.exit('cannot find the folder ' + ' or '.join(cands))

BAT_COLOR = 'MPTIPC %1 %2\r\n'
BAT_MONO = 'MPTIPC M %1 %2\r\n'
AUTOEXEC = ('B:\r\n'
            'ECHO OFF\r\n'
            'ECHO MICROPOLIS FOR TIPC - Flying Horse Labs\r\n'
            'ECHO Type COLOR (512K or more) or MONO (256K or more), then Enter.\r\n'
            'ECHO With a sample city:  COLOR LINECITY.CTY   or   MONO LINECITY.CTY\r\n'
            'ECHO In the game: H shows the keys, S saves (for example A:MYCITY.CTY).\r\n')

def program_disk(kb, bindir, cdir, cities, label):
    im = fat12.Image.blank(kb, stub_boot(kb))
    for n in ('MPTIPC.EXE', 'TILES8.BIN', 'TILES1.BIN', 'SPR8.BIN', 'SPR1.BIN'):
        im.add(n, open(os.path.join(bindir, n), 'rb').read(), time=TIME)
    im.add('COLOR.BAT', BAT_COLOR.encode(), time=TIME)
    im.add('MONO.BAT', BAT_MONO.encode(), time=TIME)
    for c in cities:
        im.add(c + '.CTY', open(os.path.join(cdir, c.lower() + '.cty'), 'rb').read(), time=TIME)
    return im

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--dos', metavar='IMG', help='your bootable TI MS-DOS 2.11 disk image (360K); makes BOOT360.img')
    ap.add_argument('--out', default=os.path.join(here, 'disks'), help='output folder (default disks/)')
    ap.add_argument('--bin', default=None, help='folder with MPTIPC.EXE, TILES8.BIN, TILES1.BIN, SPR8.BIN, SPR1.BIN (default bin/)')
    ap.add_argument('--cities', default=None, help='folder with the .cty files (default src/cities/ or cities/)')
    ap.add_argument('--imd', action='store_true', help='also write .imd copies with 2:1 interleave')
    a = ap.parse_args()
    bindir = a.bin or find(os.path.join(here, 'bin'))
    cdir = a.cities or find(os.path.join(here, 'src', 'cities'), os.path.join(here, 'cities'))
    os.makedirs(a.out, exist_ok=True)
    out = {}
    out['MICRO360.img'] = (program_disk(360, bindir, cdir, SMALL, 'MICRO360'), 360)
    out['MICRO720.img'] = (program_disk(720, bindir, cdir, MIDDLE, 'MICRO720'), 720)
    allc = sorted(f[:-4].upper() for f in os.listdir(cdir) if f.lower().endswith('.cty'))
    ci = fat12.Image.blank(720, stub_boot(720))
    for c in allc: ci.add(c + '.CTY', open(os.path.join(cdir, c.lower() + '.cty'), 'rb').read(), time=TIME)
    out['CITIES720.img'] = (ci, 720)
    if a.dos:
        src = fat12.Image(open(a.dos, 'rb').read())
        fs = src.files()
        for n in ('IO.SYS', 'MSDOS.SYS', 'COMMAND.COM'):
            if n not in fs: sys.exit('%s not found on %s: it must be a bootable TI MS-DOS 2.11 disk' % (n, a.dos))
        b = fat12.Image.blank(360, src.d[:512])       # TI's boot sector comes from your disk
        b.add('IO.SYS', fs['IO.SYS'], attr=0x27, time=TIME)     # must be first: the boot code loads it
        b.add('MSDOS.SYS', fs['MSDOS.SYS'], attr=0x27, time=TIME)
        b.add('COMMAND.COM', fs['COMMAND.COM'], time=TIME)
        b.add('AUTOEXEC.BAT', AUTOEXEC.encode(), time=TIME)
        out['BOOT360.img'] = (b, 360)
    for name, (im, kb) in out.items():
        open(os.path.join(a.out, name), 'wb').write(im.d)
        print('%-14s %3dK  %7d bytes free' % (name, kb, im.free_bytes()))
        if a.imd:
            import subprocess
            subprocess.check_call([sys.executable, os.path.join(here, 'img2imd.py'), os.path.join(a.out, name),
                                   os.path.join(a.out, name[:-4] + '.imd'), '2', '40' if kb == 360 else '80'])

main()
