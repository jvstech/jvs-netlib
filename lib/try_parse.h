#if !defined(JVS_NETLIB_TRY_PARSE_H_)
#define JVS_NETLIB_TRY_PARSE_H_

#include <optional>
#include <sstream>
#include <string_view>
#include <type_traits>

namespace jvs
{

template <typename T>
static std::optional<T> try_parse(std::string_view value)
{
  static_assert(std::is_default_constructible_v<T>);
  T result;
  std::stringstream ss(value.data());
  if (ss >> result)
  {
    return result;
  }

  return {};
}

} // namespace jvs


#endif // !JVS_NETLIB_TRY_PARSE_H_
