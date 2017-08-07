# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CheckCXXCompilerFlag
# --------------------
#
# Check whether the CXX compiler supports a given flag.
#
# CHECK_CXX_COMPILER_FLAG(<flag> <var>)
#
# ::
#
#   <flag> - the compiler flag
#   <var>  - variable to store the result
#
# This internally calls the check_cxx_source_compiles macro and sets
# CMAKE_REQUIRED_DEFINITIONS to <flag>.  See help for
# CheckCXXSourceCompiles for a listing of variables that can otherwise
# modify the build.  The result only tells that the compiler does not
# give an error message when it encounters the flag.  If the flag has
# any effect or even a specific one is beyond the scope of this module.

include(CheckCXXSourceCompiles)
include(CMakeCheckCompilerFlagCommonPatterns)

macro (CHECK_CXX_COMPILER_FLAG _FLAG _RESULT)
   set(SAFE_CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
   set(CMAKE_REQUIRED_DEFINITIONS "${_FLAG}")

   # Normalize locale during test compilation.
   set(_CheckCXXCompilerFlag_LOCALE_VARS LC_ALL LC_MESSAGES LANG)
   foreach(v ${_CheckCXXCompilerFlag_LOCALE_VARS})
     set(_CheckCXXCompilerFlag_SAVED_${v} "$ENV{${v}}")
     set(ENV{${v}} C)
   endforeach()
   CHECK_COMPILER_FLAG_COMMON_PATTERNS(_CheckCXXCompilerFlag_COMMON_PATTERNS)
   CHECK_CXX_SOURCE_COMPILES("int main() { return 0; }" ${_RESULT}
     # Some compilers do not fail with a bad flag
     FAIL_REGEX "command line option .* is valid for .* but not for C\\\\+\\\\+" # GNU
     ${_CheckCXXCompilerFlag_COMMON_PATTERNS}
     )
   foreach(v ${_CheckCXXCompilerFlag_LOCALE_VARS})
     set(ENV{${v}} ${_CheckCXXCompilerFlag_SAVED_${v}})
     unset(_CheckCXXCompilerFlag_SAVED_${v})
   endforeach()
   unset(_CheckCXXCompilerFlag_LOCALE_VARS)
   unset(_CheckCXXCompilerFlag_COMMON_PATTERNS)

   set (CMAKE_REQUIRED_DEFINITIONS "${SAFE_CMAKE_REQUIRED_DEFINITIONS}")
endmacro ()

