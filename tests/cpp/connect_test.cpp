#include "simulator/Simulator.h"
#include <gtest/gtest.h>

class ConnSrc : public ModuleBase {
public:
    ConnSrc(const std::string& id) : ModuleBase(id) {
        add_signal(Signal("out_sig", Signal::Direction::OUTPUT, 0));
    }
    void register_processes() override {}
    void register_message_handlers() override {}
};
class ConnDst : public ModuleBase {
public:
    ConnDst(const std::string& id) : ModuleBase(id) {
        add_signal(Signal("in_sig", Signal::Direction::INPUT, 0));
    }
    void register_processes() override {}
    void register_message_handlers() override {}
};
class ConnWrongDir : public ModuleBase {
public:
    ConnWrongDir(const std::string& id) : ModuleBase(id) {
        add_signal(Signal("in_sig", Signal::Direction::INPUT, 0));
    }
    void register_processes() override {}
    void register_message_handlers() override {}
};
class ConnWrongType : public ModuleBase {
public:
    ConnWrongType(const std::string& id) : ModuleBase(id) {
        add_signal(Signal("in_sig", Signal::Direction::INPUT, false));
    }
    void register_processes() override {}
    void register_message_handlers() override {}
};

TEST(ConnectTest, ValidConnection) {
    auto sim = std::make_shared<CycleAccurateSimulator>();
    sim->register_module<ConnSrc>("src", 1);
    sim->register_module<ConnDst>("dst", 0);
    EXPECT_NO_THROW(sim->connect_modules("src", "out_sig", "dst", "in_sig"));
}

TEST(ConnectTest, MissingSignal) {
    auto sim = std::make_shared<CycleAccurateSimulator>();
    sim->register_module<ConnSrc>("src", 1);
    sim->register_module<ConnDst>("dst", 0);
    EXPECT_THROW(sim->connect_modules("src", "nonexistent", "dst", "in_sig"), std::runtime_error);
}

TEST(ConnectTest, WrongDirection) {
    auto sim = std::make_shared<CycleAccurateSimulator>();
    sim->register_module<ConnWrongDir>("src", 1);  // 源信号是 INPUT（应为 OUTPUT）
    sim->register_module<ConnDst>("dst", 0);       // 目标信号是 INPUT
    EXPECT_THROW(sim->connect_modules("src", "in_sig", "dst", "in_sig"), std::runtime_error);
}

TEST(ConnectTest, TypeMismatch) {
    auto sim = std::make_shared<CycleAccurateSimulator>();
    sim->register_module<ConnSrc>("src", 1);      // int out_sig
    sim->register_module<ConnWrongType>("dst", 0); // bool in_sig
    EXPECT_THROW(sim->connect_modules("src", "out_sig", "dst", "in_sig"), std::runtime_error);
}
