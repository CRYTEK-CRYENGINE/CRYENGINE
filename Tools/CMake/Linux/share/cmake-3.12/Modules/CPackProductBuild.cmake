# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CPackProductBuild
# -----------------
#
# productbuild CPack generator (Mac OS X).
#
# Variables specific to CPack productbuild generator
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# The following variable is specific to installers built on Mac
# OS X using productbuild:
#
# .. variable:: CPACK_COMMAND_PRODUCTBUILD
#
#  Path to the productbuild(1) command used to generate a product archive for
#  the OS X Installer or Mac App Store.  This variable can be used to override
#  the automatically detected command (or specify its location if the
#  auto-detection fails to find it.)
#
# .. variable:: CPACK_PRODUCTBUILD_IDENTITY_NAME
#
#  Adds a digital signature to the resulting package.
#
#
# .. variable:: CPACK_PRODUCTBUILD_KEYCHAIN_PATH
#
#  Specify a specific keychain to search for the signing identity.
#
#
# .. variable:: CPACK_COMMAND_PKGBUILD
#
#  Path to the pkgbuild(1) command used to generate an OS X component package
#  on OS X.  This variable can be used to override the automatically detected
#  command (or specify its location if the auto-detection fails to find it.)
#
#
# .. variable:: CPACK_PKGBUILD_IDENTITY_NAME
#
#  Adds a digital signature to the resulting package.
#
#
# .. variable:: CPACK_PKGBUILD_KEYCHAIN_PATH
#
#  Specify a specific keychain to search for the signing identity.
#
#
# .. variable:: CPACK_PREFLIGHT_<COMP>_SCRIPT
#
#  Full path to a file that will be used as the ``preinstall`` script for the
#  named ``<COMP>`` component's package, where ``<COMP>`` is the uppercased
#  component name.  No ``preinstall`` script is added if this variable is not
#  defined for a given component.
#
#
# .. variable:: CPACK_POSTFLIGHT_<COMP>_SCRIPT
#
#  Full path to a file that will be used as the ``postinstall`` script for the
#  named ``<COMP>`` component's package, where ``<COMP>`` is the uppercased
#  component name.  No ``postinstall`` script is added if this variable is not
#  defined for a given component.
#
#
# .. variable:: CPACK_PRODUCTBUILD_RESOURCES_DIR
#
#  If specified the productbuild generator copies files from this directory
#  (including subdirectories) to the ``Resources`` directory. This is done
#  before the :variable:`CPACK_RESOURCE_FILE_WELCOME`,
#  :variable:`CPACK_RESOURCE_FILE_README`, and
#  :variable:`CPACK_RESOURCE_FILE_LICENSE` files are copied.
