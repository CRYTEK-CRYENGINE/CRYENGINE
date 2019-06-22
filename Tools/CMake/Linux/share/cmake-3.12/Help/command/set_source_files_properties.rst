set_source_files_properties
---------------------------

Source files can have properties that affect how they are built.

::

  set_source_files_properties([file1 [file2 [...]]]
                              PROPERTIES prop1 value1
                              [prop2 value2 [...]])

Set properties associated with source files using a key/value paired
list.  See :ref:`Source File Properties` for the list of properties known
to CMake.  Source file properties are visible only to targets added
in the same directory (CMakeLists.txt).
