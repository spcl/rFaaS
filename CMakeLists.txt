cmake_minimum_required(VERSION 3.11)
include(FetchContent)
project(serverless-rdma)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# dirty hack to remove NDEBUG
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

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
    GIT_REPOSITORY git@github.com:jarro2783/cxxopts.git
    CMAKE_ARGS -DCXXOPTS_BUILD_EXAMPLES=Off -DCXXOPTS_BUILD_TESTS=Off
  )
  FetchContent_Populate(cxxopts)
  FetchContent_MakeAvailable(cxxopts)
  add_subdirectory(${cxxopts_SOURCE_DIR} ${cxxopts_BINARY_DIR})
endif()
#include_directories($<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
###
# spdlog
###
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
  message(STATUS "Downloading and building spdlog dependency")
  FetchContent_Declare(spdlog
    GIT_REPOSITORY git@github.com:gabime/spdlog.git
    # default branch is v1.x - for some reason, cmake switches to master
    GIT_TAG v1.x
  )
  FetchContent_Populate(spdlog)
  FetchContent_MakeAvailable(spdlog)
  add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
endif()
#include_directories($<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)

###
# Main pass: build C++14, PIC and no RTTI
###
file(GLOB rdmalib_files "rdmalib/lib/*.cpp")
add_library(rdmalib STATIC ${rdmalib_files})
target_include_directories(rdmalib PUBLIC "rdmalib/include")
target_include_directories(rdmalib PUBLIC $<TARGET_PROPERTY:PkgConfig::rdmacm,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rdmalib PUBLIC $<TARGET_PROPERTY:PkgConfig::ibverbs,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(rdmalib PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(rdmalib PROPERTIES POSITION_INDEPENDENT_CODE On)
set_target_properties(rdmalib PROPERTIES LIBRARY_OUTPUT_DIRECTORY lib)
target_link_libraries(rdmalib PUBLIC PkgConfig::rdmacm)
target_link_libraries(rdmalib PUBLIC PkgConfig::ibverbs)
target_link_libraries(rdmalib PRIVATE spdlog::spdlog)

###
# Client
###
add_executable(client client/client.cpp)
add_dependencies(client cxxopts::cxxopts)
add_dependencies(client rdmalib)
target_include_directories(client PRIVATE $<TARGET_PROPERTY:rdmalib,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(client PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(client PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(client PRIVATE spdlog::spdlog)
target_link_libraries(client PRIVATE rdmalib)
set_target_properties(client PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)

###
# Server
###
add_executable(server server/server.cpp)
add_dependencies(server cxxopts::cxxopts)
add_dependencies(server spdlog::spdlog)
add_dependencies(server rdmalib)
target_include_directories(server PRIVATE $<TARGET_PROPERTY:rdmalib,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(server PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(server PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(server PRIVATE spdlog::spdlog)
target_link_libraries(server PRIVATE rdmalib)
set_target_properties(server PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)
