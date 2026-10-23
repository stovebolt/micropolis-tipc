#!/bin/sh
# rt.sh A B yearsA yearsB : load A, play yearsA, in-game-load B, play yearsB; compare with a cold start on B
cd "$(dirname "$0")/.."
SRC="s_alloc.c s_disast.c s_eval.c s_fileio.c s_gen.c s_init.c s_msg.c s_power.c s_scan.c s_sim.c s_traf.c s_zone.c s_sprite.c glue.c reloadtest.c"
[ reloadtest -nt s_sim.c ] && [ reloadtest -nt s_alloc.c ] && [ reloadtest -nt glue.c ] || gcc -std=gnu89 -O1 -w -fwrapv -no-pie -Iheaders -o reloadtest $SRC -lm || exit 1
cd "$(dirname "$0")/.."
D=/tmp/rt; mkdir -p $D
./reloadtest cold - cities/$2.cty 0 $4 $D/cold.dump > $D/cold.txt
./reloadtest warm cities/$1.cty cities/$2.cty $3 $4 $D/warm.dump > $D/warm.txt
if cmp -s $D/cold.txt $D/warm.txt; then echo "same  $1 -> $2 ($3+$4 years)"; else echo "DIFF  $1 -> $2 ($3+$4 years)"; diff $D/cold.txt $D/warm.txt | head -4; fi
