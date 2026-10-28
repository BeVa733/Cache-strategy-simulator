add_executable(policy_tests
  PolicyTests.cpp
)

target_include_directories(policy_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/policies
)

target_compile_features(policy_tests PRIVATE cxx_std_17)

add_test(NAME policy_tests COMMAND policy_tests)
