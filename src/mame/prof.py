#!/usr/bin/env python3
# prof.py prof.txt SIMTEST.MAP : map CS:IP samples to symbols (load base 0x08d2)
import sys,re,collections,bisect
base=0x8d2
syms=[]
for l in open(sys.argv[2]):
    m=re.match(r'^([0-9a-f]{4}):([0-9a-f]{4})\+?\s+(\S+)',l)
    if m: syms.append((int(m.group(1),16)*16+int(m.group(2),16),m.group(3)))
syms.sort(); keys=[s[0] for s in syms]
c=collections.Counter(); tot=0
for l in open(sys.argv[1]):
    n,k=l.split(); n=int(n); cs,ip=k.split(':'); a=(int(cs,16)-base)*16+int(ip,16)
    i=bisect.bisect_right(keys,a)-1
    c[syms[i][1] if i>=0 else '?']+=n; tot+=n
for k,v in c.most_common(18): print('%5.1f%% %s'%(100*v/tot,k))
print(tot)
