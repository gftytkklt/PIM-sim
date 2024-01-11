#ifndef SEQ_MODULE_H
#define SEQ_MODULE_H
#include "sim.h"

// use this simple func module to specify module property
// first, ctor of specific module shoule construct a module with name and parent
// second, exec func should be overrided to specify module behavior
class seqFetch : Module{
public:
    seqFetch(std::string n, Module* parent) : Module(n, parent){}
    void exec() final;
};

#endif