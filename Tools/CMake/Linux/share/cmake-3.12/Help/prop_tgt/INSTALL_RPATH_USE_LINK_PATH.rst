INSTALL_RPATH_USE_LINK_PATH
---------------------------

Add paths to linker search and installed rpath.

INSTALL_RPATH_USE_LINK_PATH is a boolean that if set to true will
append directories in the linker search path and outside the project
to the INSTALL_RPATH.  This property is initialized by the value of
the variable CMAKE_INSTALL_RPATH_USE_LINK_PATH if it is set when a
target is created.
