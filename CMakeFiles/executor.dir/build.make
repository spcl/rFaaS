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
include CMakeFiles/executor.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/executor.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/executor.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/executor.dir/flags.make

CMakeFiles/executor.dir/server/executor/cli.cpp.o: CMakeFiles/executor.dir/flags.make
CMakeFiles/executor.dir/server/executor/cli.cpp.o: server/executor/cli.cpp
CMakeFiles/executor.dir/server/executor/cli.cpp.o: CMakeFiles/executor.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/executor.dir/server/executor/cli.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/executor.dir/server/executor/cli.cpp.o -MF CMakeFiles/executor.dir/server/executor/cli.cpp.o.d -o CMakeFiles/executor.dir/server/executor/cli.cpp.o -c /home/ubuntu/rfaas/server/executor/cli.cpp

CMakeFiles/executor.dir/server/executor/cli.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/executor.dir/server/executor/cli.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/server/executor/cli.cpp > CMakeFiles/executor.dir/server/executor/cli.cpp.i

CMakeFiles/executor.dir/server/executor/cli.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/executor.dir/server/executor/cli.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/server/executor/cli.cpp -o CMakeFiles/executor.dir/server/executor/cli.cpp.s

CMakeFiles/executor.dir/server/executor/opts.cpp.o: CMakeFiles/executor.dir/flags.make
CMakeFiles/executor.dir/server/executor/opts.cpp.o: server/executor/opts.cpp
CMakeFiles/executor.dir/server/executor/opts.cpp.o: CMakeFiles/executor.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/executor.dir/server/executor/opts.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/executor.dir/server/executor/opts.cpp.o -MF CMakeFiles/executor.dir/server/executor/opts.cpp.o.d -o CMakeFiles/executor.dir/server/executor/opts.cpp.o -c /home/ubuntu/rfaas/server/executor/opts.cpp

CMakeFiles/executor.dir/server/executor/opts.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/executor.dir/server/executor/opts.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/server/executor/opts.cpp > CMakeFiles/executor.dir/server/executor/opts.cpp.i

CMakeFiles/executor.dir/server/executor/opts.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/executor.dir/server/executor/opts.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/server/executor/opts.cpp -o CMakeFiles/executor.dir/server/executor/opts.cpp.s

CMakeFiles/executor.dir/server/executor/fast_executor.cpp.o: CMakeFiles/executor.dir/flags.make
CMakeFiles/executor.dir/server/executor/fast_executor.cpp.o: server/executor/fast_executor.cpp
CMakeFiles/executor.dir/server/executor/fast_executor.cpp.o: CMakeFiles/executor.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/executor.dir/server/executor/fast_executor.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/executor.dir/server/executor/fast_executor.cpp.o -MF CMakeFiles/executor.dir/server/executor/fast_executor.cpp.o.d -o CMakeFiles/executor.dir/server/executor/fast_executor.cpp.o -c /home/ubuntu/rfaas/server/executor/fast_executor.cpp

CMakeFiles/executor.dir/server/executor/fast_executor.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/executor.dir/server/executor/fast_executor.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/server/executor/fast_executor.cpp > CMakeFiles/executor.dir/server/executor/fast_executor.cpp.i

CMakeFiles/executor.dir/server/executor/fast_executor.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/executor.dir/server/executor/fast_executor.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/server/executor/fast_executor.cpp -o CMakeFiles/executor.dir/server/executor/fast_executor.cpp.s

CMakeFiles/executor.dir/server/executor/functions.cpp.o: CMakeFiles/executor.dir/flags.make
CMakeFiles/executor.dir/server/executor/functions.cpp.o: server/executor/functions.cpp
CMakeFiles/executor.dir/server/executor/functions.cpp.o: CMakeFiles/executor.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object CMakeFiles/executor.dir/server/executor/functions.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/executor.dir/server/executor/functions.cpp.o -MF CMakeFiles/executor.dir/server/executor/functions.cpp.o.d -o CMakeFiles/executor.dir/server/executor/functions.cpp.o -c /home/ubuntu/rfaas/server/executor/functions.cpp

CMakeFiles/executor.dir/server/executor/functions.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/executor.dir/server/executor/functions.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/server/executor/functions.cpp > CMakeFiles/executor.dir/server/executor/functions.cpp.i

CMakeFiles/executor.dir/server/executor/functions.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/executor.dir/server/executor/functions.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/server/executor/functions.cpp -o CMakeFiles/executor.dir/server/executor/functions.cpp.s

# Object files for target executor
executor_OBJECTS = \
"CMakeFiles/executor.dir/server/executor/cli.cpp.o" \
"CMakeFiles/executor.dir/server/executor/opts.cpp.o" \
"CMakeFiles/executor.dir/server/executor/fast_executor.cpp.o" \
"CMakeFiles/executor.dir/server/executor/functions.cpp.o"

# External object files for target executor
executor_EXTERNAL_OBJECTS =

bin/executor: CMakeFiles/executor.dir/server/executor/cli.cpp.o
bin/executor: CMakeFiles/executor.dir/server/executor/opts.cpp.o
bin/executor: CMakeFiles/executor.dir/server/executor/fast_executor.cpp.o
bin/executor: CMakeFiles/executor.dir/server/executor/functions.cpp.o
bin/executor: CMakeFiles/executor.dir/build.make
bin/executor: _deps/spdlog-build/libspdlogd.a
bin/executor: librdmalib.a
bin/executor: librfaaslib.a
bin/executor: librdmalib.a
bin/executor: _deps/spdlog-build/libspdlogd.a
bin/executor: /usr/lib/x86_64-linux-gnu/librdmacm.so
bin/executor: /usr/lib/x86_64-linux-gnu/libibverbs.so
bin/executor: CMakeFiles/executor.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Linking CXX executable bin/executor"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/executor.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/executor.dir/build: bin/executor
.PHONY : CMakeFiles/executor.dir/build

CMakeFiles/executor.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/executor.dir/cmake_clean.cmake
.PHONY : CMakeFiles/executor.dir/clean

CMakeFiles/executor.dir/depend:
	cd /home/ubuntu/rfaas && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas/CMakeFiles/executor.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/executor.dir/depend

