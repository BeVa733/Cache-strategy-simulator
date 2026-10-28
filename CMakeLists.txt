cmake_minimum_required(VERSION 3.11)

project(CacheSystem LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_IWYU "Run include-what-you-use during compilation" OFF)
if(ENABLE_IWYU)
  find_program(IWYU_EXECUTABLE NAMES include-what-you-use)
  if(NOT IWYU_EXECUTABLE)
    message(FATAL_ERROR "ENABLE_IWYU is ON, but IWYU was not found")
  endif()

  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE
      ${IWYU_EXECUTABLE}
      -Xiwyu --error=1
      -Xiwyu --max_line_length=80
      -Xiwyu --check_also=${PROJECT_SOURCE_DIR}/include/*.hpp
      -Xiwyu --check_also=${PROJECT_SOURCE_DIR}/policies/*.hpp
      -Xiwyu --check_also=${PROJECT_SOURCE_DIR}/research/*.hpp
  )
endif()

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
