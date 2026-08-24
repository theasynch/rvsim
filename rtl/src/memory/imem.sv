// =============================================================================
// imem.sv — Instruction Memory
// =============================================================================
// Word-addressed, read-only instruction memory.
// Initialized from a hex file via $readmemh at simulation start.
//
// Parameters:
//   DEPTH — number of 32-bit words (default 1024 = 4KB)
//   INIT_FILE — path to hex file for initialization
//
// Interface:
//   addr[31:0] — byte address from PC (word-aligned, bits [1:0] ignored)
//   instr[31:0] — instruction word at that address
// =============================================================================

module imem #(
  parameter int DEPTH     = 1024,
  parameter     INIT_FILE = ""
)(
  input  logic [31:0] addr,
  output logic [31:0] instr
);

  // Memory array — each element is one 32-bit instruction word
  logic [31:0] mem [0:DEPTH-1];

  // Initialize from hex file if provided
  initial begin
    // Zero out memory first
    for (int i = 0; i < DEPTH; i++) begin
      mem[i] = 32'h0000_0013; // NOP (ADDI x0, x0, 0)
    end
    // Load program
    if (INIT_FILE != "") begin
      $readmemh(INIT_FILE, mem);
    end
  end

  // Combinational read — word-aligned (divide byte address by 4)
  // Addresses beyond memory range return NOP
  wire [31:0] word_addr = addr >> 2;

  assign instr = (word_addr < DEPTH) ? mem[word_addr] : 32'h0000_0013;

endmodule
