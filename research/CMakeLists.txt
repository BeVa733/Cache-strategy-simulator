add_executable(cache_trace_generator
    cache_trace_generator.cpp
)

add_executable(research_runner
    research_runner.cpp
)

add_executable(two_level_runner
    two_level_runner.cpp
)

add_executable(results_normalizer
    results_normalizer.cpp
)

target_compile_features(cache_trace_generator PRIVATE cxx_std_17)
target_compile_features(research_runner PRIVATE cxx_std_17)
target_compile_features(two_level_runner PRIVATE cxx_std_17)
target_compile_features(results_normalizer PRIVATE cxx_std_17)
