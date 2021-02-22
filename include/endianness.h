//!
//! @file endianness.h
//! 
//! Provides utility functions for determining native system byte ordering as
//! well as reversing the byte order of integral values.
//! 

#if !defined(JVS_ENDIANNESS_H_)
#define JVS_ENDIANNESS_H_

#if (defined(__LITTLE_ENDIAN__) && (__LITTLE_ENDIAN__ == 1)) || \
  (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
#define JVS_LITTLE_ENDIAN 1
#elif (defined(__BIG_ENDIAN__) && (__BIG_ENDIAN__ == 1)) || \
  (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
#define JVS_BIG_ENDIAN 1
#endif

#if !defined(JVS_LITTLE_ENDIAN) && !defined(JVS_BIG_ENDIAN)
#include <cstdint>
#endif

namespace jvs::net
{

enum class ByteOrder
{
  LittleEndian,
  BigEndian
};

#if defined(JVS_LITTLE_ENDIAN)

inline constexpr ByteOrder byte_order()
{
  return ByteOrder::LittleEndian;
}

inline constexpr bool is_little_endian()
{
  return true;
}

inline constexpr bool is_big_endian()
{
  return false;
}

#elif defined(JVS_BIG_ENDIAN)

inline constexpr ByteOrder byte_order()
{
  return ByteOrder::BigEndian;
}

inline constexpr bool is_big_endian()
{
  return true;
}

inline constexpr bool is_little_endian()
{
  return false;
}

#else

inline static ByteOrder byte_order()
{
  static const std::uint16_t ByteOrderTestBits = 0x0001;
  const char* bytePtr = reinterpret_cast<const char*>(&ByteOrderTestBits);
  return (bytePtr[0] ? ByteOrder::LittleEndian : ByteOrder::BigEndian);
}

inline static bool is_big_endian()
{
  return (byte_order() == ByteOrder::BigEndian);
}

inline static bool is_little_endian()
{
  return (byte_order() == ByteOrder::LittleEndian);
}

#endif

template <typename T>
static constexpr T to_reverse_order(T v)
{
  static_assert(sizeof(v) == 1 ||
    sizeof(v) == 2 ||
    sizeof(v) == 4 ||
    sizeof(v) == 8 ||
    sizeof(v) == 16);

  if constexpr (sizeof(v) == 1)
  {
    return v;
  }
  else if constexpr (sizeof(v) == 2)
  {
    return (((v & 0xff) << 8) |
      ((v & 0xff00) >> 8));
  }
  else if constexpr (sizeof(v) == 4)
  {
    return (((v & 0xff) << 24) |
      ((v & 0xff00) << 8) |
      ((v & 0xff0000) >> 8) |
      ((v & 0xff000000) >> 24));
  }
  else if constexpr (sizeof(v) == 8)
  {
    return (
      ((v & 0x00000000000000ff) << 56) |
      ((v & 0x000000000000ff00) << 40) |
      ((v & 0x0000000000ff0000) << 24) |
      ((v & 0x00000000ff000000) << 8) |
      ((v & 0x000000ff00000000) >> 8) |
      ((v & 0x0000ff0000000000) >> 24) |
      ((v & 0x00ff000000000000) >> 40) |
      ((v & 0xff00000000000000) >> 56));
  }
  else if constexpr (sizeof(v) == 16)
  {
#define TO_U128(hi, lo) ((static_cast<T>(hi) << 64) | (lo))
    return (
      ((v & TO_U128(0x0000000000000000, 0x00000000000000ff)) << 120) |
      ((v & TO_U128(0x0000000000000000, 0x000000000000ff00)) << 104) |
      ((v & TO_U128(0x0000000000000000, 0x0000000000ff0000)) << 88) |
      ((v & TO_U128(0x0000000000000000, 0x00000000ff000000)) << 72) |
      ((v & TO_U128(0x0000000000000000, 0x000000ff00000000)) << 56) |
      ((v & TO_U128(0x0000000000000000, 0x0000ff0000000000)) << 40) |
      ((v & TO_U128(0x0000000000000000, 0x00ff000000000000)) << 24) |
      ((v & TO_U128(0x0000000000000000, 0xff00000000000000)) << 8) |
      ((v & TO_U128(0x00000000000000ff, 0x0000000000000000)) >> 8) |
      ((v & TO_U128(0x000000000000ff00, 0x0000000000000000)) >> 24) |
      ((v & TO_U128(0x0000000000ff0000, 0x0000000000000000)) >> 40) |
      ((v & TO_U128(0x00000000ff000000, 0x0000000000000000)) >> 56) |
      ((v & TO_U128(0x000000ff00000000, 0x0000000000000000)) >> 72) |
      ((v & TO_U128(0x0000ff0000000000, 0x0000000000000000)) >> 88) |
      ((v & TO_U128(0x00ff000000000000, 0x0000000000000000)) >> 104) |
      ((v & TO_U128(0xff00000000000000, 0x0000000000000000)) >> 120));
#undef TO_U128
  }
}

template <typename T>
static constexpr T to_network_order(T v)
{
  if (is_big_endian())
  {
    return v;
  }

  return to_reverse_order(v);
}

template <typename T>
static constexpr T to_host_order(T v)
{
  return to_network_order(v);
}

} // namespace jvs::net


#endif // !JVS_ENDIANNESS_H_
