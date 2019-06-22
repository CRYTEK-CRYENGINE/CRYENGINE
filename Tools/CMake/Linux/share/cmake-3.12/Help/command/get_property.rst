get_property
------------

Get a property.

::

  get_property(<variable>
               <GLOBAL             |
                DIRECTORY [dir]    |
                TARGET    <target> |
                SOURCE    <source> |
                INSTALL   <file>   |
                TEST      <test>   |
                CACHE     <entry>  |
                VARIABLE>
               PROPERTY <name>
               [SET | DEFINED | BRIEF_DOCS | FULL_DOCS])

Get one property from one object in a scope.  The first argument
specifies the variable in which to store the result.  The second
argument determines the scope from which to get the property.  It must
be one of the following:

``GLOBAL``
  Scope is unique and does not accept a name.

``DIRECTORY``
  Scope defaults to the current directory but another
  directory (already processed by CMake) may be named by full or
  relative path.

``TARGET``
  Scope must name one existing target.

``SOURCE``
  Scope must name one source file.

``INSTALL``
  Scope must name one installed file path.

``TEST``
  Scope must name one existing test.

``CACHE``
  Scope must name one cache entry.

``VARIABLE``
  Scope is unique and does not accept a name.

The required ``PROPERTY`` option is immediately followed by the name of
the property to get.  If the property is not set an empty value is
returned, although some properties support inheriting from a parent scope
if defined to behave that way (see :command:`define_property`).

If the ``SET`` option is given the variable is set to a boolean
value indicating whether the property has been set.  If the ``DEFINED``
option is given the variable is set to a boolean value indicating
whether the property has been defined such as with the
:command:`define_property` command.
If ``BRIEF_DOCS`` or ``FULL_DOCS`` is given then the variable is set to a
string containing documentation for the requested property.  If
documentation is requested for a property that has not been defined
``NOTFOUND`` is returned.
