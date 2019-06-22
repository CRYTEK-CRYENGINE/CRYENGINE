# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CPackNSIS
# ---------
#
# CPack NSIS generator specific options
#
# Variables specific to CPack NSIS generator
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# The following variables are specific to the graphical installers built
# on Windows using the Nullsoft Installation System.
#
# .. variable:: CPACK_NSIS_INSTALL_ROOT
#
#  The default installation directory presented to the end user by the NSIS
#  installer is under this root dir. The full directory presented to the end
#  user is: ${CPACK_NSIS_INSTALL_ROOT}/${CPACK_PACKAGE_INSTALL_DIRECTORY}
#
# .. variable:: CPACK_NSIS_MUI_ICON
#
#  An icon filename.  The name of a ``*.ico`` file used as the main icon for the
#  generated install program.
#
# .. variable:: CPACK_NSIS_MUI_UNIICON
#
#  An icon filename.  The name of a ``*.ico`` file used as the main icon for the
#  generated uninstall program.
#
# .. variable:: CPACK_NSIS_INSTALLER_MUI_ICON_CODE
#
#  undocumented.
#
# .. variable:: CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP
#
#  The filename of a bitmap to use as the NSIS MUI_WELCOMEFINISHPAGE_BITMAP.
#
# .. variable:: CPACK_NSIS_MUI_UNWELCOMEFINISHPAGE_BITMAP
#
#  The filename of a bitmap to use as the NSIS MUI_UNWELCOMEFINISHPAGE_BITMAP.
#
# .. variable:: CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS
#
#  Extra NSIS commands that will be added to the beginning of the install
#  Section, before your install tree is available on the target system.
#
# .. variable:: CPACK_NSIS_EXTRA_INSTALL_COMMANDS
#
#  Extra NSIS commands that will be added to the end of the install Section,
#  after your install tree is available on the target system.
#
# .. variable:: CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
#
#  Extra NSIS commands that will be added to the uninstall Section, before
#  your install tree is removed from the target system.
#
# .. variable:: CPACK_NSIS_COMPRESSOR
#
#  The arguments that will be passed to the NSIS SetCompressor command.
#
# .. variable:: CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL
#
#  Ask about uninstalling previous versions first.  If this is set to "ON",
#  then an installer will look for previous installed versions and if one is
#  found, ask the user whether to uninstall it before proceeding with the
#  install.
#
# .. variable:: CPACK_NSIS_MODIFY_PATH
#
#  Modify PATH toggle.  If this is set to "ON", then an extra page will appear
#  in the installer that will allow the user to choose whether the program
#  directory should be added to the system PATH variable.
#
# .. variable:: CPACK_NSIS_DISPLAY_NAME
#
#  The display name string that appears in the Windows Add/Remove Program
#  control panel
#
# .. variable:: CPACK_NSIS_PACKAGE_NAME
#
#  The title displayed at the top of the installer.
#
# .. variable:: CPACK_NSIS_INSTALLED_ICON_NAME
#
#  A path to the executable that contains the installer icon.
#
# .. variable:: CPACK_NSIS_HELP_LINK
#
#  URL to a web site providing assistance in installing your application.
#
# .. variable:: CPACK_NSIS_URL_INFO_ABOUT
#
#  URL to a web site providing more information about your application.
#
# .. variable:: CPACK_NSIS_CONTACT
#
#  Contact information for questions and comments about the installation
#  process.
#
# .. variable:: CPACK_NSIS_<compName>_INSTALL_DIRECTORY
#
#  Custom install directory for the specified component <compName> instead
#  of $INSTDIR.
#
# .. variable:: CPACK_NSIS_CREATE_ICONS_EXTRA
#
#  Additional NSIS commands for creating start menu shortcuts.
#
# .. variable:: CPACK_NSIS_DELETE_ICONS_EXTRA
#
#  Additional NSIS commands to uninstall start menu shortcuts.
#
# .. variable:: CPACK_NSIS_EXECUTABLES_DIRECTORY
#
#  Creating NSIS start menu links assumes that they are in 'bin' unless this
#  variable is set.  For example, you would set this to 'exec' if your
#  executables are in an exec directory.
#
# .. variable:: CPACK_NSIS_MUI_FINISHPAGE_RUN
#
#  Specify an executable to add an option to run on the finish page of the
#  NSIS installer.
#
# .. variable:: CPACK_NSIS_MENU_LINKS
#
#  Specify links in [application] menu.  This should contain a list of pair
#  "link" "link name". The link may be a URL or a path relative to
#  installation prefix.  Like::
#
#   set(CPACK_NSIS_MENU_LINKS
#       "doc/cmake-@CMake_VERSION_MAJOR@.@CMake_VERSION_MINOR@/cmake.html"
#       "CMake Help" "https://cmake.org" "CMake Web Site")
#

#FIXME we should put NSIS specific code here
#FIXME but I'm not doing it because I'm not able to test it...
