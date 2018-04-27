# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
CPackFreeBSD
------------

The built in (binary) CPack FreeBSD (pkg) generator (Unix only)

Variables specific to CPack FreeBSD (pkg) generator
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CPackFreeBSD may be used to create pkg(8) packages -- these may be used
on FreeBSD, DragonflyBSD, NetBSD, OpenBSD, but also on Linux or OSX,
depending on the installed package-management tools -- using :module:`CPack`.

CPackFreeBSD is a :module:`CPack` generator and uses the ``CPACK_XXX``
variables used by :module:`CPack`. It tries to re-use packaging information
that may already be specified for Debian packages for the :module:`CPackDeb`
generator. it also tries to re-use RPM packaging information when Debian
does not specify.

CPackFreeBSD generator should work on any host with libpkg installed. The
packages it produces are specific to the host architecture and ABI.

CPackFreeBSD sets package-metadata through :code:`CPACK_FREEBSD_XXX` variables.
CPackFreeBSD, unlike CPackDeb, does not specially support componentized
packages; a single package is created from all the software artifacts
created through CMake.

All of the variables can be set specifically for FreeBSD packaging in
the CPackConfig file or in CMakeLists.txt, but most of them have defaults
that use general settings (e.g. CMAKE_PROJECT_NAME) or Debian-specific
variables when those make sense (e.g. the homepage of an upstream project
is usually unchanged by the flavor of packaging). When there is no Debian
information to fall back on, but the RPM packaging has it, fall back to
the RPM information (e.g. package license).

.. variable:: CPACK_FREEBSD_PACKAGE_NAME

  Sets the package name (in the package manifest, but also affects the
  output filename).

  * Mandatory: YES
  * Default:

    - :variable:`CPACK_PACKAGE_NAME` (this is always set by CPack itself,
      based on CMAKE_PROJECT_NAME).

.. variable:: CPACK_FREEBSD_PACKAGE_COMMENT

  Sets the package comment. This is the short description displayed by
  pkg(8) in standard "pkg info" output.

  * Mandatory: YES
  * Default:

    - :variable:`CPACK_PACKAGE_DESCRIPTION_SUMMARY` (this is always set
      by CPack itself, if nothing else sets it explicitly).
    - :variable:`PROJECT_DESCRIPTION` (this can be set with the DESCRIPTION
      parameter for :command:`project`).

.. variable:: CPACK_FREEBSD_PACKAGE_DESCRIPTION

  Sets the package description. This is the long description of the package,
  given by "pkg info" with a specific package as argument.

  * Mandatory: YES
  * Default:

    - :variable:`CPACK_DEBIAN_PACKAGE_DESCRIPTION` (this may be set already
      for Debian packaging, so we may as well re-use it).

.. variable:: CPACK_FREEBSD_PACKAGE_WWW

  The URL of the web site for this package, preferably (when applicable) the
  site from which the original source can be obtained and any additional
  upstream documentation or information may be found.

  * Mandatory: YES
  * Default:

   - :variable:`CPACK_DEBIAN_PACKAGE_HOMEPAGE` (this may be set already
     for Debian packaging, so we may as well re-use it).

.. variable:: CPACK_FREEBSD_PACKAGE_LICENSE

  The license, or licenses, which apply to this software package. This must
  be one or more license-identifiers that pkg recognizes as acceptable license
  identifiers (e.g. "GPLv2").

  * Mandatory: YES
  * Default:

    - :variable:`CPACK_RPM_PACKAGE_LICENSE`

.. variable:: CPACK_FREEBSD_PACKAGE_LICENSE_LOGIC

  This variable is only of importance if there is more than one license.
  The default is "single", which is only applicable to a single license.
  Other acceptable values are determined by pkg -- those are "dual" or "multi" --
  meaning choice (OR) or simultaneous (AND) application of the licenses.

  * Mandatory: NO
  * Default: single

.. variable:: CPACK_FREEBSD_PACKAGE_MAINTAINER

  The FreeBSD maintainer (e.g. kde@freebsd.org) of this package.

  * Mandatory: YES
  * Default: none

.. variable:: CPACK_FREEBSD_PACKAGE_ORIGIN

  The origin (ports label) of this package; for packages built by CPack
  outside of the ports system this is of less importance. The default
  puts the package somewhere under misc/, as a stopgap.

  * Mandatory: YES
  * Default: misc/<package name>

.. variable:: CPACK_FREEBSD_PACKAGE_CATEGORIES

  The ports categories where this package lives (if it were to be built
  from ports). If none is set a single category is determined based on
  the package origin.

  * Mandatory: YES
  * Default: derived from ORIGIN

.. variable:: CPACK_FREEBSD_PACKAGE_DEPS

  A list of package origins that should be added as package dependencies.
  These are in the form <category>/<packagename>, e.g. x11/libkonq.
  No version information needs to be provided (this is not included
  in the manifest).

  * Mandatory: NO
  * Default: empty
#]=======================================================================]



if(CMAKE_BINARY_DIR)
  message(FATAL_ERROR "CPackFreeBSD.cmake may only be used by CPack internally.")
endif()

if(NOT UNIX)
  message(FATAL_ERROR "CPackFreeBSD.cmake may only be used under UNIX.")
endif()


###
#
# These bits are copied from the Debian packaging file; slightly modified.
# They are used for filling in FreeBSD-packaging variables that can take
# on values from elsewhere -- e.g. the package description may as well be
# copied from Debian.
#
function(_cpack_freebsd_fallback_var OUTPUT_VAR_NAME)
  set(FALLBACK_VAR_NAMES ${ARGN})

  set(VALUE "${${OUTPUT_VAR_NAME}}")
  if(VALUE)
    return()
  endif()

  foreach(variable_name IN LISTS FALLBACK_VAR_NAMES)
    if(${variable_name})
      set(${OUTPUT_VAR_NAME} "${${variable_name}}" PARENT_SCOPE)
      set(VALUE "${${variable_name}}")
      break()
    endif()
  endforeach()
  if(NOT VALUE)
    message(WARNING "Variable ${OUTPUT_VAR_NAME} could not be given a fallback value from any variable ${FALLBACK_VAR_NAMES}.")
  endif()
endfunction()

function(check_required_var VAR_NAME)
  if(NOT ${VAR_NAME})
    message(FATAL_ERROR "Variable ${VAR_NAME} is not set.")
  endif()
endfunction()

set(_cpack_freebsd_fallback_origin "misc/bogus")

_cpack_freebsd_fallback_var("CPACK_FREEBSD_PACKAGE_NAME"
    "CPACK_PACKAGE_NAME"
    "CMAKE_PROJECT_NAME"
    )

set(_cpack_freebsd_fallback_www "http://example.com/?pkg=${CPACK_FREEBSD_PACKAGE_NAME}")

_cpack_freebsd_fallback_var("CPACK_FREEBSD_PACKAGE_COMMENT"
    "CPACK_PACKAGE_DESCRIPTION_SUMMARY"
    )

# TODO: maybe read the PACKAGE_DESCRIPTION file for the longer
#       FreeBSD pkg-descr?
_cpack_freebsd_fallback_var("CPACK_FREEBSD_PACKAGE_DESCRIPTION"
    "CPACK_DEBIAN_PACKAGE_DESCRIPTION"
    "CPACK_PACKAGE_DESCRIPTION_SUMMARY"
    "PACKAGE_DESCRIPTION"
    )

# There's really only one homepage for a project, so
# re-use the Debian setting if it's there.
_cpack_freebsd_fallback_var("CPACK_FREEBSD_PACKAGE_WWW"
    "CPACK_DEBIAN_PACKAGE_HOMEPAGE"
    "_cpack_freebsd_fallback_www"
    )

_cpack_freebsd_fallback_var("CPACK_FREEBSD_PACKAGE_VERSION"
    "CMAKE_PROJECT_VERSION"
    "${CMAKE_PROJECT_NAME}_VERSION"
    "PROJECT_VERSION"
    "CPACK_PACKAGE_VERSION"
    "CPACK_PACKAGE_VERSION"
    )

_cpack_freebsd_fallback_var("CPACK_FREEBSD_PACKAGE_MAINTAINER"
    "CPACK_PACKAGE_CONTACT"
    )

_cpack_freebsd_fallback_var("CPACK_FREEBSD_PACKAGE_LICENSE"
    "CPACK_RPM_PACKAGE_LICENSE"
    )

_cpack_freebsd_fallback_var("CPACK_FREEBSD_PACKAGE_ORIGIN"
  "_cpack_freebsd_fallback_origin"
  )

if(NOT CPACK_FREEBSD_PACKAGE_CATEGORIES)
  string(REGEX REPLACE "/.*" "" CPACK_FREEBSD_PACKAGE_CATEGORIES ${CPACK_FREEBSD_PACKAGE_ORIGIN})
endif()

check_required_var("CPACK_FREEBSD_PACKAGE_NAME")
check_required_var("CPACK_FREEBSD_PACKAGE_ORIGIN")
check_required_var("CPACK_FREEBSD_PACKAGE_VERSION")
check_required_var("CPACK_FREEBSD_PACKAGE_MAINTAINER")
check_required_var("CPACK_FREEBSD_PACKAGE_COMMENT")
check_required_var("CPACK_FREEBSD_PACKAGE_DESCRIPTION")
check_required_var("CPACK_FREEBSD_PACKAGE_WWW")
check_required_var("CPACK_FREEBSD_PACKAGE_LICENSE")
