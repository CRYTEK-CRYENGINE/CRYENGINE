CMAKE_INSTALL_RPATH_USE_LINK_PATH
---------------------------------

Add paths to linker search and installed rpath.

``CMAKE_INSTALL_RPATH_USE_LINK_PATH`` is a boolean that if set to ``true``
will append directories in the linker search path and outside the
project to the :prop_tgt:`INSTALL_RPATH`.  This is used to initialize the
target property :prop_tgt:`INSTALL_RPATH_USE_LINK_PATH` for all targets.
