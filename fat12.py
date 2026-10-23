"""Minimal pure-Python FAT12 floppy image reader/writer (360K/720K)."""
import struct

GEOM = {  # size: (bytes/sec, sec/clus, reserved, fats, rootents, total, media, sec/fat, sec/trk, heads)
    360: (512, 2, 1, 2, 112, 720, 0xFD, 2, 9, 2),
    720: (512, 4, 1, 2, 144, 1440, 0xF9, 2, 9, 2),   # TI 720K format
}

class Image:
    def __init__(self, data):
        self.d = bytearray(data)
        b = self.d
        (self.bps, self.spc, self.res, self.nfat, self.nroot, self.tot, self.media,
         self.spf) = struct.unpack_from('<HBHBHHBH', b, 11)
        self.fat0 = self.res * self.bps
        self.root0 = (self.res + self.nfat * self.spf) * self.bps
        self.data0 = self.root0 + self.nroot * 32
        self.ncl = (self.tot * self.bps - self.data0) // (self.spc * self.bps) + 2
        self.cs = self.spc * self.bps

    @classmethod
    def blank(cls, kb, boot_sector):
        g = GEOM[kb]
        d = bytearray(g[5] * 512)
        d[:512] = boot_sector[:512]
        struct.pack_into('<HBHBHHBHHH', d, 11, *g)
        im = cls(d)
        for f in range(im.nfat):
            o = im.fat0 + f * im.spf * im.bps
            im.d[o:o + 3] = bytes([g[6], 0xFF, 0xFF])
        return im

    def getfat(self, n):
        o = self.fat0 + n * 3 // 2
        v = self.d[o] | self.d[o + 1] << 8
        return (v >> 4) if n & 1 else (v & 0xFFF)

    def setfat(self, n, val):
        for f in range(self.nfat):
            o = self.fat0 + f * self.spf * self.bps + n * 3 // 2
            v = self.d[o] | self.d[o + 1] << 8
            v = (v & 0xF) | (val << 4) if n & 1 else (v & 0xF000) | val
            self.d[o] = v & 0xFF; self.d[o + 1] = v >> 8

    def chain(self, c):
        out = []
        while 2 <= c < 0xFF8:
            out.append(c); c = self.getfat(c)
        return out

    def _entries(self, cl):
        """yield (offset, raw32) for dir at cluster cl (0=root)."""
        if cl == 0:
            offs = [self.root0 + i * 32 for i in range(self.nroot)]
        else:
            offs = [self.data0 + (c - 2) * self.cs + i * 32 for c in self.chain(cl) for i in range(self.cs // 32)]
        for o in offs:
            yield o, self.d[o:o + 32]

    def listdir(self, cl=0):
        res = []
        for o, e in self._entries(cl):
            if e[0] == 0: break
            if e[0] == 0xE5 or e[11] & 8 or e[0] == 0x2E: continue
            name = e[0:8].decode('latin1').rstrip()
            ext = e[8:11].decode('latin1').rstrip()
            res.append(dict(name=name + ('.' + ext if ext else ''), attr=e[11],
                            time=e[22:26], cl=struct.unpack_from('<H', e, 26)[0],
                            size=struct.unpack_from('<I', e, 28)[0]))
        return res

    def read(self, ent):
        data = b''.join(bytes(self.d[self.data0 + (c - 2) * self.cs:self.data0 + (c - 1) * self.cs])
                        for c in self.chain(ent['cl']))
        return data[:ent['size']]

    def walk(self, cl=0, path=''):
        for e in self.listdir(cl):
            p = path + e['name']
            if e['attr'] & 0x10:
                yield p + '\\', e
                yield from self.walk(e['cl'], p + '\\')
            else:
                yield p, e

    def files(self):
        return {p: self.read(e) for p, e in self.walk() if not p.endswith('\\')}

    # ---- writing ----
    def _alloc(self, n):
        out, c = [], 2
        while len(out) < n:
            if c >= self.ncl: raise IOError('disk full')
            if self.getfat(c) == 0: out.append(c)
            c += 1
        for a, b in zip(out, out[1:] + [0xFFF]): self.setfat(a, b)
        return out

    def free_bytes(self):
        return sum(1 for c in range(2, self.ncl) if self.getfat(c) == 0) * self.cs

    def _slot(self, dircl):
        for o, e in self._entries(dircl):
            if e[0] in (0, 0xE5): return o
        raise IOError('directory full')

    @staticmethod
    def _n83(name):
        n, _, x = name.upper().partition('.')
        return (n.ljust(8)[:8] + x.ljust(3)[:3]).encode('latin1')

    def add(self, name, data, dircl=0, attr=0x20, time=b'\x00\x00\x21\x00'):
        n = max(1, -(-len(data) // self.cs)) if data else 0
        cls = self._alloc(n) if n else []
        for i, c in enumerate(cls):
            chunk = data[i * self.cs:(i + 1) * self.cs]
            o = self.data0 + (c - 2) * self.cs
            self.d[o:o + len(chunk)] = chunk
        o = self._slot(dircl)
        e = bytearray(32); e[0:11] = self._n83(name); e[11] = attr; e[22:26] = time
        struct.pack_into('<HI', e, 26, cls[0] if cls else 0, len(data))
        self.d[o:o + 32] = e
        return cls[0] if cls else 0

    def mkdir(self, name, dircl=0, time=b'\x00\x00\x21\x00'):
        c = self._alloc(1)[0]
        o = self.data0 + (c - 2) * self.cs
        self.d[o:o + self.cs] = bytes(self.cs)
        for i, nm in enumerate((b'.          ', b'..         ')):
            e = bytearray(32); e[0:11] = nm; e[11] = 0x10; e[22:26] = time
            struct.pack_into('<H', e, 26, c if i == 0 else dircl)
            self.d[o + i * 32:o + i * 32 + 32] = e
        so = self._slot(dircl)
        e = bytearray(32); e[0:11] = self._n83(name); e[11] = 0x10; e[22:26] = time
        struct.pack_into('<H', e, 26, c)
        self.d[so:so + 32] = e
        return c
