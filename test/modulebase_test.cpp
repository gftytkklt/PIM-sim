#include "simulator/ModuleBase.h"
#include <gtest/gtest.h>

class TestModule : public ModuleBase {
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

TEST_F(ModuleBaseTest, SignalDeclarations) {
    auto decls = mod->get_signal_declarations();
    ASSERT_EQ(decls.size(), 3);
    bool found_int = false, found_bool = false, found_empty = false;
    for (const auto& [name, dir] : decls) {
        EXPECT_EQ(dir, Signal::Direction::INPUT);
        if (name == "int_signal") found_int = true;
        if (name == "bool_signal") found_bool = true;
        if (name == "empty_signal") found_empty = true;
    }
    EXPECT_TRUE(found_int && found_bool && found_empty);
}

TEST_F(ModuleBaseTest, SignalValueTypeRegistered) {
    // 声明时无值，类型为 void
    EXPECT_EQ(mod->get_signal_value_type("int_signal"), typeid(void));
    EXPECT_EQ(mod->get_signal_value_type("bool_signal"), typeid(void));
    EXPECT_EQ(mod->get_signal_value_type("empty_signal"), typeid(void));
}

TEST_F(ModuleBaseTest, SignalDirection) {
    EXPECT_EQ(mod->get_signal_direction("int_signal"), Signal::Direction::INPUT);
    EXPECT_EQ(mod->get_signal_direction("bool_signal"), Signal::Direction::INPUT);
}

// 带类型信息的信号声明
class TypedSignalModule : public ModuleBase {
public:
    TypedSignalModule(const std::string& id) : ModuleBase(id) {
        add_signal(Signal("int_out", Signal::Direction::OUTPUT, 0));     // int 类型
        add_signal(Signal("bool_in", Signal::Direction::INPUT, false));  // bool 类型
        add_signal(Signal("any_out", Signal::Direction::OUTPUT, std::make_any<int>(42))); // any 包裹 int
    }
    void register_processes() override {}
    void register_message_handlers() override {}
};

TEST(TypedSignalTest, TypeDeductionFromValue) {
    auto mod = std::make_shared<TypedSignalModule>("typed");
    // 直接值推导类型
    EXPECT_EQ(mod->get_signal_value_type("int_out"), typeid(int));
    EXPECT_EQ(mod->get_signal_value_type("bool_in"), typeid(bool));
    // std::any 包裹时取内部实际类型
    EXPECT_EQ(mod->get_signal_value_type("any_out"), typeid(int));
}

TEST(TypedSignalTest, TypeValidationOnSubmit) {
    auto mod = std::make_shared<TypedSignalModule>("typed");
    // 正确类型：int
    EXPECT_NO_THROW(mod->submit_signal_value("int_out", 10, 1));
    // 错误类型：bool → 应抛异常
    EXPECT_THROW(mod->submit_signal_value("int_out", true, 1), std::runtime_error);
}

// ========== 消息类型系统测试 ==========
// 用户自定义消息类型
struct UserMessage {
    int id;
    std::string name;
};

class MessageModule : public ModuleBase {
public:
    MessageModule(const std::string& id) : ModuleBase(id) {}
    void register_processes() override {}
    void register_message_handlers() override {}

    // 类型化注册：handler 接收 const UserMessage&，框架登记类型
    void setup() {
        register_message_handler<UserMessage>("user_msg",
            [this](const UserMessage& m) { last_msg_ = m; });
    }

    UserMessage last_msg_{0, ""};
};

TEST(MessageTypeTest, TypedRegistrationAndDispatch) {
    auto mod = std::make_shared<MessageModule>("msg_mod");
    mod->setup();

    // 类型化发送：正确类型正常分发
    EXPECT_NO_THROW(
        mod->handle_message(GenericMessage("user_msg", UserMessage{42, "hello"})));
    EXPECT_EQ(mod->last_msg_.id, 42);
    EXPECT_EQ(mod->last_msg_.name, "hello");

    // 消息类型登记正确
    EXPECT_EQ(mod->get_message_type("user_msg"), typeid(UserMessage));
}

TEST(MessageTypeTest, TypeMismatchThrows) {
    auto mod = std::make_shared<MessageModule>("msg_mod");
    mod->setup();

    // 错误类型：int 而非 UserMessage → 应抛异常
    EXPECT_THROW(
        mod->handle_message(GenericMessage("user_msg", 123)),
        std::runtime_error);

    // 发送侧校验：submit_message 类型不匹配也抛异常
    EXPECT_THROW(
        mod->submit_message("user_msg", std::string("wrong")),
        std::runtime_error);
}

TEST(MessageTypeTest, UntypedMessageStillWorks) {
    auto mod = std::make_shared<MessageModule>("msg_mod");
    // 未登记类型，通用消息仍可分发（向后兼容）
    bool received = false;
    mod->register_message_handler("plain_msg", [&received](const GenericMessage&) {
        received = true;
    });
    EXPECT_NO_THROW(mod->handle_message(GenericMessage("plain_msg", 7)));
    EXPECT_TRUE(received);
}