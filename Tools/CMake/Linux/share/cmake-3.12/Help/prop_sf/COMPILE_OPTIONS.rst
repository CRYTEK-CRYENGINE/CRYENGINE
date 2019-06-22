COMPILE_OPTIONS
---------------

List of additional options to pass to the compiler.

This property holds a :ref:`;-list <CMake Language Lists>` of options
and will be added to the list of compile flags when this
source file builds.  Use :prop_sf:`COMPILE_DEFINITIONS` to pass
additional preprocessor definitions and :prop_sf:`INCLUDE_DIRECTORIES` to pass
additional include directories.

Contents of ``COMPILE_OPTIONS`` may use "generator expressions" with the
syntax ``$<...>``.  See the :manual:`cmake-generator-expressions(7)` manual
for available expressions.  However, :generator:`Xcode`
does not support per-config per-source settings, so expressions
that depend on the build configuration are not allowed with that
generator.

.. note::

  This property should be preferred over the :prop_sf:`COMPILE_FLAGS` property.
