# Main application executable

# Create the main executable
add_executable(onnxtest main.cpp)

# Link libraries
target_link_libraries(onnxtest 
    PRIVATE
        inference_lib
        lua_interpreter
)

# Include directories for internal headers
target_include_directories(onnxtest 
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/inference/src  # Internal headers (tokenizer.hpp, etc.)
        ${CMAKE_SOURCE_DIR}/src/lua/include    # Lua API headers
        ${CMAKE_BINARY_DIR}/_deps/sol2-src/include  # Sol2 headers for new Lua API
)

# Windows: Copy required DLLs to output directory
if(WIN32)
    # Function to copy ONNX Runtime DLLs
    function(copy_onnx_dlls target_name)
        if(USE_SYSTEM_ONNXRUNTIME)
            # Copy all required DLLs including main runtime
            set(DLLS_TO_COPY
                onnxruntime.dll
                onnxruntime_providers_cuda.dll
                onnxruntime_providers_shared.dll
                onnxruntime-genai.dll
            )
        else()
            # Copy all DLLs when not using system runtime
            file(GLOB DLLS_TO_COPY RELATIVE ${ONNXRUNTIME_ROOT} "${ONNXRUNTIME_ROOT}/*.dll")
        endif()
        
        foreach(dll ${DLLS_TO_COPY})
            set(dll_path "${ONNXRUNTIME_ROOT}/${dll}")
            if(EXISTS "${dll_path}")
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${dll_path}"
                        "$<TARGET_FILE_DIR:${target_name}>"
                    COMMENT "Copying ${dll} to output directory"
                )
            endif()
        endforeach()
    endfunction()
    
    copy_onnx_dlls(onnxtest)
    
    # Copy CUDA and cuDNN DLLs for GPU acceleration
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
            add_custom_command(TARGET onnxtest POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${dll}"
                    "$<TARGET_FILE_DIR:onnxtest>"
                COMMENT "Copying CUDA DLL: ${dll}"
            )
        endif()
    endforeach()
    
    # Copy cuDNN DLLs
    foreach(dll ${CUDNN_DLLS})
        if(EXISTS "${dll}")
            add_custom_command(TARGET onnxtest POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${dll}"
                    "$<TARGET_FILE_DIR:onnxtest>"
                COMMENT "Copying cuDNN DLL: ${dll}"
            )
        endif()
    endforeach()
endif()