///
/// @file utils.h
/// 
/// Internal utility functions
/// 

#if !defined(JVS_NETLIB_UTILS_H_)
#define JVS_NETLIB_UTILS_H_

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <tuple>

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
constexpr T create_zero_filled() noexcept
{
  static_assert(std::is_default_constructible_v<T>, 
    "Type must be default constructable");
  T result;
  std::memset(&result, 0, sizeof(result));
  return result;
}

template <typename PointerT, typename T>
auto pointer_cast(T* v)
{  
  using ResultType = std::add_pointer_t<std::remove_pointer_t<PointerT>>;
  return static_cast<ResultType>(static_cast<void*>(v));
}

template <typename PointerT, typename T>
auto pointer_cast(const T* v)
{  
  using ResultType = std::add_pointer_t<std::add_const_t<std::remove_pointer_t<PointerT>>>;
  return static_cast<ResultType>(static_cast<const void*>(v));
}

} // namespace jvs

#endif // !JVS_NETLIB_UTILS_H_
