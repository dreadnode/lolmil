#ifndef CONFIG_HPP
#define CONFIG_HPP
#include <cstddef>
#include <cstdint>

struct Config {
    static constexpr int MAX_TOKENS = 2000;  // Increase max tokens for code generation
    static constexpr int MAX_SEQUENCE_LENGTH = 3900;
    static constexpr float DEFAULT_TEMPERATURE = 0.3F;  // Lower temp for more consistent code generation
    static constexpr int REPETITION_THRESHOLD = 5;
    static constexpr int MAX_REPETITIONS = 10;
    static constexpr int NUM_THREADS = 4;
    static constexpr int NUM_KV_CACHES = 64;
    static constexpr int64_t KV_HIDDEN_DIM = 32;
    static constexpr int64_t KV_HEAD_DIM = 96;
    static constexpr size_t VOCAB_SIZE_EXPECTED = 32064;
    
    // C++20: Allow designated initializers
    constexpr Config() = default;
};

struct TokenIds {
    static constexpr int32_t BOS = 1;
    static constexpr int32_t EOS = 32000;
    static constexpr int32_t UNK = 0;
    static constexpr int32_t PAD = 32000;
    static constexpr int32_t USER = 32010;
    static constexpr int32_t ASSISTANT = 32001;
    static constexpr int32_t SYSTEM = 32006;
    static constexpr int32_t END = 32007;
    
    // C++20: Allow designated initializers
    constexpr TokenIds() = default;
};

struct TokenizerConstants {
    // UTF-8 byte sequences for special markers
    static constexpr const char* SPACE_MARKER_UTF8 = "\xe2\x96\x81";  // ▁ (U+2581)
    static constexpr size_t SPACE_MARKER_LEN = 3;
    
    static constexpr const char* NEWLINE_MARKER_UTF8 = "\xc4\x8a";  // Ċ (U+010A)
    static constexpr size_t NEWLINE_MARKER_LEN = 2;
    
    // Token matching parameters
    static constexpr size_t MAX_TOKEN_MATCH_LENGTH = 100;
    
    // Byte token format
    static constexpr const char* BYTE_TOKEN_PREFIX = "<0x";
    static constexpr const char* BYTE_TOKEN_SUFFIX = ">";
    static constexpr size_t BYTE_TOKEN_PREFIX_LEN = 3;
    static constexpr int HEX_WIDTH = 2;
    
    // Special token newline ID
    static constexpr int32_t NEWLINE_TOKEN_ID = 13;  // 0x0A = 13 in tokenizer
    
    // C++20: Allow designated initializers
    constexpr TokenizerConstants() = default;
};

#endif // CONFIG_HPP