cmake_minimum_required(VERSION 3.20)
project(onnxtest VERSION 1.0.0 LANGUAGES CXX C)

# ================================================================================
# Global Settings
# ================================================================================

# C++ Standard - Using C++20 for modern features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories early
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Export compile commands for clang-tidy/IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable testing
enable_testing()
include(CTest)

# ================================================================================
# Options
# ================================================================================

option(CODE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy checks" ON)
option(USE_SYSTEM_ONNXRUNTIME "Use ONNX Runtime from Windows System32" ON)

# ================================================================================
# Platform Configuration
# ================================================================================

# Windows-specific global definitions
if(WIN32)
    add_compile_definitions(
        WIN32
        _WINDOWS
        WIN32_LEAN_AND_MEAN
        WINRT_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
endif()

# ================================================================================
# Code Coverage Configuration
# ================================================================================

if(CODE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        message(STATUS "Enabling code coverage for MSVC")
        add_compile_options(/Zi)
        add_link_options(/DEBUG /PROFILE)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Enabling code coverage for GCC/Clang")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    endif()
endif()

# ================================================================================
# Dependencies
# ================================================================================

include(FetchContent)

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # Windows compatibility
FetchContent_MakeAvailable(googletest)

# ================================================================================
# ONNX Runtime Configuration
# ================================================================================

set(ONNXRUNTIME_ROOT ${CMAKE_SOURCE_DIR}/lib/onnxruntime)
set(ONNXRUNTIME_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include/onnxruntime)

# Create interface library for ONNX Runtime
add_library(onnxruntime_interface INTERFACE)
target_include_directories(onnxruntime_interface INTERFACE 
    ${CMAKE_SOURCE_DIR}/include
    ${ONNXRUNTIME_INCLUDE_DIR}
)

# Add ONNX Runtime libraries
set(ONNX_LIBRARIES
    onnxruntime
    onnxruntime-genai
    onnxruntime_providers_shared
    onnxruntime_providers_cuda
)

foreach(lib ${ONNX_LIBRARIES})
    target_link_libraries(onnxruntime_interface INTERFACE ${ONNXRUNTIME_ROOT}/${lib}.lib)
endforeach()

# ================================================================================
# Clang-Tidy Configuration
# ================================================================================

if(ENABLE_CLANG_TIDY)
    set(CLANG_PATH "C:/clang")
    find_program(CLANG_TIDY_EXE 
        NAMES clang-tidy
        PATHS "${CLANG_PATH}/bin"
        NO_DEFAULT_PATH
    )
    
    if(NOT CLANG_TIDY_EXE)
        find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    endif()
    
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        
        # Create clang-tidy targets
        file(GLOB_RECURSE ALL_SOURCE_FILES
            ${CMAKE_SOURCE_DIR}/src/**/*.cpp
            ${CMAKE_SOURCE_DIR}/src/**/*.hpp
            ${CMAKE_SOURCE_DIR}/src/**/*.h
        )
        
        # Remove test files from clang-tidy if needed
        list(FILTER ALL_SOURCE_FILES EXCLUDE REGEX ".*/tests/.*")
        
        # Build include directories string for clang-tidy
        set(CLANG_TIDY_INCLUDES
            -I${CMAKE_SOURCE_DIR}/include
            -I${CMAKE_SOURCE_DIR}/src/inference/include
            -I${CMAKE_SOURCE_DIR}/src/inference/src
            -I${CMAKE_SOURCE_DIR}/src/lua/include
            -I${CMAKE_SOURCE_DIR}/src/lua/src
            -I${CMAKE_SOURCE_DIR}/lib/onnxruntime/include
            -I${CMAKE_SOURCE_DIR}/include/onnxruntime
            -I${CMAKE_SOURCE_DIR}/lib/lua
            -I${CMAKE_SOURCE_DIR}/lib/sol2/single/include
            -I${CMAKE_SOURCE_DIR}/lib/googletest/googletest/include
        )
        
        add_custom_target(clang-tidy
            COMMAND ${CLANG_TIDY_EXE}
                --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
                --header-filter=^${CMAKE_SOURCE_DIR}/src/.*$
                --system-headers=0
                --quiet
                ${ALL_SOURCE_FILES}
                --
                ${CLANG_TIDY_INCLUDES}
                -std=c++20
                -DWIN32
                -D_WINDOWS
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-tidy"
        )
        
        add_custom_target(clang-tidy-fix
            COMMAND ${CLANG_TIDY_EXE}
                --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
                --header-filter=^.*src/.*$
                --fix
                --fix-errors
                ${ALL_SOURCE_FILES}
                --
                ${CLANG_TIDY_INCLUDES}
                -std=c++20
                -DWIN32
                -D_WINDOWS
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running clang-tidy with auto-fix"
        )
    else()
        message(WARNING "clang-tidy not found. Static analysis disabled.")
    endif()
endif()

# ================================================================================
# Subdirectories
# ================================================================================

add_subdirectory(src/inference)
add_subdirectory(src/lua)
add_subdirectory(src/app)
add_subdirectory(src/tests)

# ================================================================================
# Coverage Targets
# ================================================================================

if(CODE_COVERAGE)
    if(MSVC)
        find_program(OPENCPPCOVERAGE OpenCppCoverage)
        if(OPENCPPCOVERAGE)
            add_custom_target(coverage
                COMMAND ${OPENCPPCOVERAGE}
                    --sources "${CMAKE_SOURCE_DIR}\\src\\inference\\src"
                    --sources "${CMAKE_SOURCE_DIR}\\src\\lua\\src"
                    --excluded_sources "*\\tests\\*"
                    --excluded_sources "*\\build\\*"
                    --export_type=html:coverage_report
                    --export_type=cobertura:coverage.xml
                    -- "$<TARGET_FILE:test_suite>"
                DEPENDS test_suite
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating coverage report"
            )
            
            add_custom_target(coverage-html
                COMMAND ${CMAKE_COMMAND} -E echo "Opening coverage report..."
                COMMAND cmd /c start coverage_report/index.html
                DEPENDS coverage
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Opening coverage report"
            )
        else()
            message(STATUS "OpenCppCoverage not found. Install from: https://github.com/OpenCppCoverage/OpenCppCoverage")
        endif()
    else()
        find_program(LCOV lcov)
        find_program(GENHTML genhtml)
        
        if(LCOV AND GENHTML)
            add_custom_target(coverage
                COMMAND ${LCOV} --directory . --zerocounters
                COMMAND $<TARGET_FILE:test_suite>
                COMMAND ${LCOV} --directory . --capture --output-file coverage.info
                COMMAND ${LCOV} --remove coverage.info '/usr/*' '*/tests/*' '*/build/*' --output-file coverage.filtered.info
                COMMAND ${GENHTML} coverage.filtered.info --output-directory coverage_report
                DEPENDS test_suite
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating coverage report"
            )
        endif()
    endif()
endif()