<LANG>_CPPCHECK
---------------

This property is supported only when ``<LANG>`` is ``C`` or ``CXX``.

Specify a :ref:`;-list <CMake Language Lists>` containing a command line
for the ``cppcheck`` static analysis tool.  The :ref:`Makefile Generators`
and the :generator:`Ninja` generator will run ``cppcheck`` along with the
compiler and report any problems.

This property is initialized by the value of the
:variable:`CMAKE_<LANG>_CPPCHECK` variable if it is set when a target is
created.
