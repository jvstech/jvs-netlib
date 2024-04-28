#if !defined(JVS_NETLIB_NETWORK_INTEGERS_H_)
#define JVS_NETLIB_NETWORK_INTEGERS_H_

#include <cassert>
#include <cstdint>
#include <type_traits>

#include "endianness.h"

namespace jvs::net
{

template <typename T>
class NetworkInteger final
{
  static_assert(std::is_integral_v<T>);
public:

  template <typename OtherT>
  friend class NetworkInteger;

  NetworkInteger() = default;
  NetworkInteger(const NetworkInteger&) = default;
  NetworkInteger(NetworkInteger&&) = default;
  NetworkInteger& operator=(const NetworkInteger&) = default;
  NetworkInteger& operator=(NetworkInteger&&) = default;

  NetworkInteger(T value)
    : network_value_(jvs::net::to_network_order(value))
  {
  }

  template <typename IntegerT>
  NetworkInteger(IntegerT value)
  {
    static_assert(std::is_integral_v<IntegerT>);
    network_value_ = jvs::net::to_network_order(static_cast<T>(value));
  }

  template <typename OtherT>
  explicit NetworkInteger(NetworkInteger<OtherT> value)
    : network_value_(jvs::net::to_network_order(
      static_cast<T>(jvs::net::to_host_order(value.network_value_))))
  {
  }

  T value() const noexcept
  {
    return jvs::net::to_host_order(network_value_);
  }

  T host_value() const noexcept
  {
    return value();
  }

  T network_value() const noexcept
  {
    return network_value_;
  }

  explicit operator T() const noexcept
  {
    return network_value_;
  }

  bool operator==(const NetworkInteger& rhs) const noexcept
  {
    return (network_value_ == rhs.network_value_);
  }

  template <typename OtherT>
  bool operator==(const NetworkInteger<OtherT>& rhs) const noexcept
  {
    return (value() == static_cast<T>(rhs.value()));
  }

  template <typename IntegerT>
  bool operator==(IntegerT rhs) const noexcept
  {
    static_assert(std::is_integral_v<IntegerT>);
    return (value() == rhs);
  }

  bool operator!=(const NetworkInteger& rhs) const noexcept
  {
    return (network_value_ != rhs.network_value_);
  }

  template <typename OtherT>
  bool operator!=(const NetworkInteger<OtherT>& rhs) const noexcept
  {
    return (value() != static_cast<T>(rhs.value()));
  }

  template <typename IntegerT>
  bool operator!=(IntegerT rhs) const noexcept
  {
    static_assert(std::is_integral_v<IntegerT>);
    return (value() != rhs);
  }

  //NetworkInteger operator&(const NetworkInteger& rhs) const noexcept
  //{
  //  return NetworkInteger(value() & rhs.value());
  //}
  //
  //template <typename OtherT>
  //NetworkInteger operator&(const NetworkInteger<OtherT>& rhs) const noexcept
  //{
  //  return NetworkInteger(value() & static_cast<T>(rhs.value()));
  //}

  template <typename IntegerT>
  NetworkInteger operator&(IntegerT rhs) const noexcept
  {
    static_assert(std::is_integral_v<IntegerT>);
    NetworkInteger result(static_cast<T>(value() & static_cast<T>(rhs)));
    return result;
  }

  template <typename IntegerT>
  NetworkInteger& operator&=(IntegerT rhs) noexcept
  {
    static_assert(std::is_integral_v<IntegerT>);
    network_value_ = 
      jvs::net::to_network_order(static_cast<T>(value() & static_cast<T>(rhs)));
    return *this;
  }

  template <typename IntegerT>
  NetworkInteger operator>>(IntegerT rhs) const noexcept
  {
    static_assert(std::is_integral_v<IntegerT>);
    return NetworkInteger(static_cast<T>(value() >> rhs));
  }

  //bool operator<(const NetworkInteger& rhs) const noexcept
  //{
  //  return (value() < rhs.value());
  //}

  //template <typename OtherT>
  //bool operator<(const NetworkInteger<OtherT>& rhs) const noexcept
  //{
  //  return (value() < static_cast<T>(rhs.value()));
  //}

  //template <typename IntegerT>
  //bool operator<(IntegerT rhs) const noexcept
  //{
  //  static_assert(std::is_integral_v<IntegerT>);
  //  return (value() < rhs);
  //}

  template <typename IntegerT>
  bool operator>(IntegerT rhs) const noexcept
  {
    static_assert(std::is_integral_v<IntegerT>);
    return (value() > rhs);
  }

  NetworkInteger operator+(const NetworkInteger& rhs) const noexcept
  {
    return NetworkInteger(value() + rhs.value());
  }

  template <typename OtherT>
  NetworkInteger operator+(const NetworkInteger<OtherT>& rhs) const noexcept
  {
    return NetworkInteger(value() + static_cast<T>(rhs.value()));
  }

  template <typename IntegerT>
  NetworkInteger operator+(IntegerT rhs) const noexcept
  {
    static_assert(std::is_integral_v<IntegerT>);
    return NetworkInteger(value() + rhs);
  }

  template <typename IntegerT>
  NetworkInteger& operator+=(IntegerT rhs) noexcept
  {
    static_assert(std::is_integral_v<IntegerT>);
    network_value_ = jvs::net::to_network_order(static_cast<T>(value() + rhs));
    return *this;
  }

  //NetworkInteger operator-(const NetworkInteger& rhs) const noexcept
  //{
  //  return NetworkInteger(value() - rhs.value());
  //}

  //NetworkInteger operator*(const NetworkInteger& rhs) const noexcept
  //{
  //  return NetworkInteger(value() * rhs.value());
  //}

  //NetworkInteger operator/(const NetworkInteger& rhs) const noexcept
  //{
  //  return NetworkInteger(value() * rhs.value());
  //}

  static NetworkInteger from_network_order(T v)
  {
    NetworkInteger result{};
    result.network_value_ = v;
    return result;
  }

private:
  T network_value_;
};

using NetworkI16 = NetworkInteger<std::int16_t>;
using NetworkI32 = NetworkInteger<std::int32_t>;
using NetworkI64 = NetworkInteger<std::int64_t>;
using NetworkU16 = NetworkInteger<std::uint16_t>;
using NetworkU32 = NetworkInteger<std::uint32_t>;
using NetworkU64 = NetworkInteger<std::uint64_t>;

template <typename DstT, typename SrcT>
DstT alias_cast(SrcT* src)
{
  // NOLINTNEXTLINE: uintptr_t is guaranteed to be the proper pointer size.
  assert(reinterpret_cast<std::uintptr_t>(src) % alignof(SrcT) == 0);

  using DstType = std::remove_pointer_t<DstT>;
  using SrcType = std::remove_pointer_t<SrcT>;

  static_assert(std::is_pointer_v<DstT>, "Destination type must be a pointer.");
  static_assert(std::is_standard_layout_v<SrcType> &&
    std::is_standard_layout_v<DstType>,
    "Source and destination types must have standard layout.");
  static_assert(alignof(SrcType) % alignof(DstType) == 0,
    "Source alignment must be equal to or a multiple of the destination "
    "alignment type.");
  static_assert(sizeof(SrcType) >= sizeof(DstType), 
    "Source type must be same size or larger than the destination type.");
  if constexpr (std::is_const_v<SrcType>)
  {
    static_assert(std::is_const_v<DstType>,
      "Destination type must be const since the source type is const.");
    return static_cast<DstT>(static_cast<const void*>(src));
  }
  
  return static_cast<DstT>(static_cast<void*>(src));
}

} // namespace jvs::net

#endif // !JVS_NETLIB_NETWORK_INTEGERS_H_
