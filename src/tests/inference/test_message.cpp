#include <gtest/gtest.h>
#include "../inference/src/message.hpp"
#include <chrono>
#include <thread>

class MessageTests : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MessageTests, CreateSystemMessage) {
    auto msg = Message::system("System prompt");
    EXPECT_EQ(msg.role, Message::System);
    EXPECT_EQ(msg.content, "System prompt");
    EXPECT_EQ(msg.roleToString(), "system");
}

TEST_F(MessageTests, CreateUserMessage) {
    auto msg = Message::user("User input");
    EXPECT_EQ(msg.role, Message::User);
    EXPECT_EQ(msg.content, "User input");
    EXPECT_EQ(msg.roleToString(), "user");
}

TEST_F(MessageTests, CreateAssistantMessage) {
    auto msg = Message::assistant("Assistant response");
    EXPECT_EQ(msg.role, Message::Assistant);
    EXPECT_EQ(msg.content, "Assistant response");
    EXPECT_EQ(msg.roleToString(), "assistant");
}

TEST_F(MessageTests, EmptyContent) {
    auto msg = Message::user("");
    EXPECT_EQ(msg.role, Message::User);
    EXPECT_EQ(msg.content, "");
    EXPECT_TRUE(msg.content.empty());
}

TEST_F(MessageTests, LongContent) {
    std::string long_content(10000, 'a');
    auto msg = Message::assistant(long_content);
    EXPECT_EQ(msg.role, Message::Assistant);
    EXPECT_EQ(msg.content, long_content);
    EXPECT_EQ(msg.content.length(), 10000);
}

TEST_F(MessageTests, SpecialCharacters) {
    std::string special = "Hello\nWorld\t\"Quotes\" 'Apostrophe' \\Backslash\\";
    auto msg = Message::user(special);
    EXPECT_EQ(msg.content, special);
}

TEST_F(MessageTests, TimestampOrdering) {
    auto msg1 = Message::user("First");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto msg2 = Message::user("Second");
    
    EXPECT_LT(msg1.timestamp, msg2.timestamp);
}

TEST_F(MessageTests, DirectConstruction) {
    Message msg(Message::System, "Direct construction");
    EXPECT_EQ(msg.role, Message::System);
    EXPECT_EQ(msg.content, "Direct construction");
    EXPECT_EQ(msg.roleToString(), "system");
}

TEST_F(MessageTests, RoleToStringAllRoles) {
    Message sys(Message::System, "test");
    Message usr(Message::User, "test");
    Message ast(Message::Assistant, "test");
    
    EXPECT_EQ(sys.roleToString(), "system");
    EXPECT_EQ(usr.roleToString(), "user");
    EXPECT_EQ(ast.roleToString(), "assistant");
}