import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;
public class Prep extends GhidraScript {
  public void run() throws Exception {
    Memory mem = currentProgram.getMemory();
    AddressSpace sp = currentProgram.getAddressFactory().getDefaultAddressSpace();
    for (MemoryBlock b: mem.getBlocks()) { b.setWrite(false); b.setName("FLASH"); }
    mem.createUninitializedBlock("RAM", sp.getAddress(0x20000000L), 0x60000, false).setWrite(true);
    MemoryBlock p=mem.createUninitializedBlock("PERIPH", sp.getAddress(0x40000000L), 0x100000, false); p.setVolatile(true); p.setWrite(true);
    MemoryBlock s=mem.createUninitializedBlock("SCS", sp.getAddress(0xE0000000L), 0x100000, false); s.setVolatile(true); s.setWrite(true);
    long base=0x08002800L;
    for (int i=1;i<120;i++){
      long v = mem.getInt(sp.getAddress(base+i*4)) & 0xffffffffL;
      if (v>=base && v<0x08040000L){
        Address a=sp.getAddress(v & ~1L);
        disassemble(a);
        if (getFunctionAt(a)==null) createFunction(a, i==1?"Reset_Handler":"vec_"+i);
      }
    }
  }
}
