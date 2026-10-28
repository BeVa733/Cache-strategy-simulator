cmake_minimum_required(VERSION 3.11)

project(CacheSystem LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(cache
    src/main.cpp
)

target_include_directories(cache PRIVATE
    include
    policies
)

target_compile_features(cache PRIVATE cxx_std_17)

add_subdirectory(research)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
