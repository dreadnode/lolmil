#include "inference_helpers.hpp"
#include "error_types.hpp"
#include "config.hpp"
#include "console_output.hpp"
#include <iostream>
#include <algorithm>
#include <random>
#include <cmath>
#include <numeric>
#include <vector>

bool setup_cuda_provider(Ort::SessionOptions& session_options) {
    try {
        OrtCUDAProviderOptions cuda_options{};
        cuda_options.device_id = 0;
        cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchDefault;
        cuda_options.arena_extend_strategy = 0;
        cuda_options.gpu_mem_limit = SIZE_MAX;
        cuda_options.do_copy_in_default_stream = 1;
        session_options.AppendExecutionProvider_CUDA(cuda_options);
        return true;
    } catch (const Ort::Exception& e) {
        ConsoleOutput::print(std::wstring(L"Note: Running on CPU (CUDA not available: ") + std::wstring(e.what(), e.what() + strlen(e.what())) + L")");
        return false;
    }
}

std::vector<float> convert_float16_to_float32(const Ort::Float16_t* fp16_data, size_t num_elements) {
    // Prevent integer overflow in allocation
    const size_t MAX_ELEMENTS = SIZE_MAX / sizeof(float) / 2;
    if (num_elements > MAX_ELEMENTS) {
        throw std::invalid_argument("Number of elements too large for safe allocation");
    }

    if (num_elements > 0 && fp16_data == nullptr) {
        throw std::invalid_argument("Null pointer provided for non-zero element count");
    }
    
    std::vector<float> result(num_elements);
    std::transform(fp16_data, fp16_data + num_elements, result.begin(),
                   [](const Ort::Float16_t& fp16) { return fp16.ToFloat(); });
    return result;
}

int32_t select_next_token(float* logits, size_t vocab_size, float temperature) {
    if (vocab_size == 0) {
        return 0;
    }
    if (logits == nullptr && vocab_size > 0) {
        throw std::invalid_argument("logits pointer cannot be null for non-zero vocab size");
    }
    const size_t MAX_VOCAB_SIZE = 10000000;  // Sanity check: 10M tokens max
    if (vocab_size > MAX_VOCAB_SIZE) {
        throw std::invalid_argument("vocab_size exceeds maximum allowed size");
    }

    if (temperature <= 0.0F || temperature < 0.01F) {
        // Greedy decoding (argmax)
        int32_t best_token = 0;
        float best_score = logits[0];
        for (size_t i = 1; i < vocab_size; i++) {
            if (logits[i] > best_score) {
                best_score = logits[i];
                best_token = static_cast<int32_t>(i);
            }
        }
        return best_token;
    }

    std::vector<float> scaled_logits(vocab_size);
    for (size_t i = 0; i < vocab_size; i++) {
        scaled_logits[i] = logits[i] / temperature;
    }

    // Numerical stability: subtract max before exp
    float max_logit = *std::max_element(scaled_logits.begin(), scaled_logits.end());

    std::vector<float> probs(vocab_size);
    float sum = 0.0f;
    for (size_t i = 0; i < vocab_size; i++) {
        probs[i] = std::exp(scaled_logits[i] - max_logit);
        sum += probs[i];
    }

    for (size_t i = 0; i < vocab_size; i++) {
        probs[i] /= sum;
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float random_val = dist(gen);

    float cumsum = 0.0f;
    for (size_t i = 0; i < vocab_size; i++) {
        cumsum += probs[i];
        if (random_val <= cumsum) {
            return static_cast<int32_t>(i);
        }
    }

    return static_cast<int32_t>(vocab_size - 1);
}