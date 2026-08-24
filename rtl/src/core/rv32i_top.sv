// =============================================================================
// rv32i_top.sv — Top-Level 5-Stage Pipelined RISC-V RV32I Processor
// =============================================================================
// This is the main processor module that instantiates and wires together:
//
//   ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐   ┌──────┐
//   │  IF  │──▶│  ID  │──▶│  EX  │──▶│ MEM  │──▶│  WB  │
//   └──────┘   └──────┘   └──────┘   └──────┘   └──────┘
//       │          │          │          │          │
//     IF/ID      ID/EX     EX/MEM     MEM/WB     (regfile)
//      reg        reg        reg        reg
//
// Plus: Forwarding Unit, Hazard Detection Unit, Register File, ALU, etc.
//
// External interface:
//   clk, rst_n         — clock and active-low reset
//   halt               — processor halted (ECALL executed)
//   debug_*            — signals exposed for testbench monitoring
// =============================================================================

module rv32i_top
  import rv32i_pkg::*;
#(
  parameter IMEM_DEPTH     = 1024,   // Instruction memory words
  parameter DMEM_DEPTH     = 4096,   // Data memory bytes
  parameter IMEM_INIT_FILE = ""      // Hex file to load into instruction memory
)(
  input  logic        clk,
  input  logic        rst_n,

  // Status
  output logic        halt,

  // Debug / testbench monitoring signals
  output logic [31:0] debug_pc,
  output logic [31:0] debug_instr,
  output logic        debug_reg_write,
  output logic [4:0]  debug_reg_addr,
  output logic [31:0] debug_reg_data,
  output logic [31:0] debug_cycle_count,

  // Additional pipeline signals for visualization
  output logic [31:0] debug_pc_if,
  output logic [31:0] debug_pc_id,
  output logic [31:0] debug_pc_ex,
  output logic [31:0] debug_pc_mem,
  output logic [31:0] debug_pc_wb,
  output logic        stall_if,
  output logic        flush_id
);

  // =========================================================================
  // Internal signals
  // =========================================================================

  // --- PC ---
  logic [31:0] pc, pc_next, pc_plus4;

  // --- Pipeline registers ---
  // IF/ID
  logic [31:0] if_id_pc, if_id_pc_plus4, if_id_instr;
  logic        if_id_valid;

  // ID/EX
  logic [31:0] id_ex_pc, id_ex_pc_plus4;
  logic [31:0] id_ex_rs1_data, id_ex_rs2_data;
  logic [31:0] id_ex_imm;
  logic [4:0]  id_ex_rs1, id_ex_rs2, id_ex_rd;
  ctrl_signals_t id_ex_ctrl;
  logic        id_ex_valid;

  // EX/MEM
  logic [31:0] ex_mem_pc, ex_mem_pc_plus4;
  logic [31:0] ex_mem_alu_result;
  logic [31:0] ex_mem_rs2_data;
  logic [4:0]  ex_mem_rd;
  logic        ex_mem_branch_taken;
  logic [31:0] ex_mem_branch_target;
  ctrl_signals_t ex_mem_ctrl;
  logic        ex_mem_valid;

  // MEM/WB
  logic [31:0] mem_wb_pc, mem_wb_pc_plus4;
  logic [31:0] mem_wb_alu_result;
  logic [31:0] mem_wb_mem_data;
  logic [4:0]  mem_wb_rd;
  ctrl_signals_t mem_wb_ctrl;
  logic        mem_wb_valid;

  // --- Instruction memory output ---
  logic [31:0] imem_instr;

  // --- Register file ---
  logic [31:0] rf_rs1_data, rf_rs2_data;
  logic [31:0] wb_data;

  // --- Immediate generator ---
  logic [31:0] imm_out;

  // --- Control unit ---
  ctrl_signals_t ctrl_out;

  // --- ALU ---
  logic [31:0] alu_a, alu_b, alu_result;
  logic        alu_zero;

  // --- Branch comparator ---
  logic        branch_taken_raw;

  // --- Forwarding ---
  forward_sel_t fwd_a, fwd_b;
  logic [31:0] fwd_a_data, fwd_b_data;

  // --- Hazard detection ---
  logic stall_id, flush_ex;

  // --- Branch/Jump control ---
  logic        do_branch;      // Branch taken signal from EX stage
  logic        do_jump;        // Jump signal from EX stage
  logic [31:0] branch_target;  // Branch/Jump target PC

  // --- Data memory ---
  logic [31:0] dmem_rd_data;

  // --- Cycle counter ---
  logic [31:0] cycle_count;

  // --- Halt register ---
  logic halted;

  // =========================================================================
  // INSTRUCTION MEMORY
  // =========================================================================
  imem #(
    .DEPTH     (IMEM_DEPTH),
    .INIT_FILE (IMEM_INIT_FILE)
  ) u_imem (
    .addr  (pc),
    .instr (imem_instr)
  );

  // =========================================================================
  // ==================== STAGE 1: INSTRUCTION FETCH (IF) ====================
  // =========================================================================

  assign pc_plus4 = pc + 32'd4;

  // PC next selection
  always_comb begin
    if (do_branch || do_jump)
      pc_next = branch_target;
    else
      pc_next = pc_plus4;
  end

  // PC register
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)
      pc <= 32'b0;
    else if (halted)
      pc <= pc;  // Hold PC when halted
    else if (!stall_if)
      pc <= pc_next;
    // else: stall — hold PC
  end

  // --- IF/ID Pipeline Register ---
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      if_id_pc      <= 32'b0;
      if_id_pc_plus4 <= 32'b0;
      if_id_instr   <= NOP_INSTR;
      if_id_valid   <= 1'b0;
    end else if (halted) begin
      // Hold on halt
    end else if (flush_id) begin
      // Flush: insert bubble
      if_id_pc      <= 32'b0;
      if_id_pc_plus4 <= 32'b0;
      if_id_instr   <= NOP_INSTR;
      if_id_valid   <= 1'b0;
    end else if (!stall_id) begin
      if_id_pc      <= pc;
      if_id_pc_plus4 <= pc_plus4;
      if_id_instr   <= imem_instr;
      if_id_valid   <= 1'b1;
    end
    // else: stall — hold current values
  end

  // =========================================================================
  // ==================== STAGE 2: INSTRUCTION DECODE (ID) ===================
  // =========================================================================

  // Control Unit
  control_unit u_ctrl (
    .instruction (if_id_instr),
    .ctrl        (ctrl_out)
  );

  // Register File
  register_file u_regfile (
    .clk      (clk),
    .rst_n    (rst_n),
    .rs1_addr (if_id_instr[19:15]),
    .rs2_addr (if_id_instr[24:20]),
    .rs1_data (rf_rs1_data),
    .rs2_data (rf_rs2_data),
    .wr_en    (mem_wb_ctrl.reg_write && mem_wb_valid && (mem_wb_rd != 5'b0)),
    .wr_addr  (mem_wb_rd),
    .wr_data  (wb_data)
  );

  // Immediate Generator
  imm_gen u_immgen (
    .instruction (if_id_instr),
    .imm         (imm_out)
  );

  // --- ID/EX Pipeline Register ---
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      id_ex_pc       <= 32'b0;
      id_ex_pc_plus4 <= 32'b0;
      id_ex_rs1_data <= 32'b0;
      id_ex_rs2_data <= 32'b0;
      id_ex_imm      <= 32'b0;
      id_ex_rs1      <= 5'b0;
      id_ex_rs2      <= 5'b0;
      id_ex_rd       <= 5'b0;
      id_ex_ctrl     <= '0;
      id_ex_valid    <= 1'b0;
    end else if (halted) begin
      // Hold on halt
    end else if (flush_ex) begin
      // Flush: insert bubble (NOP)
      id_ex_pc       <= 32'b0;
      id_ex_pc_plus4 <= 32'b0;
      id_ex_rs1_data <= 32'b0;
      id_ex_rs2_data <= 32'b0;
      id_ex_imm      <= 32'b0;
      id_ex_rs1      <= 5'b0;
      id_ex_rs2      <= 5'b0;
      id_ex_rd       <= 5'b0;
      id_ex_ctrl     <= '0;
      id_ex_valid    <= 1'b0;
    end else begin
      id_ex_pc       <= if_id_pc;
      id_ex_pc_plus4 <= if_id_pc_plus4;
      id_ex_rs1_data <= rf_rs1_data;
      id_ex_rs2_data <= rf_rs2_data;
      id_ex_imm      <= imm_out;
      id_ex_rs1      <= if_id_instr[19:15];
      id_ex_rs2      <= if_id_instr[24:20];
      id_ex_rd       <= if_id_instr[11:7];
      id_ex_ctrl     <= ctrl_out;
      id_ex_valid    <= if_id_valid;
    end
  end

  // =========================================================================
  // ====================== STAGE 3: EXECUTE (EX) ===========================
  // =========================================================================

  // Forwarding Unit
  forwarding_unit u_fwd (
    .id_ex_rs1        (id_ex_rs1),
    .id_ex_rs2        (id_ex_rs2),
    .ex_mem_reg_write (ex_mem_ctrl.reg_write && ex_mem_valid),
    .ex_mem_rd        (ex_mem_rd),
    .mem_wb_reg_write (mem_wb_ctrl.reg_write && mem_wb_valid),
    .mem_wb_rd        (mem_wb_rd),
    .forward_a        (fwd_a),
    .forward_b        (fwd_b)
  );

  // Forwarding MUX A (rs1)
  always_comb begin
    case (fwd_a)
      FWD_EX_MEM: fwd_a_data = ex_mem_alu_result;
      FWD_MEM_WB: fwd_a_data = wb_data;
      default:    fwd_a_data = id_ex_rs1_data;
    endcase
  end

  // Forwarding MUX B (rs2)
  always_comb begin
    case (fwd_b)
      FWD_EX_MEM: fwd_b_data = ex_mem_alu_result;
      FWD_MEM_WB: fwd_b_data = wb_data;
      default:    fwd_b_data = id_ex_rs2_data;
    endcase
  end

  // ALU operand A: forwarded rs1 data, or PC for AUIPC
  assign alu_a = id_ex_ctrl.auipc ? id_ex_pc : fwd_a_data;

  // ALU operand B: forwarded rs2 data or immediate
  assign alu_b = id_ex_ctrl.alu_src ? id_ex_imm : fwd_b_data;

  // ALU
  alu u_alu (
    .a      (alu_a),
    .b      (alu_b),
    .op     (id_ex_ctrl.alu_op),
    .result (alu_result),
    .zero   (alu_zero)
  );

  // Branch Comparator (uses forwarded register values)
  branch_comp u_branch_comp (
    .rs1_data     (fwd_a_data),
    .rs2_data     (fwd_b_data),
    .funct3       (id_ex_ctrl.funct3),
    .branch_taken (branch_taken_raw)
  );

  // Branch/Jump resolution
  assign do_branch = id_ex_ctrl.branch && branch_taken_raw && id_ex_valid;
  assign do_jump   = (id_ex_ctrl.jump) && id_ex_valid;

  // Branch/Jump target
  always_comb begin
    if (id_ex_ctrl.jalr)
      // JALR: target = (rs1 + imm) & ~1
      branch_target = (fwd_a_data + id_ex_imm) & 32'hFFFF_FFFE;
    else
      // JAL / Branch: target = PC + imm
      branch_target = id_ex_pc + id_ex_imm;
  end

  // --- EX/MEM Pipeline Register ---
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      ex_mem_pc            <= 32'b0;
      ex_mem_pc_plus4      <= 32'b0;
      ex_mem_alu_result    <= 32'b0;
      ex_mem_rs2_data      <= 32'b0;
      ex_mem_rd            <= 5'b0;
      ex_mem_branch_taken  <= 1'b0;
      ex_mem_branch_target <= 32'b0;
      ex_mem_ctrl          <= '0;
      ex_mem_valid         <= 1'b0;
    end else if (halted) begin
      // Hold on halt
    end else begin
      ex_mem_pc            <= id_ex_pc;
      ex_mem_pc_plus4      <= id_ex_pc_plus4;
      ex_mem_alu_result    <= alu_result;
      ex_mem_rs2_data      <= fwd_b_data; // Store data (forwarded)
      ex_mem_rd            <= id_ex_rd;
      ex_mem_branch_taken  <= do_branch;
      ex_mem_branch_target <= branch_target;
      ex_mem_ctrl          <= id_ex_ctrl;
      ex_mem_valid         <= id_ex_valid;
    end
  end

  // =========================================================================
  // ====================== STAGE 4: MEMORY (MEM) ===========================
  // =========================================================================

  dmem #(
    .DEPTH (DMEM_DEPTH)
  ) u_dmem (
    .clk       (clk),
    .rst_n     (rst_n),
    .addr      (ex_mem_alu_result),
    .wr_data   (ex_mem_rs2_data),
    .mem_read  (ex_mem_ctrl.mem_read && ex_mem_valid),
    .mem_write (ex_mem_ctrl.mem_write && ex_mem_valid),
    .funct3    (ex_mem_ctrl.funct3),
    .rd_data   (dmem_rd_data)
  );

  // --- MEM/WB Pipeline Register ---
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      mem_wb_pc         <= 32'b0;
      mem_wb_pc_plus4   <= 32'b0;
      mem_wb_alu_result <= 32'b0;
      mem_wb_mem_data   <= 32'b0;
      mem_wb_rd         <= 5'b0;
      mem_wb_ctrl       <= '0;
      mem_wb_valid      <= 1'b0;
    end else if (halted) begin
      // Hold on halt
    end else begin
      mem_wb_pc         <= ex_mem_pc;
      mem_wb_pc_plus4   <= ex_mem_pc_plus4;
      mem_wb_alu_result <= ex_mem_alu_result;
      mem_wb_mem_data   <= dmem_rd_data;
      mem_wb_rd         <= ex_mem_rd;
      mem_wb_ctrl       <= ex_mem_ctrl;
      mem_wb_valid      <= ex_mem_valid;
    end
  end

  // =========================================================================
  // ====================== STAGE 5: WRITE-BACK (WB) ========================
  // =========================================================================

  // WB data MUX: select between ALU result, memory data, or PC+4
  always_comb begin
    if (mem_wb_ctrl.jump || mem_wb_ctrl.jalr)
      wb_data = mem_wb_pc_plus4;           // JAL/JALR: rd = PC+4
    else if (mem_wb_ctrl.mem_to_reg)
      wb_data = mem_wb_mem_data;           // Load: rd = mem[addr]
    else
      wb_data = mem_wb_alu_result;         // ALU: rd = alu_result
  end

  // Write-back happens through the register file instance above (u_regfile)

  // =========================================================================
  // HAZARD DETECTION UNIT
  // =========================================================================

  hazard_unit u_hazard (
    .id_ex_mem_read (id_ex_ctrl.mem_read && id_ex_valid),
    .id_ex_rd       (id_ex_rd),
    .if_id_rs1      (if_id_instr[19:15]),
    .if_id_rs2      (if_id_instr[24:20]),
    .branch_taken   (do_branch),
    .jump           (do_jump),
    .stall_if       (stall_if),
    .stall_id       (stall_id),
    .flush_id       (flush_id),
    .flush_ex       (flush_ex)
  );

  // =========================================================================
  // HALT DETECTION (ECALL)
  // =========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)
      halted <= 1'b0;
    else if (ex_mem_ctrl.is_ecall && ex_mem_valid)
      halted <= 1'b1;
  end

  assign halt = halted;

  // =========================================================================
  // CYCLE COUNTER
  // =========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n)
      cycle_count <= 32'b0;
    else if (!halted)
      cycle_count <= cycle_count + 32'd1;
  end

  // =========================================================================
  // DEBUG OUTPUTS
  // =========================================================================
  assign debug_pc          = mem_wb_pc;
  assign debug_instr       = imem_instr;
  assign debug_reg_write   = mem_wb_ctrl.reg_write && (mem_wb_rd != 0);
  assign debug_reg_addr    = mem_wb_rd;
  assign debug_reg_data    = wb_data;

  // Additional visualization signals
  assign debug_pc_if       = pc;
  assign debug_pc_id       = if_id_pc;
  assign debug_pc_ex       = id_ex_pc;
  assign debug_pc_mem      = ex_mem_pc;
  assign debug_pc_wb       = mem_wb_pc;

  // Cycle counter
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) debug_cycle_count <= '0;
    else        debug_cycle_count <= debug_cycle_count + 1;
  end

endmodule
