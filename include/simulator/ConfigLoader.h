#ifndef SIMULATOR_CONFIG_LOADER_H
#define SIMULATOR_CONFIG_LOADER_H

#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include "Simulator.h"
#include <boost/json.hpp>

/**
 * 配置驱动模块图加载器
 *
 * 从 JSON 配置构建模拟器模块图（模块实例化 + 信号连接），
 * 替代硬编码 C++ 代码。JSON schema 结构：
 *
 * {
 *   "modules": [
 *     { "id": "simd0", "type": "SIMD", "depth": 4 },
 *     { "id": "task_scheduler0", "type": "TaskScheduler", "depth": 2,
 *       "params": { "tasks": [ { "block_num":1,"row":8,"col":8,
 *                                "channel_num":128,"pooling":true }, ... ] } },
 *     ...
 *   ],
 *   "connections": [
 *     { "src": "task_scheduler0", "src_signal": "cache_read_trigger",
 *       "dst": "L1_cache0", "dst_signal": "cache_read_trigger" },
 *     ...
 *   ]
 * }
 *
 * 使用方式：
 *   1. 用户通过 register_factory("SIMD", factory_lambda) 注册模块类型工厂
 *   2. SimConfigLoader::load(sim, json) 解析配置并构建模块图
 *
 * 模块工厂 lambda 签名：
 *   void(ISimulator& sim, const std::string& id,
 *        int depth, const boost::json::object& params)
 * 工厂内部调用 sim.template register_module<ConcreteType>(id, depth, args...)
 * 以保持类型安全并自动触发 register_processes/register_message_handlers。
 */
class SimConfigLoader {
public:
    using ModuleFactory = std::function<void(
        ISimulator&, const std::string& id, int depth,
        const boost::json::object& params)>;

    // 注册模块类型工厂
    void register_factory(const std::string& type_name, ModuleFactory factory) {
        factories_[type_name] = std::move(factory);
    }

    // 从 JSON 配置构建模块图
    void load(ISimulator& sim, const boost::json::value& config) {
        const auto& obj = config.as_object();
        load_modules(sim, obj);
        load_connections(sim, obj);
    }

    void load(ISimulator& sim, const std::string& json_str) {
        auto config = boost::json::parse(json_str);
        load(sim, config);
    }

private:
    void load_modules(ISimulator& sim, const boost::json::object& obj) {
        if (!obj.contains("modules")) {
            throw std::runtime_error("SimConfigLoader: missing 'modules' array");
        }
        for (const auto& mod_val : obj.at("modules").as_array()) {
            const auto& mod = mod_val.as_object();
            std::string id = mod.at("id").as_string().c_str();
            std::string type = mod.at("type").as_string().c_str();
            int depth = mod.contains("depth") ? mod.at("depth").as_int64() : 0;
            boost::json::object params;
            if (mod.contains("params")) {
                params = mod.at("params").as_object();
            }
            auto it = factories_.find(type);
            if (it == factories_.end()) {
                throw std::runtime_error("SimConfigLoader: unknown module type '" + type + "' for '" + id + "'");
            }
            it->second(sim, id, depth, params);
        }
    }

    void load_connections(ISimulator& sim, const boost::json::object& obj) {
        if (!obj.contains("connections")) {
            return; // connections 可选
        }
        for (const auto& conn_val : obj.at("connections").as_array()) {
            const auto& conn = conn_val.as_object();
            sim.connect_modules(
                conn.at("src").as_string().c_str(),
                conn.at("src_signal").as_string().c_str(),
                conn.at("dst").as_string().c_str(),
                conn.at("dst_signal").as_string().c_str());
        }
    }

    std::unordered_map<std::string, ModuleFactory> factories_;
};

#endif // SIMULATOR_CONFIG_LOADER_H