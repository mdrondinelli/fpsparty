#ifndef FPSPARTY_RC_HPP
#define FPSPARTY_RC_HPP

#include <atomic>
#include <concepts>
#include <cstddef>
#include <memory>
#include <memory_resource>

namespace fpsparty::rc {
class Base_object;
template <typename T> class Object;
template <typename T> class Strong;
template <typename T> class Weak;
template <typename T> class Factory;

class Base_object {
public:
  Base_object() noexcept = default;

  Base_object(const Base_object &other) = delete;
  Base_object operator=(const Base_object &other) = delete;

  Base_object(Base_object &&) = delete;
  Base_object &operator=(Base_object &&) = delete;

  virtual ~Base_object() = default;

  virtual void finalize() noexcept {}

private:
  template <typename T> friend class Strong;
  template <typename T> friend class Weak;
  template <typename T> friend class Factory;

  std::pmr::memory_resource *_memory_resource{};
  std::atomic<std::size_t> _strong_reference_count{};
  std::atomic<std::size_t> _weak_reference_count{};
};

namespace detail {
template <typename T> Strong<T> construct_strong(T *object) noexcept {
  return Strong<T>{object};
}
} // namespace detail

template <typename T> class Strong {
public:
  constexpr Strong() noexcept = default;

  constexpr Strong(std::nullptr_t) noexcept {}

  Strong(const Strong &other) noexcept : _object{other._object} {
    if (_object) {
      ++_object->_strong_reference_count;
      ++_object->_weak_reference_count;
    }
  }

  Strong &operator=(const Strong &other) noexcept {
    auto temp{other};
    swap(temp);
    return *this;
  }

  constexpr Strong(Strong &&other) noexcept
      : _object{std::exchange(other._object, nullptr)} {}

  constexpr Strong &operator=(Strong &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Strong() {
    if (_object) {
      if (--_object->_strong_reference_count == 0) {
        _object->finalize();
        if (--_object->_weak_reference_count == 0) {
          auto allocator =
              std::pmr::polymorphic_allocator{_object->_memory_resource};
          allocator.delete_object(_object);
        }
      }
    }
  }

  constexpr operator bool() const noexcept { return _object != nullptr; }

  operator Strong<const T>() const noexcept {
    if (_object) {
      ++_object->_strong_reference_count;
      ++_object->_weak_reference_count;
    }
    return detail::construct_strong<const T>(_object);
  }

  template <typename U>
  requires std::derived_from<T, U>
  operator Strong<U>() const noexcept {
    if (_object) {
      ++_object->_strong_reference_count;
      ++_object->_weak_reference_count;
    }
    return detail::construct_strong<U>(_object);
  }

  template <std::derived_from<T> U> Strong<U> downcast() const noexcept {
    const auto u = dynamic_cast<U *>(_object);
    if (u != nullptr) {
      if (_object) {
        ++_object->_strong_reference_count;
        ++_object->_weak_reference_count;
      }
      return detail::construct_strong(u);
    } else {
      return nullptr;
    }
  }

  constexpr T &operator*() const noexcept { return *_object; }

  constexpr T *operator->() const noexcept { return _object; }

  friend bool operator==(const Strong &lhs,
                         const Strong &rhs) noexcept = default;

private:
  friend class Factory<T>;
  friend class Object<T>;
  friend class Weak<T>;

  friend Strong<T> detail::construct_strong(T *object) noexcept;

  constexpr explicit Strong(T *object) noexcept : _object{object} {}

  constexpr void swap(Strong &other) noexcept {
    std::swap(_object, other._object);
  }

  T *_object{};
};

template <typename T> class Weak {
public:
  constexpr Weak() noexcept = default;

  constexpr Weak(std::nullptr_t) noexcept : _object{} {}

  Weak(Strong<T> strong) : _object{strong._object} {
    if (_object) {
      ++_object->_weak_reference_count;
    }
  }

  Weak(const Weak &other) noexcept : _object{other._object} {
    if (_object) {
      ++_object->_weak_reference_count;
    }
  }

  Weak &operator=(const Weak &other) noexcept {
    auto temp{other};
    swap(temp);
    return *this;
  }

  constexpr Weak(Weak &&other) noexcept
      : _object{std::exchange(other._object, nullptr)} {}

  Weak &operator=(Weak &&other) noexcept {
    auto temp{std::move(other)};
    swap(temp);
    return *this;
  }

  ~Weak() {
    if (_object) {
      if (--_object->_weak_reference_count == 0) {
        auto allocator =
            std::pmr::polymorphic_allocator{_object->_memory_resource};
        allocator.delete_object(_object);
      }
    }
  }

  operator Weak<const T>() const noexcept {
    if (_object) {
      ++_object->_weak_reference_count;
    }
    return Weak<const T>{_object};
  }

  Strong<T> lock() const noexcept {
    if (_object) {
      auto old_count = _object->_strong_reference_count.load();
      while (old_count != 0) {
        if (_object->_strong_reference_count.compare_exchange_weak(
                old_count, old_count + 1)) {
          return Strong<T>{_object};
        }
      }
    }
    return Strong<T>{};
  }

  friend bool operator==(const Weak &lhs, const Weak &rhs) noexcept = default;

  friend bool operator==(const Weak &lhs, const Strong<T> &rhs) noexcept {
    return lhs._object == rhs._object;
  }

  friend bool operator==(const Strong<T> &lhs, const Weak &rhs) noexcept {
    return lhs._object == rhs._object;
  }

private:
  friend class Object<T>;

  constexpr void swap(Weak &other) noexcept {
    std::swap(_object, other._object);
  }

  T *_object{};
};

template <typename T> class Object : public virtual Base_object {
public:
  Strong<T> make_strong_reference() noexcept {}

  Weak<T> make_weak_reference() noexcept {}
};

template <typename T> class Factory {
public:
  Factory() noexcept : Factory{std::pmr::get_default_resource()} {}

  explicit Factory(std::pmr::memory_resource *upstream_memory_resource) noexcept
      : _memory_resource{std::make_unique<std::pmr::synchronized_pool_resource>(
            std::pmr::pool_options{
                .largest_required_pool_block = sizeof(T),
            },
            upstream_memory_resource)} {}

  template <typename... Args> Strong<T> create(Args &&...args) {
    auto allocator = std::pmr::polymorphic_allocator{_memory_resource.get()};
    const auto object = allocator.new_object<T>(std::forward<Args>(args)...);
    object->_memory_resource = _memory_resource.get();
    ++object->_strong_reference_count;
    ++object->_weak_reference_count;
    return Strong<T>{object};
  }

private:
  std::unique_ptr<std::pmr::synchronized_pool_resource> _memory_resource;
};
} // namespace fpsparty::rc

#endif
