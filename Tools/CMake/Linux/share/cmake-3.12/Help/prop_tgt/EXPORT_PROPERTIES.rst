EXPORT_PROPERTIES
-----------------

List additional properties to export for a target.

This property contains a list of property names that should be exported by
the :command:`install(EXPORT)` and :command:`export` commands.  By default
only a limited number of properties are exported. This property can be used
to additionally export other properties as well.

Properties starting with ``INTERFACE_`` or ``IMPORTED_`` are not allowed as
they are reserved for internal CMake use.

Properties containing generator expressions are also not allowed.
