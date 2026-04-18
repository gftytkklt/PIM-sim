#ifndef CACHE_H
#define CACHE_H

#include "simulator/ModuleBase.h"
#include <cstdint>
#include <vector>
#include <map>
#include <queue>
#include <iostream>

/**
 * 缓存模块
 * 实现一个简单的组相联缓存
 */
class Cache : public ModuleBase<Cache> {
public:
    // 缓存行状态
    enum class CacheLineState {
        INVALID,
        SHARED,
        MODIFIED
    };
    
    // 缓存行结构
    struct CacheLine {
        uint64_t tag;
        uint32_t data[4];  // 16字节行大小
        CacheLineState state;
        uint64_t lru_counter;
        bool valid;
        
        CacheLine() : tag(0), state(CacheLineState::INVALID), lru_counter(0), valid(false) {
            for (int i = 0; i < 4; i++) data[i] = 0;
        }
    };
    
    // 缓存集结构
    struct CacheSet {
        std::vector<CacheLine> ways;
        CacheSet(int associativity) : ways(associativity) {}
    };
    
    // 请求类型
    enum class RequestType {
        READ,
        WRITE,
        INVALIDATE
    };
    
    // 未完成请求
    struct PendingRequest {
        RequestType type;
        uint64_t addr;
        uint32_t data;
        uint64_t issue_cycle;
        std::weak_ptr<ISimulatable> requester;
        
        PendingRequest(RequestType t, uint64_t a, uint32_t d, uint64_t c, std::weak_ptr<ISimulatable> r)
            : type(t), addr(a), data(d), issue_cycle(c), requester(r) {}
    };
    
private:
    // 缓存参数
    const int cache_size_;        // 总大小（字节）
    const int line_size_;         // 行大小（字节）
    const int associativity_;     // 相联度
    const int num_sets_;          // 组数
    
    // 缓存存储
    std::vector<CacheSet> sets_;
    
    // 未完成请求队列
    std::queue<PendingRequest> pending_requests_;
    
    // 内存延迟
    const uint64_t hit_latency_{1};
    const uint64_t miss_latency_{4};
    
    // 统计信息
    uint64_t read_hits_{0};
    uint64_t read_misses_{0};
    uint64_t write_hits_{0};
    uint64_t write_misses_{0};
    uint64_t evictions_{0};
    uint64_t total_accesses_{0};
    
    // LRU计数器
    uint64_t global_lru_counter_{0};
    
public:
    Cache(const std::string& id, int size_kb = 32, int assoc = 4, int line_bytes = 16)
        : ModuleBase<Cache>(id)
        , cache_size_(size_kb * 1024)
        , line_size_(line_bytes)
        , associativity_(assoc)
        , num_sets_((size_kb * 1024) / (line_bytes * assoc))
        , sets_(num_sets_, CacheSet(assoc))
    {
        // 添加信号
        add_signal(Signal("clk", Signal::Direction::INPUT));
        add_signal(Signal("rst", Signal::Direction::INPUT));
        add_signal(Signal("addr_in", Signal::Direction::INPUT));
        add_signal(Signal("data_in", Signal::Direction::INPUT));
        add_signal(Signal("req_in", Signal::Direction::INPUT));
        add_signal(Signal("we_in", Signal::Direction::INPUT));
        
        add_signal(Signal("data_out", Signal::Direction::OUTPUT));
        add_signal(Signal("data_valid", Signal::Direction::OUTPUT, false));
        add_signal(Signal("ready", Signal::Direction::OUTPUT));
        
        add_signal(Signal("mem_addr", Signal::Direction::INTERNAL));
        add_signal(Signal("mem_data", Signal::Direction::INTERNAL));
        add_signal(Signal("mem_req", Signal::Direction::INTERNAL));
        add_signal(Signal("mem_we", Signal::Direction::INTERNAL));
        add_signal(Signal("mem_valid", Signal::Direction::INTERNAL));
        
        // 绑定信号到进程
        bind_signal_to_process("clk", "cache_access");
        bind_signal_to_process("req_in", "cache_access");
        bind_signal_to_process("addr_in", "cache_access");
        bind_signal_to_process("data_in", "cache_access");
        bind_signal_to_process("we_in", "cache_access");
    }
    
    void register_processes() override {
        // 注册内存响应进程
        register_process("memory_response",
            [this]() { return check_memory_response_trigger(); },
            [this]() { return check_memory_response_exec(); },
            [this]() { return check_memory_response_finish(); },
            [this]() { return check_memory_response_end(); },
            1
        );
        // 注册缓存访问进程
        register_process("cache_access",
            [this]() { return check_cache_access_trigger(); },
            [this]() { return check_cache_access_exec(); },
            [this]() { return check_cache_access_finish(); },
            [this]() { return check_cache_access_end(); },
            1
        );
        
    }
    
    std::unordered_map<std::string, uint64_t> get_module_specific_stats() const override {
        std::unordered_map<std::string, uint64_t> stats;
        stats["read_hits"] = read_hits_;
        stats["read_misses"] = read_misses_;
        stats["write_hits"] = write_hits_;
        stats["write_misses"] = write_misses_;
        stats["evictions"] = evictions_;
        stats["total_accesses"] = total_accesses_;
        stats["hit_rate"] = total_accesses_ > 0 ? ((read_hits_ + write_hits_) * 100) / total_accesses_ : 0;
        stats["pending_requests"] = pending_requests_.size();
        
        // 计算缓存占用率
        uint64_t used_lines = 0;
        uint64_t total_lines = num_sets_ * associativity_;
        for (const auto& set : sets_) {
            for (const auto& line : set.ways) {
                if (line.valid) used_lines++;
            }
        }
        stats["cache_utilization"] = (used_lines * 100) / total_lines;
        
        return stats;
    }
    
    // 获取缓存行数据（用于调试）
    const CacheLine* get_cache_line(uint64_t addr) const {
        uint64_t set_index = (addr / line_size_) % num_sets_;
        uint64_t tag = addr / (line_size_ * num_sets_);
        
        for (const auto& line : sets_[set_index].ways) {
            if (line.valid && line.tag == tag) {
                return &line;
            }
        }
        return nullptr;
    }
    
private:
    // ========== 进程条件函数 ==========
    bool check_cache_access_trigger() {
        auto clk = get_signal_value("clk");
        auto req = get_signal_value("req_in");
        auto data_valid = get_signal_value("data_valid");

        std::cout << "Checking cache access trigger: clk=" 
                  << (clk.has_value() ? std::any_cast<bool>(clk) : false) 
                  << ", req_in=" 
                  << (req.has_value() ? std::any_cast<bool>(req) : false) 
                  << ", data_valid="
                  << (data_valid.has_value() ? std::any_cast<bool>(data_valid) : false)
                  << std::endl;
        
        if (clk.has_value() && req.has_value()) {
            try {
                bool clock_edge = std::any_cast<bool>(clk);
                bool request = std::any_cast<bool>(req);
                
                // 在时钟上升沿且有请求时触发
                return clock_edge && request && pending_requests_.empty();
            } catch (...) {}
        }
        return false;
    }
    
    bool check_cache_access_exec() {
        // 应该return ready信号的值。当前假设触发就可以立刻执行
        // 获取输入信号
        auto addr_val = get_signal_value("addr_in");
        auto data_val = get_signal_value("data_in");
        auto we_val = get_signal_value("we_in");
        
        if (!addr_val.has_value()) return false;
        
        try {
            uint64_t addr = std::any_cast<uint64_t>(addr_val);
            bool write_enable = we_val.has_value() ? std::any_cast<bool>(we_val) : false;
            
            total_accesses_++;
            
            // 计算缓存索引
            uint64_t set_index = (addr / line_size_) % num_sets_;
            uint64_t tag = addr / (line_size_ * num_sets_);
            uint64_t line_offset = (addr % line_size_) / 4;  // 4字节字偏移
            
            // 查找缓存行
            CacheLine* hit_line = nullptr;
            CacheLine* lru_line = nullptr;
            uint64_t min_lru = UINT64_MAX;
            
            for (auto& line : sets_[set_index].ways) {
                if (line.valid && line.tag == tag) {
                    hit_line = &line;
                    break;
                }
                if (line.lru_counter < min_lru) {
                    min_lru = line.lru_counter;
                    lru_line = &line;
                }
            }
            
            if (hit_line) {
                // 缓存命中
                if (write_enable) {
                    // 写命中
                    write_hits_++;
                    hit_line->state = CacheLineState::MODIFIED;
                    
                    if (data_val.has_value()) {
                        uint32_t data = std::any_cast<uint32_t>(data_val);
                        hit_line->data[line_offset] = data;
                    }
                    
                    // 立即响应
                    submit_signal_value("ready", true, hit_latency_);
                } else {
                    // 读命中
                    read_hits_++;
                    uint32_t data = hit_line->data[line_offset];
                    
                    // 延迟响应
                    submit_signal_value("data_out", data, hit_latency_);
                    submit_signal_value("data_valid", true, hit_latency_);
                }
                
                // 更新LRU
                hit_line->lru_counter = ++global_lru_counter_;
            } else {
                // 缓存未命中
                if (write_enable) {
                    write_misses_++;
                } else {
                    read_misses_++;
                }
                
                // 创建未完成请求
                uint32_t data = data_val.has_value() ? std::any_cast<uint32_t>(data_val) : 0;
                auto requester = std::weak_ptr<ISimulatable>(
                    std::static_pointer_cast<ISimulatable>(shared_from_this())
                );
                
                pending_requests_.push(PendingRequest(
                    write_enable ? RequestType::WRITE : RequestType::READ,
                    addr, data, get_process_manager()->get_performance_stats()["total_completed_events"],
                    requester
                ));
                
                // 需要从内存加载
                if (lru_line && lru_line->valid && lru_line->state == CacheLineState::MODIFIED) {
                    // 写回脏行
                    evictions_++;
                    uint64_t evict_addr = (lru_line->tag * num_sets_ + set_index) * line_size_;
                    submit_signal_value("mem_addr", evict_addr, 1);
                    submit_signal_value("mem_data", lru_line->data[0], 1);  // 简化：只写第一个字
                    submit_signal_value("mem_req", true, 1);
                    submit_signal_value("mem_we", true, 1);
                }
                
                // 发起内存读请求
                submit_signal_value("mem_addr", (addr / line_size_) * line_size_, 1);
                submit_signal_value("mem_req", true, 1);
                submit_signal_value("mem_we", false, 1);
                
                // 分配新的缓存行
                if (lru_line) {
                    lru_line->tag = tag;
                    lru_line->state = write_enable ? CacheLineState::MODIFIED : CacheLineState::SHARED;
                    lru_line->valid = true;
                    lru_line->lru_counter = ++global_lru_counter_;
                    
                    if (write_enable) {
                        lru_line->data[line_offset] = data;
                    }
                }
                
                // 设置未命中延迟
                // submit_signal_value("ready", false, miss_latency_);
                // submit_signal_value("mem_valid", true, miss_latency_); // 等待内存响应
            }
            
        } catch (const std::bad_any_cast& e) {
            std::cerr << "Cache: Bad any_cast in cache access: " << e.what() << std::endl;
        }
        return true;
    }
    
    bool check_cache_access_finish() {
        auto data_valid = get_signal_value("data_valid");
        // 
        return data_valid.has_value() && std::any_cast<bool>(data_valid);
    }
    
    bool check_cache_access_end() {
        // 实际返回valid和下级ready的握手情况
        submit_signal_value("ready", true, 1); // 访问完成后重置ready信号
        submit_signal_value("data_valid", false, 1); // 重置数据有效信号
        return true;
    }
    
    bool check_memory_response_trigger() {
        // 模拟内存响应（简化：固定延迟后触发）
        // if (!pending_requests_.empty()) {
        //     uint64_t current_cycle = get_process_manager()->get_performance_stats()["total_completed_events"];
        //     const auto& req = pending_requests_.front();
            
        //     // 检查是否到达内存延迟
        //     return (current_cycle - req.issue_cycle) >= miss_latency_;
        // }
        // return false;
        return !pending_requests_.empty();
    }
    
    bool check_memory_response_exec() {
        // 实际应该返回主存和cache请求的握手，此处简化
        auto& req = pending_requests_.front();
        
        // 处理内存响应
        if (req.type == RequestType::READ) {
            // 模拟从内存读取数据（简化：返回地址作为数据）
            uint32_t simulated_data = static_cast<uint32_t>(req.addr & 0xFFFFFFFF);
            
            // 更新缓存行数据
            uint64_t set_index = (req.addr / line_size_) % num_sets_;
            uint64_t tag = req.addr / (line_size_ * num_sets_);
            uint64_t line_offset = (req.addr % line_size_) / 4;
            
            for (auto& line : sets_[set_index].ways) {
                if (line.valid && line.tag == tag) {
                    line.data[line_offset] = simulated_data;
                    line.lru_counter = ++global_lru_counter_;
                    break;
                }
            }
            // 发送数据响应
            submit_signal_value("mem_data", simulated_data, miss_latency_);
            submit_signal_value("mem_valid", true, miss_latency_);
            submit_signal_value("data_valid", true, miss_latency_);
        }
        
        // 请求完成
        submit_signal_value("ready", true, 1);
        pending_requests_.pop();
        return true;
    }
    
    bool check_memory_response_finish() {
        auto mem_valid = get_signal_value("mem_valid");
        return mem_valid.has_value() && std::any_cast<bool>(mem_valid);
        // return true;
        
        // return true;
    }
    
    bool check_memory_response_end() {
        // 重置内存响应信号
        submit_signal_value("mem_valid", false, 1);
        return true;
    }
};

#endif // CACHE_H