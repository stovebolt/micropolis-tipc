#!/bin/sh
# Linux build vs saved 30-year baseline (ref30/) for all 24 cities, then DOS (emu2) vs Linux.
make -s simtest || exit 1
f=0; for c in cities/*.cty; do n=$(basename $c .cty); ./simtest $c 30 | cmp -s - ref30/$n.txt || { echo "LINUX DIFF $n"; f=1; }; done
echo "linux vs baseline: $([ $f = 0 ] && echo all 24 match || echo FAIL)"
[ "$1" = linux ] && exit $f
export WATCOM=/home/box/watcom PATH=/home/box/watcom/binl64:/home/box/watcom/binl:$PATH INCLUDE=/home/box/watcom/h
make -B -f Makefile.wat > wbuild.log 2>&1 || { grep -i error wbuild.log; echo BUILD FAILED; exit 1; }; rm -f *.obj
./runtests.sh 30
