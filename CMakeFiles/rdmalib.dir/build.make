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
include CMakeFiles/rdmalib.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/rdmalib.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/rdmalib.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/rdmalib.dir/flags.make

CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.o: CMakeFiles/rdmalib.dir/flags.make
CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.o: rdmalib/lib/buffer.cpp
CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.o: CMakeFiles/rdmalib.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.o -MF CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.o.d -o CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.o -c /home/ubuntu/rfaas/rdmalib/lib/buffer.cpp

CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/rdmalib/lib/buffer.cpp > CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.i

CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/rdmalib/lib/buffer.cpp -o CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.s

CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.o: CMakeFiles/rdmalib.dir/flags.make
CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.o: rdmalib/lib/connection.cpp
CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.o: CMakeFiles/rdmalib.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.o -MF CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.o.d -o CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.o -c /home/ubuntu/rfaas/rdmalib/lib/connection.cpp

CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/rdmalib/lib/connection.cpp > CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.i

CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/rdmalib/lib/connection.cpp -o CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.s

CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.o: CMakeFiles/rdmalib.dir/flags.make
CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.o: rdmalib/lib/functions.cpp
CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.o: CMakeFiles/rdmalib.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.o -MF CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.o.d -o CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.o -c /home/ubuntu/rfaas/rdmalib/lib/functions.cpp

CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/rdmalib/lib/functions.cpp > CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.i

CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/rdmalib/lib/functions.cpp -o CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.s

CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.o: CMakeFiles/rdmalib.dir/flags.make
CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.o: rdmalib/lib/rdmalib.cpp
CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.o: CMakeFiles/rdmalib.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.o -MF CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.o.d -o CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.o -c /home/ubuntu/rfaas/rdmalib/lib/rdmalib.cpp

CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/rdmalib/lib/rdmalib.cpp > CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.i

CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/rdmalib/lib/rdmalib.cpp -o CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.s

CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.o: CMakeFiles/rdmalib.dir/flags.make
CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.o: rdmalib/lib/server.cpp
CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.o: CMakeFiles/rdmalib.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.o -MF CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.o.d -o CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.o -c /home/ubuntu/rfaas/rdmalib/lib/server.cpp

CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/rdmalib/lib/server.cpp > CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.i

CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/rdmalib/lib/server.cpp -o CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.s

CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.o: CMakeFiles/rdmalib.dir/flags.make
CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.o: rdmalib/lib/util.cpp
CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.o: CMakeFiles/rdmalib.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Building CXX object CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.o"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.o -MF CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.o.d -o CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.o -c /home/ubuntu/rfaas/rdmalib/lib/util.cpp

CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.i"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/ubuntu/rfaas/rdmalib/lib/util.cpp > CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.i

CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.s"
	/usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/ubuntu/rfaas/rdmalib/lib/util.cpp -o CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.s

# Object files for target rdmalib
rdmalib_OBJECTS = \
"CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.o" \
"CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.o" \
"CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.o" \
"CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.o" \
"CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.o" \
"CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.o"

# External object files for target rdmalib
rdmalib_EXTERNAL_OBJECTS =

librdmalib.a: CMakeFiles/rdmalib.dir/rdmalib/lib/buffer.cpp.o
librdmalib.a: CMakeFiles/rdmalib.dir/rdmalib/lib/connection.cpp.o
librdmalib.a: CMakeFiles/rdmalib.dir/rdmalib/lib/functions.cpp.o
librdmalib.a: CMakeFiles/rdmalib.dir/rdmalib/lib/rdmalib.cpp.o
librdmalib.a: CMakeFiles/rdmalib.dir/rdmalib/lib/server.cpp.o
librdmalib.a: CMakeFiles/rdmalib.dir/rdmalib/lib/util.cpp.o
librdmalib.a: CMakeFiles/rdmalib.dir/build.make
librdmalib.a: CMakeFiles/rdmalib.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/ubuntu/rfaas/CMakeFiles --progress-num=$(CMAKE_PROGRESS_7) "Linking CXX static library librdmalib.a"
	$(CMAKE_COMMAND) -P CMakeFiles/rdmalib.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/rdmalib.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/rdmalib.dir/build: librdmalib.a
.PHONY : CMakeFiles/rdmalib.dir/build

CMakeFiles/rdmalib.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/rdmalib.dir/cmake_clean.cmake
.PHONY : CMakeFiles/rdmalib.dir/clean

CMakeFiles/rdmalib.dir/depend:
	cd /home/ubuntu/rfaas && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas /home/ubuntu/rfaas/CMakeFiles/rdmalib.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/rdmalib.dir/depend

