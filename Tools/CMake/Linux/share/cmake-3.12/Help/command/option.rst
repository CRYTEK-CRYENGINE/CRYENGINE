option
------

Provides an option that the user can optionally select.

::

  option(<option_variable> "help string describing option"
         [initial value])

Provide an option for the user to select as ``ON`` or ``OFF``.  If no
initial value is provided, ``OFF`` is used.

If you have options that depend on the values of other options, see
the module help for :module:`CMakeDependentOption`.
