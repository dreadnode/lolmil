#pragma once

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include "tokenizer.hpp"

namespace fs = std::filesystem;

class TokenizerTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = fs::temp_directory_path() / ("tokenizer_test_" + std::to_string(std::rand()));
        fs::create_directories(temp_dir);
        tokenizer_path = temp_dir / "tokenizer.json";
    }

    void TearDown() override {
        tokenizer.reset();
        if (fs::exists(temp_dir)) {
            fs::remove_all(temp_dir);
        }
    }

    void CreateMinimalTokenizer() {
        std::ofstream file(tokenizer_path);
        file << R"({
            "model": {
                "vocab": {
                    "hello": 1,
                    "world": 2,
                    "test": 3,
                    "<|system|>": 32006,
                    "<|user|>": 32010,
                    "<|assistant|>": 32001,
                    "<|end|>": 32007,
                    "<|endoftext|>": 32000
                },
                "merges": []
            },
            "added_tokens": [
                {"id": 32006, "content": "<|system|>", "special": true},
                {"id": 32010, "content": "<|user|>", "special": true},
                {"id": 32001, "content": "<|assistant|>", "special": true},
                {"id": 32007, "content": "<|end|>", "special": true},
                {"id": 32000, "content": "<|endoftext|>", "special": true}
            ]
        })";
        file.close();
        tokenizer = std::make_unique<Tokenizer>(tokenizer_path.string());
    }

    void CreateAdvancedTokenizer() {
        std::ofstream file(tokenizer_path);
        file << R"({
            "model": {
                "vocab": {
                    "hello": 1,
                    "world": 2,
                    "test": 3,
                    "the": 4,
                    "quick": 5,
                    "brown": 6,
                    "fox": 7,
                    "jumps": 8,
                    "over": 9,
                    "lazy": 10,
                    "dog": 11,
                    "▁": 12,
                    "Ġ": 13,
                    "Ċ": 14,
                    "é": 15,
                    "こんにちは": 16,
                    "🌍": 17,
                    "<0x00>": 256,
                    "<0xFF>": 511,
                    "<|system|>": 32006,
                    "<|user|>": 32010,
                    "<|assistant|>": 32001,
                    "<|end|>": 32007,
                    "<|endoftext|>": 32000
                },
                "merges": [
                    "h e",
                    "he l",
                    "hel lo",
                    "w o",
                    "wo r",
                    "wor ld"
                ]
            },
            "added_tokens": [
                {"id": 32006, "content": "<|system|>", "special": true},
                {"id": 32010, "content": "<|user|>", "special": true},
                {"id": 32001, "content": "<|assistant|>", "special": true},
                {"id": 32007, "content": "<|end|>", "special": true},
                {"id": 32000, "content": "<|endoftext|>", "special": true}
            ]
        })";
        file.close();
        tokenizer = std::make_unique<Tokenizer>(tokenizer_path.string());
    }

    void CreateTokenizerWithCustomVocab(const std::map<std::string, int>& vocab,
                                        const std::vector<std::string>& merges = {}) {
        std::ofstream file(tokenizer_path);
        file << "{\n  \"model\": {\n    \"vocab\": {\n";
        
        bool first = true;
        for (const auto& [token, id] : vocab) {
            if (!first) file << ",\n";
            file << "      \"" << token << "\": " << id;
            first = false;
        }
        
        file << "\n    },\n    \"merges\": [";
        for (size_t i = 0; i < merges.size(); ++i) {
            if (i > 0) file << ",";
            file << "\n      \"" << merges[i] << "\"";
        }
        file << "\n    ]\n  },\n  \"added_tokens\": [\n";
        file << "    {\"id\": 32006, \"content\": \"<|system|>\", \"special\": true},\n";
        file << "    {\"id\": 32010, \"content\": \"<|user|>\", \"special\": true},\n";
        file << "    {\"id\": 32001, \"content\": \"<|assistant|>\", \"special\": true},\n";
        file << "    {\"id\": 32007, \"content\": \"<|end|>\", \"special\": true},\n";
        file << "    {\"id\": 32000, \"content\": \"<|endoftext|>\", \"special\": true}\n";
        file << "  ]\n}";
        file.close();
        tokenizer = std::make_unique<Tokenizer>(tokenizer_path.string());
    }

    fs::path temp_dir;
    fs::path tokenizer_path;
    std::unique_ptr<Tokenizer> tokenizer;
};