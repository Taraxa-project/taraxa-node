from conans import ConanFile, CMake, tools
import platform
import shutil
import urllib.request
import tarfile
import sys
import os

class TaraxaConan(ConanFile):
    name = "taraxa-node"
    version = "0.1"
    description = "Taraxa is a Practical Byzantine Fault Tolerance blockchain."
    topics = ("blockchain", "taraxa", "crypto")
    url = "https://github.com/Taraxa-project/taraxa-node"
    homepage = "https://www.taraxa.io"
    license = "MIT"

    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"

    def _add_clang_utils_on_darwin(self):
        current_path = self.recipe_folder # dir with conanfile.py
        clang_format = "clang-format"
        clang_tidy = "clang-tidy"
        path_to_format = current_path + "/" + clang_format
        path_to_tidy = current_path + "/" + clang_tidy
        find_format = os.path.exists(path_to_format)
        find_tidy = os.path.exists(path_to_tidy)
        if not find_format or not find_tidy:
            print("downloading LLVM...")
            dirname = "clang+llvm-10.0.0-x86_64-apple-darwin"
            llvm_bin_link = "https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/" + dirname + ".tar.xz"
            ftpstream = urllib.request.urlopen(llvm_bin_link)
            thetarfile = tarfile.open(fileobj=ftpstream, mode="r|xz")
            thetarfile.extractall()
            if not find_format:
                shutil.move(dirname + "/bin/" + clang_format, path_to_format)
            if not find_tidy:
                shutil.move(dirname + "/bin/" + clang_tidy, path_to_tidy)
            shutil.rmtree(dirname)
        print("clang-format path: " + path_to_format)
        print("clang-tidy path: " + path_to_tidy)

    def requirements(self):
        self.requires("boost/1.71.0")
        self.requires("cppcheck/2.3")
        self.requires("openssl/1.1.1f")
        self.requires("cryptopp/8.4.0")
        self.requires("gtest/1.10.0")
        self.requires("rocksdb/6.8.1")
        self.requires("gmp/6.2.1")
        self.requires("mpfr/4.0.2")
        self.requires("snappy/1.1.8")
        self.requires("zstd/1.4.4")
        self.requires("lz4/1.9.2")
        self.requires("libjson-rpc-cpp/1.1.0@bincrafters/stable")

        # if it darwin we will check for some clang utils
        if platform.system() == "Darwin":
            self._add_clang_utils_on_darwin()

    def _configure_boost_libs(self):
        self.options["boost"].without_atomic = False
        self.options["boost"].without_chrono = False
        self.options["boost"].without_container = False
        self.options["boost"].without_context = True
        self.options["boost"].without_contract = True
        self.options["boost"].without_coroutine = True
        self.options["boost"].without_date_time = False
        self.options["boost"].without_exception = False
        self.options["boost"].without_fiber = True
        self.options["boost"].without_filesystem = False
        self.options["boost"].without_graph = True
        self.options["boost"].without_graph_parallel = True
        self.options["boost"].without_iostreams = True
        self.options["boost"].without_locale = False
        self.options["boost"].without_log = False
        self.options["boost"].without_math = False
        self.options["boost"].without_mpi = True
        self.options["boost"].without_program_options = False
        self.options["boost"].without_python = True
        self.options["boost"].without_random = False
        self.options["boost"].without_regex = False
        self.options["boost"].without_serialization = False
        self.options["boost"].without_stacktrace = True
        self.options["boost"].without_system = False
        self.options["boost"].without_test = True
        self.options["boost"].without_thread = False
        self.options["boost"].without_timer = True
        self.options["boost"].without_type_erasure = True
        self.options["boost"].without_wave = True

    def configure(self):
        # Configure boost
        self._configure_boost_libs()
        # Configure gtest
        self.options["gtest"].build_gmock = False
        self.options["rocksdb"].use_rtti = True
        self.options["libjson-rpc-cpp"].shared = False

    def _configure_cmake(self):
        cmake = CMake(self)
        # set find path to clang utils dowloaded by that script
        cmake.configure()
        return cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["taraxa-node"]
