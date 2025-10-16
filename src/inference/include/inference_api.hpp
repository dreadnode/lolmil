#ifndef INFERENCE_API_HPP
#define INFERENCE_API_HPP

#include <string>
#include <vector>
#include <memory>
#include <onnxruntime_cxx_api.h>

// Result structure for generation operations
struct GenerationResult {
    std::string content;           // The generated text
    bool truncated = false;        // True if hit max_tokens limit
    size_t tokens_used = 0;        // Actual number of tokens generated

    enum class StopReason {
        END_TOKEN,                  // Hit EOS or END token
        MAX_TOKENS,                 // Hit token limit
        CODE_BLOCK_COMPLETE,        // Found closing ``` after code
        REPETITION_LIMIT,           // Too many repetitions
        ERROR,                      // Error during generation
        STOP_PATTERN               // Hit a stop pattern (like \n\n\n)
    };
    StopReason stop_reason = StopReason::END_TOKEN;

    // Convenience method for backward compatibility
    operator std::string() const { return content; }

    // Helper to check if generation was successful
    [[nodiscard]] bool is_complete() const {
        return !truncated && stop_reason != StopReason::ERROR;
    }
};

// Forward declaration to avoid circular dependency
class InferenceSession;

// Core message and conversation types
#include "../src/message.hpp"
#include "../src/conversation.hpp"
#include "../src/code_extractor.hpp"

// Utility function for CUDA setup
bool setup_cuda_provider(Ort::SessionOptions& session_options);

#endif // INFERENCE_API_HPP