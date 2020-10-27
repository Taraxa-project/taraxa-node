#include "encoding_rlp.hpp"

namespace taraxa::util::encoding_rlp {

ErrTooManyRLPFields::ErrTooManyRLPFields(uint expected_field_count,
                                         string const &additional_err_info)
    : runtime_error([&] {
        string ret("Too many rlp fields, expected " +
                   to_string(expected_field_count));
        if (!additional_err_info.empty()) {
          ret += "\nAdditional info: " + additional_err_info;
        }
        return ret;
      }()) {}

void traverse_rlp(RLP const &rlp, uint expected_field_count,
                  function<void(RLP const &, uint)> const &cb,
                  string const &additional_err_info) {
  uint field_n = 0;
  for (auto const &el : rlp) {
    if (field_n == expected_field_count) {
      BOOST_THROW_EXCEPTION(
          ErrTooManyRLPFields(expected_field_count, additional_err_info));
    }
    cb(el, field_n++);
  }
}

//
}  // namespace taraxa::util::encoding_rlp
