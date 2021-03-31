cmake_minimum_required(VERSION 3.11)
include(FetchContent)
project(serverless-rdma)

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
# librdmacm
###
find_package(PkgConfig REQUIRED)
pkg_check_modules(rdmacm REQUIRED IMPORTED_TARGET librdmacm)
###
# libibverbs
###
pkg_check_modules(ibverbs REQUIRED IMPORTED_TARGET libibverbs)

###
# External dependencies
###
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
###
# cxxopts
###
find_package(cxxopts QUIET)
if(NOT cxxopts_FOUND)
  message(STATUS "Downloading and building cxxopts dependency")
  FetchContent_Declare(cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    CMAKE_ARGS -DCXXOPTS_BUILD_EXAMPLES=Off -DCXXOPTS_BUILD_TESTS=Off
  )
  FetchContent_Populate(cxxopts)
  FetchContent_MakeAvailable(cxxopts)
  add_subdirectory(${cxxopts_SOURCE_DIR} ${cxxopts_BINARY_DIR})
endif()

###
# spdlog
###
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
  message(STATUS "Downloading and building spdlog dependency")
  FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    # default branch is v1.x - for some reason, cmake switches to master
    GIT_TAG v1.8.0
  )
  FetchContent_Populate(spdlog)
  FetchContent_MakeAvailable(spdlog)
  add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
else()
  add_custom_target(spdlog)
endif()

###
# cereal
###
find_package(cereal QUIET)
if(NOT cereal_FOUND)
  message(STATUS "Downloading and building cereal dependency")
  FetchContent_Declare(cereal
    GIT_REPOSITORY https://github.com/USCiLab/cereal.git
    CMAKE_ARGS -DSKIP_PERFORMANCE_COMPARISON=On -DSKIP_PORTABILITY_TEST=On -DJUST_INSTALL_CEREAL=On
    # default branch is v1.x - for some reason, cmake switches to master
    GIT_TAG v1.3.0
  )
  # for some reason the CMAKE_ARGS are ignored here
  set(SKIP_PERFORMANCE_COMPARISON ON CACHE INTERNAL "")
  set(SKIP_PORTABILITY_TEST ON CACHE INTERNAL "")
  set(JUST_INSTALL_CEREAL ON CACHE INTERNAL "")
  FetchContent_Populate(cereal)
  FetchContent_MakeAvailable(cereal)
  add_subdirectory(${cereal_SOURCE_DIR} ${cereal_BINARY_DIR})
endif()

###
# rdmalib: build C++14, PIC and no RTTI
###
file(GLOB rdmalib_files "rdmalib/lib/*.cpp")
add_library(rdmalib STATIC ${rdmalib_files})
add_dependencies(rdmalib spdlog)
add_dependencies(rdmalib cereal)
target_include_directories(rdmalib PUBLIC "rdmalib/include")
target_include_directories(rdmalib PUBLIC $<TARGET_PROPERTY:PkgConfig::rdmacm,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rdmalib PUBLIC $<TARGET_PROPERTY:PkgConfig::ibverbs,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rdmalib PUBLIC $<TARGET_PROPERTY:cereal,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rdmalib PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
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
target_include_directories(rfaaslib PUBLIC $<TARGET_PROPERTY:cereal,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rfaaslib PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(rfaaslib PROPERTIES POSITION_INDEPENDENT_CODE On)
set_target_properties(rfaaslib PROPERTIES LIBRARY_OUTPUT_DIRECTORY lib)
target_link_libraries(rfaaslib PUBLIC rdmalib)
target_link_libraries(rfaaslib PUBLIC PkgConfig::rdmacm)
target_link_libraries(rfaaslib PUBLIC PkgConfig::ibverbs)
target_link_libraries(rfaaslib PRIVATE spdlog::spdlog)
target_link_libraries(rfaaslib PRIVATE cereal)
target_link_libraries(rfaaslib PUBLIC dl)

###
# Warm/hot benchmarker
###
add_executable(warm_benchmarker tests/warm_benchmark.cpp tests/warm_benchmark_opts.cpp)
add_executable(parallel_invocations tests/parallel_invocations.cpp tests/parallel_invocations_opts.cpp)
add_executable(cold_benchmarker tests/cold_benchmark.cpp tests/cold_benchmark_opts.cpp)
#set(tests_targets "warm_benchmarker" "cold_benchmarker")
set(tests_targets "warm_benchmarker" "cold_benchmarker" "parallel_invocations")
foreach(target ${tests_targets})
  add_dependencies(${target} cxxopts::cxxopts)
  add_dependencies(${target} rdmalib)
  add_dependencies(${target} rfaaslib)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rdmalib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rfaaslib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
  target_link_libraries(${target} PRIVATE spdlog::spdlog)
  target_link_libraries(${target} PRIVATE rfaaslib)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY tests)
endforeach()

###
# Server
###
add_executable(executor server/executor.cpp server/opts.cpp server/fast_executor.cpp server/functions.cpp)
add_executable(executor_manager server/executor_manager.cpp server/manager.cpp server/opts.cpp)
set(targets "executor" "executor_manager")
foreach(target ${targets})
  add_dependencies(${target} cxxopts::cxxopts)
  add_dependencies(${target} spdlog::spdlog)
  add_dependencies(${target} rdmalib)
  target_include_directories(${target} PRIVATE server/)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rdmalib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
  target_link_libraries(${target} PRIVATE spdlog::spdlog)
  target_link_libraries(${target} PRIVATE rdmalib)
  target_link_libraries(${target} PRIVATE Threads::Threads)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
endforeach()
target_link_libraries(executor PRIVATE dl)

###
# Examples
###
add_library(functions SHARED examples/functions.cpp)
set_target_properties(functions PROPERTIES POSITION_INDEPENDENT_CODE On)
set_target_properties(functions PROPERTIES LIBRARY_OUTPUT_DIRECTORY examples)

