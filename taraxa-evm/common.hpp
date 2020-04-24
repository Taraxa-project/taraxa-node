#ifndef TARAXA_NODE_TARAXA_EVM_COMMON_HPP_
#define TARAXA_NODE_TARAXA_EVM_COMMON_HPP_

#include <taraxa-evm/taraxa-evm.h>

#include <stdexcept>
#include <string>
#include <string_view>

namespace taraxa::taraxa_evm::common {
using std::runtime_error;
using std::string;
using std::string_view;

inline string_view bytes_2_str_view(taraxa_evm_Bytes const& b) {
  return {(char*)b.Data, b.Len};
}

inline string bytes_2_str(taraxa_evm_Bytes const& b) {
  return string(bytes_2_str_view(b));
}

struct err : runtime_error {
  using runtime_error::runtime_error;
};
inline taraxa_evm_BytesCallback const err_handler_c{
    nullptr,
    [](auto _, auto err_bytes) {
      taraxa_evm_Traceback({
          &err_bytes,
          [](auto err_bytes_ptr, auto traceback_bytes) {
            auto const& err_bytes = *((taraxa_evm_Bytes*)err_bytes_ptr);
            throw err("\n=== Error in Go runtime. Message:\n" +
                      bytes_2_str(err_bytes) + "\n=== Go backtrace:\n" +
                      bytes_2_str(traceback_bytes));
          },
      });
    },
};
//
}  // namespace taraxa::taraxa_evm::common

#endif  // TARAXA_NODE_TARAXA_EVM_COMMON_HPP_
