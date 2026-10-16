# Inside an ESP-IDF build the repository is a header-only component.
if(ESP_PLATFORM)
    idf_component_register(INCLUDE_DIRS "src")
    return()
endif()

cmake_minimum_required(VERSION 3.16)
project(embedded_cpp_toolkit VERSION 1.0.0 LANGUAGES CXX)

add_library(embedded_cpp_toolkit INTERFACE)
add_library(ect::toolkit ALIAS embedded_cpp_toolkit)
target_include_directories(embedded_cpp_toolkit INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(embedded_cpp_toolkit INTERFACE cxx_std_17)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(ECT_BUILD_TESTS "Build the host unit tests" ON)
    option(ECT_SANITIZE "Build the unit tests with AddressSanitizer and UBSan" OFF)
    if(ECT_BUILD_TESTS)
        enable_testing()
        add_subdirectory(test)
    endif()
endif()
