# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/ubuntu/rfaas

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/ubuntu/rfaas

# Include any dependencies generated for this target.
include CMakeFiles/cpp_interface.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/cpp_interface.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/cpp_interface.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/cpp_interface.dir/flags.make

CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.o: CMakeFiles/cpp_interface.dir/flags.make
CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.o: benchmarks/cpp_interface.cpp
CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.o: CMakeFiles/cpp_interface.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.o -MF CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.o.d -o CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.o -c /home/ubuntu/rfaas/benchmarks/cpp_interface.cpp

CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/benchmarks/cpp_interface.cpp > CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.i

CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/benchmarks/cpp_interface.cpp -o CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.s

CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.o: CMakeFiles/cpp_interface.dir/flags.make
CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.o: benchmarks/cpp_interface_opts.cpp
CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.o: CMakeFiles/cpp_interface.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.o -MF CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.o.d -o CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.o -c /home/ubuntu/rfaas/benchmarks/cpp_interface_opts.cpp

CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/benchmarks/cpp_interface_opts.cpp > CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.i

CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/benchmarks/cpp_interface_opts.cpp -o CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.s

# Object files for target cpp_interface
cpp_interface_OBJECTS = \
"CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.o" \
"CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.o"

# External object files for target cpp_interface
cpp_interface_EXTERNAL_OBJECTS =

benchmarks/cpp_interface: CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface.cpp.o
benchmarks/cpp_interface: CMakeFiles/cpp_interface.dir/benchmarks/cpp_interface_opts.cpp.o
benchmarks/cpp_interface: CMakeFiles/cpp_interface.dir/build.make
benchmarks/cpp_interface: _deps/spdlog-build/libspdlogd.a
benchmarks/cpp_interface: librfaaslib.a
benchmarks/cpp_interface: libbenchmarks.a
benchmarks/cpp_interface: librfaaslib.a
benchmarks/cpp_interface: librdmalib.a
benchmarks/cpp_interface: _deps/spdlog-build/libspdlogd.a
benchmarks/cpp_interface: /usr/lib/x86_64-linux-gnu/librdmacm.so
benchmarks/cpp_interface: /usr/lib/x86_64-linux-gnu/libibverbs.so
benchmarks/cpp_interface: CMakeFiles/cpp_interface.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable benchmarks/cpp_interface"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/cpp_interface.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/cpp_interface.dir/build: benchmarks/cpp_interface
.PHONY : CMakeFiles/cpp_interface.dir/build

CMakeFiles/cpp_interface.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/cpp_interface.dir/cmake_clean.cmake
.PHONY : CMakeFiles/cpp_interface.dir/clean

CMakeFiles/cpp_interface.dir/depend:
	cd /home/ubuntu/rfaas && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas/CMakeFiles/cpp_interface.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/cpp_interface.dir/depend
