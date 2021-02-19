#include "error.h"

#include <cstdlib>
#include <iostream>


char jvs::ErrorInfoBase::ID = 0;
char jvs::ErrorList::ID = 0;
char jvs::StringError::ID = 0;

[[noreturn]]
void jvs::do_unreachable(const char* msg, const char* fileName, 
  std::size_t lineNum)
{
  std::cerr << msg << "\n  at " << fileName << ':' << lineNum << '\n';
#define GCC_VERSION (__GNUC__ * 10000 \
                               + __GNUC_MINOR__ * 100 \
                               + __GNUC_PATCHLEVEL__)
#if GCC_VERSION >= 40500 || defined(__clang__)
  __builtin_unreachable();
#else
  abort();
#endif
}

void jvs::log_all_unhandled_errors(Error e, std::ostream& os, 
  std::string_view errorBanner) 
{
  if (!e)
    return;
  os << errorBanner;
  handle_all_errors(std::move(e), 
    [&](const ErrorInfoBase& ei) 
    {
      ei.log(os);
      os << "\n";
    });
}

[[noreturn]]
void jvs::Error::fatal_unchecked_error() const
{
  std::cerr << "Program aborted due to an unhandled Error:\n";
  if (get_ptr())
  {
    get_ptr()->log(std::cerr);
    std::cerr << "\n";
  }
  else
  {
    std::cerr << "Error value was Success. (Note: Success values must still be "
      "checked prior to being destroyed).\n";
  }

  abort();
}

jvs::StringError::StringError(std::string_view s)
  : msg_(s)
{
}

void jvs::StringError::log(std::ostream& os) const
{
  os << msg_;
}
