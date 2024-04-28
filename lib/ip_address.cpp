///
/// @file ip_address.cpp
///
/// This is mostly a translation of the .NET Core IPAddress class from C# to 
/// C++. Most comments have been kept the same (other than grammar fixes) and
/// new comments have been added. The referenced source files can be found here:
///
/// Repo: https://github.com/dotnet/runtime.git
///   * src/libraries/System.Net.Primitives/src/System/Net/IPAddress.cs
///   * src/libraries/System.Net.Primitives/src/System/Net/IPAddressParser.cs
///   * src/libraries/Common/src/System/Net/IPv4AddressHelper.Common.cs
///   * src/libraries/Common/src/System/Net/IPv6AddressHelper.Common.cs
/// 

#include <jvs-netlib/ip_address.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>

#include <jvs-netlib/endianness.h>
#include <jvs-netlib/network_integers.h>

using jvs::net::alias_cast;

namespace
{

struct Base
{
  static constexpr int Dec = 10;
  static constexpr int Oct = 8;
  static constexpr int Hex = 16;
};

// "255.255.255.255\0"
inline constexpr std::size_t Ipv4AddressLength = 16;
// "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255%4294967295\0"
inline constexpr std::size_t Ipv6AddressLength = 57;

using AddressNumbersArray = std::array<jvs::net::NetworkU16, 8>;
using AddressByteArray = std::array<std::uint8_t, 16>;

// Converts a IPv6-sized byte array to an array of NetworkU16 values without
// violating strict aliasing rules.
static AddressNumbersArray get_address_numbers(
  const AddressByteArray& addrBytes)
{
  AddressNumbersArray result{};
  auto arySize = (jvs::net::Ipv6AddressSize >> 1);
  for (int i = 0; i < arySize; ++i)
  {
    result[i] = static_cast<std::uint16_t>(
      (addrBytes[i << 1] << 8) | addrBytes[(i << 1) + 1]);
  }

  return result;
}

static inline bool equals_zero(const jvs::net::NetworkU16& v)
{
  return (v == 0);
}

template <typename T>
static void fill_bytes(std::uint8_t* dst, T bytes)
{
  assert(dst);
  int dataSize = sizeof(T);
  if constexpr (sizeof(T) > jvs::net::Ipv6AddressSize)
  {
    dataSize = jvs::net::Ipv6AddressSize;
  }

  for (int i = 0; i < dataSize; ++i)
  {
    int shiftAmt = ((dataSize - (i + 1)) * 8);
    dst[i] = 
      static_cast<std::uint8_t>((bytes & (0xff << shiftAmt)) >> shiftAmt);
  }
}

static constexpr bool is_hex_digit(char ch)
{
  return (((ch >= '0') && (ch <= '9')) ||
    ((ch >= 'A') && (ch <= 'F')) ||
    ((ch >= 'a') && (ch <= 'f')));
}

template <typename T = int>
static std::optional<T> from_hex(char ch)
{
  if (is_hex_digit(ch))
  {
    return (ch <= '9')
      ? (static_cast<T>(ch) - static_cast<T>('0'))
      : (((ch <= 'F')
        ? (static_cast<T>(ch) - static_cast<T>('A'))
        : (static_cast<T>(ch) - static_cast<T>('a')))
        + 10);
  }

  return {};
}

static std::pair<int, int> find_compression_range(
  const jvs::net::NetworkU16* numbers, std::size_t length)
{
  assert(numbers);
  if (length > Ipv6AddressLength)
  {
    length = Ipv4AddressLength;
  }

  int longestSequenceLength{0};
  int longestSequenceStart{-1};
  int currentSequenceLength{0};

  for (int i = 0; i < length; ++i)
  {
    if (numbers[i] == 0)
    {
      ++currentSequenceLength;
      if (currentSequenceLength > longestSequenceLength)
      {
        longestSequenceLength = currentSequenceLength;
        longestSequenceStart = i - currentSequenceLength + 1;
      }
    }
    else
    {
      currentSequenceLength = 0;
    }
  }
  
  return (longestSequenceLength > 1
    ? std::make_pair(
        longestSequenceStart, longestSequenceStart + longestSequenceLength)
    : std::make_pair(-1, -1));
}

static void append_hex(const jvs::net::NetworkU16& v, std::string& buffer)
{
  auto toHexCharLower = [](jvs::net::NetworkU16 value)
  {
    value &= 0xF;
    value += '0';
    if (value > '9')
    {
      value += ('a' - ('9' + 1));
    }

    return static_cast<char>(value.value());
  };

  // convert to host-order before making adjustments
  if ((v & 0xf000) != 0)
  {
    buffer.push_back(toHexCharLower(v >> 12));
  }

  if ((v & 0xff00) != 0)
  {
    buffer.push_back(toHexCharLower(v >> 8));
  }

  if ((v & 0xfff0) != 0)
  {
    buffer.push_back(toHexCharLower(v >> 4));
  }

  buffer.push_back(toHexCharLower(v));
}

static void append_ipv6_sections(const jvs::net::NetworkU16* address, 
  int fromInclusive, int toExclusive, std::string& buffer)
{
  const jvs::net::NetworkU16* addressSpan{address + fromInclusive};
  int spanLength = toExclusive - fromInclusive;
  bool needsColon{false};
  auto [zeroStart, zeroEnd] = find_compression_range(addressSpan, spanLength);

  for (int i = fromInclusive; i < zeroStart; ++i)
  {
    if (needsColon)
    {
      buffer.push_back(':');
    }

    needsColon = true;
    append_hex(address[i], buffer);
  }

  // Output the zero sequence if there is one.
  if (zeroStart >= 0)
  {
    buffer.append("::");
    needsColon = false;
    fromInclusive = zeroEnd;
  }

  // Output everything after the zero sequence
  for (int i = fromInclusive; i < toExclusive; ++i)
  {
    if (needsColon)
    {
      buffer.push_back(':');
    }

    needsColon = true;
    append_hex(address[i], buffer);
  }
}

static bool is_valid_ipv4_canonical(std::string_view addrStr, int start, 
  int& end, bool allowIpv6, bool notImplicit)
{
  int dotCount{0};
  int number{0};
  bool hasNumber{false};
  bool firstCharIsZero{false};

  while (start < end)
  {
    char ch = addrStr[start];
    if (allowIpv6)
    {
      if (ch == ']' || ch == '/' || ch == '%')
      {
        break;
      }
    }
    else if (ch == '/' || ch == '\\' ||
      (notImplicit && (ch == ':' || ch == '?' || ch == '#')))
    {
      break;
    }

    if (ch <= '9' && ch >= '0')
    {
      if (!hasNumber && (ch == '0'))
      {
        if ((start + 1 < end) && addrStr[start + 1] == '0')
        {
          // 00 is not allowed as a prefix.
          return false;
        }

        firstCharIsZero = true;
      }

      hasNumber = true;
      number = number * 10 + (addrStr[start] - '0');
      if (number > 255)
      {
        return false;
      }
    }
    else if (ch == '.')
    {
      if (!hasNumber || (number > 0 && firstCharIsZero))
      {
        // 0 is not allowed to prefix a number.
        return false;
      }

      ++dotCount;
      hasNumber = false;
      number = 0;
      firstCharIsZero = false;
    }
    else
    {
      return false;
    }

    ++start;
  }

  bool result = (dotCount == 3) && hasNumber;
  if (result)
  {
    end = start;
  }

  return result;
}

// Extracts an IPv4 address from the given bytes
static std::string ipv4_to_string(const std::uint8_t* addrBytes)
{
  assert(addrBytes);
  std::string result{};
  result.reserve(Ipv4AddressLength);
  result
    .append(std::to_string(static_cast<int>(addrBytes[0])))
    .append(".")
    .append(std::to_string(static_cast<int>(addrBytes[1])))
    .append(".")
    .append(std::to_string(static_cast<int>(addrBytes[2])))
    .append(".")
    .append(std::to_string(static_cast<int>(addrBytes[3])))
    ;
  result.shrink_to_fit();
  return result;
}

static std::string ipv4_to_string(const jvs::net::IpAddress& addr)
{
  assert(addr.family() == jvs::net::IpAddress::Family::IPv4);
  return ipv4_to_string(addr.address_bytes());
}

static std::string ipv6_to_string(const jvs::net::IpAddress& addr)
{
  assert(addr.family() == jvs::net::IpAddress::Family::IPv6);
  auto numbers = get_address_numbers(addr.address_byte_array());
  //const std::uint16_t* numbers =
  //  alias_cast<const std::uint16_t*>(addr.address_bytes());

  bool hasEmbeddedIpv4{false};
  std::string buffer{};
  buffer.reserve(Ipv6AddressLength);
  if (numbers[6] != 0 && 
    std::all_of(numbers.begin(), numbers.begin() + 5, equals_zero))
  {
    // RFC 5952 Section 5 - 0:0 : 0:0 : 0:[0 | FFFF] : x.x.x.x
    // SIIT - 0:0 : 0:0 : FFFF:0 : x.x.x.x
    if ((numbers[4] == 0 && (numbers[5] == 0 || numbers[5] == 0xffff)) ||
      (numbers[4] == 0xffff && numbers[5] == 0))
    {
      hasEmbeddedIpv4 = true;
    }
  }
  else if (numbers[4] == 0 &&
    numbers[5] ==
      jvs::net::to_network_order(static_cast<std::uint16_t>(0x5efe)))
  {
    // ISATAP
    hasEmbeddedIpv4 = true;
  }

  if (hasEmbeddedIpv4)
  {
    append_ipv6_sections(numbers.data(), 0, 6, buffer);
    if (buffer.back() != ':')
    {
      buffer.push_back(':');
    }

    buffer.append(ipv4_to_string(addr.address_bytes() + 12));
  }
  else
  {
    // No IPv4 address.  Output all 8 sections as part of the IPv6 address
    // with normal formatting rules.
    append_ipv6_sections(numbers.data(), 0, 8, buffer);
  }

  // If there's a scope ID, append it.
  if (addr.scope_id() != 0)
  {
    buffer.push_back('%');
    buffer.append(std::to_string(addr.scope_id()));
  }

  buffer.shrink_to_fit();
  return buffer;
}

static bool parse_ipv4(std::string_view ipAddressString, int start, int& end,
  bool notImplicit, std::uint8_t* dst);

static bool is_valid_ipv4(std::string_view addrStr, int start, int& end, 
  bool allowIpv6, bool notImplicit, bool unknownScheme)
{
  if (allowIpv6 || unknownScheme)
  {
    return is_valid_ipv4_canonical(addrStr, start, end, allowIpv6, notImplicit);
  }
  else
  {
    return parse_ipv4(addrStr, start, end, notImplicit, nullptr);
  }
}

static bool is_valid_ipv6(std::string_view addrStr, int start, int& end,
  bool strict)
{
  if (addrStr.empty())
  {
    return false;
  }

  int sequenceCount {0};
  int sequenceLength{0};
  bool hasCompressor{false};
  bool hasIpv4Address{false};
  bool hasPrefix{false};
  bool expectingNumber{true};
  int lastSequence{1};
  
  if (start < end && addrStr[start] == '[')
  {
    ++start;
    if (start >= end)
    {
      return false;
    }
  }

  if (addrStr[start] == ':' &&
    (start + 1 >= end || addrStr[start + 1] != ':') && strict)
  {
    return false;
  }

  int i = start;
  for (; i < end; ++i)
  {
    if (hasPrefix
      ? (addrStr[i] >= '0' && addrStr[i] <= '9')
      : is_hex_digit(addrStr[i]))
    {
      ++sequenceLength;
      expectingNumber = false;
    }
    else
    {
      if (sequenceLength > 4)
      {
        return false;
      }

      if (sequenceLength != 0)
      {
        ++sequenceCount;
        lastSequence = i - sequenceLength;
      }

      auto handleTerminator = [&]()
      {
        start = i;
        i = end;
      };

      auto handlePrefix = [&]()
      {
        if (strict)
        {
          return false;
        }

        if ((sequenceCount == 0) || hasPrefix)
        {
          return false;
        }

        hasPrefix = true;
        expectingNumber = true;
        return true;
      };

      bool shouldContinue{false};
      switch (addrStr[i])
      {
      case '%':
        for (;;)
        {
          if (++i == end)
          {
            // no closing ']'
            return false;
          }

          if (addrStr[i] == ']')
          {
            handleTerminator();
            shouldContinue = true;
            break;
          }
          else if (addrStr[i] == '/')
          {
            if (!handlePrefix())
            {
              return false;
            }

            break;
          }
        }

        if (shouldContinue)
        {
          shouldContinue = false;
          continue;
        }

        break;

      case ']':
        handleTerminator();
        continue;

      case ':':
        if ((i > 0) && (addrStr[i - 1] == ':'))
        {
          if (hasCompressor)
          {
            // Already have a compressor, so this is an invalid IPv6 address.
            return false;
          }

          hasCompressor = true;
          expectingNumber = false;
        }
        else
        {
          expectingNumber = true;
        }

        break;

      case '/':
        if (!handlePrefix())
        {
          return false;
        }

        break;

      case '.':
        if (hasIpv4Address)
        {
          return false;
        }

        i = end;
        if (!is_valid_ipv4(addrStr, lastSequence, i, /*allowIpv6 =*/ true, 
          /*notImplicit =*/ false, /*unknownScheme =*/ false))
        {
          return false;
        }

        // IPv4 addresses take 2 slots in IPv6 addresses; one is counted meeting
        // the '.'
        ++sequenceCount;
        hasIpv4Address = true;
        --i;
        break;

      default:
        return false;
      }

      sequenceLength = 0;
    }
  }

  // If the last token was a prefix, check the number of digits.
  if (hasPrefix && ((sequenceLength < 1) || (sequenceLength > 2)))
  {
    return false;
  }

  int expectedSequenceCount = 8 + (hasPrefix ? 1 : 0);
  if (!expectingNumber && (sequenceLength <= 4) &&
    (hasCompressor ? (sequenceCount < expectedSequenceCount)
                   : (sequenceCount == expectedSequenceCount)))
  {
    if (i == end + 1)
    {
      // ']' was found
      end = start + 1;
      return true;
    }

    return false;
  }

  return false;
}

static bool parse_ipv4_canonical(std::string_view addrStr,
  std::uint8_t* numbers, int start, int end);

static bool parse_ipv6(std::string_view addrStr, std::uint16_t* numbers,
  int start, std::string& scopeId)
{
  assert(numbers);

  int end = addrStr.length();

  bool isValid = is_valid_ipv6(addrStr, 0, end, true);
  if (isValid || (end != addrStr.length()))
  {
    std::uint32_t number{0};
    int index{0};
    int compressorIndex{-1};
    bool validNumber{true};
    int prefixLength{0};
    if (addrStr[start] == '[')
    {
      ++start;
    }

    for (int i = start; i < addrStr.length() && addrStr[i] != ']';)
    {
      switch (addrStr[i])
      {
      case '%':
        if (validNumber)
        {
          numbers[index++] =
            jvs::net::to_network_order(static_cast<std::uint16_t>(number));
          validNumber = false;
        }

        start = i;
        for (++i;
          i < addrStr.length() && addrStr[i] != ']' && addrStr[i] != '/';
          ++i)
        {
        }

        scopeId = addrStr.substr(start, i - start);
        // ignore any prefix
        for (; i < addrStr.length() && addrStr[i] != ']'; ++i)
        {
        }

        break;

      case ':':
        numbers[index++] =
          jvs::net::to_network_order(static_cast<std::uint16_t>(number));
        number = 0;
        ++i;
        if (addrStr[i] == ':')
        {
          compressorIndex = index;
          ++i;
        }
        else if ((compressorIndex < 0) && (index < 6))
        {
          // No compressor? Already parsed 6 16-bit numbers? No need to check 
          // for an IPv4 address.
          break;
        }

        // Check to see if the upcoming number is safe to parse as an IPv4
        // address.
        for (int j = i;
          j < addrStr.length() &&
          addrStr[j] != ']' &&
          addrStr[j] != ':' &&
          addrStr[j] != '%' &&
          addrStr[j] != '/' &&
          (j < i + jvs::net::Ipv4AddressSize);
          ++j)
        {
          if (addrStr[j] == '.')
          {
            // Found an IPv4 address
            // Find the end of the IPv4 address
            for (; j < addrStr.length() && (addrStr[j] != ']') &&
                 (addrStr[j] != '/') && (addrStr[j] != '%');
                 ++j)
            {
            }

            std::array<std::uint8_t, jvs::net::Ipv4AddressSize> byteNumbers{};
            parse_ipv4_canonical(addrStr, &byteNumbers[0], i, j);
            std::memcpy(
              numbers + index, byteNumbers.data(), jvs::net::Ipv4AddressSize);
            // No need to convert to network order; it's already in that order
            // from parse_ipv4_canonical().            
            index += 2;
            i = j;

            // Set this to avoid adding another number to the array if there's a
            // prefix.
            number = 0;
            validNumber = false;
            break;
          }
        }

        break;

      case '/':
        if (validNumber)
        {
          numbers[index++] =
            jvs::net::to_network_order(static_cast<std::uint16_t>(number));
          validNumber = false;
        }

        // Since we have a valid IPv6 address string, the prefix length is the
        // last token in the string.
        for (++i; addrStr[i] != ']'; ++i)
        {
          prefixLength = prefixLength * 10 + (addrStr[i] - '0');
        }

        break;

      default:
        if (auto numberVal = from_hex(addrStr[i++]))
        {
          number = number * 16 + *numberVal;
          break;
        }

        return false;
      }
    }

    // Add the number to the array if its not the prefix length or part of an
    // IPv4 address that's already been handled.
    if (validNumber)
    {
      numbers[index++] =
        jvs::net::to_network_order(static_cast<std::uint16_t>(number));
    }

    // If we had a compressor sequence ("::"), we need to expand the numbers
    // array.
    if (compressorIndex > 0)
    {
      int toIndex = 7; // 8 labels - 1
      int fromIndex = index - 1;

      // If fromIndex and toIndex are the same, it means that "zero bits" are
      // already in the correct place for leading and trailing compression.
      if (fromIndex != toIndex)
      {
        for (int i = index - compressorIndex; i > 0; --i)
        {
          numbers[toIndex--] = numbers[fromIndex];
          numbers[fromIndex--] = 0;
        }
      }
    }
    
    return true;
  }


  return false;
}

static bool parse_ipv6(std::string_view ipAddressString, int start, int& end,
  bool strict, std::uint8_t* dst, std::uint32_t& dstScopeId)
{
  assert(dst);
  if (ipAddressString.empty())
  {
    return false;
  }

  constexpr int LabelCount = 8;

  if (is_valid_ipv6(ipAddressString, start, end, /*strict =*/ true))
  {
    std::array<std::uint16_t, LabelCount> numbers{};
    std::string scopeId{};
    parse_ipv6(ipAddressString, &numbers[0], 0, scopeId);
    if (scopeId.empty())
    {
      dstScopeId = 0;
    }
    else
    {
      scopeId = scopeId.substr(1);
      std::stringstream ss(scopeId);
      std::uint32_t scopeIdValue;
      if (ss >> scopeIdValue)
      {
        dstScopeId = scopeIdValue;
      }
    }

    // Address numbers are already in network order, so just copy the bytes
    // directly.
    std::memcpy(dst, numbers.data(), numbers.size() * 2);
    return true;
  }

  return false;
}

static bool parse_ipv6(std::string_view ipAddressString, std::uint8_t* dst,
  std::uint32_t& dstScopeId)
{
  int end = ipAddressString.size();
  return parse_ipv6(ipAddressString, 0, end, /*strict =*/ false, dst, 
    dstScopeId);
}

static bool parse_ipv4(std::string_view ipAddressString, int start, int& end,
  bool notImplicit, std::uint8_t* dst)
{
  if (ipAddressString.empty())
  {
    return false;
  }

  int base = Base::Dec;
  char ch;
  std::uint32_t parts[jvs::net::Ipv4AddressSize];
  std::uint32_t currentValue = 0;
  std::uint32_t result;
  bool atLeastOneChar = false;
  int dotCount = 0;
  int current = start;

  for (; current < end; ++current)
  {
    ch = ipAddressString[current];
    currentValue = 0;
    base = Base::Dec;
    if (ch == '0')
    {
      base = Base::Oct;
      ++current;
      atLeastOneChar = true;
      if (current < end)
      {
        ch = ipAddressString[current];
        if (ch == 'x' || ch == 'X')
        {
          base = Base::Hex;
          ++current;
          atLeastOneChar = false;
        }
      }
    }

    for (; current < end; ++current)
    {
      ch = ipAddressString[current];
      int digitValue;

      if ((base == Base::Dec || base == Base::Hex) && '0' <= ch && ch <= '9')
      {
        digitValue = ch - '0';
      }
      else if (base == Base::Oct && '0' <= ch && ch <= '7')
      {
        digitValue = ch - '0';
      }
      else if (base == Base::Hex && 'a' <= ch && ch <= 'f')
      {
        digitValue = ch + 10 - 'a';
      }
      else if (base == Base::Hex && 'A' <= ch && ch <= 'F')
      {
        digitValue = ch + 10 - 'A';
      }
      else
      {
        // Invalid/terminator
        break;
      }

      currentValue = (currentValue * base) + digitValue;

      if (currentValue > std::numeric_limits<std::uint32_t>::max())
      {
        return false;
      }

      atLeastOneChar = true;
    }

    if (current < end && ipAddressString[current] == '.')
    {
      if (dotCount >= 3 || !atLeastOneChar || currentValue > 0xff)
      {
        return false;
      }

      parts[dotCount] = currentValue;
      ++dotCount;
      atLeastOneChar = false;
      continue;
    }

    break;
  }

  if (!atLeastOneChar)
  {
    return false;
  }
  else if (current >= end)
  {
    // Nothing to do
  }
  else if ((ch = ipAddressString[current]) == '/' || ch == '\\' || 
    (notImplicit && (ch == ':' || ch == '?' || ch == '#')))
  {
    end = current;
  }
  else
  {
    return false;
  }

  parts[dotCount] = currentValue;

  switch (dotCount)
  {
  case 0:
    if (parts[0] > std::numeric_limits<std::uint32_t>::max())
    {
      return false;
    }

    result = parts[0];
    break;

  case 1:
    if (parts[1] > 0xffffff)
    {
      return false;
    }

    result = ((parts[0] << 24) | (parts[1] & 0xffffff));
    break;

  case 2:
    if (parts[2] > 0xffff)
    {
      return false;
    }

    result =
      ((parts[0] << 24) | ((parts[1] & 0xff) << 16) | (parts[2] & 0xffff));
    break;
  case 3:
    if (parts[3] > 0xff)
    {
      return false;
    }

    result = ((parts[0] << 24) | ((parts[1] & 0xff) << 16) | 
      ((parts[2] & 0xff) << 8) | (parts[3] & 0xff));
    break;

  default:
    return false;
  }

  if (dst)
  {
    fill_bytes(dst, result);
  }

  return true;
}

static bool parse_ipv4(std::string_view ipAddressString, std::uint8_t* dst)
{
  int end = ipAddressString.size();
  return parse_ipv4(ipAddressString, 0, end, true, dst);
}

// Returns true if the IPv4 address is a localhost
static bool parse_ipv4_canonical(std::string_view addrStr,
  std::uint8_t* numbers, int start, int end)
{
  assert(numbers);
  for (int i = 0; i < jvs::net::Ipv4AddressSize; ++i)
  {
    int b{0};
    char ch;
    for (; (start < end) && (ch = addrStr[start]) != '.' && ch != ':'; ++start)
    {
      b = (b * 10) + ch - '0';
    }

    numbers[i] = static_cast<std::uint8_t>(b);
    ++start;
  }

  return (numbers[0] == 127);
}

static std::uint64_t operator "" _u64(unsigned long long v)
{
  return static_cast<std::uint64_t>(v);
}

static std::uint32_t operator "" _u32(unsigned long long v)
{
  return static_cast<std::uint32_t>(v);
}

} // namespace 


jvs::net::IpAddress::IpAddress(
  const std::array<std::uint8_t, jvs::net::Ipv4AddressSize>& ipv4Bytes)
  : family_(Family::IPv4)
{
  std::copy_n(
    ipv4Bytes.begin(), jvs::net::Ipv4AddressSize, address_bytes_.begin());
}

jvs::net::IpAddress::IpAddress(
  const std::array<std::uint8_t, jvs::net::Ipv6AddressSize>& ipv6Bytes)
  : IpAddress(ipv6Bytes, 0)
{
}

jvs::net::IpAddress::IpAddress(
  const std::array<std::uint8_t, jvs::net::Ipv6AddressSize>& ipv6Bytes, 
  std::uint32_t scopeId)
  : family_(Family::IPv6),
  scope_id_(scopeId)
{
  std::copy_n(
    ipv6Bytes.begin(), jvs::net::Ipv6AddressSize, address_bytes_.begin());
}

jvs::net::IpAddress::IpAddress(std::uint32_t ipv4Bits)
  : family_(Family::IPv4)
{
  fill_bytes(&address_bytes_[0], ipv4Bits);
}

jvs::net::IpAddress::IpAddress(std::uint64_t ipv6BitsHi,
  std::uint64_t ipv6BitsLo)
  : IpAddress(ipv6BitsHi, ipv6BitsLo, 0)
{
}

jvs::net::IpAddress::IpAddress(std::uint64_t ipv6BitsHi, 
  std::uint64_t ipv6BitsLo, std::uint32_t scopeId)
  : family_(Family::IPv6),
  scope_id_(scopeId)
{
  fill_bytes(&address_bytes_[0], ipv6BitsHi);
  fill_bytes(&address_bytes_[8], ipv6BitsLo);
}

#if defined(__SIZEOF_INT128__)

jvs::net::IpAddress::IpAddress(__int128 ipv6Bits)
  : family_(Family::IPv6),
  scope_id_(0)
{
  fill_bytes(&address_bytes_[0], ipv6Bits);
}

jvs::net::IpAddress::IpAddress(__int128 ipv6Bits, std::uint32_t scopeId)
  : family_(Family::IPv6),
  scope_id_(scopeId)
{
  fill_bytes(&address_bytes_[0], ipv6Bits);
}

#endif  // __SIZEOF_INT128__

const std::uint8_t* jvs::net::IpAddress::address_bytes() const noexcept
{
  return address_bytes_.data();
}

jvs::net::IpAddress::Family jvs::net::IpAddress::family() const noexcept
{
  return family_;
}

std::uint32_t jvs::net::IpAddress::scope_id() const noexcept
{
  return scope_id_;
}

auto jvs::net::IpAddress::address_byte_array() const noexcept
  -> const std::array<std::uint8_t, jvs::net::Ipv6AddressSize>&
{
  return address_bytes_;
}

std::size_t jvs::net::IpAddress::address_size() const noexcept
{
  switch (family_)
  {
  case Family::IPv4:
    return jvs::net::Ipv4AddressSize;
  case Family::IPv6:
    return jvs::net::Ipv6AddressSize;
  case Family::Unspecified:
    return 0;
  }
}

bool jvs::net::IpAddress::is_loopback() const noexcept
{
  switch (family_)
  {
  case Family::IPv4:
    return (address_bytes_[0] == 127);
  case Family::IPv6:
    return (*this == ipv6_loopback() || *this == ipv4_loopback_mapped_ipv6());
  case Family::Unspecified:
    return false;
  }
}

bool jvs::net::IpAddress::is_ipv4() const noexcept
{
  return (family_ == Family::IPv4);
}

bool jvs::net::IpAddress::is_ipv6() const noexcept
{
  return (family_ == Family::IPv6);
}

bool jvs::net::IpAddress::is_ipv6_multicast() const noexcept
{
  auto addrNums = get_address_numbers(address_bytes_);
  return (is_ipv6() &&
    ((addrNums[0] & 0xff00) == 0xff00));
}

bool jvs::net::IpAddress::is_ipv6_link_local() const noexcept
{
  auto addrNums = get_address_numbers(address_bytes_);
  return (is_ipv6() &&
    ((addrNums[0] & 0xffc0) == 0xfe80));
}

bool jvs::net::IpAddress::is_ipv6_site_local() const noexcept
{
  auto addrNums = get_address_numbers(address_bytes_);
  return (is_ipv6() &&
    (addrNums[0] & 0xffc0) == 0xfec0);
}

bool jvs::net::IpAddress::is_ipv6_teredo() const noexcept
{
  auto addrNums = get_address_numbers(address_bytes_);
  return (is_ipv6() &&
    (addrNums[0] == 0x2001) &&
    (addrNums[1] == 0));
}

bool jvs::net::IpAddress::is_ipv4_mapped_to_ipv6() const noexcept
{
  if (is_ipv4())
  {
    return false;
  }

  auto addrNums = get_address_numbers(address_bytes_);
  if (!std::all_of(addrNums.begin(), addrNums.begin() + 5, equals_zero))
  {
    return false;
  }

  return (addrNums[5] == 0xffff);
}

jvs::net::IpAddress jvs::net::IpAddress::operator/(int cidr) const noexcept
{
  if (is_ipv6() || cidr >= 32)
  {
    return *this;
  }

  if (cidr <= 0)
  {
    return IpAddress::ipv4_any();
  }

  IpAddress subnetMask(~((1_u32 << (32 - cidr)) - 1));
  return (*this / subnetMask);
}

jvs::net::IpAddress jvs::net::IpAddress::operator/(const IpAddress& addr) const 
  noexcept
{
  if (is_ipv6() || addr.is_ipv6())
  {
    return *this;
  }

  if (addr == IpAddress::ipv4_any())
  {
    return IpAddress::ipv4_any();
  }

  if (addr == IpAddress::ipv4_none())
  {
    return *this;
  }

  IpAddress result{*this};
  for (int i = 0; i < Ipv4AddressSize; ++i)
  {
    result.address_bytes_[i] &= addr.address_bytes_[i];
  }

  return result;
}

const jvs::net::IpAddress& jvs::net::IpAddress::unspecified() noexcept
{
  static IpAddress UnspecifiedAddress{};
  return UnspecifiedAddress;
}

const jvs::net::IpAddress& jvs::net::IpAddress::ipv4_any() noexcept
{
  // 0.0.0.0
  static IpAddress Ipv4Any{0_u32};
  return Ipv4Any;
}

const jvs::net::IpAddress& jvs::net::IpAddress::ipv4_loopback() noexcept
{
  // 127.0.0.1
  static IpAddress Ipv4Loopback{static_cast<std::uint32_t>((127 << 24) | 1)};
  return Ipv4Loopback;
}

const jvs::net::IpAddress& jvs::net::IpAddress::ipv4_broadcast() noexcept
{
  // 255.255.255.255
  static IpAddress Ipv4Broadcast{0xffffffff_u32};
  return Ipv4Broadcast;
}

const jvs::net::IpAddress& jvs::net::IpAddress::ipv4_none() noexcept
{
  // 255.255.255.255
  return ipv4_broadcast();
}

const jvs::net::IpAddress& jvs::net::IpAddress::ipv6_any() noexcept
{
  static IpAddress Ipv6Any{0_u64, 0_u64};
  return Ipv6Any;
}

const jvs::net::IpAddress& jvs::net::IpAddress::ipv6_loopback() noexcept
{
  static IpAddress Ipv6Loopback{0_u64, 1_u64};
  return Ipv6Loopback;
}

const jvs::net::IpAddress& jvs::net::IpAddress::ipv6_none() noexcept
{
  static IpAddress Ipv6None{0_u64, 0_u64};
  return Ipv6None;
}

const jvs::net::IpAddress& jvs::net::IpAddress::ipv4_loopback_mapped_ipv6()
{
  static IpAddress Ipv6MappedLoopback{0_u64, 0xffff7f000001_u64};
  return Ipv6MappedLoopback;
}

jvs::net::IpAddress jvs::net::map_to_ipv4(const IpAddress& ipAddress) noexcept
{
  if (ipAddress.is_ipv4())
  {
    return ipAddress;
  }

  std::array<std::uint8_t, Ipv4AddressSize> ipv4Bytes{};
  std::memcpy(
    &ipv4Bytes[0], ipAddress.address_bytes() + 12, jvs::net::Ipv4AddressSize);
  return IpAddress(ipv4Bytes);
}

jvs::net::IpAddress jvs::net::map_to_ipv6(const IpAddress& ipAddress) noexcept
{
  if (ipAddress.is_ipv6())
  {
    return ipAddress;
  }

  std::array<std::uint8_t, Ipv6AddressSize> ipv6Bytes{};
  ipv6Bytes[10] = 0xff;
  ipv6Bytes[11] = 0xff;
  std::memcpy(&ipv6Bytes[12], ipAddress.address_bytes(), Ipv4AddressSize);
  return IpAddress(ipv6Bytes);
}

std::string jvs::net::to_string(const IpAddress& addr) noexcept
{
  if (addr.family() == IpAddress::Family::IPv4)
  {
    return ipv4_to_string(addr);
  }
  else if (addr.family() == IpAddress::Family::IPv6)
  {
    return ipv6_to_string(addr);
  }
  else
  {
    // Unspecified
    return std::string{};
  }
}

std::optional<jvs::net::IpAddress> jvs::net::IpAddress::parse(
  std::string_view ipAddressString)
{
  IpAddress addr{};
  if (ipAddressString.find(':') != std::string_view::npos)
  {
    // Parse as IPv6
    
    // Copy the address to a string that can be manipulated
    std::string addrStr{ipAddressString};
    if (addrStr[0] != '[')
    {
      // Add an implicit terminator only if the address string doesn't start
      // with a bracket.
      addrStr += ']';
    }

    if (parse_ipv6(addrStr, &addr.address_bytes_[0], addr.scope_id_))
    {
      addr.family_ = Family::IPv6;
      return addr;
    }
  }
  else
  {
    // Parse as IPv4
    if (parse_ipv4(ipAddressString, &addr.address_bytes_[0]))
    {
      addr.family_ = Family::IPv4;
      return addr;
    }
  }

  return {};
}

std::optional<jvs::net::IpAddress> jvs::net::IpAddress::get(
  const std::initializer_list<std::uint8_t>& bytes)
{
  IpAddress addr{};

  if (bytes.size() == jvs::net::Ipv4AddressSize)
  {
    std::copy_n(
      bytes.begin(), jvs::net::Ipv4AddressSize, addr.address_bytes_.begin());
    addr.family_ = Family::IPv4;
  }
  else if (bytes.size() == jvs::net::Ipv6AddressSize)
  {
    std::copy_n(
      bytes.begin(), jvs::net::Ipv6AddressSize, addr.address_bytes_.begin());
    addr.family_ = Family::IPv6;
  }

  return {};
}

bool jvs::net::operator==(const IpAddress& a, const IpAddress& b) noexcept
{
  if (a.family() != b.family())
  {
    return false;
  }

  if (a.is_ipv6())
  {
    return (a.scope_id() == b.scope_id() &&
      std::equal(a.address_bytes(), a.address_bytes() + a.address_size(),
        b.address_bytes()));
  }
  else
  {
    return (std::equal(a.address_bytes(), a.address_bytes() + a.address_size(),
      b.address_bytes()));
  }
}

bool jvs::net::operator!=(const IpAddress& a, const IpAddress& b) noexcept
{
  return (!(a == b));
}

bool jvs::net::is_valid_ipv4_address(std::string_view ipAddressString,
  bool strict)
{
  int end{static_cast<int>(ipAddressString.length())};
  bool allowIpv6 = !strict;
  bool unknownScheme = !strict;
  return is_valid_ipv4(ipAddressString, 0, end, allowIpv6, false, 
    unknownScheme);
}

bool jvs::net::is_valid_ipv6_address(std::string_view ipAddressString, 
  bool strict)
{
  int end{static_cast<int>(ipAddressString.length())};
  return is_valid_ipv6(ipAddressString, 0, end, strict);
}

std::string jvs::ConvertCast<jvs::net::IpAddress, std::string>::operator()(
  const jvs::net::IpAddress& address) const
{
  return jvs::net::to_string(address);
}

auto jvs::ConvertCast<std::string, jvs::net::IpAddress>::operator()(
  const std::string& ipAddressString) const
  -> std::optional<jvs::net::IpAddress>
{
  return jvs::net::IpAddress::parse(ipAddressString);
}

std::size_t std::hash<jvs::net::IpAddress>::operator()(
  const jvs::net::IpAddress& addr) const noexcept
{
  std::size_t hashOffset;
  std::size_t hashPrime;
  if constexpr (sizeof(std::size_t) == 8)
  {
    hashOffset = 0xcbf29ce484222325;
    hashPrime = 0x00000100000001b3;
  }
  else
  {
    hashOffset = 0x811c9dc5;
    hashPrime = 0x01000193;
  }

  return std::accumulate(
    addr.address_bytes(), addr.address_bytes() + addr.address_size(),
    hashOffset, [hashPrime](std::size_t v, std::uint8_t a)
    {
      v ^= a;
      v *= hashPrime;
      return v;
    });
}
