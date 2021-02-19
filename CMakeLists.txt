cmake_minimum_required(VERSION 3.8)
project(jvs-netlib)

option(JVS_NETLIB_BUILD_SHARED "Build jvs-netlib as a shared library" ON)
option(JVS_NETLIB_BUILD_STATIC "Build jvs-netlib as a static library" ON)
option(JVS_NETLIB_ENABLE_TESTS "Enable jvs-netlib testing" ON)

################################################################################
# Library configuration


set(incDir ${CMAKE_SOURCE_DIR}/include)
set(libDir ${CMAKE_SOURCE_DIR}/lib)
set(unitTestDir ${CMAKE_SOURCE_DIR}/unittests)

# source files
set(srcFiles 
  ${libDir}/error.cpp
  ${libDir}/ip_address.cpp
  ${libDir}/ip_end_point.cpp
  ${libDir}/socket_errors.cpp
  )

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows" OR MSVC)
  list(APPEND srcFiles ${libDir}/winsock/winsock_sockets.cpp)
else()
  list(APPEND srcFiles ${libDir}/bsd/bsd_sockets.cpp)
endif()

# header files
set(pubIncFileNames
  convert_cast.h
  endianness.h
  error.h
  ip_address.h
  ip_end_point.h
  network_integers.h
  socket.h
  )

foreach(pubIncFileName ${pubIncFileNames})
  list(APPEND pubIncFiles "${incDir}/${pubIncFileName}")
endforeach()

# library options

set(sharedName "jvs-netlib")
set(staticName "jvs-netlib-static")
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(sharedName "${sharedName}-debug")
  set(staticName "${staticName}-debug")
endif()

if (JVS_NETLIB_BUILD_SHARED)
  add_library(${sharedName} SHARED
    ${srcFiles}
    )
  set_target_properties(${sharedName}
    PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON
      PUBLIC_HEADER "${pubIncFiles}")
  target_include_directories(${sharedName}
    PUBLIC ${incDir}
    PRIVATE ${libDir})
  install(TARGETS ${sharedName} 
    DESTINATION lib
    PUBLIC_HEADER DESTINATION "include")
endif()

if (JVS_NETLIB_BUILD_STATIC)
  add_library(${staticName} STATIC
    ${srcFiles}
    )
  set_target_properties(${staticName}
    PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON
      PUBLIC_HEADER "${pubIncFiles}")
  target_include_directories(${staticName}
    PUBLIC ${incDir}
    PRIVATE ${libDir})
  install(TARGETS ${staticName} 
    DESTINATION lib
    PUBLIC_HEADER DESTINATION "include")
endif()

################################################################################
# Google Test acquisition and configuration

include(FetchContent)

if (JVS_NETLIB_ENABLE_TESTS)
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
  set(testSources
    ${unitTestDir}/unittest_main.cpp
    ${unitTestDir}/ip_address_test.cpp
    ${unitTestDir}/ip_end_point_test.cpp
    ${unitTestDir}/network_integer_test.cpp
    ${unitTestDir}/socket_test.cpp
    )
  add_executable(jvs-netlib-test ${testSources})
  target_include_directories(jvs-netlib-test PRIVATE 
    ${googletest_SOURCE_DIR}/include
    ${incDir})
  target_link_libraries(jvs-netlib-test gtest_main)
  if (JVS_NETLIB_BUILD_SHARED)
    target_link_libraries(jvs-netlib-test ${sharedName})
  else()
    target_link_libraries(jvs-netlib-test ${staticName})
  endif()
  set_target_properties(jvs-netlib-test
    PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON
    )
  enable_testing()
  add_test(NAME jvs-netlib-test
    COMMAND jvs-netlib-test)
endif()
