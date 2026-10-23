import subprocess, os, sys, collections
exe='./reloadtest'
syms=[]; base=None
for l in subprocess.check_output(['nm','-n','-S','--defined-only',exe],text=True).splitlines():
    p=l.split()
    if len(p)==3 and p[2]=='__data_start': base=int(p[0],16)
    if len(p)==4 and p[2].lower() in 'bd': syms.append((int(p[0],16),int(p[1],16),p[3],p[2]))
cities=[l.strip() for l in open('/tmp/cl.txt')]
cnt=collections.Counter(); coldv={}
prev=cities[-1]
for c in cities:
    for dis in ('', '1'):
        env=dict(os.environ); env.pop('DISASTERS',None)
        if dis: env['DISASTERS']='1'
        subprocess.run([exe,'cold','-','cities/%s.cty'%c,'0','1','/tmp/rt/c.dump'],capture_output=True,env=env)
        subprocess.run([exe,'warm','cities/%s.cty'%prev,'cities/%s.cty'%c,'6','1','/tmp/rt/w.dump'],capture_output=True,env=env)
        A=open('/tmp/rt/c.dump.pre','rb').read(); B=open('/tmp/rt/w.dump.pre','rb').read()
        for a,s,n,t in syms:
            o=a-base
            if o+s<=len(A) and A[o:o+s]!=B[o:o+s]: cnt[n]+=1; coldv[n]=(s,t,A[o:o+s])
    prev=c
for n,k in sorted(cnt.items(), key=lambda x:-x[1]):
    s,t,v=coldv[n]
    if s<=8: print('%-18s %s size %d count %2d cold=%s'%(n,t,s,k,v.hex()))
print('arrays:',[ (n,coldv[n][0]) for n in cnt if coldv[n][0]>8])
