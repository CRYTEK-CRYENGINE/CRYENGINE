CMAKE_DEBUG_TARGET_PROPERTIES
-----------------------------

Enables tracing output for target properties.

This variable can be populated with a list of properties to generate
debug output for when evaluating target properties.  Currently it can
only be used when evaluating the :prop_tgt:`INCLUDE_DIRECTORIES`,
:prop_tgt:`COMPILE_DEFINITIONS`, :prop_tgt:`COMPILE_OPTIONS`,
:prop_tgt:`AUTOUIC_OPTIONS`, :prop_tgt:`SOURCES`, :prop_tgt:`COMPILE_FEATURES`,
:prop_tgt:`POSITION_INDEPENDENT_CODE` target properties and any other property
listed in :prop_tgt:`COMPATIBLE_INTERFACE_STRING` and other
``COMPATIBLE_INTERFACE_`` properties.  It outputs an origin for each entry in
the target property.  Default is unset.
