# Inference module - static library for ONNX inference

# Source files
set(INFERENCE_SOURCES
    src/tokenizer.cpp
    src/tokenizer_json_parser.cpp
    src/inference_helpers.cpp
    src/platform_utils.cpp
    src/utf_utils.cpp
    src/app_config.cpp
    src/conversation.cpp
    src/code_extractor.cpp
    src/inference_session.cpp
)

# Create static library
add_library(inference_lib STATIC ${INFERENCE_SOURCES})

# Public API headers location
target_include_directories(inference_lib 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link with ONNX Runtime interface (defined in root CMakeLists.txt)
target_link_libraries(inference_lib
    PUBLIC
        onnxruntime_interface
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(inference_lib PUBLIC windowsapp.lib)
endif()

# Set properties
set_target_properties(inference_lib PROPERTIES
    OUTPUT_NAME "inference"
    POSITION_INDEPENDENT_CODE ON
)

# Ensure C++17 standard
target_compile_features(inference_lib PUBLIC cxx_std_17)