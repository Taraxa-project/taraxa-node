#pragma once

#include <sodium/core.h>
#include <sys/statvfs.h>

#include "common/util.hpp"

namespace taraxa {

inline void static_init() {
  if (sodium_init() == -1) {
    throw std::runtime_error("libsodium init failure");
  }
}

inline bool checkDiskSpace(const std::string& path, uint64_t required_space_MB) {
  // Convert MB to bytes
  const uint64_t required_space_bytes = required_space_MB * 1024 * 1024;

  struct statvfs stat;

  // Get file system statistics
  if (statvfs(path.c_str(), &stat) != 0) {
    // If statvfs fails, return false
    std::cerr << "Error getting file system stats" << std::endl;
    return false;
  }

  // Calculate available space
  const uint64_t available_space = stat.f_bsize * stat.f_bavail;

  // Check if available space is greater than or equal to the required space
  return available_space >= required_space_bytes;
}

}  // namespace taraxa
