cmake_minimum_required(VERSION 3.8)
project(jvs-netlib)

option(JVS_NETLIB_BUILD_SHARED "Build jvs-netlib as a shared library" ON)
option(JVS_NETLIB_BUILD_STATIC "Build jvs-netlib as a static library" ON)
option(JVS_NETLIB_ENABLE_TESTS "Enable jvs-netlib testing" ON)

add_subdirectory(lib)

if (JVS_NETLIB_ENABLE_TESTS)
  add_subdirectory(unittests)
endif()
