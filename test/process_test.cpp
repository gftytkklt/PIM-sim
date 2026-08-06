#include "simulator/Process.h"
#include <gtest/gtest.h>
#include <memory>

class ProcessEventTest : public ::testing::Test {
protected:
    void SetUp() override {
        event = std::make_shared<ProcessEvent>("test_process", 42);
    }
    std::shared_ptr<ProcessEvent> event;
};

TEST_F(ProcessEventTest, InitialStateIsIdle) {
    EXPECT_TRUE(event->is_idle());
    EXPECT_FALSE(event->is_triggered());
    EXPECT_FALSE(event->is_executing());
    EXPECT_FALSE(event->is_finished());
    EXPECT_FALSE(event->is_ended());
    EXPECT_EQ(event->get_state(), ProcessEvent::State::IDLE);
}

TEST_F(ProcessEventTest, StateTransitions) {
    event->set_triggered(10);
    EXPECT_TRUE(event->is_triggered());
    EXPECT_EQ(event->get_trigger_time(), 10);

    event->set_executing(12);
    EXPECT_TRUE(event->is_executing());
    EXPECT_EQ(event->get_exec_time(), 12);

    event->set_finished(15);
    EXPECT_TRUE(event->is_finished());
    EXPECT_EQ(event->get_finish_time(), 15);

    event->set_ended(18);
    EXPECT_TRUE(event->is_ended());
    EXPECT_EQ(event->get_end_time(), 18);
}

TEST_F(ProcessEventTest, InvalidTransitionsIgnored) {
    event->set_executing(10);
    EXPECT_FALSE(event->is_executing());

    event->set_triggered(10);
    event->set_finished(15);
    EXPECT_FALSE(event->is_finished());
}

TEST_F(ProcessEventTest, TimingStats) {
    event->set_triggered(10);
    event->set_executing(12);
    event->set_finished(15);
    event->set_ended(18);

    auto stats = event->get_timing_stats();
    EXPECT_EQ(stats["trigger_time"], 10);
    EXPECT_EQ(stats["exec_time"], 12);
    EXPECT_EQ(stats["finish_time"], 15);
    EXPECT_EQ(stats["end_time"], 18);
    EXPECT_EQ(stats["total_latency"], 8);
    EXPECT_EQ(stats["trigger_to_exec_latency"], 2);
    EXPECT_EQ(stats["exec_to_finish_latency"], 3);
    EXPECT_EQ(stats["finish_to_end_latency"], 3);
}

TEST_F(ProcessEventTest, Reset) {
    event->set_triggered(10);
    event->set_executing(12);
    event->reset();
    EXPECT_TRUE(event->is_idle());
    EXPECT_EQ(event->get_trigger_time(), 0);
}

TEST_F(ProcessEventTest, GetId) {
    EXPECT_EQ(event->get_process_type(), "test_process");
    EXPECT_EQ(event->get_instance_id(), 42);
}

class ProcessManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        mgr = std::make_unique<ProcessManager>();
    }
    std::unique_ptr<ProcessManager> mgr;
};

TEST_F(ProcessManagerTest, RegisterProcessType) {
    bool result = mgr->register_process_type(
        "compute",
        []() { return true; },
        []() { return true; },
        []() { return true; },
        []() { return true; },
        10
    );
    EXPECT_TRUE(result);
}

TEST_F(ProcessManagerTest, DuplicateRegistrationThrows) {
    mgr->register_process_type("compute", nullptr, nullptr, nullptr, nullptr, 1);
    EXPECT_THROW(
        mgr->register_process_type("compute", nullptr, nullptr, nullptr, nullptr, 1),
        std::runtime_error
    );
}

TEST_F(ProcessManagerTest, CreateActiveEvent) {
    mgr->register_process_type("compute", nullptr, nullptr, nullptr, nullptr, 1);
    auto event = mgr->create_active_event("compute", 100);
    ASSERT_NE(event, nullptr);
    EXPECT_TRUE(event->is_triggered());
    EXPECT_EQ(event->get_trigger_time(), 100);
    EXPECT_EQ(event->get_process_type(), "compute");
    EXPECT_EQ(event->get_instance_id(), 0);
}

TEST_F(ProcessManagerTest, CreateEventForUnregisteredType) {
    auto event = mgr->create_active_event("unknown", 100);
    EXPECT_EQ(event, nullptr);
}

TEST_F(ProcessManagerTest, DriveStateTransitions) {
    int step = 0;
    mgr->register_process_type(
        "compute",
        [&]() { return step == 0; },
        [&]() { return step >= 1; },
        [&]() { return step >= 2; },
        [&]() { return step >= 3; },
        1
    );

    step = 0;
    mgr->drive_state_transitions(0);
    auto events = mgr->get_active_events();
    ASSERT_GE(events.size(), 1);
    EXPECT_TRUE(events[0]->is_triggered());

    step = 1;
    mgr->drive_state_transitions(1);
    EXPECT_TRUE(events[0]->is_executing());

    step = 2;
    mgr->drive_state_transitions(2);
    EXPECT_TRUE(events[0]->is_finished());

    step = 3;
    mgr->drive_state_transitions(3);
    EXPECT_TRUE(events[0]->is_ended());
}

TEST_F(ProcessManagerTest, CleanupMovesEndedEvents) {
    bool triggered = false;
    mgr->register_process_type(
        "compute",
        [&]() { return !triggered; },
        []() { return true; },
        []() { return true; },
        []() { return true; },
        1
    );

    mgr->drive_state_transitions(0);
    triggered = true;
    for (int i = 1; i < 4; i++) {
        mgr->drive_state_transitions(i);
    }

    EXPECT_EQ(mgr->get_active_events().size(), 0);
    EXPECT_EQ(mgr->get_completed_events().size(), 1);
}

TEST_F(ProcessManagerTest, HasActiveEventOfType) {
    mgr->register_process_type(
        "compute",
        []() { return true; },
        []() { return false; },
        []() { return false; },
        []() { return false; },
        1
    );

    mgr->drive_state_transitions(0);
    EXPECT_TRUE(mgr->has_active_event_of_type("compute"));
    EXPECT_FALSE(mgr->has_active_event_of_type("unknown"));
}

TEST_F(ProcessManagerTest, ResetAllEvents) {
    mgr->register_process_type("compute", nullptr, nullptr, nullptr, nullptr, 1);
    mgr->create_active_event("compute", 0);
    mgr->create_active_event("compute", 1);
    EXPECT_EQ(mgr->get_active_events().size(), 2);

    mgr->reset_all_events();
    EXPECT_EQ(mgr->get_active_events().size(), 0);
    EXPECT_EQ(mgr->get_completed_events().size(), 0);
}

TEST_F(ProcessManagerTest, PerformanceStats) {
    int step = 0;
    mgr->register_process_type(
        "compute",
        [&]() { return step == 0; },
        [&]() { return step >= 1; },
        [&]() { return step >= 2; },
        [&]() { return step >= 3; },
        1
    );

    for (step = 0; step < 4; step++) {
        mgr->drive_state_transitions(step);
    }

    auto stats = mgr->get_performance_stats();
    EXPECT_EQ(stats["total_completed_events"], 1);
    EXPECT_GT(stats["total_latency"], 0);
}