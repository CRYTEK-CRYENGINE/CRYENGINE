XCODE_FILE_ATTRIBUTES
---------------------

Add values to the Xcode ``ATTRIBUTES`` setting on its reference to a
source file.  Among other things, this can be used to set the role on
a mig file::

  set_source_files_properties(defs.mig
      PROPERTIES
          XCODE_FILE_ATTRIBUTES "Client;Server"
  )
