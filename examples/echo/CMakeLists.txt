function(add_netlib_example exampleName)
  add_executable(${exampleName} ${ARGN})
  target_include_directories(${exampleName} PUBLIC ${NETLIB_INC_DIR})
  if (JVS_NETLIB_BUILD_STATIC)
    target_link_libraries(${exampleName} PRIVATE ${NETLIB_STATIC_NAME})
  else()
    target_link_libraries(${exampleName} PRIVATE ${NETLIB_SHARED_NAME})
  endif()
  if (NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(${exampleName} PRIVATE Threads::Threads)
  endif()
  set_target_properties(${exampleName}
    PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON)
endfunction()

add_netlib_example(echo-server-tcp echo_server_tcp.cpp)
