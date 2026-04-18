#ifndef PROCESSING_UNIT_H
#define PROCESSING_UNIT_H

#include "simulator/ModuleBase.h"
#include <cstdint>
#include <queue>
#include <vector>
#include <iostream>

/**
 * 计算单元模块
 * 负责执行算术和逻辑运算
 */
class ProcessingUnit : public ModuleBase<ProcessingUnit> {
public:
    // 内部操作类型
    enum class Operation {
        NOP,
        ADD,
        SUB,
        MUL,
        DIV,
        AND,
        OR,
        XOR,
        LOAD,
        STORE
    };
    
    // 指令结构
    struct Instruction {
        Operation op;
        uint32_t src1;
        uint32_t src2;
        uint32_t dst;
        uint32_t imm;
        uint64_t pc;
        
        Instruction() : op(Operation::NOP), src1(0), src2(0), dst(0), imm(0), pc(0) {}
    };
    
    // 流水线寄存器
    struct PipelineReg {
        Instruction inst;
        uint32_t src1_val;
        uint32_t src2_val;
        bool valid;
        uint64_t cycle_issued;
        
        PipelineReg() : valid(false), cycle_issued(0) {}
    };
    
private:
    // 内部状态
    std::queue<Instruction> inst_buffer_;        // 指令缓冲
    PipelineReg fetch_reg_;                      // 取指级寄存器
    PipelineReg decode_reg_;                     // 译码级寄存器
    PipelineReg execute_reg_;                    // 执行级寄存器
    PipelineReg writeback_reg_;                  // 写回级寄存器
    
    // 寄存器文件
    std::vector<uint32_t> reg_file_;
    
    // 性能计数器
    uint64_t instructions_executed_{0};
    uint64_t cycles_stalled_{0};
    uint64_t data_hazards_{0};
    
    // 控制信号
    bool stalled_{false};
    bool flush_pipeline_{false};
    
public:
    ProcessingUnit(const std::string& id) 
        : ModuleBase<ProcessingUnit>(id)
        , reg_file_(32, 0)  // 32个通用寄存器
    {
        // 添加输入输出信号
        add_signal(Signal("clk", Signal::Direction::INPUT));
        add_signal(Signal("rst", Signal::Direction::INPUT));
        add_signal(Signal("inst_in", Signal::Direction::INPUT));
        add_signal(Signal("inst_valid", Signal::Direction::INPUT));
        add_signal(Signal("data_in", Signal::Direction::INPUT));
        add_signal(Signal("data_valid", Signal::Direction::INPUT));
        
        add_signal(Signal("inst_ready", Signal::Direction::OUTPUT));
        add_signal(Signal("data_out", Signal::Direction::OUTPUT));
        add_signal(Signal("data_addr", Signal::Direction::OUTPUT));
        add_signal(Signal("data_req", Signal::Direction::OUTPUT));
        add_signal(Signal("data_we", Signal::Direction::OUTPUT));
        add_signal(Signal("alu_result", Signal::Direction::OUTPUT));
        add_signal(Signal("pc_out", Signal::Direction::OUTPUT));
        
        // 绑定信号到进程
        bind_signal_to_process("clk", "pipeline_advance");
        bind_signal_to_process("inst_in", "instruction_fetch");
        bind_signal_to_process("inst_valid", "instruction_fetch");
        bind_signal_to_process("data_in", "data_access");
        bind_signal_to_process("data_valid", "data_access");
    }
    
    // 必须实现的接口
    void register_processes() override {
        // 注册流水线推进进程
        register_process("pipeline_advance",
            [this]() { return check_pipeline_advance_trigger(); },
            [this]() { return check_pipeline_advance_exec(); },
            [this]() { return check_pipeline_advance_finish(); },
            [this]() { return check_pipeline_advance_end(); },
            1
        );
        
        // 注册取指进程
        register_process("instruction_fetch",
            [this]() { return check_fetch_trigger(); },
            [this]() { return check_fetch_exec(); },
            [this]() { return check_fetch_finish(); },
            [this]() { return check_fetch_end(); },
            1
        );
        
        // 注册数据访问进程
        register_process("data_access",
            [this]() { return check_data_access_trigger(); },
            [this]() { return check_data_access_exec(); },
            [this]() { return check_data_access_finish(); },
            [this]() { return check_data_access_end(); },
            1
        );
        
        // 注册ALU计算进程
        register_process("alu_compute",
            [this]() { return check_alu_trigger(); },
            [this]() { return check_alu_exec(); },
            [this]() { return check_alu_finish(); },
            [this]() { return check_alu_end(); },
            1
        );
    }
    
    std::unordered_map<std::string, uint64_t> get_module_specific_stats() const override {
        std::unordered_map<std::string, uint64_t> stats;
        stats["instructions_executed"] = instructions_executed_;
        stats["cycles_stalled"] = cycles_stalled_;
        stats["data_hazards"] = data_hazards_;
        stats["inst_buffer_size"] = inst_buffer_.size();
        stats["pipeline_utilization"] = (instructions_executed_ * 100) / (get_process_manager()->get_performance_stats()["total_completed_events"] + 1);
        return stats;
    }
    
    // 外部接口：注入指令
    void inject_instruction(const Instruction& inst) {
        inst_buffer_.push(inst);
    }
    
    // 获取寄存器值（用于调试）
    uint32_t get_register(uint32_t reg_num) const {
        if (reg_num < reg_file_.size()) {
            return reg_file_[reg_num];
        }
        return 0;
    }
    
private:
    // ========== 进程条件函数 ==========
    
    // pipeline_advance进程的条件函数
    bool check_pipeline_advance_trigger() {
        // 每个时钟周期都触发流水线推进
        auto clk_val = get_signal_value("clk");
        if (clk_val.has_value()) {
            try {
                return std::any_cast<bool>(clk_val) && !stalled_;
            } catch (...) {}
        }
        return false;
    }
    
    bool check_pipeline_advance_exec() {
        return true;  // 总是可以执行
    }
    
    bool check_pipeline_advance_finish() {
        // 推进流水线
        advance_pipeline();
        return true;
    }
    
    bool check_pipeline_advance_end() {
        return true;
    }
    
    // instruction_fetch进程的条件函数
    bool check_fetch_trigger() {
        // 当指令缓冲非空且取指级空闲时触发
        return !inst_buffer_.empty() && !fetch_reg_.valid;
    }
    
    bool check_fetch_exec() {
        return true;
    }
    
    bool check_fetch_finish() {
        // 执行取指
        if (!inst_buffer_.empty()) {
            fetch_reg_.inst = inst_buffer_.front();
            fetch_reg_.valid = true;
            fetch_reg_.cycle_issued = get_process_manager()->get_performance_stats()["total_completed_events"];
            inst_buffer_.pop();
            
            // 输出PC值
            submit_signal_value("pc_out", fetch_reg_.inst.pc, 1);
            
            // 请求下一条指令
            submit_signal_value("inst_ready", true, 1);
        }
        return true;
    }
    
    bool check_fetch_end() {
        return true;
    }
    
    // alu_compute进程的条件函数
    bool check_alu_trigger() {
        // 当执行级有有效指令时触发
        return execute_reg_.valid;
    }
    
    bool check_alu_exec() {
        return true;
    }
    
    bool check_alu_finish() {
        // 执行ALU计算
        uint32_t result = 0;
        const auto& inst = execute_reg_.inst;
        
        switch (inst.op) {
            case Operation::ADD:
                result = execute_reg_.src1_val + execute_reg_.src2_val;
                break;
            case Operation::SUB:
                result = execute_reg_.src1_val - execute_reg_.src2_val;
                break;
            case Operation::MUL:
                result = execute_reg_.src1_val * execute_reg_.src2_val;
                break;
            case Operation::AND:
                result = execute_reg_.src1_val & execute_reg_.src2_val;
                break;
            case Operation::OR:
                result = execute_reg_.src1_val | execute_reg_.src2_val;
                break;
            case Operation::XOR:
                result = execute_reg_.src1_val ^ execute_reg_.src2_val;
                break;
            case Operation::LOAD:
                // 加载指令：生成内存地址
                result = execute_reg_.src1_val + inst.imm;
                submit_signal_value("data_addr", result, 1);
                submit_signal_value("data_req", true, 1);
                submit_signal_value("data_we", false, 1);
                break;
            case Operation::STORE:
                // 存储指令：生成内存地址和数据
                result = execute_reg_.src1_val + inst.imm;
                submit_signal_value("data_addr", result, 1);
                submit_signal_value("data_out", execute_reg_.src2_val, 1);
                submit_signal_value("data_req", true, 1);
                submit_signal_value("data_we", true, 1);
                break;
            default:
                break;
        }
        
        // 设置写回级寄存器
        writeback_reg_.inst = inst;
        if (inst.op != Operation::LOAD && inst.op != Operation::STORE) {
            writeback_reg_.src1_val = result;
        }
        writeback_reg_.valid = true;
        
        // 输出ALU结果
        submit_signal_value("alu_result", result, 1);
        
        instructions_executed_++;
        return true;
    }
    
    bool check_alu_end() {
        return true;
    }
    
    // data_access进程的条件函数
    bool check_data_access_trigger() {
        // 当有数据返回时触发
        auto data_valid = get_signal_value("data_valid");
        if (data_valid.has_value()) {
            try {
                return std::any_cast<bool>(data_valid);
            } catch (...) {}
        }
        return false;
    }
    
    bool check_data_access_exec() {
        return true;
    }
    
    bool check_data_access_finish() {
        // 处理数据返回
        if (writeback_reg_.valid && writeback_reg_.inst.op == Operation::LOAD) {
            auto data_in = get_signal_value("data_in");
            if (data_in.has_value()) {
                try {
                    uint32_t data = std::any_cast<uint32_t>(data_in);
                    writeback_reg_.src1_val = data;
                    
                    // 写入寄存器文件
                    if (writeback_reg_.inst.dst < reg_file_.size()) {
                        reg_file_[writeback_reg_.inst.dst] = data;
                    }
                } catch (...) {}
            }
        }
        return true;
    }
    
    bool check_data_access_end() {
        return true;
    }
    
    // ========== 辅助函数 ==========
    
    void advance_pipeline() {
        // 写回级 -> 完成
        if (writeback_reg_.valid) {
            // 非LOAD指令的寄存器写入
            if (writeback_reg_.inst.op != Operation::LOAD && 
                writeback_reg_.inst.op != Operation::STORE &&
                writeback_reg_.inst.op != Operation::NOP) {
                if (writeback_reg_.inst.dst < reg_file_.size()) {
                    reg_file_[writeback_reg_.inst.dst] = writeback_reg_.src1_val;
                }
            }
            writeback_reg_.valid = false;
        }
        
        // 执行级 -> 写回级
        if (execute_reg_.valid && !writeback_reg_.valid) {
            writeback_reg_ = execute_reg_;
            execute_reg_.valid = false;
        } else if (execute_reg_.valid) {
            // 写回级被占用，流水线暂停
            stalled_ = true;
            cycles_stalled_++;
            return;
        }
        
        // 译码级 -> 执行级
        if (decode_reg_.valid && !execute_reg_.valid) {
            execute_reg_ = decode_reg_;
            
            // 检查数据冒险
            if (check_data_hazard(decode_reg_.inst)) {
                data_hazards_++;
                // 插入气泡
                execute_reg_.valid = false;
                stalled_ = true;
            } else {
                // 读取操作数
                execute_reg_.src1_val = read_operand(decode_reg_.inst.src1);
                execute_reg_.src2_val = read_operand(decode_reg_.inst.src2);
            }
            decode_reg_.valid = false;
        } else if (decode_reg_.valid) {
            stalled_ = true;
            cycles_stalled_++;
            return;
        }
        
        // 取指级 -> 译码级
        if (fetch_reg_.valid && !decode_reg_.valid) {
            decode_reg_ = fetch_reg_;
            fetch_reg_.valid = false;
        } else if (fetch_reg_.valid) {
            stalled_ = true;
            cycles_stalled_++;
            return;
        }
        
        stalled_ = false;
    }
    
    bool check_data_hazard(const Instruction& inst) {
        // 检查RAW冒险
        if (inst.src1 != 0 && (inst.src1 == writeback_reg_.inst.dst && writeback_reg_.valid)) {
            return true;
        }
        if (inst.src2 != 0 && (inst.src2 == writeback_reg_.inst.dst && writeback_reg_.valid)) {
            return true;
        }
        return false;
    }
    
    uint32_t read_operand(uint32_t reg_num) {
        if (reg_num == 0) return 0;  // 零寄存器
        if (reg_num < reg_file_.size()) {
            return reg_file_[reg_num];
        }
        return 0;
    }
};

#endif // PROCESSING_UNIT_H