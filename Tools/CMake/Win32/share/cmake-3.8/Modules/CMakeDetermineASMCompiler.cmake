# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# determine the compiler to use for ASM programs

include(${CMAKE_ROOT}/Modules/CMakeDetermineCompiler.cmake)

if(NOT CMAKE_ASM${ASM_DIALECT}_COMPILER)
  # prefer the environment variable ASM
  if(NOT $ENV{ASM${ASM_DIALECT}} STREQUAL "")
    get_filename_component(CMAKE_ASM${ASM_DIALECT}_COMPILER_INIT $ENV{ASM${ASM_DIALECT}} PROGRAM PROGRAM_ARGS CMAKE_ASM${ASM_DIALECT}_FLAGS_ENV_INIT)
    if(CMAKE_ASM${ASM_DIALECT}_FLAGS_ENV_INIT)
      set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ARG1 "${CMAKE_ASM${ASM_DIALECT}_FLAGS_ENV_INIT}" CACHE STRING "First argument to ASM${ASM_DIALECT} compiler")
    endif()
    if(NOT EXISTS ${CMAKE_ASM${ASM_DIALECT}_COMPILER_INIT})
      message(FATAL_ERROR "Could not find compiler set in environment variable ASM${ASM_DIALECT}:\n$ENV{ASM${ASM_DIALECT}}.")
    endif()
  endif()

  # finally list compilers to try
  if("ASM${ASM_DIALECT}" STREQUAL "ASM") # the generic assembler support
    if(NOT CMAKE_ASM_COMPILER_INIT)
      if(CMAKE_C_COMPILER)
        set(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}" CACHE FILEPATH "The ASM compiler")
        set(CMAKE_ASM_COMPILER_ID "${CMAKE_C_COMPILER_ID}")
      elseif(CMAKE_CXX_COMPILER)
        set(CMAKE_ASM_COMPILER "${CMAKE_CXX_COMPILER}" CACHE FILEPATH "The ASM compiler")
        set(CMAKE_ASM_COMPILER_ID "${CMAKE_CXX_COMPILER_ID}")
      else()
        # List all default C and CXX compilers
        set(CMAKE_ASM${ASM_DIALECT}_COMPILER_LIST
             ${_CMAKE_TOOLCHAIN_PREFIX}cc  ${_CMAKE_TOOLCHAIN_PREFIX}gcc cl bcc xlc
          CC ${_CMAKE_TOOLCHAIN_PREFIX}c++ ${_CMAKE_TOOLCHAIN_PREFIX}g++ aCC cl bcc xlC)
      endif()
    endif()
  else() # some specific assembler "dialect"
    if(NOT CMAKE_ASM${ASM_DIALECT}_COMPILER_INIT  AND NOT CMAKE_ASM${ASM_DIALECT}_COMPILER_LIST)
      message(FATAL_ERROR "CMAKE_ASM${ASM_DIALECT}_COMPILER_INIT or CMAKE_ASM${ASM_DIALECT}_COMPILER_LIST must be preset !")
    endif()
  endif()

  # Find the compiler.
  _cmake_find_compiler(ASM${ASM_DIALECT})

else()
  _cmake_find_compiler_path(ASM${ASM_DIALECT})
endif()
mark_as_advanced(CMAKE_ASM${ASM_DIALECT}_COMPILER)

if (NOT _CMAKE_TOOLCHAIN_LOCATION)
  get_filename_component(_CMAKE_TOOLCHAIN_LOCATION "${CMAKE_ASM${ASM_DIALECT}_COMPILER}" PATH)
endif ()


if(NOT CMAKE_ASM${ASM_DIALECT}_COMPILER_ID)

  # Table of per-vendor compiler id flags with expected output.
  list(APPEND CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDORS GNU )
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_FLAGS_GNU "--version")
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_REGEX_GNU "(GNU assembler)|(GCC)|(Free Software Foundation)")

  list(APPEND CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDORS HP )
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_FLAGS_HP "-V")
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_REGEX_HP "HP C")

  list(APPEND CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDORS Intel )
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_FLAGS_Intel "--version")
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_REGEX_Intel "(ICC)")

  list(APPEND CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDORS SunPro )
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_FLAGS_SunPro "-V")
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_REGEX_SunPro "Sun C")

  list(APPEND CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDORS XL )
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_FLAGS_XL "-qversion")
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_REGEX_XL "XL C")

  list(APPEND CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDORS MSVC )
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_FLAGS_MSVC "/?")
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_REGEX_MSVC "Microsoft")

  list(APPEND CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDORS TI )
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_FLAGS_TI "-h")
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_REGEX_TI "Texas Instruments")

  list(APPEND CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDORS GNU IAR)
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_FLAGS_IAR )
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_REGEX_IAR "IAR Assembler")

  list(APPEND CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDORS ARMCC)
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_FLAGS_ARMCC )
  set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID_VENDOR_REGEX_ARMCC "(ARM Compiler)|(ARM Assembler)")

  include(CMakeDetermineCompilerId)
  CMAKE_DETERMINE_COMPILER_ID_VENDOR(ASM${ASM_DIALECT})

endif()

if(CMAKE_ASM${ASM_DIALECT}_COMPILER_ID)
  message(STATUS "The ASM${ASM_DIALECT} compiler identification is ${CMAKE_ASM${ASM_DIALECT}_COMPILER_ID}")
else()
  message(STATUS "The ASM${ASM_DIALECT} compiler identification is unknown")
endif()



# If we have a gas/as cross compiler, they have usually some prefix, like
# e.g. powerpc-linux-gas, arm-elf-gas or i586-mingw32msvc-gas , optionally
# with a 3-component version number at the end
# The other tools of the toolchain usually have the same prefix
# NAME_WE cannot be used since then this test will fail for names like
# "arm-unknown-nto-qnx6.3.0-gas.exe", where BASENAME would be
# "arm-unknown-nto-qnx6" instead of the correct "arm-unknown-nto-qnx6.3.0-"
if (NOT _CMAKE_TOOLCHAIN_PREFIX)
  get_filename_component(COMPILER_BASENAME "${CMAKE_ASM${ASM_DIALECT}_COMPILER}" NAME)
  if (COMPILER_BASENAME MATCHES "^(.+-)g?as(-[0-9]+\\.[0-9]+\\.[0-9]+)?(\\.exe)?$")
    set(_CMAKE_TOOLCHAIN_PREFIX ${CMAKE_MATCH_1})
  endif ()
endif ()

# Now try the C compiler regexp:
if (NOT _CMAKE_TOOLCHAIN_PREFIX)
  if (COMPILER_BASENAME MATCHES "^(.+-)g?cc(-[0-9]+\\.[0-9]+\\.[0-9]+)?(\\.exe)?$")
    set(_CMAKE_TOOLCHAIN_PREFIX ${CMAKE_MATCH_1})
  endif ()
endif ()

# Finally try the CXX compiler regexp:
if (NOT _CMAKE_TOOLCHAIN_PREFIX)
  if (COMPILER_BASENAME MATCHES "^(.+-)[gc]\\+\\+(-[0-9]+\\.[0-9]+\\.[0-9]+)?(\\.exe)?$")
    set(_CMAKE_TOOLCHAIN_PREFIX ${CMAKE_MATCH_1})
  endif ()
endif ()


include(CMakeFindBinUtils)

set(CMAKE_ASM${ASM_DIALECT}_COMPILER_ENV_VAR "ASM${ASM_DIALECT}")

if(CMAKE_ASM${ASM_DIALECT}_COMPILER)
  message(STATUS "Found assembler: ${CMAKE_ASM${ASM_DIALECT}_COMPILER}")
else()
  message(STATUS "Didn't find assembler")
endif()


set(_CMAKE_ASM_COMPILER "${CMAKE_ASM${ASM_DIALECT}_COMPILER}")
set(_CMAKE_ASM_COMPILER_ID "${CMAKE_ASM${ASM_DIALECT}_COMPILER_ID}")
set(_CMAKE_ASM_COMPILER_ARG1 "${CMAKE_ASM${ASM_DIALECT}_COMPILER_ARG1}")
set(_CMAKE_ASM_COMPILER_ENV_VAR "${CMAKE_ASM${ASM_DIALECT}_COMPILER_ENV_VAR}")

# configure variables set in this file for fast reload later on
configure_file(${CMAKE_ROOT}/Modules/CMakeASMCompiler.cmake.in
  ${CMAKE_PLATFORM_INFO_DIR}/CMakeASM${ASM_DIALECT}Compiler.cmake @ONLY)

set(_CMAKE_ASM_COMPILER)
set(_CMAKE_ASM_COMPILER_ARG1)
set(_CMAKE_ASM_COMPILER_ENV_VAR)
