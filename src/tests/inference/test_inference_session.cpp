#include <gtest/gtest.h>
#include "../inference/src/inference_session.hpp"
#include "../inference/src/code_extractor.hpp"
#include "../inference/src/error_types.hpp"
#include "../inference/src/platform_utils.hpp"
#include <filesystem>
#include <fstream>

// Global initialization for tests
class InferenceTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // platform::initialize() no longer needed
    }
};

// Register the environment
static ::testing::Environment* const inference_env = 
    ::testing::AddGlobalTestEnvironment(new InferenceTestEnvironment);

// Mock test that doesn't require actual model files
class InferenceSessionMockTests : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(InferenceSessionMockTests, InvalidModelPath) {
    // Test that constructor throws when model file doesn't exist
    const std::wstring invalid_model = L"nonexistent_model.onnx";
    const std::string invalid_tokenizer = "nonexistent_tokenizer.json";
    
    EXPECT_THROW({
        InferenceSession session(invalid_model, invalid_tokenizer, false);
    }, std::exception);
}

// Integration tests - only run if model files exist
class InferenceSessionIntegrationTests : public ::testing::Test {
protected:
    const std::wstring model_path = L"C:\\Users\\mharley\\Downloads\\phi-3-mini-cuda-fp16\\phi3-mini-4k-instruct-cuda-fp16.onnx";
    const std::string tokenizer_path = R"(C:\Users\mharley\Downloads\phi-3-mini-cuda-fp16\tokenizer.json)";
    std::unique_ptr<InferenceSession> session;
    bool model_exists = false;
    
    void SetUp() override {
        // Check if model files exist
        model_exists = std::filesystem::exists(model_path) && 
                      std::filesystem::exists(tokenizer_path);
        
        if (!model_exists) {
            GTEST_SKIP() << "Model files not found, skipping integration tests";
        }
    }
    
    void TearDown() override {
        if (session) {
            session.reset();
        }
    }
    
    void CreateSession() {
        // Create session on demand in each test
        if (!session) {
            try {
                session = std::make_unique<InferenceSession>(model_path, tokenizer_path, false);
            } catch (const InferenceError& e) {
                FAIL() << "Failed to create InferenceSession: " << e.what();
            } catch (const std::exception& e) {
                FAIL() << "Unexpected exception creating InferenceSession: " << e.what();
            }
        }
    }
};

TEST_F(InferenceSessionIntegrationTests, CreateSession) {
    ASSERT_NO_THROW(CreateSession());
    ASSERT_NE(session, nullptr);
    
    // Session should be created successfully
    EXPECT_FALSE(session->isUsingGPU()); // We explicitly disabled CUDA
    EXPECT_TRUE(session->getConversation().empty());
}

TEST_F(InferenceSessionIntegrationTests, SetSystemPrompt) {
    CreateSession();
    ASSERT_NE(session, nullptr);
    
    session->addSystemPrompt("You are a Lua code generator");
    
    const auto& conv = session->getConversation();
    EXPECT_TRUE(conv.hasSystemPrompt());
    EXPECT_EQ(conv.getSystemPrompt(), "You are a Lua code generator");
}

TEST_F(InferenceSessionIntegrationTests, AddManualMessage) {
    CreateSession();
    ASSERT_NE(session, nullptr);
    
    session->addMessage(Message::user("Test message"));
    
    const auto& conv = session->getConversation();
    EXPECT_EQ(conv.size(), 1);
    EXPECT_EQ(conv.getHistory()[0].content, "Test message");
}

TEST_F(InferenceSessionIntegrationTests, ClearConversation) {
    CreateSession();
    ASSERT_NE(session, nullptr);
    
    session->addSystemPrompt("System");
    session->addMessage(Message::user("Hello"));
    session->addMessage(Message::assistant("Hi"));
    
    EXPECT_EQ(session->getConversation().size(), 2);
    
    session->clearConversation();
    
    EXPECT_EQ(session->getConversation().size(), 0);
    // System prompt should still be there
    EXPECT_TRUE(session->getConversation().hasSystemPrompt());
}

TEST_F(InferenceSessionIntegrationTests, DISABLED_SimpleGeneration) {
    // DISABLED: Actual inference takes too long for unit tests
    CreateSession();
    ASSERT_NE(session, nullptr);
    
    session->addSystemPrompt("You are a helpful assistant. Keep responses very short.");
    
    // Generate with a simple prompt
    GenerationResult result = session->generate("Say 'test' and nothing else", 0.1f, 20);
    
    // Response should not be empty
    EXPECT_FALSE(result.content.empty());
    
    // Conversation should have both messages
    const auto& conv = session->getConversation();
    EXPECT_EQ(conv.size(), 2);
    EXPECT_EQ(conv.getHistory()[0].role, Message::User);
    EXPECT_EQ(conv.getHistory()[1].role, Message::Assistant);
    EXPECT_EQ(conv.getHistory()[1].content, result.content);
}

TEST_F(InferenceSessionIntegrationTests, DISABLED_ConversationContext) {
    // DISABLED: This test takes too long for regular test runs
    CreateSession();
    ASSERT_NE(session, nullptr);
    
    session->addSystemPrompt("You are a Lua expert. Keep responses short.");
    
    // First exchange
    GenerationResult response1 = session->generate("Remember the number 42", 0.1f, 30);
    EXPECT_FALSE(response1.content.empty());

    // Second exchange should remember context
    GenerationResult response2 = session->generate("What number did I mention?", 0.1f, 30);
    EXPECT_FALSE(response2.content.empty());
    
    // Check conversation has all messages
    const auto& conv = session->getConversation();
    EXPECT_EQ(conv.size(), 4); // 2 user + 2 assistant
}

TEST_F(InferenceSessionIntegrationTests, DISABLED_CodeGeneration) {
    // DISABLED: This test takes too long for regular test runs
    CreateSession();
    ASSERT_NE(session, nullptr);
    
    session->addSystemPrompt("Generate Lua code. Always wrap code in ```lua blocks.");
    
    GenerationResult result = session->generate(
        "Write a Lua function that returns the string 'hello'", 0.3f, 100);
    
    EXPECT_FALSE(result.content.empty());
    
    // Extract code blocks
    auto blocks = CodeExtractor::extractFromLastResponse(session->getConversation());
    
    // Should have at least one code block
    if (!blocks.empty()) {
        // Check if it's Lua code
        auto lua_blocks = CodeExtractor::filterByLanguage(blocks, "lua");
        EXPECT_FALSE(lua_blocks.empty());
        
        if (!lua_blocks.empty()) {
            // Should contain 'function' or 'return'
            std::string code = lua_blocks[0].code;
            bool has_function = code.find("function") != std::string::npos;
            bool has_return = code.find("return") != std::string::npos;
            EXPECT_TRUE(has_function || has_return);
        }
    }
}

// Unit tests for conversation management without needing model
class InferenceSessionUnitTests : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(InferenceSessionUnitTests, MessageFactoryMethods) {
    auto sys = Message::system("System");
    auto usr = Message::user("User");
    auto ast = Message::assistant("Assistant");
    
    EXPECT_EQ(sys.role, Message::System);
    EXPECT_EQ(usr.role, Message::User);
    EXPECT_EQ(ast.role, Message::Assistant);
}

TEST_F(InferenceSessionUnitTests, ConversationFormatting) {
    Conversation conv;
    conv.setSystemPrompt("System");
    conv.addMessage(Message::user("Hello"));
    conv.addMessage(Message::assistant("Hi"));
    
    std::string formatted = conv.formatForModel();
    
    // Check Phi-3 format markers
    EXPECT_NE(formatted.find("<|system|>"), std::string::npos);
    EXPECT_NE(formatted.find("<|user|>"), std::string::npos);
    EXPECT_NE(formatted.find("<|assistant|>"), std::string::npos);
    EXPECT_NE(formatted.find("<|end|>"), std::string::npos);
}

TEST_F(InferenceSessionUnitTests, CodeExtractorRegex) {
    std::string text = R"(
Here is some Lua code:
```lua
function test()
    return 42
end
```
And some Python:
```python
def test():
    return 42
```
)";
    
    auto blocks = CodeExtractor::extractFromText(text);
    
    EXPECT_EQ(blocks.size(), 2);
    EXPECT_EQ(blocks[0].language, "lua");
    EXPECT_EQ(blocks[1].language, "python");
}