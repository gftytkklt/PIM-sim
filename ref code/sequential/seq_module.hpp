#include "seq_module.h"

template<typename Inst>
void SeqFetch<Inst>::fetch_inst(){
    // std::cout << "fetch inst" << "\n";
    if((fetch_index < memory.size()) && (this->state.fetchqueue.size() < fetch_size)){
        OPTYPE type = memory[fetch_index++]->type;
        this->state.fetchqueue.push_back({fetch_lat, type, false});
        // std::cout << "inst: " << fetch_index << "\n";
        if(type == OPTYPE::ADD){
            // std::cout << "inst " << fetch_index << ": add" << "\n";
        }
        else if(type == OPTYPE::MUL){
            // std::cout << "inst " << fetch_index << ": mul" << "\n";
        }
    }
}

template<typename Inst>
void SeqFetch<Inst>::update_queue(){
    // int i = 0;
    for(auto& it: this->state.fetchqueue){
        if(it.time_left == 0){it.valid = true;}
        else{it.time_left--;}
        // std::cout << i << ": " << it.time_left << ", valid: " << it.valid << std::endl;
        // i++;
    }
}

template<typename Inst>
void SeqFetch<Inst>::exec(){
    fetch_inst();
    update_queue();
}

template<typename Inst>
void SeqIssue<Inst>::exec(){
    // check head of fetchqueue
    bool elem_valid;
    OPTYPE elem_type;
    if(!this->state.fetchqueue.empty()){
        auto& elem = this->state.fetchqueue.front();
        elem_valid = elem.valid;
        elem_type = elem.type;
    }
    if(elem_valid){
        switch (elem_type)
        {
        case OPTYPE::ADD:
            if(this->state.addqueue.size()<add_size){
                this->state.addqueue.emplace(issue_lat, false);
                this->state.fetchqueue.pop_front();
            }
            break;
        case OPTYPE::MUL:
            if(this->state.mulqueue.size()<mul_size){
                this->state.mulqueue.emplace(issue_lat, false);
                this->state.fetchqueue.pop_front();
            }
            break;
        default:
            std::cout << "should not reach here!" << std::endl;
            break;
        }
    }
    // this->state.fetchqueue.pop();
}

template<typename Inst>
void SeqExec<Inst>::exec(){
    //add
    if(!this->state.addqueue.empty()){
        this->state.add_valid = true;
        this->state.addqueue.pop();
    }
    //fetch mul
    if(!this->state.mulqueue.empty() && !mul_busy){
        this->state.mulqueue.pop();
        mul_busy = true;
        mul_timeleft = mul_lat;
    }
    //activate & update mul
    if(mul_timeleft == 0){
        this->state.mul_valid = mul_busy;
        mul_busy = false;
    }
    else{
        mul_timeleft--;
    }
    // mul congestion
    if(!this->state.mulqueue.empty() && mul_busy){
        this->state.mul_congest++;
    }
}

template<typename Inst>
void SeqCommit<Inst>::exec(){
    if(this->state.add_valid){
        this->state.commit_num++;
        this->state.add_cmt++;
        this->state.add_valid = false;
        // std::cout << "cmt add at tick " << this->state.tick << "\n";
    }
    if(this->state.mul_valid){
        this->state.commit_num++;
        this->state.mul_cmt++;
        this->state.mul_valid = false;
        // std::cout << "cmt mul at tick " << this->state.tick << "\n";
    }
    this->state.tick++;
}
