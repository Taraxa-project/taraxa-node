# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.19

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
CMAKE_COMMAND = /usr/local/Cellar/cmake/3.19.3/bin/cmake

# The command to remove a file.
RM = /usr/local/Cellar/cmake/3.19.3/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/jsnapp/Documents/Taraxa/taraxa-node

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/jsnapp/Documents/Taraxa/taraxa-node/build

# Include any dependencies generated for this target.
include tests/CMakeFiles/p2p_test.dir/depend.make

# Include the progress variables for this target.
include tests/CMakeFiles/p2p_test.dir/progress.make

# Include the compile flags for this target's objects.
include tests/CMakeFiles/p2p_test.dir/flags.make

tests/CMakeFiles/p2p_test.dir/p2p_test.cpp.o: tests/CMakeFiles/p2p_test.dir/flags.make
tests/CMakeFiles/p2p_test.dir/p2p_test.cpp.o: ../tests/p2p_test.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/jsnapp/Documents/Taraxa/taraxa-node/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object tests/CMakeFiles/p2p_test.dir/p2p_test.cpp.o"
	cd /Users/jsnapp/Documents/Taraxa/taraxa-node/build/tests && /usr/local/bin/ccache g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/p2p_test.dir/p2p_test.cpp.o -c /Users/jsnapp/Documents/Taraxa/taraxa-node/tests/p2p_test.cpp

tests/CMakeFiles/p2p_test.dir/p2p_test.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/p2p_test.dir/p2p_test.cpp.i"
	cd /Users/jsnapp/Documents/Taraxa/taraxa-node/build/tests && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/jsnapp/Documents/Taraxa/taraxa-node/tests/p2p_test.cpp > CMakeFiles/p2p_test.dir/p2p_test.cpp.i

tests/CMakeFiles/p2p_test.dir/p2p_test.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/p2p_test.dir/p2p_test.cpp.s"
	cd /Users/jsnapp/Documents/Taraxa/taraxa-node/build/tests && g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/jsnapp/Documents/Taraxa/taraxa-node/tests/p2p_test.cpp -o CMakeFiles/p2p_test.dir/p2p_test.cpp.s

# Object files for target p2p_test
p2p_test_OBJECTS = \
"CMakeFiles/p2p_test.dir/p2p_test.cpp.o"

# External object files for target p2p_test
p2p_test_EXTERNAL_OBJECTS =

tests/p2p_test: tests/CMakeFiles/p2p_test.dir/p2p_test.cpp.o
tests/p2p_test: tests/CMakeFiles/p2p_test.dir/build.make
tests/p2p_test: src/libapp_base.a
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libcrypto.dylib
tests/p2p_test: /usr/local/lib/libmpfr.dylib
tests/p2p_test: /usr/local/lib/libgmp.dylib
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libboost_program_options.dylib
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libboost_system.dylib
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libboost_log_setup.dylib
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libboost_log.dylib
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libboost_filesystem.dylib
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libboost_thread.dylib
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libboost_atomic.dylib
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libboost_chrono.dylib
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libboost_date_time.dylib
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libboost_regex.dylib
tests/p2p_test: /usr/local/lib/librocksdb.dylib
tests/p2p_test: /usr/local/lib/libsnappy.dylib
tests/p2p_test: /usr/local/lib/libzstd.dylib
tests/p2p_test: ../for_devs/macos/system_home_override/lib/libbz2.a
tests/p2p_test: /Library/Developer/CommandLineTools/SDKs/MacOSX11.0.sdk/usr/lib/libz.tbd
tests/p2p_test: /usr/local/lib/liblz4.dylib
tests/p2p_test: /usr/local/lib/libscrypt.dylib
tests/p2p_test: /usr/local/lib/libjsoncpp.dylib
tests/p2p_test: /usr/local/lib/libjsonrpccpp-common.dylib
tests/p2p_test: /usr/local/lib/libjsonrpccpp-server.dylib
tests/p2p_test: tests/CMakeFiles/p2p_test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/jsnapp/Documents/Taraxa/taraxa-node/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable p2p_test"
	cd /Users/jsnapp/Documents/Taraxa/taraxa-node/build/tests && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/p2p_test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
tests/CMakeFiles/p2p_test.dir/build: tests/p2p_test

.PHONY : tests/CMakeFiles/p2p_test.dir/build

tests/CMakeFiles/p2p_test.dir/clean:
	cd /Users/jsnapp/Documents/Taraxa/taraxa-node/build/tests && $(CMAKE_COMMAND) -P CMakeFiles/p2p_test.dir/cmake_clean.cmake
.PHONY : tests/CMakeFiles/p2p_test.dir/clean

tests/CMakeFiles/p2p_test.dir/depend:
	cd /Users/jsnapp/Documents/Taraxa/taraxa-node/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/jsnapp/Documents/Taraxa/taraxa-node /Users/jsnapp/Documents/Taraxa/taraxa-node/tests /Users/jsnapp/Documents/Taraxa/taraxa-node/build /Users/jsnapp/Documents/Taraxa/taraxa-node/build/tests /Users/jsnapp/Documents/Taraxa/taraxa-node/build/tests/CMakeFiles/p2p_test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tests/CMakeFiles/p2p_test.dir/depend

