#ifndef TASK_DEPENDENCY_H
#define TASK_DEPENDENCY_H

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include "MessageBase.h"

/**
 * 任务依赖原语（多核事务机制）
 *
 * 框架只约定表项结构，生产-消费屏障由用户建模时自定义：
 *   - ProducerHook：任务完成消息 → 更新表项状态（计数器等）
 *   - ConditionCheck：状态是否满足触发条件
 *   - ConsumerAction：条件满足时执行的消费者事务（触发下游等）
 *
 * 每个表项代表一个生产-消费屏障。某个生产者的输出到达时，
 * 由 TaskDependencyTable 广播到所有活跃表项，各表项自行判断
 * 是否相关并更新状态/判断触发。
 */

// 任务表项：一个生产-消费屏障
class TaskDependencyEntry {
public:
    using ProducerHook = std::function<void(TaskDependencyEntry&, const GenericMessage&)>;
    using ConditionCheck = std::function<bool(const TaskDependencyEntry&)>;
    using ConsumerAction = std::function<void()>;

    TaskDependencyEntry(std::string name, ProducerHook on_msg,
                        ConditionCheck ready, ConsumerAction fire)
        : name_(std::move(name)), producer_hook_(std::move(on_msg)),
          ready_(std::move(ready)), fire_(std::move(fire)) {}

    const std::string& get_name() const { return name_; }

    // 某个生产者输出到达：更新状态 + 判断触发
    // 返回是否触发了消费者事务（供测试/日志）
    bool on_message(const GenericMessage& msg) {
        if (!active_) return false;
        if (producer_hook_) producer_hook_(*this, msg);
        if (ready_ && ready_(*this)) {
            if (fire_) fire_();
            // 屏障触发后清零计数器，准备下一轮
            counters_.clear();
            return true;
        }
        return false;
    }

    bool is_active() const { return active_; }
    void set_active(bool active) { active_ = active; }

    // 表项可携带的通用计数状态（用户 ProducerHook/ConditionCheck 可访问）
    // key: 如 "core_name:bank_id"
    int& counter(const std::string& key) { return counters_[key]; }
    int counter(const std::string& key) const {
        auto it = counters_.find(key);
        return it != counters_.end() ? it->second : 0;
    }
    void reset_counter(const std::string& key) { counters_[key] = 0; }
    void reset_all_counters() { counters_.clear(); }

private:
    std::string name_;
    ProducerHook producer_hook_;
    ConditionCheck ready_;
    ConsumerAction fire_;
    bool active_{true};
    std::unordered_map<std::string, int> counters_;
};

using TaskDependencyEntryPtr = std::shared_ptr<TaskDependencyEntry>;

/**
 * 活跃任务表：维护所有生产-消费屏障表项。
 * 某个生产者的任务完成消息到达时，广播到所有活跃表项，
 * 驱动各自的状态更新与消费者事务触发判断。
 */
class TaskDependencyTable {
public:
    // 注册表项
    void register_entry(TaskDependencyEntryPtr entry) {
        entries_.push_back(std::move(entry));
    }

    // 某个生产者的输出到达：驱动所有相关表项的状态更新与触发判断
    // 返回触发了消费者事务的表项数（供测试）
    int on_task_done(const GenericMessage& msg) {
        int fired = 0;
        for (auto& entry : entries_) {
            if (entry->on_message(msg)) fired++;
        }
        return fired;
    }

    const std::vector<TaskDependencyEntryPtr>& get_entries() const { return entries_; }

private:
    std::vector<TaskDependencyEntryPtr> entries_;
};

#endif // TASK_DEPENDENCY_H