load_cache
----------

Load in the values from another project's CMake cache.

::

  load_cache(pathToCacheFile READ_WITH_PREFIX
             prefix entry1...)

Read the cache and store the requested entries in variables with their
name prefixed with the given prefix.  This only reads the values, and
does not create entries in the local project's cache.

::

  load_cache(pathToCacheFile [EXCLUDE entry1...]
             [INCLUDE_INTERNALS entry1...])

Load in the values from another cache and store them in the local
project's cache as internal entries.  This is useful for a project
that depends on another project built in a different tree.  ``EXCLUDE``
option can be used to provide a list of entries to be excluded.
``INCLUDE_INTERNALS`` can be used to provide a list of internal entries to
be included.  Normally, no internal entries are brought in.  Use of
this form of the command is strongly discouraged, but it is provided
for backward compatibility.
