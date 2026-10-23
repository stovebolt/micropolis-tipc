#!/bin/sh
# run_mame.sh OUTDIR RAM CITY "ARGS" QUIT_SECONDS   (env SNAPS, KEYS, CHARS passed to shots.lua)
# Boots TI MS-DOS 2.11 in MAME tipc (5 MHz 8088, 3-plane graphics) and runs MPTIPC. Snapshots land in OUTDIR/snap.
D=$(cd "$(dirname "$0")" && pwd); O=$1; RAM=$2; CITY=$3; ARGS=$4; Q=$5
python3 $D/mkdisks.py $O $CITY "$ARGS" || exit 1
mkdir -p $O/snap; cd $O
BIMG=B.img; AIMG=A.img
IL=${IL-2}   # B disk interleave 2 by default (IL= for none); IMD images cannot be written back, so program output goes to the raw A: disk
if [ -n "$IL" ]; then python3 $D/img2imd.py B.img B.imd $IL; BIMG=B.imd; fi
if [ -n "$ILA" ]; then python3 $D/img2imd.py A.img A.imd $ILA 40; AIMG=A.imd; fi
LOG=$O/shots.log QUIT=$Q timeout 3000 /workspace/mame/tipc tipc -rompath ${ROMPATH:-/workspace/t/rp} -fdc:1 525qd -flop1 $AIMG -flop2 $BIMG -ram $RAM \
  -video none -sound none ${WAV:+-wavwrite $WAV} -nothrottle -skip_gameinfo -autoboot_script $D/shots.lua -snapshot_directory $O/snap -seconds_to_run $((Q+60)) > run.log 2>&1
python3 -c "
import sys; sys.path.insert(0,'/workspace/tipc/ports/goldbox/builder'); import fat12
fb=fat12.Image(open('$O/B.img','rb').read()).files()
fa=fat12.Image(open('$O/A.img','rb').read()).files()
for n in ('OUT.TXT','CHK.TXT'): open('$O/'+n,'wb').write(fb.get(n) or fa.get(n,b''))
for d in (fa, fb):
  for n in d:
    if n.endswith('.CTY') and n != 'CITY.CTY': open('$O/'+n,'wb').write(d[n])"
