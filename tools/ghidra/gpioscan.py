import re,struct,sys,bisect
D=sys.argv[1]
d=open(D+'/fw.bin','rb').read(); B=0x08002800
lines=open(D+'/fw.dis').read().splitlines()
funcs=[]
for l in open(D+'/dec_named.c'):
    if l.startswith('// ==== '):
        p=l.split(); funcs.append((int(p[4],16),p[2]))
funcs.sort(); fa=[f[0] for f in funcs]
def fn(a):
    i=bisect.bisect_right(fa,a)-1
    return funcs[i][1] if i>=0 else '?'
def lit(a):
    o=a-B
    return struct.unpack('<I',d[o:o+4])[0] if 0<=o<=len(d)-4 else None
gp={0x40020000:'PA',0x40020400:'PB',0x40020800:'PC',0x40020C00:'PD',0x40021400:'PF'}
regn={0x18:'SET',0x28:'CLR',0x2c:'TOG',0x14:'ODT',0x10:'IDT'}
def imm(s):
    s=s.strip().lstrip('#')
    return int(s,0)
R={}
for l in lines:
    m=re.match(r'\s*([0-9a-f]+):\s+(?:[0-9a-f]{4} ?){1,2}\s+(\S+)\s*(.*)',l)
    if not m: continue
    a=int(m.group(1),16); op=m.group(2); args=m.group(3).split('@')[0].strip()
    if op.startswith(('b','pop','push','cb')) and not op.startswith('bic'):
        if op in ('bl','blx') or op.startswith(('b.','b','pop','cb')): R={} if op not in('bl','blx') else {k:v for k,v in R.items() if k in('r4','r5','r6','r7','r8')}
        continue
    mm=re.match(r'(r\d+), \[pc, #\d+\]',args)
    if op in('ldr','ldr.w') and mm:
        t=re.search(r'\((0x[0-9a-f]+)\)',m.group(3)); 
        if t: R[mm.group(1)]=lit(int(t.group(1),16)); continue
    mm=re.match(r'(r\d+), #(-?\d+)$',args)
    if op in('movs','mov.w','movw','mov') and mm: R[mm.group(1)]=int(mm.group(2)); continue
    mm=re.match(r'(r\d+), (r\d+)$',args)
    if op in('mov',) and mm: R[mm.group(1)]=R.get(mm.group(2)); continue
    mm=re.match(r'(r\d+), #(\d+)$',args)
    if op in('adds','subs','add.w','sub.w','add','sub') and mm and R.get(mm.group(1)) is not None:
        R[mm.group(1)]=R[mm.group(1)]+(int(mm.group(2)) if op.startswith('add') else -int(mm.group(2))); continue
    mm=re.match(r'(r\d+), (r\d+), #(\d+)$',args)
    if op in('lsls','asrs','lsrs') and mm and R.get(mm.group(2)) is not None:
        v=R[mm.group(2)];s=int(mm.group(3)); R[mm.group(1)]=(v<<s)&0xffffffff if op=='lsls' else v>>s; continue
    mm=re.match(r'(r\d+), \[(r\d+)(?:, #(-?\d+))?\]',args)
    if op in('str','str.w') and mm:
        base=R.get(mm.group(2)); off=int(mm.group(3) or 0)
        if base is not None:
            ad=base+off
            for g,p in gp.items():
                if g<=ad<g+0x40:
                    v=R.get(mm.group(1)); 
                    bits=[i for i in range(16) if v is not None and (v>>i)&1]
                    print('%08x %-14s %s %-4s val=%s pins=%s'%(a,fn(a),p,regn.get(ad-g,hex(ad-g)),hex(v) if v is not None else '?',','.join('%s%d'%(p,b) for b in bits)))
        continue
    if op in ('ldr','ldrh','ldrb') and mm:
        base=R.get(mm.group(2)); off=int(mm.group(3) or 0)
        if base is not None:
            ad=base+off
            for g,p in gp.items():
                if g<=ad<g+0x40: print('%08x %-14s %s READ %s'%(a,fn(a),p,hex(ad-g)))
    t=re.match(r'(r\d+)',args)
    if t: R.pop(t.group(1),None)
