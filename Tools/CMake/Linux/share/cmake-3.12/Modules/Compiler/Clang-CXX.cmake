include(Compiler/Clang)
__compiler_clang(CXX)

if(NOT "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
  set(CMAKE_CXX_COMPILE_OPTIONS_VISIBILITY_INLINES_HIDDEN "-fvisibility-inlines-hidden")
endif()

cmake_policy(GET CMP0025 appleClangPolicy)
if(APPLE AND NOT appleClangPolicy STREQUAL NEW)
  return()
endif()

if(NOT "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
  if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 2.1)
    set(CMAKE_CXX98_STANDARD_COMPILE_OPTION "-std=c++98")
    set(CMAKE_CXX98_EXTENSION_COMPILE_OPTION "-std=gnu++98")
  endif()

  if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.1)
    set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-std=c++11")
    set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-std=gnu++11")
  elseif(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 2.1)
    set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-std=c++0x")
    set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-std=gnu++0x")
  endif()

  if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.5)
    set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "-std=c++14")
    set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "-std=gnu++14")
  elseif(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.4)
    set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "-std=c++1y")
    set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "-std=gnu++1y")
  endif()

  set(_clang_version_std17 5.0)
  if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(_clang_version_std17 6.0)
  endif()

  if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "${_clang_version_std17}")
    set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std=c++17")
    set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "-std=gnu++17")
  elseif (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.5)
    set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std=c++1z")
    set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "-std=gnu++1z")
  endif()

  if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "${_clang_version_std17}")
    set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std=c++2a")
    set(CMAKE_CXX20_EXTENSION_COMPILE_OPTION "-std=gnu++2a")
  endif()

  unset(_clang_version_std17)

  __compiler_check_default_language_standard(CXX 2.1 98)
elseif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 3.9
    AND CMAKE_CXX_SIMULATE_VERSION VERSION_GREATER_EQUAL 19.0)
  # This version of clang-cl and the MSVC version it simulates have
  # support for -std: flags.
  set(CMAKE_CXX98_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX98_EXTENSION_COMPILE_OPTION "")
  set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "")
  set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "-std:c++14")
  set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "-std:c++14")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 6.0)
    set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std:c++17")
    set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "-std:c++17")
    set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std:c++latest")
    set(CMAKE_CXX20_EXTENSION_COMPILE_OPTION "-std:c++latest")
  else()
    set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "-std:c++latest")
    set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "-std:c++latest")
  endif()

  __compiler_check_default_language_standard(CXX 3.9 14)
else()
  # This version of clang-cl, or the MSVC version it simulates, does not have
  # language standards.  Set these options as empty strings so the feature
  # test infrastructure can at least check to see if they are defined.
  set(CMAKE_CXX98_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX98_EXTENSION_COMPILE_OPTION "")
  set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "")
  set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "")
  set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "")
  set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "")
  set(CMAKE_CXX20_EXTENSION_COMPILE_OPTION "")

  # There is no meaningful default for this
  set(CMAKE_CXX_STANDARD_DEFAULT "")

  # There are no compiler modes so we only need to test features once.
  # Override the default macro for this special case.  Pretend that
  # all language standards are available so that at least compilation
  # can be attempted.
  macro(cmake_record_cxx_compile_features)
    list(APPEND CMAKE_CXX_COMPILE_FEATURES
      cxx_std_98
      cxx_std_11
      cxx_std_14
      cxx_std_17
      cxx_std_20
      )
    _record_compiler_features(CXX "" CMAKE_CXX_COMPILE_FEATURES)
  endmacro()
endif()
