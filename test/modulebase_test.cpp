#include "simulator/ModuleBase.h"
#include <gtest/gtest.h>

class TestModule : public ModuleBase<TestModule> {
public:
    TestModule(const std::string& id) : ModuleBase(id) {
        add_signal(Signal("int_signal", Signal::Direction::INPUT));
        add_signal(Signal("bool_signal", Signal::Direction::INPUT));
        add_signal(Signal("empty_signal", Signal::Direction::INPUT));
    }
    void register_processes() override {}
    void register_message_handlers() override {}
};

class ModuleBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        mod = std::make_shared<TestModule>("test_module");
        mod->set_signal_value("int_signal", 42, 0);
        mod->set_signal_value("bool_signal", true, 0);
    }
    std::shared_ptr<TestModule> mod;
};

TEST_F(ModuleBaseTest, GetSignalAsCorrectType) {
    auto val = mod->get_signal_as<int>("int_signal");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
}

TEST_F(ModuleBaseTest, GetSignalAsWrongType) {
    auto val = mod->get_signal_as<bool>("int_signal");
    EXPECT_FALSE(val.has_value());
}

TEST_F(ModuleBaseTest, GetSignalAsBool) {
    auto val = mod->get_signal_as<bool>("bool_signal");
    ASSERT_TRUE(val.has_value());
    EXPECT_TRUE(*val);
}

TEST_F(ModuleBaseTest, GetSignalAsMissing) {
    auto val = mod->get_signal_as<int>("nonexistent");
    EXPECT_FALSE(val.has_value());
}

TEST_F(ModuleBaseTest, GetSignalAsEmpty) {
    auto val = mod->get_signal_as<int>("empty_signal");
    EXPECT_FALSE(val.has_value());
}

TEST_F(ModuleBaseTest, HasSignal) {
    EXPECT_TRUE(mod->has_signal("int_signal"));
    EXPECT_FALSE(mod->has_signal("nonexistent"));
}

TEST_F(ModuleBaseTest, SetAndGetSignalValue) {
    mod->set_signal_value("int_signal", 100, 0);
    auto val = mod->get_signal_as<int>("int_signal");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 100);
}

TEST_F(ModuleBaseTest, InvalidateSignal) {
    mod->invalidate_signal("bool_signal");
    auto val = mod->get_signal_as<bool>("bool_signal");
    EXPECT_FALSE(val.has_value());
}