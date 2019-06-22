add_compile_definitions
-----------------------

Adds preprocessor definitions to the compilation of source files.

::

  add_compile_definitions(<definition> ...)

Adds preprocessor definitions to the compiler command line for targets in the
current directory and below (whether added before or after this command is
invoked). See documentation of the :prop_dir:`directory <COMPILE_DEFINITIONS>`
and :prop_tgt:`target <COMPILE_DEFINITIONS>` ``COMPILE_DEFINITIONS`` properties.

Definitions are specified using the syntax ``VAR`` or ``VAR=value``.
Function-style definitions are not supported. CMake will automatically
escape the value correctly for the native build system (note that CMake
language syntax may require escapes to specify some values).

Arguments to ``add_compile_definitions`` may use "generator expressions" with
the syntax ``$<...>``.  See the :manual:`cmake-generator-expressions(7)`
manual for available expressions.  See the :manual:`cmake-buildsystem(7)`
manual for more on defining buildsystem properties.
