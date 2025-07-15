#ifndef FPSPARTY_ALGORITHMS_UNORDERED_ERASE_HPP
#define FPSPARTY_ALGORITHMS_UNORDERED_ERASE_HPP

#include <algorithm>
#include <vector>

namespace fpsparty::algorithms {
template <typename T, typename Allocator, typename U = T>
void unordered_erase_at(std::vector<T, Allocator> &container,
                        typename std::vector<T, Allocator>::iterator iterator) {
  *iterator = std::move(container.back());
  container.pop_back();
}

template <typename T, typename Allocator, typename U = T>
bool unordered_erase_one(std::vector<T, Allocator> &container, const U &value) {
  const auto it = std::ranges::find(container, value);
  if (it != container.end()) {
    unordered_erase_at(container, it);
    return true;
  } else {
    return false;
  }
}

template <typename T, typename Allocator, typename U = T>
std::size_t unordered_erase_many(std::vector<T, Allocator> &container,
                                 const U &value) {
  auto retval = std::size_t{};
  auto it = container.begin();
  while (it != container.end()) {
    if (*it == value) {
      unordered_erase_at(container, it);
      ++retval;
    } else {
      ++it;
    }
  }
  return retval;
}
} // namespace fpsparty::algorithms

#endif
