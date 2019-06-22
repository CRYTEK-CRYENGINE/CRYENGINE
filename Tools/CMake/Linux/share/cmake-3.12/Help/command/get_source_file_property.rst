get_source_file_property
------------------------

Get a property for a source file.

::

  get_source_file_property(VAR file property)

Get a property from a source file.  The value of the property is
stored in the variable ``VAR``.  If the source property is not found, the
behavior depends on whether it has been defined to be an ``INHERITED`` property
or not (see :command:`define_property`).  Non-inherited properties will set
``VAR`` to "NOTFOUND", whereas inherited properties will search the relevant
parent scope as described for the :command:`define_property` command and
if still unable to find the property, ``VAR`` will be set to an empty string.

Use :command:`set_source_files_properties` to set property values.  Source
file properties usually control how the file is built. One property that is
always there is :prop_sf:`LOCATION`.

See also the more general :command:`get_property` command.
