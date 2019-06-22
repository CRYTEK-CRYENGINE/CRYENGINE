return
------

Return from a file, directory or function.

::

  return()

Returns from a file, directory or function.  When this command is
encountered in an included file (via :command:`include` or
:command:`find_package`), it causes processing of the current file to stop
and control is returned to the including file.  If it is encountered in a
file which is not included by another file, e.g.  a ``CMakeLists.txt``,
control is returned to the parent directory if there is one.  If return is
called in a function, control is returned to the caller of the function.
Note that a macro is not a function and does not handle return like a
function does.
