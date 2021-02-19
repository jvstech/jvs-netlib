#if !defined(JVS_NETLIB_UTILS_H_)
#define JVS_NETLIB_UTILS_H_

#include <type_traits>
#include <string>

namespace jvs
{

template <typename T>
struct FunctionDecl
{
};

template <typename ReturnT, typename... ArgsT>
struct FunctionDecl<ReturnT(ArgsT...)>
{
  using return_type = ReturnT;
  using argument_types = std::tuple<ArgsT...>;
};

template <typename ReturnT, typename... ArgsT>
struct FunctionDecl<ReturnT(ArgsT...) throw()>
{
  using return_type = ReturnT;
  using argument_types = std::tuple<ArgsT...>;
};

template <typename T>
using ReturnType = typename FunctionDecl<T>::return_type;

template <typename T>
static constexpr T create_zero_filled() noexcept
{
  static_assert(std::is_default_constructible_v<T>, 
    "Type must be default constructable");
  T result;
  std::memset(&result, 0, sizeof(result));
  return result;
}


} // namespace jvs

#endif // !JVS_NETLIB_UTILS_H_
