// forwarding_unit.cpp
#include "core/forwarding_unit.h"

namespace rv32i {

ForwardingSignals ForwardingUnit::compute(
    const IDEXReg&  id_ex,
    const EXMEMReg& ex_mem,
    const MEMWBReg& mem_wb)
{
    ForwardingSignals f;

    // -----------------------------------------------------------------------
    // Forward A (ALU input A = rs1 of instruction in EX)
    //
    // Check EX hazard first (priority: closest instruction wins)
    // EX hazard condition:
    //   EX/MEM.RegWrite AND EX/MEM.Rd != x0 AND EX/MEM.Rd == ID/EX.Rs1
    // -----------------------------------------------------------------------
    if (ex_mem.valid && ex_mem.ctrl.reg_write &&
        ex_mem.rd != 0 && ex_mem.rd == id_ex.rs1)
    {
        f.forward_a = ForwardSel::EX_MEM;
    }
    // MEM hazard (only if EX hazard didn't fire):
    //   MEM/WB.RegWrite AND MEM/WB.Rd != x0 AND MEM/WB.Rd == ID/EX.Rs1
    else if (mem_wb.valid && mem_wb.ctrl.reg_write &&
             mem_wb.rd != 0 && mem_wb.rd == id_ex.rs1)
    {
        f.forward_a = ForwardSel::MEM_WB;
    }

    // -----------------------------------------------------------------------
    // Forward B (ALU input B = rs2 of instruction in EX)
    // Same logic; note that alu_src may override this with an immediate, but
    // we still compute it — the EX stage multiplexer picks the right value.
    // -----------------------------------------------------------------------
    if (ex_mem.valid && ex_mem.ctrl.reg_write &&
        ex_mem.rd != 0 && ex_mem.rd == id_ex.rs2)
    {
        f.forward_b = ForwardSel::EX_MEM;
    }
    else if (mem_wb.valid && mem_wb.ctrl.reg_write &&
             mem_wb.rd != 0 && mem_wb.rd == id_ex.rs2)
    {
        f.forward_b = ForwardSel::MEM_WB;
    }

    return f;
}

} // namespace rv32i
