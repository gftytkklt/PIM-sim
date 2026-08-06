#include "simulator/tile2_0/config.h"
#include <gtest/gtest.h>

TEST(ConfigTest, ConstexprValuesMatchLegacyMacros) {
    // Crossbar
    EXPECT_EQ(hw_config::xbar_compute_latency, 40);
    EXPECT_EQ(hw_config::xbar_switch_latency, 10);
    EXPECT_EQ(hw_config::xbar_num, 4);
    EXPECT_EQ(hw_config::xbar_wl, 1152);
    EXPECT_EQ(hw_config::xbar_bl, 256);

    // SIMD
    EXPECT_EQ(hw_config::simd_quant_latency, 1);
    EXPECT_EQ(hw_config::simd_activate_latency, 1);
    EXPECT_EQ(hw_config::simd_pooling_latency, 1);
    EXPECT_EQ(hw_config::simd_num, 16);

    // L1C
    EXPECT_EQ(hw_config::l1c_size_kb, 48);
    EXPECT_EQ(hw_config::l1c_bank, 4);
    EXPECT_EQ(hw_config::l1c_sram_line_bytes, 16);
    EXPECT_EQ(hw_config::l1c_sram_num, 12);
}

TEST(ConfigTest, LegacyMacrosEqualConstexpr) {
    EXPECT_EQ(XBAR_COMPUTE_LATENCY, hw_config::xbar_compute_latency);
    EXPECT_EQ(XBAR_SWITCH_LATENCY, hw_config::xbar_switch_latency);
    EXPECT_EQ(SIMD_NUM, hw_config::simd_num);
    EXPECT_EQ(L1C_BANK, hw_config::l1c_bank);
    EXPECT_EQ(L1C_SRAM_DEPTH, hw_config::l1c_sram_depth);
}