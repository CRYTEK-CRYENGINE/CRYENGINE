# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CPackIFW
# --------
#
# .. _QtIFW: http://doc.qt.io/qtinstallerframework/index.html
#
# This module looks for the location of the command line utilities supplied
# with the Qt Installer Framework (QtIFW_).
#
# The module also defines several commands to control the behavior of the
# CPack ``IFW`` generator.
#
#
# Overview
# ^^^^^^^^
#
# CPack ``IFW`` generator helps you to create online and offline
# binary cross-platform installers with a graphical user interface.
#
# CPack IFW generator prepares project installation and generates configuration
# and meta information for QtIFW_ tools.
#
# The QtIFW_ provides a set of tools and utilities to create
# installers for the supported desktop Qt platforms: Linux, Microsoft Windows,
# and Mac OS X.
#
# You should also install QtIFW_ to use CPack ``IFW`` generator.
#
# Hints
# ^^^^^
#
# Generally, the CPack ``IFW`` generator automatically finds QtIFW_ tools,
# but if you don't use a default path for installation of the QtIFW_ tools,
# the path may be specified in either a CMake or an environment variable:
#
# .. variable:: CPACK_IFW_ROOT
#
#  An CMake variable which specifies the location of the QtIFW_ tool suite.
#
#  The variable will be cached in the ``CPackConfig.cmake`` file and used at
#  CPack runtime.
#
# .. variable:: QTIFWDIR
#
#  An environment variable which specifies the location of the QtIFW_ tool
#  suite.
#
# .. note::
#   The specified path should not contain "bin" at the end
#   (for example: "D:\\DevTools\\QtIFW2.0.5").
#
# The :variable:`CPACK_IFW_ROOT` variable has a higher priority and overrides
# the value of the :variable:`QTIFWDIR` variable.
#
# Internationalization
# ^^^^^^^^^^^^^^^^^^^^
#
# Some variables and command arguments support internationalization via
# CMake script. This is an optional feature.
#
# Installers created by QtIFW_ tools have built-in support for
# internationalization and many phrases are localized to many languages,
# but this does not apply to the description of the your components and groups
# that will be distributed.
#
# Localization of the description of your components and groups is useful for
# users of your installers.
#
# A localized variable or argument can contain a single default value, and a
# set of pairs the name of the locale and the localized value.
#
# For example:
#
# .. code-block:: cmake
#
#    set(LOCALIZABLE_VARIABLE "Default value"
#      en "English value"
#      en_US "American value"
#      en_GB "Great Britain value"
#      )
#
# Variables
# ^^^^^^^^^
#
# You can use the following variables to change behavior of CPack ``IFW``
# generator.
#
# Debug
# """"""
#
# .. variable:: CPACK_IFW_VERBOSE
#
#  Set to ``ON`` to enable addition debug output.
#  By default is ``OFF``.
#
# Package
# """""""
#
# .. variable:: CPACK_IFW_PACKAGE_TITLE
#
#  Name of the installer as displayed on the title bar.
#  By default used :variable:`CPACK_PACKAGE_DESCRIPTION_SUMMARY`.
#
# .. variable:: CPACK_IFW_PACKAGE_PUBLISHER
#
#  Publisher of the software (as shown in the Windows Control Panel).
#  By default used :variable:`CPACK_PACKAGE_VENDOR`.
#
# .. variable:: CPACK_IFW_PRODUCT_URL
#
#  URL to a page that contains product information on your web site.
#
# .. variable:: CPACK_IFW_PACKAGE_ICON
#
#  Filename for a custom installer icon. The actual file is '.icns' (Mac OS X),
#  '.ico' (Windows). No functionality on Unix.
#
# .. variable:: CPACK_IFW_PACKAGE_WINDOW_ICON
#
#  Filename for a custom window icon in PNG format for the Installer
#  application.
#
# .. variable:: CPACK_IFW_PACKAGE_LOGO
#
#  Filename for a logo is used as QWizard::LogoPixmap.
#
# .. variable:: CPACK_IFW_PACKAGE_WATERMARK
#
#  Filename for a watermark is used as QWizard::WatermarkPixmap.
#
# .. variable:: CPACK_IFW_PACKAGE_BANNER
#
#  Filename for a banner is used as QWizard::BannerPixmap.
#
# .. variable:: CPACK_IFW_PACKAGE_BACKGROUND
#
#  Filename for an image used as QWizard::BackgroundPixmap (only used by MacStyle).
#
# .. variable:: CPACK_IFW_PACKAGE_WIZARD_STYLE
#
#  Wizard style to be used ("Modern", "Mac", "Aero" or "Classic").
#
# .. variable:: CPACK_IFW_PACKAGE_WIZARD_DEFAULT_WIDTH
#
#  Default width of the wizard in pixels. Setting a banner image will override this.
#
# .. variable:: CPACK_IFW_PACKAGE_WIZARD_DEFAULT_HEIGHT
#
#  Default height of the wizard in pixels. Setting a watermark image will override this.
#
# .. variable:: CPACK_IFW_PACKAGE_TITLE_COLOR
#
#  Color of the titles and subtitles (takes an HTML color code, such as "#88FF33").
#
# .. variable:: CPACK_IFW_PACKAGE_START_MENU_DIRECTORY
#
#  Name of the default program group for the product in the Windows Start menu.
#
#  By default used :variable:`CPACK_IFW_PACKAGE_NAME`.
#
# .. variable:: CPACK_IFW_TARGET_DIRECTORY
#
#  Default target directory for installation.
#  By default used
#  "@ApplicationsDir@/:variable:`CPACK_PACKAGE_INSTALL_DIRECTORY`"
#
#  You can use predefined variables.
#
# .. variable:: CPACK_IFW_ADMIN_TARGET_DIRECTORY
#
#  Default target directory for installation with administrator rights.
#
#  You can use predefined variables.
#
# .. variable:: CPACK_IFW_PACKAGE_GROUP
#
#  The group, which will be used to configure the root package
#
# .. variable:: CPACK_IFW_PACKAGE_NAME
#
#  The root package name, which will be used if configuration group is not
#  specified
#
# .. variable:: CPACK_IFW_PACKAGE_MAINTENANCE_TOOL_NAME
#
#  Filename of the generated maintenance tool.
#  The platform-specific executable file extension is appended.
#
#  By default used QtIFW_ defaults (``maintenancetool``).
#
# .. variable:: CPACK_IFW_PACKAGE_MAINTENANCE_TOOL_INI_FILE
#
#  Filename for the configuration of the generated maintenance tool.
#
#  By default used QtIFW_ defaults (``maintenancetool.ini``).
#
# .. variable:: CPACK_IFW_PACKAGE_ALLOW_NON_ASCII_CHARACTERS
#
#  Set to ``ON`` if the installation path can contain non-ASCII characters.
#
#  Is ``ON`` for QtIFW_ less 2.0 tools.
#
# .. variable:: CPACK_IFW_PACKAGE_ALLOW_SPACE_IN_PATH
#
#  Set to ``OFF`` if the installation path cannot contain space characters.
#
#  Is ``ON`` for QtIFW_ less 2.0 tools.
#
# .. variable:: CPACK_IFW_PACKAGE_CONTROL_SCRIPT
#
#  Filename for a custom installer control script.
#
# .. variable:: CPACK_IFW_PACKAGE_RESOURCES
#
#  List of additional resources ('.qrc' files) to include in the installer
#  binary.
#
#  You can use :command:`cpack_ifw_add_package_resources` command to resolve
#  relative paths.
#
# .. variable:: CPACK_IFW_PACKAGE_FILE_EXTENSION
#
#  The target binary extension.
#
#  On Linux, the name of the target binary is automatically extended with
#  '.run', if you do not specify the extension.
#
#  On Windows, the target is created as an application with the extension
#  '.exe', which is automatically added, if not supplied.
#
#  On Mac, the target is created as an DMG disk image with the extension
#  '.dmg', which is automatically added, if not supplied.
#
# .. variable:: CPACK_IFW_REPOSITORIES_ALL
#
#  The list of remote repositories.
#
#  The default value of this variable is computed by CPack and contains
#  all repositories added with command :command:`cpack_ifw_add_repository`
#  or updated with command :command:`cpack_ifw_update_repository`.
#
# .. variable:: CPACK_IFW_DOWNLOAD_ALL
#
#  If this is ``ON`` all components will be downloaded.
#  By default is ``OFF`` or used value
#  from ``CPACK_DOWNLOAD_ALL`` if set
#
# Components
# """"""""""
#
# .. variable:: CPACK_IFW_RESOLVE_DUPLICATE_NAMES
#
#  Resolve duplicate names when installing components with groups.
#
# .. variable:: CPACK_IFW_PACKAGES_DIRECTORIES
#
#  Additional prepared packages dirs that will be used to resolve
#  dependent components.
#
# .. variable:: CPACK_IFW_REPOSITORIES_DIRECTORIES
#
#  Additional prepared repository dirs that will be used to resolve and
#  repack dependent components. This feature available only
#  since QtIFW_ 3.1.
#
# Tools
# """""
#
# .. variable:: CPACK_IFW_FRAMEWORK_VERSION
#
#  The version of used QtIFW_ tools.
#
# .. variable:: CPACK_IFW_BINARYCREATOR_EXECUTABLE
#
#  The path to "binarycreator" command line client.
#
#  This variable is cached and may be configured if needed.
#
# .. variable:: CPACK_IFW_REPOGEN_EXECUTABLE
#
#  The path to "repogen" command line client.
#
#  This variable is cached and may be configured if needed.
#
# .. variable:: CPACK_IFW_INSTALLERBASE_EXECUTABLE
#
#  The path to "installerbase" installer executable base.
#
#  This variable is cached and may be configured if needed.
#
# .. variable:: CPACK_IFW_DEVTOOL_EXECUTABLE
#
#  The path to "devtool" command line client.
#
#  This variable is cached and may be configured if needed.
#
# Commands
# ^^^^^^^^^
#
# The module defines the following commands:
#
# .. command:: cpack_ifw_configure_component
#
#   Sets the arguments specific to the CPack IFW generator.
#
#   ::
#
#     cpack_ifw_configure_component(<compname> [COMMON] [ESSENTIAL] [VIRTUAL]
#                         [FORCED_INSTALLATION] [REQUIRES_ADMIN_RIGHTS]
#                         [NAME <name>]
#                         [DISPLAY_NAME <display_name>] # Note: Internationalization supported
#                         [DESCRIPTION <description>] # Note: Internationalization supported
#                         [UPDATE_TEXT <update_text>]
#                         [VERSION <version>]
#                         [RELEASE_DATE <release_date>]
#                         [SCRIPT <script>]
#                         [PRIORITY|SORTING_PRIORITY <sorting_priority>] # Note: PRIORITY is deprecated
#                         [DEPENDS|DEPENDENCIES <com_id> ...]
#                         [AUTO_DEPEND_ON <comp_id> ...]
#                         [LICENSES <display_name> <file_path> ...]
#                         [DEFAULT <value>]
#                         [USER_INTERFACES <file_path> <file_path> ...]
#                         [TRANSLATIONS <file_path> <file_path> ...]
#                         [REPLACES <comp_id> ...]
#                         [CHECKABLE <value>])
#
#   This command should be called after :command:`cpack_add_component` command.
#
#   ``COMMON``
#     if set, then the component will be packaged and installed as part
#     of a group to which it belongs.
#
#   ``ESSENTIAL``
#     if set, then the package manager stays disabled until that
#     component is updated.
#
#   ``VIRTUAL``
#     if set, then the component will be hidden from the installer.
#     It is a equivalent of the ``HIDDEN`` option from the
#     :command:`cpack_add_component` command.
#
#   ``FORCED_INSTALLATION``
#     if set, then the component must always be installed.
#     It is a equivalent of the ``REQUARED`` option from the
#     :command:`cpack_add_component` command.
#
#   ``REQUIRES_ADMIN_RIGHTS``
#     set it if the component needs to be installed with elevated permissions.
#
#   ``NAME``
#     is used to create domain-like identification for this component.
#     By default used origin component name.
#
#   ``DISPLAY_NAME``
#     set to rewrite original name configured by
#     :command:`cpack_add_component` command.
#
#   ``DESCRIPTION``
#     set to rewrite original description configured by
#     :command:`cpack_add_component` command.
#
#   ``UPDATE_TEXT``
#     will be added to the component description if this is an update to
#     the component.
#
#   ``VERSION``
#     is version of component.
#     By default used :variable:`CPACK_PACKAGE_VERSION`.
#
#   ``RELEASE_DATE``
#     keep empty to auto generate.
#
#   ``SCRIPT``
#     is a relative or absolute path to operations script
#     for this component.
#
#   ``PRIORITY`` | ``SORTING_PRIORITY``
#     is priority of the component in the tree.
#     The ``PRIORITY`` option is deprecated and will be removed in a future
#     version of CMake. Please use ``SORTING_PRIORITY`` option instead.
#
#   ``DEPENDS`` | ``DEPENDENCIES``
#     list of dependency component or component group identifiers in
#     QtIFW_ style.
#
#   ``AUTO_DEPEND_ON``
#     list of identifiers of component or component group in QtIFW_ style
#     that this component has an automatic dependency on.
#
#   ``LICENSES``
#     pair of <display_name> and <file_path> of license text for this
#     component. You can specify more then one license.
#
#   ``DEFAULT``
#     Possible values are: TRUE, FALSE, and SCRIPT.
#     Set to FALSE to disable the component in the installer or to SCRIPT
#     to resolved during runtime (don't forget add the file of the script
#     as a value of the ``SCRIPT`` option).
#
#   ``USER_INTERFACES``
#     is a list of <file_path> ('.ui' files) representing pages to load.
#
#   ``TRANSLATIONS``
#     is a list of <file_path> ('.qm' files) representing translations to load.
#
#   ``REPLACES``
#     list of identifiers of component or component group to replace.
#
#   ``CHECKABLE``
#     Possible values are: TRUE, FALSE.
#     Set to FALSE if you want to hide the checkbox for an item.
#     This is useful when only a few subcomponents should be selected
#     instead of all.
#
#
# .. command:: cpack_ifw_configure_component_group
#
#   Sets the arguments specific to the CPack IFW generator.
#
#   ::
#
#     cpack_ifw_configure_component_group(<groupname> [VIRTUAL]
#                         [FORCED_INSTALLATION] [REQUIRES_ADMIN_RIGHTS]
#                         [NAME <name>]
#                         [DISPLAY_NAME <display_name>] # Note: Internationalization supported
#                         [DESCRIPTION <description>] # Note: Internationalization supported
#                         [UPDATE_TEXT <update_text>]
#                         [VERSION <version>]
#                         [RELEASE_DATE <release_date>]
#                         [SCRIPT <script>]
#                         [PRIORITY|SORTING_PRIORITY <sorting_priority>] # Note: PRIORITY is deprecated
#                         [DEPENDS|DEPENDENCIES <com_id> ...]
#                         [AUTO_DEPEND_ON <comp_id> ...]
#                         [LICENSES <display_name> <file_path> ...]
#                         [DEFAULT <value>]
#                         [USER_INTERFACES <file_path> <file_path> ...]
#                         [TRANSLATIONS <file_path> <file_path> ...]
#                         [REPLACES <comp_id> ...]
#                         [CHECKABLE <value>])
#
#   This command should be called after :command:`cpack_add_component_group`
#   command.
#
#   ``VIRTUAL``
#     if set, then the group will be hidden from the installer.
#     Note that setting this on a root component does not work.
#
#   ``FORCED_INSTALLATION``
#     if set, then the group must always be installed.
#
#   ``REQUIRES_ADMIN_RIGHTS``
#     set it if the component group needs to be installed with elevated
#     permissions.
#
#   ``NAME``
#     is used to create domain-like identification for this component group.
#     By default used origin component group name.
#
#   ``DISPLAY_NAME``
#     set to rewrite original name configured by
#     :command:`cpack_add_component_group` command.
#
#   ``DESCRIPTION``
#     set to rewrite original description configured by
#     :command:`cpack_add_component_group` command.
#
#   ``UPDATE_TEXT``
#     will be added to the component group description if this is an update to
#     the component group.
#
#   ``VERSION``
#     is version of component group.
#     By default used :variable:`CPACK_PACKAGE_VERSION`.
#
#   ``RELEASE_DATE``
#     keep empty to auto generate.
#
#   ``SCRIPT``
#     is a relative or absolute path to operations script
#     for this component group.
#
#   ``PRIORITY`` | ``SORTING_PRIORITY``
#     is priority of the component group in the tree.
#     The ``PRIORITY`` option is deprecated and will be removed in a future
#     version of CMake. Please use ``SORTING_PRIORITY`` option instead.
#
#   ``DEPENDS`` | ``DEPENDENCIES``
#     list of dependency component or component group identifiers in
#     QtIFW_ style.
#
#   ``AUTO_DEPEND_ON``
#     list of identifiers of component or component group in QtIFW_ style
#     that this component group has an automatic dependency on.
#
#   ``LICENSES``
#     pair of <display_name> and <file_path> of license text for this
#     component group. You can specify more then one license.
#
#   ``DEFAULT``
#     Possible values are: TRUE, FALSE, and SCRIPT.
#     Set to TRUE to preselect the group in the installer
#     (this takes effect only on groups that have no visible child components)
#     or to SCRIPT to resolved during runtime (don't forget add the file of
#     the script as a value of the ``SCRIPT`` option).
#
#   ``USER_INTERFACES``
#     is a list of <file_path> ('.ui' files) representing pages to load.
#
#   ``TRANSLATIONS``
#     is a list of <file_path> ('.qm' files) representing translations to load.
#
#   ``REPLACES``
#     list of identifiers of component or component group to replace.
#
#   ``CHECKABLE``
#     Possible values are: TRUE, FALSE.
#     Set to FALSE if you want to hide the checkbox for an item.
#     This is useful when only a few subcomponents should be selected
#     instead of all.
#
#
# .. command:: cpack_ifw_add_repository
#
#   Add QtIFW_ specific remote repository to binary installer.
#
#   ::
#
#     cpack_ifw_add_repository(<reponame> [DISABLED]
#                         URL <url>
#                         [USERNAME <username>]
#                         [PASSWORD <password>]
#                         [DISPLAY_NAME <display_name>])
#
#   This command will also add the <reponame> repository
#   to a variable :variable:`CPACK_IFW_REPOSITORIES_ALL`.
#
#   ``DISABLED``
#     if set, then the repository will be disabled by default.
#
#   ``URL``
#     is points to a list of available components.
#
#   ``USERNAME``
#     is used as user on a protected repository.
#
#   ``PASSWORD``
#     is password to use on a protected repository.
#
#   ``DISPLAY_NAME``
#     is string to display instead of the URL.
#
#
# .. command:: cpack_ifw_update_repository
#
#   Update QtIFW_ specific repository from remote repository.
#
#   ::
#
#     cpack_ifw_update_repository(<reponame>
#                         [[ADD|REMOVE] URL <url>]|
#                          [REPLACE OLD_URL <old_url> NEW_URL <new_url>]]
#                         [USERNAME <username>]
#                         [PASSWORD <password>]
#                         [DISPLAY_NAME <display_name>])
#
#   This command will also add the <reponame> repository
#   to a variable :variable:`CPACK_IFW_REPOSITORIES_ALL`.
#
#   ``URL``
#     is points to a list of available components.
#
#   ``OLD_URL``
#     is points to a list that will replaced.
#
#   ``NEW_URL``
#     is points to a list that will replace to.
#
#   ``USERNAME``
#     is used as user on a protected repository.
#
#   ``PASSWORD``
#     is password to use on a protected repository.
#
#   ``DISPLAY_NAME``
#     is string to display instead of the URL.
#
#
# .. command:: cpack_ifw_add_package_resources
#
#   Add additional resources in the installer binary.
#
#   ::
#
#     cpack_ifw_add_package_resources(<file_path> <file_path> ...)
#
#   This command will also add the specified files
#   to a variable :variable:`CPACK_IFW_PACKAGE_RESOURCES`.
#
#
# Example usage
# ^^^^^^^^^^^^^
#
# .. code-block:: cmake
#
#    set(CPACK_PACKAGE_NAME "MyPackage")
#    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MyPackage Installation Example")
#    set(CPACK_PACKAGE_VERSION "1.0.0") # Version of installer
#
#    include(CPack)
#    include(CPackIFW)
#
#    cpack_add_component(myapp
#        DISPLAY_NAME "MyApp"
#        DESCRIPTION "My Application") # Default description
#    cpack_ifw_configure_component(myapp
#        DESCRIPTION ru_RU "Мое Приложение" # Localized description
#        VERSION "1.2.3" # Version of component
#        SCRIPT "operations.qs")
#    cpack_add_component(mybigplugin
#        DISPLAY_NAME "MyBigPlugin"
#        DESCRIPTION "My Big Downloadable Plugin"
#        DOWNLOADED)
#    cpack_ifw_add_repository(myrepo
#        URL "http://example.com/ifw/repo/myapp"
#        DISPLAY_NAME "My Application Repository")
#
#
# Online installer
# ^^^^^^^^^^^^^^^^
#
# By default CPack IFW generator makes offline installer. This means that all
# components will be packaged into a binary file.
#
# To make a component downloaded, you must set the ``DOWNLOADED`` option in
# :command:`cpack_add_component`.
#
# Then you would use the command :command:`cpack_configure_downloads`.
# If you set ``ALL`` option all components will be downloaded.
#
# You also can use command :command:`cpack_ifw_add_repository` and
# variable :variable:`CPACK_IFW_DOWNLOAD_ALL` for more specific configuration.
#
# CPack IFW generator creates "repository" dir in current binary dir. You
# would copy content of this dir to specified ``site`` (``url``).
#
# See Also
# ^^^^^^^^
#
# Qt Installer Framework Manual:
#
# * Index page:
#   http://doc.qt.io/qtinstallerframework/index.html
#
# * Component Scripting:
#   http://doc.qt.io/qtinstallerframework/scripting.html
#
# * Predefined Variables:
#   http://doc.qt.io/qtinstallerframework/scripting.html#predefined-variables
#
# * Promoting Updates:
#   http://doc.qt.io/qtinstallerframework/ifw-updates.html
#
# Download Qt Installer Framework for you platform from Qt site:
#  http://download.qt.io/official_releases/qt-installer-framework
#

#=============================================================================
# Search Qt Installer Framework tools
#=============================================================================

# Default path

foreach(_CPACK_IFW_PATH_VAR "CPACK_IFW_ROOT" "QTIFWDIR" "QTDIR")
  if(DEFINED ${_CPACK_IFW_PATH_VAR}
    AND NOT "${${_CPACK_IFW_PATH_VAR}}" STREQUAL "")
    list(APPEND _CPACK_IFW_PATHS "${${_CPACK_IFW_PATH_VAR}}")
  endif()
  if(NOT "$ENV{${_CPACK_IFW_PATH_VAR}}" STREQUAL "")
    list(APPEND _CPACK_IFW_PATHS "$ENV{${_CPACK_IFW_PATH_VAR}}")
  endif()
endforeach()
if(WIN32)
  list(APPEND _CPACK_IFW_PATHS
    "$ENV{HOMEDRIVE}/Qt"
    "C:/Qt")
else()
  list(APPEND _CPACK_IFW_PATHS
    "$ENV{HOME}/Qt"
    "/opt/Qt")
endif()
list(REMOVE_DUPLICATES _CPACK_IFW_PATHS)

set(_CPACK_IFW_PREFIXES
  # QtSDK
  "Tools/QtInstallerFramework/"
  # Second branch
  "QtIFW"
  # First branch
  "QtIFW-")

set(_CPACK_IFW_VERSIONS
  "3.1"
  "3.1.0"
  "3.0"
  "3.0.0"
  "2.3"
  "2.3.0"
  "2.2"
  "2.2.0"
  "2.1"
  "2.1.0"
  "2.0"
  "2.0.5"
  "2.0.3"
  "2.0.2"
  "2.0.1"
  "2.0.0"
  "1.6"
  "1.6.0"
  "1.5"
  "1.5.0"
  "1.4"
  "1.4.0"
  "1.3"
  "1.3.0")

set(_CPACK_IFW_SUFFIXES "bin")
foreach(_CPACK_IFW_PREFIX ${_CPACK_IFW_PREFIXES})
  foreach(_CPACK_IFW_VERSION ${_CPACK_IFW_VERSIONS})
    list(APPEND
      _CPACK_IFW_SUFFIXES "${_CPACK_IFW_PREFIX}${_CPACK_IFW_VERSION}/bin")
  endforeach()
endforeach()

# Look for 'binarycreator'

find_program(CPACK_IFW_BINARYCREATOR_EXECUTABLE
  NAMES binarycreator
  PATHS ${_CPACK_IFW_PATHS}
  PATH_SUFFIXES ${_CPACK_IFW_SUFFIXES}
  DOC "QtIFW binarycreator command line client")

mark_as_advanced(CPACK_IFW_BINARYCREATOR_EXECUTABLE)

# Look for 'repogen'

find_program(CPACK_IFW_REPOGEN_EXECUTABLE
  NAMES repogen
  PATHS ${_CPACK_IFW_PATHS}
  PATH_SUFFIXES ${_CPACK_IFW_SUFFIXES}
  DOC "QtIFW repogen command line client"
  )
mark_as_advanced(CPACK_IFW_REPOGEN_EXECUTABLE)

# Look for 'installerbase'

find_program(CPACK_IFW_INSTALLERBASE_EXECUTABLE
  NAMES installerbase
  PATHS ${_CPACK_IFW_PATHS}
  PATH_SUFFIXES ${_CPACK_IFW_SUFFIXES}
  DOC "QtIFW installer executable base"
  )
mark_as_advanced(CPACK_IFW_INSTALLERBASE_EXECUTABLE)

# Look for 'devtool' (appeared in the second branch)

find_program(CPACK_IFW_DEVTOOL_EXECUTABLE
  NAMES devtool
  PATHS ${_CPACK_IFW_PATHS}
  PATH_SUFFIXES ${_CPACK_IFW_SUFFIXES}
  DOC "QtIFW devtool command line client"
  )
mark_as_advanced(CPACK_IFW_DEVTOOL_EXECUTABLE)

#
## Next code is included only once
#

if(NOT CPackIFW_CMake_INCLUDED)
set(CPackIFW_CMake_INCLUDED 1)

#=============================================================================
# Framework version
#=============================================================================

set(CPACK_IFW_FRAMEWORK_VERSION_FORCED ""
  CACHE STRING "The forced version of used QtIFW tools")
mark_as_advanced(CPACK_IFW_FRAMEWORK_VERSION_FORCED)
set(CPACK_IFW_FRAMEWORK_VERSION_TIMEOUT 1
  CACHE STRING "The timeout to return QtIFW framework version string from \"installerbase\" executable")
mark_as_advanced(CPACK_IFW_FRAMEWORK_VERSION_TIMEOUT)
if(CPACK_IFW_INSTALLERBASE_EXECUTABLE AND NOT CPACK_IFW_FRAMEWORK_VERSION_FORCED)
  set(CPACK_IFW_FRAMEWORK_VERSION)
  # Invoke version from "installerbase" executable
  foreach(_ifw_version_argument --framework-version --version)
    if(NOT CPACK_IFW_FRAMEWORK_VERSION)
      execute_process(COMMAND
        "${CPACK_IFW_INSTALLERBASE_EXECUTABLE}" ${_ifw_version_argument}
        TIMEOUT ${CPACK_IFW_FRAMEWORK_VERSION_TIMEOUT}
        RESULT_VARIABLE CPACK_IFW_FRAMEWORK_VERSION_RESULT
        OUTPUT_VARIABLE CPACK_IFW_FRAMEWORK_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ENCODING UTF8)
      if(NOT CPACK_IFW_FRAMEWORK_VERSION_RESULT AND CPACK_IFW_FRAMEWORK_VERSION_OUTPUT)
        string(REGEX MATCH "[0-9]+(\\.[0-9]+)*"
          CPACK_IFW_FRAMEWORK_VERSION "${CPACK_IFW_FRAMEWORK_VERSION_OUTPUT}")
        if(CPACK_IFW_FRAMEWORK_VERSION)
          if("${_ifw_version_argument}" STREQUAL "--framework-version")
            set(CPACK_IFW_FRAMEWORK_VERSION_SOURCE "INSTALLERBASE_FRAMEWORK_VERSION")
          elseif("${_ifw_version_argument}" STREQUAL "--version")
            set(CPACK_IFW_FRAMEWORK_VERSION_SOURCE "INSTALLERBASE_FRAMEWORK_VERSION")
          endif()
        endif()
      endif()
    endif()
  endforeach()
  # Finaly try to get version from executable path
  if(NOT CPACK_IFW_FRAMEWORK_VERSION)
    string(REGEX MATCH "[0-9]+(\\.[0-9]+)*"
      CPACK_IFW_FRAMEWORK_VERSION "${CPACK_IFW_INSTALLERBASE_EXECUTABLE}")
    if(CPACK_IFW_FRAMEWORK_VERSION)
      set(CPACK_IFW_FRAMEWORK_VERSION_SOURCE "INSTALLERBASE_PATH")
    endif()
  endif()
elseif(CPACK_IFW_FRAMEWORK_VERSION_FORCED)
  set(CPACK_IFW_FRAMEWORK_VERSION ${CPACK_IFW_FRAMEWORK_VERSION_FORCED})
  set(CPACK_IFW_FRAMEWORK_VERSION_SOURCE "FORCED")
endif()
if(CPACK_IFW_VERBOSE)
  if(CPACK_IFW_FRAMEWORK_VERSION AND CPACK_IFW_FRAMEWORK_VERSION_FORCED)
    message(STATUS "Found QtIFW ${CPACK_IFW_FRAMEWORK_VERSION} (forced) version")
  elseif(CPACK_IFW_FRAMEWORK_VERSION)
    message(STATUS "Found QtIFW ${CPACK_IFW_FRAMEWORK_VERSION} version")
  endif()
endif()
if(CPACK_IFW_INSTALLERBASE_EXECUTABLE AND NOT CPACK_IFW_FRAMEWORK_VERSION)
  message(WARNING "Could not detect QtIFW tools version. Set used version to variable \"CPACK_IFW_FRAMEWORK_VERSION_FORCED\" manualy.")
endif()

#=============================================================================
# Macro definition
#=============================================================================

# Macro definition based on CPackComponent

if(NOT CPackComponent_CMake_INCLUDED)
    include(CPackComponent)
endif()

# Resolve full filename for script file
macro(_cpack_ifw_resolve_script _variable)
  set(_ifw_script_macro ${_variable})
  set(_ifw_script_file ${${_ifw_script_macro}})
  if(DEFINED ${_ifw_script_macro})
    get_filename_component(${_ifw_script_macro} ${_ifw_script_file} ABSOLUTE)
    set(_ifw_script_file ${${_ifw_script_macro}})
    if(NOT EXISTS ${_ifw_script_file})
      message(WARNING "CPack IFW: script file \"${_ifw_script_file}\" is not exists")
      set(${_ifw_script_macro})
    endif()
  endif()
endmacro()

# Resolve full path to lisense file
macro(_cpack_ifw_resolve_lisenses _variable)
  if(${_variable})
    set(_ifw_license_file FALSE)
    set(_ifw_licenses_fix)
    foreach(_ifw_licenses_arg ${${_variable}})
      if(_ifw_license_file)
        get_filename_component(_ifw_licenses_arg "${_ifw_licenses_arg}" ABSOLUTE)
        set(_ifw_license_file FALSE)
      else()
        set(_ifw_license_file TRUE)
      endif()
      list(APPEND _ifw_licenses_fix "${_ifw_licenses_arg}")
    endforeach(_ifw_licenses_arg)
    set(${_variable} "${_ifw_licenses_fix}")
  endif()
endmacro()

# Resolve full path to a list of provided files
macro(_cpack_ifw_resolve_file_list _variable)
  if(${_variable})
    set(_ifw_list_fix)
    foreach(_ifw_file_arg ${${_variable}})
      get_filename_component(_ifw_file_arg "${_ifw_file_arg}" ABSOLUTE)
      if(EXISTS ${_ifw_file_arg})
        list(APPEND _ifw_list_fix "${_ifw_file_arg}")
      else()
        message(WARNING "CPack IFW: page file \"${_ifw_file_arg}\" does not exist. Skipping")
      endif()
    endforeach(_ifw_file_arg)
    set(${_variable} "${_ifw_list_fix}")
  endif()
endmacro()

# Macro for configure component
macro(cpack_ifw_configure_component compname)

  string(TOUPPER ${compname} _CPACK_IFWCOMP_UNAME)

  set(_IFW_OPT COMMON ESSENTIAL VIRTUAL FORCED_INSTALLATION REQUIRES_ADMIN_RIGHTS)
  set(_IFW_ARGS NAME VERSION RELEASE_DATE SCRIPT PRIORITY SORTING_PRIORITY UPDATE_TEXT DEFAULT CHECKABLE)
  set(_IFW_MULTI_ARGS DISPLAY_NAME DESCRIPTION DEPENDS DEPENDENCIES AUTO_DEPEND_ON LICENSES USER_INTERFACES TRANSLATIONS REPLACES)
  cmake_parse_arguments(CPACK_IFW_COMPONENT_${_CPACK_IFWCOMP_UNAME} "${_IFW_OPT}" "${_IFW_ARGS}" "${_IFW_MULTI_ARGS}" ${ARGN})

  _cpack_ifw_resolve_script(CPACK_IFW_COMPONENT_${_CPACK_IFWCOMP_UNAME}_SCRIPT)
  _cpack_ifw_resolve_lisenses(CPACK_IFW_COMPONENT_${_CPACK_IFWCOMP_UNAME}_LICENSES)
  _cpack_ifw_resolve_file_list(CPACK_IFW_COMPONENT_${_CPACK_IFWCOMP_UNAME}_USER_INTERFACES)
  _cpack_ifw_resolve_file_list(CPACK_IFW_COMPONENT_${_CPACK_IFWCOMP_UNAME}_TRANSLATIONS)

  set(_CPACK_IFWCOMP_STR "\n# Configuration for IFW component \"${compname}\"\n")

  foreach(_IFW_ARG_NAME ${_IFW_OPT})
  cpack_append_option_set_command(
    CPACK_IFW_COMPONENT_${_CPACK_IFWCOMP_UNAME}_${_IFW_ARG_NAME}
    _CPACK_IFWCOMP_STR)
  endforeach()

  foreach(_IFW_ARG_NAME ${_IFW_ARGS})
  cpack_append_string_variable_set_command(
    CPACK_IFW_COMPONENT_${_CPACK_IFWCOMP_UNAME}_${_IFW_ARG_NAME}
    _CPACK_IFWCOMP_STR)
  endforeach()

  foreach(_IFW_ARG_NAME ${_IFW_MULTI_ARGS})
  cpack_append_list_variable_set_command(
    CPACK_IFW_COMPONENT_${_CPACK_IFWCOMP_UNAME}_${_IFW_ARG_NAME}
    _CPACK_IFWCOMP_STR)
  endforeach()

  if(CPack_CMake_INCLUDED)
    file(APPEND "${CPACK_OUTPUT_CONFIG_FILE}" "${_CPACK_IFWCOMP_STR}")
  endif()

endmacro()

# Macro for configure group
macro(cpack_ifw_configure_component_group grpname)

  string(TOUPPER ${grpname} _CPACK_IFWGRP_UNAME)

  set(_IFW_OPT VIRTUAL FORCED_INSTALLATION REQUIRES_ADMIN_RIGHTS)
  set(_IFW_ARGS NAME VERSION RELEASE_DATE SCRIPT PRIORITY SORTING_PRIORITY UPDATE_TEXT DEFAULT CHECKABLE)
  set(_IFW_MULTI_ARGS DISPLAY_NAME DESCRIPTION DEPENDS DEPENDENCIES AUTO_DEPEND_ON LICENSES USER_INTERFACES TRANSLATIONS REPLACES)
  cmake_parse_arguments(CPACK_IFW_COMPONENT_GROUP_${_CPACK_IFWGRP_UNAME} "${_IFW_OPT}" "${_IFW_ARGS}" "${_IFW_MULTI_ARGS}" ${ARGN})

  _cpack_ifw_resolve_script(CPACK_IFW_COMPONENT_GROUP_${_CPACK_IFWGRP_UNAME}_SCRIPT)
  _cpack_ifw_resolve_lisenses(CPACK_IFW_COMPONENT_GROUP_${_CPACK_IFWGRP_UNAME}_LICENSES)
  _cpack_ifw_resolve_file_list(CPACK_IFW_COMPONENT_GROUP_${_CPACK_IFWGRP_UNAME}_USER_INTERFACES)
  _cpack_ifw_resolve_file_list(CPACK_IFW_COMPONENT_GROUP_${_CPACK_IFWGRP_UNAME}_TRANSLATIONS)

  set(_CPACK_IFWGRP_STR "\n# Configuration for IFW component group \"${grpname}\"\n")

  foreach(_IFW_ARG_NAME ${_IFW_ARGS})
  cpack_append_string_variable_set_command(
    CPACK_IFW_COMPONENT_GROUP_${_CPACK_IFWGRP_UNAME}_${_IFW_ARG_NAME}
    _CPACK_IFWGRP_STR)
  endforeach()

  foreach(_IFW_ARG_NAME ${_IFW_MULTI_ARGS})
  cpack_append_list_variable_set_command(
    CPACK_IFW_COMPONENT_GROUP_${_CPACK_IFWGRP_UNAME}_${_IFW_ARG_NAME}
    _CPACK_IFWGRP_STR)
  endforeach()

  if(CPack_CMake_INCLUDED)
    file(APPEND "${CPACK_OUTPUT_CONFIG_FILE}" "${_CPACK_IFWGRP_STR}")
  endif()
endmacro()

# Macro for adding repository
macro(cpack_ifw_add_repository reponame)

  string(TOUPPER ${reponame} _CPACK_IFWREPO_UNAME)

  set(_IFW_OPT DISABLED)
  set(_IFW_ARGS URL USERNAME PASSWORD DISPLAY_NAME)
  set(_IFW_MULTI_ARGS)
  cmake_parse_arguments(CPACK_IFW_REPOSITORY_${_CPACK_IFWREPO_UNAME} "${_IFW_OPT}" "${_IFW_ARGS}" "${_IFW_MULTI_ARGS}" ${ARGN})

  set(_CPACK_IFWREPO_STR "\n# Configuration for IFW repository \"${reponame}\"\n")

  foreach(_IFW_ARG_NAME ${_IFW_OPT})
  cpack_append_option_set_command(
    CPACK_IFW_REPOSITORY_${_CPACK_IFWREPO_UNAME}_${_IFW_ARG_NAME}
    _CPACK_IFWREPO_STR)
  endforeach()

  foreach(_IFW_ARG_NAME ${_IFW_ARGS})
  cpack_append_string_variable_set_command(
    CPACK_IFW_REPOSITORY_${_CPACK_IFWREPO_UNAME}_${_IFW_ARG_NAME}
    _CPACK_IFWREPO_STR)
  endforeach()

  foreach(_IFW_ARG_NAME ${_IFW_MULTI_ARGS})
  cpack_append_variable_set_command(
    CPACK_IFW_REPOSITORY_${_CPACK_IFWREPO_UNAME}_${_IFW_ARG_NAME}
    _CPACK_IFWREPO_STR)
  endforeach()

  list(APPEND CPACK_IFW_REPOSITORIES_ALL ${reponame})
  string(APPEND _CPACK_IFWREPO_STR "list(APPEND CPACK_IFW_REPOSITORIES_ALL ${reponame})\n")

  if(CPack_CMake_INCLUDED)
    file(APPEND "${CPACK_OUTPUT_CONFIG_FILE}" "${_CPACK_IFWREPO_STR}")
  endif()

endmacro()

# Macro for updating repository
macro(cpack_ifw_update_repository reponame)

  string(TOUPPER ${reponame} _CPACK_IFWREPO_UNAME)

  set(_IFW_OPT ADD REMOVE REPLACE DISABLED)
  set(_IFW_ARGS URL OLD_URL NEW_URL USERNAME PASSWORD DISPLAY_NAME)
  set(_IFW_MULTI_ARGS)
  cmake_parse_arguments(CPACK_IFW_REPOSITORY_${_CPACK_IFWREPO_UNAME} "${_IFW_OPT}" "${_IFW_ARGS}" "${_IFW_MULTI_ARGS}" ${ARGN})

  set(_CPACK_IFWREPO_STR "\n# Configuration for IFW repository \"${reponame}\" update\n")

  foreach(_IFW_ARG_NAME ${_IFW_OPT})
  cpack_append_option_set_command(
    CPACK_IFW_REPOSITORY_${_CPACK_IFWREPO_UNAME}_${_IFW_ARG_NAME}
    _CPACK_IFWREPO_STR)
  endforeach()

  foreach(_IFW_ARG_NAME ${_IFW_ARGS})
  cpack_append_string_variable_set_command(
    CPACK_IFW_REPOSITORY_${_CPACK_IFWREPO_UNAME}_${_IFW_ARG_NAME}
    _CPACK_IFWREPO_STR)
  endforeach()

  foreach(_IFW_ARG_NAME ${_IFW_MULTI_ARGS})
  cpack_append_variable_set_command(
    CPACK_IFW_REPOSITORY_${_CPACK_IFWREPO_UNAME}_${_IFW_ARG_NAME}
    _CPACK_IFWREPO_STR)
  endforeach()

  if(CPACK_IFW_REPOSITORY_${_CPACK_IFWREPO_UNAME}_ADD
    OR CPACK_IFW_REPOSITORY_${_CPACK_IFWREPO_UNAME}_REMOVE
    OR CPACK_IFW_REPOSITORY_${_CPACK_IFWREPO_UNAME}_REPLACE)
    list(APPEND CPACK_IFW_REPOSITORIES_ALL ${reponame})
    string(APPEND _CPACK_IFWREPO_STR "list(APPEND CPACK_IFW_REPOSITORIES_ALL ${reponame})\n")
  else()
    set(_CPACK_IFWREPO_STR)
  endif()

  if(CPack_CMake_INCLUDED AND _CPACK_IFWREPO_STR)
    file(APPEND "${CPACK_OUTPUT_CONFIG_FILE}" "${_CPACK_IFWREPO_STR}")
  endif()

endmacro()

# Macro for adding resources
macro(cpack_ifw_add_package_resources)
  set(_CPACK_IFW_PACKAGE_RESOURCES ${ARGV})
  _cpack_ifw_resolve_file_list(_CPACK_IFW_PACKAGE_RESOURCES)
  list(APPEND CPACK_IFW_PACKAGE_RESOURCES ${_CPACK_IFW_PACKAGE_RESOURCES})
  set(_CPACK_IFWQRC_STR "list(APPEND CPACK_IFW_PACKAGE_RESOURCES \"${_CPACK_IFW_PACKAGE_RESOURCES}\")\n")
  if(CPack_CMake_INCLUDED)
    file(APPEND "${CPACK_OUTPUT_CONFIG_FILE}" "${_CPACK_IFWQRC_STR}")
  endif()
endmacro()

# Resolve package control script
_cpack_ifw_resolve_script(CPACK_IFW_PACKAGE_CONTROL_SCRIPT)

endif() # NOT CPackIFW_CMake_INCLUDED
