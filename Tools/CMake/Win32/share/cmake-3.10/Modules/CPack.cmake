# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CPack
# -----
#
# Build binary and source package installers.
#
# Variables common to all CPack generators
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# The
# CPack module generates binary and source installers in a variety of
# formats using the cpack program.  Inclusion of the CPack module adds
# two new targets to the resulting makefiles, package and
# package_source, which build the binary and source installers,
# respectively.  The generated binary installers contain everything
# installed via CMake's INSTALL command (and the deprecated
# INSTALL_FILES, INSTALL_PROGRAMS, and INSTALL_TARGETS commands).
#
# For certain kinds of binary installers (including the graphical
# installers on Mac OS X and Windows), CPack generates installers that
# allow users to select individual application components to install.
# See CPackComponent module for that.
#
# The CPACK_GENERATOR variable has different meanings in different
# contexts.  In your CMakeLists.txt file, CPACK_GENERATOR is a *list of
# generators*: when run with no other arguments, CPack will iterate over
# that list and produce one package for each generator.  In a
# CPACK_PROJECT_CONFIG_FILE, though, CPACK_GENERATOR is a *string naming
# a single generator*.  If you need per-cpack- generator logic to
# control *other* cpack settings, then you need a
# CPACK_PROJECT_CONFIG_FILE.
#
# The CMake source tree itself contains a CPACK_PROJECT_CONFIG_FILE.
# See the top level file CMakeCPackOptions.cmake.in for an example.
#
# If set, the CPACK_PROJECT_CONFIG_FILE is included automatically on a
# per-generator basis.  It only need contain overrides.
#
# Here's how it works:
#
# * cpack runs
# * it includes CPackConfig.cmake
# * it iterates over the generators listed in that file's
#   CPACK_GENERATOR list variable (unless told to use just a
#   specific one via -G on the command line...)
# * foreach generator, it then
#
#   - sets CPACK_GENERATOR to the one currently being iterated
#   - includes the CPACK_PROJECT_CONFIG_FILE
#   - produces the package for that generator
#
# This is the key: For each generator listed in CPACK_GENERATOR in
# CPackConfig.cmake, cpack will *reset* CPACK_GENERATOR internally to
# *the one currently being used* and then include the
# CPACK_PROJECT_CONFIG_FILE.
#
# Before including this CPack module in your CMakeLists.txt file, there
# are a variety of variables that can be set to customize the resulting
# installers.  The most commonly-used variables are:
#
# .. variable:: CPACK_PACKAGE_NAME
#
#  The name of the package (or application). If not specified, defaults to
#  the project name.
#
# .. variable:: CPACK_PACKAGE_VENDOR
#
#  The name of the package vendor. (e.g., "Kitware").
#
# .. variable:: CPACK_PACKAGE_DIRECTORY
#
#  The directory in which CPack is doing its packaging. If it is not set
#  then this will default (internally) to the build dir. This variable may
#  be defined in CPack config file or from the cpack command line option
#  "-B". If set the command line option override the value found in the
#  config file.
#
# .. variable:: CPACK_PACKAGE_VERSION_MAJOR
#
#  Package major Version
#
# .. variable:: CPACK_PACKAGE_VERSION_MINOR
#
#  Package minor Version
#
# .. variable:: CPACK_PACKAGE_VERSION_PATCH
#
#  Package patch Version
#
# .. variable:: CPACK_PACKAGE_DESCRIPTION_FILE
#
#  A text file used to describe the project. Used, for example, the
#  introduction screen of a CPack-generated Windows installer to describe
#  the project.
#
# .. variable:: CPACK_PACKAGE_DESCRIPTION_SUMMARY
#
#  Short description of the project (only a few words). Default value is::
#
#    ${PROJECT_DESCRIPTION}
#
#  if DESCRIPTION has given to the project() call or
#  CMake generated string with PROJECT_NAME otherwise.
#
# .. variable:: CPACK_PACKAGE_FILE_NAME
#
#  The name of the package file to generate, not including the
#  extension. For example, cmake-2.6.1-Linux-i686.  The default value is::
#
#   ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}.
#
# .. variable:: CPACK_PACKAGE_INSTALL_DIRECTORY
#
#  Installation directory on the target system. This may be used by some
#  CPack generators like NSIS to create an installation directory e.g.,
#  "CMake 2.5" below the installation prefix. All installed element will be
#  put inside this directory.
#
# .. variable:: CPACK_PACKAGE_ICON
#
#  A branding image that will be displayed inside the installer (used by GUI
#  installers).
#
# .. variable:: CPACK_PACKAGE_CHECKSUM
#
#  An algorithm that will be used to generate additional file with checksum
#  of the package. Output file name will be::
#
#     ${CPACK_PACKAGE_FILE_NAME}.${CPACK_PACKAGE_CHECKSUM}
#
#  Supported algorithms are those listed by the
#  :ref:`string(\<HASH\>) <Supported Hash Algorithms>`
#  command.
#
# .. variable:: CPACK_PROJECT_CONFIG_FILE
#
#  CPack-time project CPack configuration file. This file included at cpack
#  time, once per generator after CPack has set CPACK_GENERATOR to the
#  actual generator being used. It allows per-generator setting of CPACK_*
#  variables at cpack time.
#
# .. variable:: CPACK_RESOURCE_FILE_LICENSE
#
#  License to be embedded in the installer. It will typically be displayed
#  to the user by the produced installer (often with an explicit "Accept"
#  button, for graphical installers) prior to installation. This license
#  file is NOT added to installed file but is used by some CPack generators
#  like NSIS. If you want to install a license file (may be the same as this
#  one) along with your project you must add an appropriate CMake INSTALL
#  command in your CMakeLists.txt.
#
# .. variable:: CPACK_RESOURCE_FILE_README
#
#  ReadMe file to be embedded in the installer. It typically describes in
#  some detail the purpose of the project during the installation. Not all
#  CPack generators uses this file.
#
# .. variable:: CPACK_RESOURCE_FILE_WELCOME
#
#  Welcome file to be embedded in the installer. It welcomes users to this
#  installer.  Typically used in the graphical installers on Windows and Mac
#  OS X.
#
# .. variable:: CPACK_MONOLITHIC_INSTALL
#
#  Disables the component-based installation mechanism. When set the
#  component specification is ignored and all installed items are put in a
#  single "MONOLITHIC" package.  Some CPack generators do monolithic
#  packaging by default and may be asked to do component packaging by
#  setting CPACK_<GENNAME>_COMPONENT_INSTALL to 1/TRUE.
#
# .. variable:: CPACK_GENERATOR
#
#  List of CPack generators to use. If not specified, CPack will create a
#  set of options CPACK_BINARY_<GENNAME> (e.g., CPACK_BINARY_NSIS) allowing
#  the user to enable/disable individual generators. This variable may be
#  used on the command line as well as in::
#
#   cpack -D CPACK_GENERATOR="ZIP;TGZ" /path/to/build/tree
#
# .. variable:: CPACK_OUTPUT_CONFIG_FILE
#
#  The name of the CPack binary configuration file. This file is the CPack
#  configuration generated by the CPack module for binary
#  installers. Defaults to CPackConfig.cmake.
#
# .. variable:: CPACK_PACKAGE_EXECUTABLES
#
#  Lists each of the executables and associated text label to be used to
#  create Start Menu shortcuts. For example, setting this to the list
#  ccmake;CMake will create a shortcut named "CMake" that will execute the
#  installed executable ccmake. Not all CPack generators use it (at least
#  NSIS, WIX and OSXX11 do).
#
# .. variable:: CPACK_STRIP_FILES
#
#  List of files to be stripped. Starting with CMake 2.6.0 CPACK_STRIP_FILES
#  will be a boolean variable which enables stripping of all files (a list
#  of files evaluates to TRUE in CMake, so this change is compatible).
#
# .. variable:: CPACK_VERBATIM_VARIABLES
#
#  If set to TRUE, values of variables prefixed with CPACK_ will be escaped
#  before being written to the configuration files, so that the cpack program
#  receives them exactly as they were specified. If not, characters like quotes
#  and backslashes can cause parsing errors or alter the value received by the
#  cpack program. Defaults to FALSE for backwards compatibility.
#
#  * Mandatory : NO
#  * Default   : FALSE
#
# The following CPack variables are specific to source packages, and
# will not affect binary packages:
#
# .. variable:: CPACK_SOURCE_PACKAGE_FILE_NAME
#
#  The name of the source package. For example cmake-2.6.1.
#
# .. variable:: CPACK_SOURCE_STRIP_FILES
#
#  List of files in the source tree that will be stripped. Starting with
#  CMake 2.6.0 CPACK_SOURCE_STRIP_FILES will be a boolean variable which
#  enables stripping of all files (a list of files evaluates to TRUE in
#  CMake, so this change is compatible).
#
# .. variable:: CPACK_SOURCE_GENERATOR
#
#  List of generators used for the source packages. As with CPACK_GENERATOR,
#  if this is not specified then CPack will create a set of options (e.g.,
#  CPACK_SOURCE_ZIP) allowing users to select which packages will be
#  generated.
#
# .. variable:: CPACK_SOURCE_OUTPUT_CONFIG_FILE
#
#  The name of the CPack source configuration file. This file is the CPack
#  configuration generated by the CPack module for source
#  installers. Defaults to CPackSourceConfig.cmake.
#
# .. variable:: CPACK_SOURCE_IGNORE_FILES
#
#  Pattern of files in the source tree that won't be packaged when building
#  a source package. This is a list of regular expression patterns (that
#  must be properly escaped), e.g.,
#  /CVS/;/\\.svn/;\\.swp$;\\.#;/#;.*~;cscope.*
#
# The following variables are for advanced uses of CPack:
#
# .. variable:: CPACK_CMAKE_GENERATOR
#
#  What CMake generator should be used if the project is CMake
#  project. Defaults to the value of CMAKE_GENERATOR few users will want to
#  change this setting.
#
# .. variable:: CPACK_INSTALL_CMAKE_PROJECTS
#
#  List of four values that specify what project to install. The four values
#  are: Build directory, Project Name, Project Component, Directory. If
#  omitted, CPack will build an installer that installs everything.
#
# .. variable:: CPACK_SYSTEM_NAME
#
#  System name, defaults to the value of ${CMAKE_SYSTEM_NAME}.
#
# .. variable:: CPACK_PACKAGE_VERSION
#
#  Package full version, used internally. By default, this is built from
#  CPACK_PACKAGE_VERSION_MAJOR, CPACK_PACKAGE_VERSION_MINOR, and
#  CPACK_PACKAGE_VERSION_PATCH.
#
# .. variable:: CPACK_TOPLEVEL_TAG
#
#  Directory for the installed files.
#
# .. variable:: CPACK_INSTALL_COMMANDS
#
#  Extra commands to install components.
#
# .. variable:: CPACK_INSTALLED_DIRECTORIES
#
#  Extra directories to install.
#
# .. variable:: CPACK_PACKAGE_INSTALL_REGISTRY_KEY
#
#  Registry key used when installing this project. This is only used by
#  installer for Windows.  The default value is based on the installation
#  directory.
#
# .. variable:: CPACK_CREATE_DESKTOP_LINKS
#
#  List of desktop links to create.
#  Each desktop link requires a corresponding start menu shortcut
#  as created by :variable:`CPACK_PACKAGE_EXECUTABLES`.

# Define this var in order to avoid (or warn) concerning multiple inclusion
if(CPack_CMake_INCLUDED)
  message(WARNING "CPack.cmake has already been included!!")
else()
  set(CPack_CMake_INCLUDED 1)
endif()

# Pick a configuration file
set(cpack_input_file "${CMAKE_ROOT}/Templates/CPackConfig.cmake.in")
if(EXISTS "${CMAKE_SOURCE_DIR}/CPackConfig.cmake.in")
  set(cpack_input_file "${CMAKE_SOURCE_DIR}/CPackConfig.cmake.in")
endif()
set(cpack_source_input_file "${CMAKE_ROOT}/Templates/CPackConfig.cmake.in")
if(EXISTS "${CMAKE_SOURCE_DIR}/CPackSourceConfig.cmake.in")
  set(cpack_source_input_file "${CMAKE_SOURCE_DIR}/CPackSourceConfig.cmake.in")
endif()

# Backward compatibility
# Include CPackComponent macros if it has not already been included before.
include(CPackComponent)

# Macro for setting values if a user did not overwrite them
# Mangles CMake-special characters. Only kept for backwards compatibility.
macro(cpack_set_if_not_set name value)
  message(DEPRECATION "cpack_set_if_not_set is obsolete; do not use.")
  _cpack_set_default("${name}" "${value}")
endmacro()

# cpack_encode_variables - Function to encode variables for the configuration file
# find any variable that starts with CPACK and create a variable
# _CPACK_OTHER_VARIABLES_ that contains SET commands for
# each cpack variable.  _CPACK_OTHER_VARIABLES_ is then
# used as an @ replacment in configure_file for the CPackConfig.
function(cpack_encode_variables)
  set(commands "")
  get_cmake_property(res VARIABLES)
  foreach(var ${res})
    if(var MATCHES "^CPACK")
      if(CPACK_VERBATIM_VARIABLES)
        _cpack_escape_for_cmake(value "${${var}}")
      else()
        set(value "${${var}}")
      endif()

      string(APPEND commands "\nSET(${var} \"${value}\")")
    endif()
  endforeach()

  set(_CPACK_OTHER_VARIABLES_ "${commands}" PARENT_SCOPE)
endfunction()

# Internal use functions
function(_cpack_set_default name value)
  if(NOT DEFINED "${name}")
    set("${name}" "${value}" PARENT_SCOPE)
  endif()
endfunction()

function(_cpack_escape_for_cmake var value)
  string(REGEX REPLACE "([\\\$\"])" "\\\\\\1" escaped "${value}")
  set("${var}" "${escaped}" PARENT_SCOPE)
endfunction()

# Set the package name
_cpack_set_default(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
_cpack_set_default(CPACK_PACKAGE_VERSION_MAJOR "0")
_cpack_set_default(CPACK_PACKAGE_VERSION_MINOR "1")
_cpack_set_default(CPACK_PACKAGE_VERSION_PATCH "1")
_cpack_set_default(CPACK_PACKAGE_VERSION
  "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
_cpack_set_default(CPACK_PACKAGE_VENDOR "Humanity")
if(CMAKE_PROJECT_DESCRIPTION)
  _cpack_set_default(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "${CMAKE_PROJECT_DESCRIPTION}")
else()
  _cpack_set_default(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "${CMAKE_PROJECT_NAME} built using CMake")
endif()

_cpack_set_default(CPACK_PACKAGE_DESCRIPTION_FILE
  "${CMAKE_ROOT}/Templates/CPack.GenericDescription.txt")
_cpack_set_default(CPACK_RESOURCE_FILE_LICENSE
  "${CMAKE_ROOT}/Templates/CPack.GenericLicense.txt")
_cpack_set_default(CPACK_RESOURCE_FILE_README
  "${CMAKE_ROOT}/Templates/CPack.GenericDescription.txt")
_cpack_set_default(CPACK_RESOURCE_FILE_WELCOME
  "${CMAKE_ROOT}/Templates/CPack.GenericWelcome.txt")

_cpack_set_default(CPACK_MODULE_PATH "${CMAKE_MODULE_PATH}")

if(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL)
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
endif()

if(CPACK_NSIS_MODIFY_PATH)
  set(CPACK_NSIS_MODIFY_PATH ON)
endif()

set(__cpack_system_name ${CMAKE_SYSTEM_NAME})
if(__cpack_system_name MATCHES "Windows")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(__cpack_system_name win64)
  else()
    set(__cpack_system_name win32)
  endif()
endif()
_cpack_set_default(CPACK_SYSTEM_NAME "${__cpack_system_name}")

# Root dir: default value should be the string literal "$PROGRAMFILES"
# for backwards compatibility. Projects may set this value to anything.
# When creating 64 bit binaries we set the default value to "$PROGRAMFILES64"
if("x${__cpack_system_name}" STREQUAL "xwin64")
  set(__cpack_root_default "$PROGRAMFILES64")
else()
  set(__cpack_root_default "$PROGRAMFILES")
endif()
_cpack_set_default(CPACK_NSIS_INSTALL_ROOT "${__cpack_root_default}")

# <project>-<major>.<minor>.<patch>-<release>-<platform>.<pkgtype>
_cpack_set_default(CPACK_PACKAGE_FILE_NAME
  "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
_cpack_set_default(CPACK_PACKAGE_INSTALL_DIRECTORY
  "${CPACK_PACKAGE_NAME} ${CPACK_PACKAGE_VERSION}")
_cpack_set_default(CPACK_PACKAGE_INSTALL_REGISTRY_KEY
  "${CPACK_PACKAGE_INSTALL_DIRECTORY}")
_cpack_set_default(CPACK_PACKAGE_DEFAULT_LOCATION "/")
_cpack_set_default(CPACK_PACKAGE_RELOCATABLE "true")

# always force to exactly "true" or "false" for CPack.Info.plist.in:
if(CPACK_PACKAGE_RELOCATABLE)
  set(CPACK_PACKAGE_RELOCATABLE "true")
else()
  set(CPACK_PACKAGE_RELOCATABLE "false")
endif()

macro(cpack_check_file_exists file description)
  if(NOT EXISTS "${file}")
    message(SEND_ERROR "CPack ${description} file: \"${file}\" could not be found.")
  endif()
endmacro()

cpack_check_file_exists("${CPACK_PACKAGE_DESCRIPTION_FILE}" "package description")
cpack_check_file_exists("${CPACK_RESOURCE_FILE_LICENSE}"    "license resource")
cpack_check_file_exists("${CPACK_RESOURCE_FILE_README}"     "readme resource")
cpack_check_file_exists("${CPACK_RESOURCE_FILE_WELCOME}"    "welcome resource")

macro(cpack_optional_append _list _cond _item)
  if(${_cond})
    set(${_list} ${${_list}} ${_item})
  endif()
endmacro()

#.rst:
# .. variable:: CPACK_BINARY_<GENNAME>
#
#  CPack generated options for binary generators. The CPack.cmake module
#  generates (when CPACK_GENERATOR is not set) a set of CMake options (see
#  CMake option command) which may then be used to select the CPack
#  generator(s) to be used when launching the package target.
#
#  Provide options to choose generators we might check here if the required
#  tools for the generates exist and set the defaults according to the results
if(NOT CPACK_GENERATOR)
  if(UNIX)
    if(CYGWIN)
      option(CPACK_BINARY_CYGWIN "Enable to build Cygwin binary packages" ON)
    else()
      if(APPLE)
        option(CPACK_BINARY_BUNDLE       "Enable to build OSX bundles"      OFF)
        option(CPACK_BINARY_DRAGNDROP    "Enable to build OSX Drag And Drop package" OFF)
        option(CPACK_BINARY_OSXX11       "Enable to build OSX X11 packages"      OFF)
        option(CPACK_BINARY_PACKAGEMAKER "Enable to build PackageMaker packages" OFF)
        option(CPACK_BINARY_PRODUCTBUILD "Enable to build productbuild packages" OFF)
      else()
        option(CPACK_BINARY_TZ  "Enable to build TZ packages"     ON)
      endif()
      option(CPACK_BINARY_DEB  "Enable to build Debian packages"  OFF)
      option(CPACK_BINARY_FREEBSD  "Enable to build FreeBSD packages"  OFF)
      option(CPACK_BINARY_NSIS "Enable to build NSIS packages"    OFF)
      option(CPACK_BINARY_RPM  "Enable to build RPM packages"     OFF)
      option(CPACK_BINARY_STGZ "Enable to build STGZ packages"    ON)
      option(CPACK_BINARY_TBZ2 "Enable to build TBZ2 packages"    OFF)
      option(CPACK_BINARY_TGZ  "Enable to build TGZ packages"     ON)
      option(CPACK_BINARY_TXZ  "Enable to build TXZ packages"     OFF)
    endif()
  else()
    option(CPACK_BINARY_7Z   "Enable to build 7-Zip packages" OFF)
    option(CPACK_BINARY_NSIS "Enable to build NSIS packages" ON)
    option(CPACK_BINARY_WIX  "Enable to build WiX packages" OFF)
    option(CPACK_BINARY_ZIP  "Enable to build ZIP packages" OFF)
  endif()
  option(CPACK_BINARY_IFW "Enable to build IFW packages" OFF)

  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_7Z           7Z)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_BUNDLE       Bundle)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_CYGWIN       CygwinBinary)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_DEB          DEB)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_DRAGNDROP    DragNDrop)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_FREEBSD      FREEBSD)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_IFW          IFW)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_NSIS         NSIS)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_OSXX11       OSXX11)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_PACKAGEMAKER PackageMaker)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_PRODUCTBUILD productbuild)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_RPM          RPM)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_STGZ         STGZ)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_TBZ2         TBZ2)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_TGZ          TGZ)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_TXZ          TXZ)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_TZ           TZ)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_WIX          WIX)
  cpack_optional_append(CPACK_GENERATOR  CPACK_BINARY_ZIP          ZIP)

endif()

# Provide options to choose source generators
if(NOT CPACK_SOURCE_GENERATOR)
  if(UNIX)
    if(CYGWIN)
      option(CPACK_SOURCE_CYGWIN "Enable to build Cygwin source packages" ON)
    else()
      option(CPACK_SOURCE_RPM  "Enable to build RPM source packages"  OFF)
      option(CPACK_SOURCE_TBZ2 "Enable to build TBZ2 source packages" ON)
      option(CPACK_SOURCE_TGZ  "Enable to build TGZ source packages"  ON)
      option(CPACK_SOURCE_TXZ  "Enable to build TXZ source packages"  ON)
      option(CPACK_SOURCE_TZ   "Enable to build TZ source packages"   ON)
      option(CPACK_SOURCE_ZIP  "Enable to build ZIP source packages"  OFF)
    endif()
  else()
    option(CPACK_SOURCE_7Z  "Enable to build 7-Zip source packages" ON)
    option(CPACK_SOURCE_ZIP "Enable to build ZIP source packages" ON)
  endif()

  cpack_optional_append(CPACK_SOURCE_GENERATOR  CPACK_SOURCE_7Z      7Z)
  cpack_optional_append(CPACK_SOURCE_GENERATOR  CPACK_SOURCE_CYGWIN  CygwinSource)
  cpack_optional_append(CPACK_SOURCE_GENERATOR  CPACK_SOURCE_RPM     RPM)
  cpack_optional_append(CPACK_SOURCE_GENERATOR  CPACK_SOURCE_TBZ2    TBZ2)
  cpack_optional_append(CPACK_SOURCE_GENERATOR  CPACK_SOURCE_TGZ     TGZ)
  cpack_optional_append(CPACK_SOURCE_GENERATOR  CPACK_SOURCE_TXZ     TXZ)
  cpack_optional_append(CPACK_SOURCE_GENERATOR  CPACK_SOURCE_TZ      TZ)
  cpack_optional_append(CPACK_SOURCE_GENERATOR  CPACK_SOURCE_ZIP     ZIP)
endif()

# mark the above options as advanced
mark_as_advanced(
  CPACK_BINARY_7Z
  CPACK_BINARY_BUNDLE
  CPACK_BINARY_CYGWIN
  CPACK_BINARY_DEB
  CPACK_BINARY_DRAGNDROP
  CPACK_BINARY_FREEBSD
  CPACK_BINARY_IFW
  CPACK_BINARY_NSIS
  CPACK_BINARY_OSXX11
  CPACK_BINARY_PACKAGEMAKER
  CPACK_BINARY_PRODUCTBUILD
  CPACK_BINARY_RPM
  CPACK_BINARY_STGZ
  CPACK_BINARY_TBZ2
  CPACK_BINARY_TGZ
  CPACK_BINARY_TXZ
  CPACK_BINARY_TZ
  CPACK_BINARY_WIX
  CPACK_BINARY_ZIP
  CPACK_SOURCE_7Z
  CPACK_SOURCE_CYGWIN
  CPACK_SOURCE_RPM
  CPACK_SOURCE_TBZ2
  CPACK_SOURCE_TGZ
  CPACK_SOURCE_TXZ
  CPACK_SOURCE_TZ
  CPACK_SOURCE_ZIP
  )

# Set some other variables
_cpack_set_default(CPACK_INSTALL_CMAKE_PROJECTS
  "${CMAKE_BINARY_DIR};${CMAKE_PROJECT_NAME};ALL;/")
_cpack_set_default(CPACK_CMAKE_GENERATOR "${CMAKE_GENERATOR}")
_cpack_set_default(CPACK_TOPLEVEL_TAG "${CPACK_SYSTEM_NAME}")
# if the user has set CPACK_NSIS_DISPLAY_NAME remember it
if(DEFINED CPACK_NSIS_DISPLAY_NAME)
  set(CPACK_NSIS_DISPLAY_NAME_SET TRUE)
endif()
# if the user has set CPACK_NSIS_DISPLAY
# explicitly, then use that as the default
# value of CPACK_NSIS_PACKAGE_NAME  instead
# of CPACK_PACKAGE_INSTALL_DIRECTORY
_cpack_set_default(CPACK_NSIS_DISPLAY_NAME "${CPACK_PACKAGE_INSTALL_DIRECTORY}")

if(CPACK_NSIS_DISPLAY_NAME_SET)
  _cpack_set_default(CPACK_NSIS_PACKAGE_NAME "${CPACK_NSIS_DISPLAY_NAME}")
else()
  _cpack_set_default(CPACK_NSIS_PACKAGE_NAME "${CPACK_PACKAGE_INSTALL_DIRECTORY}")
endif()

_cpack_set_default(CPACK_OUTPUT_CONFIG_FILE
  "${CMAKE_BINARY_DIR}/CPackConfig.cmake")

_cpack_set_default(CPACK_SOURCE_OUTPUT_CONFIG_FILE
  "${CMAKE_BINARY_DIR}/CPackSourceConfig.cmake")

_cpack_set_default(CPACK_SET_DESTDIR OFF)
_cpack_set_default(CPACK_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

_cpack_set_default(CPACK_NSIS_INSTALLER_ICON_CODE "")
_cpack_set_default(CPACK_NSIS_INSTALLER_MUI_ICON_CODE "")

# WiX specific variables
_cpack_set_default(CPACK_WIX_SIZEOF_VOID_P "${CMAKE_SIZEOF_VOID_P}")

# set sysroot so SDK tools can be used
if(CMAKE_OSX_SYSROOT)
  _cpack_set_default(CPACK_OSX_SYSROOT "${_CMAKE_OSX_SYSROOT_PATH}")
endif()

_cpack_set_default(CPACK_BUILD_SOURCE_DIRS "${CMAKE_SOURCE_DIR};${CMAKE_BINARY_DIR}")

if(DEFINED CPACK_COMPONENTS_ALL)
  if(CPACK_MONOLITHIC_INSTALL)
    message("CPack warning: both CPACK_COMPONENTS_ALL and CPACK_MONOLITHIC_INSTALL have been set.\nDefaulting to a monolithic installation.")
    set(CPACK_COMPONENTS_ALL)
  else()
    # The user has provided the set of components to be installed as
    # part of a component-based installation; trust her.
    set(CPACK_COMPONENTS_ALL_SET_BY_USER TRUE)
  endif()
else()
  # If the user has not specifically requested a monolithic installer
  # but has specified components in various "install" commands, tell
  # CPack about those components.
  if(NOT CPACK_MONOLITHIC_INSTALL)
    get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
    list(LENGTH CPACK_COMPONENTS_ALL CPACK_COMPONENTS_LEN)
    if(CPACK_COMPONENTS_LEN EQUAL 1)
      # Only one component: this is not a component-based installation
      # (at least, it isn't a component-based installation, but may
      # become one later if the user uses the cpack_add_* commands).
      set(CPACK_COMPONENTS_ALL)
    endif()
    set(CPACK_COMPONENTS_LEN)
  endif()
endif()

# CMake always generates a component named "Unspecified", which is
# used to install everything that doesn't have an explicitly-provided
# component. Since these files should always be installed, we'll make
# them hidden and required.
set(CPACK_COMPONENT_UNSPECIFIED_HIDDEN TRUE)
set(CPACK_COMPONENT_UNSPECIFIED_REQUIRED TRUE)

cpack_encode_variables()
configure_file("${cpack_input_file}" "${CPACK_OUTPUT_CONFIG_FILE}" @ONLY)

# Generate source file
_cpack_set_default(CPACK_SOURCE_INSTALLED_DIRECTORIES
  "${CMAKE_SOURCE_DIR};/")
_cpack_set_default(CPACK_SOURCE_TOPLEVEL_TAG "${CPACK_SYSTEM_NAME}-Source")
_cpack_set_default(CPACK_SOURCE_PACKAGE_FILE_NAME
  "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-Source")

set(__cpack_source_ignore_files_default
  "/CVS/;/\\.svn/;/\\.bzr/;/\\.hg/;/\\.git/;\\.swp$;\\.#;/#")
if(NOT CPACK_VERBATIM_VARIABLES)
  _cpack_escape_for_cmake(__cpack_source_ignore_files_default
    "${__cpack_source_ignore_files_default}")
endif()
_cpack_set_default(CPACK_SOURCE_IGNORE_FILES "${__cpack_source_ignore_files_default}")

set(CPACK_INSTALL_CMAKE_PROJECTS "${CPACK_SOURCE_INSTALL_CMAKE_PROJECTS}")
set(CPACK_INSTALLED_DIRECTORIES "${CPACK_SOURCE_INSTALLED_DIRECTORIES}")
set(CPACK_GENERATOR "${CPACK_SOURCE_GENERATOR}")
set(CPACK_TOPLEVEL_TAG "${CPACK_SOURCE_TOPLEVEL_TAG}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_SOURCE_PACKAGE_FILE_NAME}")
set(CPACK_IGNORE_FILES "${CPACK_SOURCE_IGNORE_FILES}")
set(CPACK_STRIP_FILES "${CPACK_SOURCE_STRIP_FILES}")

set(CPACK_RPM_PACKAGE_SOURCES "ON")

cpack_encode_variables()
configure_file("${cpack_source_input_file}"
  "${CPACK_SOURCE_OUTPUT_CONFIG_FILE}" @ONLY)
