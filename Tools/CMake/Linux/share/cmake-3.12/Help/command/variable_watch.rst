variable_watch
--------------

Watch the CMake variable for change.

::

  variable_watch(<variable name> [<command to execute>])

If the specified variable changes, the message will be printed about
the variable being changed.  If the command is specified, the command
will be executed.  The command will receive the following arguments:
COMMAND(<variable> <access> <value> <current list file> <stack>)
