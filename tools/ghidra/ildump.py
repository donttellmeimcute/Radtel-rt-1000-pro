import sys, dnfile
from dncil.cil.body import CilMethodBody
from dncil.cil.body.reader import CilMethodBodyReaderBase
from dncil.clr.token import Token, StringToken, InvalidToken
from dncil.cil.error import MethodBodyFormatError
class R(CilMethodBodyReaderBase):
    def __init__(s,pe,rva): s.pe=pe; s.off=pe.get_offset_from_rva(rva)
    def read(s,n):
        d=s.pe.get_data(s.pe.get_rva_from_offset(s.off),n); s.off+=n; return d
    def tell(s): return s.off
    def seek(s,o): s.off=o; return o
pe=dnfile.dnPE(sys.argv[1])
def tokname(tok):
    try:
        if isinstance(tok,StringToken): 
            s=pe.net.user_strings.get(tok.rid); s=s.value if s else '?'
            return repr(s[:80])
        t=tok.table; rid=tok.rid
        tbl=pe.net.mdtables.tables_by_name if False else None
        from dncil.clr.token import Token
        tabs={0x06:'MethodDef',0x0A:'MemberRef',0x04:'Field',0x01:'TypeRef',0x02:'TypeDef',0x2B:'MethodSpec',0x1B:'TypeSpec'}
        name=tabs.get(t)
        row=getattr(pe.net.mdtables,name)[rid-1]
        if name=='MemberRef':
            c=row.Class.row; cn=getattr(c,'TypeName','?')
            return f'{cn}::{row.Name}'
        if name in('MethodDef','Field'): return str(row.Name)
        if name in('TypeRef','TypeDef'): return f'{row.TypeNamespace}.{row.TypeName}'
        return f'{name}#{rid}'
    except Exception as e: return f'tok{tok}'
for td in pe.net.mdtables.TypeDef:
    for m in td.MethodList:
        r=m.row
        print(f'\n=== {td.TypeName}::{r.Name}')
        if not r.Rva: continue
        try: body=CilMethodBody(R(pe,r.Rva))
        except Exception as e: print('err',e); continue
        for ins in body.instructions:
            op=ins.operand
            if isinstance(op,Token): op=tokname(op)
            elif isinstance(op,list): op=[hex(x) if isinstance(x,int) else x for x in op]
            print(f'  {ins.offset:04x} {ins.opcode.name} {op if op is not None else ""}')
