get_filename_component
----------------------

Get a specific component of a full filename.

------------------------------------------------------------------------------

::

  get_filename_component(<VAR> <FileName> <COMP> [CACHE])

Set ``<VAR>`` to a component of ``<FileName>``, where ``<COMP>`` is one of:

::

 DIRECTORY = Directory without file name
 NAME      = File name without directory
 EXT       = File name longest extension (.b.c from d/a.b.c)
 NAME_WE   = File name without directory or longest extension
 PATH      = Legacy alias for DIRECTORY (use for CMake <= 2.8.11)

Paths are returned with forward slashes and have no trailing slashes.
The longest file extension is always considered.  If the optional
``CACHE`` argument is specified, the result variable is added to the
cache.

------------------------------------------------------------------------------

::

  get_filename_component(<VAR> <FileName>
                         <COMP> [BASE_DIR <BASE_DIR>]
                         [CACHE])

Set ``<VAR>`` to the absolute path of ``<FileName>``, where ``<COMP>`` is one
of:

::

 ABSOLUTE  = Full path to file
 REALPATH  = Full path to existing file with symlinks resolved

If the provided ``<FileName>`` is a relative path, it is evaluated relative
to the given base directory ``<BASE_DIR>``.  If no base directory is
provided, the default base directory will be
:variable:`CMAKE_CURRENT_SOURCE_DIR`.

Paths are returned with forward slashes and have no trailing slashes.  If the
optional ``CACHE`` argument is specified, the result variable is added to the
cache.

------------------------------------------------------------------------------

::

  get_filename_component(<VAR> <FileName>
                         PROGRAM [PROGRAM_ARGS <ARG_VAR>]
                         [CACHE])

The program in ``<FileName>`` will be found in the system search path or
left as a full path.  If ``PROGRAM_ARGS`` is present with ``PROGRAM``, then
any command-line arguments present in the ``<FileName>`` string are split
from the program name and stored in ``<ARG_VAR>``.  This is used to
separate a program name from its arguments in a command line string.
