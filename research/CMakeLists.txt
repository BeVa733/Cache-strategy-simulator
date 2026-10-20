add_executable(cache_trace_generator
    cache_trace_generator.cpp
)

add_executable(research_runner
    research_runner.cpp
)

target_compile_features(cache_trace_generator PRIVATE cxx_std_17)
target_compile_features(research_runner PRIVATE cxx_std_17)
