#!/bin/sh
# timeall.sh TAG : time linecity/radial/haight at 768k and 256k in parallel (1 game year each); results in /workspace/t/TAG*
for r in 768k 256k; do for c in linecity radial haight; do mkdir -p /workspace/t/$1$r$c; (nohup $(dirname $0)/run.sh /workspace/t/$1$r$c $r 1 $c >/dev/null 2>&1 &); done; done
