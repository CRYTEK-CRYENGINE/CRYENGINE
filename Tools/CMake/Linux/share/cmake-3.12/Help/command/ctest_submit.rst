ctest_submit
------------

Perform the :ref:`CTest Submit Step` as a :ref:`Dashboard Client`.

::

  ctest_submit([PARTS <part>...] [FILES <file>...]
               [HTTPHEADER <header>]
               [RETRY_COUNT <count>]
               [RETRY_DELAY <delay>]
               [RETURN_VALUE <result-var>]
               [QUIET]
               )

Submit results to a dashboard server.
By default all available parts are submitted.

The options are:

``PARTS <part>...``
  Specify a subset of parts to submit.  Valid part names are::

    Start      = nothing
    Update     = ctest_update results, in Update.xml
    Configure  = ctest_configure results, in Configure.xml
    Build      = ctest_build results, in Build.xml
    Test       = ctest_test results, in Test.xml
    Coverage   = ctest_coverage results, in Coverage.xml
    MemCheck   = ctest_memcheck results, in DynamicAnalysis.xml
    Notes      = Files listed by CTEST_NOTES_FILES, in Notes.xml
    ExtraFiles = Files listed by CTEST_EXTRA_SUBMIT_FILES
    Upload     = Files prepared for upload by ctest_upload(), in Upload.xml
    Submit     = nothing

``FILES <file>...``
  Specify an explicit list of specific files to be submitted.
  Each individual file must exist at the time of the call.

``HTTPHEADER <HTTP-header>``
  Specify HTTP header to be included in the request to CDash during submission.
  This suboption can be repeated several times.

``RETRY_COUNT <count>``
  Specify how many times to retry a timed-out submission.

``RETRY_DELAY <delay>``
  Specify how long (in seconds) to wait after a timed-out submission
  before attempting to re-submit.

``RETURN_VALUE <result-var>``
  Store in the ``<result-var>`` variable ``0`` for success and
  non-zero on failure.

``QUIET``
  Suppress all non-error messages that would have otherwise been
  printed to the console.

Submit to CDash Upload API
^^^^^^^^^^^^^^^^^^^^^^^^^^

::

  ctest_submit(CDASH_UPLOAD <file> [CDASH_UPLOAD_TYPE <type>]
               [HTTPHEADER <header>]
               [RETRY_COUNT <count>]
               [RETRY_DELAY <delay>]
               [QUIET])

This second signature is used to upload files to CDash via the CDash
file upload API. The API first sends a request to upload to CDash along
with a content hash of the file. If CDash does not already have the file,
then it is uploaded. Along with the file, a CDash type string is specified
to tell CDash which handler to use to process the data.

This signature accepts the ``HTTPHEADER``, ``RETRY_COUNT``, ``RETRY_DELAY``, and
``QUIET`` options as described above.
