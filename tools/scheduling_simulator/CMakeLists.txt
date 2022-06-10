
cmake_minimum_required(VERSION 3.11)
project(scheduling-simulator)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/dependencies.cmake")

find_package(MPI REQUIRED)

file(GLOB simulator_sources *cpp)
add_executable(scheduling_simulator ${simulator_sources})
target_include_directories(scheduling_simulator SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(scheduling_simulator SYSTEM PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(scheduling_simulator SYSTEM PRIVATE $<TARGET_PROPERTY:cereal,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(scheduling_simulator SYSTEM PRIVATE $<TARGET_PROPERTY:MPI::MPI_CXX,INTERFACE_INCLUDE_DIRECTORIES>)

target_link_libraries(scheduling_simulator PRIVATE spdlog::spdlog)
target_link_libraries(scheduling_simulator PRIVATE cereal)
target_link_libraries(scheduling_simulator PRIVATE MPI::MPI_CXX)

set_target_properties(scheduling_simulator PROPERTIES RUNTIME_OUTPUT_DIRECTORY tools)
set_target_properties(scheduling_simulator PROPERTIES CXX_STANDARD 17)

