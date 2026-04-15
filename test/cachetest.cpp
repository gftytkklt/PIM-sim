// test_cache.cpp
#include "simulator/modules/Cache.h"
#include "simulator/ModuleBase.h"
#include "simulator/Simulator.h"
#include <gtest/gtest.h>
#include <iostream>


// 信号驱动模块（用于生成测试激励）
class SignalDriver : public ModuleBase<SignalDriver> {
public:
    SignalDriver(const std::string& id) : ModuleBase<SignalDriver>(id) {
        // 添加输出信号
        add_signal(Signal("clk_out", Signal::Direction::OUTPUT));
        add_signal(Signal("addr_out", Signal::Direction::OUTPUT));
        add_signal(Signal("data_out", Signal::Direction::OUTPUT));
        add_signal(Signal("req_out", Signal::Direction::OUTPUT));
        add_signal(Signal("we_out", Signal::Direction::OUTPUT));
        
        // 当前周期计数
        current_cycle_ = 0;
    }
    
    void register_processes() override {
        // 注册时钟生成进程
        register_process("clock_generator",
            [this]() { return true; },  // 每个周期都触发
            [this]() { return true; },
            [this]() { 
                // 生成时钟信号
                bool clock_value = (current_cycle_ % 2 == 0);
                submit_signal_value("clk_out", clock_value, 1);
                
                // 生成测试激励
                generate_test_stimuli();
                
                return true; 
            },
            [this]() { return true; },
            1
        );
    }
    
    std::unordered_map<std::string, uint64_t> get_module_specific_stats() const override {
        return {{"cycles_generated", current_cycle_}};
    }
    
    // 设置测试激励
    void set_test_stimuli(uint64_t addr, uint32_t data, bool write_enable) {
        test_addr_ = addr;
        test_data_ = data;
        test_we_ = write_enable;
        test_active_ = true;
    }
    
    // 清除测试激励
    void clear_stimuli() {
        test_active_ = false;
    }
    
private:
    uint64_t current_cycle_{0};
    bool test_active_{false};
    uint64_t test_addr_{0};
    uint32_t test_data_{0};
    bool test_we_{false};
    
    void generate_test_stimuli() {
        if (test_active_) {
            submit_signal_value("addr_out", test_addr_, 1);
            submit_signal_value("data_out", test_data_, 1);
            submit_signal_value("req_out", true, 1);
            submit_signal_value("we_out", test_we_, 1);
        } else {
            // 无请求时
            submit_signal_value("req_out", false, 1);
        }
        
        current_cycle_++;
    }
};

// Cache模拟器集成测试夹具
class CacheSimulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建模拟器实例
        simulator = std::make_shared<CycleAccurateSimulator>(100);
        
        // 注册Cache模块
        cache = simulator->register_module<Cache>("test_cache", 2);
        
        // 注册一个简单的驱动模块，用于生成测试激励
        driver = simulator->register_module<SignalDriver>("driver", 3);
        
        // 连接驱动模块到Cache
        simulator->connect_modules("driver", "clk_out", "test_cache", "clk");
        simulator->connect_modules("driver", "addr_out", "test_cache", "addr_in");
        simulator->connect_modules("driver", "data_out", "test_cache", "data_in");
        simulator->connect_modules("driver", "req_out", "test_cache", "req_in");
        simulator->connect_modules("driver", "we_out", "test_cache", "we_in");
    }
    
    void TearDown() override {
        simulator.reset();
    }
    
    std::shared_ptr<CycleAccurateSimulator> simulator;
    std::shared_ptr<Cache> cache;
    std::shared_ptr<SignalDriver> driver;
};

// 测试1: 通过模拟器进行缓存读测试
TEST_F(CacheSimulatorTest, CacheReadThroughSimulator) {
    std::cout << "开始测试: CacheReadThroughSimulator" << std::endl;
    
    // 1. 通过驱动模块设置读请求
    driver->set_test_stimuli(0x1000, 0, false);  // 地址0x1000，读请求
    
    // 2. 运行模拟器10个周期
    simulator->run();
    
    // 3. 获取Cache模块统计
    std::unordered_map<std::string, uint64_t> stats;
    cache->get_performance_stats(stats);
    
    std::cout << "缓存统计 - 总访问: " << stats["total_accesses"] 
              << ", 读命中: " << stats["read_hits"]
              << ", 读未命中: " << stats["read_misses"] << std::endl;
    
    // 断言：应该有访问发生
    EXPECT_GT(stats["total_accesses"], 0u);
    
    std::cout << "测试完成: CacheReadThroughSimulator" << std::endl;
}

// 测试2: 通过模拟器进行缓存写测试
TEST_F(CacheSimulatorTest, CacheWriteThroughSimulator) {
    std::cout << "开始测试: CacheWriteThroughSimulator" << std::endl;
    
    // 设置写请求
    driver->set_test_stimuli(0x2000, 0xDEADBEEF, true);
    
    // 运行模拟器
    simulator->run();
    
    // 获取统计
    std::unordered_map<std::string, uint64_t> stats;
    cache->get_performance_stats(stats);
    
    std::cout << "缓存统计 - 总访问: " << stats["total_accesses"] 
              << ", 写命中: " << stats["write_hits"]
              << ", 写未命中: " << stats["write_misses"] << std::endl;
    
    EXPECT_GT(stats["total_accesses"], 0u);
    
    std::cout << "测试完成: CacheWriteThroughSimulator" << std::endl;
}