<div align="center">

```
██████╗ ██╗   ██╗███████╗██╗███╗   ███╗
██╔══██╗██║   ██║██╔════╝██║████╗ ████║
██████╔╝██║   ██║███████╗██║██╔████╔██║
██╔══██╗╚██╗ ██╔╝╚════██║██║██║╚██╔╝██║
██║  ██║ ╚████╔╝ ███████║██║██║ ╚═╝ ██║
╚═╝  ╚═╝  ╚═══╝  ╚══════╝╚═╝╚═╝     ╚═╝
```

### **RISC-V Processor Simulation & Visualization Tool**
*Computer Architecture — 30 Mark Project*

[![Status](https://img.shields.io/badge/status-Planning%20Phase-blue?style=flat-square)]()
[![ISA](https://img.shields.io/badge/ISA-RV32I-orange?style=flat-square)](https://riscv.org/)
[![RTL](https://img.shields.io/badge/RTL-SystemVerilog-blueviolet?style=flat-square)]()
[![Sim](https://img.shields.io/badge/Sim-Verilator-green?style=flat-square)]()
[![License](https://img.shields.io/badge/license-All%20Rights%20Reserved-red?style=flat-square)](LICENSE)

</div>

---

## 📌 Project Idea

Most computer architecture simulators are either too academic (just a textbook diagram) or too complex (industry tools that require a PhD to operate). There is no tool a professor can hand to a student, have them run in 10 seconds, and use it to *see* a processor thinking — cycle by cycle.

**RVSim** is that tool.

It is an interactive, standalone desktop application that simulates a RISC-V RV32I processor at the cycle level. A professor downloads one file, double-clicks it, and a browser opens showing a live 5-stage pipeline diagram. They load a program, press Step, and watch every instruction travel through Fetch → Decode → Execute → Memory → Writeback in real time. They can toggle forwarding on and off, swap branch predictors, resize the cache, and immediately see how each decision affects performance.

The goal is not just to build a simulator — it is to make pipeline concepts *viscerally obvious* to someone who has only ever seen them as static diagrams on a slide.

---

## 🎯 What the Final Product Does

When complete, a professor or student will:

1. **Download `RVSim.exe`** — a single file, ~1MB, no installation
2. **Double-click it** — a browser opens at `localhost:8080` automatically
3. **Select a program** from a built-in library (Fibonacci, bubble sort, matrix multiply, etc.) or **type their own RISC-V assembly** directly in the browser
4. **Step through execution cycle by cycle**, watching:
   - Instructions flowing through all 5 pipeline stages simultaneously
   - Stall bubbles appearing on load-use hazards
   - Flush events on branch mispredictions
   - Register values updating in real time
   - Cache sets being filled, hits and misses counted
5. **Change configurations** mid-run: swap the branch predictor, change cache associativity, toggle data forwarding — and immediately see the IPC and miss rate change

No Python. No Node.js. No Verilator. No toolchain. One file.

---

## 🏗️ Planned Architecture

The project has two major technical layers that both model the same processor:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           RVSim — System Overview                       │
│                                                                         │
│   ┌──────────────────────────┐      ┌──────────────────────────────┐   │
│   │   SystemVerilog RTL      │      │   C++ Behavioral Simulator   │   │
│   │   (Actual Hardware)      │      │   (Drives the Web UI)        │   │
│   │                          │      │                              │   │
│   │  rv32i_top.sv            │      │  pipeline.cpp                │   │
│   │  alu.sv                  │      │  decoder.cpp                 │   │
│   │  hazard_unit.sv          │ same │  hazard_unit.cpp             │   │
│   │  forwarding_unit.sv      │design│  forwarding_unit.cpp         │   │
│   │  register_file.sv        │      │  cache.cpp                   │   │
│   │  imem.sv / dmem.sv       │      │  branch predictors           │   │
│   └──────────┬───────────────┘      └──────────────┬───────────────┘   │
│              │                                      │                   │
│              ▼                                      ▼                   │
│      Verilator → C++                      Embedded HTTP Server          │
│      .vcd waveforms                       REST API (cpp-httplib)        │
│      (GTKWave)                            Serves embedded frontend      │
│                                                     │                   │
│                                                     ▼                   │
│                                           localhost:8080                │
│                                           Interactive Web UI            │
└─────────────────────────────────────────────────────────────────────────┘
```

### Why Two Implementations?

| Layer | Purpose |
|-------|---------|
| **SystemVerilog RTL** | Actual synthesizable hardware — can be run on an FPGA, waveforms can be analyzed in GTKWave. Proves the design is architecturally sound. |
| **C++ Behavioral Simulator** | Fast, flexible, drives the web interface. Can be extended with features (branch predictors, cache) that would require significant hardware to simulate in RTL. |

Both are designed from the same architectural specification. The RTL is the hardware; the C++ is the software model of that hardware.

---

## 🔩 Planned Modules

### Layer 1 — SystemVerilog RTL (`rtl/`)

| Module | File | Description |
|--------|------|-------------|
| **ISA Package** | `pkg/rv32i_pkg.sv` | All type definitions: opcodes, ALU ops, forwarding selectors, pipeline register structs |
| **Top-Level Processor** | `core/rv32i_top.sv` | Instantiates and wires all units; contains the 4 pipeline registers |
| **Control Unit** | `core/control_unit.sv` | Decodes opcode/funct3/funct7 into control signals for every stage |
| **ALU** | `datapath/alu.sv` | 32-bit combinational ALU: ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND |
| **Branch Comparator** | `datapath/branch_comp.sv` | Evaluates BEQ, BNE, BLT, BGE, BLTU, BGEU using forwarded register values |
| **Immediate Generator** | `datapath/imm_gen.sv` | Sign-extends immediates for all 6 RV32I instruction formats (R/I/S/B/U/J) |
| **Register File** | `datapath/register_file.sv` | 32 × 32-bit registers; x0 hardwired to zero; synchronous write, async read |
| **Hazard Detection Unit** | `hazard/hazard_unit.sv` | Detects load-use hazards; generates stall_if, stall_id, flush_id, flush_ex |
| **Forwarding Unit** | `hazard/forwarding_unit.sv` | Detects EX/MEM → EX and MEM/WB → EX data hazards; selects bypass path |
| **Instruction Memory** | `memory/imem.sv` | Read-only synchronous memory; loads programs from `.hex` files |
| **Data Memory** | `memory/dmem.sv` | Read/write byte-addressable memory; supports LB/LH/LW/SB/SH/SW |

### Layer 2 — C++ Behavioral Simulator (`simulator/`)

| Module | Description |
|--------|-------------|
| **ISA Decoder** | Full RV32I decode: all 47 instructions, all 6 formats, produces control signals |
| **Pipeline Engine** | Cycle-accurate 5-stage simulation with pipeline registers as C++ structs |
| **Register File** | 32 × 32-bit register file with ABI name mapping |
| **Memory Model** | Flat 16MB byte-addressable memory with hex program loader |
| **Hazard Unit** | Load-use detection, stall/flush signal generation |
| **Forwarding Unit** | EX/MEM and MEM/WB bypass path computation |
| **Branch Predictors** | Static Not-Taken, Static Taken, 2-Bit Saturating, GShare, Tournament |
| **Cache Model** | Configurable L1 I-Cache and D-Cache: any size, associativity, LRU or FIFO |
| **Performance Counters** | IPC, stall cycles, flush cycles, miss rate, misprediction rate |

### Layer 3 — Web Interface (`frontend/`)

| Feature | Description |
|---------|-------------|
| **Pipeline Diagram** | Live 5-stage visualization; highlights stalls (yellow), flushes (red), forwarding (green) |
| **Register Viewer** | All 32 ABI registers; flashes on write |
| **Cache State** | Set/way occupancy visualization; hit/miss bar |
| **Predictor Internals** | PHT (Pattern History Table) and GHR (Global History Register) state |
| **Statistics Panel** | IPC, cycle count, stall cycles, flush cycles, misprediction rate, miss rate |
| **Assembly Editor** | Type RV32I assembly in the browser; assembles and loads instantly |

### Layer 4 — HTTP Server (`server/`)

| Component | Description |
|-----------|-------------|
| **Embedded Server** | `cpp-httplib` running on `localhost:8080` — no separate install |
| **REST API** | `POST /api/step`, `POST /api/run`, `POST /api/load`, `GET /api/state` etc. |
| **Frontend Embedding** | `tools/embed_frontend.py` bakes `index.html` into a C++ string at build time |

---

## 📐 The Pipeline — Design Specification

```
              RISC-V 5-Stage In-Order Pipeline (RV32I)

  ┌─────────┐  IF/ID  ┌─────────┐  ID/EX  ┌─────────┐  EX/MEM ┌─────────┐  MEM/WB ┌─────────┐
  │   IF    │════════▶│   ID    │════════▶│   EX    │════════▶│   MEM   │════════▶│   WB    │
  │         │  reg    │         │  reg    │         │  reg    │         │  reg    │         │
  │ Fetch   │         │ Decode  │         │  ALU    │         │ D-Cache │         │ RegFile │
  │ I-Cache │         │ RegRead │         │ Branch  │         │  Load / │         │  Write  │
  │ PC + 4  │         │ ImmGen  │         │ Forward │         │  Store  │         │         │
  └─────────┘         └─────────┘         └─────────┘         └─────────┘         └─────────┘
                           │                   ▲
                           │    Forwarding ─────┤ EX/MEM → EX (1-cycle old result)
                           │    Unit       ─────┘ MEM/WB → EX (2-cycle old result)
                           │
                      Hazard Unit ──── detects load-use ──── asserts stall_if, stall_id
                                  ──── detects branch    ──── asserts flush_id, flush_ex
```

### Hazard Handling Plan

| Hazard | When | Detected By | Resolution |
|--------|------|-------------|------------|
| **Load-Use RAW** | `LW x1` then `ADD x3,x1,x2` | `id_ex_mem_read AND id_ex_rd == if_id_rs` | Stall 1 cycle, insert NOP bubble |
| **EX→EX RAW** | `ADD x1` then `ADD x3,x1,x2` | `ex_mem_regwrite AND ex_mem_rd == id_ex_rs` | Forward EX/MEM result to EX input |
| **MEM→EX RAW** | `ADD x1` 2 cycles ago, now used | `mem_wb_regwrite AND mem_wb_rd == id_ex_rs` | Forward MEM/WB result to EX input |
| **Control Hazard** | Branch taken / JAL / JALR | Branch resolved in EX | Flush IF and ID (2 penalty cycles) |

---

## 📅 Project Timeline

| Review | Date | What Will Be Shown |
|--------|------|--------------------|
| **Review 1** | August 28, 2026 | Project proposal, architecture design, module specifications, implementation plan *(this document)* |
| **Review 2** | September 15, 2026 | Working pipeline simulator, live hazard/forwarding demo, basic cache visualization, web UI running |
| **Final Review** | October 13, 2026 | Complete tool — all branch predictors, full cache configurability, performance analysis charts, project report |

---

## 🛠️ Planned Tech Stack

| Component | Technology | Reason |
|-----------|-----------|--------|
| Hardware Design | SystemVerilog (IEEE 1800-2017) | Industry standard for RTL; synthesizable to FPGA |
| RTL Simulation | Verilator 5.x | Fast open-source RTL simulator; compiles SV to C++ |
| Waveform Viewer | GTKWave | Standard tool for viewing `.vcd` signal traces |
| Behavioral Simulator | C++17 | Performance; can model cache/predictor without RTL complexity |
| HTTP Server | cpp-httplib (header-only) | Embedded in the `.exe`; zero install |
| JSON | nlohmann/json (header-only) | API responses between simulator and browser |
| Web UI | Vanilla HTML/CSS/JS | No framework dependencies; works in any browser |
| Build System | CMake 3.20+ | Cross-platform; integrates Python embedding step |
| Standalone Packaging | MSVC Static Runtime (/MT) | Zero DLL dependencies on end-user machine |

---

## 📁 Planned Repository Structure

```
rvsim/
│
├── 📄 README.md                    This document
├── 📄 CMakeLists.txt               Root build config
├── 📄 vcpkg.json                   C++ dependency manifest
├── 📄 setup.ps1                    One-click Windows build script
│
├── 📂 rtl/                         SystemVerilog Hardware Design
│   ├── src/
│   │   ├── pkg/rv32i_pkg.sv        ISA types, structs, constants
│   │   ├── core/rv32i_top.sv       Top-level processor + pipeline registers
│   │   ├── core/control_unit.sv    Instruction → control signals decode
│   │   ├── datapath/alu.sv         32-bit ALU
│   │   ├── datapath/branch_comp.sv Branch condition evaluator
│   │   ├── datapath/imm_gen.sv     Immediate sign-extension (all 6 formats)
│   │   ├── datapath/register_file.sv 32×32 register file
│   │   ├── hazard/hazard_unit.sv   Stall + flush signal generation
│   │   ├── hazard/forwarding_unit.sv Data bypass path selection
│   │   └── memory/imem.sv + dmem.sv  Instruction + data memories
│   ├── tb/sim_main.cpp             Verilator C++ testbench
│   ├── sw/tests/                   RISC-V assembly test programs
│   └── Makefile                    RTL build + simulation targets
│
├── 📂 simulator/                   C++ Behavioral Simulator
│   ├── include/isa/                ISA types + decoder interface
│   ├── include/core/               Pipeline, hazard, forwarding, memory
│   ├── include/branch/             Branch predictor interfaces
│   ├── include/cache/              Cache model interface
│   └── src/                        Implementations
│
├── 📂 server/                      Embedded HTTP API Server
│   ├── include/api_server.h
│   └── src/api_server.cpp          Route handlers
│
├── 📂 frontend/
│   └── index.html                  Complete single-file web application
│
├── 📂 third_party/                 Bundled dependencies (no install needed)
│   ├── httplib/httplib.h
│   └── nlohmann/json.hpp
│
└── 📂 tools/
    └── embed_frontend.py           Bakes HTML into C++ string at build time
```

---

## 🔬 RISC-V ISA — Instruction Formats

The RV32I base ISA uses 6 fixed-width 32-bit instruction formats:

```
 31        25 24    20 19    15 14  12 11        7 6       0
┌────────────┬────────┬────────┬──────┬───────────┬────────┐
│   funct7   │  rs2   │  rs1   │funct3│    rd     │ opcode │  R-type (register ops)
├────────────┴────────┼────────┼──────┼───────────┼────────┤
│        imm[11:0]    │  rs1   │funct3│    rd     │ opcode │  I-type (loads, ADDI)
├─────────────────────┴────────┼──────┼───────────┼────────┤
│    imm[11:5]  │     rs2│ rs1 │funct3│ imm[4:0]  │ opcode │  S-type (stores)
├───────────────┴────────┴─────┼──────┼───────────┼────────┤
│         imm[12|10:5]         │funct3│ imm[4:1|11]│opcode │  B-type (branches)
├──────────────────────────────┴──────┴───────────┼────────┤
│                  imm[31:12]                      │ opcode │  U-type (LUI, AUIPC)
├──────────────────────────────────────────────────┼────────┤
│            imm[20|10:1|11|19:12]                 │ opcode │  J-type (JAL)
└──────────────────────────────────────────────────┴────────┘
```

The immediate generator handles all 6 formats, sign-extending the immediate to 32 bits and reassembling the non-contiguous bits (especially in B-type and J-type).

---

## 📊 Expected Performance Results

After implementation, we plan to generate the following experimental data for the report:

| Experiment | Variable | Expected Observation |
|-----------|----------|---------------------|
| Forwarding ON vs OFF | IPC | ~30–50% IPC improvement with forwarding on typical programs |
| Branch predictor comparison | Misprediction rate | Static: ~40–60%, 2-bit: ~5–15% on loops, GShare: ~3–10% |
| Cache size sweep (1KB→32KB) | Miss rate | Steep drop from 1KB→8KB; flat after 16KB for test programs |
| Direct-mapped vs 4-way | Miss rate | 4-way reduces conflict misses; gap is largest on matrix multiply |
| Load-use frequency | Stall cycles | Load-heavy programs: up to 20% stall cycles without forwarding |

---

## 📖 References

- Patterson & Hennessy — *Computer Organization and Design: RISC-V Edition* (the pipeline design follows this textbook directly)
- RISC-V Foundation — *The RISC-V Instruction Set Manual, Volume I: Unprivileged ISA*
- Verilator documentation — *Fast Simulation of SystemVerilog Designs*
- Hennessy & Patterson — *Computer Architecture: A Quantitative Approach* (branch prediction and cache sections)

---

<div align="center">

*Computer Architecture Project — Review 1 Presentation*
*August 28, 2026*

</div>
