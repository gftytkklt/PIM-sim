#ifndef SEQ_MODULE_H
#define SEQ_MODULE_H
#include "../sim.h"
#include <queue>
#include <deque>

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

// use this simple func module to specify module property
// first, ctor of specific module shoule construct a module with name and parent
// second, exec func should be overrided to specify module behavior
template<typename Inst>
struct SeqState{
    uint64_t pipewidth = 1;
    uint64_t commit_num = 0;
    uint64_t add_cmt = 0;
    uint64_t mul_cmt = 0;
    uint64_t mul_congest = 0;
    // fetch queue, include op info
    struct FetchReq{
        uint64_t time_left;
        OPTYPE type;
        bool valid;
    };
    std::deque<FetchReq> fetchqueue;
    // issue queue, only include valid info
    struct IssueReq{
        uint64_t time_left;
        bool valid;
    };
    std::queue<IssueReq> addqueue, mulqueue;
    // ALU valid, single element
    bool add_valid = false;
    bool mul_valid = false;
    uint64_t tick = 0;
};

template<typename Inst>
class BaseSeqModel : public Module{
public:
    BaseSeqModel(std::string n, Module* parent, SeqState<Inst>& state) : Module(n, parent), state(state){}
protected:
    SeqState<Inst>& state;
};

template<typename Inst>
class SeqFetch : public BaseSeqModel<Inst>{
public:
    SeqFetch(std::string n, Module* parent, SeqState<Inst>& state, std::vector<std::unique_ptr<Inst>>& memoryRef)
    : BaseSeqModel<Inst>(n, parent, state), memory(memoryRef){}
    // void exec() final{std::cout << "Fetch!" << ++state.fetchn+1 << std::endl;}
    void exec() final;
private:
    uint64_t fetch_lat = 2;// fetch latency
    uint64_t fetch_size = 3;// fetch queue capacity
    uint64_t fetch_index = 0;
    const std::vector<std::unique_ptr<Inst>>& memory;// mem interface
    void fetch_inst();
    void update_queue();
};

template<typename Inst>
class SeqIssue : public BaseSeqModel<Inst>{
public:
    SeqIssue(std::string n, Module* parent, SeqState<Inst>& state) : BaseSeqModel<Inst>(n, parent, state){}
    // void exec() final{std::cout << "Issue!" << ++state.issuen+2 << std::endl;}
    void exec() final;
private:
    uint64_t issue_lat = 1;
    uint64_t add_size = 1;
    uint64_t mul_size = 2;
};

template<typename Inst>
class SeqExec : public BaseSeqModel<Inst>{
public:
    SeqExec(std::string n, Module* parent, SeqState<Inst>& state) : BaseSeqModel<Inst>(n, parent, state){}
    // void exec() final{std::cout << "Exec!" << ++state.execn+3 << std::endl;}
    void exec() final;
private:
    uint64_t add_lat = 0;
    uint64_t mul_lat = 16;
    uint64_t mul_timeleft = 0;
    bool mul_busy = false;
};

template<typename Inst>
class SeqCommit : public BaseSeqModel<Inst>{
public:
    SeqCommit(std::string n, Module* parent, SeqState<Inst>& state) : BaseSeqModel<Inst>(n, parent, state){}
    // void exec() final{std::cout << "Commit!" << state.commitn+4 << std::endl;}
    void exec() final;
};


// class SeqFetch : public Module{
// public:
//     SeqFetch(std::string n, Module* parent) : Module(n, parent){}
//     // void exec() final;
// private:
//     SeqState& state;
// };

// class SeqIssue : public Module{
// public:
//     SeqIssue(std::string n, Module* parent) : Module(n, parent){}
//     // void exec() final;
// };

// class SeqExec : public Module{
// public:
//     SeqExec(std::string n, Module* parent) : Module(n, parent){}
//     // void exec() final;
// };

// class SeqCommit : public Module{
// public:
//     SeqCommit(std::string n, Module* parent) : Module(n, parent){}
//     // void exec() final;
// };
#include "seq_module.hpp"

#endif