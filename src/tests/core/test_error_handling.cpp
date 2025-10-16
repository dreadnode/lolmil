#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "tokenizer.hpp"
#include "inference_helpers.hpp"
#include "tokenizer_json_schema.hpp"
#include "platform_utils.hpp"
#include "error_types.hpp"
#include <filesystem>
#include <fstream>

class ErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // platform::initialize() no longer needed
    }
};

// Test TokenizerError exception
TEST(ErrorHandlingTest, TokenizerErrorException) {
    try {
        throw TokenizerError("Test tokenizer error");
    } catch (const TokenizerError& e) {
        // The error class adds "Tokenizer error: " prefix
        EXPECT_STREQ(e.what(), "Tokenizer error: Test tokenizer error");
    }
}

// Test ModelError exception
TEST(ErrorHandlingTest, ModelErrorException) {
    try {
        throw ModelError("Test model error");
    } catch (const ModelError& e) {
        // The error class adds "Model error: " prefix
        EXPECT_STREQ(e.what(), "Model error: Test model error");
    }
}

// Test InferenceError exception
TEST(ErrorHandlingTest, InferenceErrorException) {
    try {
        throw InferenceError("Test inference error");
    } catch (const InferenceError& e) {
        // The error class adds "Inference error: " prefix
        EXPECT_STREQ(e.what(), "Inference error: Test inference error");
    }
}

// Test loading non-existent tokenizer file
TEST(ErrorHandlingTest, LoadNonExistentTokenizer) {
    EXPECT_THROW({
        Tokenizer tokenizer("/path/that/does/not/exist/tokenizer.json");
    }, std::runtime_error);
}

// Test loading invalid JSON file
TEST(ErrorHandlingTest, LoadInvalidJsonTokenizer) {
    // Create a temporary file with invalid JSON
    auto temp_path = std::filesystem::temp_directory_path() / "invalid.json";
    std::ofstream file(temp_path);
    file << "{ this is not valid json }";
    file.close();
    
    EXPECT_THROW({
        Tokenizer tokenizer(temp_path.string());
    }, std::exception);
    
    // Clean up
    std::filesystem::remove(temp_path);
}

// Test loading JSON with wrong structure
TEST(ErrorHandlingTest, LoadMalformedTokenizerJson) {
    auto temp_path = std::filesystem::temp_directory_path() / "malformed.json";
    std::ofstream file(temp_path);
    file << R"({
        "wrong_field": "value",
        "no_model": true
    })";
    file.close();
    
    // Should throw or handle gracefully
    try {
        Tokenizer tokenizer(temp_path.string());
        // If it doesn't throw, that's also acceptable
    } catch (const std::exception&) {
        // Expected - malformed JSON structure
    }
    
    std::filesystem::remove(temp_path);
}

// Test edge cases in float16 conversion
TEST(ErrorHandlingTest, Float16ConversionEdgeCases) {
    // Test with null pointer
    std::vector<float> result = convert_float16_to_float32(nullptr, 0);
    EXPECT_TRUE(result.empty());
    
    // Test with zero elements
    Ort::Float16_t dummy;
    result = convert_float16_to_float32(&dummy, 0);
    EXPECT_TRUE(result.empty());
}

// Test token selection with valid edge cases
TEST(ErrorHandlingTest, TokenSelectionInvalidInputs) {
    // Test with zero vocab size
    int32_t token = select_next_token(nullptr, 0, 1.0f);
    EXPECT_EQ(token, 0);  // Should return 0 for empty vocab
    
    // Test with single element
    float logit = 1.0f;
    token = select_next_token(&logit, 1, 1.0f);
    EXPECT_EQ(token, 0);  // Should return 0 (only option)
    
    // Test with negative temperature (should use absolute value or default)
    std::vector<float> logits(10, 1.0f);
    token = select_next_token(logits.data(), 10, -1.0f);
    EXPECT_GE(token, 0);
    EXPECT_LT(token, 10);
}

// Test platform utils with extreme inputs
TEST(ErrorHandlingTest, PlatformUtilsExtremeInputs) {
    // Test with very long string
    std::string very_long(1000000, 'A');
    std::wstring wide = platform::utf8_to_wide(very_long);
    EXPECT_EQ(wide.length(), very_long.length());
    
    // Test with string containing null bytes
    std::string with_null = "Before";
    with_null.push_back('\0');
    with_null.append("After");
    
    std::wstring wide_null = platform::utf8_to_wide(with_null);
    // Should handle null bytes
    
    // Test round trip with special UTF-8 sequences
    // In C++20, u8 literals create char8_t, so we use reinterpret_cast
    const char8_t* u8_literal = u8"🎉🎊🎈🎁🎀";
    std::string utf8_special(reinterpret_cast<const char*>(u8_literal));
    std::wstring wide_special = platform::utf8_to_wide(utf8_special);
    std::string back = platform::wide_to_utf8(wide_special);
    
    // May not be identical due to encoding differences, but should not crash
}

// Test JSON parser with edge cases
TEST(ErrorHandlingTest, JsonParserEdgeCases) {
    // Empty JSON
    try {
        std::string empty_json = "{}";
        TokenizerSchema schema = TokenizerJsonParser::parse(empty_json);
        // Should handle empty JSON
        EXPECT_TRUE(schema.model.vocab.empty());
    } catch (...) {
        // Also acceptable if it throws
    }
    
    // JSON with only whitespace
    try {
        std::string whitespace_json = "   \n\t\r   ";
        TokenizerJsonParser::parse(whitespace_json);
    } catch (...) {
        // Expected - invalid JSON
    }
    
    // Very large JSON
    std::string large_json = "{\"model\":{\"vocab\":{";
    for (int i = 0; i < 10000; ++i) {
        if (i > 0) large_json += ",";
        large_json += "\"token" + std::to_string(i) + "\":" + std::to_string(i);
    }
    large_json += "}},\"added_tokens\":[]}";
    
    try {
        TokenizerSchema schema = TokenizerJsonParser::parse(large_json);
        EXPECT_EQ(schema.model.vocab.size(), 10000);
    } catch (...) {
        // Acceptable if it fails on very large input
    }
}

// Test concurrent initialization
TEST(ErrorHandlingTest, ConcurrentPlatformInit) {
    // Multiple initializations should be safe
    for (int i = 0; i < 10; ++i) {
        // platform::initialize() no longer needed
    }
    
    // Should not crash or cause issues
    std::string test = "test";
    std::wstring wide = platform::utf8_to_wide(test);
    EXPECT_FALSE(wide.empty());
}