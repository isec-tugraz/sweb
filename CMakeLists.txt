cmake_minimum_required(VERSION 3.10.0)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# Include custom CMake modules from the cmake/ subdirectory
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set(HDD_IMAGE_RAW "SWEB-flat.vmdk")
set(HDD_IMAGE "SWEB.qcow2")

if ("${ARCH}" STREQUAL "")
  set (ARCH "x86/64")
endif("${ARCH}" STREQUAL "")
MESSAGE("-- Target architecture: ${ARCH}")

if ("${ARCH}" MATCHES "arm.*")
  add_definitions(-DARM)
endif("${ARCH}" MATCHES "arm.*")

unset(CMAKE_CROSSCOMPILING)
if(APPLE OR WIN32)
  set(CMAKE_CROSSCOMPILING 1)
  MESSAGE("-- Cross-Compiling on Apple/Windows")
endif(APPLE OR WIN32)

# add-dbg for userspace binaries
include(AddDebugInfo)

include(arch/${ARCH}/CMakeLists.compiler)

project(sweb
  LANGUAGES C CXX ASM)

string(REPLACE "/" ";" ARCH_LIST ${ARCH})
string(REPLACE "/" "_" ARCH_ESC ${ARCH})
string(TOUPPER ${ARCH_ESC} ARCH_ESC)

add_definitions(-DCMAKE_${ARCH_ESC}=1)

list(LENGTH ARCH_LIST ARCH_DEPTH)

#Find program executables needed during compilation
find_program(LD_EXECUTABLE gcc)
find_program(OBJCOPY_EXECUTABLE objcopy)
find_program(DOXYGEN_EXECUTABLE doxygen)
find_program(STAT_EXECUTABLE stat)

set(ColourReset "")
set(BoldRed     "")
if(NOT WIN32)
  string(ASCII 27 Esc)
  set(ColourReset "${Esc}[m")
  set(BoldRed     "${Esc}[1;31m")
endif()
set(TMPFS_CHECK "none")
execute_process(COMMAND ${STAT_EXECUTABLE} --file-system --format=%T ${PROJECT_BINARY_DIR}
                OUTPUT_VARIABLE TMPFS_CHECK
                OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT("${TMPFS_CHECK}" STREQUAL "tmpfs"))
  MESSAGE("-- ${BoldRed}WARNING: build folder is not tmpfs - compilation will be slow and bad for the hard disk${ColourReset}")
endif(NOT("${TMPFS_CHECK}" STREQUAL "tmpfs"))

set(NOPIEFLAG -no-pie)
execute_process(COMMAND ${LD_EXECUTABLE} ${NOPIEFLAG}
                ERROR_VARIABLE PIE_CHECK
                ERROR_STRIP_TRAILING_WHITESPACE)
if("${PIE_CHECK}" MATCHES ".*unrecognized.*")
set(NOPIEFLAG )
endif()
set(NOPICFLAG -fno-PIC)
execute_process(COMMAND ${CMAKE_C_COMPILER} ${NOPICFLAG}
                ERROR_VARIABLE PIC_CHECK
                ERROR_STRIP_TRAILING_WHITESPACE)
if("${PIC_CHECK}" MATCHES ".*unrecognized.*")
set(NOPICFLAG )
endif()

#Initialize CMake output directories
set(LIBRARY_OUTPUT_PATH  "${PROJECT_BINARY_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")

include(arch/${ARCH}/CMakeLists.include)
include(arch/${ARCH}/CMakeLists.userspace)
string (REPLACE ";" " " KERNEL_CMAKE_C_FLAGS_STR "${KERNEL_CMAKE_C_FLAGS}")
set(CMAKE_ASM_FLAGS ${KERNEL_CMAKE_C_FLAGS_STR})

if(CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSS_COMPILE_FLAGS -G "Unix Makefiles")
else(CMAKE_CROSSCOMPILING) # not cross compiling
  set(CMAKE_CROSS_COMPILE_FLAGS )
endif(CMAKE_CROSSCOMPILING)

if ("${DEBUG}" STREQUAL "1")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG=1")
endif()


# Searches for asm, c and cpp files and adds the library
function(ADD_PROJECT_LIBRARY LIBRARY_NAME)
  arch2obj(archobj_libname ${LIBRARY_NAME})

  file(GLOB source_files ${SOURCE_WILDCARDS})

  set(library_files)

  if(source_files)
    set(library_files ${source_files})
  endif(source_files)

  if(archobj_libname)
    set(library_files ${library_files} $<TARGET_OBJECTS:${archobj_libname}>)
  endif(archobj_libname)

  if(library_files)
    add_library(${LIBRARY_NAME} ${library_files})

    if(archobj_libname)
      add_dependencies(${LIBRARY_NAME} ${archobj_libname})
    endif(archobj_libname)

    target_compile_options(${LIBRARY_NAME} PRIVATE
      $<$<COMPILE_LANGUAGE:CXX>:${KERNEL_CMAKE_CXX_FLAGS}>
      $<$<COMPILE_LANGUAGE:C>:${KERNEL_CMAKE_C_FLAGS}>
      )

    set(ENV{LIBRARY_NAMES} "$ENV{LIBRARY_NAMES};${LIBRARY_NAME}")
  endif(library_files)
endfunction(ADD_PROJECT_LIBRARY)

set (SOURCE_WILDCARDS *.cpp *.c *.S)

set(LIBRARY_FILENAMES)

#Initialize global (environment) variables
set(ENV{LIBRARY_NAMES})

#Create target for userspace libc (Need to to that here because some files (e.g. syscall.c) are all over the place)
add_library(userspace_libc "")
target_compile_options(userspace_libc PUBLIC ${ARCH_USERSPACE_COMPILE_OPTIONS})

#Add the source directories
add_subdirectory(arch)
add_subdirectory(common)
add_subdirectory(utils)
add_subdirectory(userspace)


#FINAL_LIB_NAMES should contain the names of all libraries
#these names can be used to link the kernel, no unpacking of *.a files is needed anymore
set(FINAL_LIB_NAMES $ENV{LIBRARY_NAMES})

#Name of the executables of the userspace, needed for dependency checking
set(FINAL_USERSPACE_NAMES $ENV{USERSPACE_NAMES})



#Build the Linker command
set(KERNEL_LD_ARGUMENT ${KERNEL_LD_ARGUMENT} -g -u entry -Wl,-T ${CMAKE_SOURCE_DIR}/arch/${ARCH}/utils/kernel-ld-script.ld)
set(KERNEL_LD_ARGUMENT ${KERNEL_LD_ARGUMENT} -o ${PROJECT_BINARY_DIR}/kernel.x)
#set(KERNEL_LD_ARGUMENT ${KERNEL_LD_ARGUMENT} -Wl,-Map -Wl,${PROJECT_BINARY_DIR}/kernel.map)

set(KERNEL_LD_ARGUMENT ${KERNEL_LD_ARGUMENT} -Wl,--start-group)
foreach(libfile ${FINAL_LIB_NAMES})
  set(KERNEL_LD_ARGUMENT ${KERNEL_LD_ARGUMENT} ${LIBRARY_OUTPUT_PATH}/lib${libfile}.a)
endforeach(libfile)
set(KERNEL_LD_ARGUMENT ${KERNEL_LD_ARGUMENT} -Wl,--end-group)
set(KERNEL_LD_ARGUMENT ${KERNEL_LD_ARGUMENT} ${ARCH_APPEND_LD_ARGUMENTS})

#Build userspace exe2minixfs command
set(MINIXFS_ARGUMENT "")
foreach(file $ENV{USERSPACE_NAMES_EXE2MINIX})
  list(APPEND MINIXFS_ARGUMENT ${file})
endforeach(file)

file(GLOB userspace_data userspace/data/*)
foreach(file ${userspace_data})
  get_filename_component(datafile ${file} NAME)
  list(APPEND MINIXFS_ARGUMENT ${file} ${datafile})
endforeach(file)

#Custom Target: hdd_image
#Creates the hd image and copies all files to it
add_custom_target (hdd_image ALL
  DEPENDS kernel_to_image userspace_to_image
  COMMAND qemu-img convert -f raw -O qcow2 ${HDD_IMAGE_RAW} ${HDD_IMAGE}
)

#Custom Command: invoke exe2minixfs and copy boot files to our hd image
add_custom_target (kernel_to_image
    DEPENDS blank_hdd_image kernel_image exe2minixfs

    # x86/32 doesn't generate kernel.dbg
    COMMAND ${CMAKE_COMMAND} -E touch "./kernel.dbg"
    COMMAND ./exe2minixfs ${HDD_IMAGE_RAW} 32256 "${CMAKE_SOURCE_DIR}/utils/images/menu.lst" boot/grub/menu.lst
                                                 ./kernel.x boot/kernel.x
                                                 ./kernel.dbg boot/kernel.dbg
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMENT "Copying kernel files to image..."
)

#Custom Command: Outputs kernel_image
#Executes the linker command after all libraries where build successfully
if("${ARCH}" MATCHES "arm")
#For armv8 we need to execute the final linking step twice because after the first time
#The debug info gets extracted and linked in at the second link step
add_custom_target (kernel_image
    DEPENDS ${FINAL_LIB_NAMES} add-dbg
    COMMAND ${LD_EXECUTABLE} ${KERNEL_LD_ARGUMENT} ${KERNEL_IMAGE_OBJCOPY}
    COMMAND ${LD_EXECUTABLE} ${KERNEL_LD_ARGUMENT} ${ADD_LD_ARGUMENT}
    WORKING_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)

else()

add_custom_target (kernel_image
    DEPENDS ${FINAL_LIB_NAMES} add-dbg
    COMMAND ${LD_EXECUTABLE} ${KERNEL_LD_ARGUMENT} ${KERNEL_IMAGE_OBJCOPY}
    WORKING_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)

endif()

#Custom Command: invoke exe2minixfs and copy all userspace programs to our hd image second partition
add_custom_target (userspace_to_image
    DEPENDS blank_hdd_image ${FINAL_USERSPACE_NAMES} exe2minixfs
    COMMAND mkdir -p userspace/data
    COMMAND cp -f ${CMAKE_SOURCE_DIR}/userspace/data/* ${PROJECT_BINARY_DIR}/userspace/data
    COMMAND ./exe2minixfs ${HDD_IMAGE_RAW} 10321920 ${MINIXFS_ARGUMENT}
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMENT "Copying userspace programs to image..."
)


#Custom target: make bochs
#Run bochs in non debugging mode
add_custom_target(bochs
    COMMAND ${BOCHS_PATH} -q -f ${PROJECT_SOURCE_DIR}/utils/bochsrc
    COMMENT "Going to ${BOCHS_PATH} -f ${PROJECT_SOURCE_DIR}/utils/bochsrc"
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)

#Custom target: make bochsgdb
#Run bochs in debugging mode
add_custom_target(bochsgdb
    COMMAND ${BOCHS_PATH} -q -f '${PROJECT_SOURCE_DIR}/utils/bochsrc' "gdbstub: enabled=1, port=1234"
    COMMENT "Going to ${BOCHS_PATH} -f ${PROJECT_SOURCE_DIR}/utils/bochsrc \"gdbstub: enabled=1, port=1234\""
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)

#Custom target: make emu
#Run qemu in emu mode
add_custom_target(emu
    COMMAND cat '${PROJECT_SOURCE_DIR}/utils/emu.txt'
)

#Custom target: make runddd
#Run ddd with some presets
add_custom_target(runddd
    COMMAND ddd -cd ${PROJECT_SOURCE_DIR}/ -command ${PROJECT_SOURCE_DIR}/utils/gdbinit ${PROJECT_BINARY_DIR}/${KERNEL_BINARY}
    COMMENT "Going to ddd"
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)

#Custom target: make runcgdb
#Run cgdb with some presets
add_custom_target(runcgdb
    COMMAND cgdb -cd ${PROJECT_SOURCE_DIR}/ -command ${PROJECT_SOURCE_DIR}/utils/gdbinit ${PROJECT_BINARY_DIR}/${KERNEL_BINARY}
    COMMENT "Going to cgdb"
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)

#Custom target: make rungdbgui
#Run gdbgui with some presets
add_custom_target(rungdbgui
    COMMAND gdbgui -g 'gdb --ex "file ${PROJECT_BINARY_DIR}/${KERNEL_BINARY}" --command=${PROJECT_SOURCE_DIR}/utils/gdbinit' --project /tmp/sweb_upstream/ ${PROJECT_BINARY_DIR}/${KERNEL_BINARY}
    COMMENT "Going to gdbgui"
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)


#Custom target: make rungdb
#Run gdb with some presets
add_custom_target(rungdb
    COMMAND gdb -cd ${PROJECT_SOURCE_DIR}/ -command ${PROJECT_SOURCE_DIR}/utils/gdbinit ${PROJECT_BINARY_DIR}/${KERNEL_BINARY}
    COMMENT "Going to gdb"
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)

#Custom target: make mrproper
#Makes really clean (except for the object files)
add_custom_target(mrproper
    COMMAND make clean
    COMMAND ${PROJECT_SOURCE_DIR}/utils/prompt.sh "rm -fR ${PROJECT_BINARY_DIR}/*: remove all arguments recursively [Y/n]? "
    COMMAND rm -fR ${PROJECT_BINARY_DIR}/* || true
    COMMAND cmake -DARCH=${ARCH} ${PROJECT_SOURCE_DIR} ${CMAKE_CROSS_COMPILE_FLAGS}
)

#Custom target: make debug
add_custom_target(debug
    COMMAND make clean
    COMMAND ${PROJECT_SOURCE_DIR}/utils/prompt.sh "rm -fR ${PROJECT_BINARY_DIR}/*: remove all arguments recursively [Y/n]? "
    COMMAND rm -fR ${PROJECT_BINARY_DIR}/* || true
    COMMAND cmake -DDEBUG=1 -DARCH=${ARCH} ${PROJECT_SOURCE_DIR} ${CMAKE_CROSS_COMPILE_FLAGS}
)


# Define the "make ${ARCH}" targets
include(MakeArchTarget)

MAKE_ARCH_TARGET(x86_32 x86/32 "")
MAKE_ARCH_TARGET(x86_32_pae x86/32/pae "")
MAKE_ARCH_TARGET(x86_64 x86/64 "")

MAKE_ARCH_TARGET(arm_icp arm/integratorcp "")
MAKE_ARCH_TARGET(arm_rpi2 arm/rpi2 "")
MAKE_ARCH_TARGET(armv8_rpi3 armv8/rpi3 "-DVIRTUALIZED_QEMU=1")
MAKE_ARCH_TARGET(armv8_rpi3_hardware armv8/rpi3 "")



#Builds the doxygen documentation
add_custom_target(doxygen
    COMMAND mkdir -p \"${PROJECT_SOURCE_DIR}/doc\"
    COMMAND cp \"${PROJECT_SOURCE_DIR}/utils/sweb.dox\" \"${PROJECT_SOURCE_DIR}/doc/sweb_cmake.dox\"
    COMMAND sed -i -e 's/\\$$\\[DOC_OUTPUT_DIR\\]$$/docs/' '${PROJECT_SOURCE_DIR}/doc/sweb_cmake.dox'
    COMMAND ${DOXYGEN_EXECUTABLE} \"${PROJECT_SOURCE_DIR}/doc/sweb_cmake.dox\"
    COMMAND rm -f \"${PROJECT_SOURCE_DIR}/doc/sweb_cmake.dox\"
    COMMENT "Building SWEB documentation"
)

#Outputs help on build targets
add_custom_target(showhelp
    COMMAND printf \"\\nBuild target summary:\\n\\n\"
    COMMAND printf \"<default target>\\tbuild changed files, generate kernel,\\n\\t\\t\\textract hdd image, copy kernel\\n\\t\\t\\tand userspace file to image\\n\"
    COMMAND printf \"\\nclean\\t\\tclean up object files\\n\"
    COMMAND printf \"\\nmrproper\\tdeletes binary dir, cleans up,\\n\\t\\tregenerates all cmake specific files\\n\"
    COMMAND printf \"\\nbochs\\t\\truns bochs without gdb\\n\"
    COMMAND printf \"\\nbochsgdb\\truns bochs, waiting for gdb connection at localhost:1234\\n\"
    COMMAND printf \"\\nqemu\\t\\truns qemu, without gdb\\n\"
    COMMAND printf \"\\nqemugdb\\t\\truns qemu, waiting for gdb connection at localhost:1234\\n\"
    COMMAND printf \"\\nrunddd\\t\\truns ddd, connecting to localhost:1234\\n\"
    COMMAND printf \"\\nruncgdb\\t\\truns cgdb, connecting to localhost:1234\\n\"
    COMMAND printf \"\\nrungdb\\t\\truns gdb, connecting to localhost:1234\\n\"
    COMMAND printf \"\\ndoxygen\\t\\tbuilds the html and latex source documentation of SWEB\\n\"
    COMMAND printf \"\\nshowhelp\\t\\tshows this target summary\\n\"
    COMMAND printf \"\\n\\n\"
)
