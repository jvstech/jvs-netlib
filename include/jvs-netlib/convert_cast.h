///
/// @file convert_cast.h
/// 
/// Defines a callable object for defining and providing custom type
/// conversions.
/// 

#if !defined(JVS_CONVERT_CAST_H_)
#define JVS_CONVERT_CAST_H_

namespace jvs
{

///
/// @struct ConvertCast
/// 
/// Default implementation of a type conversion provider. This is meant to be
/// specialized by users to provide completely custom type conversion via the
/// convert_to function.
/// 
template <typename FromType, typename ToType>
struct ConvertCast
{
  ToType operator()(const FromType& value) const
  {
    return reinterpret_cast<ToType>(value);
  }
};

template <typename ToType, typename FromType>
inline static auto convert_to(const FromType& value)
{
  return ConvertCast<FromType, ToType>{}(value);
}

} // namespace jvs


#endif // !JVS_CONVERT_CAST_H_
