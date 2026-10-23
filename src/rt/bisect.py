import subprocess, sys, os
exe='./reloadtest'; A,B,ya,yb = sys.argv[1:5]
syms=[]
base=None
for l in subprocess.check_output(['nm','-n','-S','--defined-only',exe],text=True).splitlines():
    p=l.split()
    if len(p)==3 and p[2]=='__data_start': base=int(p[0],16)
    if len(p)==4 and p[2].lower() in 'bd': syms.append((int(p[0],16),int(p[1],16),p[3]))
def run(sel, tag):
    with open('/tmp/rt/ranges.txt','w') as f:
        for a,s,n in sel: f.write('%d %d\n'%(a-base,s))
    env=dict(os.environ, RANGES='/tmp/rt/ranges.txt')
    o=subprocess.run([exe,'warm','cities/%s.cty'%A,'cities/%s.cty'%B,ya,yb,'/tmp/rt/%s.dump'%tag],capture_output=True,text=True,env=env).stdout
    return o
cold=subprocess.run([exe,'cold','-','cities/%s.cty'%B,'0',yb,'/tmp/rt/cold.dump'],capture_output=True,text=True).stdout
ptr=lambda n: n.endswith('Ptr') or n.endswith('Base') or n in ('ResHis','ComHis','IndHis','CrimeHis','PollutionHis','MoneyHis','MiscHis','Map','MapData','PowerMapData','snap')
cand=[s for s in syms if s[2] not in ('snap','tmark','simdbg') and s[1]>0 and not ptr(s[2])]
print(len(cand),'candidate symbols')
print('baseline warm ok:', run([], 'w0')==cold)
print('restore all ok:', run(cand,'w1')==cold)
cur=list(cand)
# greedy removal: drop every symbol whose removal keeps equality
i=0
while i<len(cur):
    t=cur[:i]+cur[i+1:]
    if run(t,'w2')==cold: cur=t
    else: i+=1
print('NEEDED:', [n for a,s,n in cur])
