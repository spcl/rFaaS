
find_package(OpenCV REQUIRED)

file(MAKE_DIRECTORY examples/thumbnailer)
file(MAKE_DIRECTORY examples/image-recognition)
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory examples/thumbnailer/)
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory examples/image-recognition/)

## thumbnailer

add_executable(thumbnailer "examples/thumbnailer/client.cpp" "examples/thumbnailer/opts.cpp")
add_executable(image-recognition "examples/image-recognition/client.cpp" "examples/image-recognition/opts.cpp")
set(tests_targets "thumbnailer" "image-recognition")
foreach(target ${tests_targets})
  add_dependencies(${target} cxxopts::cxxopts)
  add_dependencies(${target} rdmalib)
  add_dependencies(${target} rfaaslib)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rdmalib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:rfaaslib,INTERFACE_INCLUDE_DIRECTORIES>)
  target_include_directories(${target} SYSTEM PRIVATE $<TARGET_PROPERTY:cxxopts::cxxopts,INTERFACE_INCLUDE_DIRECTORIES>)
  target_link_libraries(${target} PRIVATE spdlog::spdlog)
  target_link_libraries(${target} PRIVATE rfaaslib)
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY examples/${target})
endforeach()

add_library(thumbnailer_functions SHARED "examples/thumbnailer/functions.cpp")
target_include_directories(thumbnailer_functions PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(thumbnailer_functions PUBLIC ${OpenCV_LIBS})
set_target_properties(thumbnailer_functions PROPERTIES LIBRARY_OUTPUT_DIRECTORY examples/thumbnailer)

## image recognition
find_package(Torch REQUIRED)
find_package(TorchVision REQUIRED)

add_library(img_recg_functions SHARED "examples/image-recognition/functions.cpp")
target_include_directories(img_recg_functions PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(img_recg_functions PUBLIC TorchVision::TorchVision "${TORCH_LIBRARIES}" ${OpenCV_LIBS})
set_target_properties(img_recg_functions PROPERTIES LIBRARY_OUTPUT_DIRECTORY examples/image-recognition)
set_property(TARGET img_recg_functions PROPERTY CXX_STANDARD 14)
