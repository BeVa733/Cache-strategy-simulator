cmake_minimum_required(VERSION 3.11)

project(CacheSystem LANGUAGES CXX)

add_executable(cache
    src/main.cpp
)

target_include_directories(cache PRIVATE
    include
    policies
)

target_compile_features(cache PRIVATE cxx_std_17)

# ---- tests: ctest --test-dir build, or ./build/policy_tests; table: ./build/compare
enable_testing()

add_executable(policy_tests tests/PolicyTests.cpp)
target_include_directories(policy_tests PRIVATE include policies)
target_compile_features(policy_tests PRIVATE cxx_std_17)
add_test(NAME policy_tests COMMAND policy_tests)

add_executable(compare tests/Compare.cpp)
target_include_directories(compare PRIVATE include policies)
target_compile_features(compare PRIVATE cxx_std_17)
