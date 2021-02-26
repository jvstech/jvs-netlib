cmake_minimum_required(VERSION 3.16)

project(jvs-netlib)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(JVS_NETLIB_ENABLE_TESTS_DEFAULT ON)
else()
  set(JVS_NETLIB_ENABLE_TESTS_DEFAULT OFF)
endif()

option(JVS_NETLIB_BUILD_SHARED "Build jvs-netlib as a shared library" ON)
option(JVS_NETLIB_BUILD_STATIC "Build jvs-netlib as a static library" ON)
option(JVS_NETLIB_ENABLE_TESTS "Enable jvs-netlib testing" ${JVS_NETLIB_ENABLE_TESTS_DEFAULT})
option(JVS_NETLIB_ENABLE_EXAMPLES "Build jvs-netlib examples" OFF)

add_subdirectory(lib)

if (JVS_NETLIB_ENABLE_TESTS)
  add_subdirectory(unittests)
endif()

if (JVS_NETLIB_ENABLE_EXAMPLES)
  add_subdirectory(examples)
endif()
