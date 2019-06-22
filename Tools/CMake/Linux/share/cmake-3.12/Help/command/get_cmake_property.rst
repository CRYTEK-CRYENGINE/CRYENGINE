get_cmake_property
------------------

Get a global property of the CMake instance.

::

  get_cmake_property(VAR property)

Get a global property from the CMake instance.  The value of the property is
stored in the variable ``VAR``.  If the property is not found, ``VAR``
will be  set to "NOTFOUND".  See the :manual:`cmake-properties(7)` manual
for available properties.

See also the :command:`get_property` command ``GLOBAL`` option.

In addition to global properties, this command (for historical reasons)
also supports the :prop_dir:`VARIABLES` and :prop_dir:`MACROS` directory
properties.  It also supports a special ``COMPONENTS`` global property that
lists the components given to the :command:`install` command.
