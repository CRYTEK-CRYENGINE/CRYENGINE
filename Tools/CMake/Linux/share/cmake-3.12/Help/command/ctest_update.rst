ctest_update
------------

Perform the :ref:`CTest Update Step` as a :ref:`Dashboard Client`.

::

  ctest_update([SOURCE <source-dir>] [RETURN_VALUE <result-var>] [QUIET])

Update the source tree from version control and record results in
``Update.xml`` for submission with the :command:`ctest_submit` command.

The options are:

``SOURCE <source-dir>``
  Specify the source directory.  If not given, the
  :variable:`CTEST_SOURCE_DIRECTORY` variable is used.

``RETURN_VALUE <result-var>``
  Store in the ``<result-var>`` variable the number of files
  updated or ``-1`` on error.

``QUIET``
  Tell CTest to suppress most non-error messages that it would
  have otherwise printed to the console.  CTest will still report
  the new revision of the repository and any conflicting files
  that were found.

The update always follows the version control branch currently checked
out in the source directory.  See the :ref:`CTest Update Step`
documentation for more information.
