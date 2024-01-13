#ifndef SEQ_MODULE_H
#define SEQ_MODULE_H
#include "../sim.h"

// use this simple func module to specify module property
// first, ctor of specific module shoule construct a module with name and parent
// second, exec func should be overrided to specify module behavior
struct SeqState{
    int fetchn = 0;
    int issuen = 0;
    int execn = 0;
    int commitn = 0;
};

class BaseSeqModel : public Module{
public:
    BaseSeqModel(std::string n, Module* parent, SeqState& state) : Module(n, parent), state(state){}
protected:
    SeqState& state;
};

class SeqFetch : public BaseSeqModel{
public:
    SeqFetch(std::string n, Module* parent, SeqState& state) : BaseSeqModel(n, parent, state){}
    void exec() final{std::cout << "Fetch!" << ++state.fetchn+1 << std::endl;}
};

class SeqIssue : public BaseSeqModel{
public:
    SeqIssue(std::string n, Module* parent, SeqState& state) : BaseSeqModel(n, parent, state){}
    void exec() final{std::cout << "Issue!" << ++state.issuen+2 << std::endl;}
};

class SeqExec : public BaseSeqModel{
public:
    SeqExec(std::string n, Module* parent, SeqState& state) : BaseSeqModel(n, parent, state){}
    void exec() final{std::cout << "Exec!" << ++state.execn+3 << std::endl;}
};

class SeqCommit : public BaseSeqModel{
public:
    SeqCommit(std::string n, Module* parent, SeqState& state) : BaseSeqModel(n, parent, state){}
    void exec() final{std::cout << "Commit!" << state.commitn+4 << std::endl;}
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


#endif