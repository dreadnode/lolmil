#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "tokenizer_json_schema.hpp"
#include "error_types.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>

class TokenizerJsonParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // No longer need platform::initialize()
    }
};

TEST_F(TokenizerJsonParserTest, ParseValidJson) {
    std::string json_content = R"({
        "model": {
            "vocab": {
                "hello": 100,
                "world": 200,
                "test": 300
            }
        },
        "added_tokens": [
            {"id": 1000, "content": "<special>", "special": true}
        ]
    })";
    
    TokenizerSchema schema = TokenizerJsonParser::parse(json_content);
    
    // Check vocab was parsed
    EXPECT_EQ(schema.model.vocab.size(), 3);
    EXPECT_EQ(schema.model.vocab["hello"], 100);
    EXPECT_EQ(schema.model.vocab["world"], 200);
    EXPECT_EQ(schema.model.vocab["test"], 300);
    
    // Check added tokens were parsed
    EXPECT_EQ(schema.added_tokens.size(), 1);
    EXPECT_EQ(schema.added_tokens[0].id, 1000);
    EXPECT_EQ(schema.added_tokens[0].content, "<special>");
}

TEST_F(TokenizerJsonParserTest, ParseEmptyVocab) {
    std::string json_content = R"({
        "model": {
            "vocab": {}
        }
    })";
    
    TokenizerSchema schema = TokenizerJsonParser::parse(json_content);
    
    EXPECT_EQ(schema.model.vocab.size(), 0);
    EXPECT_EQ(schema.added_tokens.size(), 0);
}

TEST_F(TokenizerJsonParserTest, ParseWithoutAddedTokens) {
    std::string json_content = R"({
        "model": {
            "vocab": {
                "token1": 1,
                "token2": 2
            }
        }
    })";
    
    TokenizerSchema schema = TokenizerJsonParser::parse(json_content);
    
    EXPECT_EQ(schema.model.vocab.size(), 2);
    EXPECT_EQ(schema.added_tokens.size(), 0);
}

TEST_F(TokenizerJsonParserTest, ParseMultipleAddedTokens) {
    std::string json_content = R"({
        "model": {
            "vocab": {
                "normal": 50
            }
        },
        "added_tokens": [
            {"id": 100, "content": "<token1>"},
            {"id": 200, "content": "<token2>"},
            {"id": 300, "content": "<token3>"}
        ]
    })";
    
    TokenizerSchema schema = TokenizerJsonParser::parse(json_content);
    
    EXPECT_EQ(schema.model.vocab.size(), 1);
    EXPECT_EQ(schema.added_tokens.size(), 3);
    EXPECT_EQ(schema.added_tokens[0].id, 100);
    EXPECT_EQ(schema.added_tokens[1].id, 200);
    EXPECT_EQ(schema.added_tokens[2].id, 300);
}

TEST_F(TokenizerJsonParserTest, ParseInvalidJsonThrows) {
    std::string invalid_json = "{ invalid json }";
    
    EXPECT_THROW(TokenizerJsonParser::parse(invalid_json), TokenizerError);
}

TEST_F(TokenizerJsonParserTest, ParseMissingModelThrows) {
    std::string json_content = R"({
        "other_key": {}
    })";
    
    EXPECT_THROW(TokenizerJsonParser::parse(json_content), TokenizerError);
}

TEST_F(TokenizerJsonParserTest, ParseMissingVocabThrows) {
    std::string json_content = R"({
        "model": {
            "other_key": {}
        }
    })";
    
    EXPECT_THROW(TokenizerJsonParser::parse(json_content), TokenizerError);
}

TEST_F(TokenizerJsonParserTest, MergeToMaps) {
    std::string json_content = R"({
        "model": {
            "vocab": {
                "hello": 1,
                "world": 2
            }
        },
        "added_tokens": [
            {"id": 100, "content": "<special>"}
        ]
    })";
    
    TokenizerSchema schema = TokenizerJsonParser::parse(json_content);
    
    std::unordered_map<std::string, int32_t> token_to_id;
    std::unordered_map<int32_t, std::string> id_to_token;
    
    schema.merge_to_maps(token_to_id, id_to_token);
    
    // Check regular tokens
    EXPECT_EQ(token_to_id["hello"], 1);
    EXPECT_EQ(token_to_id["world"], 2);
    EXPECT_EQ(id_to_token[1], "hello");
    EXPECT_EQ(id_to_token[2], "world");
    
    // Check special token
    EXPECT_EQ(token_to_id["<special>"], 100);
    EXPECT_EQ(id_to_token[100], "<special>");
}

TEST_F(TokenizerJsonParserTest, ParseUnicodeTokens) {
    std::string json_content = R"({
        "model": {
            "vocab": {
                "▁": 29871,
                "Ċ": 198,
                "Ġ": 220,
                "世界": 1234
            }
        }
    })";
    
    TokenizerSchema schema = TokenizerJsonParser::parse(json_content);
    
    EXPECT_EQ(schema.model.vocab.size(), 4);
    EXPECT_EQ(schema.model.vocab["▁"], 29871);
    EXPECT_EQ(schema.model.vocab["Ċ"], 198);
    EXPECT_EQ(schema.model.vocab["Ġ"], 220);
    EXPECT_EQ(schema.model.vocab["世界"], 1234);
}

TEST_F(TokenizerJsonParserTest, ParseByteTokens) {
    std::string json_content = R"({
        "model": {
            "vocab": {
                "<0x0A>": 10,
                "<0x0D>": 13,
                "<0x20>": 32,
                "<0xFF>": 255
            }
        }
    })";
    
    TokenizerSchema schema = TokenizerJsonParser::parse(json_content);
    
    EXPECT_EQ(schema.model.vocab.size(), 4);
    EXPECT_EQ(schema.model.vocab["<0x0A>"], 10);
    EXPECT_EQ(schema.model.vocab["<0x0D>"], 13);
    EXPECT_EQ(schema.model.vocab["<0x20>"], 32);
    EXPECT_EQ(schema.model.vocab["<0xFF>"], 255);
}

TEST_F(TokenizerJsonParserTest, ParseLargeVocab) {
    // Build a large vocabulary JSON
    std::stringstream ss;
    ss << R"({"model": {"vocab": {)";
    
    const int vocab_size = 10000;
    for (int i = 0; i < vocab_size; ++i) {
        ss << "\"token_" << i << "\": " << i;
        if (i < vocab_size - 1) {
            ss << ", ";
        }
    }
    
    ss << R"(}}})";
    
    TokenizerSchema schema = TokenizerJsonParser::parse(ss.str());
    
    EXPECT_EQ(schema.model.vocab.size(), vocab_size);
    EXPECT_EQ(schema.model.vocab["token_0"], 0);
    EXPECT_EQ(schema.model.vocab["token_5000"], 5000);
    EXPECT_EQ(schema.model.vocab["token_9999"], 9999);
}