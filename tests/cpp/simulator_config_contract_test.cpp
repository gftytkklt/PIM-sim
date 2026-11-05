#include "simulator/ConfigLoader.h"
#include <gtest/gtest.h>

class ConfigContractModule : public ModuleBase {
public:
    using ModuleBase::ModuleBase;
    void register_processes() override {}
    void register_message_handlers() override {}
};

TEST(SimulatorConfigContract, StructuralErrorsDoNotPartiallyRegisterModules) {
    const std::vector<std::string> invalid_configs = {
        R"({"modules":[{"id":"a","type":"valid"},{"id":"a","type":"valid"}]})",
        R"({"modules":[{"id":"a","type":"valid"},{"id":"b","type":"unknown"}]})",
        R"({"modules":[{"id":"a","type":"valid"},{"id":"b","type":"valid","depth":2147483648}]})",
        R"({"modules":[{"id":"a","type":"valid"},{"id":"b","type":"valid","depth":-1}]})",
        R"({"modules":[{"id":"a","type":"valid"}],"connections":[{"src":"a","src_signal":"out","dst":"missing","dst_signal":"in"}]})"
    };
    for (const auto& config : invalid_configs) {
        CycleAccurateSimulator sim;
        SimConfigLoader loader;
        int factory_calls = 0;
        loader.register_factory("valid", [&](ISimulator& engine, const std::string& id, int depth,
                                            const boost::json::object&) {
            ++factory_calls;
            engine.register_module<ConfigContractModule>(id, depth);
        });
        EXPECT_THROW(loader.load(sim, config), std::runtime_error) << config;
        EXPECT_EQ(factory_calls, 0) << config;
        EXPECT_TRUE(sim.get_all_modules().empty());
    }
}

TEST(SimulatorConfigContract, InvalidFactoryIsRejected) {
    SimConfigLoader loader;
    EXPECT_THROW(loader.register_factory("valid", {}), std::invalid_argument);
}
