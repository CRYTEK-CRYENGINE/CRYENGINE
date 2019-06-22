# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CPackBundle
# -----------
#
# CPack Bundle generator (Mac OS X) specific options
#
# Variables specific to CPack Bundle generator
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# Installers built on Mac OS X using the Bundle generator use the
# aforementioned DragNDrop (CPACK_DMG_xxx) variables, plus the following
# Bundle-specific parameters (CPACK_BUNDLE_xxx).
#
# .. variable:: CPACK_BUNDLE_NAME
#
#  The name of the generated bundle. This appears in the OSX finder as the
#  bundle name. Required.
#
# .. variable:: CPACK_BUNDLE_PLIST
#
#  Path to an OSX plist file that will be used for the generated bundle. This
#  assumes that the caller has generated or specified their own Info.plist
#  file. Required.
#
# .. variable:: CPACK_BUNDLE_ICON
#
#  Path to an OSX icon file that will be used as the icon for the generated
#  bundle. This is the icon that appears in the OSX finder for the bundle, and
#  in the OSX dock when the bundle is opened.  Required.
#
# .. variable:: CPACK_BUNDLE_STARTUP_COMMAND
#
#  Path to a startup script. This is a path to an executable or script that
#  will be run whenever an end-user double-clicks the generated bundle in the
#  OSX Finder. Optional.
#
# .. variable:: CPACK_BUNDLE_APPLE_CERT_APP
#
#  The name of your Apple supplied code signing certificate for the application.
#  The name usually takes the form "Developer ID Application: [Name]" or
#  "3rd Party Mac Developer Application: [Name]". If this variable is not set
#  the application will not be signed.
#
# .. variable:: CPACK_BUNDLE_APPLE_ENTITLEMENTS
#
#  The name of the plist file that contains your apple entitlements for sandboxing
#  your application. This file is required for submission to the Mac App Store.
#
# .. variable:: CPACK_BUNDLE_APPLE_CODESIGN_FILES
#
#  A list of additional files that you wish to be signed. You do not need to
#  list the main application folder, or the main executable. You should
#  list any frameworks and plugins that are included in your app bundle.
#
# .. variable:: CPACK_BUNDLE_APPLE_CODESIGN_PARAMETER
#
#  Additional parameter that will passed to codesign.
#  Default value: "--deep -f"
#
# .. variable:: CPACK_COMMAND_CODESIGN
#
#  Path to the codesign(1) command used to sign applications with an
#  Apple cert. This variable can be used to override the automatically
#  detected command (or specify its location if the auto-detection fails
#  to find it.)

#Bundle Generator specific code should be put here
