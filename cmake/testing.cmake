
enable_testing()
include(GoogleTest)

add_executable(
  basic_allocation_test
  tests/basic_allocation_test.cpp
)

set(tests_targets "basic_allocation_test")
foreach(target ${tests_targets})
  add_dependencies(${target} rfaaslib)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rfaaslib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_link_libraries(${target} PRIVATE rfaaslib gtest_main)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY tests)
  gtest_discover_tests(basic_allocation_test)
endforeach()


