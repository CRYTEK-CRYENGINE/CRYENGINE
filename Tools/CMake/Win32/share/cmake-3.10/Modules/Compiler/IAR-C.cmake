# This file is processed when the IAR compiler is used for a C file

include(Compiler/IAR)
include(Compiler/CMakeCommonCompilerMacros)

# The toolchains for ARM and AVR are quite different:
if("${CMAKE_C_COMPILER_ARCHITECTURE_ID}" STREQUAL "ARM")

  set(CMAKE_C_EXTENSION_COMPILE_OPTION -e)

  set(CMAKE_C90_STANDARD_COMPILE_OPTION "")
  set(CMAKE_C90_EXTENSION_COMPILE_OPTION -e)

  if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 6.10)
    set(CMAKE_C90_STANDARD_COMPILE_OPTION --c89)
    set(CMAKE_C90_EXTENSION_COMPILE_OPTION --c89 -e)
    set(CMAKE_C99_STANDARD_COMPILE_OPTION "")
    set(CMAKE_C99_EXTENSION_COMPILE_OPTION -e)
  endif()
  if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 8.10)
    set(CMAKE_C11_STANDARD_COMPILE_OPTION "")
    set(CMAKE_C11_EXTENSION_COMPILE_OPTION -e)
  endif()

  __compiler_iar_ARM(C)
  __compiler_check_default_language_standard(C 1.10 90 6.10 99 8.10 11)

elseif("${CMAKE_C_COMPILER_ARCHITECTURE_ID}" STREQUAL "AVR")
  __compiler_iar_AVR(C)
  set(CMAKE_C_OUTPUT_EXTENSION ".r90")

  if(NOT CMAKE_C_LINK_FLAGS)
    set(CMAKE_C_LINK_FLAGS "-Fmotorola")
  endif()

  set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> <OBJECTS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <LINK_LIBRARIES> -o <TARGET>")
  set(CMAKE_C_CREATE_STATIC_LIBRARY "<CMAKE_AR> -o <TARGET> <OBJECTS> ")

  # add the target specific include directory:
  get_filename_component(_compilerDir "${CMAKE_C_COMPILER}" PATH)
  get_filename_component(_compilerDir "${_compilerDir}" PATH)
  include_directories("${_compilerDir}/inc" )
endif()
