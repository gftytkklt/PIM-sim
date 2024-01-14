#ifndef SEQ_H
#define SEQ_H
#include <memory>
#include <vector>
#include <unordered_map>
// #include "../sim.h"
#include "seq_module.h"
#include <iostream>

// SeqTask, provide task property
enum class OPTYPE{NONE, ADD, MUL};

class SeqTask{
public:
    SeqTask(uint64_t inst_num) : inst_num(inst_num){std::cout<< inst_num << " tasks set for sim" <<std::endl;}
    uint64_t get_inst_num() const {return inst_num;}
    bool is_finished() const {return finished;}
    void set_finished(){finished = true;}
    struct Inst{
        int data;
        OPTYPE type;
        int src1, src2, dest;
    };
private:
    uint64_t inst_num;
    bool finished = false;
};
// SeqMemory, unimpl now
class SeqMemory{
public:
    SeqMemory(){std::cout<<"Hello mem\n"<<std::endl;}
};
// TODO: add actual run func, use Task info to halt sim
// TODO: add global parameter for control flow and perf counter
// TODO: add module defined in seq_module
class SeqPipeline : public PerfModel<SeqTask>{
public:
    SeqPipeline(std::string n, uint64_t inst_num);
    uint64_t getInstNum() const {return task.get_inst_num();}
    bool isFinished() const {return task.is_finished();}
    void setFinished() {task.set_finished();}
    void run() override;
    void init_memory();
    void addInst(std::unique_ptr<Inst> inst){memory.push_back(std::move(inst));}
    std::unique_ptr<Inst> getInst(){
        if (memory.empty()) {
            return nullptr; // 或者抛出一个异常
        }
        std::unique_ptr<Inst> removedElement = std::move(memory.front()); // 移动队头元素
        memory.erase(memory.begin()); // 从向量中移除元素
        return removedElement; // 返回移除的元素
    }
private:
    SeqTask task;
    SeqState<Inst> state;
    std::vector<std::unique_ptr<Inst>> memory;
    std::shared_ptr<SeqFetch<Inst>> fetch;
    std::shared_ptr<SeqIssue<Inst>> issue;
    std::shared_ptr<SeqExec<Inst>> exec;
    std::shared_ptr<SeqCommit<Inst>> commit;
};

#endif