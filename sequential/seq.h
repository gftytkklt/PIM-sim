#ifndef SEQ_H
#define SEQ_H
#include <memory>
#include <vector>
#include <unordered_map>
#include "sim.h"
#include "seq_module.h"
#include <iostream>

// SeqTask, provide task property
class SeqTask{
public:
    SeqTask(uint64_t inst_num) : inst_num(inst_num){std::cout<<"Hello task"<<std::endl;}
    void set_inst_num(uint64_t num){inst_num = num;}
    void incr_commit_num(){commit_num++;}
    uint64_t get_inst_num() const {return inst_num;}
    uint64_t get_commit_num() const {return commit_num;}
    bool is_finished() const {return finished;}
    void set_finished(){finished = true;}
private:
    uint64_t inst_num;
    uint64_t commit_num = 0;
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
class SeqPipeline : public PerfModel<SeqTask, SeqMemory>{
public:
    SeqPipeline(std::string n, uint64_t inst_num) : PerfModel<SeqTask, SeqMemory>(n), task(inst_num){}
    void setInstNum(uint64_t num) {
        task.set_inst_num(num);
    }

    void incrCommitNum() {
        task.incr_commit_num();
    }

    uint64_t getInstNum() const {
        return task.get_inst_num();
    }

    uint64_t getCommitNum() const {
        return task.get_commit_num();
    }

    bool isFinished() const {
        return task.is_finished();
    }

    void setFinished() {
        task.set_finished();
    }
private:
    SeqTask task;
};

#endif