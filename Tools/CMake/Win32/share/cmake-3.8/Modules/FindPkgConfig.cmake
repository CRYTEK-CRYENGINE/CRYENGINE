# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindPkgConfig
# -------------
#
# A `pkg-config` module for CMake.
#
# Finds the ``pkg-config`` executable and add the
# :command:`pkg_check_modules` and :command:`pkg_search_module`
# commands.
#
# In order to find the ``pkg-config`` executable, it uses the
# :variable:`PKG_CONFIG_EXECUTABLE` variable or the ``PKG_CONFIG``
# environment variable first.

### Common stuff ####
set(PKG_CONFIG_VERSION 1)

# find pkg-config, use PKG_CONFIG if set
if((NOT PKG_CONFIG_EXECUTABLE) AND (NOT "$ENV{PKG_CONFIG}" STREQUAL ""))
  set(PKG_CONFIG_EXECUTABLE "$ENV{PKG_CONFIG}" CACHE FILEPATH "pkg-config executable")
endif()
find_program(PKG_CONFIG_EXECUTABLE NAMES pkg-config DOC "pkg-config executable")
mark_as_advanced(PKG_CONFIG_EXECUTABLE)

if (PKG_CONFIG_EXECUTABLE)
  execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --version
    OUTPUT_VARIABLE PKG_CONFIG_VERSION_STRING
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
endif ()

include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
find_package_handle_standard_args(PkgConfig
                                  REQUIRED_VARS PKG_CONFIG_EXECUTABLE
                                  VERSION_VAR PKG_CONFIG_VERSION_STRING)

# This is needed because the module name is "PkgConfig" but the name of
# this variable has always been PKG_CONFIG_FOUND so this isn't automatically
# handled by FPHSA.
set(PKG_CONFIG_FOUND "${PKGCONFIG_FOUND}")

# Unsets the given variables
macro(_pkgconfig_unset var)
  set(${var} "" CACHE INTERNAL "")
endmacro()

macro(_pkgconfig_set var value)
  set(${var} ${value} CACHE INTERNAL "")
endmacro()

# Invokes pkgconfig, cleans up the result and sets variables
macro(_pkgconfig_invoke _pkglist _prefix _varname _regexp)
  set(_pkgconfig_invoke_result)

  execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} ${ARGN} ${_pkglist}
    OUTPUT_VARIABLE _pkgconfig_invoke_result
    RESULT_VARIABLE _pkgconfig_failed
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if (_pkgconfig_failed)
    set(_pkgconfig_${_varname} "")
    _pkgconfig_unset(${_prefix}_${_varname})
  else()
    string(REGEX REPLACE "[\r\n]"                  " " _pkgconfig_invoke_result "${_pkgconfig_invoke_result}")

    if (NOT ${_regexp} STREQUAL "")
      string(REGEX REPLACE "${_regexp}" " " _pkgconfig_invoke_result "${_pkgconfig_invoke_result}")
    endif()

    separate_arguments(_pkgconfig_invoke_result)

    #message(STATUS "  ${_varname} ... ${_pkgconfig_invoke_result}")
    set(_pkgconfig_${_varname} ${_pkgconfig_invoke_result})
    _pkgconfig_set(${_prefix}_${_varname} "${_pkgconfig_invoke_result}")
  endif()
endmacro()

#[========================================[.rst:
.. command:: pkg_get_variable

  Retrieves the value of a variable from a package::

    pkg_get_variable(<RESULT> <MODULE> <VARIABLE>)

  For example:

  .. code-block:: cmake

    pkg_get_variable(GI_GIRDIR gobject-introspection-1.0 girdir)
#]========================================]
function (pkg_get_variable result pkg variable)
  _pkgconfig_invoke("${pkg}" "prefix" "result" "" "--variable=${variable}")
  set("${result}"
    "${prefix_result}"
    PARENT_SCOPE)
endfunction ()

# Invokes pkgconfig two times; once without '--static' and once with
# '--static'
macro(_pkgconfig_invoke_dyn _pkglist _prefix _varname cleanup_regexp)
  _pkgconfig_invoke("${_pkglist}" ${_prefix}        ${_varname} "${cleanup_regexp}" ${ARGN})
  _pkgconfig_invoke("${_pkglist}" ${_prefix} STATIC_${_varname} "${cleanup_regexp}" --static  ${ARGN})
endmacro()

# Splits given arguments into options and a package list
macro(_pkgconfig_parse_options _result _is_req _is_silent _no_cmake_path _no_cmake_environment_path _imp_target)
  set(${_is_req} 0)
  set(${_is_silent} 0)
  set(${_no_cmake_path} 0)
  set(${_no_cmake_environment_path} 0)
  set(${_imp_target} 0)
  if(DEFINED PKG_CONFIG_USE_CMAKE_PREFIX_PATH)
    if(NOT PKG_CONFIG_USE_CMAKE_PREFIX_PATH)
      set(${_no_cmake_path} 1)
      set(${_no_cmake_environment_path} 1)
    endif()
  elseif(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 3.1)
    set(${_no_cmake_path} 1)
    set(${_no_cmake_environment_path} 1)
  endif()

  foreach(_pkg ${ARGN})
    if (_pkg STREQUAL "REQUIRED")
      set(${_is_req} 1)
    endif ()
    if (_pkg STREQUAL "QUIET")
      set(${_is_silent} 1)
    endif ()
    if (_pkg STREQUAL "NO_CMAKE_PATH")
      set(${_no_cmake_path} 1)
    endif()
    if (_pkg STREQUAL "NO_CMAKE_ENVIRONMENT_PATH")
      set(${_no_cmake_environment_path} 1)
    endif()
    if (_pkg STREQUAL "IMPORTED_TARGET")
      set(${_imp_target} 1)
    endif()
  endforeach()

  set(${_result} ${ARGN})
  list(REMOVE_ITEM ${_result} "REQUIRED")
  list(REMOVE_ITEM ${_result} "QUIET")
  list(REMOVE_ITEM ${_result} "NO_CMAKE_PATH")
  list(REMOVE_ITEM ${_result} "NO_CMAKE_ENVIRONMENT_PATH")
  list(REMOVE_ITEM ${_result} "IMPORTED_TARGET")
endmacro()

# Add the content of a variable or an environment variable to a list of
# paths
# Usage:
#  - _pkgconfig_add_extra_path(_extra_paths VAR)
#  - _pkgconfig_add_extra_path(_extra_paths ENV VAR)
function(_pkgconfig_add_extra_path _extra_paths_var _var)
  set(_is_env 0)
  if(ARGC GREATER 2 AND _var STREQUAL "ENV")
    set(_var ${ARGV2})
    set(_is_env 1)
  endif()
  if(NOT _is_env)
    if(NOT "${${_var}}" STREQUAL "")
      list(APPEND ${_extra_paths_var} ${${_var}})
    endif()
  else()
    if(NOT "$ENV{${_var}}" STREQUAL "")
      file(TO_CMAKE_PATH "$ENV{${_var}}" _path)
      list(APPEND ${_extra_paths_var} ${_path})
      unset(_path)
    endif()
  endif()
  set(${_extra_paths_var} ${${_extra_paths_var}} PARENT_SCOPE)
endfunction()

# scan the LDFLAGS returned by pkg-config for library directories and
# libraries, figure out the absolute paths of that libraries in the
# given directories, and create an imported target from them
function(_pkg_create_imp_target _prefix _no_cmake_path _no_cmake_environment_path)
  unset(_libs)
  unset(_find_opts)

  # set the options that are used as long as the .pc file does not provide a library
  # path to look into
  if(_no_cmake_path)
    set(_find_opts "NO_CMAKE_PATH")
  endif()
  if(_no_cmake_environment_path)
    string(APPEND _find_opts " NO_CMAKE_ENVIRONMENT_PATH")
  endif()

  foreach (flag IN LISTS ${_prefix}_LDFLAGS)
    if (flag MATCHES "^-L(.*)")
      # only look into the given paths from now on
      set(_find_opts HINTS ${CMAKE_MATCH_1} NO_DEFAULT_PATH)
      continue()
    endif()
    if (flag MATCHES "^-l(.*)")
      set(_pkg_search "${CMAKE_MATCH_1}")
    else()
      continue()
    endif()

    find_library(pkgcfg_lib_${_prefix}_${_pkg_search}
                 NAMES ${_pkg_search}
                 ${_find_opts})
    list(APPEND _libs "${pkgcfg_lib_${_prefix}_${_pkg_search}}")
  endforeach()

  # only create the target if it is linkable, i.e. no executables
  if (NOT TARGET PkgConfig::${_prefix}
      AND ( ${_prefix}_INCLUDE_DIRS OR _libs OR ${_prefix}_CFLAGS_OTHER ))
    add_library(PkgConfig::${_prefix} INTERFACE IMPORTED)

    unset(_props)
    if(${_prefix}_INCLUDE_DIRS)
      set_property(TARGET PkgConfig::${_prefix} PROPERTY
                   INTERFACE_INCLUDE_DIRECTORIES "${${_prefix}_INCLUDE_DIRS}")
    endif()
    if(_libs)
      set_property(TARGET PkgConfig::${_prefix} PROPERTY
                   INTERFACE_LINK_LIBRARIES "${_libs}")
    endif()
    if(${_prefix}_CFLAGS_OTHER)
      set_property(TARGET PkgConfig::${_prefix} PROPERTY
                   INTERFACE_COMPILE_OPTIONS "${${_prefix}_CFLAGS_OTHER}")
    endif()
  endif()
endfunction()

###
macro(_pkg_check_modules_internal _is_required _is_silent _no_cmake_path _no_cmake_environment_path _imp_target _prefix)
  _pkgconfig_unset(${_prefix}_FOUND)
  _pkgconfig_unset(${_prefix}_VERSION)
  _pkgconfig_unset(${_prefix}_PREFIX)
  _pkgconfig_unset(${_prefix}_INCLUDEDIR)
  _pkgconfig_unset(${_prefix}_LIBDIR)
  _pkgconfig_unset(${_prefix}_LIBS)
  _pkgconfig_unset(${_prefix}_LIBS_L)
  _pkgconfig_unset(${_prefix}_LIBS_PATHS)
  _pkgconfig_unset(${_prefix}_LIBS_OTHER)
  _pkgconfig_unset(${_prefix}_CFLAGS)
  _pkgconfig_unset(${_prefix}_CFLAGS_I)
  _pkgconfig_unset(${_prefix}_CFLAGS_OTHER)
  _pkgconfig_unset(${_prefix}_STATIC_LIBDIR)
  _pkgconfig_unset(${_prefix}_STATIC_LIBS)
  _pkgconfig_unset(${_prefix}_STATIC_LIBS_L)
  _pkgconfig_unset(${_prefix}_STATIC_LIBS_PATHS)
  _pkgconfig_unset(${_prefix}_STATIC_LIBS_OTHER)
  _pkgconfig_unset(${_prefix}_STATIC_CFLAGS)
  _pkgconfig_unset(${_prefix}_STATIC_CFLAGS_I)
  _pkgconfig_unset(${_prefix}_STATIC_CFLAGS_OTHER)

  # create a better addressable variable of the modules and calculate its size
  set(_pkg_check_modules_list ${ARGN})
  list(LENGTH _pkg_check_modules_list _pkg_check_modules_cnt)

  if(PKG_CONFIG_EXECUTABLE)
    # give out status message telling checked module
    if (NOT ${_is_silent})
      if (_pkg_check_modules_cnt EQUAL 1)
        message(STATUS "Checking for module '${_pkg_check_modules_list}'")
      else()
        message(STATUS "Checking for modules '${_pkg_check_modules_list}'")
      endif()
    endif()

    set(_pkg_check_modules_packages)
    set(_pkg_check_modules_failed)

    set(_extra_paths)

    if(NOT _no_cmake_path)
      _pkgconfig_add_extra_path(_extra_paths CMAKE_PREFIX_PATH)
      _pkgconfig_add_extra_path(_extra_paths CMAKE_FRAMEWORK_PATH)
      _pkgconfig_add_extra_path(_extra_paths CMAKE_APPBUNDLE_PATH)
    endif()

    if(NOT _no_cmake_environment_path)
      _pkgconfig_add_extra_path(_extra_paths ENV CMAKE_PREFIX_PATH)
      _pkgconfig_add_extra_path(_extra_paths ENV CMAKE_FRAMEWORK_PATH)
      _pkgconfig_add_extra_path(_extra_paths ENV CMAKE_APPBUNDLE_PATH)
    endif()

    if(NOT "${_extra_paths}" STREQUAL "")
      # Save the PKG_CONFIG_PATH environment variable, and add paths
      # from the CMAKE_PREFIX_PATH variables
      set(_pkgconfig_path_old "$ENV{PKG_CONFIG_PATH}")
      set(_pkgconfig_path "${_pkgconfig_path_old}")
      if(NOT "${_pkgconfig_path}" STREQUAL "")
        file(TO_CMAKE_PATH "${_pkgconfig_path}" _pkgconfig_path)
      endif()

      # Create a list of the possible pkgconfig subfolder (depending on
      # the system
      set(_lib_dirs)
      if(NOT DEFINED CMAKE_SYSTEM_NAME
          OR (CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU)$"
              AND NOT CMAKE_CROSSCOMPILING))
        if(EXISTS "/etc/debian_version") # is this a debian system ?
          if(CMAKE_LIBRARY_ARCHITECTURE)
            list(APPEND _lib_dirs "lib/${CMAKE_LIBRARY_ARCHITECTURE}/pkgconfig")
          endif()
        else()
          # not debian, check the FIND_LIBRARY_USE_LIB32_PATHS and FIND_LIBRARY_USE_LIB64_PATHS properties
          get_property(uselib32 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB32_PATHS)
          if(uselib32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
            list(APPEND _lib_dirs "lib32/pkgconfig")
          endif()
          get_property(uselib64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
          if(uselib64 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
            list(APPEND _lib_dirs "lib64/pkgconfig")
          endif()
        endif()
      endif()
      list(APPEND _lib_dirs "lib/pkgconfig")
      list(APPEND _lib_dirs "share/pkgconfig")

      # Check if directories exist and eventually append them to the
      # pkgconfig path list
      foreach(_prefix_dir ${_extra_paths})
        foreach(_lib_dir ${_lib_dirs})
          if(EXISTS "${_prefix_dir}/${_lib_dir}")
            list(APPEND _pkgconfig_path "${_prefix_dir}/${_lib_dir}")
            list(REMOVE_DUPLICATES _pkgconfig_path)
          endif()
        endforeach()
      endforeach()

      # Prepare and set the environment variable
      if(NOT "${_pkgconfig_path}" STREQUAL "")
        # remove empty values from the list
        list(REMOVE_ITEM _pkgconfig_path "")
        file(TO_NATIVE_PATH "${_pkgconfig_path}" _pkgconfig_path)
        if(UNIX)
          string(REPLACE ";" ":" _pkgconfig_path "${_pkgconfig_path}")
          string(REPLACE "\\ " " " _pkgconfig_path "${_pkgconfig_path}")
        endif()
        set(ENV{PKG_CONFIG_PATH} "${_pkgconfig_path}")
      endif()

      # Unset variables
      unset(_lib_dirs)
      unset(_pkgconfig_path)
    endif()

    # iterate through module list and check whether they exist and match the required version
    foreach (_pkg_check_modules_pkg ${_pkg_check_modules_list})
      set(_pkg_check_modules_exist_query)

      # check whether version is given
      if (_pkg_check_modules_pkg MATCHES "(.*[^><])(>=|=|<=)(.*)")
        set(_pkg_check_modules_pkg_name "${CMAKE_MATCH_1}")
        set(_pkg_check_modules_pkg_op "${CMAKE_MATCH_2}")
        set(_pkg_check_modules_pkg_ver "${CMAKE_MATCH_3}")
      else()
        set(_pkg_check_modules_pkg_name "${_pkg_check_modules_pkg}")
        set(_pkg_check_modules_pkg_op)
        set(_pkg_check_modules_pkg_ver)
      endif()

      # handle the operands
      if (_pkg_check_modules_pkg_op STREQUAL ">=")
        list(APPEND _pkg_check_modules_exist_query --atleast-version)
      endif()

      if (_pkg_check_modules_pkg_op STREQUAL "=")
        list(APPEND _pkg_check_modules_exist_query --exact-version)
      endif()

      if (_pkg_check_modules_pkg_op STREQUAL "<=")
        list(APPEND _pkg_check_modules_exist_query --max-version)
      endif()

      # create the final query which is of the format:
      # * --atleast-version <version> <pkg-name>
      # * --exact-version <version> <pkg-name>
      # * --max-version <version> <pkg-name>
      # * --exists <pkg-name>
      if (_pkg_check_modules_pkg_op)
        list(APPEND _pkg_check_modules_exist_query "${_pkg_check_modules_pkg_ver}")
      else()
        list(APPEND _pkg_check_modules_exist_query --exists)
      endif()
      list(APPEND _pkg_check_modules_exist_query --print-errors --short-errors)

      _pkgconfig_unset(${_prefix}_${_pkg_check_modules_pkg_name}_VERSION)
      _pkgconfig_unset(${_prefix}_${_pkg_check_modules_pkg_name}_PREFIX)
      _pkgconfig_unset(${_prefix}_${_pkg_check_modules_pkg_name}_INCLUDEDIR)
      _pkgconfig_unset(${_prefix}_${_pkg_check_modules_pkg_name}_LIBDIR)

      list(APPEND _pkg_check_modules_exist_query "${_pkg_check_modules_pkg_name}")
      list(APPEND _pkg_check_modules_packages    "${_pkg_check_modules_pkg_name}")

      # execute the query
      execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} ${_pkg_check_modules_exist_query}
        RESULT_VARIABLE _pkgconfig_retval
        ERROR_VARIABLE _pkgconfig_error
        ERROR_STRIP_TRAILING_WHITESPACE)

      # evaluate result and tell failures
      if (_pkgconfig_retval)
        if(NOT ${_is_silent})
          message(STATUS "  ${_pkgconfig_error}")
        endif()

        set(_pkg_check_modules_failed 1)
      endif()
    endforeach()

    if(_pkg_check_modules_failed)
      # fail when requested
      if (${_is_required})
        message(FATAL_ERROR "A required package was not found")
      endif ()
    else()
      # when we are here, we checked whether requested modules
      # exist. Now, go through them and set variables

      _pkgconfig_set(${_prefix}_FOUND 1)
      list(LENGTH _pkg_check_modules_packages pkg_count)

      # iterate through all modules again and set individual variables
      foreach (_pkg_check_modules_pkg ${_pkg_check_modules_packages})
        # handle case when there is only one package required
        if (pkg_count EQUAL 1)
          set(_pkg_check_prefix "${_prefix}")
        else()
          set(_pkg_check_prefix "${_prefix}_${_pkg_check_modules_pkg}")
        endif()

        _pkgconfig_invoke(${_pkg_check_modules_pkg} "${_pkg_check_prefix}" VERSION    ""   --modversion )
        pkg_get_variable("${_pkg_check_prefix}_PREFIX" ${_pkg_check_modules_pkg} "prefix")
        pkg_get_variable("${_pkg_check_prefix}_INCLUDEDIR" ${_pkg_check_modules_pkg} "includedir")
        pkg_get_variable("${_pkg_check_prefix}_LIBDIR" ${_pkg_check_modules_pkg} "libdir")
        foreach (variable IN ITEMS PREFIX INCLUDEDIR LIBDIR)
          _pkgconfig_set("${_pkg_check_prefix}_${variable}" "${${_pkg_check_prefix}_${variable}}")
        endforeach ()

        if (NOT ${_is_silent})
          message(STATUS "  Found ${_pkg_check_modules_pkg}, version ${_pkgconfig_VERSION}")
        endif ()
      endforeach()

      # set variables which are combined for multiple modules
      _pkgconfig_invoke_dyn("${_pkg_check_modules_packages}" "${_prefix}" LIBRARIES           "(^| )-l" --libs-only-l )
      _pkgconfig_invoke_dyn("${_pkg_check_modules_packages}" "${_prefix}" LIBRARY_DIRS        "(^| )-L" --libs-only-L )
      _pkgconfig_invoke_dyn("${_pkg_check_modules_packages}" "${_prefix}" LDFLAGS             ""        --libs )
      _pkgconfig_invoke_dyn("${_pkg_check_modules_packages}" "${_prefix}" LDFLAGS_OTHER       ""        --libs-only-other )

      _pkgconfig_invoke_dyn("${_pkg_check_modules_packages}" "${_prefix}" INCLUDE_DIRS        "(^| )-I" --cflags-only-I )
      _pkgconfig_invoke_dyn("${_pkg_check_modules_packages}" "${_prefix}" CFLAGS              ""        --cflags )
      _pkgconfig_invoke_dyn("${_pkg_check_modules_packages}" "${_prefix}" CFLAGS_OTHER        ""        --cflags-only-other )

      if (_imp_target)
        _pkg_create_imp_target("${_prefix}" _no_cmake_path _no_cmake_environment_path)
      endif()
    endif()

    if(NOT "${_extra_paths}" STREQUAL "")
      # Restore the environment variable
      set(ENV{PKG_CONFIG_PATH} "${_pkgconfig_path_old}")
    endif()

    unset(_extra_paths)
    unset(_pkgconfig_path_old)
  else()
    if (${_is_required})
      message(SEND_ERROR "pkg-config tool not found")
    endif ()
  endif()
endmacro()

###
### User visible macros start here
###

#[========================================[.rst:
.. command:: pkg_check_modules

 Checks for all the given modules. ::

    pkg_check_modules(<PREFIX> [REQUIRED] [QUIET]
                      [NO_CMAKE_PATH] [NO_CMAKE_ENVIRONMENT_PATH]
                      [IMPORTED_TARGET]
                      <MODULE> [<MODULE>]*)


 When the ``REQUIRED`` argument was set, macros will fail with an error
 when module(s) could not be found.

 When the ``QUIET`` argument is set, no status messages will be printed.

 By default, if :variable:`CMAKE_MINIMUM_REQUIRED_VERSION` is 3.1 or
 later, or if :variable:`PKG_CONFIG_USE_CMAKE_PREFIX_PATH` is set, the
 :variable:`CMAKE_PREFIX_PATH`, :variable:`CMAKE_FRAMEWORK_PATH`, and
 :variable:`CMAKE_APPBUNDLE_PATH` cache and environment variables will
 be added to ``pkg-config`` search path.
 The ``NO_CMAKE_PATH`` and ``NO_CMAKE_ENVIRONMENT_PATH`` arguments
 disable this behavior for the cache variables and the environment
 variables, respectively.
 The ``IMPORTED_TARGET`` argument will create an imported target named
 PkgConfig::<PREFIX>> that can be passed directly as an argument to
 :command:`target_link_libraries`.

 It sets the following variables: ::

    PKG_CONFIG_FOUND          ... if pkg-config executable was found
    PKG_CONFIG_EXECUTABLE     ... pathname of the pkg-config program
    PKG_CONFIG_VERSION_STRING ... the version of the pkg-config program found
                                  (since CMake 2.8.8)

 For the following variables two sets of values exist; first one is the
 common one and has the given PREFIX.  The second set contains flags
 which are given out when ``pkg-config`` was called with the ``--static``
 option. ::

    <XPREFIX>_FOUND          ... set to 1 if module(s) exist
    <XPREFIX>_LIBRARIES      ... only the libraries (w/o the '-l')
    <XPREFIX>_LIBRARY_DIRS   ... the paths of the libraries (w/o the '-L')
    <XPREFIX>_LDFLAGS        ... all required linker flags
    <XPREFIX>_LDFLAGS_OTHER  ... all other linker flags
    <XPREFIX>_INCLUDE_DIRS   ... the '-I' preprocessor flags (w/o the '-I')
    <XPREFIX>_CFLAGS         ... all required cflags
    <XPREFIX>_CFLAGS_OTHER   ... the other compiler flags

 ::

    <XPREFIX> = <PREFIX>        for common case
    <XPREFIX> = <PREFIX>_STATIC for static linking

 There are some special variables whose prefix depends on the count of
 given modules.  When there is only one module, <PREFIX> stays
 unchanged.  When there are multiple modules, the prefix will be
 changed to <PREFIX>_<MODNAME>: ::

    <XPREFIX>_VERSION    ... version of the module
    <XPREFIX>_PREFIX     ... prefix-directory of the module
    <XPREFIX>_INCLUDEDIR ... include-dir of the module
    <XPREFIX>_LIBDIR     ... lib-dir of the module

 ::

    <XPREFIX> = <PREFIX>  when |MODULES| == 1, else
    <XPREFIX> = <PREFIX>_<MODNAME>

 A <MODULE> parameter can have the following formats: ::

    {MODNAME}            ... matches any version
    {MODNAME}>={VERSION} ... at least version <VERSION> is required
    {MODNAME}={VERSION}  ... exactly version <VERSION> is required
    {MODNAME}<={VERSION} ... modules must not be newer than <VERSION>

 Examples

 .. code-block:: cmake

    pkg_check_modules (GLIB2   glib-2.0)

 .. code-block:: cmake

    pkg_check_modules (GLIB2   glib-2.0>=2.10)

 Requires at least version 2.10 of glib2 and defines e.g.
 ``GLIB2_VERSION=2.10.3``

 .. code-block:: cmake

    pkg_check_modules (FOO     glib-2.0>=2.10 gtk+-2.0)

 Requires both glib2 and gtk2, and defines e.g.
 ``FOO_glib-2.0_VERSION=2.10.3`` and ``FOO_gtk+-2.0_VERSION=2.8.20``

 .. code-block:: cmake

    pkg_check_modules (XRENDER REQUIRED xrender)

 Defines for example::

   XRENDER_LIBRARIES=Xrender;X11``
   XRENDER_STATIC_LIBRARIES=Xrender;X11;pthread;Xau;Xdmcp
#]========================================]
macro(pkg_check_modules _prefix _module0)
  _pkgconfig_parse_options(_pkg_modules _pkg_is_required _pkg_is_silent _no_cmake_path _no_cmake_environment_path _imp_target "${_module0}" ${ARGN})
  # check cached value
  if (NOT DEFINED __pkg_config_checked_${_prefix} OR __pkg_config_checked_${_prefix} LESS ${PKG_CONFIG_VERSION} OR NOT ${_prefix}_FOUND OR NOT "${__pkg_config_arguments_${_prefix}}" STREQUAL "${_module0};${ARGN}")
    _pkg_check_modules_internal("${_pkg_is_required}" "${_pkg_is_silent}" ${_no_cmake_path} ${_no_cmake_environment_path} ${_imp_target} "${_prefix}" ${_pkg_modules})

    _pkgconfig_set(__pkg_config_checked_${_prefix} ${PKG_CONFIG_VERSION})
    if (${_prefix}_FOUND)
      _pkgconfig_set(__pkg_config_arguments_${_prefix} "${_module0};${ARGN}")
    endif()
  elseif (${_prefix}_FOUND AND ${_imp_target})
    _pkg_create_imp_target("${_prefix}" _no_cmake_path _no_cmake_environment_path)
  endif()
endmacro()


#[========================================[.rst:
.. command:: pkg_search_module

 Same as :command:`pkg_check_modules`, but instead it checks for given
 modules and uses the first working one. ::

    pkg_search_module(<PREFIX> [REQUIRED] [QUIET]
                      [NO_CMAKE_PATH] [NO_CMAKE_ENVIRONMENT_PATH]
                      [IMPORTED_TARGET]
                      <MODULE> [<MODULE>]*)

 Examples

 .. code-block:: cmake

    pkg_search_module (BAR     libxml-2.0 libxml2 libxml>=2)
#]========================================]
macro(pkg_search_module _prefix _module0)
  _pkgconfig_parse_options(_pkg_modules_alt _pkg_is_required _pkg_is_silent _no_cmake_path _no_cmake_environment_path _imp_target "${_module0}" ${ARGN})
  # check cached value
  if (NOT DEFINED __pkg_config_checked_${_prefix} OR __pkg_config_checked_${_prefix} LESS ${PKG_CONFIG_VERSION} OR NOT ${_prefix}_FOUND)
    set(_pkg_modules_found 0)

    if (NOT ${_pkg_is_silent})
      message(STATUS "Checking for one of the modules '${_pkg_modules_alt}'")
    endif ()

    # iterate through all modules and stop at the first working one.
    foreach(_pkg_alt ${_pkg_modules_alt})
      if(NOT _pkg_modules_found)
        _pkg_check_modules_internal(0 1 ${_no_cmake_path} ${_no_cmake_environment_path} ${_imp_target} "${_prefix}" "${_pkg_alt}")
      endif()

      if (${_prefix}_FOUND)
        set(_pkg_modules_found 1)
      endif()
    endforeach()

    if (NOT ${_prefix}_FOUND)
      if(${_pkg_is_required})
        message(SEND_ERROR "None of the required '${_pkg_modules_alt}' found")
      endif()
    endif()

    _pkgconfig_set(__pkg_config_checked_${_prefix} ${PKG_CONFIG_VERSION})
  elseif (${_prefix}_FOUND AND ${_imp_target})
    _pkg_create_imp_target("${_prefix}" _no_cmake_path _no_cmake_environment_path)
  endif()
endmacro()


#[========================================[.rst:
.. variable:: PKG_CONFIG_EXECUTABLE

 Path to the pkg-config executable.


.. variable:: PKG_CONFIG_USE_CMAKE_PREFIX_PATH

 Whether :command:`pkg_check_modules` and :command:`pkg_search_module`
 should add the paths in :variable:`CMAKE_PREFIX_PATH`,
 :variable:`CMAKE_FRAMEWORK_PATH`, and :variable:`CMAKE_APPBUNDLE_PATH`
 cache and environment variables to ``pkg-config`` search path.

 If this variable is not set, this behavior is enabled by default if
 :variable:`CMAKE_MINIMUM_REQUIRED_VERSION` is 3.1 or later, disabled
 otherwise.
#]========================================]


### Local Variables:
### mode: cmake
### End:
