add_executable(QuartumGridNavTest
  QuartumGridNavTest.cpp
)

target_include_directories(QuartumGridNavTest PRIVATE
  ${REPO_ROOT}
  ${REPO_ROOT}/src
)

target_link_libraries(QuartumGridNavTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(QuartumGridNavTest)
