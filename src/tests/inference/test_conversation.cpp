#include <gtest/gtest.h>
#include "../inference/src/conversation.hpp"
#include "../inference/src/message.hpp"

class ConversationTests : public ::testing::Test {
protected:
    Conversation conversation;
    
    void SetUp() override {
        conversation = Conversation();
    }
    
    void TearDown() override {}
};

TEST_F(ConversationTests, InitialState) {
    EXPECT_TRUE(conversation.empty());
    EXPECT_EQ(conversation.size(), 0);
    EXPECT_FALSE(conversation.hasSystemPrompt());
    EXPECT_EQ(conversation.getSystemPrompt(), "");
}

TEST_F(ConversationTests, SetSystemPrompt) {
    conversation.setSystemPrompt("You are a helpful assistant");
    EXPECT_TRUE(conversation.hasSystemPrompt());
    EXPECT_EQ(conversation.getSystemPrompt(), "You are a helpful assistant");
}

TEST_F(ConversationTests, AddSingleMessage) {
    auto msg = Message::user("Hello");
    conversation.addMessage(msg);
    
    EXPECT_FALSE(conversation.empty());
    EXPECT_EQ(conversation.size(), 1);
    EXPECT_EQ(conversation.getHistory()[0].content, "Hello");
    EXPECT_EQ(conversation.getHistory()[0].role, Message::User);
}

TEST_F(ConversationTests, AddMultipleMessages) {
    conversation.addMessage(Message::user("Question"));
    conversation.addMessage(Message::assistant("Answer"));
    conversation.addMessage(Message::user("Follow-up"));
    
    EXPECT_EQ(conversation.size(), 3);
    auto history = conversation.getHistory();
    EXPECT_EQ(history[0].role, Message::User);
    EXPECT_EQ(history[1].role, Message::Assistant);
    EXPECT_EQ(history[2].role, Message::User);
}

TEST_F(ConversationTests, ClearConversation) {
    conversation.setSystemPrompt("System");
    conversation.addMessage(Message::user("Hello"));
    conversation.addMessage(Message::assistant("Hi"));
    
    conversation.clear();
    
    EXPECT_TRUE(conversation.empty());
    EXPECT_EQ(conversation.size(), 0);
    // System prompt should persist after clear
    EXPECT_TRUE(conversation.hasSystemPrompt());
    EXPECT_EQ(conversation.getSystemPrompt(), "System");
}

TEST_F(ConversationTests, GetLastExchangeSimple) {
    conversation.addMessage(Message::user("First question"));
    conversation.addMessage(Message::assistant("First answer"));
    conversation.addMessage(Message::user("Second question"));
    conversation.addMessage(Message::assistant("Second answer"));
    
    auto exchange = conversation.getLastExchange();
    EXPECT_EQ(exchange.size(), 2);
    EXPECT_EQ(exchange[0].content, "Second question");
    EXPECT_EQ(exchange[1].content, "Second answer");
}

TEST_F(ConversationTests, GetLastExchangeOnlyUser) {
    conversation.addMessage(Message::user("Only question"));
    
    auto exchange = conversation.getLastExchange();
    EXPECT_EQ(exchange.size(), 1);
    EXPECT_EQ(exchange[0].content, "Only question");
}

TEST_F(ConversationTests, GetLastExchangeEmpty) {
    auto exchange = conversation.getLastExchange();
    EXPECT_TRUE(exchange.empty());
}

TEST_F(ConversationTests, FormatForModelEmpty) {
    std::string formatted = conversation.formatForModel();
    EXPECT_EQ(formatted, "<|assistant|>\n");
}

TEST_F(ConversationTests, FormatForModelWithSystemPrompt) {
    conversation.setSystemPrompt("Be helpful");
    std::string formatted = conversation.formatForModel();
    
    EXPECT_NE(formatted.find("<|system|>\nBe helpful<|end|>\n"), std::string::npos);
    EXPECT_NE(formatted.find("<|assistant|>\n"), std::string::npos);
}

TEST_F(ConversationTests, FormatForModelFullConversation) {
    conversation.setSystemPrompt("System prompt");
    conversation.addMessage(Message::user("Hello"));
    conversation.addMessage(Message::assistant("Hi there"));
    conversation.addMessage(Message::user("How are you?"));
    
    std::string formatted = conversation.formatForModel();
    
    // Check all parts are present in order
    EXPECT_NE(formatted.find("<|system|>\nSystem prompt<|end|>\n"), std::string::npos);
    EXPECT_NE(formatted.find("<|user|>\nHello<|end|>\n"), std::string::npos);
    EXPECT_NE(formatted.find("<|assistant|>\nHi there<|end|>\n"), std::string::npos);
    EXPECT_NE(formatted.find("<|user|>\nHow are you?<|end|>\n"), std::string::npos);
    
    // Should end with assistant prompt
    EXPECT_EQ(formatted.substr(formatted.length() - 14), "<|assistant|>\n");
}

TEST_F(ConversationTests, FormatLastUserMessage) {
    conversation.addMessage(Message::user("First"));
    conversation.addMessage(Message::assistant("Response"));
    conversation.addMessage(Message::user("Last"));
    
    EXPECT_EQ(conversation.formatLastUserMessage(), "Last");
}

TEST_F(ConversationTests, FormatLastUserMessageNoUser) {
    conversation.addMessage(Message::assistant("Only assistant"));
    EXPECT_EQ(conversation.formatLastUserMessage(), "");
}

TEST_F(ConversationTests, SystemMessageInHistory) {
    conversation.addMessage(Message::system("System in history"));
    conversation.addMessage(Message::user("User"));
    
    std::string formatted = conversation.formatForModel();
    EXPECT_NE(formatted.find("<|system|>\nSystem in history<|end|>\n"), std::string::npos);
}

TEST_F(ConversationTests, MultilineContent) {
    std::string multiline = "Line 1\nLine 2\nLine 3";
    conversation.addMessage(Message::user(multiline));
    
    auto history = conversation.getHistory();
    EXPECT_EQ(history[0].content, multiline);
    
    std::string formatted = conversation.formatForModel();
    EXPECT_NE(formatted.find(multiline), std::string::npos);
}

TEST_F(ConversationTests, SpecialCharactersInContent) {
    std::string special = "Special <|characters|> & symbols";
    conversation.addMessage(Message::user(special));
    
    auto history = conversation.getHistory();
    EXPECT_EQ(history[0].content, special);
}