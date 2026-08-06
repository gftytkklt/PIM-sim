# AGENTS.md — PIM-sim (PIMapping)

## Project identity
- Academic research: PIM architecture NN mapping + performance evaluation framework.
- CMake project name: `PIMapping`. Repo name: `PIM-sim`.
- C++20 core + Python (pybind11) bindings. Linux-targeted (Ubuntu 24 LTS).
- Docs in Chinese. README.md may be outdated — trust CMakeLists.txt and build scripts over prose.

## Build & test

```bash
source env.sh          # Load modules: gcc/11.4, cmake/3.28, python/3.11, boost/1.84, gtest/1.14, pybind11/2.13
./build.sh             # cmake + make → produces pimapping.<ext>.so (pybind11 module)
./test.sh              # Build + run all gtest tests
./test.sh bankingtest  # Run a single test by name (test names match .cpp files in test/)
```

- `build.sh` does a **force clean** (`rm -rf build/`) every time.
- C++ lib is `libPIMapping.a` (static). Python module is `PIModule` → output name `pimapping`.
- `libmain.so` in repo root is a **stale pre-built artifact** tracked in git (see `.gitignore`). The current build produces `pimapping.<suffix>.so`, not `libmain.so`.
- Tests require `pthread` (linked in test/CMakeLists.txt).

## Architecture

```
                  ┌─────────────────────────────────────┐
Python            │  perf.py  torch2onnx.py              │
                   │  onnx_analysis.py  MappingInfo.py    │
                   │  MNSIM/ (HW models: latency/energy)  │
                  └──────────────┬──────────────────────┘
                     pybind11    │
                  ┌──────────────▼──────────────────────┐
C++ (libPIMapping) │  Analyzer → Graph hierarchy:        │
                   │  CGraph → TGraph → HGraph → DGraph  │
                   │  Mapper → Scheduler → Simulator      │
                   │  strategy/ (PIMAPPING/SPATEM/HITM/MNSIM) │
                  └─────────────────────────────────────┘
```

- **Entry point**: `python3 perf.py` runs the full pipeline (ONNX → graph analysis → Booksim NoC → results).
- **C++ entry**: `main.cpp` exports `analyze()` and `test()` to Python as `pimapping` module.
- **Graph hierarchy**: C-VDFG (crossbar-level) → T-VDFG (tile-level) → H-VDFG → D-VDFG. Built on Boost Graph Library.
- **MNSIM**: Python library in `MNSIM/` for hardware modeling (latency, power, area, energy). Called by `MappingInfo.py`.
- **Booksim**: External pre-compiled binary (`booksim`) for NoC simulation. Called via `subprocess` in `MappingInfo.py`.

## Key files

| File | Role |
|---|---|
| `SimConfig.ini` | Hardware parameters (Xbar size, bitwidth, etc.) |
| `perf.py` | Main performance analysis script (1778 lines) |
| `onnx_analysis.py` | ONNX parsing → NNkernel extraction |
| `MappingInfo.py` | Tile latency, Booksim invocation, bandwidth modeling |
| `include/graph.h` | Core graph class definitions (CGraph, TGraph, HGraph, DGraph) |
| `include/analyzer.h` | Top-level Analyzer orchestrating the pipeline |
| `include/mapper.h` | Physical tile mapping (shared_ptr injectable) |
| `include/scheduler.h` | Path scheduling + congestion (shared_ptr injectable) |
| `include/logger.h` | C++ structured logging (PIM_INFO/PIM_WARN/PIM_ERROR) |
| `include/errors.h` | Exception hierarchy (PIMException/GraphError/...) |
| `src/cgraph.cpp` | CGraph: C-VDFG (crossbar-level) implementation |
| `src/tgraph.cpp` | TGraph: T-VDFG (tile-level) with 5 create_tnodes_* strategies |
| `src/hgraph.cpp` | HGraph: HCG (hardware connection graph) with zigzag/greedy/SPATEM |
| `src/dgraph.cpp` | DGraph: DHCG (dynamic) with BCE/XY routing |
| `src/graph_io.cpp` | operator<< overloads for graph node/edge types |

## Conventions & gotchas

- **No linting, no formatting config, no CI**. No `.clang-format`, `.pre-commit`, or GitHub Actions.
- **`.gitignore` is whitelist-style**: ignores everything (`*`), then re-includes specific extensions (`.cpp`, `.h`, `.hpp`, `.sh`, `.py`, `CMakeLists.txt`). Adding new file types requires updating `.gitignore`.
- **`source env.sh` is required** before build/test on the team's server. It uses `module load`. On other machines, install dependencies manually.
- **`perf.py` caches results via pickle** — first run is slow (Booksim simulation), subsequent runs reuse cache.
- **All commands must run from repo root** (relative paths throughout).
- **`models/` directory** contains ONNX files. Default model is `resnet18.onnx`.
- The `onnx_analysis.py` → `load_kernel()` path expects ONNX models with `.onnx` extension.
- Python module `pimapping` must be importable — built `.so` must be in the working directory or `PYTHONPATH`.