#include "seq.h"

SeqPipeline::SeqPipeline(std::string n, uint64_t inst_num) : PerfModel<SeqTask>(n), task(inst_num){
    fetch = std::make_shared<SeqFetch<Inst>>("fetch", root.get(), state, memory);
    issue = std::make_shared<SeqIssue<Inst>>("issue", root.get(), state);
    exec = std::make_shared<SeqExec<Inst>>("exec", root.get(), state);
    commit = std::make_shared<SeqCommit<Inst>>("commit", root.get(), state);
    // model dependency
    fetch->addNext(issue);
    issue->addNext(exec);
    exec->addNext(commit);
}

void SeqPipeline::run(){
    while(!task.is_finished()){
        clock();
        if(state.commit_num == task.get_inst_num()){
            task.set_finished();
        }
        else if(get_cycle() > 100){
            break;
        }
        incr_cycle();
    }
    if(!task.is_finished()){
        std::cout << "check dead loop!" << std::endl;
    }
    else{
        std::cout << "hit good trap at cycle "<< get_cycle() << std::endl;
    }
}

void SeqPipeline::init_memory(){
    auto inst_num = task.get_inst_num();
    for(uint64_t i = 0; i < inst_num; i++){
        OPTYPE op = (i % 5) ? OPTYPE::ADD : OPTYPE::MUL;
        addInst(std::make_unique<Inst>(0, op, 0, 0, 0));
    }
}