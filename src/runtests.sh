#!/bin/sh
# Compare DOS (emu2) build output against Linux reference for every bundled city.
# usage: ./runtests.sh YEARS
Y=${1:-12}; EMU=${EMU2:-/home/box/emu2}; pass=0; fail=0
mkdir -p dosrun out
cp SIMTEST.EXE dosrun/
for f in cities/*.cty; do
  n=$(basename $f .cty); U=$(echo $n | tr a-z A-Z)
  cp $f dosrun/$U.CTY
  ./simtest $f $Y > out/$n.linux.txt
  (cd dosrun && $EMU SIMTEST.EXE $U.CTY $Y | tr -d "\r" | grep -v "^MEM") > out/$n.dos.txt
  if cmp -s out/$n.linux.txt out/$n.dos.txt; then pass=$((pass+1)); else fail=$((fail+1)); echo "DIFF $n"; fi
done
echo "years=$Y pass=$pass fail=$fail"
