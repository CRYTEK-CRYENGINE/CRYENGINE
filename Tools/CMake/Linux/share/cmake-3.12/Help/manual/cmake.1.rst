.. cmake-manual-description: CMake Command-Line Reference

cmake(1)
********

Synopsis
========

.. parsed-literal::

 cmake [<options>] {<path-to-source> | <path-to-existing-build>}
 cmake [{-D <var>=<value>}...] -P <cmake-script-file>
 cmake --build <dir> [<options>...] [-- <build-tool-options>...]
 cmake --open <dir>
 cmake -E <command> [<options>...]
 cmake --find-package <options>...

Description
===========

The "cmake" executable is the CMake command-line interface.  It may be
used to configure projects in scripts.  Project configuration settings
may be specified on the command line with the -D option.

CMake is a cross-platform build system generator.  Projects specify
their build process with platform-independent CMake listfiles included
in each directory of a source tree with the name CMakeLists.txt.
Users build a project by using CMake to generate a build system for a
native tool on their platform.

.. _`CMake Options`:

Options
=======

.. include:: OPTIONS_BUILD.txt

``-E <command> [<options>...]``
 See `Command-Line Tool Mode`_.

``-L[A][H]``
 List non-advanced cached variables.

 List cache variables will run CMake and list all the variables from
 the CMake cache that are not marked as INTERNAL or ADVANCED.  This
 will effectively display current CMake settings, which can then be
 changed with -D option.  Changing some of the variables may result
 in more variables being created.  If A is specified, then it will
 display also advanced variables.  If H is specified, it will also
 display help for each variable.

``--build <dir>``
 See `Build Tool Mode`_.

``--open <dir>``
 Open the generated project in the associated application.  This is
 only supported by some generators.

``-N``
 View mode only.

 Only load the cache.  Do not actually run configure and generate
 steps.

``-P <file>``
 Process script mode.

 Process the given cmake file as a script written in the CMake
 language.  No configure or generate step is performed and the cache
 is not modified.  If variables are defined using -D, this must be
 done before the -P argument.

``--find-package``
 See `Find-Package Tool Mode`_.

``--graphviz=[file]``
 Generate graphviz of dependencies, see :module:`CMakeGraphVizOptions` for more.

 Generate a graphviz input file that will contain all the library and
 executable dependencies in the project.  See the documentation for
 :module:`CMakeGraphVizOptions` for more details.

``--system-information [file]``
 Dump information about this system.

 Dump a wide range of information about the current system.  If run
 from the top of a binary tree for a CMake project it will dump
 additional information such as the cache, log files etc.

``--debug-trycompile``
 Do not delete the try_compile build tree. Only useful on one try_compile at a time.

 Do not delete the files and directories created for try_compile
 calls.  This is useful in debugging failed try_compiles.  It may
 however change the results of the try-compiles as old junk from a
 previous try-compile may cause a different test to either pass or
 fail incorrectly.  This option is best used for one try-compile at a
 time, and only when debugging.

``--debug-output``
 Put cmake in a debug mode.

 Print extra information during the cmake run like stack traces with
 message(send_error ) calls.

``--trace``
 Put cmake in trace mode.

 Print a trace of all calls made and from where.

``--trace-expand``
 Put cmake in trace mode.

 Like ``--trace``, but with variables expanded.

``--trace-source=<file>``
 Put cmake in trace mode, but output only lines of a specified file.

 Multiple options are allowed.

``--warn-uninitialized``
 Warn about uninitialized values.

 Print a warning when an uninitialized variable is used.

``--warn-unused-vars``
 Warn about unused variables.

 Find variables that are declared or set, but not used.

``--no-warn-unused-cli``
 Don't warn about command line options.

 Don't find variables that are declared on the command line, but not
 used.

``--check-system-vars``
 Find problems with variable usage in system files.

 Normally, unused and uninitialized variables are searched for only
 in CMAKE_SOURCE_DIR and CMAKE_BINARY_DIR.  This flag tells CMake to
 warn about other files as well.

.. include:: OPTIONS_HELP.txt

.. _`Build Tool Mode`:

Build Tool Mode
===============

CMake provides a command-line signature to build an already-generated
project binary tree::

 cmake --build <dir> [<options>...] [-- <build-tool-options>...]

This abstracts a native build tool's command-line interface with the
following options:

``--build <dir>``
  Project binary directory to be built.  This is required and must be first.

``-j [<jobs>], --parallel [<jobs>]``
  The maximum number of concurrent processes to use when building.
  If ``<jobs>`` is omitted the native build tool's default number is used.

  The :envvar:`CMAKE_BUILD_PARALLEL_LEVEL` environment variable, if set,
  specifies a default parallel level when this option is not given.

``--target <tgt>``
  Build ``<tgt>`` instead of default targets.  May only be specified once.

``--config <cfg>``
  For multi-configuration tools, choose configuration ``<cfg>``.

``--clean-first``
  Build target ``clean`` first, then build.
  (To clean only, use ``--target clean``.)

``--use-stderr``
  Ignored.  Behavior is default in CMake >= 3.0.

``--``
  Pass remaining options to the native tool.

Run ``cmake --build`` with no options for quick help.

Command-Line Tool Mode
======================

CMake provides builtin command-line tools through the signature::

 cmake -E <command> [<options>...]

Run ``cmake -E`` or ``cmake -E help`` for a summary of commands.
Available commands are:

``capabilities``
  Report cmake capabilities in JSON format. The output is a JSON object
  with the following keys:

  ``version``
    A JSON object with version information. Keys are:

    ``string``
      The full version string as displayed by cmake ``--version``.
    ``major``
      The major version number in integer form.
    ``minor``
      The minor version number in integer form.
    ``patch``
      The patch level in integer form.
    ``suffix``
      The cmake version suffix string.
    ``isDirty``
      A bool that is set if the cmake build is from a dirty tree.

  ``generators``
    A list available generators. Each generator is a JSON object with the
    following keys:

    ``name``
      A string containing the name of the generator.
    ``toolsetSupport``
      ``true`` if the generator supports toolsets and ``false`` otherwise.
    ``platformSupport``
      ``true`` if the generator supports platforms and ``false`` otherwise.
    ``extraGenerators``
      A list of strings with all the extra generators compatible with
      the generator.

  ``serverMode``
    ``true`` if cmake supports server-mode and ``false`` otherwise.

``chdir <dir> <cmd> [<arg>...]``
  Change the current working directory and run a command.

``compare_files <file1> <file2>``
  Check if ``<file1>`` is same as ``<file2>``. If files are the same,
  then returns 0, if not it returns 1.

``copy <file>... <destination>``
  Copy files to ``<destination>`` (either file or directory).
  If multiple files are specified, the ``<destination>`` must be
  directory and it must exist. Wildcards are not supported.

``copy_directory <dir>... <destination>``
  Copy directories to ``<destination>`` directory.
  If ``<destination>`` directory does not exist it will be created.

``copy_if_different <file>... <destination>``
  Copy files to ``<destination>`` (either file or directory) if
  they have changed.
  If multiple files are specified, the ``<destination>`` must be
  directory and it must exist.

``echo [<string>...]``
  Displays arguments as text.

``echo_append [<string>...]``
  Displays arguments as text but no new line.

``env [--unset=NAME]... [NAME=VALUE]... COMMAND [ARG]...``
  Run command in a modified environment.

``environment``
  Display the current environment variables.

``make_directory <dir>...``
  Create ``<dir>`` directories.  If necessary, create parent
  directories too.  If a directory already exists it will be
  silently ignored.

``md5sum <file>...``
  Create MD5 checksum of files in ``md5sum`` compatible format::

     351abe79cd3800b38cdfb25d45015a15  file1.txt
     052f86c15bbde68af55c7f7b340ab639  file2.txt

``sha1sum <file>...``
  Create SHA1 checksum of files in ``sha1sum`` compatible format::

     4bb7932a29e6f73c97bb9272f2bdc393122f86e0  file1.txt
     1df4c8f318665f9a5f2ed38f55adadb7ef9f559c  file2.txt

``sha224sum <file>...``
  Create SHA224 checksum of files in ``sha224sum`` compatible format::

     b9b9346bc8437bbda630b0b7ddfc5ea9ca157546dbbf4c613192f930  file1.txt
     6dfbe55f4d2edc5fe5c9197bca51ceaaf824e48eba0cc453088aee24  file2.txt

``sha256sum <file>...``
  Create SHA256 checksum of files in ``sha256sum`` compatible format::

     76713b23615d31680afeb0e9efe94d47d3d4229191198bb46d7485f9cb191acc  file1.txt
     15b682ead6c12dedb1baf91231e1e89cfc7974b3787c1e2e01b986bffadae0ea  file2.txt

``sha384sum <file>...``
  Create SHA384 checksum of files in ``sha384sum`` compatible format::

     acc049fedc091a22f5f2ce39a43b9057fd93c910e9afd76a6411a28a8f2b8a12c73d7129e292f94fc0329c309df49434  file1.txt
     668ddeb108710d271ee21c0f3acbd6a7517e2b78f9181c6a2ff3b8943af92b0195dcb7cce48aa3e17893173c0a39e23d  file2.txt

``sha512sum <file>...``
  Create SHA512 checksum of files in ``sha512sum`` compatible format::

     2a78d7a6c5328cfb1467c63beac8ff21794213901eaadafd48e7800289afbc08e5fb3e86aa31116c945ee3d7bf2a6194489ec6101051083d1108defc8e1dba89  file1.txt
     7a0b54896fe5e70cca6dd643ad6f672614b189bf26f8153061c4d219474b05dad08c4e729af9f4b009f1a1a280cb625454bf587c690f4617c27e3aebdf3b7a2d  file2.txt

``remove [-f] <file>...``
  Remove the file(s). If any of the listed files already do not
  exist, the command returns a non-zero exit code, but no message
  is logged. The ``-f`` option changes the behavior to return a
  zero exit code (i.e. success) in such situations instead.

``remove_directory <dir>``
  Remove a directory and its contents.  If a directory does
  not exist it will be silently ignored.

``rename <oldname> <newname>``
  Rename a file or directory (on one volume).

``server``
  Launch :manual:`cmake-server(7)` mode.

``sleep <number>...``
  Sleep for given number of seconds.

``tar [cxt][vf][zjJ] file.tar [<options>...] [--] [<file>...]``
  Create or extract a tar or zip archive.  Options are:

  ``--``
    Stop interpreting options and treat all remaining arguments
    as file names even if they start in ``-``.
  ``--files-from=<file>``
    Read file names from the given file, one per line.
    Blank lines are ignored.  Lines may not start in ``-``
    except for ``--add-file=<name>`` to add files whose
    names start in ``-``.
  ``--mtime=<date>``
    Specify modification time recorded in tarball entries.
  ``--format=<format>``
    Specify the format of the archive to be created.
    Supported formats are: ``7zip``, ``gnutar``, ``pax``,
    ``paxr`` (restricted pax, default), and ``zip``.

``time <command> [<args>...]``
  Run command and display elapsed time.

``touch <file>``
  Touch a file.

``touch_nocreate <file>``
  Touch a file if it exists but do not create it.  If a file does
  not exist it will be silently ignored.

UNIX-specific Command-Line Tools
--------------------------------

The following ``cmake -E`` commands are available only on UNIX:

``create_symlink <old> <new>``
  Create a symbolic link ``<new>`` naming ``<old>``.

.. note::
  Path to where ``<new>`` symbolic link will be created has to exist beforehand.

Windows-specific Command-Line Tools
-----------------------------------

The following ``cmake -E`` commands are available only on Windows:

``delete_regv <key>``
  Delete Windows registry value.

``env_vs8_wince <sdkname>``
  Displays a batch file which sets the environment for the provided
  Windows CE SDK installed in VS2005.

``env_vs9_wince <sdkname>``
  Displays a batch file which sets the environment for the provided
  Windows CE SDK installed in VS2008.

``write_regv <key> <value>``
  Write Windows registry value.

Find-Package Tool Mode
======================

CMake provides a helper for Makefile-based projects with the signature::

  cmake --find-package <options>...

This runs in a pkg-config like mode.

Search a package using :command:`find_package()` and print the resulting flags
to stdout.  This can be used to use cmake instead of pkg-config to find
installed libraries in plain Makefile-based projects or in autoconf-based
projects (via ``share/aclocal/cmake.m4``).

.. note::
  This mode is not well-supported due to some technical limitations.
  It is kept for compatibility but should not be used in new projects.

See Also
========

.. include:: LINKS.txt
