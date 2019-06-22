separate_arguments
------------------

Parse space-separated arguments into a semicolon-separated list.

::

  separate_arguments(<var> <NATIVE|UNIX|WINDOWS>_COMMAND "<args>")

Parses a UNIX- or Windows-style command-line string "<args>" and
stores a semicolon-separated list of the arguments in ``<var>``.  The
entire command line must be given in one "<args>" argument.

The ``UNIX_COMMAND`` mode separates arguments by unquoted whitespace.  It
recognizes both single-quote and double-quote pairs.  A backslash
escapes the next literal character (``\"`` is ``"``); there are no special
escapes (``\n`` is just ``n``).

The ``WINDOWS_COMMAND`` mode parses a Windows command-line using the same
syntax the runtime library uses to construct argv at startup.  It
separates arguments by whitespace that is not double-quoted.
Backslashes are literal unless they precede double-quotes.  See the
MSDN article `Parsing C Command-Line Arguments`_ for details.

The ``NATIVE_COMMAND`` mode parses a Windows command-line if the host
system is Windows, and a UNIX command-line otherwise.

.. _`Parsing C Command-Line Arguments`: https://msdn.microsoft.com/library/a1y7w461.aspx

::

  separate_arguments(<var>)

Convert the value of ``<var>`` to a semi-colon separated list.  All
spaces are replaced with ';'.  This helps with generating command
lines.
