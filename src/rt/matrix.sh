cd "$(dirname "$0")/.."
n=0; bad=0; prev=$(tail -1 /tmp/cl.txt)
for c in $(cat /tmp/cl.txt); do for D in "" 1; do if [ -n "$D" ]; then export DISASTERS=1; else unset DISASTERS; fi
 r=$(rt/rt.sh $prev $c $1 $2 | head -1); n=$((n+1)); case "$r" in same*) ;; *) bad=$((bad+1)); echo "D=$D $r";; esac; done; prev=$c; done; unset DISASTERS; echo "pairs=$n bad=$bad (A years $1, B years $2)"
