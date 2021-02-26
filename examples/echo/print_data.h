#if !defined(JVS_NETLIB_EXAMPLES_ECHO_PRINT_DATA_H_)
#define JVS_NETLIB_EXAMPLES_ECHO_PRINT_DATA_H_

#include <cctype>
#include <ostream>
#include <string>
#include <type_traits>

namespace jvs
{

template <typename InputIterT>
static void print_data(
  InputIterT first, InputIterT last, std::ostream& os) noexcept
{
  using ValueType =
    std::remove_reference_t<decltype(*std::declval<InputIterT>())>;
  static_assert(std::is_integral_v<ValueType>);
  static_assert(sizeof(ValueType) == sizeof(char));
  
  auto toHex = [](char c)
  {
    static constexpr char HexTable[] = { '0', '1', '2', '3', '4', '5', '6', '7',
      '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    std::string s(2, 0);
    s[0] = HexTable[(c & 0xF0) >> 4];
    s[1] = HexTable[c & 0xF];
    return s;
  };

  for (auto it = first; it != last; ++it)
  {
    char c = static_cast<char>(*it);
    if (c == '\\')
    {
      os << "\\\\";
    }

    if (std::isprint(c))
    {
      os.write(&c, 1);
    }
    else
    {
      os << "\\x" << toHex(c);
    }
  }
}


} // namespace jvs



#endif // !JVS_NETLIB_EXAMPLES_ECHO_PRINT_DATA_H_
