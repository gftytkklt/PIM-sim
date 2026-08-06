#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <queue>
#include "simulator/ModuleBase.h"

namespace hw_config {

// Crossbar parameters
constexpr int xbar_compute_latency = 40;
constexpr int xbar_switch_latency = 10;
constexpr int xbar_num = 4;
constexpr int xbar_wl = 1152;
constexpr int xbar_bl = 256;

// SIMD parameters
constexpr int simd_quant_latency = 1;
constexpr int simd_activate_latency = 1;
constexpr int simd_pooling_latency = 1;
constexpr int simd_num = 16;

// L1C parameters
constexpr int l1c_size_kb = 48;
constexpr int l1c_bank = 4;
constexpr int l1c_sram_line_bytes = 16;
constexpr int l1c_sram_num = 12;
constexpr int l1c_sram_depth = (l1c_size_kb * 1024 / l1c_sram_num / l1c_sram_line_bytes);

} // namespace hw_config

// Legacy #define aliases for backward compatibility
#define XBAR_COMPUTE_LATENCY hw_config::xbar_compute_latency
#define XBAR_SWITCH_LATENCY hw_config::xbar_switch_latency
#define XBAR_NUM hw_config::xbar_num
#define XBAR_WL hw_config::xbar_wl
#define XBAR_BL hw_config::xbar_bl
#define SIMD_QUANT_LATENCY hw_config::simd_quant_latency
#define SIMD_ACTIVATE_LATENCY hw_config::simd_activate_latency
#define SIMD_POOLING_LATENCY hw_config::simd_pooling_latency
#define SIMD_NUM hw_config::simd_num
#define L1C_SIZE_KB hw_config::l1c_size_kb
#define L1C_BANK hw_config::l1c_bank
#define L1C_SRAM_LINE_BYTES hw_config::l1c_sram_line_bytes
#define L1C_SRAM_NUM hw_config::l1c_sram_num
#define L1C_SRAM_DEPTH hw_config::l1c_sram_depth

#endif