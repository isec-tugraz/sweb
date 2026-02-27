FILE(GLOB userspace_libcpp_SOURCES ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

target_sources(userspace_libcpp
  PRIVATE
    ${userspace_libcpp_SOURCES})
