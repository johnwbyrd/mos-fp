cmake_minimum_required(VERSION 3.20)
project(opine
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "Optimized Policy-Instantiated Numeric Engine"
)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library
add_library(opine INTERFACE)
target_include_directories(opine INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Require BitInt support
target_compile_features(opine INTERFACE cxx_std_20)

# Enable testing
enable_testing()

# Add unit tests
add_subdirectory(tests)
