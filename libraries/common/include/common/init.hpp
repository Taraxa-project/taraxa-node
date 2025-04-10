#pragma once

#include <sodium/core.h>
#include <sys/statvfs.h>

#include <cstdint>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace taraxa {

inline void static_init() {
  if (sodium_init() == -1) {
    throw std::runtime_error("libsodium init failure");
  }
}

inline bool checkDiskSpace(const fs::path& path, uint64_t required_space_MB) {
  try {
    std::error_code ec;
    if (!fs::exists(path) && !fs::create_directories(path, ec)) {
      std::cerr << "Error creating directory " << path << ": " << ec.message() << std::endl;
      return false;
    }
    // Retrieve disk space information for the given path
    fs::space_info spaceInfo = fs::space(path);

    // Convert the required space from MB to bytes
    uint64_t required_space_bytes = required_space_MB * 1024 * 1024;

    // Compare the available bytes with the required amount
    return spaceInfo.available >= required_space_bytes;
  } catch (const fs::filesystem_error& e) {
    // If an error occurs (e.g., the path doesn't exist), print the error message.
    std::cerr << "Filesystem error: " << e.what() << std::endl;
    // Depending on your application's requirements, you might choose to:
    // - Return false (indicating not enough space)
    // - Return true (to bypass the check)
    // - Terminate the program or rethrow the exception.
    // Here, we return true as a fallback.
    return true;
  }
}

}  // namespace taraxa
