#include <gtest/gtest.h>
#include "json.hpp"
#include <string>

TEST(JsonValidateTest, ParseAdvancedTestJson) {
    // Same JSON from TokenizerAdvancedTest
    const char* TEST_JSON = R"({
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
                "The": 450,
                "quick": 4996,
                "brown": 17354,
                "fox": 1701,
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
    
    try {
        nlohmann::json j = nlohmann::json::parse(TEST_JSON);
        ASSERT_TRUE(j.contains("model"));
        ASSERT_TRUE(j["model"].contains("vocab"));
        ASSERT_TRUE(j.contains("added_tokens"));
        
        // Count vocab items
        size_t vocab_size = j["model"]["vocab"].size();
        EXPECT_GT(vocab_size, 0) << "Vocab should have items";
        
        // Check for duplicate values issue
        std::unordered_map<std::string, int> vocab_map;
        for (auto& [key, val] : j["model"]["vocab"].items()) {
            vocab_map[key] = val.get<int>();
        }
        EXPECT_EQ(vocab_map.size(), vocab_size) << "Map size should match JSON size";
        
    } catch (const std::exception& e) {
        FAIL() << "JSON parsing failed: " << e.what();
    }
}