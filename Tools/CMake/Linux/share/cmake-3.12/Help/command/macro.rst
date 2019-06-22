macro
-----

Start recording a macro for later invocation as a command::

  macro(<name> [arg1 [arg2 [arg3 ...]]])
    COMMAND1(ARGS ...)
    COMMAND2(ARGS ...)
    ...
  endmacro(<name>)

Define a macro named ``<name>`` that takes arguments named ``arg1``,
``arg2``, ``arg3``, (...).
Commands listed after macro, but before the matching
:command:`endmacro()`, are not invoked until the macro is invoked.
When it is invoked, the commands recorded in the macro are first
modified by replacing formal parameters (``${arg1}``) with the arguments
passed, and then invoked as normal commands.
In addition to referencing the formal parameters you can reference the
values ``${ARGC}`` which will be set to the number of arguments passed
into the function as well as ``${ARGV0}``, ``${ARGV1}``, ``${ARGV2}``,
...  which will have the actual values of the arguments passed in.
This facilitates creating macros with optional arguments.
Additionally ``${ARGV}`` holds the list of all arguments given to the
macro and ``${ARGN}`` holds the list of arguments past the last expected
argument.
Referencing to ``${ARGV#}`` arguments beyond ``${ARGC}`` have undefined
behavior. Checking that ``${ARGC}`` is greater than ``#`` is the only
way to ensure that ``${ARGV#}`` was passed to the function as an extra
argument.

See the :command:`cmake_policy()` command documentation for the behavior
of policies inside macros.

Macro Argument Caveats
^^^^^^^^^^^^^^^^^^^^^^

Note that the parameters to a macro and values such as ``ARGN`` are
not variables in the usual CMake sense.  They are string
replacements much like the C preprocessor would do with a macro.
Therefore you will NOT be able to use commands like::

 if(ARGV1) # ARGV1 is not a variable
 if(DEFINED ARGV2) # ARGV2 is not a variable
 if(ARGC GREATER 2) # ARGC is not a variable
 foreach(loop_var IN LISTS ARGN) # ARGN is not a variable

In the first case, you can use ``if(${ARGV1})``.
In the second and third case, the proper way to check if an optional
variable was passed to the macro is to use ``if(${ARGC} GREATER 2)``.
In the last case, you can use ``foreach(loop_var ${ARGN})`` but this
will skip empty arguments.
If you need to include them, you can use::

 set(list_var "${ARGN}")
 foreach(loop_var IN LISTS list_var)

Note that if you have a variable with the same name in the scope from
which the macro is called, using unreferenced names will use the
existing variable instead of the arguments. For example::

 macro(_BAR)
   foreach(arg IN LISTS ARGN)
     [...]
   endforeach()
 endmacro()

 function(_FOO)
   _bar(x y z)
 endfunction()

 _foo(a b c)

Will loop over ``a;b;c`` and not over ``x;y;z`` as one might be expecting.
If you want true CMake variables and/or better CMake scope control you
should look at the function command.
