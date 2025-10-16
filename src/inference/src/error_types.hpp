#ifndef ERROR_TYPES_HPP
#define ERROR_TYPES_HPP

#include <stdexcept>
#include <string>

class TokenizerError : public std::runtime_error {
public:
    explicit TokenizerError(const std::string& message) 
        : std::runtime_error("Tokenizer error: " + message) {}
};

class ModelError : public std::runtime_error {
public:
    explicit ModelError(const std::string& message)
        : std::runtime_error("Model error: " + message) {}
};

class InferenceError : public std::runtime_error {
public:
    explicit InferenceError(const std::string& message)
        : std::runtime_error("Inference error: " + message) {}
};

#endif // ERROR_TYPES_HPP