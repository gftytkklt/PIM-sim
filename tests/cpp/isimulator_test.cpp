#include "simulator/Simulator.h"
#include <gtest/gtest.h>

// 覆盖引擎钩子验证可测试性
class MockSimulator : public CycleAccurateSimulator {
public:
    MockSimulator() : CycleAccurateSimulator(10) {}
    int cycle_count = 0;
    int signal_processed = 0;
    int message_processed = 0;
protected:
    void simulate_cycle() override {
        cycle_count++;
        CycleAccurateSimulator::simulate_cycle();
    }
    void process_signal_events(uint64_t c) override {
        signal_processed++;
        CycleAccurateSimulator::process_signal_events(c);
    }
    void process_message_events(uint64_t c) override {
        message_processed++;
        CycleAccurateSimulator::process_message_events(c);
    }
};

TEST(ISimulatorTest, InterfacePolymorphism) {
    std::shared_ptr<ISimulator> sim = std::make_shared<MockSimulator>();
    EXPECT_EQ(sim->get_current_cycle(), 0);
    EXPECT_FALSE(sim->is_simulation_done());
}

TEST(ISimulatorTest, EngineHooksOverridable) {
    auto sim = std::make_shared<MockSimulator>();
    sim->run();
    EXPECT_GT(sim->cycle_count, 0);       // simulate_cycle 被覆盖调用
    EXPECT_GT(sim->signal_processed, 0);  // process_signal_events 被覆盖调用
    EXPECT_GT(sim->message_processed, 0); // process_message_events 被覆盖调用
}
