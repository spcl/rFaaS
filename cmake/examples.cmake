
find_package(OpenCV REQUIRED)

## thumbnailer

file(MAKE_DIRECTORY examples/thumbnailer)
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory examples/thumbnailer/)
add_executable(thumbnailer "examples/thumbnailer/client.cpp" "examples/thumbnailer/opts.cpp")
set(tests_targets "thumbnailer")
foreach(target ${tests_targets})
  add_dependencies(${target} cxxopts::cxxopts)
  add_dependencies(${target} rdmalib)
  add_dependencies(${target} rfaaslib)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rdmalib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rfaaslib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
  target_link_libraries(${target} PRIVATE spdlog::spdlog)
  target_link_libraries(${target} PRIVATE rfaaslib)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY examples/thumbnailer)
endforeach()

add_library(thumbnailer_functions SHARED "examples/thumbnailer/functions.cpp")
target_include_directories(thumbnailer_functions PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(thumbnailer_functions PUBLIC ${OpenCV_LIBS})
set_target_properties(thumbnailer_functions PROPERTIES LIBRARY_OUTPUT_DIRECTORY examples/thumbnailer)


