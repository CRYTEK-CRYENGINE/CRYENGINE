SKIP_AUTORCC
------------

Exclude the source file from :prop_tgt:`AUTORCC` processing (for Qt projects).

For broader exclusion control see :prop_sf:`SKIP_AUTOGEN`.

EXAMPLE
^^^^^^^

.. code-block:: cmake

  # ...
  set_property(SOURCE file.qrc PROPERTY SKIP_AUTORCC ON)
  # ...
