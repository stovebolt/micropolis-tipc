"""Minimal XPM reader (the Micropolis tile sets): returns (w, h, rows of palette indexes, palette list of (r,g,b))."""
import re
def read_xpm(path):
    lines=[l.strip() for l in open(path) if l.strip().startswith('"')]
    w,h,nc,cpp=[int(v) for v in lines[0].strip('",').split()]
    pal={}; plist=[]
    for l in lines[1:1+nc]:
        s=l.strip().rstrip(',')[1:-1]
        key=s[:cpp]; m=re.search(r'c\s+#([0-9A-Fa-f]+)',s)
        h6=m.group(1); n=len(h6)//3
        rgb=tuple(int(h6[i*n:i*n+2],16) for i in range(3))
        pal[key]=len(plist); plist.append(rgb)
    rows=[]
    for l in lines[1+nc:1+nc+h]:
        s=l.strip().rstrip(',')[1:-1]
        rows.append([pal[s[i*cpp:(i+1)*cpp]] for i in range(w)])
    return w,h,rows,plist
