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
include CMakeFiles/benchmarks.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/benchmarks.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/benchmarks.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/benchmarks.dir/flags.make

CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.o: CMakeFiles/benchmarks.dir/flags.make
CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.o: benchmarks/settings.cpp
CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.o: CMakeFiles/benchmarks.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.o -MF CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.o.d -o CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.o -c /home/ubuntu/rfaas/benchmarks/settings.cpp

CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/benchmarks/settings.cpp > CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.i

CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/benchmarks/settings.cpp -o CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.s

# Object files for target benchmarks
benchmarks_OBJECTS = \
"CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.o"

# External object files for target benchmarks
benchmarks_EXTERNAL_OBJECTS =

libbenchmarks.a: CMakeFiles/benchmarks.dir/benchmarks/settings.cpp.o
libbenchmarks.a: CMakeFiles/benchmarks.dir/build.make
libbenchmarks.a: CMakeFiles/benchmarks.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX static library libbenchmarks.a"
	$(CMAKE_COMMAND) -P CMakeFiles/benchmarks.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/benchmarks.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/benchmarks.dir/build: libbenchmarks.a
.PHONY : CMakeFiles/benchmarks.dir/build

CMakeFiles/benchmarks.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/benchmarks.dir/cmake_clean.cmake
.PHONY : CMakeFiles/benchmarks.dir/clean

CMakeFiles/benchmarks.dir/depend:
	cd /home/ubuntu/rfaas && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas/CMakeFiles/benchmarks.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/benchmarks.dir/depend
