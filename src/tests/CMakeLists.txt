# Test suite for all modules

# Test source files
set(TEST_SOURCES
    test_main.cpp
    # Core tests
    core/test_platform_utils_refactored.cpp
    core/test_error_handling.cpp
    core/test_json_validate.cpp
    core/test_security_core.cpp
    core/test_security_platform.cpp
    # Inference tests
    inference/test_tokenizer_refactored.cpp
    inference/test_tokenizer_json_parser.cpp
    inference/test_console_output.cpp
    inference/test_tokenizer_advanced.cpp
    inference/test_inference.cpp
    inference/test_message.cpp
    inference/test_conversation.cpp
    inference/test_code_extractor.cpp
    inference/test_inference_session.cpp
    # Lua module tests
    lua/lua_runtime_tests.cpp
    lua/lua_win32_tests.cpp
    lua/lua_extractor_tests.cpp
    lua/test_win32_tools.cpp
    lua/test_filesystem_tools.cpp
    lua/test_service_vulnerabilities.cpp
)

# Create test executable
add_executable(test_suite ${TEST_SOURCES})

# Include directories for test fixtures and utilities
target_include_directories(test_suite 
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}            # For fixtures and utils subdirs
        ${CMAKE_SOURCE_DIR}/src/inference/src  # Inference internal headers
        ${CMAKE_SOURCE_DIR}/src/lua/include    # Lua API headers
        ${CMAKE_BINARY_DIR}/_deps/sol2-src/include  # Sol2 headers
)

# Link libraries
target_link_libraries(test_suite
    PRIVATE
        gtest
        gtest_main
        gmock
        inference_lib
        lua_interpreter
)

# Enable GoogleTest discovery
include(GoogleTest)
gtest_discover_tests(test_suite
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    PROPERTIES LABELS "unit"
)

# Windows: Copy ONNX Runtime DLLs for tests
if(WIN32)
    # Reuse the function from app CMakeLists
    function(copy_onnx_dlls_for_tests target_name)
        if(USE_SYSTEM_ONNXRUNTIME)
            set(DLLS_TO_COPY
                onnxruntime.dll
                onnxruntime_providers_cuda.dll
                onnxruntime_providers_shared.dll
                onnxruntime-genai.dll
            )
        else()
            file(GLOB DLLS_TO_COPY RELATIVE ${ONNXRUNTIME_ROOT} "${ONNXRUNTIME_ROOT}/*.dll")
        endif()
        
        foreach(dll ${DLLS_TO_COPY})
            set(dll_path "${ONNXRUNTIME_ROOT}/${dll}")
            if(EXISTS "${dll_path}")
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${dll_path}"
                        "$<TARGET_FILE_DIR:${target_name}>"
                    COMMENT "Copying ${dll} for tests"
                )
            endif()
        endforeach()
    endfunction()
    
    copy_onnx_dlls_for_tests(test_suite)
    
    # Copy CUDA and cuDNN DLLs for GPU acceleration in tests
    set(CUDA_PATH "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.6")
    set(CUDNN_PATH "C:/Program Files/NVIDIA/CUDNN/v9.12/bin/12.9")
    
    # Essential CUDA runtime DLLs
    set(CUDA_DLLS
        "${CUDA_PATH}/bin/cudart64_12.dll"
        "${CUDA_PATH}/bin/cublas64_12.dll"
        "${CUDA_PATH}/bin/cublasLt64_12.dll"
        "${CUDA_PATH}/bin/cufft64_11.dll"
        "${CUDA_PATH}/bin/curand64_10.dll"
    )
    
    # Essential cuDNN DLLs
    set(CUDNN_DLLS
        "${CUDNN_PATH}/cudnn64_9.dll"
        "${CUDNN_PATH}/cudnn_ops64_9.dll"
        "${CUDNN_PATH}/cudnn_cnn64_9.dll"
        "${CUDNN_PATH}/cudnn_adv64_9.dll"
        "${CUDNN_PATH}/cudnn_graph64_9.dll"
        "${CUDNN_PATH}/cudnn_engines_precompiled64_9.dll"
        "${CUDNN_PATH}/cudnn_engines_runtime_compiled64_9.dll"
        "${CUDNN_PATH}/cudnn_heuristic64_9.dll"
    )
    
    # Copy CUDA DLLs
    foreach(dll ${CUDA_DLLS})
        if(EXISTS "${dll}")
            add_custom_command(TARGET test_suite POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${dll}"
                    "$<TARGET_FILE_DIR:test_suite>"
                COMMENT "Copying CUDA DLL for tests: ${dll}"
            )
        endif()
    endforeach()
    
    # Copy cuDNN DLLs
    foreach(dll ${CUDNN_DLLS})
        if(EXISTS "${dll}")
            add_custom_command(TARGET test_suite POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${dll}"
                    "$<TARGET_FILE_DIR:test_suite>"
                COMMENT "Copying cuDNN DLL for tests: ${dll}"
            )
        endif()
    endforeach()
endif()

# Add test command for convenience
add_custom_target(run-tests
    COMMAND $<TARGET_FILE:test_suite>
    DEPENDS test_suite
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running all unit tests"
)

# Add filtered test command
add_custom_target(run-tests-filtered
    COMMAND $<TARGET_FILE:test_suite> --gtest_filter=${GTEST_FILTER}
    DEPENDS test_suite
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running filtered tests: ${GTEST_FILTER}"
)