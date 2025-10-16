#ifndef INFERENCE_HELPERS_HPP
#define INFERENCE_HELPERS_HPP
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include "tokenizer.hpp"
#include "config.hpp"

bool setup_cuda_provider(Ort::SessionOptions& session_options);
std::vector<float> convert_float16_to_float32(const Ort::Float16_t* fp16_data, size_t num_elements);
int32_t select_next_token(float* logits, size_t vocab_size, float temperature = Config::DEFAULT_TEMPERATURE);

#endif // INFERENCE_HELPERS_HPP