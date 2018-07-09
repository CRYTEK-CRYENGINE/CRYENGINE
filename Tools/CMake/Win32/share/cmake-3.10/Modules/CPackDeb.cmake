# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CPackDeb
# --------
#
# The built in (binary) CPack Deb generator (Unix only)
#
# Variables specific to CPack Debian (DEB) generator
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# CPackDeb may be used to create Deb package using :module:`CPack`.
# CPackDeb is a :module:`CPack` generator thus it uses the ``CPACK_XXX``
# variables used by :module:`CPack`.
#
# CPackDeb generator should work on any Linux host but it will produce
# better deb package when Debian specific tools ``dpkg-xxx`` are usable on
# the build system.
#
# CPackDeb has specific features which are controlled by the specifics
# :code:`CPACK_DEBIAN_XXX` variables.
#
# :code:`CPACK_DEBIAN_<COMPONENT>_XXXX` variables may be used in order to have
# **component** specific values.  Note however that ``<COMPONENT>`` refers to
# the **grouping name** written in upper case. It may be either a component name
# or a component GROUP name.
#
# Here are some CPackDeb wiki resources that are here for historic reasons and
# are no longer maintained but may still prove useful:
#
#  - https://cmake.org/Wiki/CMake:CPackConfiguration
#  - https://cmake.org/Wiki/CMake:CPackPackageGenerators#DEB_.28UNIX_only.29
#
# List of CPackDEB specific variables:
#
# .. variable:: CPACK_DEB_COMPONENT_INSTALL
#
#  Enable component packaging for CPackDEB
#
#  * Mandatory : NO
#  * Default   : OFF
#
#  If enabled (ON) multiple packages are generated. By default a single package
#  containing files of all components is generated.
#
# .. variable:: CPACK_DEBIAN_PACKAGE_NAME
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_NAME
#
#  Set Package control field (variable is automatically transformed to lower
#  case).
#
#  * Mandatory : YES
#  * Default   :
#
#    - :variable:`CPACK_PACKAGE_NAME` for non-component based
#      installations
#    - :variable:`CPACK_DEBIAN_PACKAGE_NAME` suffixed with -<COMPONENT>
#      for component-based installations.
#
#  See https://www.debian.org/doc/debian-policy/ch-controlfields.html#s-f-Source
#
# .. variable:: CPACK_DEBIAN_FILE_NAME
#               CPACK_DEBIAN_<COMPONENT>_FILE_NAME
#
#  Package file name.
#
#  * Mandatory : YES
#  * Default   : ``<CPACK_PACKAGE_FILE_NAME>[-<component>].deb``
#
#  This may be set to ``DEB-DEFAULT`` to allow CPackDeb to generate package file
#  name by itself in deb format::
#
#    <PackageName>_<VersionNumber>-<DebianRevisionNumber>_<DebianArchitecture>.deb
#
#  Alternatively provided package file name must end
#  with either ``.deb`` or ``.ipk`` suffix.
#
#  .. note::
#
#    Preferred setting of this variable is ``DEB-DEFAULT`` but for backward
#    compatibility with CPackDeb in CMake prior to version 3.6 this feature
#    is disabled by default.
#
#  .. note::
#
#    By using non default filenames duplicate names may occur. Duplicate files
#    get overwritten and it is up to the packager to set the variables in a
#    manner that will prevent such errors.
#
# .. variable:: CPACK_DEBIAN_PACKAGE_EPOCH
#
#  The Debian package epoch
#
#  * Mandatory : No
#  * Default   : -
#
#  Optional number that should be incremented when changing versioning schemas
#  or fixing mistakes in the version numbers of older packages.
#
# .. variable:: CPACK_DEBIAN_PACKAGE_VERSION
#
#  The Debian package version
#
#  * Mandatory : YES
#  * Default   : :variable:`CPACK_PACKAGE_VERSION`
#
#  This variable may contain only alphanumerics (A-Za-z0-9) and the characters
#  . + - ~ (full stop, plus, hyphen, tilde) and should start with a digit. If
#  :variable:`CPACK_DEBIAN_PACKAGE_RELEASE` is not set then hyphens are not
#  allowed.
#
#  .. note::
#
#    For backward compatibility with CMake 3.9 and lower a failed test of this
#    variable's content is not a hard error when both
#    :variable:`CPACK_DEBIAN_PACKAGE_RELEASE` and
#    :variable:`CPACK_DEBIAN_PACKAGE_EPOCH` variables are not set. An author
#    warning is reported instead.
#
# .. variable:: CPACK_DEBIAN_PACKAGE_RELEASE
#
#  The Debian package release - Debian revision number.
#
#  * Mandatory : No
#  * Default   : -
#
#  This is the numbering of the DEB package itself, i.e. the version of the
#  packaging and not the version of the content (see
#  :variable:`CPACK_DEBIAN_PACKAGE_VERSION`). One may change the default value
#  if the previous packaging was buggy and/or you want to put here a fancy Linux
#  distro specific numbering.
#
# .. variable:: CPACK_DEBIAN_PACKAGE_ARCHITECTURE
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_ARCHITECTURE
#
#  The Debian package architecture
#
#  * Mandatory : YES
#  * Default   : Output of :code:`dpkg --print-architecture` (or :code:`i386`
#    if :code:`dpkg` is not found)
#
# .. variable:: CPACK_DEBIAN_PACKAGE_DEPENDS
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_DEPENDS
#
#  Sets the Debian dependencies of this package.
#
#  * Mandatory : NO
#  * Default   :
#
#    - An empty string for non-component based installations
#    - :variable:`CPACK_DEBIAN_PACKAGE_DEPENDS` for component-based
#      installations.
#
#  .. note::
#
#    If :variable:`CPACK_DEBIAN_PACKAGE_SHLIBDEPS` or
#    more specifically :variable:`CPACK_DEBIAN_<COMPONENT>_PACKAGE_SHLIBDEPS`
#    is set for this component, the discovered dependencies will be appended
#    to :variable:`CPACK_DEBIAN_<COMPONENT>_PACKAGE_DEPENDS` instead of
#    :variable:`CPACK_DEBIAN_PACKAGE_DEPENDS`. If
#    :variable:`CPACK_DEBIAN_<COMPONENT>_PACKAGE_DEPENDS` is an empty string,
#    only the automatically discovered dependencies will be set for this
#    component.
#
#  Example::
#
#    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.1-6), libc6 (< 2.4)")
#
# .. variable:: CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS
#
#  Sets inter component dependencies if listed with
#  :variable:`CPACK_COMPONENT_<compName>_DEPENDS` variables.
#
#  * Mandatory : NO
#  * Default   : -
#
# .. variable:: CPACK_DEBIAN_PACKAGE_MAINTAINER
#
#  The Debian package maintainer
#
#  * Mandatory : YES
#  * Default   : :code:`CPACK_PACKAGE_CONTACT`
#
# .. variable:: CPACK_DEBIAN_PACKAGE_DESCRIPTION
#               CPACK_COMPONENT_<COMPONENT>_DESCRIPTION
#
#  The Debian package description
#
#  * Mandatory : YES
#  * Default   :
#
#    - :variable:`CPACK_DEBIAN_PACKAGE_DESCRIPTION` if set or
#    - :variable:`CPACK_PACKAGE_DESCRIPTION_SUMMARY`
#
#
# .. variable:: CPACK_DEBIAN_PACKAGE_SECTION
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_SECTION
#
#  Set Section control field e.g. admin, devel, doc, ...
#
#  * Mandatory : YES
#  * Default   : "devel"
#
#  See https://www.debian.org/doc/debian-policy/ch-archive.html#s-subsections
#
# .. variable:: CPACK_DEBIAN_ARCHIVE_TYPE
#
#  The archive format used for creating the Debian package.
#
#  * Mandatory : YES
#  * Default   : "paxr"
#
#  Possible values are:
#
#  - paxr
#  - gnutar
#
#  .. note::
#
#    Default pax archive format is the most portable format and generates
#    packages that do not treat sparse files specially.
#    GNU tar format on the other hand supports longer filenames.
#
# .. variable:: CPACK_DEBIAN_COMPRESSION_TYPE
#
#  The compression used for creating the Debian package.
#
#  * Mandatory : YES
#  * Default   : "gzip"
#
#  Possible values are:
#
#  - lzma
#  - xz
#  - bzip2
#  - gzip
#
# .. variable:: CPACK_DEBIAN_PACKAGE_PRIORITY
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_PRIORITY
#
#  Set Priority control field e.g. required, important, standard, optional,
#  extra
#
#  * Mandatory : YES
#  * Default   : "optional"
#
#  See https://www.debian.org/doc/debian-policy/ch-archive.html#s-priorities
#
# .. variable:: CPACK_DEBIAN_PACKAGE_HOMEPAGE
#
#  The URL of the web site for this package, preferably (when applicable) the
#  site from which the original source can be obtained and any additional
#  upstream documentation or information may be found.
#
#  * Mandatory : NO
#  * Default   : -
#
#  .. note::
#
#    The content of this field is a simple URL without any surrounding
#    characters such as <>.
#
# .. variable:: CPACK_DEBIAN_PACKAGE_SHLIBDEPS
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_SHLIBDEPS
#
#  May be set to ON in order to use :code:`dpkg-shlibdeps` to generate
#  better package dependency list.
#
#  * Mandatory : NO
#  * Default   :
#
#    - :variable:`CPACK_DEBIAN_PACKAGE_SHLIBDEPS` if set or
#    - OFF
#
#  .. note::
#
#    You may need set :variable:`CMAKE_INSTALL_RPATH` to an appropriate value
#    if you use this feature, because if you don't :code:`dpkg-shlibdeps`
#    may fail to find your own shared libs.
#    See https://cmake.org/Wiki/CMake_RPATH_handling.
#
# .. variable:: CPACK_DEBIAN_PACKAGE_DEBUG
#
#  May be set when invoking cpack in order to trace debug information
#  during CPackDeb run.
#
#  * Mandatory : NO
#  * Default   : -
#
# .. variable:: CPACK_DEBIAN_PACKAGE_PREDEPENDS
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_PREDEPENDS
#
#  Sets the `Pre-Depends` field of the Debian package.
#  Like :variable:`Depends <CPACK_DEBIAN_PACKAGE_DEPENDS>`, except that it
#  also forces :code:`dpkg` to complete installation of the packages named
#  before even starting the installation of the package which declares the
#  pre-dependency.
#
#  * Mandatory : NO
#  * Default   :
#
#    - An empty string for non-component based installations
#    - :variable:`CPACK_DEBIAN_PACKAGE_PREDEPENDS` for component-based
#      installations.
#
#  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps
#
# .. variable:: CPACK_DEBIAN_PACKAGE_ENHANCES
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_ENHANCES
#
#  Sets the `Enhances` field of the Debian package.
#  Similar to :variable:`Suggests <CPACK_DEBIAN_PACKAGE_SUGGESTS>` but works
#  in the opposite direction: declares that a package can enhance the
#  functionality of another package.
#
#  * Mandatory : NO
#  * Default   :
#
#    - An empty string for non-component based installations
#    - :variable:`CPACK_DEBIAN_PACKAGE_ENHANCES` for component-based
#      installations.
#
#  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps
#
# .. variable:: CPACK_DEBIAN_PACKAGE_BREAKS
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_BREAKS
#
#  Sets the `Breaks` field of the Debian package.
#  When a binary package (P) declares that it breaks other packages (B),
#  :code:`dpkg` will not allow the package (P) which declares `Breaks` be
#  **unpacked** unless the packages that will be broken (B) are deconfigured
#  first.
#  As long as the package (P) is configured, the previously deconfigured
#  packages (B) cannot be reconfigured again.
#
#  * Mandatory : NO
#  * Default   :
#
#    - An empty string for non-component based installations
#    - :variable:`CPACK_DEBIAN_PACKAGE_BREAKS` for component-based
#      installations.
#
#  See https://www.debian.org/doc/debian-policy/ch-relationships.html#s-breaks
#
# .. variable:: CPACK_DEBIAN_PACKAGE_CONFLICTS
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_CONFLICTS
#
#  Sets the `Conflicts` field of the Debian package.
#  When one binary package declares a conflict with another using a `Conflicts`
#  field, :code:`dpkg` will not allow them to be unpacked on the system at
#  the same time.
#
#  * Mandatory : NO
#  * Default   :
#
#    - An empty string for non-component based installations
#    - :variable:`CPACK_DEBIAN_PACKAGE_CONFLICTS` for component-based
#      installations.
#
#  See https://www.debian.org/doc/debian-policy/ch-relationships.html#s-conflicts
#
#  .. note::
#
#    This is a stronger restriction than
#    :variable:`Breaks <CPACK_DEBIAN_PACKAGE_BREAKS>`, which prevents the
#    broken package from being configured while the breaking package is in
#    the "Unpacked" state but allows both packages to be unpacked at the same
#    time.
#
# .. variable:: CPACK_DEBIAN_PACKAGE_PROVIDES
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_PROVIDES
#
#  Sets the `Provides` field of the Debian package.
#  A virtual package is one which appears in the `Provides` control field of
#  another package.
#
#  * Mandatory : NO
#  * Default   :
#
#    - An empty string for non-component based installations
#    - :variable:`CPACK_DEBIAN_PACKAGE_PROVIDES` for component-based
#      installations.
#
#  See https://www.debian.org/doc/debian-policy/ch-relationships.html#s-virtual
#
# .. variable:: CPACK_DEBIAN_PACKAGE_REPLACES
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_REPLACES
#
#  Sets the `Replaces` field of the Debian package.
#  Packages can declare in their control file that they should overwrite
#  files in certain other packages, or completely replace other packages.
#
#  * Mandatory : NO
#  * Default   :
#
#    - An empty string for non-component based installations
#    - :variable:`CPACK_DEBIAN_PACKAGE_REPLACES` for component-based
#      installations.
#
#  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps
#
# .. variable:: CPACK_DEBIAN_PACKAGE_RECOMMENDS
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_RECOMMENDS
#
#  Sets the `Recommends` field of the Debian package.
#  Allows packages to declare a strong, but not absolute, dependency on other
#  packages.
#
#  * Mandatory : NO
#  * Default   :
#
#    - An empty string for non-component based installations
#    - :variable:`CPACK_DEBIAN_PACKAGE_RECOMMENDS` for component-based
#      installations.
#
#  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps
#
# .. variable:: CPACK_DEBIAN_PACKAGE_SUGGESTS
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_SUGGESTS
#
#  Sets the `Suggests` field of the Debian package.
#  Allows packages to declare a suggested package install grouping.
#
#  * Mandatory : NO
#  * Default   :
#
#    - An empty string for non-component based installations
#    - :variable:`CPACK_DEBIAN_PACKAGE_SUGGESTS` for component-based
#      installations.
#
#  See http://www.debian.org/doc/debian-policy/ch-relationships.html#s-binarydeps
#
# .. variable:: CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS
#
#  * Mandatory : NO
#  * Default   : OFF
#
#  Allows to generate shlibs control file automatically. Compatibility is defined by
#  :variable:`CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY` variable value.
#
#  .. note::
#
#    Libraries are only considered if they have both library name and version
#    set. This can be done by setting SOVERSION property with
#    :command:`set_target_properties` command.
#
# .. variable:: CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY
#
#  Compatibility policy for auto-generated shlibs control file.
#
#  * Mandatory : NO
#  * Default   : "="
#
#  Defines compatibility policy for auto-generated shlibs control file.
#  Possible values: "=", ">="
#
#  See https://www.debian.org/doc/debian-policy/ch-sharedlibs.html#s-sharedlibs-shlibdeps
#
# .. variable:: CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_CONTROL_EXTRA
#
#  This variable allow advanced user to add custom script to the
#  control.tar.gz.
#  Typical usage is for conffiles, postinst, postrm, prerm.
#
#  * Mandatory : NO
#  * Default   : -
#
#  Usage::
#
#   set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
#       "${CMAKE_CURRENT_SOURCE_DIR/prerm;${CMAKE_CURRENT_SOURCE_DIR}/postrm")
#
#  .. note::
#
#    The original permissions of the files will be used in the final
#    package unless the variable
#    :variable:`CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION` is set.
#    In particular, the scripts should have the proper executable
#    flag prior to the generation of the package.
#
# .. variable:: CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_CONTROL_STRICT_PERMISSION
#
#  This variable indicates if the Debian policy on control files should be
#  strictly followed.
#
#  * Mandatory : NO
#  * Default   : FALSE
#
#  Usage::
#
#   set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
#
#  .. note::
#
#    This overrides the permissions on the original files, following the rules
#    set by Debian policy
#    https://www.debian.org/doc/debian-policy/ch-files.html#s-permissions-owners
#
# .. variable:: CPACK_DEBIAN_PACKAGE_SOURCE
#               CPACK_DEBIAN_<COMPONENT>_PACKAGE_SOURCE
#
#  Sets the ``Source`` field of the binary Debian package.
#  When the binary package name is not the same as the source package name
#  (in particular when several components/binaries are generated from one
#  source) the source from which the binary has been generated should be
#  indicated with the field ``Source``.
#
#  * Mandatory : NO
#  * Default   :
#
#    - An empty string for non-component based installations
#    - :variable:`CPACK_DEBIAN_PACKAGE_SOURCE` for component-based
#      installations.
#
#  See https://www.debian.org/doc/debian-policy/ch-controlfields.html#s-f-Source
#
#  .. note::
#
#    This value is not interpreted. It is possible to pass an optional
#    revision number of the referenced source package as well.
#
# Building Debian packages on Windows
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# To communicate UNIX file permissions from the install stage
# to the CPack DEB generator the "cmake_mode_t" NTFS
# alternate data stream (ADT) is used.
#
# When a filesystem without ADT support is used only owner read/write
# permissions can be preserved.

# CPack script for creating Debian package
# Author: Mathieu Malaterre
#
# http://wiki.debian.org/HowToPackageForDebian

if(CMAKE_BINARY_DIR)
  message(FATAL_ERROR "CPackDeb.cmake may only be used by CPack internally.")
endif()

function(cpack_deb_variable_fallback OUTPUT_VAR_NAME)
  set(FALLBACK_VAR_NAMES ${ARGN})

  foreach(variable_name IN LISTS FALLBACK_VAR_NAMES)
    if(${variable_name})
      set(${OUTPUT_VAR_NAME} "${${variable_name}}" PARENT_SCOPE)
      break()
    endif()
  endforeach()
endfunction()

function(get_component_package_name var component)
  string(TOUPPER "${component}" component_upcase)
  if(CPACK_DEBIAN_${component_upcase}_PACKAGE_NAME)
    string(TOLOWER "${CPACK_DEBIAN_${component_upcase}_PACKAGE_NAME}" package_name)
  else()
    string(TOLOWER "${CPACK_DEBIAN_PACKAGE_NAME}-${component}" package_name)
  endif()

  set("${var}" "${package_name}" PARENT_SCOPE)
endfunction()

#extract library name and version for given shared object
function(extract_so_info shared_object libname version)
  if(READELF_EXECUTABLE)
    execute_process(COMMAND "${READELF_EXECUTABLE}" -d "${shared_object}"
      WORKING_DIRECTORY "${CPACK_TEMPORARY_DIRECTORY}"
      RESULT_VARIABLE result
      OUTPUT_VARIABLE output
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(result EQUAL 0)
      string(REGEX MATCH "\\(SONAME\\)[^\n]*\\[([^\n]+)\\.so\\.([^\n]*)\\]" soname "${output}")
      set(${libname} "${CMAKE_MATCH_1}" PARENT_SCOPE)
      set(${version} "${CMAKE_MATCH_2}" PARENT_SCOPE)
    else()
      message(WARNING "Error running readelf for \"${shared_object}\"")
    endif()
  else()
    message(FATAL_ERROR "Readelf utility is not available.")
  endif()
endfunction()

function(cpack_deb_prepare_package_vars)
  # CPACK_DEBIAN_PACKAGE_SHLIBDEPS
  # If specify OFF, only user depends are used
  if(NOT DEFINED CPACK_DEBIAN_PACKAGE_SHLIBDEPS)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
  endif()

  set(WDIR "${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}${CPACK_DEB_PACKAGE_COMPONENT_PART_PATH}")

  # per component automatic discover: some of the component might not have
  # binaries.
  if(CPACK_DEB_PACKAGE_COMPONENT)
    string(TOUPPER "${CPACK_DEB_PACKAGE_COMPONENT}" _local_component_name)
    set(_component_shlibdeps_var "CPACK_DEBIAN_${_local_component_name}_PACKAGE_SHLIBDEPS")

    # if set, overrides the global configuration
    if(DEFINED ${_component_shlibdeps_var})
      set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS "${${_component_shlibdeps_var}}")
      if(CPACK_DEBIAN_PACKAGE_DEBUG)
        message("CPackDeb Debug: component '${CPACK_DEB_PACKAGE_COMPONENT}' dpkg-shlibdeps set to ${CPACK_DEBIAN_PACKAGE_SHLIBDEPS}")
      endif()
    endif()
  endif()

  if(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OR CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS)
    # Generating binary list - Get type of all install files
    cmake_policy(PUSH)
      # Tell file(GLOB_RECURSE) not to follow directory symlinks
      # even if the project does not set this policy to NEW.
      cmake_policy(SET CMP0009 NEW)
      file(GLOB_RECURSE FILE_PATHS_ LIST_DIRECTORIES false RELATIVE "${WDIR}" "${WDIR}/*")
    cmake_policy(POP)

    find_program(FILE_EXECUTABLE file)
    if(NOT FILE_EXECUTABLE)
      message(FATAL_ERROR "CPackDeb: file utility is not available. CPACK_DEBIAN_PACKAGE_SHLIBDEPS and CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS options are not available.")
    endif()

    # get file info so that we can determine if file is executable or not
    unset(CPACK_DEB_INSTALL_FILES)
    foreach(FILE_ IN LISTS FILE_PATHS_)
      execute_process(COMMAND env LC_ALL=C ${FILE_EXECUTABLE} "./${FILE_}"
        WORKING_DIRECTORY "${WDIR}"
        RESULT_VARIABLE FILE_RESULT_
        OUTPUT_VARIABLE INSTALL_FILE_)
      if(NOT FILE_RESULT_ EQUAL 0)
        message (FATAL_ERROR "CPackDeb: execution of command: '${FILE_EXECUTABLE} ./${FILE_}' failed with exit code: ${FILE_RESULT_}")
      endif()
      list(APPEND CPACK_DEB_INSTALL_FILES "${INSTALL_FILE_}")
    endforeach()

    # Only dynamically linked ELF files are included
    # Extract only file name infront of ":"
    foreach(_FILE IN LISTS CPACK_DEB_INSTALL_FILES)
      if(_FILE MATCHES "ELF.*dynamically linked")
        string(REGEX MATCH "(^.*):" _FILE_NAME "${_FILE}")
        list(APPEND CPACK_DEB_BINARY_FILES "${CMAKE_MATCH_1}")
        set(CONTAINS_EXECUTABLE_FILES_ TRUE)
      endif()
      if(_FILE MATCHES "ELF.*shared object")
        string(REGEX MATCH "(^.*):" _FILE_NAME "${_FILE}")
        list(APPEND CPACK_DEB_SHARED_OBJECT_FILES "${CMAKE_MATCH_1}")
      endif()
    endforeach()
  endif()

  if(CPACK_DEBIAN_PACKAGE_SHLIBDEPS)
    # dpkg-shlibdeps is a Debian utility for generating dependency list
    find_program(SHLIBDEPS_EXECUTABLE dpkg-shlibdeps)

    if(SHLIBDEPS_EXECUTABLE)
      # Check version of the dpkg-shlibdeps tool using CPackDEB method
      execute_process(COMMAND env LC_ALL=C ${SHLIBDEPS_EXECUTABLE} --version
        OUTPUT_VARIABLE _TMP_VERSION
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      if(_TMP_VERSION MATCHES "dpkg-shlibdeps version ([0-9]+\\.[0-9]+\\.[0-9]+)")
        set(SHLIBDEPS_EXECUTABLE_VERSION "${CMAKE_MATCH_1}")
      else()
        set(SHLIBDEPS_EXECUTABLE_VERSION "")
      endif()

      if(CPACK_DEBIAN_PACKAGE_DEBUG)
        message("CPackDeb Debug: dpkg-shlibdeps --version output is '${_TMP_VERSION}'")
        message("CPackDeb Debug: dpkg-shlibdeps version is <${SHLIBDEPS_EXECUTABLE_VERSION}>")
      endif()

      if(CONTAINS_EXECUTABLE_FILES_)
        message("CPackDeb: - Generating dependency list")

        # Create blank control file for running dpkg-shlibdeps
        # There might be some other way to invoke dpkg-shlibdeps without creating this file
        # but standard debian package should not have anything that can collide with this file or directory
        file(MAKE_DIRECTORY ${CPACK_TEMPORARY_DIRECTORY}/debian)
        file(WRITE ${CPACK_TEMPORARY_DIRECTORY}/debian/control "")

        # Create a DEBIAN directory so that dpkg-shlibdeps can find the package dir when resolving $ORIGIN.
        file(MAKE_DIRECTORY "${CPACK_TEMPORARY_DIRECTORY}/DEBIAN")

        # Add --ignore-missing-info if the tool supports it
        execute_process(COMMAND env LC_ALL=C ${SHLIBDEPS_EXECUTABLE} --help
          OUTPUT_VARIABLE _TMP_HELP
          ERROR_QUIET
          OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(_TMP_HELP MATCHES "--ignore-missing-info")
          set(IGNORE_MISSING_INFO_FLAG "--ignore-missing-info")
        endif()

        # Execute dpkg-shlibdeps
        # --ignore-missing-info : allow dpkg-shlibdeps to run even if some libs do not belong to a package
        # -O : print to STDOUT
        execute_process(COMMAND ${SHLIBDEPS_EXECUTABLE} ${IGNORE_MISSING_INFO_FLAG} -O ${CPACK_DEB_BINARY_FILES}
          WORKING_DIRECTORY "${CPACK_TEMPORARY_DIRECTORY}"
          OUTPUT_VARIABLE SHLIBDEPS_OUTPUT
          RESULT_VARIABLE SHLIBDEPS_RESULT
          ERROR_VARIABLE SHLIBDEPS_ERROR
          OUTPUT_STRIP_TRAILING_WHITESPACE )
        if(CPACK_DEBIAN_PACKAGE_DEBUG)
          # dpkg-shlibdeps will throw some warnings if some input files are not binary
          message( "CPackDeb Debug: dpkg-shlibdeps warnings \n${SHLIBDEPS_ERROR}")
        endif()
        if(NOT SHLIBDEPS_RESULT EQUAL 0)
          message (FATAL_ERROR "CPackDeb: dpkg-shlibdeps: '${SHLIBDEPS_ERROR}';\n"
              "executed command: '${SHLIBDEPS_EXECUTABLE} ${IGNORE_MISSING_INFO_FLAG} -O ${CPACK_DEB_BINARY_FILES}';\n"
              "found files: '${INSTALL_FILE_}';\n"
              "files info: '${CPACK_DEB_INSTALL_FILES}';\n"
              "binary files: '${CPACK_DEB_BINARY_FILES}'")
        endif()

        #Get rid of prefix generated by dpkg-shlibdeps
        string(REGEX REPLACE "^.*Depends=" "" CPACK_DEBIAN_PACKAGE_AUTO_DEPENDS "${SHLIBDEPS_OUTPUT}")

        if(CPACK_DEBIAN_PACKAGE_DEBUG)
          message("CPackDeb Debug: Found dependency: ${CPACK_DEBIAN_PACKAGE_AUTO_DEPENDS} from output ${SHLIBDEPS_OUTPUT}")
        endif()

        # Remove blank control file
        # Might not be safe if package actual contain file or directory named debian
        file(REMOVE_RECURSE "${CPACK_TEMPORARY_DIRECTORY}/debian")

        # remove temporary directory that was created only for dpkg-shlibdeps execution
        file(REMOVE_RECURSE "${CPACK_TEMPORARY_DIRECTORY}/DEBIAN")
      else()
        if(CPACK_DEBIAN_PACKAGE_DEBUG)
          message(AUTHOR_WARNING "CPackDeb Debug: Using only user-provided depends because package does not contain executable files that link to shared libraries.")
        endif()
      endif()
    else()
      message("CPackDeb: Using only user-provided dependencies because dpkg-shlibdeps is not found.")
    endif()

  else()
    if(CPACK_DEBIAN_PACKAGE_DEBUG)
      message("CPackDeb Debug: Using only user-provided dependencies")
    endif()
  endif()

  # Let's define the control file found in debian package:

  # Binary package:
  # http://www.debian.org/doc/debian-policy/ch-controlfields.html#s-binarycontrolfiles

  # DEBIAN/control
  # debian policy enforce lower case for package name
  # Package: (mandatory)
  if(NOT CPACK_DEBIAN_PACKAGE_NAME)
    string(TOLOWER "${CPACK_PACKAGE_NAME}" CPACK_DEBIAN_PACKAGE_NAME)
  endif()

  # Version: (mandatory)
  if(NOT CPACK_DEBIAN_PACKAGE_VERSION)
    if(NOT CPACK_PACKAGE_VERSION)
      message(FATAL_ERROR "CPackDeb: Debian package requires a package version")
    endif()
    set(CPACK_DEBIAN_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION})
  endif()

  if(DEFINED CPACK_DEBIAN_PACKAGE_RELEASE OR DEFINED CPACK_DEBIAN_PACKAGE_EPOCH)
    # only test the version format if CPACK_DEBIAN_PACKAGE_RELEASE or
    # CPACK_DEBIAN_PACKAGE_EPOCH is set
    if(NOT CPACK_DEBIAN_PACKAGE_VERSION MATCHES "^[0-9][A-Za-z0-9.+~-]*$")
      message(FATAL_ERROR
        "CPackDeb: Debian package version must confirm to \"^[0-9][A-Za-z0-9.+~-]*$\" regex!")
    endif()
  else()
    # before CMake 3.10 version format was not tested so only warn to preserve
    # backward compatibility
    if(NOT CPACK_DEBIAN_PACKAGE_VERSION MATCHES "^([0-9]+:)?[0-9][A-Za-z0-9.+~-]*$")
      message(AUTHOR_WARNING
        "CPackDeb: Debian package versioning ([<epoch>:]<version>[-<release>])"
        " should confirm to \"^([0-9]+:)?[0-9][A-Za-z0-9.+~-]*$\" regex in"
        " order to satisfy Debian packaging rules.")
    endif()
  endif()

  if(CPACK_DEBIAN_PACKAGE_RELEASE)
    if(NOT CPACK_DEBIAN_PACKAGE_RELEASE MATCHES "^[A-Za-z0-9.+~]+$")
      message(FATAL_ERROR
        "CPackDeb: Debian package release must confirm to \"^[A-Za-z0-9.+~]+$\" regex!")
    endif()
    string(APPEND CPACK_DEBIAN_PACKAGE_VERSION
      "-${CPACK_DEBIAN_PACKAGE_RELEASE}")
  elseif(DEFINED CPACK_DEBIAN_PACKAGE_EPOCH)
    # only test the version format if CPACK_DEBIAN_PACKAGE_RELEASE or
    # CPACK_DEBIAN_PACKAGE_EPOCH is set - versions CPack/Deb generator before
    # CMake 3.10 did not check for version format so we have to preserve
    # backward compatibility
    if(CPACK_DEBIAN_PACKAGE_VERSION MATCHES ".*-.*")
      message(FATAL_ERROR
        "CPackDeb: Debian package version must not contain hyphens when CPACK_DEBIAN_PACKAGE_RELEASE is not provided!")
    endif()
  endif()

  if(CPACK_DEBIAN_PACKAGE_EPOCH)
    if(NOT CPACK_DEBIAN_PACKAGE_EPOCH MATCHES "^[0-9]+$")
      message(FATAL_ERROR
        "CPackDeb: Debian package epoch must confirm to \"^[0-9]+$\" regex!")
    endif()
    set(CPACK_DEBIAN_PACKAGE_VERSION
      "${CPACK_DEBIAN_PACKAGE_EPOCH}:${CPACK_DEBIAN_PACKAGE_VERSION}")
  endif()

  # Architecture: (mandatory)
  if(CPACK_DEB_PACKAGE_COMPONENT AND CPACK_DEBIAN_${_local_component_name}_PACKAGE_ARCHITECTURE)
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CPACK_DEBIAN_${_local_component_name}_PACKAGE_ARCHITECTURE}")
  elseif(NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
    # There is no such thing as i686 architecture on debian, you should use i386 instead
    # $ dpkg --print-architecture
    find_program(DPKG_CMD dpkg)
    if(NOT DPKG_CMD)
      message(STATUS "CPackDeb: Can not find dpkg in your path, default to i386.")
      set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386)
    endif()
    execute_process(COMMAND "${DPKG_CMD}" --print-architecture
      OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
  endif()

  # Source: (optional)
  # in case several packages are constructed from a unique source
  # (multipackaging), the source may be indicated as well.
  # The source might contain a version if the generated package
  # version is different from the source version
  if(NOT CPACK_DEBIAN_PACKAGE_SOURCE)
    set(CPACK_DEBIAN_PACKAGE_SOURCE "")
  endif()

  # have a look at get_property(result GLOBAL PROPERTY ENABLED_FEATURES),
  # this returns the successful find_package() calls, maybe this can help
  # Depends:
  # You should set: DEBIAN_PACKAGE_DEPENDS
  # TODO: automate 'objdump -p | grep NEEDED'

  # if per-component variable, overrides the global CPACK_DEBIAN_PACKAGE_${variable_type_}
  # automatic dependency discovery will be performed afterwards.
  if(CPACK_DEB_PACKAGE_COMPONENT)
    foreach(value_type_ DEPENDS RECOMMENDS SUGGESTS PREDEPENDS ENHANCES BREAKS CONFLICTS PROVIDES REPLACES SOURCE SECTION PRIORITY NAME)
      set(_component_var "CPACK_DEBIAN_${_local_component_name}_PACKAGE_${value_type_}")

      # if set, overrides the global variable
      if(DEFINED ${_component_var})
        set(CPACK_DEBIAN_PACKAGE_${value_type_} "${${_component_var}}")
        if(CPACK_DEBIAN_PACKAGE_DEBUG)
          message("CPackDeb Debug: component '${_local_component_name}' ${value_type_} "
            "value set to '${CPACK_DEBIAN_PACKAGE_${value_type_}}'")
        endif()
      endif()
    endforeach()

    if(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS)
      set(COMPONENT_DEPENDS "")
      foreach (_PACK ${CPACK_COMPONENT_${_local_component_name}_DEPENDS})
        get_component_package_name(_PACK_NAME "${_PACK}")
        if(COMPONENT_DEPENDS)
          set(COMPONENT_DEPENDS "${_PACK_NAME} (= ${CPACK_DEBIAN_PACKAGE_VERSION}), ${COMPONENT_DEPENDS}")
        else()
          set(COMPONENT_DEPENDS "${_PACK_NAME} (= ${CPACK_DEBIAN_PACKAGE_VERSION})")
        endif()
      endforeach()
      if(COMPONENT_DEPENDS)
        if(CPACK_DEBIAN_PACKAGE_DEPENDS)
          set(CPACK_DEBIAN_PACKAGE_DEPENDS "${COMPONENT_DEPENDS}, ${CPACK_DEBIAN_PACKAGE_DEPENDS}")
        else()
          set(CPACK_DEBIAN_PACKAGE_DEPENDS "${COMPONENT_DEPENDS}")
        endif()
      endif()
    endif()
  endif()

  # at this point, the CPACK_DEBIAN_PACKAGE_DEPENDS is properly set
  # to the minimal dependency of the package
  # Append automatically discovered dependencies .
  if(NOT "${CPACK_DEBIAN_PACKAGE_AUTO_DEPENDS}" STREQUAL "")
    if (CPACK_DEBIAN_PACKAGE_DEPENDS)
      set (CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, ${CPACK_DEBIAN_PACKAGE_AUTO_DEPENDS}")
    else ()
      set (CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_AUTO_DEPENDS}")
    endif ()
  endif()

  if(NOT CPACK_DEBIAN_PACKAGE_DEPENDS)
    message(STATUS "CPACK_DEBIAN_PACKAGE_DEPENDS not set, the package will have no dependencies.")
  endif()

  # Maintainer: (mandatory)
  if(NOT CPACK_DEBIAN_PACKAGE_MAINTAINER)
    if(NOT CPACK_PACKAGE_CONTACT)
      message(FATAL_ERROR "CPackDeb: Debian package requires a maintainer for a package, set CPACK_PACKAGE_CONTACT or CPACK_DEBIAN_PACKAGE_MAINTAINER")
    endif()
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER ${CPACK_PACKAGE_CONTACT})
  endif()

  # Description: (mandatory)
  if(NOT CPACK_DEB_PACKAGE_COMPONENT)
    if(NOT CPACK_DEBIAN_PACKAGE_DESCRIPTION)
      if(NOT CPACK_PACKAGE_DESCRIPTION_SUMMARY)
        message(FATAL_ERROR "CPackDeb: Debian package requires a summary for a package, set CPACK_PACKAGE_DESCRIPTION_SUMMARY or CPACK_DEBIAN_PACKAGE_DESCRIPTION")
      endif()
      set(CPACK_DEBIAN_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION_SUMMARY})
    endif()
  else()
    set(component_description_var CPACK_COMPONENT_${_local_component_name}_DESCRIPTION)

    # component description overrides package description
    if(${component_description_var})
      set(CPACK_DEBIAN_PACKAGE_DESCRIPTION ${${component_description_var}})
    elseif(NOT CPACK_DEBIAN_PACKAGE_DESCRIPTION)
      if(NOT CPACK_PACKAGE_DESCRIPTION_SUMMARY)
        message(FATAL_ERROR "CPackDeb: Debian package requires a summary for a package, set CPACK_PACKAGE_DESCRIPTION_SUMMARY or CPACK_DEBIAN_PACKAGE_DESCRIPTION or ${component_description_var}")
      endif()
      set(CPACK_DEBIAN_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION_SUMMARY})
    endif()
  endif()

  # Section: (recommended)
  if(NOT CPACK_DEBIAN_PACKAGE_SECTION)
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
  endif()

  # Priority: (recommended)
  if(NOT CPACK_DEBIAN_PACKAGE_PRIORITY)
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
  endif()

  if(CPACK_DEBIAN_ARCHIVE_TYPE)
    set(archive_types_ "paxr;gnutar")
    cmake_policy(PUSH)
      cmake_policy(SET CMP0057 NEW)
      if(NOT CPACK_DEBIAN_ARCHIVE_TYPE IN_LIST archive_types_)
        message(FATAL_ERROR "CPACK_DEBIAN_ARCHIVE_TYPE set to unsupported"
          "type ${CPACK_DEBIAN_ARCHIVE_TYPE}")
      endif()
    cmake_policy(POP)
  else()
    set(CPACK_DEBIAN_ARCHIVE_TYPE "paxr")
  endif()

  # Compression: (recommended)
  if(NOT CPACK_DEBIAN_COMPRESSION_TYPE)
    set(CPACK_DEBIAN_COMPRESSION_TYPE "gzip")
  endif()

  # Recommends:
  # You should set: CPACK_DEBIAN_PACKAGE_RECOMMENDS

  # Suggests:
  # You should set: CPACK_DEBIAN_PACKAGE_SUGGESTS

  # CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
  # This variable allow advanced user to add custom script to the control.tar.gz (inside the .deb archive)
  # Typical examples are:
  # - conffiles
  # - postinst
  # - postrm
  # - prerm
  # Usage:
  # set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
  #    "${CMAKE_CURRENT_SOURCE_DIR/prerm;${CMAKE_CURRENT_SOURCE_DIR}/postrm")

  # Are we packaging components ?
  if(CPACK_DEB_PACKAGE_COMPONENT)
    # override values with per component version if set
    foreach(VAR_NAME_ "PACKAGE_CONTROL_EXTRA" "PACKAGE_CONTROL_STRICT_PERMISSION")
      if(CPACK_DEBIAN_${_local_component_name}_${VAR_NAME_})
        set(CPACK_DEBIAN_${VAR_NAME_} "${CPACK_DEBIAN_${_local_component_name}_${VAR_NAME_}}")
      endif()
    endforeach()
    get_component_package_name(CPACK_DEBIAN_PACKAGE_NAME ${_local_component_name})
  endif()

  set(CPACK_DEBIAN_PACKAGE_SHLIBS_LIST "")

  if (NOT CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY)
    set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY "=")
  endif()

  find_program(READELF_EXECUTABLE NAMES readelf)

  if(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS)
    if(READELF_EXECUTABLE)
      foreach(_FILE IN LISTS CPACK_DEB_SHARED_OBJECT_FILES)
        extract_so_info("${_FILE}" libname soversion)
        if(libname AND soversion)
          list(APPEND CPACK_DEBIAN_PACKAGE_SHLIBS_LIST
               "${libname} ${soversion} ${CPACK_DEBIAN_PACKAGE_NAME} (${CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY} ${CPACK_DEBIAN_PACKAGE_VERSION})")
        else()
          message(AUTHOR_WARNING "Shared library '${_FILE}' is missing soname or soversion. Library will not be added to DEBIAN/shlibs control file.")
        endif()
      endforeach()
      if (CPACK_DEBIAN_PACKAGE_SHLIBS_LIST)
        string(REPLACE ";" "\n" CPACK_DEBIAN_PACKAGE_SHLIBS_LIST "${CPACK_DEBIAN_PACKAGE_SHLIBS_LIST}")
      endif()
    else()
      message(FATAL_ERROR "Readelf utility is not available. CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS option is not available.")
    endif()
  endif()

  # add ldconfig call in default postrm and postint
  set(CPACK_ADD_LDCONFIG_CALL 0)
  foreach(_FILE ${CPACK_DEB_SHARED_OBJECT_FILES})
    get_filename_component(_DIR ${_FILE} DIRECTORY)
    # all files in CPACK_DEB_SHARED_OBJECT_FILES have dot at the beginning
    if(_DIR STREQUAL "./lib" OR _DIR STREQUAL "./usr/lib")
      set(CPACK_ADD_LDCONFIG_CALL 1)
    endif()
  endforeach()

  if(CPACK_ADD_LDCONFIG_CALL)
    set(CPACK_DEBIAN_GENERATE_POSTINST 1)
    set(CPACK_DEBIAN_GENERATE_POSTRM 1)
    foreach(f ${PACKAGE_CONTROL_EXTRA})
      get_filename_component(n "${f}" NAME)
      if("${n}" STREQUAL "postinst")
        set(CPACK_DEBIAN_GENERATE_POSTINST 0)
      endif()
      if("${n}" STREQUAL "postrm")
        set(CPACK_DEBIAN_GENERATE_POSTRM 0)
      endif()
    endforeach()
  else()
    set(CPACK_DEBIAN_GENERATE_POSTINST 0)
    set(CPACK_DEBIAN_GENERATE_POSTRM 0)
  endif()

  cpack_deb_variable_fallback("CPACK_DEBIAN_FILE_NAME"
    "CPACK_DEBIAN_${_local_component_name}_FILE_NAME"
    "CPACK_DEBIAN_FILE_NAME")
  if(CPACK_DEBIAN_FILE_NAME)
    if(CPACK_DEBIAN_FILE_NAME STREQUAL "DEB-DEFAULT")
      # Patch package file name to be in corrent debian format:
      # <foo>_<VersionNumber>-<DebianRevisionNumber>_<DebianArchitecture>.deb
      set(CPACK_OUTPUT_FILE_NAME
        "${CPACK_DEBIAN_PACKAGE_NAME}_${CPACK_DEBIAN_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")
    else()
      cmake_policy(PUSH)
        cmake_policy(SET CMP0010 NEW)
        if(NOT CPACK_DEBIAN_FILE_NAME MATCHES ".*\\.(deb|ipk)")
      cmake_policy(POP)
          message(FATAL_ERROR "'${CPACK_DEBIAN_FILE_NAME}' is not a valid DEB package file name as it must end with '.deb' or '.ipk'!")
        endif()
      cmake_policy(POP)

      set(CPACK_OUTPUT_FILE_NAME "${CPACK_DEBIAN_FILE_NAME}")
    endif()

    set(CPACK_TEMPORARY_PACKAGE_FILE_NAME "${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_OUTPUT_FILE_NAME}")
    get_filename_component(BINARY_DIR "${CPACK_OUTPUT_FILE_PATH}" DIRECTORY)
    set(CPACK_OUTPUT_FILE_PATH "${BINARY_DIR}/${CPACK_OUTPUT_FILE_NAME}")
  endif() # else() back compatibility - don't change the name

  # Print out some debug information if we were asked for that
  if(CPACK_DEBIAN_PACKAGE_DEBUG)
     message("CPackDeb:Debug: CPACK_TOPLEVEL_DIRECTORY          = '${CPACK_TOPLEVEL_DIRECTORY}'")
     message("CPackDeb:Debug: CPACK_TOPLEVEL_TAG                = '${CPACK_TOPLEVEL_TAG}'")
     message("CPackDeb:Debug: CPACK_TEMPORARY_DIRECTORY         = '${CPACK_TEMPORARY_DIRECTORY}'")
     message("CPackDeb:Debug: CPACK_OUTPUT_FILE_NAME            = '${CPACK_OUTPUT_FILE_NAME}'")
     message("CPackDeb:Debug: CPACK_OUTPUT_FILE_PATH            = '${CPACK_OUTPUT_FILE_PATH}'")
     message("CPackDeb:Debug: CPACK_PACKAGE_FILE_NAME           = '${CPACK_PACKAGE_FILE_NAME}'")
     message("CPackDeb:Debug: CPACK_PACKAGE_INSTALL_DIRECTORY   = '${CPACK_PACKAGE_INSTALL_DIRECTORY}'")
     message("CPackDeb:Debug: CPACK_TEMPORARY_PACKAGE_FILE_NAME = '${CPACK_TEMPORARY_PACKAGE_FILE_NAME}'")
     message("CPackDeb:Debug: CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION = '${CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION}'")
     message("CPackDeb:Debug: CPACK_DEBIAN_PACKAGE_SOURCE       = '${CPACK_DEBIAN_PACKAGE_SOURCE}'")
  endif()

  # For debian source packages:
  # debian/control
  # http://www.debian.org/doc/debian-policy/ch-controlfields.html#s-sourcecontrolfiles

  # .dsc
  # http://www.debian.org/doc/debian-policy/ch-controlfields.html#s-debiansourcecontrolfiles

  # Builds-Depends:
  #if(NOT CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS)
  #  set(CPACK_DEBIAN_PACKAGE_BUILDS_DEPENDS
  #    "debhelper (>> 5.0.0), libncurses5-dev, tcl8.4"
  #  )
  #endif()

  # move variables to parent scope so that they may be used to create debian package
  set(GEN_CPACK_OUTPUT_FILE_NAME "${CPACK_OUTPUT_FILE_NAME}" PARENT_SCOPE)
  set(GEN_CPACK_TEMPORARY_PACKAGE_FILE_NAME "${CPACK_TEMPORARY_PACKAGE_FILE_NAME}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_NAME "${CPACK_DEBIAN_PACKAGE_NAME}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_VERSION "${CPACK_DEBIAN_PACKAGE_VERSION}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_SECTION "${CPACK_DEBIAN_PACKAGE_SECTION}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_PRIORITY "${CPACK_DEBIAN_PACKAGE_PRIORITY}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_DEBIAN_PACKAGE_MAINTAINER}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_DEBIAN_PACKAGE_DESCRIPTION}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_ARCHIVE_TYPE "${CPACK_DEBIAN_ARCHIVE_TYPE}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_COMPRESSION_TYPE "${CPACK_DEBIAN_COMPRESSION_TYPE}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_RECOMMENDS "${CPACK_DEBIAN_PACKAGE_RECOMMENDS}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_SUGGESTS "${CPACK_DEBIAN_PACKAGE_SUGGESTS}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_DEBIAN_PACKAGE_HOMEPAGE}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_PREDEPENDS "${CPACK_DEBIAN_PACKAGE_PREDEPENDS}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_ENHANCES "${CPACK_DEBIAN_PACKAGE_ENHANCES}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_BREAKS "${CPACK_DEBIAN_PACKAGE_BREAKS}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_CONFLICTS "${CPACK_DEBIAN_PACKAGE_CONFLICTS}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_PROVIDES "${CPACK_DEBIAN_PACKAGE_PROVIDES}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_REPLACES "${CPACK_DEBIAN_PACKAGE_REPLACES}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_SHLIBS "${CPACK_DEBIAN_PACKAGE_SHLIBS_LIST}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION
      "${CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_PACKAGE_SOURCE
     "${CPACK_DEBIAN_PACKAGE_SOURCE}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_GENERATE_POSTINST "${CPACK_DEBIAN_GENERATE_POSTINST}" PARENT_SCOPE)
  set(GEN_CPACK_DEBIAN_GENERATE_POSTRM "${CPACK_DEBIAN_GENERATE_POSTRM}" PARENT_SCOPE)
  set(GEN_WDIR "${WDIR}" PARENT_SCOPE)
endfunction()

cpack_deb_prepare_package_vars()
