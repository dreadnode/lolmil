# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Building the Project
```bash
# Configure with Visual Studio generator (recommended for Windows)
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build main executable
cmake --build build --config Release --target onnxtest

# Build test suite
cmake --build build --config Release --target test_suite

# Build everything
cmake --build build --config Release
```

### Running Tests
```bash
# Run all tests via CTest
cd build && ctest -C Release

# Run test executable directly (better output)
./build/Release/test_suite.exe

# Run tests using custom target (convenience)
cmake --build build --target run-tests

# Run specific test
./build/Release/test_suite.exe --gtest_filter=TokenizerTests.*

# Run filtered tests via CMake
cmake --build build --target run-tests-filtered -DGTEST_FILTER="TokenizerTests.*"

# List available tests
./build/Release/test_suite.exe --gtest_list_tests
```

### Code Coverage
```bash
# Configure with coverage enabled
cmake -B build-coverage -G "Visual Studio 17 2022" -A x64 -DCODE_COVERAGE=ON

# Build and generate coverage
cmake --build build-coverage --config Debug --target test_suite
cmake --build build-coverage --target coverage

# Open HTML report
cmake --build build-coverage --target coverage-html
```

### Linting
```bash
# Check code style issues
cmake --build build --target clang-tidy

# Auto-fix style issues
cmake --build build --target clang-tidy-fix
```

## Project Architecture

### Module Structure
```
src/
├── inference/          # ONNX inference module (static library)
│   ├── include/        # Public API headers
│   │   ├── inference_api.hpp    # Main inference API
│   │   └── tokenizer_api.hpp    # Tokenizer public interface
│   └── src/            # Implementation
│       ├── tokenizer.cpp/hpp     # BPE tokenizer for Phi-3
│       ├── inference_helpers.cpp # CUDA setup, token selection, generation loop
│       ├── inference_session.cpp # Session management and text generation
│       ├── conversation.cpp      # Multi-turn conversation handling
│       ├── code_extractor.cpp    # Extract code blocks from AI responses
│       ├── config.hpp            # Model constants (max tokens, temperature, etc.)
│       └── console_output.hpp    # Windows console Unicode handling
├── lua/                # Lua interpreter module (static library)
│   ├── include/        # Public API headers
│   │   └── lua_api.hpp
│   └── src/            # Implementation
│       ├── lua_runtime.cpp       # Core Lua VM management
│       ├── lua_executor.cpp      # High-level execution interface
│       ├── lua_extractor.cpp     # Extract Lua code from text
│       └── win32_bindings.cpp    # Windows API bindings for Lua
├── app/                # Main application
│   └── main.cpp        # Entry point, orchestrates inference + Lua execution
└── tests/              # All unit tests
    ├── inference/      # Inference module tests
    ├── lua/            # Lua module tests
    └── test_main.cpp   # Test runner entry point
```

### Key Architectural Decisions

**Inference Module Design:**
The inference module (`src/inference/`) is built as a static library that encapsulates:
- Tokenization logic with BPE (Byte Pair Encoding) for Phi-3 models
- ONNX Runtime session management with automatic CUDA/CPU fallback
- Text generation loop with KV-cache management for efficient inference
- Conversation management for multi-turn interactions
- Code extraction utilities to parse AI-generated code blocks
- Windows-specific console handling for Unicode I/O

**Lua Module Design:**
The Lua module (`src/lua/`) provides:
- Sandboxed Lua 5.4 runtime with Sol2 C++ bindings
- Win32 API bindings accessible via `win32` namespace in Lua
- Automatic extraction and execution of Lua code from AI responses
- Resource management and error handling

**Model Integration Flow:**
1. `main.cpp` initializes ONNX Runtime environment and session
2. `setup_cuda_provider()` attempts GPU setup, falls back to CPU
3. `Tokenizer` loads vocabulary from JSON and handles prompt formatting
4. `InferenceSession` manages the conversation and generation loop:
   - Encodes prompt → runs model → samples tokens → updates KV-cache → decodes output
   - Manages past_key_values tensors across iterations for context retention
5. `LuaExtractor` parses AI response for code blocks
6. `LuaExecutor` runs extracted Lua code in sandboxed environment

**Critical Paths in Code:**
- Model path hardcoded in `src/app/main.cpp:18`
- Tokenizer path hardcoded in `src/app/main.cpp:19`
- Update these before running!

## Dependencies and Setup

### ONNX Runtime Files
Place in `lib/onnxruntime/`:
- All `.lib` files (for linking)
- `onnxruntime_providers_cuda.dll` (GPU acceleration)
- `onnxruntime_providers_shared.dll` (required dependency)
- `onnxruntime-genai.dll` (generation features)

The main `onnxruntime.dll` uses system installation from `C:\Windows\System32` by default.

### CUDA Setup for GPU
Add to PATH:
```powershell
$env:PATH = $env:PATH + ";C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin;C:\Program Files\NVIDIA\CUDNN\v9.12\bin\12.9"
```

## Adding New Tests

1. Create test file in appropriate subdirectory:
   - `src/tests/inference/` for inference tests
   - `src/tests/lua/` for Lua tests
   - `src/tests/` for core/utility tests

2. Add to `src/tests/CMakeLists.txt` in the `TEST_SOURCES` list:
   ```cmake
   set(TEST_SOURCES
       test_main.cpp
       # ... existing files ...
       your_new_test.cpp  # or inference/your_new_test.cpp
   )
   ```

3. Use GoogleTest macros: `TEST()`, `TEST_F()`, `EXPECT_EQ()`, etc.

## CMake Configuration Options

- `CODE_COVERAGE` - Enable code coverage analysis (OFF by default)
- `ENABLE_CLANG_TIDY` - Enable clang-tidy static analysis (ON by default)
- `USE_SYSTEM_ONNXRUNTIME` - Use system ONNX Runtime DLL from System32 (ON by default)

## Important Build Notes (Lessons Learned)

### CMake Location on Windows
CMake is typically installed at `C:\Program Files\CMake\bin\cmake.exe` and may not be in PATH by default. Use the full path:
```bash
"C:\Program Files\CMake\bin\cmake.exe" -B build -G "Visual Studio 17 2022" -A x64
```

### Running Tests on Windows
When running tests from bash-like environments on Windows, use PowerShell or cmd wrappers:
```bash
# Using PowerShell (recommended)
powershell -Command "& './build/Release/test_suite.exe' --gtest_brief=1"

# Using cmd
cmd /c build\Release\test_suite.exe --gtest_brief=1

# Filter out problematic tests if needed
powershell -Command "& './build/Release/test_suite.exe' --gtest_filter=-TokenizerAdvancedTest.* --gtest_brief=1"
```

### C++20 Migration Notes
When upgrading from C++17 to C++20, be aware of:
- `u8` string literals now create `char8_t*` instead of `char*`. Fix with:
  ```cpp
  // C++20 fix for u8 literals
  const char8_t* u8_literal = u8"UTF-8 text";
  std::string str(reinterpret_cast<const char*>(u8_literal));
  ```
- Update both `CMAKE_CXX_STANDARD` and any hardcoded `-std=c++17` flags in CMakeLists.txt

### Modern C++ Features Now Available
- **C++20**: Requires Visual Studio 2022 (v17) or later
- `std::filesystem` for path handling (replaces Windows-specific APIs)
- `std::string_view` for non-owning string parameters
- `[[nodiscard]]` attributes for important return values
- `constexpr` for compile-time constants (already used extensively)
- `std::span` for array views (C++20)
- Concepts for template constraints (C++20)

### Known Test Issues
Some tests may fail or need to be skipped:
- `TokenizerAdvancedTest.MixedSpecialAndRegularTokens` - SEH exception on some systems
- `InferenceTest.*` - May fail if model files are not present
- Console thread safety tests may cause buffer corruption on Windows