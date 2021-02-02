cmake_minimum_required(VERSION 3.11)
include(FetchContent)
project(serverless-rdma)

###
# Optional: use an existing installation
###
set(CXXOPTS_PATH "" CACHE STRING "Path to installation of cxxopts")
if( NOT CXXOPTS_PATH STREQUAL "")
  set(cxxopts_DIR "${CXXOPTS_PATH}/lib/cmake/cxxopts")
endif()

###
# External dependencies
###
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
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
include_directories($<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)

###
# Client
###
add_executable(client client/client.cpp)
set_property(TARGET client PROPERTY CXX_STANDARD 14)
add_dependencies(client cxxopts::cxxopts)
set_target_properties(client PROPERTIES RUNTIME_OUTPUT_DIRECTORY bin)

