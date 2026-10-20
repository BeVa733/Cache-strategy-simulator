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
