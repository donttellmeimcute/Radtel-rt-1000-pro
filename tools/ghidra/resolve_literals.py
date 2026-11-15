import re,struct,sys
b=open(sys.argv[1],'rb').read(); src=open(sys.argv[2]).read()
def rep(m):
    a=int(m.group(1),16)-0x08002800
    if 0<=a<=len(b)-4:
        return '(*(uint*)0x%08x)'%struct.unpack_from('<I',b,a)[0] if False else 'K_%08x'%struct.unpack_from('<I',b,a)[0]
    return m.group(0)
src=re.sub(r'\bDAT_(080[0-3][0-9a-f]{4})\b',rep,src)
open(sys.argv[3],'w').write(src)
