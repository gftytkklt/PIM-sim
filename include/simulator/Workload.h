#ifndef PIMAPPING_WORKLOAD_H
#define PIMAPPING_WORKLOAD_H

#include <cstdint>
#include <string>
#include "analyzer.h"

namespace pim {
// Portable task-level timing model. Durations must be supplied by the caller;
// this interface does not infer Tile2.0 register values or hardware calibration.
std::string export_workload(const std::vector<NNkernel>& kernels, const HWInfo& hw,
                            const OptInfo& opt, const std::string& timing_json);
std::string simulate_workload(const std::string& workload_json, uint64_t max_cycles);
}  // namespace pim
#endif
