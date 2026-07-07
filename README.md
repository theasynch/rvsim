<div align="center">

```
██████╗ ██╗   ██╗███████╗██╗███╗   ███╗
██╔══██╗██║   ██║██╔════╝██║████╗ ████║
██████╔╝██║   ██║███████╗██║██╔████╔██║
██╔══██╗╚██╗ ██╔╝╚════██║██║██║╚██╔╝██║
██║  ██║ ╚████╔╝ ███████║██║██║ ╚═╝ ██║
╚═╝  ╚═╝  ╚═══╝  ╚══════╝╚═╝╚═╝     ╚═╝
```

### **Cycle-Accurate RISC-V Pipeline & Memory Hierarchy Simulator**

*A fully interactive, standalone desktop application for learning computer architecture*

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square&logo=cplusplus)](https://isocpp.org/)
[![RISC-V](https://img.shields.io/badge/ISA-RV32I-orange?style=flat-square)](https://riscv.org/)
[![Platform](https://img.shields.io/badge/platform-Windows-lightgrey?style=flat-square&logo=windows)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/license-MIT-green?style=flat-square)](LICENSE)
[![Build](https://img.shields.io/badge/build-CMake%204.3-red?style=flat-square&logo=cmake)](https://cmake.org/)

<br/>

> **Double-click `RVSim.exe` → browser opens → start learning.**  
> No Python. No Node.js. No installation. One file.

<br/>

</div>

---

## ✨ What is RVSim?

RVSim is a **cycle-accurate microarchitecture simulator** for the RISC-V RV32I instruction set. It models a classic textbook 5-stage in-order pipeline — the same architecture taught in every computer architecture course — with a beautiful interactive web interface embedded directly in the executable.

Load a program, step through it cycle by cycle, and **watch every instruction flow through Fetch → Decode → Execute → Memory → Writeback** in real time. Toggle forwarding on/off to see stalls appear. Swap branch predictors and watch the misprediction rate change. Resize the cache and observe miss rates. Every concept you learned in lecture, made tangible.

---

## 🎬 Feature Overview

<table>
<tr>
<td width="50%">

### 🔄 5-Stage Pipeline
- Cycle-accurate **IF → ID → EX → MEM → WB**
- Live pipeline diagram updates every cycle
- **Stall** visualization (load-use hazards)
- **Flush** visualization (branch mispredictions)
- **Forwarding paths** shown as animated overlays

</td>
<td width="50%">

### 🧠 Branch Prediction
- **Static Not-Taken** — always predict not taken
- **Static Taken** — always predict taken
- **2-Bit Saturating Counter** — FSM with 4 states
- **GShare** — XOR of PC and global history
- **Tournament** — hybrid local + global + chooser

</td>
</tr>
<tr>
<td width="50%">

### 💾 Cache Hierarchy
- **Direct-Mapped**, N-way Set-Associative, Fully-Associative
- **LRU** and **FIFO** replacement policies
- Separate **I-Cache** and **D-Cache**
- Hit/miss rate tracking and set-state visualization
- Configurable size (1KB → 32KB)

</td>
<td width="50%">

### ⌨️ In-Browser Assembler
- Type **RISC-V assembly** directly in the app
- All 47 RV32I instructions supported
- Common pseudo-instructions: `NOP`, `MV`, `LI`, `RET`, `J`, `BEQZ`, `BNEZ`
- Label support for branches and jumps
- Instant error feedback

</td>
</tr>
</table>

---

## 🚀 Quick Start

### Option A — Build & Run (Recommended)

```powershell
# 1. Clone the repo
git clone https://github.com/theasynch/coa-prj.git
cd coa-prj

# 2. Configure CMake (no package manager needed — headers are bundled)
$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake -B build -S .

# 3. Build
& $cmake --build build --config Release

# 4. Run — browser opens automatically
.\bin\Release\RVSim.exe
```

> **Requirements:** Visual Studio 2022 (Desktop C++ workload). That's it.

### Option B — Download Pre-built Exe

Grab the latest `RVSim.exe` from the [**Releases page**](https://github.com/theasynch/coa-prj/releases/latest) — no installation, just double-click.

> **Requirements:** Windows 10/11. Nothing else.

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                          RVSim.exe                                  │
│                                                                     │
│   ┌─────────────────────────────────────────────────────────────┐  │
│   │                    C++ Simulator Core                        │  │
│   │                                                             │  │
│   │  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │  │
│   │  │ISA Decode│  │ Pipeline │  │  Branch  │  │  Cache   │   │  │
│   │  │ RV32I    │→ │IF ID EX  │← │Predictors│  │ L1 I & D │   │  │
│   │  │ 47 instrs│  │MEM  WB   │  │ ×4 types │  │ LRU/FIFO │   │  │
│   │  └──────────┘  └────┬─────┘  └──────────┘  └──────────┘   │  │
│   │                     │  Hazard Unit + Forwarding Unit        │  │
│   └─────────────────────┼───────────────────────────────────────┘  │
│                         │ JSON over REST API                        │
│   ┌─────────────────────▼───────────────────────────────────────┐  │
│   │              Embedded HTTP Server (cpp-httplib)              │  │
│   │         POST /api/step   GET /api/state   POST /api/load    │  │
│   └─────────────────────┬───────────────────────────────────────┘  │
│                         │ Serves embedded HTML/CSS/JS               │
└─────────────────────────┼───────────────────────────────────────────┘
                          │
                    localhost:8080
                          │
              ┌───────────▼────────────┐
              │   User's Browser       │
              │  Pipeline Visualizer   │
              │  Register File Viewer  │
              │  Cache State Display   │
              │  Predictor PHT/GHR     │
              │  In-Browser Assembler  │
              └────────────────────────┘
```

### Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| **C++ simulator core** | Performance + academic credibility; signals the simulator is serious |
| **Embedded HTTP server** | Self-contained `.exe`; no Electron bloat, no Node.js required |
| **Frontend baked into binary** | `embed_frontend.py` converts HTML → C++ `const char*` at build time |
| **Static MSVC runtime** | `/MT` flag means zero DLL dependencies on end-user machines |
| **REST API + JSON** | Clean separation; frontend can run independently during development |

---

## 🔬 The Pipeline — In Detail

```
                    RISC-V 5-Stage In-Order Pipeline

  ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐    ┌────────┐
  │   IF   │───▶│   ID   │───▶│   EX   │───▶│  MEM   │───▶│   WB   │
  │        │    │        │    │        │    │        │    │        │
  │ Fetch  │    │ Decode │    │  ALU   │    │ D-Cache│    │ RegFile│
  │ I-Cache│    │ RegRead│    │ Branch │    │ Load / │    │  Write │
  │ PC+4   │    │ Control│    │ Forward│    │  Store │    │        │
  └────────┘    └────────┘    └────────┘    └────────┘    └────────┘
      ▲              │              │              │
      │         IF/ID reg      ID/EX reg      EX/MEM reg
      │         (flip-flop)   (flip-flop)    (flip-flop)
      │                            │
      │              ◄─────────────┘  Forwarding (EX→EX, MEM→EX)
      │
      └──── Hazard Unit: stall PC + IF/ID on load-use hazard
            Branch Unit: flush IF + ID on taken branch (2-cycle penalty)
```

### Hazard Types Handled

| Hazard | Type | Detection | Resolution |
|--------|------|-----------|------------|
| `LW x1` → `ADD x3,x1,x2` | Load-Use (RAW) | ID_EX.MemRead ∧ ID_EX.Rd == IF_ID.Rs1 | Stall 1 cycle, insert bubble |
| `ADD x1` → `ADD x3,x1,x2` | EX hazard (RAW) | EX_MEM.RegWrite ∧ EX_MEM.Rd == ID_EX.Rs1 | Forward EX/MEM → EX |
| `SUB x1` 2 cycles ago → use | MEM hazard (RAW) | MEM_WB.RegWrite ∧ MEM_WB.Rd == ID_EX.Rs1 | Forward MEM/WB → EX |
| Branch taken | Control | branch_taken in EX stage | Flush IF + ID (2 cycles) |

---

## 🧩 Branch Predictors — How They Work

### 1. Static Not-Taken
Predicts every branch as **not taken**. Simple, cheap, terrible for loops.
```
predict(pc) → always false
update(pc, taken) → if taken: misprediction++
```

### 2. 2-Bit Saturating Counter
A 4-state FSM per PC entry. Requires two consecutive wrong outcomes to flip prediction.
```
States: SNT(00) ──taken──▶ WNT(01) ──taken──▶ WT(10) ──taken──▶ ST(11)
                ◀─not──           ◀─not──           ◀─not──
Predict TAKEN if state ≥ 10 (WT or ST)
```

### 3. GShare
XORs the PC with a **Global History Register** (GHR) to index the PHT. Exploits *branch correlation* — the outcome of branch B often depends on earlier branches.
```
index = PC[k+1:2] XOR GHR[k-1:0]
GHR  = shift-left, insert new outcome at bit 0
```

### 4. Tournament
Combines a local predictor (2-bit indexed by PC) and global predictor (GShare), with a **chooser** that learns which is more accurate per branch.
```
chooser[pc] ∈ {0,1} → prefer local
              {2,3} → prefer global
Update: if local_correct ∧ ¬global_correct → prefer local more
```

---

## 💾 Cache Organization

```
Address decomposition (32-bit):
┌─────────────────┬─────────────┬──────────────────┐
│      TAG        │    INDEX    │   BLOCK OFFSET   │
│  32-idx-off bits│  idx bits   │    off bits      │
└─────────────────┴─────────────┴──────────────────┘

Where:
  offset_bits = log₂(block_size)        e.g. 64B blocks → 6 bits
  index_bits  = log₂(num_sets)          e.g. 32 sets    → 5 bits
  tag_bits    = 32 - offset - index     = 21 bits

num_sets = size / (block_size × associativity)
```

| Config | num_sets | Conflict misses | Capacity misses |
|--------|----------|----------------|-----------------|
| Direct-mapped (1-way) | High | High — two addresses can fight over the same set | Low |
| 4-way set-assoc | Medium | Lower — 4 candidates per set | Medium |
| Fully-associative | 1 | Zero — any block goes anywhere | Lowest |

---

## 📡 REST API Reference

The simulator exposes a simple REST API. Every response is JSON.

| Method | Endpoint | Body | Returns |
|--------|---------|------|---------|
| `GET` | `/` | — | Frontend HTML |
| `POST` | `/api/load` | `{"builtin":"fibonacci"}` or `{"hex":"..."}` | Pipeline state |
| `POST` | `/api/config` | `{"predictor":"gshare","forwarding":true}` | Config echo |
| `POST` | `/api/step` | `{"cycles":1}` | Pipeline state after N cycles |
| `POST` | `/api/run` | `{"max_cycles":100000}` | Final pipeline state |
| `POST` | `/api/reset` | — | Fresh pipeline state |
| `GET` | `/api/state` | — | Current pipeline state |
| `GET` | `/api/programs` | — | List of built-in programs |

### Example State Response

```json
{
  "cycle": 42,
  "halted": false,
  "stall": false,
  "flush": true,
  "stages": [
    { "stage": "IF",  "asm": "LW x1, 0(x4)",     "pc_hex": "0x00000050", "bubble": false },
    { "stage": "ID",  "asm": "ADD x3, x1, x2",    "pc_hex": "0x0000004C", "bubble": false },
    { "stage": "EX",  "asm": "BEQ x5, x6, +8",    "pc_hex": "0x00000048", "forward_a": "MEM_WB" },
    { "stage": "MEM", "asm": "SW x7, 4(x8)",       "pc_hex": "0x00000044", "cache_hit": true },
    { "stage": "WB",  "asm": "ADDI x10, x0, 55",  "pc_hex": "0x00000040", "wb_active": true }
  ],
  "stats": {
    "cycles": 42, "instructions": 35, "ipc": 0.833,
    "stall_cycles": 3, "flush_cycles": 4,
    "branch_count": 8, "mispredictions": 2, "mispred_rate": 0.25,
    "cache_accesses": 42, "cache_hits": 38, "miss_rate": 0.095
  }
}
```

---

## 📁 Project Structure

```
coa-prj/
│
├── 📄 CMakeLists.txt              Root build config (vcpkg + static linking)
├── 📄 vcpkg.json                  Dependencies: cpp-httplib, nlohmann-json
├── 📄 main.cpp                    Entry point: start server + open browser
├── 📄 setup.ps1                   One-shot Windows build script
│
├── 📂 simulator/                  C++ simulator core (compiled as static library)
│   ├── include/
│   │   ├── isa/
│   │   │   ├── rv32i_types.h      Instruction formats, opcodes, pipeline regs
│   │   │   └── decoder.h          RV32I instruction decoder interface
│   │   ├── core/
│   │   │   ├── pipeline.h         5-stage pipeline + visualization state
│   │   │   ├── hazard_unit.h      RAW hazard detection logic
│   │   │   ├── forwarding_unit.h  EX/MEM→EX data bypassing
│   │   │   ├── register_file.h    32 × 32-bit registers (x0 hardwired to 0)
│   │   │   └── memory.h           Flat 16MB byte-addressable memory
│   │   ├── branch/
│   │   │   ├── predictor.h        Abstract base class
│   │   │   ├── static_pred.h      Always-taken / always-not-taken
│   │   │   ├── two_bit_pred.h     2-bit saturating counter + PHT
│   │   │   ├── gshare_pred.h      GShare (PC XOR GHR indexing)
│   │   │   └── tournament_pred.h  Local + global + chooser
│   │   ├── cache/
│   │   │   └── cache.h            Configurable cache (assoc, size, policy)
│   │   └── stats/
│   │       └── perf_counters.h    IPC, miss rate, misprediction rate
│   └── src/                       Implementations (.cpp files)
│
├── 📂 server/                     HTTP API server
│   ├── include/api_server.h       REST endpoint definitions
│   └── src/api_server.cpp         Route handlers + built-in programs
│
├── 📂 frontend/
│   └── index.html                 Complete single-file web app (self-contained)
│                                  Dark glassmorphism UI, in-browser assembler,
│                                  pipeline/cache/predictor visualizations
│
└── 📂 tools/
    └── embed_frontend.py          Converts index.html → C++ const char* header
                                   Run automatically during CMake build
```

---

## ⌨️ Built-in Programs

| Program | What it does | Why it's interesting |
|---------|-------------|---------------------|
| **Fibonacci(10)** | Iteratively computes F(10)=55 | Loop back-branches stress-test predictors |
| **Bubble Sort** | Sorts 8 integers | Nested loops, data-dependent branches |
| **Matrix Multiply** | 2×2 integer matrix A×B=C | Dense memory access, cache sensitivity |
| **1..10 Sum** | Σ(1..10) = 55 | Simple loop, good for learning pipeline basics |
| **Memory Loop** | Repeated store/load | Highlights load-use stalls and cache cold-start |

Plus the **in-browser assembler** — write your own programs directly in the UI.

---

## ⌨️ Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `Space` or `→` | Step 1 cycle |
| `R` | Reset simulator |

---

## 🎓 Concepts Covered

This simulator is designed to make every major computer architecture topic tangible:

| Topic | Where to see it |
|-------|----------------|
| **Instruction Encoding** | Look at the hex dump of any loaded program; each line is one instruction |
| **Pipeline Stages** | The 5-stage diagram — watch instructions flow left to right |
| **Data Hazards (RAW)** | Load a program with `LW` followed by immediate use; observe the stall bubble |
| **Forwarding / Bypassing** | Toggle "Data Forwarding" OFF and watch stall cycles skyrocket |
| **Control Hazards** | Use any branch program; switch to Static Not-Taken and watch flush cycles |
| **2-Bit Predictor FSM** | Run Fibonacci — watch the PHT converge to ST (Strongly Taken) for the loop |
| **Branch Correlation** | Compare GShare vs 2-Bit on programs with correlated branches |
| **Cache Cold Start** | Reset and run — first N accesses are always misses (compulsory misses) |
| **Conflict Misses** | Switch from 4-way to Direct-Mapped on the matrix multiply; miss rate rises |
| **IPC vs Ideal** | Compare IPC with/without forwarding, with/without good branch prediction |

---

## 🛠️ Building from Source — Detailed

### Prerequisites

| Tool | Where to get it | Notes |
|------|----------------|-------|
| Visual Studio 2022 | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) | Select "Desktop dev with C++" workload |
| CMake 4.3+ | Included with VS | Or from [cmake.org](https://cmake.org/) |
| Python 3 | [python.org](https://python.org) | For frontend embedding script |
| Git | [git-scm.com](https://git-scm.com) | For cloning vcpkg |

### Step-by-Step (Manual)

```powershell
# 1. Clone and bootstrap vcpkg (one-time)
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat -disableMetrics

# 2. Configure (downloads & builds cpp-httplib and nlohmann-json)
cmake -B build -S . `
    -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static `
    -DCMAKE_BUILD_TYPE=Release

# 3. Build
cmake --build build --config Release

# 4. Run
.\bin\Release\RVSim.exe
```

### What `setup.ps1` does

The included `setup.ps1` automates all of the above. It:
1. Clones vcpkg to `C:\vcpkg` if not present
2. Runs `bootstrap-vcpkg.bat`
3. Finds the VS-bundled CMake automatically
4. Runs CMake configure (vcpkg downloads + compiles dependencies)
5. Runs the build
6. Offers to launch the exe

---

## 🔒 Why Standalone?

Most simulators require a Python environment, Node.js, or a specific runtime. RVSim packages everything into a single Windows executable using:

- **cpp-httplib** — header-only HTTP server compiled directly into the binary
- **Embedded frontend** — `embed_frontend.py` converts `index.html` into a C++ raw string literal at build time
- **Static MSVC runtime** — `/MT` flag links `VCRUNTIME140.dll` and `MSVCRT.dll` statically, so no redistributable is needed
- **nlohmann/json** — header-only, no `.dll` needed

Result: the `.exe` is the entire application. It can be emailed, put on a USB drive, or shared on a course page — and it will just run.

---

## 📊 Performance Notes

The simulator is deliberately **not optimized for speed** — it's optimized for **clarity and correctness**. Every pipeline register is a full C++ struct with named fields. Every stage is a separate function. This makes the code match the textbook diagrams.

For reference, on a modern laptop:
- Simple programs (~50 instructions): finishes in **< 1ms**
- Loop-heavy programs (~10,000 instructions): finishes in **< 50ms**
- The web UI refreshes instantly after each step

---

## 🤝 Acknowledgements

- **RISC-V Foundation** — for the open ISA specification
- **cpp-httplib** by yhirose — the header-only HTTP server that makes standalone distribution possible
- **nlohmann/json** — the single-header JSON library
- **Patterson & Hennessy** — *Computer Organization and Design RISC-V Edition* — the textbook this simulator is based on

---

<div align="center">

**Built as a Computer Architecture course project**

*Made with ❤️ and a lot of pipeline diagrams*

</div>
