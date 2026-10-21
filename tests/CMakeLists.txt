add_executable(policy_tests
  PolicyTests.cpp
)

target_compile_features(policy_tests PRIVATE cxx_std_17)

add_test(NAME policy_tests COMMAND policy_tests)
