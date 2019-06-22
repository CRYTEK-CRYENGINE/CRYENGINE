execute_process
---------------

Execute one or more child processes.

.. code-block:: cmake

  execute_process(COMMAND <cmd1> [args1...]]
                  [COMMAND <cmd2> [args2...] [...]]
                  [WORKING_DIRECTORY <directory>]
                  [TIMEOUT <seconds>]
                  [RESULT_VARIABLE <variable>]
                  [RESULTS_VARIABLE <variable>]
                  [OUTPUT_VARIABLE <variable>]
                  [ERROR_VARIABLE <variable>]
                  [INPUT_FILE <file>]
                  [OUTPUT_FILE <file>]
                  [ERROR_FILE <file>]
                  [OUTPUT_QUIET]
                  [ERROR_QUIET]
                  [OUTPUT_STRIP_TRAILING_WHITESPACE]
                  [ERROR_STRIP_TRAILING_WHITESPACE]
                  [ENCODING <name>])

Runs the given sequence of one or more commands in parallel with the standard
output of each process piped to the standard input of the next.
A single standard error pipe is used for all processes.

Options:

``COMMAND``
 A child process command line.

 CMake executes the child process using operating system APIs directly.
 All arguments are passed VERBATIM to the child process.
 No intermediate shell is used, so shell operators such as ``>``
 are treated as normal arguments.
 (Use the ``INPUT_*``, ``OUTPUT_*``, and ``ERROR_*`` options to
 redirect stdin, stdout, and stderr.)

 If a sequential execution of multiple commands is required, use multiple
 :command:`execute_process` calls with a single ``COMMAND`` argument.

``WORKING_DIRECTORY``
 The named directory will be set as the current working directory of
 the child processes.

``TIMEOUT``
 The child processes will be terminated if they do not finish in the
 specified number of seconds (fractions are allowed).

``RESULT_VARIABLE``
 The variable will be set to contain the result of last child process.
 This will be an integer return code from the last child or a string
 describing an error condition.

``RESULTS_VARIABLE <variable>``
 The variable will be set to contain the result of all processes as a
 :ref:`;-list <CMake Language Lists>`, in order of the given ``COMMAND``
 arguments.  Each entry will be an integer return code from the
 corresponding child or a string describing an error condition.

``OUTPUT_VARIABLE``, ``ERROR_VARIABLE``
 The variable named will be set with the contents of the standard output
 and standard error pipes, respectively.  If the same variable is named
 for both pipes their output will be merged in the order produced.

``INPUT_FILE, OUTPUT_FILE``, ``ERROR_FILE``
 The file named will be attached to the standard input of the first
 process, standard output of the last process, or standard error of
 all processes, respectively.  If the same file is named for both
 output and error then it will be used for both.

``OUTPUT_QUIET``, ``ERROR_QUIET``
 The standard output or standard error results will be quietly ignored.

``ENCODING <name>``
 On Windows, the encoding that is used to decode output from the process.
 Ignored on other platforms.
 Valid encoding names are:

 ``NONE``
   Perform no decoding.  This assumes that the process output is encoded
   in the same way as CMake's internal encoding (UTF-8).
   This is the default.
 ``AUTO``
   Use the current active console's codepage or if that isn't
   available then use ANSI.
 ``ANSI``
   Use the ANSI codepage.
 ``OEM``
   Use the original equipment manufacturer (OEM) code page.
 ``UTF8`` or ``UTF-8``
   Use the UTF-8 codepage. Prior to CMake 3.11.0, only ``UTF8`` was accepted
   for this encoding. In CMake 3.11.0, ``UTF-8`` was added for consistency with
   the `UTF-8 RFC <https://www.ietf.org/rfc/rfc3629>`_ naming convention.

If more than one ``OUTPUT_*`` or ``ERROR_*`` option is given for the
same pipe the precedence is not specified.
If no ``OUTPUT_*`` or ``ERROR_*`` options are given the output will
be shared with the corresponding pipes of the CMake process itself.

The :command:`execute_process` command is a newer more powerful version of
:command:`exec_program`, but the old command has been kept for compatibility.
Both commands run while CMake is processing the project prior to build
system generation.  Use :command:`add_custom_target` and
:command:`add_custom_command` to create custom commands that run at
build time.
