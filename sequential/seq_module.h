#ifndef SEQ_MODULE_H
#define SEQ_MODULE_H
#include "../sim.h"
#include <queue>

// use this simple func module to specify module property
// first, ctor of specific module shoule construct a module with name and parent
// second, exec func should be overrided to specify module behavior
template<typename Inst>
struct SeqState{
    uint64_t pipewidth = 1;
    uint64_t commit_num = 0;
    struct FetchReq{
        uint64_t time_left;
        OPTYPE type;
        bool valid;
    };
    std::queue<FetchReq> fetchqueue;
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
    uint64_t fetch_cost = 2;// fetch latency
    uint64_t fetch_queue = 3;// fetch queue capacity
    uint64_t fetch_index = 0;
    const std::vector<std::unique_ptr<Inst>>& memory;// mem interface
    
};

template<typename Inst>
class SeqIssue : public BaseSeqModel<Inst>{
public:
    SeqIssue(std::string n, Module* parent, SeqState<Inst>& state) : BaseSeqModel<Inst>(n, parent, state){}
    // void exec() final{std::cout << "Issue!" << ++state.issuen+2 << std::endl;}
    void exec() final;
};

template<typename Inst>
class SeqExec : public BaseSeqModel<Inst>{
public:
    SeqExec(std::string n, Module* parent, SeqState<Inst>& state) : BaseSeqModel<Inst>(n, parent, state){}
    // void exec() final{std::cout << "Exec!" << ++state.execn+3 << std::endl;}
    void exec() final;
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