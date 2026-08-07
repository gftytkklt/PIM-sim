# PIM-sim Development Skill

## Key reference files
- `TASKS.md` — Complete task list (26/52 completed), always check before starting new work
- `AGENTS.md` — Build instructions, architecture, conventions
- `README.md` — Full methodology, engineering structure, quick start

## Project context
- Academic research: PIM architecture NN mapping + performance evaluation framework.
- CMake project `PIMapping`, repo `PIM-sim`. C++20 + Python (pybind11). Linux (Ubuntu 24 LTS).
- Trust `AGENTS.md`, `CMakeLists.txt`, and build scripts over `README.md`.

## Build & test commands

```bash
source /home/gftyt/PIMapping/pimapping/bin/activate    # Virtual environment
PATH=/usr/bin:/usr/local/bin:$PATH                       # System cmake/ctest

cd /home/gftyt/PIMapping
rm -rf build && mkdir build && cd build                  # Force clean
/usr/bin/cmake .. -DCMAKE_BUILD_TYPE=Release             # Configure
make -j$(nproc)                                          # Build
/usr/bin/ctest --test-dir . -V                           # Run tests

# AddressSanitizer
/usr/bin/cmake .. -DENABLE_ASAN=ON && make -j$(nproc) && /usr/bin/ctest --test-dir .

# Quick regression (demo models)
/home/gftyt/PIMapping/pimapping/bin/python3 result_develop/scripts/compare.py

# Full regression (all models)
/home/gftyt/PIMapping/pimapping/bin/python3 result_develop/scripts/compare.py --full
```

## Architecture

```
Python:    perf.py → analysis.py / plotting.py  →  onnx_analysis.py
           MappingInfo.py (Booksim+MNSIM)  →  config_validator.py
pybind11:  main.cpp exports pimapping module (DepInfo, NNkernel, HWInfo, ProcessEvent, etc.)
C++:       Analyzer → CGraph → TGraph → HGraph → DGraph
           Mapper (shared_ptr injectable)  Scheduler (shared_ptr injectable)
           strategy/ (CStrategy/TStrategy/HStrategy/DStrategy per graph level)
           simulator/ (ISimulatable → ModuleBase → Crossbar/SIMD/L1C/TaskScheduler)
```

## Code conventions

- `include/graph.h` is the core header. Graph implementations split into `src/cgraph.cpp`, `tgraph.cpp`, `hgraph.cpp`, `dgraph.cpp`, `graph_io.cpp`.
- `ModuleBase` is non-template (CRTP removed). Uses `enable_shared_from_this<ISimulatable>`.
- Signal access: `get_signal_as<T>()` returns `std::optional<T>` for safe type-checked access. Signal system: framework defines `Signal` struct (name/direction/value_type), users declare their own signals via `add_signal`; `register_module` auto-collects declarations into `signal_registry_`; `connect_modules` auto-validates signal existence, direction (OUTPUT→INPUT), and value type. Signal names are user-defined strings, not framework enums.
- Core factory: `create_core_modules()` in `include/simulator/tile2_0/core_factory.h` eliminates repeated module registration.
- Hardware config: `hw_config` namespace with `constexpr int` values. Legacy `#define` aliases kept for backward compatibility.
- `src/CMakeLists.txt` uses `GLOB_RECURSE` — new .cpp files auto-discovered.
- `test/CMakeLists.txt` uses `GLOB_RECURSE` — each .cpp becomes a test executable.
- Logger: `PIM_INFO(msg)`, `PIM_WARN(msg)`, `PIM_ERROR(msg)` macros. Output to `runs/cpp_analysis.log`.
- Errors: `pim::PIMException`, `pim::GraphError`, `pim::MappingError`, `pim::SchedulingError`, `pim::ConfigError`.

## Workflow protocol

**Before executing any task**, assess complexity and suggest the approach:

1. Classify the task using the matrix below
2. Use the `question` tool to present your recommendation and ask for confirmation
3. Wait for user to switch mode/model before starting work
4. If the user says "just do it", proceed with current settings

### Task complexity matrix

| Type | Examples | Thinking | Model | Reason |
|------|----------|----------|-------|--------|
| **Trivial** | Fix typo, add comment, update doc | Default | Flash | Single file, no logic change |
| **Simple** | Rename function, add one test, fix single warning | Default | Flash | Mechanical change, low risk |
| **Moderate** | Add new class method, refactor one file, fix bug with test | High | Default | Multi-file but bounded scope |
| **Complex** | Split large file, decouple component, multi-file refactor | High | Default | Architectural change, needs verification |
| **Heavy** | CRTP removal, strategy pattern refactor, simulator overhaul | Max | Pro | Cross-cutting, affects many files, high risk |
| **Unknown** | Vague instruction, unfamiliar code area | High | Default | Needs exploration before classification |

### Decision flow
```
User instruction
  → Classify task type using matrix
  → Suggest: mode + model + brief reasoning
  → Wait for user confirmation
  → Execute
  → Verify with ctest + compare.py
```

## Refactoring workflow

When making structural changes to C++ code:

1. **Before changes**: Run `collect_reference.py` to capture baseline data
2. **During changes**: 
   - Each change should be independently testable
   - Build + ctest after each change
   - Run `compare.py` to verify perf_analysis output unchanged
3. **After changes**: 
   - Full ctest with ASan: `cmake -DENABLE_ASAN=ON .. && make && ctest`
   - Quick regression: `compare.py`
   - Full regression: `compare.py --full` (slow, only for final verification)

## Regression test infrastructure

```
result_develop/
├── scripts/
│   ├── collect_reference.py    # Save baseline (ctest + perf_analysis)
│   ├── compare.py              # Compare current vs baseline
│   └── regression_test.sh      # One-shot build + test
└── reference/                  # Generated baseline data (not tracked)
```

## Common patterns

### Adding a new graph class method
1. Declare in `include/graph.h` under the appropriate class
2. Implement in the corresponding `src/cgraph.cpp`/`tgraph.cpp`/`hgraph.cpp`/`dgraph.cpp`
3. If it's strategy-specific, call from the strategy class in `src/strategy/`

### Adding a new simulator test
1. Create `test/new_test.cpp` with `TEST_F` fixtures
2. `test/CMakeLists.txt` auto-discovers via `GLOB_RECURSE`
3. Run with `./test.sh new_test` or `ctest -R new_test`

### Adding a new module type
1. Create `include/simulator/tile2_0/NewModule.h` extending `ModuleBase`
2. Implement `register_processes()` and `register_message_handlers()`
3. Create `src/simulator/tile2_0/NewModule.cpp`
4. Use `create_core_modules()` or `register_module<NewModule>()` in simulator Init()

### Dependency injection
- Mapper: `shared_ptr<Mapper>` injected into `HGraph` via `mapper_` member
- Scheduler: `shared_ptr<Scheduler>` injected into `DGraph` via `scheduler_` member
- Strategy: `shared_ptr<StrategyBase<GraphType>>` injected into each graph via constructor

## Current status & remaining work

See `TASKS.md` for the full task list. Current progress: 26/52 completed.

### Completed refactoring (26 items)
- **Code organization**: graph.cpp split, strategy pattern, Mapper/Scheduler decoupling, .gitignore, CMake
- **Interface**: logging (PIM_INFO), error handling (PIMException), naming conventions (snake_case)
- **Simulator optimization**: CRTP removal, hw_config constexpr, core_factory, any_cast safety, completion detection
- **Integration**: cache key fix, Booksim timeout, Perf.py split, pybind11 ProcessEvent, config_validator.py
- **Engineering**: CI, clang-format, ASan, Doxygen, 8 test files (25+ cases)

### Remaining work — see `TASKS.md` for details
- **Algorithm improvements**: DHCG segmentation, Intensity Map tuning, spectral embedding, weight replication, more DNN ops
- **Simulator features**: mapper-simulator integration, backpressure, multi-core transactions
- **Simulator architecture**: ISimulator interface, message types, config-driven module graph
- **Simulator design gaps** (from paper Ch.5 analysis): event state update model (signal-occupancy linkage), event counter automation, combinational logic dependency detection, feedback timing dependency data queue, generic multi-core transaction primitives, NoC communication modeling interface, storage granularity consistency
- **Bug fixes**: get_adjacent_edges, cycle detection, channel intersection, BCE normalization, process state machine
- **Testing**: Mapper/Scheduler unit tests, waveform trace output

### Known quirks
- `perf.py` has uncommitted pre-existing changes (stashed)
- `test/tilingtest.cpp` hits max_cycles (300000) before completion
- Boost `-Wmaybe-uninitialized` false positives (13 warnings from template internals)
- `mappingalexnet` is the only test without simulator (passes ASan with 0 leaks)
- **ASan timing anomaly**: under ASan builds, simulator tests run to max_cycles instead of terminating early (TaskScheduler processes never complete). This is a pre-existing phenomenon (present in original master). Use normal (non-ASan) builds to verify timing correctness; use ASan only for memory/leak detection. To run ASan tests, use `cmake -DENABLE_ASAN=ON ..`.
- **Simulator decoupling**: `evaluate()` returns `std::vector<SimulatorEvent>` (defined in `SimulatorEvent.h`); modules accumulate events in `pending_events_` instead of using callbacks into simulator private queues. Signal system uses string names + runtime registry validation (see Code conventions).