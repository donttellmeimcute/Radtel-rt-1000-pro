import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
public class DumpAll extends GhidraScript {
  public void run() throws Exception {
    String out = getScriptArgs()[0];
    Memory mem = currentProgram.getMemory();
    AddressSpace sp = currentProgram.getAddressFactory().getDefaultAddressSpace();
    DecompInterface d=new DecompInterface(); d.openProgram(currentProgram);
    PrintWriter pw=new PrintWriter(new FileWriter(out));
    for (Function f: currentProgram.getFunctionManager().getFunctions(true)){
      DecompileResults r=d.decompileFunction(f,60,monitor);
      pw.println("// ==== "+f.getName()+" @ "+f.getEntryPoint()+" size="+f.getBody().getNumAddresses());
      if (r!=null && r.decompileCompleted()) pw.println(r.getDecompiledFunction().getC()); else pw.println("// decompile failed");
    }
    pw.close();
  }
}
