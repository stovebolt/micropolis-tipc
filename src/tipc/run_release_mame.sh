#!/bin/sh
# run_release_mame.sh PKGDIR OUTDIR RAM(256k|512k|...) SIZE(360|720) "TYPED TEXT" QUIT_SECONDS   (env SNAPS, START, STEP, DOS)
# Boot test of a release package in MAME tipc: BOOT360 (made from the TI MS-DOS image in $DOS) in A:, the 360K or 720K program
# disk in B: (both with 2:1 interleave), then types TEXT (newline = Enter, as shots.lua does).  Snapshots land in OUTDIR/snap.
P=$1; O=$2; RAM=$3; SZ=$4; TXT=$5; Q=$6
DOS=${DOS:-/workspace/tipc/gfx/scroll/scroll.img}
D=$(cd "$(dirname "$0")" && pwd)
rm -rf $O; mkdir -p $O/snap $O/disks; cd $P
python3 build_disks.py --dos $DOS --out $O/disks >/dev/null || exit 1
cp $O/disks/BOOT360.img $O/A.img; cp $O/disks/MICRO$SZ.img $O/B.img
python3 $P/img2imd.py $O/A.img $O/A.imd 2 40; BF=B.img; if [ $SZ = 720 ]; then python3 $P/img2imd.py $O/B.img $O/B.imd 2 80; BF=B.imd; fi   # a 360K disk in the 80-track B: drive only works as a raw image in MAME
printf "$TXT" > $O/typed.txt
cd $O
SCRIPT=$O/typed.txt START=${START:-40} STEP=${STEP:-0.4} LOG=$O/shots.log QUIT=$Q timeout 3000 /workspace/mame/tipc tipc -rompath ${ROMPATH:-/workspace/t/rp} -fdc:1 525qd -flop1 ${AF:-A.imd} -flop2 $BF -ram $RAM \
  -video none -sound none -nothrottle -skip_gameinfo -autoboot_script $D/shots.lua -snapshot_directory $O/snap -seconds_to_run $((Q+60)) > run.log 2>&1
python3 -c "
import sys; sys.path.insert(0,'$P'); import fat12
for n in ('A','B'):
  fs=fat12.Image(open('$O/'+n+'.img','rb').read()).files()
  for f in fs:
    if f.endswith('.CTY') or f.endswith('.TXT'): pass
"
