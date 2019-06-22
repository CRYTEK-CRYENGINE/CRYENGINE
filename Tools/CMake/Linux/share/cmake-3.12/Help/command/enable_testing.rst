enable_testing
--------------

Enable testing for current directory and below.

::

  enable_testing()

Enables testing for this directory and below.  See also the
:command:`add_test` command.  Note that ctest expects to find a test file
in the build directory root.  Therefore, this command should be in the
source directory root.
