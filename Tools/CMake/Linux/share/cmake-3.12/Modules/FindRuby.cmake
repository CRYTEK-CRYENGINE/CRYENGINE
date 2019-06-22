# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindRuby
# --------
#
# Find Ruby
#
# This module finds if Ruby is installed and determines where the
# include files and libraries are.  Ruby 1.8, 1.9, 2.0 and 2.1 are
# supported.
#
# The minimum required version of Ruby can be specified using the
# standard syntax, e.g.  find_package(Ruby 1.8)
#
# It also determines what the name of the library is.  This code sets
# the following variables:
#
# ``RUBY_EXECUTABLE``
#   full path to the ruby binary
# ``RUBY_INCLUDE_DIRS``
#   include dirs to be used when using the ruby library
# ``RUBY_LIBRARY``
#   full path to the ruby library
# ``RUBY_VERSION``
#   the version of ruby which was found, e.g. "1.8.7"
# ``RUBY_FOUND``
#   set to true if ruby ws found successfully
#
# Also:
#
# ``RUBY_INCLUDE_PATH``
#   same as RUBY_INCLUDE_DIRS, only provided for compatibility reasons, don't use it

#   RUBY_ARCHDIR=`$RUBY -r rbconfig -e 'printf("%s",Config::CONFIG@<:@"archdir"@:>@)'`
#   RUBY_SITEARCHDIR=`$RUBY -r rbconfig -e 'printf("%s",Config::CONFIG@<:@"sitearchdir"@:>@)'`
#   RUBY_SITEDIR=`$RUBY -r rbconfig -e 'printf("%s",Config::CONFIG@<:@"sitelibdir"@:>@)'`
#   RUBY_LIBDIR=`$RUBY -r rbconfig -e 'printf("%s",Config::CONFIG@<:@"libdir"@:>@)'`
#   RUBY_LIBRUBYARG=`$RUBY -r rbconfig -e 'printf("%s",Config::CONFIG@<:@"LIBRUBYARG_SHARED"@:>@)'`

# uncomment the following line to get debug output for this file
# set(_RUBY_DEBUG_OUTPUT TRUE)

# Determine the list of possible names of the ruby executable depending
# on which version of ruby is required
set(_RUBY_POSSIBLE_EXECUTABLE_NAMES ruby)

# if 1.9 is required, don't look for ruby18 and ruby1.8, default to version 1.8
if(DEFINED Ruby_FIND_VERSION_MAJOR AND DEFINED Ruby_FIND_VERSION_MINOR)
   set(Ruby_FIND_VERSION_SHORT_NODOT "${Ruby_FIND_VERSION_MAJOR}${RUBY_FIND_VERSION_MINOR}")
   # we can't construct that if only major version is given
   set(_RUBY_POSSIBLE_EXECUTABLE_NAMES
       ruby${Ruby_FIND_VERSION_MAJOR}.${Ruby_FIND_VERSION_MINOR}
       ruby${Ruby_FIND_VERSION_MAJOR}${Ruby_FIND_VERSION_MINOR}
       ${_RUBY_POSSIBLE_EXECUTABLE_NAMES})
else()
   set(Ruby_FIND_VERSION_SHORT_NODOT "18")
endif()

if(NOT Ruby_FIND_VERSION_EXACT)
  list(APPEND _RUBY_POSSIBLE_EXECUTABLE_NAMES ruby2.4 ruby24)
  list(APPEND _RUBY_POSSIBLE_EXECUTABLE_NAMES ruby2.3 ruby23)
  list(APPEND _RUBY_POSSIBLE_EXECUTABLE_NAMES ruby2.2 ruby22)
  list(APPEND _RUBY_POSSIBLE_EXECUTABLE_NAMES ruby2.1 ruby21)
  list(APPEND _RUBY_POSSIBLE_EXECUTABLE_NAMES ruby2.0 ruby20)
  list(APPEND _RUBY_POSSIBLE_EXECUTABLE_NAMES ruby1.9 ruby19)

  # if we want a version below 1.9, also look for ruby 1.8
  if("${Ruby_FIND_VERSION_SHORT_NODOT}" VERSION_LESS "19")
    list(APPEND _RUBY_POSSIBLE_EXECUTABLE_NAMES ruby1.8 ruby18)
  endif()

  list(REMOVE_DUPLICATES _RUBY_POSSIBLE_EXECUTABLE_NAMES)
endif()

find_program(RUBY_EXECUTABLE NAMES ${_RUBY_POSSIBLE_EXECUTABLE_NAMES})

if(RUBY_EXECUTABLE  AND NOT  RUBY_VERSION_MAJOR)
  function(_RUBY_CONFIG_VAR RBVAR OUTVAR)
    execute_process(COMMAND ${RUBY_EXECUTABLE} -r rbconfig -e "print RbConfig::CONFIG['${RBVAR}']"
      RESULT_VARIABLE _RUBY_SUCCESS
      OUTPUT_VARIABLE _RUBY_OUTPUT
      ERROR_QUIET)
    if(_RUBY_SUCCESS OR _RUBY_OUTPUT STREQUAL "")
      execute_process(COMMAND ${RUBY_EXECUTABLE} -r rbconfig -e "print Config::CONFIG['${RBVAR}']"
        RESULT_VARIABLE _RUBY_SUCCESS
        OUTPUT_VARIABLE _RUBY_OUTPUT
        ERROR_QUIET)
    endif()
    set(${OUTVAR} "${_RUBY_OUTPUT}" PARENT_SCOPE)
  endfunction()


  # query the ruby version
   _RUBY_CONFIG_VAR("MAJOR" RUBY_VERSION_MAJOR)
   _RUBY_CONFIG_VAR("MINOR" RUBY_VERSION_MINOR)
   _RUBY_CONFIG_VAR("TEENY" RUBY_VERSION_PATCH)

   # query the different directories
   _RUBY_CONFIG_VAR("archdir" RUBY_ARCH_DIR)
   _RUBY_CONFIG_VAR("arch" RUBY_ARCH)
   _RUBY_CONFIG_VAR("rubyhdrdir" RUBY_HDR_DIR)
   _RUBY_CONFIG_VAR("rubyarchhdrdir" RUBY_ARCHHDR_DIR)
   _RUBY_CONFIG_VAR("libdir" RUBY_POSSIBLE_LIB_DIR)
   _RUBY_CONFIG_VAR("rubylibdir" RUBY_RUBY_LIB_DIR)

   # site_ruby
   _RUBY_CONFIG_VAR("sitearchdir" RUBY_SITEARCH_DIR)
   _RUBY_CONFIG_VAR("sitelibdir" RUBY_SITELIB_DIR)

   # vendor_ruby available ?
   execute_process(COMMAND ${RUBY_EXECUTABLE} -r vendor-specific -e "print 'true'"
      OUTPUT_VARIABLE RUBY_HAS_VENDOR_RUBY  ERROR_QUIET)

   if(RUBY_HAS_VENDOR_RUBY)
      _RUBY_CONFIG_VAR("vendorlibdir" RUBY_VENDORLIB_DIR)
      _RUBY_CONFIG_VAR("vendorarchdir" RUBY_VENDORARCH_DIR)
   endif()

   # save the results in the cache so we don't have to run ruby the next time again
   set(RUBY_VERSION_MAJOR    ${RUBY_VERSION_MAJOR}    CACHE PATH "The Ruby major version" FORCE)
   set(RUBY_VERSION_MINOR    ${RUBY_VERSION_MINOR}    CACHE PATH "The Ruby minor version" FORCE)
   set(RUBY_VERSION_PATCH    ${RUBY_VERSION_PATCH}    CACHE PATH "The Ruby patch version" FORCE)
   set(RUBY_ARCH_DIR         ${RUBY_ARCH_DIR}         CACHE PATH "The Ruby arch dir" FORCE)
   set(RUBY_HDR_DIR          ${RUBY_HDR_DIR}          CACHE PATH "The Ruby header dir (1.9+)" FORCE)
   set(RUBY_ARCHHDR_DIR      ${RUBY_ARCHHDR_DIR}      CACHE PATH "The Ruby arch header dir (2.0+)" FORCE)
   set(RUBY_POSSIBLE_LIB_DIR ${RUBY_POSSIBLE_LIB_DIR} CACHE PATH "The Ruby lib dir" FORCE)
   set(RUBY_RUBY_LIB_DIR     ${RUBY_RUBY_LIB_DIR}     CACHE PATH "The Ruby ruby-lib dir" FORCE)
   set(RUBY_SITEARCH_DIR     ${RUBY_SITEARCH_DIR}     CACHE PATH "The Ruby site arch dir" FORCE)
   set(RUBY_SITELIB_DIR      ${RUBY_SITELIB_DIR}      CACHE PATH "The Ruby site lib dir" FORCE)
   set(RUBY_HAS_VENDOR_RUBY  ${RUBY_HAS_VENDOR_RUBY}  CACHE BOOL "Vendor Ruby is available" FORCE)
   set(RUBY_VENDORARCH_DIR   ${RUBY_VENDORARCH_DIR}   CACHE PATH "The Ruby vendor arch dir" FORCE)
   set(RUBY_VENDORLIB_DIR    ${RUBY_VENDORLIB_DIR}    CACHE PATH "The Ruby vendor lib dir" FORCE)

   mark_as_advanced(
     RUBY_ARCH_DIR
     RUBY_ARCH
     RUBY_HDR_DIR
     RUBY_ARCHHDR_DIR
     RUBY_POSSIBLE_LIB_DIR
     RUBY_RUBY_LIB_DIR
     RUBY_SITEARCH_DIR
     RUBY_SITELIB_DIR
     RUBY_HAS_VENDOR_RUBY
     RUBY_VENDORARCH_DIR
     RUBY_VENDORLIB_DIR
     RUBY_VERSION_MAJOR
     RUBY_VERSION_MINOR
     RUBY_VERSION_PATCH
     )
endif()

# In case RUBY_EXECUTABLE could not be executed (e.g. cross compiling)
# try to detect which version we found. This is not too good.
if(RUBY_EXECUTABLE AND NOT RUBY_VERSION_MAJOR)
   # by default assume 1.8.0
   set(RUBY_VERSION_MAJOR 1)
   set(RUBY_VERSION_MINOR 8)
   set(RUBY_VERSION_PATCH 0)
   # check whether we found 1.9.x
   if(${RUBY_EXECUTABLE} MATCHES "ruby1\\.?9")
      set(RUBY_VERSION_MAJOR 1)
      set(RUBY_VERSION_MINOR 9)
   endif()
   # check whether we found 2.0.x
   if(${RUBY_EXECUTABLE} MATCHES "ruby2\\.?0")
      set(RUBY_VERSION_MAJOR 2)
      set(RUBY_VERSION_MINOR 0)
   endif()
   # check whether we found 2.1.x
   if(${RUBY_EXECUTABLE} MATCHES "ruby2\\.?1")
      set(RUBY_VERSION_MAJOR 2)
      set(RUBY_VERSION_MINOR 1)
   endif()
   # check whether we found 2.2.x
   if(${RUBY_EXECUTABLE} MATCHES "ruby2\\.?2")
      set(RUBY_VERSION_MAJOR 2)
      set(RUBY_VERSION_MINOR 2)
   endif()
   # check whether we found 2.3.x
   if(${RUBY_EXECUTABLE} MATCHES "ruby2\\.?3")
      set(RUBY_VERSION_MAJOR 2)
      set(RUBY_VERSION_MINOR 3)
   endif()
   # check whether we found 2.4.x
   if(${RUBY_EXECUTABLE} MATCHES "ruby2\\.?4")
      set(RUBY_VERSION_MAJOR 2)
      set(RUBY_VERSION_MINOR 4)
   endif()
endif()

if(RUBY_VERSION_MAJOR)
   set(RUBY_VERSION "${RUBY_VERSION_MAJOR}.${RUBY_VERSION_MINOR}.${RUBY_VERSION_PATCH}")
   set(_RUBY_VERSION_SHORT "${RUBY_VERSION_MAJOR}.${RUBY_VERSION_MINOR}")
   set(_RUBY_VERSION_SHORT_NODOT "${RUBY_VERSION_MAJOR}${RUBY_VERSION_MINOR}")
   set(_RUBY_NODOT_VERSION "${RUBY_VERSION_MAJOR}${RUBY_VERSION_MINOR}${RUBY_VERSION_PATCH}")
endif()

find_path(RUBY_INCLUDE_DIR
   NAMES ruby.h
   HINTS
   ${RUBY_HDR_DIR}
   ${RUBY_ARCH_DIR}
   /usr/lib/ruby/${_RUBY_VERSION_SHORT}/i586-linux-gnu/ )

set(RUBY_INCLUDE_DIRS ${RUBY_INCLUDE_DIR} )

# if ruby > 1.8 is required or if ruby > 1.8 was found, search for the config.h dir
if( "${Ruby_FIND_VERSION_SHORT_NODOT}" GREATER 18  OR  "${_RUBY_VERSION_SHORT_NODOT}" GREATER 18  OR  RUBY_HDR_DIR)
   find_path(RUBY_CONFIG_INCLUDE_DIR
     NAMES ruby/config.h  config.h
     HINTS
     ${RUBY_HDR_DIR}/${RUBY_ARCH}
     ${RUBY_ARCH_DIR}
     ${RUBY_ARCHHDR_DIR}
     )

   set(RUBY_INCLUDE_DIRS ${RUBY_INCLUDE_DIRS} ${RUBY_CONFIG_INCLUDE_DIR} )
endif()


# Determine the list of possible names for the ruby library
set(_RUBY_POSSIBLE_LIB_NAMES ruby ruby-static ruby${_RUBY_VERSION_SHORT} ruby${_RUBY_VERSION_SHORT_NODOT} ruby-${_RUBY_VERSION_SHORT} ruby-${RUBY_VERSION})

if(WIN32)
   set( _RUBY_MSVC_RUNTIME "" )
   if( MSVC_VERSION EQUAL 1200 )
     set( _RUBY_MSVC_RUNTIME "60" )
   endif()
   if( MSVC_VERSION EQUAL 1300 )
     set( _RUBY_MSVC_RUNTIME "70" )
   endif()
   if( MSVC_VERSION EQUAL 1310 )
     set( _RUBY_MSVC_RUNTIME "71" )
   endif()
   if( MSVC_VERSION EQUAL 1400 )
     set( _RUBY_MSVC_RUNTIME "80" )
   endif()
   if( MSVC_VERSION EQUAL 1500 )
     set( _RUBY_MSVC_RUNTIME "90" )
   endif()

   set(_RUBY_ARCH_PREFIX "")
   if(CMAKE_SIZEOF_VOID_P EQUAL 8)
     set(_RUBY_ARCH_PREFIX "x64-")
   endif()

   list(APPEND _RUBY_POSSIBLE_LIB_NAMES
               "${_RUBY_ARCH_PREFIX}msvcr${_RUBY_MSVC_RUNTIME}-ruby${_RUBY_NODOT_VERSION}"
               "${_RUBY_ARCH_PREFIX}msvcr${_RUBY_MSVC_RUNTIME}-ruby${_RUBY_NODOT_VERSION}-static"
               "${_RUBY_ARCH_PREFIX}msvcrt-ruby${_RUBY_NODOT_VERSION}"
               "${_RUBY_ARCH_PREFIX}msvcrt-ruby${_RUBY_NODOT_VERSION}-static" )
endif()

find_library(RUBY_LIBRARY NAMES ${_RUBY_POSSIBLE_LIB_NAMES} HINTS ${RUBY_POSSIBLE_LIB_DIR} )

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
set(_RUBY_REQUIRED_VARS RUBY_EXECUTABLE RUBY_INCLUDE_DIR RUBY_LIBRARY)
if(_RUBY_VERSION_SHORT_NODOT GREATER 18)
   list(APPEND _RUBY_REQUIRED_VARS RUBY_CONFIG_INCLUDE_DIR)
endif()

if(_RUBY_DEBUG_OUTPUT)
   message(STATUS "--------FindRuby.cmake debug------------")
   message(STATUS "_RUBY_POSSIBLE_EXECUTABLE_NAMES: ${_RUBY_POSSIBLE_EXECUTABLE_NAMES}")
   message(STATUS "_RUBY_POSSIBLE_LIB_NAMES: ${_RUBY_POSSIBLE_LIB_NAMES}")
   message(STATUS "RUBY_ARCH_DIR: ${RUBY_ARCH_DIR}")
   message(STATUS "RUBY_HDR_DIR: ${RUBY_HDR_DIR}")
   message(STATUS "RUBY_POSSIBLE_LIB_DIR: ${RUBY_POSSIBLE_LIB_DIR}")
   message(STATUS "Found RUBY_VERSION: \"${RUBY_VERSION}\" , short: \"${_RUBY_VERSION_SHORT}\", nodot: \"${_RUBY_VERSION_SHORT_NODOT}\"")
   message(STATUS "_RUBY_REQUIRED_VARS: ${_RUBY_REQUIRED_VARS}")
   message(STATUS "RUBY_EXECUTABLE: ${RUBY_EXECUTABLE}")
   message(STATUS "RUBY_LIBRARY: ${RUBY_LIBRARY}")
   message(STATUS "RUBY_INCLUDE_DIR: ${RUBY_INCLUDE_DIR}")
   message(STATUS "RUBY_CONFIG_INCLUDE_DIR: ${RUBY_CONFIG_INCLUDE_DIR}")
   message(STATUS "--------------------")
endif()

FIND_PACKAGE_HANDLE_STANDARD_ARGS(Ruby  REQUIRED_VARS  ${_RUBY_REQUIRED_VARS}
                                        VERSION_VAR RUBY_VERSION )

mark_as_advanced(
  RUBY_EXECUTABLE
  RUBY_LIBRARY
  RUBY_INCLUDE_DIR
  RUBY_CONFIG_INCLUDE_DIR
  )

# Set some variables for compatibility with previous version of this file
set(RUBY_POSSIBLE_LIB_PATH ${RUBY_POSSIBLE_LIB_DIR})
set(RUBY_RUBY_LIB_PATH ${RUBY_RUBY_LIB_DIR})
set(RUBY_INCLUDE_PATH ${RUBY_INCLUDE_DIRS})
