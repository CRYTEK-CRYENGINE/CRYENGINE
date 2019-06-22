# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.


# This module is shared by multiple languages; use include blocker.
if(__COMPILER_CLANG)
  return()
endif()
set(__COMPILER_CLANG 1)

include(Compiler/CMakeCommonCompilerMacros)

if("x${CMAKE_C_SIMULATE_ID}" STREQUAL "xMSVC"
    OR "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC"
    OR "x${CMAKE_Fortran_SIMULATE_ID}" STREQUAL "xMSVC")
  macro(__compiler_clang lang)
  endmacro()
else()
  include(Compiler/GNU)

  macro(__compiler_clang lang)
    __compiler_gnu(${lang})
    set(CMAKE_${lang}_COMPILE_OPTIONS_PIE "-fPIE")
    set(CMAKE_INCLUDE_SYSTEM_FLAG_${lang} "-isystem ")
    set(CMAKE_${lang}_COMPILE_OPTIONS_VISIBILITY "-fvisibility=")
    if(CMAKE_${lang}_COMPILER_VERSION VERSION_LESS 3.4.0)
      set(CMAKE_${lang}_COMPILE_OPTIONS_TARGET "-target ")
      set(CMAKE_${lang}_COMPILE_OPTIONS_EXTERNAL_TOOLCHAIN "-gcc-toolchain ")
    else()
      set(CMAKE_${lang}_COMPILE_OPTIONS_TARGET "--target=")
      set(CMAKE_${lang}_COMPILE_OPTIONS_EXTERNAL_TOOLCHAIN "--gcc-toolchain=")
    endif()

    set(_CMAKE_${lang}_IPO_SUPPORTED_BY_CMAKE YES)
    set(_CMAKE_${lang}_IPO_MAY_BE_SUPPORTED_BY_COMPILER YES)

    string(COMPARE EQUAL "${CMAKE_${lang}_COMPILER_ID}" "AppleClang" __is_apple_clang)

    # '-flto=thin' available since Clang 3.9 and Xcode 8
    # * http://clang.llvm.org/docs/ThinLTO.html#clang-llvm
    # * https://trac.macports.org/wiki/XcodeVersionInfo
    set(_CMAKE_LTO_THIN TRUE)
    if(__is_apple_clang)
      if(CMAKE_${lang}_COMPILER_VERSION VERSION_LESS 8.0)
        set(_CMAKE_LTO_THIN FALSE)
      endif()
    else()
      if(CMAKE_${lang}_COMPILER_VERSION VERSION_LESS 3.9)
        set(_CMAKE_LTO_THIN FALSE)
      endif()
    endif()

    if(_CMAKE_LTO_THIN)
      set(CMAKE_${lang}_COMPILE_OPTIONS_IPO "-flto=thin")
    else()
      set(CMAKE_${lang}_COMPILE_OPTIONS_IPO "-flto")
    endif()

    if(ANDROID)
      # https://github.com/android-ndk/ndk/issues/242
      set(CMAKE_${lang}_LINK_OPTIONS_IPO "-fuse-ld=gold")
    endif()

    if(ANDROID OR __is_apple_clang)
      set(__ar "${CMAKE_AR}")
      set(__ranlib "${CMAKE_RANLIB}")
    else()
      set(__ar "${CMAKE_${lang}_COMPILER_AR}")
      set(__ranlib "${CMAKE_${lang}_COMPILER_RANLIB}")
    endif()

    set(CMAKE_${lang}_ARCHIVE_CREATE_IPO
      "\"${__ar}\" cr <TARGET> <LINK_FLAGS> <OBJECTS>"
    )

    set(CMAKE_${lang}_ARCHIVE_APPEND_IPO
      "\"${__ar}\" r <TARGET> <LINK_FLAGS> <OBJECTS>"
    )

    set(CMAKE_${lang}_ARCHIVE_FINISH_IPO
      "\"${__ranlib}\" <TARGET>"
    )
  endmacro()
endif()
