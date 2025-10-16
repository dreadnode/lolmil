# Lua interpreter module with embedded Lua runtime

include(FetchContent)

# ================================================================================
# Dependencies
# ================================================================================

# Sol2 - Modern C++ Lua bindings (header-only)
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(sol2)

# Lua - The Lua programming language
set(LUA_VERSION "5.4.8")
FetchContent_Declare(
    lua_download
    URL https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz
    URL_HASH SHA256=4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua_download)

# ================================================================================
# Build Lua static library
# ================================================================================

if(NOT TARGET lua_static)
    set(LUA_SRC_DIR ${lua_download_SOURCE_DIR}/src)
    
    # Collect Lua core sources (exclude standalone interpreters)
    file(GLOB LUA_CORE_SRCS ${LUA_SRC_DIR}/l*.c)
    list(REMOVE_ITEM LUA_CORE_SRCS 
        ${LUA_SRC_DIR}/lua.c    # Standalone interpreter
        ${LUA_SRC_DIR}/luac.c   # Lua compiler
    )
    
    # Create Lua static library
    add_library(lua_static STATIC ${LUA_CORE_SRCS})
    
    # Lua headers
    target_include_directories(lua_static PUBLIC ${LUA_SRC_DIR})
    
    # Platform-specific Lua configuration
    if(WIN32)
        target_compile_definitions(lua_static PRIVATE LUA_BUILD_AS_DLL)
    elseif(UNIX AND NOT APPLE)
        target_compile_definitions(lua_static PRIVATE LUA_USE_LINUX)
    elseif(APPLE)
        target_compile_definitions(lua_static PRIVATE LUA_USE_MACOSX)
    else()
        target_compile_definitions(lua_static PRIVATE LUA_USE_POSIX)
    endif()
    
    # Set properties
    set_target_properties(lua_static PROPERTIES
        OUTPUT_NAME "lua"
        POSITION_INDEPENDENT_CODE ON
    )
endif()

# ================================================================================
# Lua interpreter module library
# ================================================================================

# Source files
set(LUA_INTERPRETER_SOURCES
    # Modern C++20 implementation
    src/lua_runtime.cpp
    src/lua_modules.cpp
    src/win32_tools.cpp
    src/filesystem_tools.cpp
    # Note: lua_extract.hpp is header-only
)

# Create static library
add_library(lua_interpreter STATIC ${LUA_INTERPRETER_SOURCES})

# Include directories
target_include_directories(lua_interpreter 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${sol2_SOURCE_DIR}/include  # Sol2 headers
        ${CMAKE_SOURCE_DIR}/src/inference/include  # For utf_utils.hpp
)

# Link dependencies
target_link_libraries(lua_interpreter 
    PUBLIC 
        lua_static
        inference_lib  # For utf_utils implementation
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(lua_interpreter 
        PUBLIC
            user32
            kernel32
            advapi32
    )
endif()

# Set properties
set_target_properties(lua_interpreter PROPERTIES
    OUTPUT_NAME "lua_interpreter"
    POSITION_INDEPENDENT_CODE ON
)

# Ensure C++20 standard
target_compile_features(lua_interpreter PUBLIC cxx_std_20)