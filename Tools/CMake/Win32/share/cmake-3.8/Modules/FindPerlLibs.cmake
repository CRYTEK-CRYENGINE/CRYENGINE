# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindPerlLibs
# ------------
#
# Find Perl libraries
#
# This module finds if PERL is installed and determines where the
# include files and libraries are.  It also determines what the name of
# the library is.  This code sets the following variables:
#
# ::
#
#   PERLLIBS_FOUND    = True if perl.h & libperl were found
#   PERL_INCLUDE_PATH = path to where perl.h is found
#   PERL_LIBRARY      = path to libperl
#   PERL_EXECUTABLE   = full path to the perl binary
#
#
#
# The minimum required version of Perl can be specified using the
# standard syntax, e.g.  find_package(PerlLibs 6.0)
#
# ::
#
#   The following variables are also available if needed
#   (introduced after CMake 2.6.4)
#
#
#
# ::
#
#   PERL_SITESEARCH    = path to the sitesearch install dir
#   PERL_SITELIB       = path to the sitelib install directory
#   PERL_VENDORARCH    = path to the vendor arch install directory
#   PERL_VENDORLIB     = path to the vendor lib install directory
#   PERL_ARCHLIB       = path to the arch lib install directory
#   PERL_PRIVLIB       = path to the priv lib install directory
#   PERL_EXTRA_C_FLAGS = Compilation flags used to build perl

# find the perl executable
include(${CMAKE_CURRENT_LIST_DIR}/FindPerl.cmake)

if (PERL_EXECUTABLE)
  ### PERL_PREFIX
  execute_process(
    COMMAND
      ${PERL_EXECUTABLE} -V:prefix
      OUTPUT_VARIABLE
        PERL_PREFIX_OUTPUT_VARIABLE
      RESULT_VARIABLE
        PERL_PREFIX_RESULT_VARIABLE
  )

  if (NOT PERL_PREFIX_RESULT_VARIABLE)
    string(REGEX REPLACE "prefix='([^']+)'.*" "\\1" PERL_PREFIX ${PERL_PREFIX_OUTPUT_VARIABLE})
  endif ()

  ### PERL_ARCHNAME
  execute_process(
    COMMAND
      ${PERL_EXECUTABLE} -V:archname
      OUTPUT_VARIABLE
        PERL_ARCHNAME_OUTPUT_VARIABLE
      RESULT_VARIABLE
        PERL_ARCHNAME_RESULT_VARIABLE
  )
  if (NOT PERL_ARCHNAME_RESULT_VARIABLE)
    string(REGEX REPLACE "archname='([^']+)'.*" "\\1" PERL_ARCHNAME ${PERL_ARCHNAME_OUTPUT_VARIABLE})
  endif ()



  ### PERL_EXTRA_C_FLAGS
  execute_process(
    COMMAND
      ${PERL_EXECUTABLE} -V:cppflags
    OUTPUT_VARIABLE
      PERL_CPPFLAGS_OUTPUT_VARIABLE
    RESULT_VARIABLE
      PERL_CPPFLAGS_RESULT_VARIABLE
    )
  if (NOT PERL_CPPFLAGS_RESULT_VARIABLE)
    string(REGEX REPLACE "cppflags='([^']+)'.*" "\\1" PERL_EXTRA_C_FLAGS ${PERL_CPPFLAGS_OUTPUT_VARIABLE})
  endif ()

  ### PERL_SITESEARCH
  execute_process(
    COMMAND
      ${PERL_EXECUTABLE} -V:installsitesearch
    OUTPUT_VARIABLE
      PERL_SITESEARCH_OUTPUT_VARIABLE
    RESULT_VARIABLE
      PERL_SITESEARCH_RESULT_VARIABLE
  )
  if (NOT PERL_SITESEARCH_RESULT_VARIABLE)
    string(REGEX REPLACE "install[a-z]+='([^']+)'.*" "\\1" PERL_SITESEARCH ${PERL_SITESEARCH_OUTPUT_VARIABLE})
    file(TO_CMAKE_PATH "${PERL_SITESEARCH}" PERL_SITESEARCH)
  endif ()

  ### PERL_SITELIB
  execute_process(
    COMMAND
      ${PERL_EXECUTABLE} -V:installsitelib
    OUTPUT_VARIABLE
      PERL_SITELIB_OUTPUT_VARIABLE
    RESULT_VARIABLE
      PERL_SITELIB_RESULT_VARIABLE
  )
  if (NOT PERL_SITELIB_RESULT_VARIABLE)
    string(REGEX REPLACE "install[a-z]+='([^']+)'.*" "\\1" PERL_SITELIB ${PERL_SITELIB_OUTPUT_VARIABLE})
    file(TO_CMAKE_PATH "${PERL_SITELIB}" PERL_SITELIB)
  endif ()

  ### PERL_VENDORARCH
  execute_process(
    COMMAND
      ${PERL_EXECUTABLE} -V:installvendorarch
    OUTPUT_VARIABLE
      PERL_VENDORARCH_OUTPUT_VARIABLE
    RESULT_VARIABLE
      PERL_VENDORARCH_RESULT_VARIABLE
    )
  if (NOT PERL_VENDORARCH_RESULT_VARIABLE)
    string(REGEX REPLACE "install[a-z]+='([^']+)'.*" "\\1" PERL_VENDORARCH ${PERL_VENDORARCH_OUTPUT_VARIABLE})
    file(TO_CMAKE_PATH "${PERL_VENDORARCH}" PERL_VENDORARCH)
  endif ()

  ### PERL_VENDORLIB
  execute_process(
    COMMAND
      ${PERL_EXECUTABLE} -V:installvendorlib
    OUTPUT_VARIABLE
      PERL_VENDORLIB_OUTPUT_VARIABLE
    RESULT_VARIABLE
      PERL_VENDORLIB_RESULT_VARIABLE
  )
  if (NOT PERL_VENDORLIB_RESULT_VARIABLE)
    string(REGEX REPLACE "install[a-z]+='([^']+)'.*" "\\1" PERL_VENDORLIB ${PERL_VENDORLIB_OUTPUT_VARIABLE})
    file(TO_CMAKE_PATH "${PERL_VENDORLIB}" PERL_VENDORLIB)
  endif ()

  macro(perl_adjust_darwin_lib_variable varname)
    string( TOUPPER PERL_${varname} FINDPERL_VARNAME )
    string( TOLOWER install${varname} PERL_VARNAME )

    if (NOT PERL_MINUSV_OUTPUT_VARIABLE)
      execute_process(
        COMMAND
        ${PERL_EXECUTABLE} -V
        OUTPUT_VARIABLE
        PERL_MINUSV_OUTPUT_VARIABLE
        RESULT_VARIABLE
        PERL_MINUSV_RESULT_VARIABLE
        )
    endif()

    if (NOT PERL_MINUSV_RESULT_VARIABLE)
      string(REGEX MATCH "(${PERL_VARNAME}.*points? to the Updates directory)"
        PERL_NEEDS_ADJUSTMENT ${PERL_MINUSV_OUTPUT_VARIABLE})

      if (PERL_NEEDS_ADJUSTMENT)
        string(REGEX REPLACE "(.*)/Updates/" "/System/\\1/" ${FINDPERL_VARNAME} ${${FINDPERL_VARNAME}})
      endif ()

    endif ()
  endmacro()

  ### PERL_ARCHLIB
  execute_process(
    COMMAND
      ${PERL_EXECUTABLE} -V:installarchlib
      OUTPUT_VARIABLE
        PERL_ARCHLIB_OUTPUT_VARIABLE
      RESULT_VARIABLE
        PERL_ARCHLIB_RESULT_VARIABLE
  )
  if (NOT PERL_ARCHLIB_RESULT_VARIABLE)
    string(REGEX REPLACE "install[a-z]+='([^']+)'.*" "\\1" PERL_ARCHLIB ${PERL_ARCHLIB_OUTPUT_VARIABLE})
    perl_adjust_darwin_lib_variable( ARCHLIB )
    file(TO_CMAKE_PATH "${PERL_ARCHLIB}" PERL_ARCHLIB)
  endif ()

  ### PERL_PRIVLIB
  execute_process(
    COMMAND
      ${PERL_EXECUTABLE} -V:installprivlib
    OUTPUT_VARIABLE
      PERL_PRIVLIB_OUTPUT_VARIABLE
    RESULT_VARIABLE
      PERL_PRIVLIB_RESULT_VARIABLE
  )
  if (NOT PERL_PRIVLIB_RESULT_VARIABLE)
    string(REGEX REPLACE "install[a-z]+='([^']+)'.*" "\\1" PERL_PRIVLIB ${PERL_PRIVLIB_OUTPUT_VARIABLE})
    perl_adjust_darwin_lib_variable( PRIVLIB )
    file(TO_CMAKE_PATH "${PERL_PRIVLIB}" PERL_PRIVLIB)
  endif ()

  ### PERL_POSSIBLE_LIBRARY_NAMES
  execute_process(
    COMMAND
      ${PERL_EXECUTABLE} -V:libperl
    OUTPUT_VARIABLE
      PERL_LIBRARY_OUTPUT_VARIABLE
    RESULT_VARIABLE
      PERL_LIBRARY_RESULT_VARIABLE
  )
  if (NOT PERL_LIBRARY_RESULT_VARIABLE)
    string(REGEX REPLACE "libperl='([^']+)'.*" "\\1" PERL_POSSIBLE_LIBRARY_NAMES ${PERL_LIBRARY_OUTPUT_VARIABLE})
  else ()
    set(PERL_POSSIBLE_LIBRARY_NAMES perl${PERL_VERSION_STRING} perl)
  endif ()

  ### PERL_INCLUDE_PATH
  find_path(PERL_INCLUDE_PATH
    NAMES
      perl.h
    PATHS
      ${PERL_ARCHLIB}/CORE
      /usr/lib/perl5/${PERL_VERSION_STRING}/${PERL_ARCHNAME}/CORE
      /usr/lib/perl/${PERL_VERSION_STRING}/${PERL_ARCHNAME}/CORE
      /usr/lib/perl5/${PERL_VERSION_STRING}/CORE
      /usr/lib/perl/${PERL_VERSION_STRING}/CORE
  )

  ### PERL_LIBRARY
  find_library(PERL_LIBRARY
    NAMES
      ${PERL_POSSIBLE_LIBRARY_NAMES}
    PATHS
      ${PERL_ARCHLIB}/CORE
      /usr/lib/perl5/${PERL_VERSION_STRING}/${PERL_ARCHNAME}/CORE
      /usr/lib/perl/${PERL_VERSION_STRING}/${PERL_ARCHNAME}/CORE
      /usr/lib/perl5/${PERL_VERSION_STRING}/CORE
      /usr/lib/perl/${PERL_VERSION_STRING}/CORE
  )

endif ()

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
find_package_handle_standard_args(PerlLibs REQUIRED_VARS PERL_LIBRARY PERL_INCLUDE_PATH
                                           VERSION_VAR PERL_VERSION_STRING)

# Introduced after CMake 2.6.4 to bring module into compliance
set(PERL_INCLUDE_DIR  ${PERL_INCLUDE_PATH})
set(PERL_INCLUDE_DIRS ${PERL_INCLUDE_PATH})
set(PERL_LIBRARIES    ${PERL_LIBRARY})
# For backward compatibility with CMake before 2.8.8
set(PERL_VERSION ${PERL_VERSION_STRING})

mark_as_advanced(
  PERL_INCLUDE_PATH
  PERL_LIBRARY
)
