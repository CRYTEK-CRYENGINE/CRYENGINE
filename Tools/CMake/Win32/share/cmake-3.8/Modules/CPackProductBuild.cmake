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
