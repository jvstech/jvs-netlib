///
/// @file ip_address.h
/// 

#if !defined(JVS_NETLIB_IP_ADDRESS_H_)
#define JVS_NETLIB_IP_ADDRESS_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "convert_cast.h"

namespace jvs::net
{

inline constexpr unsigned int Ipv4AddressSize = 4;
inline constexpr unsigned int Ipv6AddressSize = 16;

///
/// @class IpAddress
/// 
/// Implementation of an address family-agnostic (IPv4 or IPv6) IP address.
/// 
class IpAddress final
{
public:
  enum class Family
  {
    Unspecified,
    IPv4,
    IPv6
  };

  IpAddress() = default;
  
  IpAddress(const std::array<std::uint8_t, Ipv4AddressSize>& ipv4Bytes);

  IpAddress(const std::array<std::uint8_t, Ipv6AddressSize>& ipv6Bytes);

  IpAddress(const std::array<std::uint8_t, Ipv6AddressSize>& ipv6Bytes, 
    std::uint32_t scopeId);

  explicit IpAddress(std::uint32_t ipv4Bits);

  IpAddress(std::uint64_t ipv6BitsHi, std::uint64_t ipv6BitsLo);

  IpAddress(std::uint64_t ipv6BitsHi, std::uint64_t ipv6BitsLo, 
    std::uint32_t scopeId);

#if defined(__SIZEOF_INT128__)
  explicit IpAddress(__int128 ipv6Bits);

  explicit IpAddress(__int128 ipv6Bits, std::uint32_t scopeId);
#endif

  // !!! UNSAFE !!!
  // #TODO: Convert to use `std::span`.
  template <typename T>
  IpAddress(const T* bytes, Family family)
    : family_(family)
  {
    std::memcpy(&address_bytes_[0], bytes, (family_ == Family::IPv6)
      ? Ipv6AddressSize
      : Ipv4AddressSize);
  }

  //
  // accessors
  //

  const std::uint8_t* address_bytes() const noexcept;
  Family family() const noexcept;
  std::uint32_t scope_id() const noexcept;

  //
  // member functions
  //

  const std::array<std::uint8_t, Ipv6AddressSize>& address_byte_array() const 
    noexcept;
  std::size_t address_size() const noexcept;
  bool is_loopback() const noexcept;
  bool is_ipv4() const noexcept;
  bool is_ipv6() const noexcept;
  bool is_ipv6_multicast() const noexcept;
  bool is_ipv6_link_local() const noexcept;
  bool is_ipv6_site_local() const noexcept;
  bool is_ipv6_teredo() const noexcept;
  bool is_ipv4_mapped_to_ipv6() const noexcept;

  // The `/` operator is provided as a way of generating host addresses either
  // via CIDR or subnet notation.

  // CIDR notation
  IpAddress operator/(int cidr) const noexcept;
  // Subnet notation
  IpAddress operator/(const IpAddress& addr) const noexcept;

  //
  // constant IP addresses
  //

  static const IpAddress& unspecified() noexcept;
  static const IpAddress& ipv4_any() noexcept;
  static const IpAddress& ipv4_loopback() noexcept;
  static const IpAddress& ipv4_broadcast() noexcept;
  static const IpAddress& ipv4_none() noexcept;

  static const IpAddress& ipv6_any() noexcept;
  static const IpAddress& ipv6_loopback() noexcept;
  static const IpAddress& ipv6_none() noexcept;

  //
  // named constructors
  //

  static std::optional<IpAddress> parse(std::string_view ipAddressString);
  static std::optional<IpAddress> get(
    const std::initializer_list<std::uint8_t>& bytes);

private:

  static const IpAddress& ipv4_loopback_mapped_ipv6();

  // byte array large enough to hold an IPv6 address (16 bytes)
  std::array<std::uint8_t, Ipv6AddressSize> address_bytes_{};
  Family family_{Family::Unspecified};
  std::uint32_t scope_id_{0};
};

bool operator==(const IpAddress& a, const IpAddress& b) noexcept;
bool operator!=(const IpAddress& a, const IpAddress& b) noexcept;

bool is_valid_ipv4_address(std::string_view ipAddressString, bool strict);

static inline bool is_valid_ipv4_address(std::string_view ipAddressString)
{
  return is_valid_ipv4_address(ipAddressString, false);
}

bool is_valid_ipv6_address(std::string_view ipAddressString, bool strict);

static inline bool is_valid_ipv6_address(std::string_view ipAddressString)
{
  return is_valid_ipv6_address(ipAddressString, false);
}

IpAddress map_to_ipv4(const IpAddress& ipAddress) noexcept;

IpAddress map_to_ipv6(const IpAddress& ipAddress) noexcept;

std::string to_string(const IpAddress& ipAddress) noexcept;

} // namespace jvs::net

namespace jvs
{

template <>
struct ConvertCast<net::IpAddress, std::string>
{
  std::string operator()(const net::IpAddress& address) const;
};

template <>
struct ConvertCast<std::string, net::IpAddress>
{
  std::optional<net::IpAddress> operator()(
    const std::string& ipAddressString) const;
};

} // namespace jvs



// std namepace injection for std::hash<jvs::net::IpAddress>
namespace std
{

template <>
struct hash<jvs::net::IpAddress>
{
  std::size_t operator()(const jvs::net::IpAddress& addr) const noexcept;
};

} // namespace std



#endif // !JVS_NETLIB_IP_ADDRESS_H_
