
find_package(MPI REQUIRED)

file(GLOB simulator_sources tools/scheduling_simulator/*cpp)
add_executable(scheduling_simulator ${simulator_sources})
target_include_directories(scheduling_simulator SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(scheduling_simulator SYSTEM PRIVATE $<TARGET_PROPERTY:spdlog::spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(scheduling_simulator SYSTEM PRIVATE $<TARGET_PROPERTY:MPI::MPI_CXX,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(scheduling_simulator PROPERTIES RUNTIME_OUTPUT_DIRECTORY tools)
target_link_libraries(scheduling_simulator PRIVATE spdlog::spdlog)
target_link_libraries(scheduling_simulator PRIVATE MPI::MPI_CXX)

