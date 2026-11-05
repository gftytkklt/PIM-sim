#ifndef SIMULATOR_CONFIG_LOADER_H
#define SIMULATOR_CONFIG_LOADER_H

#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <limits>
#include <set>
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
        if (type_name.empty() || !factory) throw std::invalid_argument("Module factory needs a non-empty type and callable");
        factories_[type_name] = std::move(factory);
    }

    // 从 JSON 配置构建模块图
    void load(ISimulator& sim, const boost::json::value& config) {
        const auto& obj = config.as_object();
        validate_structure(sim, obj);
        load_modules(sim, obj);
        load_connections(sim, obj);
    }

    void load(ISimulator& sim, const std::string& json_str) {
        auto config = boost::json::parse(json_str);
        load(sim, config);
    }

private:
    // 在调用用户工厂前检查框架字段。工厂自身的参数验证/异常不支持回滚；
    // load 失败后应丢弃该模拟器并重新构造，不能继续运行部分图。
    void validate_structure(const ISimulator& sim, const boost::json::object& obj) const {
        if (!obj.contains("modules") || !obj.at("modules").is_array()) {
            throw std::runtime_error("SimConfigLoader: missing or invalid 'modules' array");
        }
        std::set<std::string> ids;
        for (const auto& module : sim.get_all_modules()) ids.insert(module->get_id());
        for (const auto& value : obj.at("modules").as_array()) {
            const auto& module = value.as_object();
            const auto id = std::string(module.at("id").as_string());
            const auto type = std::string(module.at("type").as_string());
            if (id.empty() || !ids.insert(id).second) throw std::runtime_error("SimConfigLoader: empty or duplicate module id: " + id);
            if (!factories_.count(type)) throw std::runtime_error("SimConfigLoader: unknown module type: " + type);
            if (module.contains("depth")) {
                const auto& depth = module.at("depth");
                if (!depth.is_int64() || depth.as_int64() < 0 || depth.as_int64() > std::numeric_limits<int>::max()) {
                    throw std::runtime_error("SimConfigLoader: depth must be a nonnegative int");
                }
            }
            if (module.contains("params") && !module.at("params").is_object()) {
                throw std::runtime_error("SimConfigLoader: params must be an object");
            }
        }
        if (obj.contains("connections")) {
            std::set<std::pair<std::string, std::string>> destinations;
            for (const auto& value : obj.at("connections").as_array()) {
                const auto& connection = value.as_object();
                const auto src = std::string(connection.at("src").as_string());
                const auto dst = std::string(connection.at("dst").as_string());
                const auto source_signal = std::string(connection.at("src_signal").as_string());
                const auto target_signal = std::string(connection.at("dst_signal").as_string());
                if (!ids.count(src) || !ids.count(dst) || source_signal.empty() || target_signal.empty()) {
                    throw std::runtime_error("SimConfigLoader: invalid connection endpoint");
                }
                if (!destinations.emplace(dst, target_signal).second) {
                    throw std::runtime_error("SimConfigLoader: multiple drivers for " + dst + "." + target_signal);
                }
            }
        }
    }

    void load_modules(ISimulator& sim, const boost::json::object& obj) {
        if (!obj.contains("modules")) {
            throw std::runtime_error("SimConfigLoader: missing 'modules' array");
        }
        for (const auto& mod_val : obj.at("modules").as_array()) {
            const auto& mod = mod_val.as_object();
            std::string id(mod.at("id").as_string());
            std::string type(mod.at("type").as_string());
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
                std::string(conn.at("src").as_string()),
                std::string(conn.at("src_signal").as_string()),
                std::string(conn.at("dst").as_string()),
                std::string(conn.at("dst_signal").as_string()));
        }
    }

    std::unordered_map<std::string, ModuleFactory> factories_;
};

#endif // SIMULATOR_CONFIG_LOADER_H
