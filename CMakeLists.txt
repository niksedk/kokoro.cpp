cmake_minimum_required(VERSION 3.14)
project(KokoroCPP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options("/utf-8")
endif()

# ONNX Runtime Configuration
# Priority 1: Check third_party/onnxruntime
set(LOCAL_ONNX_PATH "${CMAKE_SOURCE_DIR}/third_party/onnxruntime")
if(EXISTS "${LOCAL_ONNX_PATH}")
    message(STATUS "Using local ONNX Runtime from: ${LOCAL_ONNX_PATH}")
    set(ONNXRUNTIME_ROOT "${LOCAL_ONNX_PATH}")
# Priority 2: User defined ONNXRUNTIME_ROOT
elseif(NOT DEFINED ONNXRUNTIME_ROOT)
    # Priority 3: Common system paths
    if (APPLE)
        set(ONNXRUNTIME_ROOT "/opt/homebrew/opt/onnxruntime")
    else()
        set(ONNXRUNTIME_ROOT "/usr/local")
    endif()
endif()

message(STATUS "Looking for ONNX Runtime in: ${ONNXRUNTIME_ROOT}")

find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
    HINTS ${ONNXRUNTIME_ROOT}/include
    PATH_SUFFIXES onnxruntime core/session
)

find_library(ONNXRUNTIME_LIB onnxruntime
    HINTS ${ONNXRUNTIME_ROOT}/lib
    PATH_SUFFIXES 
        mac_arm64 
        linux_x64 
        windows_x64
        mac_x64
        linux_aarch64
)

if (ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIB)
    message(STATUS "Found ONNX Runtime: ${ONNXRUNTIME_LIB}")
    include_directories(${ONNXRUNTIME_INCLUDE_DIR})
else()
    message(WARNING "ONNX Runtime not found. Please set ONNXRUNTIME_ROOT to your installation path.")
    message(WARNING "Example: cmake -DONNXRUNTIME_ROOT=/path/to/onnxruntime ..")
endif()

# Include paths
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/third_party/cppjieba/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party/cppjieba/deps/limonp/include)

# Shared Sources
set(LIB_SOURCES
    Kokoro.cpp
    Tokenizer.cpp
    ZHG2P.cpp
    PinyinFinder.cpp
    ToneSandhi.cpp
    ZHFrontend.cpp
)

add_library(kokoro_core STATIC ${LIB_SOURCES})

# Demo Executables
add_executable(zhg2p_demo main.cpp)
target_link_libraries(zhg2p_demo kokoro_core)

add_executable(kokoro_demo kokoro_main.cpp)
target_link_libraries(kokoro_demo kokoro_core)

if (ONNXRUNTIME_LIB)
    target_link_libraries(kokoro_demo ${ONNXRUNTIME_LIB})
    
    # Dynamic linking usually handles dependencies automatically.
    # But explicitly linking Foundation/CoreFoundation on Mac is safe and sometimes needed.
    if (APPLE)
        target_link_libraries(kokoro_demo "-framework Foundation" "-framework CoreFoundation")
    endif()
    
    # Add rpath to find the dylib at runtime
    if (APPLE)
        set_target_properties(kokoro_demo PROPERTIES INSTALL_RPATH "@loader_path/../third_party/onnxruntime/lib/mac_arm64")
    elseif (UNIX)
        set_target_properties(kokoro_demo PROPERTIES INSTALL_RPATH "$ORIGIN/../third_party/onnxruntime/lib/linux_x64")
    endif()
endif()

