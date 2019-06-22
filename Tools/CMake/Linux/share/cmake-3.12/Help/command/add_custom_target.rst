add_custom_target
-----------------

Add a target with no output so it will always be built.

::

  add_custom_target(Name [ALL] [command1 [args1...]]
                    [COMMAND command2 [args2...] ...]
                    [DEPENDS depend depend depend ... ]
                    [BYPRODUCTS [files...]]
                    [WORKING_DIRECTORY dir]
                    [COMMENT comment]
                    [VERBATIM] [USES_TERMINAL]
                    [COMMAND_EXPAND_LISTS]
                    [SOURCES src1 [src2...]])

Adds a target with the given name that executes the given commands.
The target has no output file and is *always considered out of date*
even if the commands try to create a file with the name of the target.
Use the :command:`add_custom_command` command to generate a file with
dependencies.  By default nothing depends on the custom target.  Use
the :command:`add_dependencies` command to add dependencies to or
from other targets.

The options are:

``ALL``
  Indicate that this target should be added to the default build
  target so that it will be run every time (the command cannot be
  called ``ALL``).

``BYPRODUCTS``
  Specify the files the command is expected to produce but whose
  modification time may or may not be updated on subsequent builds.
  If a byproduct name is a relative path it will be interpreted
  relative to the build tree directory corresponding to the
  current source directory.
  Each byproduct file will be marked with the :prop_sf:`GENERATED`
  source file property automatically.

  Explicit specification of byproducts is supported by the
  :generator:`Ninja` generator to tell the ``ninja`` build tool
  how to regenerate byproducts when they are missing.  It is
  also useful when other build rules (e.g. custom commands)
  depend on the byproducts.  Ninja requires a build rule for any
  generated file on which another rule depends even if there are
  order-only dependencies to ensure the byproducts will be
  available before their dependents build.

  The ``BYPRODUCTS`` option is ignored on non-Ninja generators
  except to mark byproducts ``GENERATED``.

``COMMAND``
  Specify the command-line(s) to execute at build time.
  If more than one ``COMMAND`` is specified they will be executed in order,
  but *not* necessarily composed into a stateful shell or batch script.
  (To run a full script, use the :command:`configure_file` command or the
  :command:`file(GENERATE)` command to create it, and then specify
  a ``COMMAND`` to launch it.)

  If ``COMMAND`` specifies an executable target name (created by the
  :command:`add_executable` command) it will automatically be replaced
  by the location of the executable created at build time. If set, the
  :prop_tgt:`CROSSCOMPILING_EMULATOR` executable target property will
  also be prepended to the command to allow the executable to run on
  the host.
  Additionally a target-level dependency will be added so that the
  executable target will be built before this custom target.

  Arguments to ``COMMAND`` may use
  :manual:`generator expressions <cmake-generator-expressions(7)>`.
  References to target names in generator expressions imply target-level
  dependencies.

  The command and arguments are optional and if not specified an empty
  target will be created.

``COMMENT``
  Display the given message before the commands are executed at
  build time.

``DEPENDS``
  Reference files and outputs of custom commands created with
  :command:`add_custom_command` command calls in the same directory
  (``CMakeLists.txt`` file).  They will be brought up to date when
  the target is built.

  Use the :command:`add_dependencies` command to add dependencies
  on other targets.

``COMMAND_EXPAND_LISTS``
  Lists in ``COMMAND`` arguments will be expanded, including those
  created with
  :manual:`generator expressions <cmake-generator-expressions(7)>`,
  allowing ``COMMAND`` arguments such as
  ``${CC} "-I$<JOIN:$<TARGET_PROPERTY:foo,INCLUDE_DIRECTORIES>,;-I>" foo.cc``
  to be properly expanded.

``SOURCES``
  Specify additional source files to be included in the custom target.
  Specified source files will be added to IDE project files for
  convenience in editing even if they have no build rules.

``VERBATIM``
  All arguments to the commands will be escaped properly for the
  build tool so that the invoked command receives each argument
  unchanged.  Note that one level of escapes is still used by the
  CMake language processor before ``add_custom_target`` even sees
  the arguments.  Use of ``VERBATIM`` is recommended as it enables
  correct behavior.  When ``VERBATIM`` is not given the behavior
  is platform specific because there is no protection of
  tool-specific special characters.

``USES_TERMINAL``
  The command will be given direct access to the terminal if possible.
  With the :generator:`Ninja` generator, this places the command in
  the ``console`` :prop_gbl:`pool <JOB_POOLS>`.

``WORKING_DIRECTORY``
  Execute the command with the given current working directory.
  If it is a relative path it will be interpreted relative to the
  build tree directory corresponding to the current source directory.
