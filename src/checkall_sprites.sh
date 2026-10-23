#!/bin/sh
# Sprites-on determinism check: Linux headless with sprites vs saved baselines (ref30s = trains/ships/planes/copters,
# ref30sd = the same plus random disasters: monster, tornado, explosions, crashes), then DOS (emu2) vs Linux.
# usage: ./checkall_sprites.sh [YEARS] [linux]    (the baselines are 30-year logs)
Y=${1:-30}; EMU=${EMU2:-/home/box/emu2}
make -s simtest || exit 1
f=0
for m in S SD; do
  d=ref30$(echo $m | tr A-Z a-z); [ $m = S ] && d=ref30s
  for c in cities/*.cty; do n=$(basename $c .cty); ./simtest $c 30 1 0 - $m | cmp -s - $d/$n.txt || { echo "LINUX DIFF $m $n"; f=1; }; done
done
echo "linux sprites vs baseline (S, SD): $([ $f = 0 ] && echo all match || echo FAIL)"
[ "$2" = linux ] && exit $f
export WATCOM=/home/box/watcom PATH=/home/box/watcom/binl64:/home/box/watcom/binl:$PATH INCLUDE=/home/box/watcom/h
make -B -f Makefile.wat > wbuild.log 2>&1 || { grep -i error wbuild.log; echo BUILD FAILED; exit 1; }; rm -f *.obj
mkdir -p dosrun out; cp SIMTEST.EXE dosrun/
for m in S SD; do
  d=ref30$(echo $m | tr A-Z a-z); [ $m = S ] && d=ref30s
  pass=0; fail=0
  for c in cities/*.cty; do
    n=$(basename $c .cty); U=$(echo $n | tr a-z A-Z); cp $c dosrun/$U.CTY
    (cd dosrun && $EMU SIMTEST.EXE $U.CTY 30 1 0 - $m | tr -d "\r" | grep -v "^MEM") > out/$n.dos.$m.txt
    if cmp -s $d/$n.txt out/$n.dos.$m.txt; then pass=$((pass+1)); else fail=$((fail+1)); echo "DIFF $m $n"; fi
  done
  echo "DOS sprites $m: years=30 pass=$pass fail=$fail"
done
