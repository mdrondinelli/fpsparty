#ifndef FPSPARTY_RC_HPP
#define FPSPARTY_RC_HPP

#include <array>
#include <atomic>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <memory_resource>
#include <type_traits>

namespace fpsparty::rc {
namespace detail {
struct Header {
  std::pmr::memory_resource *memory_resource{};
  std::atomic<std::uint32_t> strong_reference_count{};
  std::atomic<std::uint32_t> weak_reference_count{};
  std::uint32_t wrapper_size{};
  std::uint32_t wrapper_align{};
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
template <typename T>
Strong<T> construct_strong(Header *header, T *object) noexcept {
  assert(header);
  assert(object);
  return Strong<T>{header, object};
}

template <typename T>
Strong<T> construct_weak(Header *header, T *object) noexcept {
  assert(header);
  assert(object);
  return Weak<T>{header, object};
}
} // namespace detail

template <typename T> class Strong {
public:
  constexpr Strong() noexcept = default;

  constexpr Strong(std::nullptr_t) noexcept {}

  Strong(const Strong &other) noexcept
      : _header{other._header}, _object{other._object} {
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
      : _header{std::exchange(other._header, nullptr)},
        _object{std::exchange(other._object, nullptr)} {}

  constexpr Strong &operator=(Strong &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Strong() {
    if (_header) {
      const auto new_strong_reference_count = --_header->strong_reference_count;
      const auto new_weak_reference_count = --_header->weak_reference_count;
      if (new_strong_reference_count == 0) {
        _object->~T();
        if (new_weak_reference_count == 0) {
          auto allocator =
              std::pmr::polymorphic_allocator{_header->memory_resource};
          allocator.deallocate_bytes(_header, _header->wrapper_size,
                                     _header->wrapper_align);
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
    return detail::construct_strong<const T>(_header, _object);
  }

  template <typename U>
    requires std::derived_from<T, U>
  operator Strong<U>() const noexcept {
    if (_header) {
      ++_header->strong_reference_count;
      ++_header->weak_reference_count;
    }
    return detail::construct_strong<U>(_header, _object);
  }

  template <std::derived_from<T> U> Strong<U> static_downcast() const noexcept {
    if (_header != nullptr) {
      const auto object = static_cast<U *>(_object);
      if (object != nullptr) {
        ++_header->strong_reference_count;
        ++_header->weak_reference_count;
        return detail::construct_strong<U>(_header, object);
      }
    }
    return nullptr;
  }

  template <std::derived_from<T> U>
  Strong<U> dynamic_downcast() const noexcept {
    if (_header != nullptr) {
      const auto object = dynamic_cast<U *>(_object);
      if (object != nullptr) {
        ++_header->strong_reference_count;
        ++_header->weak_reference_count;
        return detail::construct_strong<U>(_header, object);
      }
    }
    return nullptr;
  }

  constexpr T &operator*() const noexcept { return *_object; }

  constexpr T *operator->() const noexcept { return _object; }

  friend bool operator==(const Strong &lhs, const Strong &rhs) noexcept {
    return lhs._header == rhs._header;
  }

private:
  friend class Factory<T>;
  friend class Weak<T>;

  friend Strong<T> detail::construct_strong<T>(detail::Header *header,
                                               T *object) noexcept;

  constexpr explicit Strong(detail::Header *header, T *object) noexcept
      : _header{header}, _object{object} {}

  constexpr void swap(Strong &other) noexcept {
    std::swap(_header, other._header);
    std::swap(_object, other._object);
  }

  detail::Header *_header{};
  T *_object{};
};

template <typename T> class Weak {
public:
  constexpr Weak() noexcept = default;

  constexpr Weak(std::nullptr_t) noexcept {}

  Weak(const Strong<T> &strong)
      : _header{strong._header}, _object{strong._object} {
    if (_header) {
      ++_header->weak_reference_count;
    }
  }

  Weak(const Weak &other) noexcept
      : _header{other._header}, _object{other._object} {
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
      : _header{std::exchange(other._header, nullptr)},
        _object{std::exchange(other._object, nullptr)} {}

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
        allocator.deallocate_bytes(_header, _header->wrapper_size,
                                   _header->wrapper_align);
      }
    }
  }

  operator Weak<const T>() const noexcept {
    if (_header) {
      ++_header->weak_reference_count;
    }
    return detail::construct_weak<const T>(_header, _object);
  }

  Strong<T> lock() const noexcept {
    if (_header) {
      auto old_count = _header->strong_reference_count.load();
      while (old_count != 0) {
        if (_header->strong_reference_count.compare_exchange_weak(
                old_count, old_count + 1)) {
          return detail::construct_strong<T>(_header, _object);
        }
      }
    }
    return Strong<T>{};
  }

  friend bool operator==(const Weak &lhs, const Weak &rhs) noexcept {
    return lhs._header == rhs._header;
  }

  friend bool operator==(const Weak &lhs, const Strong<T> &rhs) noexcept {
    return lhs._header == rhs._header;
  }

  friend bool operator==(const Strong<T> &lhs, const Weak &rhs) noexcept {
    return lhs._header == rhs._header;
  }

private:
  friend Weak<T> detail::construct_weak<T>(detail::Header *header,
                                           T *object) noexcept;

  constexpr void swap(Weak &other) noexcept {
    std::swap(_header, other._header);
    std::swap(_object, other._object);
  }

  detail::Header *_header{};
  T *_object{};
};

namespace detail {
struct From_this_base {
  Header *header{};
};
} // namespace detail

template <typename T> class From_this : virtual detail::From_this_base {
public:
  friend class Factory<T>;

  rc::Strong<const T> strong_from_this() const {
    ++header->strong_reference_count;
    ++header->weak_reference_count;
    return detail::construct_strong<T>(header, static_cast<T *>(this));
  }

  rc::Strong<T> strong_from_this() {
    ++header->strong_reference_count;
    ++header->weak_reference_count;
    return detail::construct_strong<T>(header, static_cast<T *>(this));
  }

  rc::Weak<const T> weak_from_this() const {
    ++header->weak_reference_count;
    return detail::construct_weak<T>(header, static_cast<T *>(this));
  }

  rc::Weak<T> weak_from_this() {
    ++header->weak_reference_count;
    return detail::construct_weak<T>(header, static_cast<T *>(this));
  }
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
    const auto memory = allocator.allocate_bytes(sizeof(detail::Wrapper<T>),
                                                 alignof(detail::Wrapper<T>));
    const auto wrapper = new (memory) detail::Wrapper<T>;
    const auto object = [&]() {
      try {
        return new (&wrapper->storage) T(std::forward<Args>(args)...);
      } catch (...) {
        allocator.deallocate_bytes(wrapper, sizeof(detail::Wrapper<T>),
                                   alignof(detail::Wrapper<T>));
        throw;
      }
    }();
    if constexpr (std::is_base_of_v<detail::From_this_base, T>) {
      static_cast<detail::From_this_base *>(object)->header = &wrapper->header;
    }
    wrapper->header.memory_resource = _memory_resource.get();
    ++wrapper->header.strong_reference_count;
    ++wrapper->header.weak_reference_count;
    wrapper->header.wrapper_size = sizeof(detail::Wrapper<T>);
    wrapper->header.wrapper_align = alignof(detail::Wrapper<T>);
    return detail::construct_strong<T>(&wrapper->header, object);
  }

private:
  std::unique_ptr<std::pmr::synchronized_pool_resource> _memory_resource;
};
} // namespace fpsparty::rc

#endif
