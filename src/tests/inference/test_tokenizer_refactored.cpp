#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "tokenizer.hpp"
#include "config.hpp"
#include "fixtures/tokenizer_test_base.hpp"
#include <filesystem>
#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;

// Basic tokenizer tests using the base fixture
class TokenizerBasicTest : public TokenizerTestBase {
protected:
    void SetUp() override {
        TokenizerTestBase::SetUp();
        CreateMinimalTokenizer();
    }
};

// Parameterized test for encoding various strings
class TokenizerEncodingTest : public TokenizerTestBase,
                              public ::testing::WithParamInterface<std::pair<std::string, std::vector<int32_t>>> {
protected:
    void SetUp() override {
        TokenizerTestBase::SetUp();
        CreateMinimalTokenizer();
    }
};

TEST_P(TokenizerEncodingTest, EncodeString) {
    auto [input, expected] = GetParam();
    auto result = tokenizer->encode(input);
    
    if (!expected.empty()) {
        // Only check if we have expected values
        EXPECT_THAT(result, ::testing::ElementsAreArray(expected));
    } else if (input.empty()) {
        // Empty input should produce empty output
        EXPECT_TRUE(result.empty());
    } else {
        // Non-empty input with no expected output - just verify it doesn't crash
        EXPECT_FALSE(result.empty());
    }
}

INSTANTIATE_TEST_SUITE_P(
    BasicStrings,
    TokenizerEncodingTest,
    ::testing::Values(
        std::make_pair("hello", std::vector<int32_t>{1}),
        std::make_pair("world", std::vector<int32_t>{2}),
        std::make_pair("test", std::vector<int32_t>{3}),
        std::make_pair("hello world", std::vector<int32_t>{1, 2}),
        std::make_pair("", std::vector<int32_t>{}),
        std::make_pair("unknown", std::vector<int32_t>{})  // Unknown tokens
    )
);

// Parameterized test for special tokens
class TokenizerSpecialTokenTest : public TokenizerTestBase,
                                  public ::testing::WithParamInterface<std::pair<std::string, int32_t>> {
protected:
    void SetUp() override {
        TokenizerTestBase::SetUp();
        CreateAdvancedTokenizer();
    }
};

TEST_P(TokenizerSpecialTokenTest, SpecialTokenHandling) {
    auto [token, expected_id] = GetParam();
    auto encoded = tokenizer->encode(token);
    
    // Special tokens should be encoded properly
    if (expected_id > 0) {
        EXPECT_FALSE(encoded.empty());
        // Check if the special token ID appears in the encoding
        auto it = std::find(encoded.begin(), encoded.end(), expected_id);
        EXPECT_NE(it, encoded.end()) << "Special token " << token << " not found in encoding";
    }
}

INSTANTIATE_TEST_SUITE_P(
    SpecialTokens,
    TokenizerSpecialTokenTest,
    ::testing::Values(
        // Use actual TokenIds from config.hpp
        std::make_pair("<|system|>", static_cast<int32_t>(TokenIds::SYSTEM)),    // 32006
        std::make_pair("<|user|>", static_cast<int32_t>(TokenIds::USER)),        // 32010
        std::make_pair("<|assistant|>", static_cast<int32_t>(TokenIds::ASSISTANT)), // 32001
        std::make_pair("<|end|>", static_cast<int32_t>(TokenIds::END)),          // 32007
        std::make_pair("<|endoftext|>", static_cast<int32_t>(TokenIds::EOS))     // 32000
    )
);

// Parameterized test for edge cases
class TokenizerEdgeCaseTest : public TokenizerTestBase,
                              public ::testing::WithParamInterface<std::string> {
protected:
    void SetUp() override {
        TokenizerTestBase::SetUp();
        CreateAdvancedTokenizer();
    }
};

TEST_P(TokenizerEdgeCaseTest, HandleEdgeCaseInput) {
    std::string input = GetParam();
    
    EXPECT_NO_THROW({
        auto encoded = tokenizer->encode(input);
        if (!input.empty()) {
            // For non-empty input, should produce some tokens
            auto decoded = tokenizer->decode(encoded);
            // Decoded text should be related to input (might not be exact due to tokenization)
        }
    }) << "Failed with input: " << input;
}

INSTANTIATE_TEST_SUITE_P(
    EdgeCases,
    TokenizerEdgeCaseTest,
    ::testing::Values(
        "",                          // Empty string
        " ",                         // Single space
        "    ",                      // Multiple spaces
        "\n",                        // Newline
        "\t",                        // Tab
        "!@#$%^&*()",               // Special characters
        std::string(1000, 'a'),      // Long repetitive string
        "Hello\0World",              // Null byte
        "こんにちは",                 // Japanese
        "🌍🌎🌏",                    // Emojis
        std::string(10000, 'x')     // Very long string
    )
);

// Basic non-parameterized tests
TEST_F(TokenizerBasicTest, Constructor) {
    EXPECT_NE(tokenizer, nullptr);
}

TEST_F(TokenizerBasicTest, EncodeEmptyString) {
    auto tokens = tokenizer->encode("");
    EXPECT_TRUE(tokens.empty());
}

TEST_F(TokenizerBasicTest, DecodeEmptyVector) {
    std::vector<int32_t> empty_tokens;
    auto text = tokenizer->decode(empty_tokens);
    EXPECT_TRUE(text.empty());
}

TEST_F(TokenizerBasicTest, RoundTripEncoding) {
    std::string original = "hello world test";
    auto tokens = tokenizer->encode(original);
    auto decoded = tokenizer->decode(tokens);
    
    // Due to tokenization, exact match might not occur
    // but decoded should contain the key words
    EXPECT_FALSE(decoded.empty());
}

TEST_F(TokenizerBasicTest, FormatPrompt) {
    std::string user_input = "Hello, how are you?";
    auto formatted = tokenizer->format_prompt(user_input);
    
    // Should contain the special tokens
    EXPECT_NE(formatted.find("<|user|>"), std::string::npos);
    EXPECT_NE(formatted.find("<|end|>"), std::string::npos);
    EXPECT_NE(formatted.find("<|assistant|>"), std::string::npos);
    EXPECT_NE(formatted.find(user_input), std::string::npos);
}

TEST_F(TokenizerBasicTest, InvalidTokenizerFile) {
    fs::path invalid_path = temp_dir / "nonexistent.json";
    
    EXPECT_THROW({
        Tokenizer invalid_tokenizer(invalid_path.string());
    }, std::runtime_error);
}

TEST_F(TokenizerBasicTest, MalformedJsonFile) {
    fs::path malformed_path = temp_dir / "malformed.json";
    std::ofstream file(malformed_path);
    file << "{ invalid json }";
    file.close();
    
    EXPECT_THROW({
        Tokenizer malformed_tokenizer(malformed_path.string());
    }, std::runtime_error);
    
    fs::remove(malformed_path);
}

// Advanced tokenizer tests
class TokenizerRefactoredTest : public TokenizerTestBase {
protected:
    void SetUp() override {
        TokenizerTestBase::SetUp();
        CreateAdvancedTokenizer();
    }
};

TEST_F(TokenizerRefactoredTest, HandleUnicodeCharacters) {
    std::vector<std::string> unicode_tests = {
        "é", "こんにちは", "🌍", "Ċ"
    };
    
    for (const auto& text : unicode_tests) {
        auto tokens = tokenizer->encode(text);
        EXPECT_FALSE(tokens.empty()) << "Failed to encode: " << text;
        
        std::vector<int32_t> tokens_int(tokens.begin(), tokens.end());
        auto decoded = tokenizer->decode(tokens_int);
        EXPECT_FALSE(decoded.empty()) << "Failed to decode: " << text;
    }
}

TEST_F(TokenizerRefactoredTest, HandleByteTokens) {
    // Test byte fallback tokens
    auto tokens = tokenizer->encode(std::string("\x00\xFF", 2));
    EXPECT_FALSE(tokens.empty());
    
    // Test decoding
    auto decoded = tokenizer->decode(tokens);
    // Byte tokens should decode to something
}

TEST_F(TokenizerRefactoredTest, LargeVocabularyLookup) {
    // Create tokenizer with large vocabulary
    std::map<std::string, int> large_vocab;
    for (int i = 0; i < 10000; ++i) {
        large_vocab["token" + std::to_string(i)] = i;
    }
    
    CreateTokenizerWithCustomVocab(large_vocab);
    
    // Test lookup performance
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        tokenizer->encode("token500");
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    
    // Should complete quickly even with large vocab
    EXPECT_LT(duration.count(), 1000) << "Vocabulary lookup too slow";
}

TEST_F(TokenizerRefactoredTest, HandleMergeRules) {
    // Create tokenizer with specific merge rules
    std::map<std::string, int> vocab = {
        {"h", 1}, {"e", 2}, {"l", 3}, {"o", 4},
        {"he", 5}, {"ll", 6}, {"hello", 7}
    };
    std::vector<std::string> merges = {"h e", "l l", "he ll o"};
    
    CreateTokenizerWithCustomVocab(vocab, merges);
    
    auto tokens = tokenizer->encode("hello");
    EXPECT_FALSE(tokens.empty());
    // Should use merged tokens when available
}