# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

# This module is shared by multiple languages; use include blocker.
if(__ANDROID_COMPILER_CLANG)
  return()
endif()
set(__ANDROID_COMPILER_CLANG 1)

# Support for NVIDIA Nsight Tegra Visual Studio Edition was previously
# implemented in the CMake VS IDE generators.  Avoid interfering with
# that functionality for now.  Later we may try to integrate this.
if(CMAKE_VS_PLATFORM_NAME STREQUAL "Tegra-Android")
  macro(__android_compiler_clang lang)
  endmacro()
  return()
endif()

# Commonly used Android toolchain files that pre-date CMake upstream support
# set CMAKE_SYSTEM_VERSION to 1.  Avoid interfering with them.
if(CMAKE_SYSTEM_VERSION EQUAL 1)
  macro(__android_compiler_clang lang)
  endmacro()
  return()
endif()

include(Platform/Android-Common)

# The NDK toolchain configuration files at:
#
#   <ndk>/[build/core/]toolchains/*-clang*/setup.mk
#
# contain logic to set LLVM_TRIPLE for Clang-based toolchains for each target.
# We need to produce the same target here to produce compatible binaries.
include(Platform/Android/abi-${CMAKE_ANDROID_ARCH_ABI}-Clang)

macro(__android_compiler_clang lang)
  if(NOT "x${lang}" STREQUAL "xASM")
    __android_compiler_common(${lang})
  endif()
  if(NOT CMAKE_${lang}_COMPILER_TARGET)
    set(CMAKE_${lang}_COMPILER_TARGET "${_ANDROID_ABI_CLANG_TARGET}")
  endif()
endmacro()
