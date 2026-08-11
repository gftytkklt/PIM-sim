#include "simulator/TaskDependency.h"
#include "simulator/Simulator.h"
#include <gtest/gtest.h>
#include <tuple>
#include <string>

// 造一个 task_batch_done 消息
static GenericMessage make_batch_done(int bank, const std::string& core) {
    return GenericMessage("task_batch_done", std::make_tuple(bank, core), 0);
}

// 简单屏障：{ts0.b0=4, ts1.b0=4} → 触发消费者计数
TEST(TaskDependencyTest, BarrierFiresWhenAllProducersReachThreshold) {
    int fired = 0;
    auto entry = std::make_shared<TaskDependencyEntry>(
        "E1",
        [](TaskDependencyEntry& self, const GenericMessage& msg) {
            auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
            int bank = std::get<0>(data);
            const std::string& core = std::get<1>(data);
            std::string key = core + ":" + std::to_string(bank);
            if (core == "task_scheduler0" && bank == 0) self.counter(key)++;
            if (core == "task_scheduler1" && bank == 0) self.counter(key)++;
        },
        [](const TaskDependencyEntry& self) {
            return self.counter("task_scheduler0:0") >= 4 &&
                   self.counter("task_scheduler1:0") >= 4;
        },
        [&fired]() { fired++; });

    // 未到阈值：ts0.b0 只发 3 次
    for (int i = 0; i < 3; i++) entry->on_message(make_batch_done(0, "task_scheduler0"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    EXPECT_EQ(fired, 0);  // ts1.b0=1 < 4

    // ts0.b0 再发 1 次（累计4），ts1.b0 补到 4 → 触发
    entry->on_message(make_batch_done(0, "task_scheduler0"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    EXPECT_EQ(fired, 1);
}

TEST(TaskDependencyTest, BarrierResetsAfterFire) {
    int fired = 0;
    auto entry = std::make_shared<TaskDependencyEntry>(
        "E1",
        [](TaskDependencyEntry& self, const GenericMessage& msg) {
            auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
            int bank = std::get<0>(data);
            const std::string& core = std::get<1>(data);
            self.counter(core + ":" + std::to_string(bank))++;
        },
        [](const TaskDependencyEntry& self) {
            return self.counter("task_scheduler0:0") >= 2 &&
                   self.counter("task_scheduler1:0") >= 2;
        },
        [&fired]() { fired++; });

    // 第一轮触发
    entry->on_message(make_batch_done(0, "task_scheduler0"));
    entry->on_message(make_batch_done(0, "task_scheduler0"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    EXPECT_EQ(fired, 1);
    // 触发后计数器清零，第二轮重新累计
    EXPECT_EQ(entry->counter("task_scheduler0:0"), 0);
    EXPECT_EQ(entry->counter("task_scheduler1:0"), 0);
    // 再各发 2 次 → 再次触发
    entry->on_message(make_batch_done(0, "task_scheduler0"));
    entry->on_message(make_batch_done(0, "task_scheduler0"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    EXPECT_EQ(fired, 2);
}

TEST(TaskDependencyTest, TableBroadcastsToAllEntries) {
    TaskDependencyTable table;
    int fired_a = 0, fired_b = 0;
    // A: 对 ts0.b0 消息计数，达到 1 触发
    table.register_entry(std::make_shared<TaskDependencyEntry>(
        "A",
        [](TaskDependencyEntry& self, const GenericMessage& msg) {
            auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
            if (std::get<1>(data) == "task_scheduler0" && std::get<0>(data) == 0)
                self.counter("ts0:0")++;
        },
        [](const TaskDependencyEntry& self) { return self.counter("ts0:0") >= 1; },
        [&fired_a]() { fired_a++; }));
    // B: 对 ts1.b0 消息计数，达到 1 触发
    table.register_entry(std::make_shared<TaskDependencyEntry>(
        "B",
        [](TaskDependencyEntry& self, const GenericMessage& msg) {
            auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
            if (std::get<1>(data) == "task_scheduler1" && std::get<0>(data) == 0)
                self.counter("ts1:0")++;
        },
        [](const TaskDependencyEntry& self) { return self.counter("ts1:0") >= 1; },
        [&fired_b]() { fired_b++; }));

    // 发 ts0.b0 消息 → 只触发 A
    int fired = table.on_task_done(make_batch_done(0, "task_scheduler0"));
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(fired_a, 1);
    EXPECT_EQ(fired_b, 0);

    // 发 ts1.b0 消息 → 只触发 B
    fired = table.on_task_done(make_batch_done(0, "task_scheduler1"));
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(fired_b, 1);
}

TEST(TaskDependencyTest, InactiveEntryIgnored) {
    int fired = 0;
    auto entry = std::make_shared<TaskDependencyEntry>(
        "X",
        [](TaskDependencyEntry& self, const GenericMessage&) {
            self.counter("cnt")++;
        },
        [](const TaskDependencyEntry& self) { return self.counter("cnt") >= 1; },
        [&fired]() { fired++; });
    entry->set_active(false);
    entry->on_message(make_batch_done(0, "task_scheduler0"));
    EXPECT_EQ(fired, 0);
    EXPECT_EQ(entry->counter("cnt"), 0);  // 未激活时不更新
}

// 通过 CycleAccurateSimulator 集成 API 测试
TEST(TaskDependencyTest, SimulatorIntegration) {
    auto sim = std::make_shared<CycleAccurateSimulator>();
    int fired = 0;
    sim->register_task_dependency(std::make_shared<TaskDependencyEntry>(
        "E1",
        [](TaskDependencyEntry& self, const GenericMessage& msg) {
            auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
            self.counter(std::get<1>(data) + ":" + std::to_string(std::get<0>(data)))++;
        },
        [](const TaskDependencyEntry& self) {
            return self.counter("task_scheduler0:0") >= 2 &&
                   self.counter("task_scheduler1:0") >= 2;
        },
        [&fired]() { fired++; }));

    // 通过模拟器 API 驱动（模拟 task_batch_done 到达）
    sim->on_task_done(make_batch_done(0, "task_scheduler0"));
    sim->on_task_done(make_batch_done(0, "task_scheduler0"));
    sim->on_task_done(make_batch_done(0, "task_scheduler1"));
    sim->on_task_done(make_batch_done(0, "task_scheduler1"));
    EXPECT_EQ(fired, 1);
}
// 通用场景 1：不同生产者可有不同计数阈值
TEST(TaskDependencyTest, DifferentThresholdsPerProducer) {
    int fired = 0;
    auto entry = std::make_shared<TaskDependencyEntry>(
        "E1",
        [](TaskDependencyEntry& self, const GenericMessage& msg) {
            auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
            int bank = std::get<0>(data);
            const std::string& core = std::get<1>(data);
            self.counter(core + ":" + std::to_string(bank))++;
        },
        // ts0.b0 需 4 次，ts1.b0 需 2 次 —— 不同阈值
        [](const TaskDependencyEntry& self) {
            return self.counter("task_scheduler0:0") >= 4 &&
                   self.counter("task_scheduler1:0") >= 2;
        },
        [&fired]() { fired++; });

    // ts1.b0 发 2 次（达标），ts0.b0 发 3 次（未达 4）→ 不触发
    for (int i = 0; i < 2; i++) entry->on_message(make_batch_done(0, "task_scheduler1"));
    for (int i = 0; i < 3; i++) entry->on_message(make_batch_done(0, "task_scheduler0"));
    EXPECT_EQ(fired, 0);

    // ts0.b0 补 1 次（到 4）→ 触发
    entry->on_message(make_batch_done(0, "task_scheduler0"));
    EXPECT_EQ(fired, 1);
}

// 通用场景 2：单个生产者可同时触发多个消费者（多个表项独立判断）
TEST(TaskDependencyTest, SingleProducerFiresMultipleConsumers) {
    TaskDependencyTable table;
    int fired_a = 0, fired_b = 0;
    // 消费者 A：ts0.b0 达 2 次触发
    table.register_entry(std::make_shared<TaskDependencyEntry>(
        "A",
        [](TaskDependencyEntry& self, const GenericMessage& msg) {
            auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
            if (std::get<1>(data) == "task_scheduler0" && std::get<0>(data) == 0)
                self.counter("ts0:0")++;
        },
        [](const TaskDependencyEntry& self) { return self.counter("ts0:0") >= 2; },
        [&fired_a]() { fired_a++; }));
    // 消费者 B：ts0.b0 达 3 次触发（不同阈值，同生产者）
    table.register_entry(std::make_shared<TaskDependencyEntry>(
        "B",
        [](TaskDependencyEntry& self, const GenericMessage& msg) {
            auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
            if (std::get<1>(data) == "task_scheduler0" && std::get<0>(data) == 0)
                self.counter("ts0:0")++;
        },
        [](const TaskDependencyEntry& self) { return self.counter("ts0:0") >= 3; },
        [&fired_b]() { fired_b++; }));

    // 同一生产者 ts0.b0 依次发 3 次消息
    table.on_task_done(make_batch_done(0, "task_scheduler0"));  // A=1,B=1
    EXPECT_EQ(fired_a, 0); EXPECT_EQ(fired_b, 0);
    table.on_task_done(make_batch_done(0, "task_scheduler0"));  // A=2,B=2 → A 触发
    EXPECT_EQ(fired_a, 1); EXPECT_EQ(fired_b, 0);
    table.on_task_done(make_batch_done(0, "task_scheduler0"));  // A 已清零, B=3 → B 触发
    EXPECT_EQ(fired_a, 1); EXPECT_EQ(fired_b, 1);
}

// 通用场景 3：同一屏障的同一生产者消息，触发后计数清零可再次累计
TEST(TaskDependencyTest, MixedThresholdsResetsAfterFire) {
    int fired = 0;
    auto entry = std::make_shared<TaskDependencyEntry>(
        "E1",
        [](TaskDependencyEntry& self, const GenericMessage& msg) {
            auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
            self.counter(std::get<1>(data) + ":" + std::to_string(std::get<0>(data)))++;
        },
        [](const TaskDependencyEntry& self) {
            return self.counter("task_scheduler0:0") >= 2 &&
                   self.counter("task_scheduler1:0") >= 3;   // 不同阈值
        },
        [&fired]() { fired++; });

    // 第一轮：ts0.b0×2 + ts1.b0×3 → 触发
    entry->on_message(make_batch_done(0, "task_scheduler0"));
    entry->on_message(make_batch_done(0, "task_scheduler0"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    EXPECT_EQ(fired, 1);
    EXPECT_EQ(entry->counter("task_scheduler0:0"), 0);  // 已清零
    EXPECT_EQ(entry->counter("task_scheduler1:0"), 0);

    // 第二轮：重新累计到各自阈值 → 再次触发
    entry->on_message(make_batch_done(0, "task_scheduler0"));
    entry->on_message(make_batch_done(0, "task_scheduler0"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    entry->on_message(make_batch_done(0, "task_scheduler1"));
    EXPECT_EQ(fired, 2);
}
