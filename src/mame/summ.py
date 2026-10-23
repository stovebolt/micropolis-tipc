import glob,re,sys
for d in sorted(glob.glob('/workspace/t/%s*'%sys.argv[1])):
    t=open(d+'/t.log').read()
    ts=[(float(a),int(b)) for a,b in re.findall(r'^([\d.]+) CT (\d+)',t,re.M)]
    p2=float(re.findall(r'^([\d.]+) PHASE 2',t,re.M)[0]); n=ts[-1][1]-ts[0][1]; tot=p2-ts[0][0]
    print(d.split('/')[-1],'s/month %.2f s/yr %.1f'%(4*tot/n,48*tot/n))
