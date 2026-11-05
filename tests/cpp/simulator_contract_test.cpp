#include "simulator/Simulator.h"
#include <gtest/gtest.h>
#include <limits>
#include <tuple>

class ContractModule : public ModuleBase {
public:
    explicit ContractModule(const std::string& id) : ModuleBase(id) {
        add_signal(Signal("out", Signal::Direction::OUTPUT, 0));
        add_signal(Signal("in", Signal::Direction::INPUT, 0));
    }
    void register_processes() override {}
    void register_message_handlers() override {}
};

TEST(SimulatorContract, ZeroDelayMessageIsDeliveredInProducingCycle) {
    CycleAccurateSimulator sim(3);
    auto module = sim.register_module<ContractModule>("source", 0);
    std::vector<uint64_t> observed;
    sim.register_task_handler("zero", [&](const GenericMessage&) {
        observed.push_back(sim.get_current_cycle());
    });
    module->submit_message("zero", 1); // default delay is zero
    EXPECT_NO_THROW(sim.run());
    EXPECT_EQ(observed, std::vector<uint64_t>({0}));
}

TEST(SimulatorContract, SameCycleSignalsPreserveSubmissionOrder) {
    CycleAccurateSimulator sim(3);
    auto module = sim.register_module<ContractModule>("source", 0);
    for (int i = 0; i < 32; ++i) {
        sim.schedule_signal_update({1, module, "out", i});
    }
    sim.run();
    ASSERT_TRUE(module->get_signal_as<int>("out"));
    EXPECT_EQ(*module->get_signal_as<int>("out"), 31);
}

TEST(SimulatorContract, DirectSignalWriteChecksTypeAndName) {
    auto module = std::make_shared<ContractModule>("typed");
    EXPECT_THROW(module->set_signal_value("out", true, 0), std::runtime_error);
    EXPECT_THROW(module->set_signal_value("missing", 1, 0), std::runtime_error);
}

TEST(SimulatorContract, FutureDirectWriteCannotOverwriteCurrentValue) {
    auto module = std::make_shared<ContractModule>("future");
    module->set_signal_value("out", 1, 0);
    EXPECT_THROW(module->set_signal_value("out", 2, 5), std::runtime_error);
    ASSERT_TRUE(module->get_signal_as<int>("out"));
    EXPECT_EQ(*module->get_signal_as<int>("out"), 1);
}

class RelayModule : public ContractModule {
public:
    using ContractModule::ContractModule;
    bool consumed{false};
    void register_processes() override {
        register_process("relay", [this] { return !consumed && get_signal_as<int>("in").has_value(); },
                         [this] {
                             submit_signal_value("out", *get_signal_as<int>("in") + 1, 0);
                             consumed = true;
                             return true;
                         }, [] { return true; }, [] { return true; }, 0);
    }
};

TEST(SimulatorContract, ZeroDelayChainSettlesInOneCycle) {
    for (int run = 0; run < 4; ++run) {
        CycleAccurateSimulator sim(10);
        auto source = sim.register_module<ContractModule>("source", 0);
        auto middle = sim.register_module<RelayModule>("middle", 1);
        auto sink = sim.register_module<RelayModule>("sink", 2);
        sim.connect_modules("source", "out", "middle", "in");
        sim.connect_modules("middle", "out", "sink", "in");
        source->submit_signal_value("out", 10, 0);
        sim.set_completion_condition([&] { return middle->consumed && sink->consumed; });
        sim.run();
        ASSERT_TRUE(sim.is_simulation_done());
        EXPECT_EQ(sim.get_stop_reason(), CycleAccurateSimulator::StopReason::Completed);
        EXPECT_EQ(sim.get_total_cycles(), 1);
        EXPECT_EQ(sim.get_total_events(), 3);
        EXPECT_EQ(*sink->get_signal_as<int>("out"), 12);
        ASSERT_EQ(sink->get_completed_processes().size(), 1);
        const auto& event = sink->get_completed_processes()[0];
        EXPECT_EQ(event->get_trigger_time(), 0);
        EXPECT_EQ(event->get_end_time(), 0);
    }
}

TEST(SimulatorContract, SameCycleMessagesPreserveSubmissionOrder) {
    CycleAccurateSimulator sim(3);
    auto source = sim.register_module<ContractModule>("source", 0);
    std::vector<int> expected, observed;
    sim.register_task_handler<int>("ordered", [&](const int& n) { observed.push_back(n); });
    for (int i = 0; i < 32; ++i) {
        expected.push_back(i);
        source->submit_message("ordered", i, 1);
    }
    sim.set_completion_condition([&] { return observed == expected; });
    sim.run();
    EXPECT_EQ(observed, expected);
    EXPECT_EQ(sim.get_total_events(), 32);
    EXPECT_EQ(sim.get_total_cycles(), 2);
    EXPECT_TRUE(sim.is_simulation_done());
}

class SignalObserver : public ContractModule {
public:
    using ContractModule::ContractModule;
    std::vector<std::optional<int>> values;
    std::vector<SimulatorEvent> evaluate(uint64_t cycle) override {
        values.push_back(get_signal_as<int>("in"));
        return ContractModule::evaluate(cycle);
    }
};

TEST(SimulatorContract, FutureFeedbackDoesNotReplaceCurrentSignal) {
    CycleAccurateSimulator sim(10);
    auto source = sim.register_module<ContractModule>("source", 2);
    auto sink = sim.register_module<SignalObserver>("sink", 0);
    sim.connect_modules("source", "out", "sink", "in");
    sim.schedule_signal_update({0, source, "out", 1});
    sim.schedule_signal_update({3, source, "out", 2});
    sim.run();
    EXPECT_EQ(sink->values, (std::vector<std::optional<int>>{1, 1, 1, 2}));
    EXPECT_EQ(sim.get_total_events(), 2);
    EXPECT_EQ(sim.get_total_cycles(), 4);
}

TEST(SimulatorContract, EventExhaustionDoesNotProveWorkloadCompletion) {
    CycleAccurateSimulator sim(3);
    sim.set_completion_condition([] { return false; });
    sim.run();
    EXPECT_FALSE(sim.is_simulation_done());
    EXPECT_EQ(sim.get_stop_reason(), CycleAccurateSimulator::StopReason::Quiescent);
    EXPECT_EQ(sim.get_total_cycles(), 1);
    EXPECT_THROW(sim.run(), std::runtime_error);
}

TEST(SimulatorContract, IdleEngineWithoutCompletionContractIsQuiescent) {
    CycleAccurateSimulator sim(3);
    sim.run();
    EXPECT_FALSE(sim.is_simulation_done());
    EXPECT_EQ(sim.get_stop_reason(), CycleAccurateSimulator::StopReason::Quiescent);
}

TEST(SimulatorContract, StalledActiveProcessReportsCycleLimit) {
    CycleAccurateSimulator sim(3);
    auto source = sim.register_module<ContractModule>("source", 0);
    source->register_process("stalled", [] { return true; }, [] { return false; },
                             [] { return false; }, [] { return false; });
    sim.set_completion_condition([] { return true; }); // active work still prevents success
    sim.run();
    EXPECT_FALSE(sim.is_simulation_done());
    EXPECT_EQ(sim.get_stop_reason(), CycleAccurateSimulator::StopReason::MaxCycles);
    EXPECT_EQ(sim.get_total_cycles(), 3);
    EXPECT_EQ(source->get_active_processes().size(), 1);
}

TEST(SimulatorContract, ZeroCycleBudgetReportsLimitWithoutEvaluation) {
    CycleAccurateSimulator sim(0);
    sim.run();
    EXPECT_EQ(sim.get_stop_reason(), CycleAccurateSimulator::StopReason::MaxCycles);
    EXPECT_EQ(sim.get_total_cycles(), 0);
    EXPECT_FALSE(sim.is_simulation_done());
}

class NonConvergingModule : public ContractModule {
public:
    using ContractModule::ContractModule;
    std::vector<SimulatorEvent> evaluate(uint64_t cycle) override {
        submit_signal_value("out", 1, 0);
        return ContractModule::evaluate(cycle);
    }
};

TEST(SimulatorContract, NonConvergingZeroDelayFeedbackFailsExplicitly) {
    CycleAccurateSimulator sim(3);
    sim.register_module<NonConvergingModule>("oscillator", 0);
    sim.set_max_delta_steps(8);
    EXPECT_THROW(sim.run(), std::runtime_error);
    EXPECT_FALSE(sim.is_simulation_done());
    EXPECT_EQ(sim.get_stop_reason(), CycleAccurateSimulator::StopReason::Error);
}

TEST(SimulatorContract, ProcessCanTriggerOnlyOncePerCycleAcrossDeltaSteps) {
    CycleAccurateSimulator sim(3);
    auto source = sim.register_module<ContractModule>("source", 0);
    int executions = 0;
    source->register_process("once", [] { return true; }, [&] {
        ++executions;
        source->submit_signal_value("out", executions, 0);
        return true;
    }, [] { return true; }, [] { return true; }, 0);
    sim.run();
    EXPECT_EQ(executions, 1);
    EXPECT_EQ(source->get_completed_processes().size(), 1);
}

TEST(SimulatorContract, InternalStateDependencySettlesBeforeQuiescence) {
    CycleAccurateSimulator sim(3);
    auto module = sim.register_module<ContractModule>("internal", 0);
    bool producer_finished = false, consumer_finished = false;
    // Register consumer first: its input becomes ready during producer execution.
    module->register_process("consumer", [&] { return producer_finished; }, [&] {
        consumer_finished = true;
        return true;
    }, [] { return true; }, [] { return true; }, 0);
    module->register_process("producer", [&] { return !producer_finished; }, [&] {
        producer_finished = true;
        return true;
    }, [] { return true; }, [] { return true; }, 0);
    sim.set_completion_condition([&] { return consumer_finished; });
    sim.run();
    EXPECT_TRUE(consumer_finished);
    EXPECT_TRUE(sim.is_simulation_done());
    EXPECT_EQ(sim.get_total_cycles(), 1);
    EXPECT_EQ(module->get_completed_processes().size(), 2);
}

TEST(SimulatorContract, LongEventCannotRetriggerInItsCompletionCycle) {
    for (int tokens = 1; tokens <= 2; ++tokens) {
        CycleAccurateSimulator sim(10);
        auto module = sim.register_module<ContractModule>("handshake", 0);
        int executions = 0;
        module->set_signal_value("in", 1, 0);
        module->register_process("compute", [&] { return module->get_signal_as<int>("in").has_value(); },
            [&] {
                ++executions;
                module->submit_signal_value("out", 1, 2);
                return true;
            }, [&] { return module->get_signal_as<int>("out").has_value(); },
            [&] {
                // A registered handshake deassertion is visible at t_end + 1.
                module->submit_signal_value("in", {}, 1);
                module->submit_signal_value("out", {}, 1);
                return true;
            }, 2);
        if (tokens == 2) sim.schedule_signal_update({4, module, "in", 2});
        sim.set_completion_condition([&] { return executions == tokens; });
        sim.run();
        EXPECT_EQ(executions, tokens);
        ASSERT_EQ(module->get_completed_processes().size(), static_cast<size_t>(tokens));
        EXPECT_EQ(module->get_completed_processes()[0]->get_trigger_time(), 0);
        EXPECT_EQ(module->get_completed_processes()[0]->get_end_time(), 2);
        if (tokens == 2) {
            EXPECT_EQ(module->get_completed_processes()[1]->get_trigger_time(), 4);
            EXPECT_EQ(module->get_completed_processes()[1]->get_end_time(), 6);
        }
        EXPECT_EQ(sim.get_total_cycles(), tokens == 1 ? 4 : 8);
        EXPECT_TRUE(sim.is_simulation_done());
    }
}

TEST(SimulatorContract, DuplicateModulesAreRejected) {
    CycleAccurateSimulator sim;
    sim.register_module<ContractModule>("unique", 0);
    EXPECT_THROW(sim.register_module<ContractModule>("unique", 1), std::runtime_error);
    EXPECT_EQ(sim.get_all_modules().size(), 1);
}

TEST(SimulatorContract, DelayedMessageKeepsEngineAliveUntilDelivered) {
    CycleAccurateSimulator sim(10);
    auto source = sim.register_module<ContractModule>("source", 0);
    int messages = 0;
    sim.register_task_handler("late", [&](const GenericMessage&) {
        EXPECT_EQ(sim.get_current_cycle(), 5);
        ++messages;
    });
    source->submit_message("late", 42, 5);
    sim.set_completion_condition([&] { return messages == 1; });
    sim.run();
    EXPECT_EQ(messages, 1);
    EXPECT_EQ(sim.get_total_cycles(), 6);
    EXPECT_TRUE(sim.is_simulation_done());
}

TEST(SimulatorContract, RelativeTimestampOverflowFailsExplicitly) {
    CycleAccurateSimulator sim(10);
    auto source = sim.register_module<ContractModule>("source", 0);
    sim.register_task_handler("start", [&](const GenericMessage&) {
        source->submit_message("never", 0, std::numeric_limits<uint64_t>::max());
    });
    source->submit_message("start", 0, 1);
    EXPECT_THROW(sim.run(), std::overflow_error);
    EXPECT_EQ(sim.get_stop_reason(), CycleAccurateSimulator::StopReason::Error);
}

TEST(SimulatorContract, EmptyTypedMessageIsRejected) {
    CycleAccurateSimulator sim(2);
    auto source = sim.register_module<ContractModule>("source", 0);
    sim.register_task_handler<int>("typed", [](const int&) {});
    source->submit_message(GenericMessage("typed", std::any{}, 0));
    EXPECT_THROW(sim.run(), std::runtime_error);
    EXPECT_EQ(sim.get_stop_reason(), CycleAccurateSimulator::StopReason::Error);
}

TEST(SimulatorContract, ProcessRegistrationDefinesStablePriority) {
    ProcessManager manager;
    std::vector<int> order;
    for (int i = 0; i < 32; ++i) {
        manager.register_process_type(std::to_string(i), [] { return true; }, [&order, i] {
            order.push_back(i);
            return true;
        }, [] { return true; }, [] { return true; });
    }
    manager.drive_state_transitions(0);
    ASSERT_EQ(order.size(), 32);
    for (int i = 0; i < 32; ++i) EXPECT_EQ(order[i], i);
    manager.drive_state_transitions(0);
    EXPECT_EQ(order.size(), 32);
    manager.reset_all_events();
    manager.drive_state_transitions(0);
    EXPECT_EQ(order.size(), 64);
}

TEST(SimulatorContract, ProcessCallbackCanCreateAnotherEventWithoutInvalidation) {
    ProcessManager manager;
    manager.register_process_type("later", {}, [] { return true; }, [] { return true; }, [] { return true; });
    manager.register_process_type("first", [] { return true; }, [&] {
        for (int i = 0; i < 100; ++i) manager.create_active_event("later", 0);
        return true;
    }, [] { return true; }, [] { return true; });
    manager.drive_state_transitions(0);
    EXPECT_EQ(manager.get_completed_events().size(), 1);
    manager.drive_state_transitions(0);
    EXPECT_EQ(manager.get_completed_events().size(), 101);
}

TEST(SimulatorContract, DuplicateAndMultipleSignalDriversAreRejected) {
    CycleAccurateSimulator sim;
    sim.register_module<ContractModule>("first", 0);
    sim.register_module<ContractModule>("second", 0);
    sim.register_module<ContractModule>("sink", 1);
    sim.connect_modules("first", "out", "sink", "in");
    EXPECT_THROW(sim.connect_modules("first", "out", "sink", "in"), std::runtime_error);
    EXPECT_THROW(sim.connect_modules("second", "out", "sink", "in"), std::runtime_error);
}

TEST(SimulatorContract, ImmediateMessageApiRejectsIgnoredDelay) {
    CycleAccurateSimulator sim;
    auto module = sim.register_module<ContractModule>("sink", 0);
    module->register_message_handler("receive", [](const GenericMessage&) {});
    EXPECT_THROW(sim.send_message_to_core("sink", GenericMessage("receive", 1, 2)), std::invalid_argument);
}

TEST(SimulatorContract, QueuedCoreMessageHonorsDelayAndPreservesPayload) {
    CycleAccurateSimulator sim(10);
    auto sink = sim.register_module<ContractModule>("sink", 0);
    std::vector<std::pair<uint64_t, int>> deliveries;
    sink->register_message_handler<int>("receive", [&](const int& value) {
        deliveries.emplace_back(sim.get_current_cycle(), value);
    });
    sim.schedule_message_to_core("sink", GenericMessage("receive", 7, 3));
    sim.schedule_message_to_core("sink", GenericMessage("receive", 8, 3));
    EXPECT_TRUE(deliveries.empty());
    sim.set_completion_condition([&] { return deliveries.size() == 2; });
    sim.run();
    EXPECT_EQ(deliveries, (std::vector<std::pair<uint64_t, int>>{{3, 7}, {3, 8}}));
    EXPECT_EQ(sim.get_total_events(), 2);
    EXPECT_EQ(sim.get_total_cycles(), 4);
    EXPECT_TRUE(sim.is_simulation_done());
}

TEST(SimulatorContract, QueuedCoreMessageZeroDelayLoopIsBounded) {
    CycleAccurateSimulator sim(2);
    sim.set_max_delta_steps(8);
    auto sink = sim.register_module<ContractModule>("sink", 0);
    sink->register_message_handler("repeat", [&](const GenericMessage& msg) {
        sim.schedule_message_to_core("sink", msg);
    });
    sim.schedule_message_to_core("sink", GenericMessage("repeat", 1, 0));
    EXPECT_THROW(sim.run(), std::runtime_error);
    EXPECT_EQ(sim.get_stop_reason(), CycleAccurateSimulator::StopReason::Error);
}

TEST(SimulatorContract, ProcessTimestampsCannotMoveBackwards) {
    ProcessEvent event("operation", 0);
    event.set_triggered(4);
    EXPECT_THROW(event.set_executing(3), std::invalid_argument);
    event.set_executing(6);
    EXPECT_THROW(event.set_finished(5), std::invalid_argument);
    event.set_finished(8);
    EXPECT_THROW(event.set_ended(7), std::invalid_argument);
}

TEST(SimulatorContract, OutputFailureIsNotReportedAsSuccess) {
    CycleAccurateSimulator sim;
    EXPECT_THROW(sim.dump_completed_events("/no-such-pim-review-directory/output.csv"), std::runtime_error);
}

TEST(SimulatorContract, IncrementDependencyConservesExcessTokens) {
    int fired = 0;
    auto entry = make_increment_dependency("tokens", [](const GenericMessage& msg) {
        return std::any_cast<std::pair<std::string, int>>(msg.body);
    }, {{"a", 2}, {"b", 3}}, [&] { ++fired; });
    entry->on_message(GenericMessage("done", std::make_pair(std::string("a"), 7)));
    entry->on_message(GenericMessage("done", std::make_pair(std::string("b"), 7)));
    EXPECT_EQ(fired, 2);
    EXPECT_EQ(entry->counter("a"), 3);
    EXPECT_EQ(entry->counter("b"), 1);
    entry->on_message(GenericMessage("done", std::make_pair(std::string("b"), 2)));
    EXPECT_EQ(fired, 3);
    EXPECT_EQ(entry->counter("a"), 1);
    EXPECT_EQ(entry->counter("b"), 0);
}

TEST(SimulatorContract, IncrementDependencyRejectsInvalidConfigurationAndOverflow) {
    TaskDependencyTable table;
    EXPECT_THROW(table.register_entry(nullptr), std::invalid_argument);
    auto extract = [](const GenericMessage& msg) {
        return std::make_pair(std::string("a"), std::any_cast<int>(msg.body));
    };
    EXPECT_THROW(make_increment_dependency("empty", extract, {}, [] {}), std::invalid_argument);
    EXPECT_THROW(make_increment_dependency("zero", extract, {{"a", 0}}, [] {}), std::invalid_argument);
    auto entry = make_increment_dependency("valid", extract,
                                          {{"a", std::numeric_limits<int>::max()}, {"b", 1}}, [] {});
    EXPECT_THROW(entry->on_message(GenericMessage("done", -1)), std::invalid_argument);
    entry->on_message(GenericMessage("done", std::numeric_limits<int>::max()));
    EXPECT_THROW(entry->on_message(GenericMessage("done", 1)), std::overflow_error);
}
