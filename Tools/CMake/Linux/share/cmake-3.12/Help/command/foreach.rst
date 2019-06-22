foreach
-------

Evaluate a group of commands for each value in a list.

::

  foreach(loop_var arg1 arg2 ...)
    COMMAND1(ARGS ...)
    COMMAND2(ARGS ...)
    ...
  endforeach(loop_var)

All commands between foreach and the matching endforeach are recorded
without being invoked.  Once the endforeach is evaluated, the recorded
list of commands is invoked once for each argument listed in the
original foreach command.  Before each iteration of the loop
``${loop_var}`` will be set as a variable with the current value in the
list.

::

  foreach(loop_var RANGE total)
  foreach(loop_var RANGE start stop [step])

Foreach can also iterate over a generated range of numbers.  There are
three types of this iteration:

* When specifying single number, the range will have elements [0, ... to
  "total"] (inclusive).

* When specifying two numbers, the range will have elements from the
  first number to the second number (inclusive).

* The third optional number is the increment used to iterate from the
  first number to the second number (inclusive).

::

  foreach(loop_var IN [LISTS [list1 [...]]]
                      [ITEMS [item1 [...]]])

Iterates over a precise list of items.  The ``LISTS`` option names
list-valued variables to be traversed, including empty elements (an
empty string is a zero-length list).  (Note macro
arguments are not variables.)  The ``ITEMS`` option ends argument
parsing and includes all arguments following it in the iteration.
