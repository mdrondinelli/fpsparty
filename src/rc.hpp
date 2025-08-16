#ifndef FPSPARTY_RC_HPP
#define FPSPARTY_RC_HPP

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <memory_resource>

namespace fpsparty::rc {
namespace detail {
struct Header {
  std::pmr::memory_resource *memory_resource{};
  std::atomic<std::uint32_t> strong_reference_count{};
  std::atomic<std::uint32_t> weak_reference_count{};
  std::uint64_t object_offset;
};

template <typename T> struct Wrapper {
  Header header;
  alignas(T) std::array<std::byte, sizeof(T)> storage;
};
} // namespace detail

template <typename T> class Strong;
template <typename T> class Weak;
template <typename T> class Factory;

namespace detail {
template <typename T> Strong<T> construct_strong(Header *header) noexcept {
  return Strong<T>{header};
}

template <typename T> Strong<T> construct_weak(Header *header) noexcept {
  return Weak<T>{header};
}
} // namespace detail

template <typename T> class Strong {
public:
  constexpr Strong() noexcept = default;

  constexpr Strong(std::nullptr_t) noexcept {}

  Strong(const Strong &other) noexcept : _header{other._header} {
    if (_header) {
      ++_header->strong_reference_count;
      ++_header->weak_reference_count;
    }
  }

  Strong &operator=(const Strong &other) noexcept {
    auto temp{other};
    swap(temp);
    return *this;
  }

  constexpr Strong(Strong &&other) noexcept
      : _header{std::exchange(other._header, nullptr)} {}

  constexpr Strong &operator=(Strong &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Strong() {
    if (_header) {
      if (--_header->strong_reference_count == 0) {
        (*this)->~T();
        if (--_header->weak_reference_count == 0) {
          auto allocator =
              std::pmr::polymorphic_allocator{_header->memory_resource};
          allocator.delete_object(_header);
        }
      }
    }
  }

  constexpr operator bool() const noexcept { return _header != nullptr; }

  operator Strong<const T>() const noexcept {
    if (_header) {
      ++_header->strong_reference_count;
      ++_header->weak_reference_count;
    }
    return detail::construct_strong<const T>(_header);
  }

  template <typename U>
  requires std::derived_from<T, U>
  operator Strong<U>() const noexcept {
    if (_header) {
      ++_header->strong_reference_count;
      ++_header->weak_reference_count;
    }
    return detail::construct_strong<U>(_header);
  }

  template <std::derived_from<T> U> Strong<U> downcast() const noexcept {
    if (_header != nullptr) {
      const auto u = dynamic_cast<U *>(operator->());
      if (u != nullptr) {
        ++_header->strong_reference_count;
        ++_header->weak_reference_count;
        return detail::construct_strong<U>(_header);
      }
    }
    return nullptr;
  }

  constexpr T &operator*() const noexcept { return *operator->(); }

  constexpr T *operator->() const noexcept {
    return std::launder(static_cast<T *>(
        static_cast<void *>(static_cast<char *>(static_cast<void *>(_header)) +
                            _header->object_offset)));
  }

  friend bool operator==(const Strong &lhs,
                         const Strong &rhs) noexcept = default;

private:
  friend class Factory<T>;
  friend class Weak<T>;

  friend Strong<T> detail::construct_strong<T>(detail::Header *header) noexcept;

  constexpr explicit Strong(detail::Header *header) noexcept
      : _header{header} {}

  constexpr void swap(Strong &other) noexcept {
    std::swap(_header, other._header);
  }

  detail::Header *_header{};
};

template <typename T> class Weak {
public:
  constexpr Weak() noexcept = default;

  constexpr Weak(std::nullptr_t) noexcept {}

  Weak(const Strong<T> &strong) : _header{strong._header} {
    if (_header) {
      ++_header->weak_reference_count;
    }
  }

  Weak(const Weak &other) noexcept : _header{other._header} {
    if (_header) {
      ++_header->weak_reference_count;
    }
  }

  Weak &operator=(const Weak &other) noexcept {
    auto temp{other};
    swap(temp);
    return *this;
  }

  constexpr Weak(Weak &&other) noexcept
      : _header{std::exchange(other._header, nullptr)} {}

  Weak &operator=(Weak &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Weak() {
    if (_header) {
      if (--_header->weak_reference_count == 0) {
        auto allocator =
            std::pmr::polymorphic_allocator{_header->memory_resource};
        allocator.delete_object(_header);
      }
    }
  }

  operator Weak<const T>() const noexcept {
    if (_header) {
      ++_header->weak_reference_count;
    }
    return detail::construct_weak<const T>(_header);
  }

  Strong<T> lock() const noexcept {
    if (_header) {
      auto old_count = _header->strong_reference_count.load();
      while (old_count != 0) {
        if (_header->strong_reference_count.compare_exchange_weak(
                old_count, old_count + 1)) {
          return detail::construct_strong<T>(_header);
        }
      }
    }
    return Strong<T>{};
  }

  friend bool operator==(const Weak &lhs, const Weak &rhs) noexcept = default;

  friend bool operator==(const Weak &lhs, const Strong<T> &rhs) noexcept {
    return lhs._header == rhs._header;
  }

  friend bool operator==(const Strong<T> &lhs, const Weak &rhs) noexcept {
    return lhs._header == rhs._header;
  }

private:
  friend Weak<T> detail::construct_weak<T>(detail::Header *header) noexcept;

  constexpr void swap(Weak &other) noexcept {
    std::swap(_header, other._header);
  }

  detail::Header *_header{};
};

template <typename T> class Factory {
public:
  Factory() noexcept : Factory{std::pmr::get_default_resource()} {}

  explicit Factory(std::pmr::memory_resource *upstream_memory_resource) noexcept
      : _memory_resource{std::make_unique<std::pmr::synchronized_pool_resource>(
            std::pmr::pool_options{
                .largest_required_pool_block = sizeof(detail::Wrapper<T>),
            },
            upstream_memory_resource)} {}

  template <typename... Args> Strong<T> create(Args &&...args) {
    auto allocator = std::pmr::polymorphic_allocator{_memory_resource.get()};
    const auto wrapper = allocator.new_object<detail::Wrapper<T>>();
    try {
      new (&wrapper->storage) T(std::forward<Args>(args)...);
    } catch (...) {
      allocator.delete_object(wrapper);
      throw;
    }
    wrapper->header.memory_resource = _memory_resource.get();
    ++wrapper->header.strong_reference_count;
    ++wrapper->header.weak_reference_count;
    wrapper->header.object_offset = offsetof(detail::Wrapper<T>, storage);
    return detail::construct_strong<T>(&wrapper->header);
  }

private:
  std::unique_ptr<std::pmr::synchronized_pool_resource> _memory_resource;
};
} // namespace fpsparty::rc

#endif
