#pragma once

#include <sodium/core.h>

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>

#include "util/util.hpp"

namespace taraxa {

inline void static_init() {
  ::signal(SIGABRT, abortHandler);
  ::signal(SIGSEGV, abortHandler);
  ::signal(SIGKILL, abortHandler);
  ::signal(SIGFPE, abortHandler);

  if (boost::filesystem::exists("./backtrace.dump")) {
    // there is a backtrace
    std::ifstream ifs("./backtrace.dump");
    std::ofstream out("./backtrace.txt");
    boost::stacktrace::stacktrace st = boost::stacktrace::stacktrace::from_dump(ifs);
    std::cout << "Previous run crashed:\n" << st << std::endl;
    out << "Previous run crashed:\n" << st << std::endl;

    // cleaning up
    ifs.close();
    boost::filesystem::remove("./backtrace.dump");
  }

  if (sodium_init() == -1) {
    throw std::runtime_error("libsodium init failure");
  }
}

}  // namespace taraxa
