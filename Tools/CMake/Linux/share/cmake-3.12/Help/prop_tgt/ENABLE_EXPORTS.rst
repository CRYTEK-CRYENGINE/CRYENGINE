ENABLE_EXPORTS
--------------

Specify whether an executable exports symbols for loadable modules.

Normally an executable does not export any symbols because it is the
final program.  It is possible for an executable to export symbols to
be used by loadable modules.  When this property is set to true CMake
will allow other targets to "link" to the executable with the
:command:`TARGET_LINK_LIBRARIES` command.  On all platforms a target-level
dependency on the executable is created for targets that link to it.
For DLL platforms an import library will be created for the exported
symbols and then used for linking.  All Windows-based systems
including Cygwin are DLL platforms.  For non-DLL platforms that
require all symbols to be resolved at link time, such as OS X, the
module will "link" to the executable using a flag like
``-bundle_loader``.  For other non-DLL platforms the link rule is simply
ignored since the dynamic loader will automatically bind symbols when
the module is loaded.

This property is initialized by the value of the variable
:variable:`CMAKE_ENABLE_EXPORTS` if it is set when a target is created.
