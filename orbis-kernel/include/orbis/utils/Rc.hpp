#pragma once

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <type_traits>
#include <utility>

namespace orbis {
// template <typename T, typename... Args> T *knew(Args &&...args);
inline namespace utils {
void kfree(void *ptr, std::size_t size);

struct RcBase {
  std::atomic<unsigned> references{0};
  unsigned _total_size = 0; // Set by knew/kcreate

  virtual ~RcBase() = default;

  void incRef() {
    if (!_total_size)
      std::abort();
    if (references.fetch_add(1, std::memory_order::relaxed) > 4096) {
      assert(!"too many references");
    }
  }

  // returns true if object was destroyed
  bool decRef() {
    if (references.fetch_sub(1, std::memory_order::relaxed) == 1) {
      auto size = _total_size;
      this->~RcBase();
      orbis::utils::kfree(this, size);
      return true;
    }

    return false;
  }
};

template <typename T>
concept WithRc = requires(T t) {
  t.incRef();
  t.decRef();
};

template <typename T> class Ref {
  T *m_ref = nullptr;

public:
  Ref() = default;
  Ref(std::nullptr_t) noexcept {}

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref(OT *ref) noexcept : m_ref(ref) {
    if (m_ref != nullptr) {
      ref->incRef();
    }
  }

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref(const Ref<OT> &other) noexcept : m_ref(other.get()) {
    if (m_ref != nullptr) {
      m_ref->incRef();
    }
  }

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref(Ref<OT> &&other) noexcept : m_ref(other.release()) {}

  Ref(const Ref &other) noexcept : m_ref(other.get()) {
    if (m_ref != nullptr) {
      m_ref->incRef();
    }
  }
  Ref(Ref &&other) noexcept : m_ref(other.release()) {}

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref &operator=(Ref<OT> &&other) noexcept {
    other.template cast<T>().swap(*this);
    return *this;
  }

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref &operator=(OT *other) noexcept {
    *this = Ref(other);
    return *this;
  }

  template <typename OT>
    requires(std::is_base_of_v<T, OT>)
  Ref &operator=(const Ref<OT> &other) noexcept {
    *this = Ref(other);
    return *this;
  }

  Ref &operator=(const Ref &other) noexcept {
    *this = Ref(other);
    return *this;
  }

  Ref &operator=(Ref &&other) noexcept {
    other.swap(*this);
    return *this;
  }

  ~Ref() {
    if (m_ref != nullptr) {
      m_ref->decRef();
    }
  }

  void swap(Ref<T> &other) noexcept { std::swap(m_ref, other.m_ref); }
  T *get() const { return m_ref; }
  T *release() { return std::exchange(m_ref, nullptr); }
  T *operator->() const { return m_ref; }
  explicit operator bool() const { return m_ref != nullptr; }
  bool operator==(const Ref &) const = default;
  bool operator==(std::nullptr_t) const { return m_ref == nullptr; }
  auto operator<=>(const T *other) const { return m_ref <=> other; }
  auto operator<=>(const Ref &other) const = default;

  template <typename OtherT> Ref<OtherT> cast() {
    return dynamic_cast<OtherT *>(m_ref);
  }
  template <typename OtherT> Ref<OtherT> staticCast() {
    return static_cast<OtherT *>(m_ref);
  }

  template <typename OtherT> OtherT *rawCast() {
    return dynamic_cast<OtherT *>(m_ref);
  }
  template <typename OtherT> OtherT *rawStaticCast() {
    return static_cast<OtherT *>(m_ref);
  }
};

// template <WithRc T, typename... ArgsT>
//   requires(std::is_constructible_v<T, ArgsT...>)
// Ref<T> kcreate(ArgsT &&...args) {
//   return Ref<T>(knew<T>(std::forward<ArgsT>(args)...));
// }
} // namespace utils
} // namespace orbis
