
enable_testing()
include(GoogleTest)

find_program(JQ NAMES jq)
if(NOT JQ)
  message(FATAL_ERROR "jq not found, but necessary for testing!")
endif()

# FIXME: configure this file!
execute_process (
  COMMAND bash -c "${JQ} -j \'.test_executor.device\' ${CMAKE_BINARY_DIR}/tests/configuration/testing.json"
  RESULT_VARIABLE TEST_DEVICE_STATUS
  OUTPUT_VARIABLE TEST_DEVICE
)
if(NOT ${TEST_DEVICE_STATUS} EQUAL 0)
  message(FATAL_ERROR "Couldn't query test executor device from ${CMAKE_BINARY_DIR}/tests/configuration/testing.json; reason ${TEST_DEVICE_STATUS}")
endif()

execute_process (
  COMMAND bash -c "${JQ} -j \'.test_executor.port\' ${CMAKE_BINARY_DIR}/tests/configuration/testing.json"
  RESULT_VARIABLE TEST_PORT_STATUS
  OUTPUT_VARIABLE TEST_PORT
)
if(NOT ${TEST_PORT_STATUS} EQUAL 0)
  message(FATAL_ERROR "Couldn't query test executor device from ${CMAKE_BINARY_DIR}/tests/configuration/testing.json; reason ${TEST_PORT_STATUS}")
endif()

execute_process (
  COMMAND bash -c "${JQ} -j \'.executor_manager_server.port\' ${CMAKE_BINARY_DIR}/tests/configuration/testing.json"
  RESULT_VARIABLE EXEC_MGR_PORT_STATUS
  OUTPUT_VARIABLE EXEC_MGR_PORT
)
if(NOT ${EXEC_MGR_PORT_STATUS} EQUAL 0)
  message(FATAL_ERROR "Couldn't query test executor manager port from ${CMAKE_BINARY_DIR}/tests/configuration/testing.json; reason ${EXEC_MGR_PORT_STATUS}")
endif()

message(STATUS "Executing device generator to prepare testing configuration!")
execute_process (
  COMMAND bash -c "${CMAKE_SOURCE_DIR}/tools/device_generator.sh -p ${TEST_PORT} > ${CMAKE_BINARY_DIR}/tests/configuration/devices.json"
  RESULT_VARIABLE DEVICES_STATUS
)
if(NOT ${DEVICES_STATUS} EQUAL 0)
  message(FATAL_ERROR "Couldn't query devices; reason ${DEVICES_STATUS}")
endif()

configure_file(${CMAKE_SOURCE_DIR}/tests/config.h.in ${CMAKE_BINARY_DIR}/tests/config.h @ONLY)

add_test(
  NAME start_exec_mgr
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/run_executor_manager.sh
  ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/tests/configuration/devices.json
)
add_test(
  NAME end_exec_mgr
  COMMAND ${CMAKE_SOURCE_DIR}/scripts/kill_executor_manager.sh ${CMAKE_BINARY_DIR}
)

set_property(TEST start_exec_mgr PROPERTY WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_property(TEST start_exec_mgr PROPERTY FIXTURES_SETUP localserver)
set_property(TEST start_exec_mgr PROPERTY ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/bin:$ENV{PATH}")
set_property(TEST end_exec_mgr PROPERTY FIXTURES_CLEANUP localserver)

add_executable(
  basic_allocation_test
  tests/basic_allocation_test.cpp
)

set(tests_targets "basic_allocation_test")
foreach(target ${tests_targets})
  add_dependencies(${target} rfaaslib)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rfaaslib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} PRIVATE ${CMAKE_BINARY_DIR}/tests)
  target_link_libraries(${target} PRIVATE rfaaslib gtest_main)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY tests)
  gtest_discover_tests(
    ${target}
    EXTRA_ARGS ${TEST_DEVICE}
    PROPERTIES FIXTURES_REQUIRED localserver
  )
  #set_tests_properties(${target} PROPERTIES FIXTURES_REQUIRED localserver)
endforeach()


