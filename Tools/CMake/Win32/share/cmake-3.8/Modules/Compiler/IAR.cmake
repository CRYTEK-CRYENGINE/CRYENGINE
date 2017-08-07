# This file is processed when the IAR compiler is used for a C or C++ file
# Documentation can be downloaded here: http://www.iar.com/website1/1.0.1.0/675/1/
# The initial feature request is here: https://gitlab.kitware.com/cmake/cmake/issues/10176
# It also contains additional links and information.

if(_IAR_CMAKE_LOADED)
  return()
endif()
set(_IAR_CMAKE_LOADED TRUE)


get_filename_component(_CMAKE_C_TOOLCHAIN_LOCATION "${CMAKE_C_COMPILER}" PATH)
get_filename_component(_CMAKE_CXX_TOOLCHAIN_LOCATION "${CMAKE_CXX_COMPILER}" PATH)
get_filename_component(_CMAKE_ASM_TOOLCHAIN_LOCATION "${CMAKE_ASM_COMPILER}" PATH)


if("${CMAKE_C_COMPILER}" MATCHES "arm"  OR  "${CMAKE_CXX_COMPILER}" MATCHES "arm"  OR  "${CMAKE_ASM_COMPILER}" MATCHES "arm")
  set(CMAKE_EXECUTABLE_SUFFIX ".elf")

  # For arm, IAR uses the "ilinkarm" linker and "iarchive" archiver:
  find_program(CMAKE_IAR_LINKER ilinkarm HINTS "${_CMAKE_C_TOOLCHAIN_LOCATION}" "${_CMAKE_CXX_TOOLCHAIN_LOCATION}" "${_CMAKE_ASM_TOOLCHAIN_LOCATION}")
  find_program(CMAKE_IAR_AR iarchive HINTS "${_CMAKE_C_TOOLCHAIN_LOCATION}" "${_CMAKE_CXX_TOOLCHAIN_LOCATION}" "${_CMAKE_ASM_TOOLCHAIN_LOCATION}" )

  set(IAR_TARGET_ARCHITECTURE "ARM" CACHE STRING "IAR compiler target architecture")
endif()

if("${CMAKE_C_COMPILER}" MATCHES "avr"  OR  "${CMAKE_CXX_COMPILER}" MATCHES "avr"  OR  "${CMAKE_ASM_COMPILER}" MATCHES "avr")
  set(CMAKE_EXECUTABLE_SUFFIX ".bin")

  # For AVR and AVR32, IAR uses the "xlink" linker and the "xar" archiver:
  find_program(CMAKE_IAR_LINKER xlink HINTS "${_CMAKE_C_TOOLCHAIN_LOCATION}" "${_CMAKE_CXX_TOOLCHAIN_LOCATION}" "${_CMAKE_ASM_TOOLCHAIN_LOCATION}" )
  find_program(CMAKE_IAR_AR xar HINTS "${_CMAKE_C_TOOLCHAIN_LOCATION}" "${_CMAKE_CXX_TOOLCHAIN_LOCATION}" "${_CMAKE_ASM_TOOLCHAIN_LOCATION}" )

  set(IAR_TARGET_ARCHITECTURE "AVR" CACHE STRING "IAR compiler target architecture")

  set(CMAKE_LIBRARY_PATH_FLAG "-I")

endif()

if(NOT IAR_TARGET_ARCHITECTURE)
  message(FATAL_ERROR "The IAR compiler for this architecture is not yet supported "
          "by CMake. Please go to https://gitlab.kitware.com/cmake/cmake/issues "
          "and enter a feature request there.")
endif()

set(CMAKE_LINKER "${CMAKE_IAR_LINKER}" CACHE FILEPATH "The IAR linker" FORCE)
set(CMAKE_AR "${CMAKE_IAR_AR}" CACHE FILEPATH "The IAR archiver" FORCE)
