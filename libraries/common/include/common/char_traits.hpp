#pragma once

#include <cstring>
#include <string>

// Custom char_traits specialization for unsigned char
namespace std {
template <>
struct char_traits<unsigned char> {
  using char_type = unsigned char;
  using int_type = int;
  using off_type = streamoff;
  using pos_type = streampos;
  using state_type = mbstate_t;

  static void assign(char_type& c1, const char_type& c2) { c1 = c2; }

  static constexpr bool eq(char_type c1, char_type c2) noexcept { return c1 == c2; }

  static constexpr bool lt(char_type c1, char_type c2) noexcept { return c1 < c2; }

  static int compare(const char_type* s1, const char_type* s2, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      if (s1[i] < s2[i]) return -1;
      if (s1[i] > s2[i]) return 1;
    }
    return 0;
  }

  static size_t length(const char_type* s) {
    size_t len = 0;
    while (*s++) ++len;
    return len;
  }

  static const char_type* find(const char_type* s, size_t n, const char_type& a) {
    for (size_t i = 0; i < n; ++i) {
      if (s[i] == a) return s + i;
    }
    return nullptr;
  }

  static char_type* move(char_type* s1, const char_type* s2, size_t n) {
    return static_cast<char_type*>(memmove(s1, s2, n * sizeof(char_type)));
  }

  static char_type* copy(char_type* s1, const char_type* s2, size_t n) {
    return static_cast<char_type*>(memcpy(s1, s2, n * sizeof(char_type)));
  }

  static char_type* assign(char_type* s, size_t n, char_type a) {
    return static_cast<char_type*>(memset(s, a, n * sizeof(char_type)));
  }

  static constexpr int_type not_eof(int_type c) noexcept { return eq_int_type(c, eof()) ? ~eof() : c; }

  static constexpr char_type to_char_type(int_type c) noexcept { return static_cast<char_type>(c); }

  static constexpr int_type to_int_type(char_type c) noexcept { return static_cast<int_type>(c); }

  static constexpr bool eq_int_type(int_type c1, int_type c2) noexcept { return c1 == c2; }

  static constexpr int_type eof() noexcept { return static_cast<int_type>(EOF); }
};
}  // namespace std