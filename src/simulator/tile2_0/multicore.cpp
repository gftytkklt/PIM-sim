#include "simulator/tile2_0/multicore.h"
#include "simulator/tile2_0/core_factory.h"

void MulticoreSimulator::Init() {
    const std::array<FmapTask, L1C_BANK> task_list0 = {{
            {1, 8, 8, 128, true}, {1, 4, 4, 128, false},
            {1, 4, 4, 128, false}, {0, 0, 0, 0, false}
        }};
    const std::array<FmapTask, L1C_BANK> task_list1 = {{
            {1, 8, 8, 128, true}, {1, 4, 4, 128, false},
            {0, 0, 0, 0, false}, {0, 0, 0, 0, false}
        }};
    const std::array<FmapTask, L1C_BANK> task_list2 = {{
            {1, 4, 4, 128, true}, {1, 4, 4, 128, false},
            {0, 0, 0, 0, false}, {0, 0, 0, 0, false}
        }};

    create_core_modules(*this, "0", task_list0);
    create_core_modules(*this, "1", task_list1);
    create_core_modules(*this, "2", task_list1);
    create_core_modules(*this, "3", task_list1);
    create_core_modules(*this, "4", task_list2);
    create_core_modules(*this, "5", task_list2);

    register_task_handler<std::tuple<int, std::string>>("task_batch_done",
            [this](const std::tuple<int, std::string>& data) {
                this->handle_batch_task_done(data);
            });

    // 注册生产-消费屏障表项（数据驱动替代硬编码 switch）
    // E1: {ts0.b0=4, ts1.b0=4} → ts4.b0/ts5.b0/ts1.b1/ts2.b1
    register_task_dependency(make_dependency(
        "E1",
        {{"task_scheduler0", 0, 4}, {"task_scheduler1", 0, 4}},
        {{4, 0, 2}, {5, 0, 2}, {1, 1, 2}, {2, 1, 2}}));
    // E2: {ts0.b1=2, ts4.b0=2, ts5.b0=2} → ts4.b1/ts5.b1
    register_task_dependency(make_dependency(
        "E2",
        {{"task_scheduler0", 1, 2}, {"task_scheduler4", 0, 2}, {"task_scheduler5", 0, 2}},
        {{4, 1, 2}, {5, 1, 2}}));
    // E3: {ts1.b1=2, ts2.b1=2, ts3.b1=2} → ts0.b2
    register_task_dependency(make_dependency(
        "E3",
        {{"task_scheduler1", 1, 2}, {"task_scheduler2", 1, 2}, {"task_scheduler3", 1, 2}},
        {{0, 2, 2}}));
    // E4: {ts2.b0=4, ts3.b0=4} → ts0.b1 + ts3.b1 (合并原B4/B5)
    register_task_dependency(make_dependency(
        "E4",
        {{"task_scheduler2", 0, 4}, {"task_scheduler3", 0, 4}},
        {{0, 1, 2}, {3, 1, 2}}));
}

// 构造一个生产-消费屏障表项
// producers: {core_name, bank_id, threshold} 每个生产者携带各自计数阈值
// targets:   {core_id, bank_id, batch_num} 消费者事务列表（→ init_task）
TaskDependencyEntryPtr MulticoreSimulator::make_dependency(
    const std::string& name,
    std::vector<std::tuple<std::string, int, int>> producers,
    std::vector<std::tuple<int, int, int>> targets) {
    // 构建阈值表：core:bank → threshold
    std::unordered_map<std::string, int> thresholds;
    for (const auto& [pcore, pbank, pthr] : producers) {
        thresholds[pcore + ":" + std::to_string(pbank)] = pthr;
    }
    // 消费者事务：触发所有目标 init_task（计数器由框架触发后自动清零）
    auto fire = [this, targets = std::move(targets)]() {
        for (const auto& [core_id, bank_id, batch_num] : targets) {
            init_task(core_id, bank_id, batch_num);
        }
    };
    // 增量提取：从 task_batch_done 消息提取 {core:bank, 1}
    auto extract = [](const GenericMessage& msg) -> std::pair<std::string, int> {
        auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
        int bank = std::get<0>(data);
        const std::string& core = std::get<1>(data);
        return {core + ":" + std::to_string(bank), 1};
    };
    return make_increment_dependency(name, extract, thresholds, fire);
}

void MulticoreSimulator::handle_batch_task_done(const std::tuple<int, std::string>& data) {
    // 泛化：任务完成消息广播到活跃任务表，各表项自行判断/触发/清零
    GenericMessage msg("task_batch_done", data, 0);
    on_task_done(msg);
}