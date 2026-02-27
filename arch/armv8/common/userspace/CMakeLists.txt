FILE(GLOB userspace_libc_SOURCES ${CMAKE_CURRENT_LIST_DIR}/*.c)

target_sources(userspace_libc
  PRIVATE
    ${userspace_libc_SOURCES})

FILE(GLOB userspace_libcpp_SOURCES ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

target_sources(userspace_libcpp
  PRIVATE
    ${userspace_libcpp_SOURCES})
