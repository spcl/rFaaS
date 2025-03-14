
cmake_minimum_required(VERSION 3.19)
include(FetchContent)
project(serverless-rdma)
set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=undefined")
#set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=undefined")
#find_program(CLANG_TIDY_COMMAND NAMES clang-tidy clang-tidy-13 clang-tidy-12 clang-tidy-11 clang-tidy-10)
#if(NOT CLANG_TIDY_COMMAND)
#  message(WARNING "clang-tidy is not found!")
#  set(CMAKE_CXX_CLANG_TIDY "" CACHE STRING "" FORCE)
#else()
#  set(CLANG_TIDY_CHECKS "*")
#  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND};-checks=${CLANG_TIDY_CHECKS};-header-filter=.")
#endif()

# dirty hack to remove NDEBUG
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " -g -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG ")
string(APPEND CMAKE_CXX_FLAGS_DEBUG " -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG ")
string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra ")

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/configuration)

option(WITH_EXAMPLES "Enable building extended function examples (requires additional dependencies)." Off)
if(${WITH_EXAMPLES})
  set(RFAAS_WITH_EXAMPLES ON)
else()
  set(RFAAS_WITH_EXAMPLES OFF)
endif()

set(WITH_TESTING "" CACHE STRING "Enable building of rFaaS tests, using the testing specification provided in JSON file.")
if( NOT WITH_TESTING STREQUAL "" )
  set(TESTING_CONFIG ${WITH_TESTING})
  set(RFAAS_WITH_TESTING ON)

  if(NOT EXISTS ${TESTING_CONFIG})
    message(FATAL_ERROR "The provided file does not exist: ${TESTING_CONFIG}!")
  endif()

  file(COPY ${TESTING_CONFIG} DESTINATION ${CMAKE_BINARY_DIR}/tests/configuration)
  message(STATUS "Enabling testing using configuration provided with ${TESTING_CONFIG}")
else()
  set(RFAAS_WITH_TESTING OFF)
endif()

###
# Optional: use existing installations
###
set(CXXOPTS_PATH "" CACHE STRING "Path to installation of cxxopts")
if( NOT CXXOPTS_PATH STREQUAL "")
  set(cxxopts_DIR "${CXXOPTS_PATH}/lib/cmake/cxxopts")
endif()
set(SPDLOG_PATH "" CACHE STRING "Path to installation of spdlog")
if( NOT SPDLOG_PATH STREQUAL "")
  set(spdlog_DIR "${SPDLOG_PATH}/lib/cmake/spdlog")
endif()
set(LIBRDMACM_PATH "" CACHE STRING "Path to installation of librdmacm")
if( NOT LIBRDMACM_PATH STREQUAL "")
  set(PKG_CONFIG_PATH "${PKG_CONFIG_PATH} ${LIBRDMACM_PATH}/lib/pkgconfig")
endif()

###
# threads
###
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

###
# PkgConfig 
###

find_package(PkgConfig REQUIRED)
if (NOT PkgConfig_FOUND)
    message(FATAL_ERROR "PkgConfig library not found")
endif (NOT PkgConfig_FOUND)

###
# librdmacm
###

pkg_check_modules(rdmacm REQUIRED IMPORTED_TARGET librdmacm)

###
# libibverbs
###
pkg_check_modules(ibverbs REQUIRED IMPORTED_TARGET libibverbs)

###
# External dependencies
###
include(dependencies)

###
# rdmalib: build C++14, PIC and no RTTI
###
file(GLOB rdmalib_files "rdmalib/lib/*.cpp")
add_library(rdmalib STATIC ${rdmalib_files})
add_dependencies(rdmalib spdlog)
add_dependencies(rdmalib cereal)
target_include_directories(rdmalib PUBLIC "rdmalib/include")
target_include_directories(rdmalib SYSTEM PUBLIC $<TARGET_PROPERTY:PkgConfig::rdmacm,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rdmalib SYSTEM PUBLIC $<TARGET_PROPERTY:PkgConfig::ibverbs,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rdmalib SYSTEM PUBLIC $<TARGET_PROPERTY:cereal,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rdmalib SYSTEM PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(rdmalib PROPERTIES POSITION_INDEPENDENT_CODE On)
set_target_properties(rdmalib PROPERTIES LIBRARY_OUTPUT_DIRECTORY lib)
target_link_libraries(rdmalib PUBLIC PkgConfig::rdmacm)
target_link_libraries(rdmalib PUBLIC PkgConfig::ibverbs)
target_link_libraries(rdmalib PRIVATE spdlog::spdlog)
target_link_libraries(rdmalib PRIVATE cereal)

###
# client library
###
file(GLOB rdmalib_files "rfaas/lib/*.cpp")
add_library(rfaaslib STATIC ${rdmalib_files})
add_dependencies(rfaaslib spdlog)
add_dependencies(rfaaslib cereal)
add_dependencies(rfaaslib rdmalib)
target_include_directories(rfaaslib PUBLIC "rfaas/include")
target_include_directories(rfaaslib PRIVATE $<TARGET_PROPERTY:rdmalib,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rfaaslib SYSTEM PUBLIC $<TARGET_PROPERTY:cereal,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rfaaslib SYSTEM PUBLIC $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(rfaaslib PROPERTIES POSITION_INDEPENDENT_CODE On)
set_target_properties(rfaaslib PROPERTIES LIBRARY_OUTPUT_DIRECTORY lib)
target_link_libraries(rfaaslib PUBLIC rdmalib)
target_link_libraries(rfaaslib PUBLIC PkgConfig::rdmacm)
target_link_libraries(rfaaslib PUBLIC PkgConfig::ibverbs)
target_link_libraries(rfaaslib PUBLIC spdlog::spdlog)
target_link_libraries(rfaaslib PRIVATE cereal)
target_link_libraries(rfaaslib PUBLIC dl)
target_link_libraries(rfaaslib PUBLIC Threads::Threads)

###
# Server
###
add_executable(executor
  server/executor/cli.cpp
  server/executor/opts.cpp
  server/executor/fast_executor.cpp
  server/executor/functions.cpp
)
add_executable(executor_manager
  server/executor_manager/cli.cpp
  server/executor_manager/opts.cpp
  server/executor_manager/settings.cpp
  server/executor_manager/manager.cpp
  server/executor_manager/client.cpp
  server/executor_manager/executor_process.cpp
)
add_executable(resource_manager
  server/resource_manager/cli.cpp
  server/resource_manager/client.cpp
  server/resource_manager/executor.cpp
  server/resource_manager/opts.cpp
  server/resource_manager/db.cpp
  server/resource_manager/http.cpp
  server/resource_manager/settings.cpp
  server/resource_manager/manager.cpp
)
set(targets "executor" "executor_manager" "resource_manager")
foreach(target ${targets})
  add_dependencies(${target} cxxopts::cxxopts)
  add_dependencies(${target} spdlog::spdlog)
  add_dependencies(${target} rdmalib)
  add_dependencies(${target} rfaaslib)
  target_include_directories(${target} PRIVATE server/)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rfaaslib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rdmalib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
  target_link_libraries(${target} PRIVATE spdlog::spdlog)
  target_link_libraries(${target} PRIVATE rdmalib)
  target_link_libraries(${target} PRIVATE rfaaslib)
  target_link_libraries(${target} PRIVATE Threads::Threads)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
  if(${target} STREQUAL "executor_manager" OR ${target} STREQUAL "resource_manager")
    add_dependencies(${target} readerwriterqueue)
    target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:readerwriterqueue,INTERFACE_INCLUDE_DIRECTORIES>)
    target_link_libraries(${target} PRIVATE readerwriterqueue)
  endif()
endforeach()
target_link_libraries(executor PRIVATE dl)
add_dependencies(resource_manager pistache_static)
target_include_directories(resource_manager SYSTEM PUBLIC $<TARGET_PROPERTY:pistache_static,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(resource_manager PRIVATE pistache_static)

###
# Benchmark apps
###
include(benchmarks)

###
# Examples
###
add_library(functions SHARED examples/functions.cpp)
set_target_properties(functions PROPERTIES POSITION_INDEPENDENT_CODE On)
set_target_properties(functions PROPERTIES LIBRARY_OUTPUT_DIRECTORY examples)
if( ${RFAAS_WITH_EXAMPLES} )
  include(examples)
endif()

###
# Examples
###
if( ${RFAAS_WITH_TESTING} )
  include(testing)
endif()

