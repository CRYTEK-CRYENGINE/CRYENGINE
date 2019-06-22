get_directory_property
----------------------

Get a property of ``DIRECTORY`` scope.

::

  get_directory_property(<variable> [DIRECTORY <dir>] <prop-name>)

Store a property of directory scope in the named ``<variable>``.
The ``DIRECTORY`` argument specifies another directory from which
to retrieve the property value instead of the current directory.
The specified directory must have already been traversed by CMake.

If the property is not defined for the nominated directory scope,
an empty string is returned.  In the case of ``INHERITED`` properties,
if the property is not found for the nominated directory scope,
the search will chain to a parent scope as described for the
:command:`define_property` command.

::

  get_directory_property(<variable> [DIRECTORY <dir>]
                         DEFINITION <var-name>)

Get a variable definition from a directory.  This form is useful to
get a variable definition from another directory.

See also the more general :command:`get_property` command.
