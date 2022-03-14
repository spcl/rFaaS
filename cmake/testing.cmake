
enable_testing()
include(GoogleTest)

find_program(JQ NAMES jq)
if(NOT JQ)
  message(FATAL_ERROR "jq not found, but necessary for testing!")
endif()

execute_process (
  COMMAND bash -c "${JQ} -j \'.test_executor.device\' ${CMAKE_BINARY_DIR}/configuration/testing.json"
  OUTPUT_VARIABLE TEST_DEVICE
)

# FIXME: config build dir
# FIXME: config user
add_test(
  NAME start_exec_mgr
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_executor_manager.sh
  ${CMAKE_BINARY_DIR} 32 1 0
)
add_test(
  NAME end_exec_mgr
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/kill_executor_manager.sh ${CMAKE_BINARY_DIR}
)

set_property(TEST start_exec_mgr PROPERTY WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_property(TEST start_exec_mgr PROPERTY FIXTURES_SETUP localserver)
set_property(TEST start_exec_mgr PROPERTY ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/bin\:$ENV{PATH}")
set_property(TEST end_exec_mgr PROPERTY FIXTURES_CLEANUP localserver)

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
  gtest_discover_tests(
    ${target}
    EXTRA_ARGS ${TEST_DEVICE}
    PROPERTIES FIXTURES_REQUIRED localserver
  )
  #set_tests_properties(${target} PROPERTIES FIXTURES_REQUIRED localserver)
endforeach()


