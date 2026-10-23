#!/bin/sh
# run.sh OUTDIR RAM YEARS CITY : boot TI DOS 2.11 in MAME tipc, run SIMTEST, collect timing/profile/output
O=$1; python3 /workspace/tipc/ports/micropolis/mame/mkdisks.py $O $3 $4 || exit 1
cd $O && OUT=$O/t.log PROF=$O/prof.txt NRUNS=1 timeout 3000 /workspace/mame/tipc tipc -rompath /workspace/tipc/engine/build/rp -fdc:1 525qd -flop1 A.img -flop2 B.img -ram $2 -video none -sound none -nothrottle -skip_gameinfo -autoboot_script /workspace/tipc/ports/micropolis/mame/time.lua -seconds_to_run 90000 > run.log 2>&1
python3 -c "
import sys; sys.path.insert(0,'/workspace/tipc/ports/goldbox/builder'); import fat12
f=fat12.Image(open('$O/B.img','rb').read()).files()
open('$O/OUT.TXT','wb').write(f.get('OUT0.TXT',b'')); open('$O/CHK.TXT','wb').write(f.get('CHK.TXT',b''))"
