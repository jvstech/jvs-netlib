///
/// @file error.h
///
/// This file is used primarily to implement a clone of LLVM's llvm::Expected
/// template.
///
/// This is a clone of LLVM's llvm/Support/Error.h with modifications to remove
/// unneeded code, match the JVS code style, and not much else. As such, the 
/// LLVM license header is shown here:
///
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
///

#if !defined(JVS_NETLIB_ERROR_H_)
#define JVS_NETLIB_ERROR_H_

#if !defined(ENABLE_FORCED_ERROR_CHECKING)
# if !defined(NDEBUG)
#   define ENABLE_FORCED_ERROR_CHECKING 1
# endif
#endif

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace jvs
{

[[noreturn]]
void do_unreachable(const char* msg, const char* sourceFile, 
  std::size_t lineNum);

#define jvs_unreachable(msg) ::jvs::do_unreachable(msg, __FILE__, __LINE__)

class ErrorSuccess;

/// Base class for error info classes. Do not extend this directly: Extend
/// the ErrorInfo template subclass instead.
class ErrorInfoBase
{
public:
  virtual ~ErrorInfoBase() = default;

  /// Print an error message to an output stream.
  virtual void log(std::ostream& os) const = 0;

  /// Return the error message as a string.
  virtual std::string message() const
  {
    std::ostringstream os{};
    log(os);
    return os.str();
  }

  // Returns the class ID for this type.
  static const void* class_id()
  {
    return &ID;
  }

  // Returns the class ID for the dynamic type of this ErrorInfoBase instance.
  virtual const void* dynamic_class_id() const = 0;

  // Check whether this instance is a subclass of the class identified by
  // classId.
  virtual bool is_a(const void* const classId) const
  {
    return classId == class_id();
  }

  // Check whether this instance is a subclass of ErrorInfoT.
  template <typename ErrorInfoT>
  bool is_a() const
  {
    return is_a(ErrorInfoT::class_id());
  }

  // Determine whether or not this error should be forced to be checked.
  virtual bool is_fatal() const
  {
    return true;
  }

private:
  
  static char ID;
};

/// Lightweight error class with error context and mandatory checking.
///
/// Instances of this class wrap a ErrorInfoBase pointer. Failure states
/// are represented by setting the pointer to a ErrorInfoBase subclass
/// instance containing information describing the failure. Success is
/// represented by a null pointer value.
///
/// Instances of Error also contains a 'Checked' flag, which must be set
/// before the destructor is called, otherwise the destructor will trigger a
/// runtime error. This enforces at runtime the requirement that all Error
/// instances be checked or returned to the caller.
///
/// There are two ways to set the checked flag, depending on what state the
/// Error instance is in. For Error instances indicating success, it
/// is sufficient to invoke the boolean conversion operator. e.g.:
///
///   @code{.cpp}
///   Error foo(<...>);
///
///   if (auto e = foo(<...>))
///     return e; // <- Return e if it is in the error state.
///   // We have verified that e was in the success state. It can now be safely
///   // destroyed.
///   @endcode
///
/// A success value *can not* be dropped. For example, just calling 'foo(<...>)'
/// without testing the return value will raise a runtime error, even if foo
/// returns success.
///
/// For Error instances representing failure, you must use either the
/// handle_errors or handle_all_errors function with a typed handler. e.g.:
///
///   @code{.cpp}
///   class MyErrorInfo : public ErrorInfo<MyErrorInfo> {
///     // Custom error info.
///   };
///
///   Error foo(<...>) { return make_error<MyErrorInfo>(...); }
///
///   auto e = foo(<...>); // <- foo returns failure with MyErrorInfo.
///   auto NewE =
///     handle_errors(e,
///       [](const MyErrorInfo &M) {
///         // Deal with the error.
///       },
///       [](std::unique_ptr<OtherError> M) -> Error {
///         if (canHandle(*M)) {
///           // handle error.
///           return Error::success();
///         }
///         // Couldn't handle this error instance. Pass it up the stack.
///         return Error(std::move(M));
///       );
///   // Note - we must check or return NewE in case any of the handlers
///   // returned a new error.
///   @endcode
///
/// The handle_all_errors function is identical to handle_errors, except
/// that it has a void return type, and requires all errors to be handled and
/// no new errors be returned. It prevents errors (assuming they can all be
/// handled) from having to be bubbled all the way to the top-level.
///
/// *All* Error instances must be checked before destruction, even if
/// they're moved-assigned or constructed from Success values that have already
/// been checked. This enforces checking through all levels of the call stack.
class [[nodiscard]] Error
{
  // ErrorList needs to be able to yank ErrorInfoBase pointers out of Errors
  // to add to the error list. It can't rely on handle_errors for this, since
  // handle_errors does not support ErrorList handlers.
  friend class ErrorList;

  // handle_errors needs to be able to set the Checked flag.
  template <typename... HandlerTs>
  friend Error handle_errors(Error E, HandlerTs&&... Handlers);

  // Expected<T> needs to be able to steal the payload when constructed from an
  // error.
  template <typename T>
  friend class Expected;

protected:
  /// Create a success value. Prefer using 'Error::success()' for readability
  Error()
  {
    set_ptr(nullptr);
    set_checked(false);
  }

public:
  /// Create a success value.
  static ErrorSuccess success();

  // Errors are not copy-constructable.
  Error(const Error& other) = delete;

  /// Move-construct an error value. The newly constructed error is considered
  /// unchecked, even if the source error had been checked. The original error
  /// becomes a checked Success value, regardless of its original state.
  Error(Error&& other)
  {
    set_checked(true);
    *this = std::move(other);
  }

  /// Create an error value. Prefer using the 'make_error' function, but
  /// this constructor can be useful when "re-throwing" errors from handlers.
  Error(std::unique_ptr<ErrorInfoBase> Payload)
  {
    set_ptr(Payload.release());
    set_checked(false);
  }

  // Errors are not copy-assignable.
  Error& operator=(const Error& other) = delete;

  /// Move-assign an error value. The current error must represent success, you
  /// you cannot overwrite an unhandled error. The current error is then
  /// considered unchecked. The source error becomes a checked success value,
  /// regardless of its original state.
  Error& operator=(Error&& other)
  {
    // Don't allow overwriting of unchecked values.
    assert_is_checked();
    set_ptr(other.get_ptr());

    // This Error is unchecked, even if the source error was checked.
    set_checked(false);

    // Null out other's payload and set its checked bit.
    other.set_ptr(nullptr);
    other.set_checked(true);

    return *this;
  }

  /// Destroy a Error. Fails with a call to abort() if the error is
  /// unchecked.
  ~Error()
  {
    assert_is_checked();
    delete get_ptr();
  }

  /// Bool conversion. Returns true if this Error is in a failure state,
  /// and false if it is in an accept state. If the error is in a Success state
  /// it will be considered checked.
  explicit operator bool()
  {
    set_checked(get_ptr() == nullptr);
    return get_ptr() != nullptr;
  }

  /// Check whether one error is a subclass of another.
  template <typename ErrT>
  bool is_a() const
  {
    return get_ptr() && get_ptr()->is_a(ErrT::class_id());
  }

  /// Returns the dynamic class id of this error, or null if this is a success
  /// value.
  const void* dynamic_class_id() const
  {
    if (!get_ptr())
    {
      return nullptr;
    }

    return get_ptr()->dynamic_class_id();
  }

private:
  // assert_is_checked() happens very frequently, but under normal circumstances
  // is supposed to be a no-op.  So we want it to be inlined, but having a bunch
  // of debug prints can cause the function to be too large for inlining.  So
  // it's important that we define this function out of line so that it can't be
  // inlined.
  [[noreturn]]
  void fatal_unchecked_error() const;

  void assert_is_checked()
  {
    if ((!get_checked() || (get_ptr() && get_ptr()->is_fatal())))
    {
      fatal_unchecked_error();
    }
  }

  ErrorInfoBase* get_ptr() const
  {
    return reinterpret_cast<ErrorInfoBase*>(
      reinterpret_cast<uintptr_t>(Payload) & ~static_cast<uintptr_t>(0x1));
  }

  void set_ptr(ErrorInfoBase* EI)
  {
    Payload = reinterpret_cast<ErrorInfoBase*>(
      (reinterpret_cast<uintptr_t>(EI) & ~static_cast<uintptr_t>(0x1)) |
      (reinterpret_cast<uintptr_t>(Payload) & 0x1));
  }

  bool get_checked() const
  {
    return (reinterpret_cast<uintptr_t>(Payload) & 0x1) == 0;
  }

  void set_checked(bool V)
  {
    Payload = reinterpret_cast<ErrorInfoBase*>(
      (reinterpret_cast<uintptr_t>(Payload) & ~static_cast<uintptr_t>(0x1)) |
      (V ? 0 : 1));
  }

  std::unique_ptr<ErrorInfoBase> take_payload()
  {
    std::unique_ptr<ErrorInfoBase> Tmp(get_ptr());
    set_ptr(nullptr);
    set_checked(true);
    return Tmp;
  }

  friend std::ostream& operator<<(std::ostream& os, const Error& E)
  {
    if (auto P = E.get_ptr())
    {
      P->log(os);
    }
    else
    {
      os << "success";
    }

    return os;
  }

  ErrorInfoBase* Payload = nullptr;
};

/// Subclass of Error for the sole purpose of identifying the success path in
/// the type system. This allows to catch invalid conversion to Expected<T> at
/// compile time.
class ErrorSuccess final : public Error
{
};

inline ErrorSuccess Error::success()
{
  return ErrorSuccess();
}

/// Make a Error instance representing failure using the given error info
/// type.
template <typename ErrT, typename... ArgTs>
Error make_error(ArgTs&&... Args)
{
  return Error(std::make_unique<ErrT>(std::forward<ArgTs>(Args)...));
}

/// Base class for user error types. Users should declare their error types
/// like:
///
/// class MyError : public ErrorInfo<MyError> {
///   ....
/// };
///
/// This class provides an implementation of the ErrorInfoBase::kind
/// method, which is used by the Error RTTI system.
template <typename ThisErrT, typename ParentErrT = ErrorInfoBase>
class ErrorInfo : public ParentErrT
{
protected:
  using Base = ErrorInfo<ThisErrT, ParentErrT>;

public:
  using ParentErrT::ParentErrT;  // inherit constructors

  static const void* class_id()
  {
    return &ThisErrT::ID;
  }

  const void* dynamic_class_id() const override
  {
    return &ThisErrT::ID;
  }

  bool is_a(const void* const ClassID) const override
  {
    return ClassID == class_id() || ParentErrT::is_a(ClassID);
  }
};

/// Special ErrorInfo subclass representing a list of ErrorInfos.
/// Instances of this class are constructed by joinError.
class ErrorList final : public ErrorInfo<ErrorList>
{
  // handle_errors needs to be able to iterate the payload list of an
  // ErrorList.
  template <typename... HandlerTs>
  friend Error handle_errors(Error E, HandlerTs&&... Handlers);

  // join_errors is implemented in terms of join.
  friend Error join_errors(Error, Error);

public:
  void log(std::ostream& os) const override
  {
    os << "Multiple errors:\n";
    for (auto& errPayload : payloads_)
    {
      errPayload->log(os);
      os << "\n";
    }
  }

  // Used by ErrorInfo::class_id.
  static char ID;

private:
  ErrorList(std::unique_ptr<ErrorInfoBase> Payload1,
    std::unique_ptr<ErrorInfoBase> Payload2)
  {
    assert(!Payload1->is_a<ErrorList>() && !Payload2->is_a<ErrorList>() &&
      "ErrorList constructor payloads should be singleton errors");
    payloads_.push_back(std::move(Payload1));
    payloads_.push_back(std::move(Payload2));
  }

  static Error join(Error e1, Error e2)
  {
    if (!e1)
    {
      return e2;
    }

    if (!e2)
    {
      return e1;
    }

    if (e1.is_a<ErrorList>())
    {
      auto& e1List = static_cast<ErrorList&>(*e1.get_ptr());
      if (e2.is_a<ErrorList>())
      {
        auto e2Payload = e2.take_payload();
        auto& e2List = static_cast<ErrorList&>(*e2Payload);
        for (auto& payload : e2List.payloads_)
        {
          e1List.payloads_.push_back(std::move(payload));
        }
      }
      else
      {
        e1List.payloads_.push_back(e2.take_payload());
      }

      return e1;
    }

    if (e2.is_a<ErrorList>())
    {
      auto& e2List = static_cast<ErrorList&>(*e2.get_ptr());
      e2List.payloads_.insert(e2List.payloads_.begin(), e1.take_payload());
      return e2;
    }

    return Error(std::unique_ptr<ErrorList>(
      new ErrorList(e1.take_payload(), e2.take_payload())));
  }

  std::vector<std::unique_ptr<ErrorInfoBase>> payloads_;
};

/// Concatenate errors. The resulting Error is unchecked, and contains the
/// ErrorInfo(s), if any, contained in e1, followed by the
/// ErrorInfo(s), if any, contained in e2.
inline Error join_errors(Error E1, Error E2)
{
  return ErrorList::join(std::move(E1), std::move(E2));
}

/// Replacement for llvm::AlignedCharArrayUnion
template <typename T, typename... Ts>
struct AlignedCharArrayUnion
{
  static constexpr std::size_t alignment_value =
    std::max({ alignof(T), alignof(Ts)... });
  alignas(alignment_value) char buffer[std::max({sizeof(T), sizeof(Ts)...})];
};

/// Tagged union holding either a T or a Error.
///
/// This class parallels ErrorOr, but replaces error_code with Error. Since
/// Error cannot be copied, this class replaces getError() with
/// take_error(). It also adds an bool error_is_a<ErrT>() method for testing the
/// error class type.
template <class T>
class [[nodiscard]] Expected
{
  template <class T1>
  friend class ExpectedAsOutParameter;

  template <class OtherT>
  friend class Expected;

  static constexpr bool isRef = std::is_reference<T>::value;

  using wrap = std::reference_wrapper<std::remove_reference_t<T>>;

  using error_type = std::unique_ptr<ErrorInfoBase>;

public:
  using storage_type = std::conditional_t<isRef, wrap, T>;
  using value_type = T;

private:
  using reference = std::remove_reference_t<T>&;
  using const_reference = const std::remove_reference_t<T>&;
  using pointer = std::remove_reference_t<T>*;
  using const_pointer = const std::remove_reference_t<T>*;

public:
  /// Create an Expected<T> error value from the given Error.
  Expected(Error err)
    : has_error_(true),
      // Expected is unchecked upon construction in Debug builds.
      unchecked_(true)
  {
    assert(err && "Cannot create Expected<T> from Error success value.");
    new (error_storage()) error_type(err.take_payload());
  }

  /// Forbid to convert from Error::success() implicitly, this avoids having
  /// Expected<T> foo() { return Error::success(); } which compiles otherwise
  /// but triggers the assertion above.
  Expected(ErrorSuccess) = delete;

  /// Create an Expected<T> success value from the given OtherT value, which
  /// must be convertible to T.
  template <typename OtherT>
  Expected(OtherT&& Val,
    std::enable_if_t<std::is_convertible_v<OtherT, T>>* = nullptr)
    : has_error_(false),
      // Expected is unchecked upon construction in Debug builds.
      unchecked_(true)
  {
    new (storage()) storage_type(std::forward<OtherT>(Val));
  }

  /// Move construct an Expected<T> value.
  Expected(Expected&& other)
  {
    move_construct(std::move(other));
  }

  /// Move construct an Expected<T> value from an Expected<OtherT>, where OtherT
  /// must be convertible to T.
  template <class OtherT>
  Expected(Expected<OtherT>&& other,
    std::enable_if_t<std::is_convertible<OtherT, T>::value>* = nullptr)
  {
    move_construct(std::move(other));
  }

  /// Move construct an Expected<T> value from an Expected<OtherT>, where OtherT
  /// isn't convertible to T.
  template <class OtherT>
  explicit Expected(Expected<OtherT>&& other,
    std::enable_if_t<!std::is_convertible<OtherT, T>::value>* = nullptr)
  {
    move_construct(std::move(other));
  }

  /// Move-assign from another Expected<T>.
  Expected& operator=(Expected&& other)
  {
    move_assign(std::move(other));
    return *this;
  }

  /// Destroy an Expected<T>.
  ~Expected()
  {
    assert_is_checked();
    if (!has_error_)
    {
      storage()->~storage_type();
    }
    else
    {
      error_storage()->~error_type();
    }
  }

  /// Return false if there is an error.
  explicit operator bool()
  {
    // I'm making Expected<T> a bit more permissive by marking it as checked
    // on a simple bool cast.
    //
    // Original code:
    //   unchecked_ = has_error_;
    //   return !has_error_;
    unchecked_ = false;
    return !has_error_;
  }

  /// Returns a reference to the stored T value.
  reference get()
  {
    assert_is_checked();
    return *storage();
  }

  /// Returns a const reference to the stored T value.
  const_reference get() const
  {
    assert_is_checked();
    return const_cast<Expected<T>*>(this)->get();
  }

  /// Check that this Expected<T> is an error of type ErrT.
  template <typename ErrT>
  bool error_is_a() const
  {
    return has_error_ && (*error_storage())->template is_a<ErrT>();
  }

  /// Take ownership of the stored error.
  /// After calling this the Expected<T> is in an indeterminate state that can
  /// only be safely destructed. No further calls (beside the destructor) should
  /// be made on the Expected<T> value.
  Error take_error()
  {
    unchecked_ = false;
    return has_error_ ? Error(std::move(*error_storage())) : Error::success();
  }

  /// Returns a pointer to the stored T value.
  pointer operator->()
  {
    assert_is_checked();
    return to_pointer(storage());
  }

  /// Returns a const pointer to the stored T value.
  const_pointer operator->() const
  {
    assert_is_checked();
    return to_pointer(storage());
  }

  /// Returns a reference to the stored T value.
  reference operator*()
  {
    assert_is_checked();
    return *storage();
  }

  /// Returns a const reference to the stored T value.
  const_reference operator*() const
  {
    assert_is_checked();
    return *storage();
  }

private:
  template <class T1>
  static bool compare_this_if_same_type(const T1& a, const T1& b)
  {
    return &a == &b;
  }

  template <class T1, class T2>
  static bool compare_this_if_same_type(const T1& a, const T2& b)
  {
    return false;
  }

  template <class OtherT>
  void move_construct(Expected<OtherT>&& other)
  {
    has_error_ = other.has_error_;
    unchecked_ = true;
    other.unchecked_ = false;

    if (!has_error_)
    {
      new (storage()) storage_type(std::move(*other.storage()));
    }
    else
    {
      new (error_storage()) error_type(std::move(*other.error_storage()));
    }
  }

  template <class OtherT>
  void move_assign(Expected<OtherT>&& other)
  {
    assert_is_checked();

    if (compare_this_if_same_type(*this, other))
    {
      return;
    }

    this->~Expected();
    new (this) Expected(std::move(other));
  }

  pointer to_pointer(pointer Val)
  {
    return Val;
  }

  const_pointer to_pointer(const_pointer Val) const
  {
    return Val;
  }

  pointer to_pointer(wrap* Val)
  {
    return &Val->get();
  }

  const_pointer to_pointer(const wrap* Val) const
  {
    return &Val->get();
  }

  storage_type* storage()
  {
    assert(!has_error_ && "Cannot get value when an error exists!");
    return reinterpret_cast<storage_type*>(value_storage_.buffer);
  }

  const storage_type* storage() const
  {
    assert(!has_error_ && "Cannot get value when an error exists!");
    return reinterpret_cast<const storage_type*>(value_storage_.buffer);
  }

  error_type* error_storage()
  {
    assert(has_error_ && "Cannot get error when a value exists!");
    return reinterpret_cast<error_type*>(error_storage_.buffer);
  }

  const error_type* error_storage() const
  {
    assert(has_error_ && "Cannot get error when a value exists!");
    return reinterpret_cast<const error_type*>(error_storage_.buffer);
  }

  // Used by ExpectedAsOutParameter to reset the checked flag.
  void set_unchecked()
  {
    unchecked_ = true;
  }

#if (defined(ENABLE_FORCED_ERROR_CHECKING) && (ENABLE_FORCED_ERROR_CHECKING))
  // #FIXME: make this compiler check more robust to support the noinline 
  // attribute.
#if (_MSC_VER)
  __declspec(noinline)
#else
  __attribute__((noinline))
#endif
  void fatal_unchecked_expected() const
  {
    if (has_error_)
    {
      if (!(*error_storage())->is_fatal())
      {
        return;
      }
    }

#if !defined(NDEBUG)
    std::cerr << "Expected<T> must be checked before access or destruction.\n";
    if (has_error_)
    {
      std::cerr << "Unchecked Expected<T> contained error:\n";
      (*error_storage())->log(std::cerr);
    }
    else
    {
      std::cerr << "Expected<T> value was in success state. (Note: Expected<T> "
        "values in success mode must still be checked prior to being "
        "destroyed).\n";
    }
#endif
    abort();
  }
#else
  void fatal_unchecked_expected() const
  {
    // Do nothing.
  }
#endif

  void assert_is_checked()
  {
    if (unchecked_)
    {
      fatal_unchecked_expected();
    }
  }

  union
  {
    AlignedCharArrayUnion<storage_type> value_storage_;
    AlignedCharArrayUnion<error_type> error_storage_;
  };

  bool has_error_ : 1;
  bool unchecked_ : 1;
};

/// Report a fatal error if err is a failure value.
///
/// This function can be used to wrap calls to fallible functions ONLY when it
/// is known that the Error will always be a success value. e.g.
///
///   @code{.cpp}
///   // foo only attempts the fallible operation if DoFallibleOperation is
///   // true. If DoFallibleOperation is false then foo always returns
///   // Error::success().
///   Error foo(bool DoFallibleOperation);
///
///   cant_fail(foo(false));
///   @endcode
inline void cant_fail(Error err, const char* msg = nullptr)
{
  if (err)
  {
    if (!msg)
    {
      msg = "Failure value returned from cant_fail wrapped call";
    }

#if !defined(NDEBUG)
    std::string Str;
    std::ostringstream os(Str);
    os << msg << "\n" << err;
    msg = os.str().c_str();
#endif
    jvs_unreachable(msg);
  }
}

/// Report a fatal error if valOrErr is a failure value, otherwise unwraps and
/// returns the contained value.
///
/// This function can be used to wrap calls to fallible functions ONLY when it
/// is known that the Error will always be a success value. e.g.
///
///   @code{.cpp}
///   // foo only attempts the fallible operation if DoFallibleOperation is
///   // true. If DoFallibleOperation is false then foo always returns an int.
///   Expected<int> foo(bool DoFallibleOperation);
///
///   int X = cant_fail(foo(false));
///   @endcode
template <typename T>
T cant_fail(Expected<T> ValOrErr, const char* msg = nullptr)
{
  if (ValOrErr)
  {
    return std::move(*ValOrErr);
  }
  else
  {
    if (!msg)
    {
      msg = "Failure value returned from cant_fail wrapped call";
    }

#if !defined(NDEBUG)
    std::string str;
    std::ostringstream os(str);
    auto e = ValOrErr.take_error();
    os << msg << "\n" << e;
    msg = os.str().c_str();
#endif
    jvs_unreachable(msg);
  }
}

/// Report a fatal error if valOrErr is a failure value, otherwise unwraps and
/// returns the contained reference.
///
/// This function can be used to wrap calls to fallible functions ONLY when it
/// is known that the Error will always be a success value. e.g.
///
///   @code{.cpp}
///   // foo only attempts the fallible operation if DoFallibleOperation is
///   // true. If DoFallibleOperation is false then foo always returns a Bar&.
///   Expected<Bar&> foo(bool DoFallibleOperation);
///
///   Bar &X = cant_fail(foo(false));
///   @endcode
template <typename T>
T& cant_fail(Expected<T&> valOrErr, const char* msg = nullptr)
{
  if (valOrErr)
  {
    return *valOrErr;
  }
  else
  {
    if (!msg)
    {
      msg = "Failure value returned from cant_fail wrapped call";
    }

#if !defined(NDEBUG)
    std::ostringstream os{};
    auto e = valOrErr.take_error();
    os << msg << "\n" << e;
    msg = os.str().c_str();
#endif
    jvs_unreachable(msg);
  }
}

/// Helper for testing applicability of, and applying, handlers for
/// ErrorInfo types.
template <typename HandlerT>
class ErrorHandlerTraits
  : public ErrorHandlerTraits<decltype(
      &std::remove_reference<HandlerT>::type::operator())>
{
};

// Specialization functions of the form 'Error (const ErrT&)'.
template <typename ErrT>
class ErrorHandlerTraits<Error (&)(ErrT&)>
{
public:
  static bool applies_to(const ErrorInfoBase& E)
  {
    return E.template is_a<ErrT>();
  }

  template <typename HandlerT>
  static Error apply(HandlerT&& H, std::unique_ptr<ErrorInfoBase> E)
  {
    assert(applies_to(*E) && "Applying incorrect handler");
    return H(static_cast<ErrT&>(*E));
  }
};

// Specialization functions of the form 'void (const ErrT&)'.
template <typename ErrT>
class ErrorHandlerTraits<void (&)(ErrT&)>
{
public:
  static bool applies_to(const ErrorInfoBase& E)
  {
    return E.template is_a<ErrT>();
  }

  template <typename HandlerT>
  static Error apply(HandlerT&& H, std::unique_ptr<ErrorInfoBase> E)
  {
    assert(applies_to(*E) && "Applying incorrect handler");
    H(static_cast<ErrT&>(*E));
    return Error::success();
  }
};

/// Specialization for functions of the form 'Error (std::unique_ptr<ErrT>)'.
template <typename ErrT>
class ErrorHandlerTraits<Error(&)(std::unique_ptr<ErrT>)>
{
public:
  static bool applies_to(const ErrorInfoBase& E)
  {
    return E.template is_a<ErrT>();
  }

  template <typename HandlerT>
  static Error apply(HandlerT&& H, std::unique_ptr<ErrorInfoBase> E)
  {
    assert(applies_to(*E) && "Applying incorrect handler");
    std::unique_ptr<ErrT> SubE(static_cast<ErrT*>(E.release()));
    return H(std::move(SubE));
  }
};

/// Specialization for functions of the form 'void (std::unique_ptr<ErrT>)'.
template <typename ErrT>
class ErrorHandlerTraits<void (&)(std::unique_ptr<ErrT>)>
{
public:
  static bool applies_to(const ErrorInfoBase& E)
  {
    return E.template is_a<ErrT>();
  }

  template <typename HandlerT>
  static Error apply(HandlerT&& H, std::unique_ptr<ErrorInfoBase> E)
  {
    assert(applies_to(*E) && "Applying incorrect handler");
    std::unique_ptr<ErrT> SubE(static_cast<ErrT*>(E.release()));
    H(std::move(SubE));
    return Error::success();
  }
};

// Specialization for member functions of the form 'RetT (const ErrT&)'.
template <typename C, typename RetT, typename ErrT>
class ErrorHandlerTraits<RetT (C::*)(ErrT&)>
  : public ErrorHandlerTraits<RetT (&)(ErrT&)>
{
};

// Specialization for member functions of the form 'RetT (const ErrT&) const'.
template <typename C, typename RetT, typename ErrT>
class ErrorHandlerTraits<RetT (C::*)(ErrT&) const>
  : public ErrorHandlerTraits<RetT (&)(ErrT&)>
{
};

// Specialization for member functions of the form 'RetT (const ErrT&)'.
template <typename C, typename RetT, typename ErrT>
class ErrorHandlerTraits<RetT (C::*)(const ErrT&)>
  : public ErrorHandlerTraits<RetT (&)(ErrT&)>
{
};

// Specialization for member functions of the form 'RetT (const ErrT&) const'.
template <typename C, typename RetT, typename ErrT>
class ErrorHandlerTraits<RetT (C::*)(const ErrT&) const>
  : public ErrorHandlerTraits<RetT (&)(ErrT&)>
{
};

/// Specialization for member functions of the form
/// 'RetT (std::unique_ptr<ErrT>)'.
template <typename C, typename RetT, typename ErrT>
class ErrorHandlerTraits<RetT (C::*)(std::unique_ptr<ErrT>)>
  : public ErrorHandlerTraits<RetT (&)(std::unique_ptr<ErrT>)>
{
};

/// Specialization for member functions of the form
/// 'RetT (std::unique_ptr<ErrT>) const'.
template <typename C, typename RetT, typename ErrT>
class ErrorHandlerTraits<RetT (C::*)(std::unique_ptr<ErrT>) const>
  : public ErrorHandlerTraits<RetT (&)(std::unique_ptr<ErrT>)>
{
};

inline Error handle_error_impl(std::unique_ptr<ErrorInfoBase> payload)
{
  return Error(std::move(payload));
}

template <typename HandlerT, typename... HandlerTs>
Error handle_error_impl(std::unique_ptr<ErrorInfoBase> payload,
  HandlerT&& handler, HandlerTs&&... handlers)
{
  if (ErrorHandlerTraits<HandlerT>::applies_to(*payload))
  {
    return ErrorHandlerTraits<HandlerT>::apply(
      std::forward<HandlerT>(handler), std::move(payload));
  }

  return handle_error_impl(
    std::move(payload), std::forward<HandlerTs>(handlers)...);
}

/// Pass the ErrorInfo(s) contained in e to their respective handlers. Any
/// unhandled errors (or Errors returned by handlers) are re-concatenated and
/// returned.
/// Because this function returns an error, its result must also be checked
/// or returned. If you intend to handle all errors use handle_all_errors
/// (which returns void, and will abort() on unhandled errors) instead.
template <typename... HandlerTs>
Error handle_errors(Error e, HandlerTs&&... hs)
{
  if (!e)
  {
    return Error::success();
  }

  std::unique_ptr<ErrorInfoBase> payload = e.take_payload();

  if (payload->is_a<ErrorList>())
  {
    ErrorList& errList = static_cast<ErrorList&>(*payload);
    Error r;
    for (auto& p : errList.payloads_)
    {
      r = ErrorList::join(std::move(r),
        handle_error_impl(std::move(p), std::forward<HandlerTs>(hs)...));
    }

    return r;
  }

  return handle_error_impl(std::move(payload), std::forward<HandlerTs>(hs)...);
}

/// Behaves the same as handle_errors, except that by contract all errors
/// *must* be handled by the given handlers (i.e. there must be no remaining
/// errors after running the handlers, or jvs_unreachable is called).
template <typename... HandlerTs>
void handle_all_errors(Error E, HandlerTs&&... Handlers)
{
  cant_fail(handle_errors(std::move(E), std::forward<HandlerTs>(Handlers)...));
}

/// Check that e is a non-error, then drop it.
/// If e is an error, jvs_unreachable will be called.
inline void handle_all_errors(Error E)
{
  cant_fail(std::move(E));
}

/// Handle any errors (if present) in an Expected<T>, then try a recovery path.
///
/// If the incoming value is a success value it is returned unmodified. If it
/// is a failure value then it the contained error is passed to handle_errors.
/// If handle_errors is able to handle the error then the recoveryPath functor
/// is called to supply the final result. If handle_errors is not able to
/// handle all errors then the unhandled errors are returned.
///
/// This utility enables the follow pattern:
///
///   @code{.cpp}
///   enum FooStrategy { Aggressive, Conservative };
///   Expected<Foo> foo(FooStrategy S);
///
///   auto ResultOrErr =
///     handle_expected(
///       foo(Aggressive),
///       []() { return foo(Conservative); },
///       [](AggressiveStrategyError&) {
///         // Implicitly conusme this - we'll recover by using a conservative
///         // strategy.
///       });
///
///   @endcode
template <typename T, typename RecoveryFtor, typename... HandlerTs>
Expected<T> handle_expected(
  Expected<T> ValOrErr, RecoveryFtor&& recoveryPath, HandlerTs&&... handlers)
{
  if (ValOrErr)
  {
    return ValOrErr;
  }

  if (auto err_ = handle_errors(
    ValOrErr.take_error(), std::forward<HandlerTs>(handlers)...))
  {
    return std::move(err_);
  }

  return recoveryPath();
}

/// Log all errors (if any) in e to os. If there are any errors, ErrorBanner
/// will be printed before the first one is logged. A newline will be printed
/// after each error.
///
/// This function is compatible with the helpers from Support/WithColor.h. You
/// can pass any of them as the os. Please consider using them instead of
/// including 'error: ' in the ErrorBanner.
///
/// This is useful in the base level of your program to allow clean termination
/// (allowing clean deallocation of resources, etc.), while reporting error
/// information to the user.
void log_all_unhandled_errors(Error e, std::ostream& os, 
  std::string_view errorBanner = {});

/// Write all error messages (if any) in e to a string. The newline character
/// is used to separate error messages.
inline std::string toString(Error E)
{
  std::vector<std::string> errors;
  errors.reserve(2);
  handle_all_errors(std::move(E),
    [&errors](const ErrorInfoBase& EI) { errors.push_back(EI.message()); });
  std::string result{};
  if (errors.empty())
  {
    return result;
  }

  std::size_t len = (std::distance(errors.begin(), errors.end()) - 1);
  auto begIt = errors.begin();
  auto endIt = errors.end();
  for (auto it = begIt; it != endIt; ++it)
  {
    len += it->size();
  }

  auto it = begIt;
  while (++it != endIt)
  {
    result += "\n";
    result += *it;
  }

  return result;
}

/// Consume a Error without doing anything. This method should be used
/// only where an error can be considered a reasonable and expected return
/// value.
///
/// Uses of this method are potentially indicative of design problems: If it's
/// legitimate to do nothing while processing an "error", the error-producer
/// might be more clearly refactored to return an Optional<T>.
inline void consume_error(Error err)
{
  handle_all_errors(std::move(err), [](const ErrorInfoBase&) {});
}

/// Convert an Expected to an Optional without doing anything. This method
/// should be used only where an error can be considered a reasonable and
/// expected return value.
///
/// Uses of this method are potentially indicative of problems: perhaps the
/// error should be propagated further, or the error-producer should just
/// return an Optional in the first place.
template <typename T>
std::optional<T> expected_to_optional(Expected<T>&& e)
{
  if (e)
  {
    return std::move(*e);
  }

  consume_error(e.take_error());
  return {};
}

/// Convert an Expected to an Optional without doing anything. This method
/// should be used only where an error can be considered a reasonable and
/// expected return value.
///
/// Uses of this method are potentially indicative of problems: perhaps the
/// error should be propagated further, or the error-producer should just
/// return an Optional in the first place.
template <typename T>
std::optional<T> unwrap(Expected<T>&& e)
{
  return expected_to_optional(std::move(e));
}

/// Helper for converting an Error to a bool.
///
/// This method returns true if err is in an error state, or false if it is
/// in a success state.  Puts err in a checked state in both cases (unlike
/// Error::operator bool(), which only does this for success states).
inline bool error_to_bool(Error err)
{
  bool isError = static_cast<bool>(err);
  if (isError)
  {
    consume_error(std::move(err));
  }

  return isError;
}

/// Helper for Errors used as out-parameters.
///
/// This helper is for use with the Error-as-out-parameter idiom, where an error
/// is passed to a function or method by reference, rather than being returned.
/// In such cases it is helpful to set the checked bit on entry to the function
/// so that the error can be written to (unchecked Errors abort on assignment)
/// and clear the checked bit on exit so that clients cannot accidentally forget
/// to check the result. This helper performs these actions automatically using
/// RAII:
///
///   @code{.cpp}
///   Result foo(Error &err) {
///     ErrorAsOutParameter ErrAsOutParam(&err); // 'Checked' flag set
///     // <body of foo>
///     // <- 'Checked' flag auto-cleared when ErrAsOutParam is destructed.
///   }
///   @endcode
///
/// ErrorAsOutParameter takes an Error* rather than Error& so that it can be
/// used with optional Errors (Error pointers that are allowed to be null). If
/// ErrorAsOutParameter took an Error reference, an instance would have to be
/// created inside every condition that verified that Error was non-null. By
/// taking an Error pointer we can just create one instance at the top of the
/// function.
class ErrorAsOutParameter
{
public:
  ErrorAsOutParameter(Error* err) 
    : err_(err)
  {
    // Raise the checked bit if err is success.
    if (err_)
      (void)!!*err_;
  }

  ~ErrorAsOutParameter()
  {
    // Clear the checked bit.
    if (err_ && !*err_)
      *err_ = Error::success();
  }

private:
  Error* err_;
};

/// Helper for Expected<T>s used as out-parameters.
///
/// See ErrorAsOutParameter.
template <typename T>
class ExpectedAsOutParameter
{
public:
  ExpectedAsOutParameter(Expected<T>* valOrErr) 
    : val_or_err_(valOrErr)
  {
    if (val_or_err_)
      (void)!!*val_or_err_;
  }

  ~ExpectedAsOutParameter()
  {
    if (val_or_err_)
    {
      val_or_err_->set_unchecked();
    }
  }

private:
  Expected<T>* val_or_err_;
};

/// This class wraps a string in an Error.
///
/// StringError is useful in cases where the client is not expected to be able
/// to consume the specific error message programmatically (for example, if the
/// error message is to be presented to the user).
///
/// StringError can also be used when additional information is to be printed
/// along with a error_code message. Depending on the constructor called, this
/// class can either display:
///    1. the error_code message (ECError behavior)
///    2. a string
///    3. the error_code message and a string
///
/// These behaviors are useful when subtyping is required; for example, when a
/// specific library needs an explicit error type. In the example below,
/// PDBError is derived from StringError:
///
///   @code{.cpp}
///   Expected<int> foo() {
///      return llvm::make_error<PDBError>(pdb_error_code::dia_failed_loading,
///                                        "Additional information");
///   }
///   @endcode
///
class StringError : public ErrorInfo<StringError>
{
public:
  static char ID;

  // Prints s
  StringError(std::string_view s);

  void log(std::ostream& os) const override;
  
private:
  std::string msg_;
};

/// Create formatted StringError object.
template <typename... Ts>
inline Error create_string_error(Ts&&... vals)
{
  std::stringstream os{};
  ((os << std::forward<Ts>(vals)), ...);
  return make_error<StringError>(os.str());
}

}  // end namespace jvs

#endif  // !JVS_NETLIB_ERROR_H_
