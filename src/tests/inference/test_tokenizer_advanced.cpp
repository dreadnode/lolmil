#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "tokenizer.hpp"
#include "config.hpp"
#include <memory>

// Helper function to get test JSON
std::string GetTestTokenizerJson() {
    return R"({
        "model": {
            "vocab": {
                "<unk>": 0,
                "<|endoftext|>": 32000,
                "<|assistant|>": 32001,
                "<|system|>": 32006,
                "<|user|>": 32010,
                "<|end|>": 32007,
                "▁": 29871,
                "▁The": 450,
                "▁quick": 4996,
                "▁brown": 17354,
                "▁fox": 1701,
                "Hello": 15043,
                "World": 2787,
                "test": 1243,
                "!": 999,
                ".": 29889,
                "?": 29973,
                " ": 259,
                "a": 263,
                "b": 289,
                "c": 274,
                "0": 400,
                "1": 401,
                "2": 402,
                "3": 403,
                "Is": 500,
                "this": 501,
                "working": 502,
                "Yes": 503,
                "ing": 292,
                "ed": 287,
                "<0x0A>": 10,
                "<0x0D>": 13,
                "\\n": 320,
                "\\t": 321
            }
        },
        "added_tokens": [
            {"id": 32000, "content": "<|endoftext|>"},
            {"id": 32001, "content": "<|assistant|>"},
            {"id": 32006, "content": "<|system|>"},
            {"id": 32010, "content": "<|user|>"},
            {"id": 32007, "content": "<|end|>"}
        ]
    })";
}

// Test tokenization with mixed special and regular tokens
TEST(TokenizerAdvancedTest, MixedSpecialAndRegularTokens) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    std::string text = "Hello <|system|> World <|user|> test";
    std::vector<int32_t> tokens = tokenizer->encode(text);
    
    EXPECT_FALSE(tokens.empty());
    
    // Should contain both special and regular tokens
    bool has_special = std::any_of(tokens.begin(), tokens.end(), 
                                  [](int32_t t) { return t >= 32000; });
    bool has_regular = std::any_of(tokens.begin(), tokens.end(), 
                                  [](int32_t t) { return t < 32000; });
    
    EXPECT_TRUE(has_special);
    EXPECT_TRUE(has_regular);
}

// Test byte token encoding
TEST(TokenizerAdvancedTest, ByteTokenEncoding) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    // Text with special characters that should be encoded as byte tokens
    std::string text_with_bytes = "test\n\r\t";
    std::vector<int32_t> tokens = tokenizer->encode(text_with_bytes, false);
    
    EXPECT_FALSE(tokens.empty());
}

// Test handling of Unicode characters
TEST(TokenizerAdvancedTest, UnicodeHandling) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    // Text with Unicode characters
    std::string unicode_text = "Hello 世界 привет";
    std::vector<int32_t> tokens = tokenizer->encode(unicode_text);
    
    EXPECT_FALSE(tokens.empty());
    
    // Decode back
    std::string decoded = tokenizer->decode(tokens);
    EXPECT_FALSE(decoded.empty());
}

// Test word boundary handling with space markers
TEST(TokenizerAdvancedTest, WordBoundaryWithSpaceMarkers) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    // Text that should trigger word boundary handling
    std::string text = "The quick brown fox";
    std::vector<int32_t> tokens = tokenizer->encode(text);
    
    EXPECT_FALSE(tokens.empty());
}

// Test consecutive special tokens
TEST(TokenizerAdvancedTest, ConsecutiveSpecialTokens) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    std::string text = "<|system|><|user|><|assistant|>";
    std::vector<int32_t> tokens = tokenizer->encode(text, true);
    
    EXPECT_FALSE(tokens.empty());
    
    // All should be special tokens
    for (int32_t token : tokens) {
        EXPECT_GE(token, 32000);
    }
}

// Test unknown character handling
TEST(TokenizerAdvancedTest, UnknownCharacterHandling) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    // Text with characters not in vocabulary
    std::string text = "@#$%^&*()_+{}[]|\\:\"<>?~`";
    std::vector<int32_t> tokens = tokenizer->encode(text);
    
    EXPECT_FALSE(tokens.empty());
}

// Test very long sequence
TEST(TokenizerAdvancedTest, VeryLongSequence) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    // Create a long text
    std::string long_text;
    for (int i = 0; i < 100; ++i) {
        long_text += "Hello World test ";
    }
    
    std::vector<int32_t> tokens = tokenizer->encode(long_text);
    
    EXPECT_FALSE(tokens.empty());
    EXPECT_GT(tokens.size(), 100);
}

// Test format_prompt functionality
TEST(TokenizerAdvancedTest, FormatPromptVariations) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    std::string user_message = "Hello, how are you?";
    std::string formatted = tokenizer->format_prompt(user_message);
    
    EXPECT_FALSE(formatted.empty());
    EXPECT_NE(formatted.find("<|user|>"), std::string::npos);
    EXPECT_NE(formatted.find("<|assistant|>"), std::string::npos);
    EXPECT_NE(formatted.find(user_message), std::string::npos);
}

// Test decoding empty token list
TEST(TokenizerAdvancedTest, DecodeEmptyTokenList) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    std::vector<int32_t> empty_tokens;
    std::string decoded = tokenizer->decode(empty_tokens);
    
    EXPECT_TRUE(decoded.empty());
}

// Test decoding with only special tokens
TEST(TokenizerAdvancedTest, DecodeOnlySpecialTokens) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    std::vector<int32_t> special_tokens = {32000, 32001, 32006, 32010};
    std::string decoded = tokenizer->decode(special_tokens);
    
    // Special tokens are skipped during decode, so result should be empty
    EXPECT_TRUE(decoded.empty());
}

// Test decode variations
TEST(TokenizerAdvancedTest, DecodeTokenVariations) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    // Test decoding various token combinations
    std::vector<std::vector<int32_t>> test_cases = {
        {15043, 2787},  // Hello World
        {1243},         // test
        {32000},        // <|endoftext|>
        {0, 1, 2, 3}    // Various tokens including unknown
    };
    
    for (const auto& tokens : test_cases) {
        std::string decoded = tokenizer->decode(tokens);
        // Even unknown tokens should produce some output (possibly empty)
        // but shouldn't crash
        EXPECT_NO_THROW(tokenizer->decode(tokens));
    }
}

// Test encode with special tokens flag
TEST(TokenizerAdvancedTest, EncodeWithSpecialTokensFlag) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    std::string text = "<|system|> Hello <|user|> World";
    
    // With special tokens (default)
    std::vector<int32_t> with_special = tokenizer->encode(text, true);
    
    // Without special tokens
    std::vector<int32_t> without_special = tokenizer->encode(text, false);
    
    EXPECT_FALSE(with_special.empty());
    EXPECT_FALSE(without_special.empty());
    
    // With special tokens should have actual special token IDs
    bool has_special_in_with = std::any_of(with_special.begin(), with_special.end(),
                                          [](int32_t t) { return t >= 32000; });
    EXPECT_TRUE(has_special_in_with);
}

// Test newline and special character handling
TEST(TokenizerAdvancedTest, NewlineAndSpecialCharHandling) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    std::string text = "Line 1\nLine 2\tTabbed\rCarriage";
    std::vector<int32_t> tokens = tokenizer->encode(text);
    
    EXPECT_FALSE(tokens.empty());
    
    // Decode back and check it's not empty
    std::string decoded = tokenizer->decode(tokens);
    EXPECT_FALSE(decoded.empty());
}

// Test tokenization of numbers and punctuation
TEST(TokenizerAdvancedTest, NumbersAndPunctuation) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    std::string text = "123.456! Is this working? Yes, it is.";
    std::vector<int32_t> tokens = tokenizer->encode(text);
    
    EXPECT_FALSE(tokens.empty());
    
    // The tokenizer should produce tokens, even if they're unknown (0)
    // For this test, we just verify we got some tokens
    EXPECT_GT(tokens.size(), 5) << "Too few tokens for input: " << text;
}

// Test malformed special token patterns
TEST(TokenizerAdvancedTest, MalformedSpecialTokens) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    // Incomplete special tokens
    std::string text = "<| |> < > <|invalid|> <|user";
    std::vector<int32_t> tokens = tokenizer->encode(text);
    
    EXPECT_FALSE(tokens.empty());
}

// Test with all byte values
TEST(TokenizerAdvancedTest, AllByteValues) {
    auto tokenizer = Tokenizer::FromJsonString(GetTestTokenizerJson());
    ASSERT_NE(tokenizer, nullptr);
    
    // Create string with various byte values
    std::string byte_string;
    for (int i = 1; i < 128; ++i) {
        if (i != '<' && i != '>') {  // Avoid special token markers
            byte_string += static_cast<char>(i);
        }
    }
    
    std::vector<int32_t> tokens = tokenizer->encode(byte_string);
    
    // Should be able to tokenize any byte sequence
    EXPECT_FALSE(tokens.empty());
}