include(FetchContent)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Mark third-party headers as system headers so -Werror applies to our code only.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25)
    set(third_party_system SYSTEM)
endif()

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    ${third_party_system}
)
FetchContent_Declare(
    arduinojson
    URL https://github.com/bblanchon/ArduinoJson/archive/refs/tags/v7.4.3.tar.gz
    URL_HASH SHA256=a2a8a277fc3cd0404499199bf1468682ea3688f354be0eaf6af8648366911257
    ${third_party_system}
)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest arduinojson)

find_package(Threads REQUIRED)

add_executable(ect_tests
    json_schema_test.cpp
    observable_test.cpp
    state_machine_test.cpp
    timing_test.cpp
    weekly_schedule_test.cpp
)
target_link_libraries(ect_tests PRIVATE ect::toolkit ArduinoJson GTest::gtest_main Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ect_tests PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
    if(ECT_SANITIZE)
        target_compile_options(ect_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ect_tests PRIVATE -fsanitize=address,undefined)
    endif()
endif()

include(GoogleTest)
gtest_discover_tests(ect_tests)
