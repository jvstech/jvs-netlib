if (NOT JVS_NETLIB_ENABLE_TESTS)
  return()
endif()

include(FetchContent)

# Google Test
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.10.0)
FetchContent_GetProperties(googletest)
if (NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()

# Unit test project
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(testSources
  unittest_main.cpp
  ip_address_test.cpp
  ip_end_point_test.cpp
  network_integer_test.cpp
  socket_test.cpp
  )

add_executable(jvs-netlib-test ${testSources})
target_include_directories(jvs-netlib-test PRIVATE 
  ${googletest_SOURCE_DIR}/include
  ${NETLIB_INC_DIR})
target_link_libraries(jvs-netlib-test gtest_main)
if (JVS_NETLIB_BUILD_STATIC)
  target_link_libraries(jvs-netlib-test ${NETLIB_STATIC_NAME})
else()
  target_link_libraries(jvs-netlib-test ${NETLIB_SHARED_NAME})
endif()
set_target_properties(jvs-netlib-test
  PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
  )

enable_testing()
add_test(NAME jvs-netlib-test
  COMMAND jvs-netlib-test)
