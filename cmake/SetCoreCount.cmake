if(ARCH MATCHES "arm" AND NOT CORES STREQUAL "4")
  message(FATAL_ERROR "The Raspberry Pi QEMU boards require four CPUs; use the 4core target.")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -DQEMU_CPUS=${CORES} "${SOURCE_DIR}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Could not change the QEMU CPU count")
endif()
