################################################################################
# Library configuration

set(NETLIB_INC_DIR ${CMAKE_SOURCE_DIR}/include CACHE PATH "" FORCE)
set(NETLIB_LIB_DIR ${CMAKE_SOURCE_DIR}/lib CACHE PATH "" FORCE)
set(unitTestDir ${CMAKE_SOURCE_DIR}/unittests)

# source files
set(srcFiles 
  error.cpp
  ip_address.cpp
  ip_end_point.cpp
  socket.cpp
  socket_errors.cpp
  socket_impl.cpp)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
  list(APPEND srcFiles ${NETLIB_LIB_DIR}/winsock_impl.cpp)
else()
  list(APPEND srcFiles ${NETLIB_LIB_DIR}/bsd_sockets_impl.cpp)
endif()

# header files
set(pubIncFileNames
  convert_cast.h
  endianness.h
  error.h
  ip_address.h
  ip_end_point.h
  network_integers.h
  socket.h)

foreach(pubIncFileName ${pubIncFileNames})
  list(APPEND pubIncFiles "${NETLIB_INC_DIR}/${pubIncFileName}")
endforeach()

# library options

set(NETLIB_SHARED_NAME "jvs-netlib" CACHE STRING "" FORCE)
set(NETLIB_STATIC_NAME "jvs-netlib-static" CACHE STRING "" FORCE)
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(NETLIB_SHARED_NAME "${NETLIB_SHARED_NAME}-debug" CACHE STRING "" FORCE)
  set(NETLIB_STATIC_NAME "${NETLIB_STATIC_NAME}-debug" CACHE STRING "" FORCE)
endif()

function(add_netlib_library libType)
  if ("${libType}" STREQUAL "STATIC")
    set(libName ${NETLIB_STATIC_NAME})
  elseif("${libType}" STREQUAL "SHARED")
    set(libName ${NETLIB_SHARED_NAME})
  endif()

  add_library(${libName} ${libType}
    ${srcFiles})
  set_target_properties(${libName}
    PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON
      PUBLIC_HEADER "${pubIncFiles}")
  target_include_directories(${libName}
    PUBLIC ${NETLIB_INC_DIR}
    PRIVATE ${NETLIB_LIB_DIR})
  install(TARGETS ${libName} 
    DESTINATION lib
    PUBLIC_HEADER DESTINATION "include")
  if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    target_link_libraries(${libName} ws2_32)
  endif()
endfunction()

if (JVS_NETLIB_BUILD_SHARED)
  add_netlib_library(SHARED)
endif()

if (JVS_NETLIB_BUILD_STATIC)
  add_netlib_library(STATIC)
endif()