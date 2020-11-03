#ifndef TARAXA_NODE_UTIL_RANGE_VIEW_HPP_
#define TARAXA_NODE_UTIL_RANGE_VIEW_HPP_

#include <functional>
#include <type_traits>

namespace taraxa::util::range_view {
using namespace std;

template <typename Element>
struct RangeView {
  using size_t = std::size_t;
  using iteration_t = function<bool(Element const &)>;
  using for_each_t = function<void(iteration_t const &)>;

  size_t const size = 0;
  for_each_t const for_each_f = [](auto) {};

  RangeView() = default;
  RangeView(size_t size, for_each_t &&for_each_f)
      : size(size), for_each_f(move(for_each_f)) {}
  template <typename Seq,
            typename = enable_if_t<!is_convertible<Seq, for_each_t>::value>>
  RangeView(Seq const &seq)
      : RangeView(seq.size(), [&seq](auto const &iteration) {
          for (auto const &el : seq) {
            if (!iteration(el)) {
              return;
            }
          }
        }) {}

  void for_each(iteration_t const &iteration) const { for_each_f(iteration); }

  void for_each(
      function<bool(Element const &, decltype(size))> const &iteration) const {
    for_each_f([&, i = size_t(0)](auto const &el) mutable {
      return iteration(el, i++);
    });
  }

  void for_each(function<void(Element const &)> const &iteration) const {
    for_each_f([&](auto const &el) {
      iteration(el);
      return true;
    });
  }

  void for_each(
      function<void(Element const &, decltype(size))> const &iteration) const {
    for_each_f([&, i = size_t(0)](auto const &el) mutable {
      iteration(el, i++);
      return true;
    });
  }

  template <typename Mapper>
  auto map(Mapper &&mapper) const
      -> RangeView<decltype(mapper(declval<Element>()))> {
    return {
        size,
        [mapper = move(mapper),
         for_each_base = for_each_f](auto const &iteration) {
          for_each_base(
              [&](auto const &el_base) { return iteration(mapper(el_base)); });
        },
    };
  }

  template <typename Mapper>
  auto map(Mapper &&mapper) const
      -> RangeView<decltype(mapper(declval<Element>(), size_t{}))> {
    return {
        size,
        [mapper = move(mapper),
         for_each_base = for_each_f](auto const &iteration) {
          for_each_base([&, i = size_t(0)](auto const &el_base) mutable {
            return iteration(mapper(el_base, i++));
          });
        },
    };
  }
};

template <typename Seq>
auto make_range_view(Seq const &seq) {
  return RangeView<decltype(*seq.begin())>(seq);
}

}  // namespace taraxa::util::range_view

namespace taraxa::util {
using range_view::make_range_view;
using range_view::RangeView;
}  // namespace taraxa::util

#endif  // TARAXA_NODE_UTIL_RANGE_VIEW_HPP_
