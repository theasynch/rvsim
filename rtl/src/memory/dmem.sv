// =============================================================================
// dmem.sv — Data Memory
// =============================================================================
// Byte-addressable data memory supporting LB/LH/LW/SB/SH/SW operations.
//
// - Synchronous write (posedge clk)
// - Combinational read (for single-cycle MEM stage)
// - Supports byte, halfword, and word access via funct3
// - Sign/zero extension on loads controlled by funct3
//
// Parameters:
//   DEPTH — number of bytes (default 4096 = 4KB)
//
// The memory is organized as a byte array for precise sub-word access,
// matching real hardware behavior.
// =============================================================================

module dmem #(
  parameter int DEPTH = 4096
)(
  input  logic        clk,
  input  logic        rst_n,

  // Read/Write interface
  input  logic [31:0] addr,        // Byte address
  input  logic [31:0] wr_data,     // Data to write
  input  logic        mem_read,    // Read enable
  input  logic        mem_write,   // Write enable
  input  logic [2:0]  funct3,      // Size: 000=B, 001=H, 010=W, 100=BU, 101=HU
  output logic [31:0] rd_data      // Data read (sign/zero extended)
);

  import rv32i_pkg::*;

  // Byte-addressed memory array
  logic [7:0] mem [0:DEPTH-1];

  // Byte offset within the memory
  logic [31:0] masked_addr;
  assign masked_addr = addr % DEPTH; // Wrap around

  // --- Combinational read with sign/zero extension ---
  always_comb begin
    rd_data = 32'b0;
    if (mem_read) begin
      case (funct3)
        F3_BYTE:  // LB — sign-extended byte
          rd_data = {{24{mem[masked_addr][7]}}, mem[masked_addr]};

        F3_HALF:  // LH — sign-extended halfword (little-endian)
          rd_data = {{16{mem[masked_addr+1][7]}}, mem[masked_addr+1], mem[masked_addr]};

        F3_WORD:  // LW — full word (little-endian)
          rd_data = {mem[masked_addr+3], mem[masked_addr+2],
                     mem[masked_addr+1], mem[masked_addr]};

        F3_BYTEU: // LBU — zero-extended byte
          rd_data = {24'b0, mem[masked_addr]};

        F3_HALFU: // LHU — zero-extended halfword
          rd_data = {16'b0, mem[masked_addr+1], mem[masked_addr]};

        default:
          rd_data = 32'b0;
      endcase
    end
  end

  // --- Synchronous write ---
  always_ff @(posedge clk) begin
    if (mem_write) begin
      case (funct3)
        F3_BYTE: // SB
          mem[masked_addr] <= wr_data[7:0];

        F3_HALF: begin // SH (little-endian)
          mem[masked_addr]   <= wr_data[7:0];
          mem[masked_addr+1] <= wr_data[15:8];
        end

        F3_WORD: begin // SW (little-endian)
          mem[masked_addr]   <= wr_data[7:0];
          mem[masked_addr+1] <= wr_data[15:8];
          mem[masked_addr+2] <= wr_data[23:16];
          mem[masked_addr+3] <= wr_data[31:24];
        end

        default: ; // No write for other funct3 values
      endcase
    end
  end

  // --- Reset ---
  // Note: In simulation, we initialize to zero. In synthesis, block RAM
  // may or may not support reset depending on the FPGA fabric.
  initial begin
    for (int i = 0; i < DEPTH; i++) begin
      mem[i] = 8'b0;
    end
  end

endmodule
