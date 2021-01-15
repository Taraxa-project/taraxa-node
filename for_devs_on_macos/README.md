## Setting up the project environment

1. Install Homebrew (brew)
2. Run `<this_directory>/installers/all.sh` to install the right versions of some dependencies that are not available via `brew`. The script will first download things to `<project_root>/local/downloads`, then install them to `<project_root>/local/system_home_override`.
3. Proceed as per [the general build instructions](../doc/building.md). When initializing the CMake build system, add these parameters to the CMake command (using `-D<param>=<value>`):
```
# This one adds the optional additional install prefix to look for dependencies
SYSTEM_HOME_OVERRIDE=<project_root>/local/system_home_override
# The following ones aren't always needed, but it's safer to use them 
CMAKE_MAKE_PROGRAM=make
CMAKE_C_COMPILER=gcc
CMAKE_CXX_COMPILER=g++
```
4. Try running a build system target of interest. It might fail due to missing packages (tools, libraries). In that case, you will find them in `brew`. It shouldn't be hard to find what needs to be installed by looking at an error.

Some known `brew` package dependencies (this list is not comprehensive):
```
coreutils, go, autoconf, automake, ccache, gflags, gdb, git, gmp, 
jsoncpp, libscrypt, libtool, make, mpfr, parallel, pkg-config,
rocksdb, snappy, zlib, zstd, libjson-rpc-cpp
```
You can also `brew install cmake`
