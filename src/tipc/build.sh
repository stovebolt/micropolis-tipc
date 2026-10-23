#!/bin/sh
# Build MPTIPC.EXE (Open Watcom 2, 16-bit DOS, large model, 8086) and the converted tile and sprite files.
#   sh tipc/build.sh        -> tbuild/MPTIPC.EXE, TILES8.BIN, TILES1.BIN, SPR8.BIN, SPR1.BIN
# Needs: Open Watcom 2 (set WATCOM to its folder), NASM, Python 3 with Pillow (for the art converters).
# All files -ox (a size-optimised build of the cold files saved nothing).
# (NOSPRITES, a lean build without sprite code for 256K colour, was dropped from the release: colour needs 512K.)
cd "$(dirname "$0")/.."
export WATCOM=${WATCOM:-/home/box/watcom} PATH=$WATCOM/binl64:$WATCOM/binl:$PATH INCLUDE=$WATCOM/h
OUT=tbuild; EXE=MPTIPC.EXE
mkdir -p $OUT && cd $OUT
rm -f *.o *.obj *.err
python3 ../art/tools/mktiles.py --out . >/dev/null || exit 1
python3 ../art/tools/mksprites.py --out . >/dev/null || exit 1
nasm -f obj -o tblit.obj ../tipc/tblit.asm || exit 1
C="-q -bt=dos -ml -0 -fp${FP:-i} -w1 -zp2 -DMSDOS -DTIPCUI -I../headers"
nasm -f obj -o snd.obj ../tipc/snd.asm || exit 1
wcl $C -ox -c ../s_scan.c ../s_sim.c ../s_power.c ../s_traf.c ../s_zone.c ../s_sprite.c ../glue.c ../s_alloc.c ../s_disast.c ../s_eval.c ../s_fileio.c ../s_gen.c ../s_init.c ../s_msg.c ../tool.c ../tcon.c ../tipc/strings.c ../tipc/mptipc.c 2>&1 | grep -i "error\|E1[0-9][0-9][0-9]"
wcl -q -bt=dos -ml -0 -fp${FP:-i} -k${STACK:-1280} -fe=$EXE -fm=${EXE%.EXE}.MAP *.o tblit.obj snd.obj 2>&1 | grep -i "error\|E1[0-9][0-9][0-9]"
rm -f *.obj *.o *.err
ls -l $EXE TILES8.BIN TILES1.BIN SPR8.BIN SPR1.BIN
grep "Memory size" ${EXE%.EXE}.MAP
