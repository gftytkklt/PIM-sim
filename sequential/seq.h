#ifndef SEQ_H
#define SEQ_H
#include <memory>
#include <vector>
#include <unordered_map>
// #include "../sim.h"
#include "seq_module.h"
#include <iostream>

// SeqTask, provide task property
enum class OPTYPE{ADD, MUL};

class SeqTask{
public:
    SeqTask(uint64_t inst_num) : inst_num(inst_num){std::cout<<"Hello task"<<std::endl;}
    // void set_inst_num(uint64_t num){inst_num = num;}
    // void incr_commit_num(){commit_num++;}
    uint64_t get_inst_num() const {return inst_num;}
    // uint64_t get_commit_num() const {return commit_num;}
    bool is_finished() const {return finished;}
    void set_finished(){finished = true;}
    struct Inst{
        int data;
        OPTYPE type;
        int src1, src2, dest;
    };
private:
    uint64_t inst_num;
    // uint64_t commit_num = 0;
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
    SeqPipeline(std::string n, uint64_t inst_num) : PerfModel<SeqTask>(n), task(inst_num){
        fetch = std::make_shared<SeqFetch>("fetch", root.get(), state);
        issue = std::make_shared<SeqIssue>("issue", root.get(), state);
        exec = std::make_shared<SeqExec>("exec", root.get(), state);
        commit = std::make_shared<SeqCommit>("commit", root.get(), state);
        // 设置模块链
        fetch->addNext(issue);
        issue->addNext(exec);
        exec->addNext(commit);
    }
    // void setInstNum(uint64_t num) {
    //     task.set_inst_num(num);
    // }

    // void incrCommitNum() {
    //     task.incr_commit_num();
    // }

    uint64_t getInstNum() const {
        return task.get_inst_num();
    }

    // uint64_t getCommitNum() const {
    //     return task.get_commit_num();
    // }

    bool isFinished() const {
        return task.is_finished();
    }

    void setFinished() {
        task.set_finished();
    }

    void run() override{
        while(!task.is_finished()){
            clock();
            if(state.commitn == task.get_inst_num()){
                task.set_finished();
            }
            else if(get_cycle() > 100){
                break;
            }
        }
        if(!task.is_finished()){
            std::cout << "check dead loop!" << std::endl;
        }
        else{
            std::cout << "hit good trap!" << std::endl;
        }
    }

private:
    SeqTask task;
    SeqState state;
    std::shared_ptr<SeqFetch> fetch;
    std::shared_ptr<SeqIssue> issue;
    std::shared_ptr<SeqExec> exec;
    std::shared_ptr<SeqCommit> commit;

};

#endif