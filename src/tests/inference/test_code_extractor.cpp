#include <gtest/gtest.h>
#include "../inference/src/code_extractor.hpp"
#include "../inference/src/conversation.hpp"
#include "../inference/src/message.hpp"

class CodeExtractorTests : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CodeExtractorTests, ExtractSimpleCodeBlock) {
    std::string text = "Here is code:\n```\nprint('hello')\n```\nDone.";
    auto blocks = CodeExtractor::extractFromText(text);
    
    EXPECT_EQ(blocks.size(), 1);
    EXPECT_EQ(blocks[0].code, "print('hello')");
    EXPECT_EQ(blocks[0].language, "");
}

TEST_F(CodeExtractorTests, ExtractCodeBlockWithLanguage) {
    std::string text = "Lua code:\n```lua\nfunction test()\n  return 42\nend\n```";
    auto blocks = CodeExtractor::extractFromText(text);
    
    EXPECT_EQ(blocks.size(), 1);
    EXPECT_EQ(blocks[0].language, "lua");
    EXPECT_EQ(blocks[0].code, "function test()\n  return 42\nend");
}

TEST_F(CodeExtractorTests, ExtractMultipleCodeBlocks) {
    std::string text = "First:\n```python\nprint('py')\n```\n"
                      "Second:\n```lua\nprint('lua')\n```\n"
                      "Third:\n```\ngeneric\n```";
    auto blocks = CodeExtractor::extractFromText(text);
    
    EXPECT_EQ(blocks.size(), 3);
    EXPECT_EQ(blocks[0].language, "python");
    EXPECT_EQ(blocks[0].code, "print('py')");
    EXPECT_EQ(blocks[1].language, "lua");
    EXPECT_EQ(blocks[1].code, "print('lua')");
    EXPECT_EQ(blocks[2].language, "");
    EXPECT_EQ(blocks[2].code, "generic");
}

TEST_F(CodeExtractorTests, NoCodeBlocks) {
    std::string text = "This text has no code blocks at all.";
    auto blocks = CodeExtractor::extractFromText(text);
    
    EXPECT_TRUE(blocks.empty());
}

TEST_F(CodeExtractorTests, EmptyCodeBlock) {
    std::string text = "Empty:\n```\n\n```";
    auto blocks = CodeExtractor::extractFromText(text);
    
    EXPECT_EQ(blocks.size(), 1);
    EXPECT_EQ(blocks[0].code, "");
}

TEST_F(CodeExtractorTests, CodeBlockPositions) {
    std::string text = "Start\n```\ncode\n```\nEnd";
    auto blocks = CodeExtractor::extractFromText(text);
    
    EXPECT_EQ(blocks.size(), 1);
    EXPECT_GT(blocks[0].start_pos, 0);
    EXPECT_GT(blocks[0].end_pos, blocks[0].start_pos);
}

TEST_F(CodeExtractorTests, ExtractFromLastResponse) {
    Conversation conv;
    conv.addMessage(Message::user("Write code"));
    conv.addMessage(Message::assistant("Here:\n```lua\nlocal x = 1\n```"));
    conv.addMessage(Message::user("More"));
    conv.addMessage(Message::assistant("Another:\n```lua\nlocal y = 2\n```"));
    
    auto blocks = CodeExtractor::extractFromLastResponse(conv);
    
    EXPECT_EQ(blocks.size(), 1);
    EXPECT_EQ(blocks[0].code, "local y = 2");
}

TEST_F(CodeExtractorTests, ExtractFromLastResponseNoAssistant) {
    Conversation conv;
    conv.addMessage(Message::user("Only user messages"));
    
    auto blocks = CodeExtractor::extractFromLastResponse(conv);
    EXPECT_TRUE(blocks.empty());
}

TEST_F(CodeExtractorTests, ExtractFromConversation) {
    Conversation conv;
    conv.addMessage(Message::user("Write code"));
    conv.addMessage(Message::assistant("First:\n```lua\ncode1\n```"));
    conv.addMessage(Message::user("More"));
    conv.addMessage(Message::assistant("Second:\n```python\ncode2\n```"));
    
    auto blocks = CodeExtractor::extractFromConversation(conv);
    
    EXPECT_EQ(blocks.size(), 2);
    EXPECT_EQ(blocks[0].language, "lua");
    EXPECT_EQ(blocks[0].code, "code1");
    EXPECT_EQ(blocks[1].language, "python");
    EXPECT_EQ(blocks[1].code, "code2");
}

TEST_F(CodeExtractorTests, FilterByLanguage) {
    std::vector<CodeExtractor::CodeBlock> blocks;
    
    CodeExtractor::CodeBlock lua_block;
    lua_block.language = "lua";
    lua_block.code = "lua code";
    blocks.push_back(lua_block);
    
    CodeExtractor::CodeBlock python_block;
    python_block.language = "python";
    python_block.code = "python code";
    blocks.push_back(python_block);
    
    CodeExtractor::CodeBlock lua_block2;
    lua_block2.language = "Lua";  // Different case
    lua_block2.code = "more lua";
    blocks.push_back(lua_block2);
    
    auto lua_only = CodeExtractor::filterByLanguage(blocks, "lua");
    
    EXPECT_EQ(lua_only.size(), 2);
    EXPECT_EQ(lua_only[0].code, "lua code");
    EXPECT_EQ(lua_only[1].code, "more lua");
}

TEST_F(CodeExtractorTests, FilterByLanguageNoMatches) {
    std::vector<CodeExtractor::CodeBlock> blocks;
    
    CodeExtractor::CodeBlock block;
    block.language = "python";
    block.code = "code";
    blocks.push_back(block);
    
    auto rust_only = CodeExtractor::filterByLanguage(blocks, "rust");
    EXPECT_TRUE(rust_only.empty());
}

TEST_F(CodeExtractorTests, ComplexCodeBlock) {
    std::string text = "Complex:\n```lua\n"
                      "-- Comment\n"
                      "function complex(a, b)\n"
                      "  if a > b then\n"
                      "    return a * 2\n"
                      "  else\n"
                      "    return b / 2\n"
                      "  end\n"
                      "end\n"
                      "```";
    
    auto blocks = CodeExtractor::extractFromText(text);
    
    EXPECT_EQ(blocks.size(), 1);
    EXPECT_EQ(blocks[0].language, "lua");
    EXPECT_NE(blocks[0].code.find("function complex"), std::string::npos);
    EXPECT_NE(blocks[0].code.find("return a * 2"), std::string::npos);
}

TEST_F(CodeExtractorTests, MultilineWithSpecialChars) {
    std::string text = "Code:\n```\n"
                      "print(\"Hello\\nWorld\")\n"
                      "x = 'It\\'s working'\n"
                      "-- Special: <>&\"\n"
                      "```";
    
    auto blocks = CodeExtractor::extractFromText(text);
    
    EXPECT_EQ(blocks.size(), 1);
    EXPECT_NE(blocks[0].code.find("Hello\\nWorld"), std::string::npos);
    EXPECT_NE(blocks[0].code.find("It\\'s working"), std::string::npos);
    EXPECT_NE(blocks[0].code.find("<>&"), std::string::npos);
}

TEST_F(CodeExtractorTests, NestedBackticks) {
    // Code blocks with backticks inside should not work with our simple regex
    std::string text = "Nested:\n```\nprint('`test`')\n```";
    auto blocks = CodeExtractor::extractFromText(text);
    
    EXPECT_EQ(blocks.size(), 1);
    EXPECT_EQ(blocks[0].code, "print('`test`')");
}

TEST_F(CodeExtractorTests, ExtractFromEmptyConversation) {
    Conversation conv;
    
    auto from_last = CodeExtractor::extractFromLastResponse(conv);
    auto from_all = CodeExtractor::extractFromConversation(conv);
    
    EXPECT_TRUE(from_last.empty());
    EXPECT_TRUE(from_all.empty());
}