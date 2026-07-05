#ifndef CONFIG_H
#define CONFIG_H

#include "simulator/ModuleBase.h"
#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <queue>

#define XBAR_COMPUTE_LATENCY 40
#define XBAR_SWITCH_LATENCY 10
#define XBAR_NUM 4
#define XBAR_WL 1152
#define XBAR_BL 256

#define SIMD_QUANT_LATENCY 1
#define SIMD_ACTIVATE_LATENCY 1
#define SIMD_POOLING_LATENCY 1
#define SIMD_NUM 16

#define L1C_SIZE_KB 48
#define L1C_BANK 4
#define L1C_SRAM_LINE_BYTES 16
#define L1C_SRAM_NUM 12
#define L1C_SRAM_DEPTH (L1C_SIZE_KB * 1024 / L1C_SRAM_NUM / L1C_SRAM_LINE_BYTES)

#endif