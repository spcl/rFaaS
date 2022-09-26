
cmake_minimum_required(VERSION 3.11)
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

###
# Mandatory: devices configuration file.
###

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/configuration)

set(DEVICES_CONFIG "" CACHE STRING "Specification of RDMA devices in JSON file")
set(TESTING_CONFIG "" CACHE STRING "Specification of testing infrastructure in JSON file.")
if( NOT DEVICES_CONFIG STREQUAL "")
  message(STATUS "Copying device configuration ${DEVICES_CONFIG} to ${CMAKE_BINARY_DIR}/configuration")
  file(COPY ${DEVICES_CONFIG} DESTINATION ${CMAKE_BINARY_DIR}/configuration)
endif()
if( NOT TESTING_CONFIG STREQUAL "")
  set(RFAAS_WITH_TESTING ON)
  message(STATUS "Copying testing configuration ${TESTING_CONFIG} to ${CMAKE_BINARY_DIR}/configuration")
  file(COPY ${TESTING_CONFIG} DESTINATION ${CMAKE_BINARY_DIR}/configuration)
endif()

option(WITH_EXAMPLES "Enable building extended function examples (requires additional dependencies)." Off)
if(${WITH_EXAMPLES})
  set(RFAAS_WITH_EXAMPLES ON)
else()
  set(RFAAS_WITH_EXAMPLES OFF)
endif()

option(WITH_TESTING "Enable building of rFaaS tests." Off)
if(${WITH_TESTING})
  if( NOT TESTING_CONFIG STREQUAL "" AND NOT DEVICES_CONFIG STREQUAL "")
    set(RFAAS_WITH_TESTING ON)
    message(STATUS "Enabling testing using configuration provided with ${TESTING_CONFIG}")
  else()
    set(RFAAS_WITH_TESTING OFF)
    message(WARNING "Testing is disabled, as either device or testing configuration are not provided!")
  endif()
else()
  set(RFAAS_WITH_TESTING OFF)
endif()

###
# Select the networking support
###
option(WITH_LIBFABRIC "Enable libfabric backend instead of ibverbs" Off)
if(${WITH_LIBFABRIC}) 
  message(STATUS "Enabling libfabric support")
  add_definitions(-DUSE_LIBFABRIC)
endif()
option(WITH_GNI_AUTH "Enable the GNI authentication backend" Off)
if(${WITH_GNI_AUTH})
  message(STATUS "Enabling the GNI authentication backend")
  add_definitions(-DUSE_GNI_AUTH)
endif()

###
# Select whether we should compile the MPI scalability experiment
###
option(WITH_SCALABILITY "Compile the scalability experiment" Off)
if(${WITH_SCALABILITY})
  message(STATUS "Enabling the compilation of the scalability experiment")
  add_definitions(-DWITH_SCALABILITY)
  find_package(MPI REQUIRED)
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
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

###
# Networking
###
find_package(PkgConfig REQUIRED)
if (${WITH_LIBFABRIC})
  ###
  # libfabric
  ###
  pkg_check_modules(fabric REQUIRED IMPORTED_TARGET libfabric)
  if (${WITH_GNI_AUTH})
      ###
      # cray-drc (aka rmda-credentials)
      ###
      pkg_check_modules(drc REQUIRED IMPORTED_TARGET cray-drc)
  endif()
else()
  ###
  # librdmacm
  ###
  pkg_check_modules(rdmacm REQUIRED IMPORTED_TARGET librdmacm)
  ###
  # libibverbs
  ###
  pkg_check_modules(ibverbs REQUIRED IMPORTED_TARGET libibverbs)
endif()

###
# pistache
###
pkg_check_modules(Pistache REQUIRED IMPORTED_TARGET libpistache)

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
if( ${WITH_LIBFABRIC} )
  target_include_directories(rdmalib SYSTEM PUBLIC $<TARGET_PROPERTY:PkgConfig::fabric,INTERFACE_INCLUDE_DIRECTORIES>)
  if( ${WITH_GNI_AUTH} )
    target_include_directories(rdmalib SYSTEM PUBLIC $<TARGET_PROPERTY:PkgConfig::drc,INTERFACE_INCLUDE_DIRECTORIES>)
  endif()
else()
  target_include_directories(rdmalib SYSTEM PUBLIC $<TARGET_PROPERTY:PkgConfig::rdmacm,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(rdmalib SYSTEM PUBLIC $<TARGET_PROPERTY:PkgConfig::ibverbs,INTERFACE_INCLUDE_DIRECTORIES>)
endif()
target_include_directories(rdmalib SYSTEM PUBLIC $<TARGET_PROPERTY:cereal,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rdmalib SYSTEM PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(rdmalib PROPERTIES POSITION_INDEPENDENT_CODE On)
set_target_properties(rdmalib PROPERTIES LIBRARY_OUTPUT_DIRECTORY lib)
if( ${WITH_LIBFABRIC} )
  target_link_libraries(rdmalib PUBLIC PkgConfig::fabric)
  if( ${WITH_GNI_AUTH} )
    target_link_libraries(rdmalib PUBLIC PkgConfig::drc)
  endif()
else()
  target_link_libraries(rdmalib PUBLIC PkgConfig::rdmacm)
  target_link_libraries(rdmalib PUBLIC PkgConfig::ibverbs)
endif()
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
endforeach()
target_link_libraries(executor PRIVATE dl)
target_include_directories(resource_manager SYSTEM PUBLIC $<TARGET_PROPERTY:PkgConfig::Pistache,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(resource_manager PRIVATE PkgConfig::Pistache)

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

if( ${WITH_LIBFABRIC} AND ${WITH_GNI_AUTH})
  configure_file(
    scripts/setup.sh.in scripts/setup.sh
    FILE_PERMISSIONS GROUP_READ GROUP_WRITE GROUP_EXECUTE OWNER_READ OWNER_WRITE OWNER_EXECUTE
  )
endif()

