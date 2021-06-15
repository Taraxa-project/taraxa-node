#pragma once

#include <type_traits>

#define GLOBAL_CONST(_type_, _name_) _type_ const &_name_()

#define GLOBAL_CONST_DEF(_name_, _init_)                               \
  decltype(_name_()) _name_() {                                        \
    static ::std::remove_reference_t<decltype(_name_())> val = _init_; \
    return val;                                                        \
  }
