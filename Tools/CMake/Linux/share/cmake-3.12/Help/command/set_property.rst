set_property
------------

Set a named property in a given scope.

::

  set_property(<GLOBAL                            |
                DIRECTORY [dir]                   |
                TARGET    [target1 [target2 ...]] |
                SOURCE    [src1 [src2 ...]]       |
                INSTALL   [file1 [file2 ...]]     |
                TEST      [test1 [test2 ...]]     |
                CACHE     [entry1 [entry2 ...]]>
               [APPEND] [APPEND_STRING]
               PROPERTY <name> [value1 [value2 ...]])

Set one property on zero or more objects of a scope.  The first
argument determines the scope in which the property is set.  It must
be one of the following:

``GLOBAL``
  Scope is unique and does not accept a name.

``DIRECTORY``
  Scope defaults to the current directory but another
  directory (already processed by CMake) may be named by full or
  relative path.

``TARGET``
  Scope may name zero or more existing targets.

``SOURCE``
  Scope may name zero or more source files.  Note that source
  file properties are visible only to targets added in the same
  directory (CMakeLists.txt).

``INSTALL``
  Scope may name zero or more installed file paths.
  These are made available to CPack to influence deployment.

  Both the property key and value may use generator expressions.
  Specific properties may apply to installed files and/or directories.

  Path components have to be separated by forward slashes,
  must be normalized and are case sensitive.

  To reference the installation prefix itself with a relative path use ".".

  Currently installed file properties are only defined for
  the WIX generator where the given paths are relative
  to the installation prefix.

``TEST``
  Scope may name zero or more existing tests.

``CACHE``
  Scope must name zero or more cache existing entries.

The required ``PROPERTY`` option is immediately followed by the name of
the property to set.  Remaining arguments are used to compose the
property value in the form of a semicolon-separated list.

If the ``APPEND`` option is given the list is appended to any existing
property value.  If the ``APPEND_STRING`` option is given the string is
appended to any existing property value as string, i.e. it results in a
longer string and not a list of strings.  When using ``APPEND`` or
``APPEND_STRING`` with a property defined to support ``INHERITED``
behavior (see :command:`define_property`), no inheriting occurs when
finding the initial value to append to.  If the property is not already
directly set in the nominated scope, the command will behave as though
``APPEND`` or ``APPEND_STRING`` had not been given.

See the :manual:`cmake-properties(7)` manual for a list of properties
in each scope.
