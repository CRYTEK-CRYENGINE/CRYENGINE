# This file is processed when the IAR compiler is used for a C++ file

include(Compiler/IAR)
include(Compiler/CMakeCommonCompilerMacros)

if("${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" STREQUAL "ARM")
  # "(extended) embedded C++" Mode
  # old version: --ec++ or --eec++
  # since 8.10:  --c++ --no_exceptions --no_rtti
  #
  # --c++ is full C++ and supported since 6.10
  if(NOT CMAKE_IAR_CXX_FLAG)
    if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.10)
      set(CMAKE_IAR_CXX_FLAG --c++)
    else()
      set(CMAKE_IAR_CXX_FLAG --eec++)
    endif()
  endif()

  set(CMAKE_CXX_EXTENSION_COMPILE_OPTION -e)

  set(CMAKE_CXX98_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX98_EXTENSION_COMPILE_OPTION -e)

  if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.10)
  set(CMAKE_CXX03_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX03_EXTENSION_COMPILE_OPTION -e)
  set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION -e)
  set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION -e)
  endif()

  __compiler_iar_ARM(CXX)
  __compiler_check_default_language_standard(CXX 6.10 98 8.10 14)

elseif("${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" STREQUAL "AVR")
  __compiler_iar_AVR(CXX)
  set(CMAKE_CXX_OUTPUT_EXTENSION ".r90")
  if(NOT CMAKE_CXX_LINK_FLAGS)
    set(CMAKE_CXX_LINK_FLAGS "-Fmotorola")
  endif()

  set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_LINKER> <OBJECTS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <LINK_LIBRARIES> -o <TARGET>")
  set(CMAKE_CXX_CREATE_STATIC_LIBRARY "<CMAKE_AR> -o <TARGET> <OBJECTS> ")

  # add the target specific include directory:
  get_filename_component(_compilerDir "${CMAKE_C_COMPILER}" PATH)
  get_filename_component(_compilerDir "${_compilerDir}" PATH)
  include_directories("${_compilerDir}/inc")
endif()
