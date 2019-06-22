target_compile_options
----------------------

Add compile options to a target.

::

  target_compile_options(<target> [BEFORE]
    <INTERFACE|PUBLIC|PRIVATE> [items1...]
    [<INTERFACE|PUBLIC|PRIVATE> [items2...] ...])

Specify compile options to use when compiling a given target.  The
named ``<target>`` must have been created by a command such as
:command:`add_executable` or :command:`add_library` and must not be an
:ref:`ALIAS target <Alias Targets>`.

If ``BEFORE`` is specified, the content will be prepended to the property
instead of being appended.

This command can be used to add any options, but
alternative commands exist to add preprocessor definitions
(:command:`target_compile_definitions` and :command:`add_compile_definitions`)
or include directories (:command:`target_include_directories` and
:command:`include_directories`).  See documentation of the
:prop_dir:`directory <COMPILE_OPTIONS>` and
:prop_tgt:`target <COMPILE_OPTIONS>` ``COMPILE_OPTIONS`` properties.

The ``INTERFACE``, ``PUBLIC`` and ``PRIVATE`` keywords are required to
specify the scope of the following arguments.  ``PRIVATE`` and ``PUBLIC``
items will populate the :prop_tgt:`COMPILE_OPTIONS` property of
``<target>``.  ``PUBLIC`` and ``INTERFACE`` items will populate the
:prop_tgt:`INTERFACE_COMPILE_OPTIONS` property of ``<target>``.
(:ref:`IMPORTED targets <Imported Targets>` only support ``INTERFACE`` items.)
The following arguments specify compile options.  Repeated calls for the same
``<target>`` append items in the order called.

Arguments to ``target_compile_options`` may use "generator expressions"
with the syntax ``$<...>``. See the :manual:`cmake-generator-expressions(7)`
manual for available expressions.  See the :manual:`cmake-buildsystem(7)`
manual for more on defining buildsystem properties.

.. include:: COMPILE_OPTIONS_SHELL.txt
