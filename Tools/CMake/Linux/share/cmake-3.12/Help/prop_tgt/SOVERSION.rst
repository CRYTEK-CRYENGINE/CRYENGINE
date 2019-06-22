SOVERSION
---------

What version number is this target.

For shared libraries :prop_tgt:`VERSION` and ``SOVERSION`` can be used to
specify the build version and API version respectively.  When building or
installing appropriate symlinks are created if the platform supports
symlinks and the linker supports so-names.  If only one of both is
specified the missing is assumed to have the same version number.
``SOVERSION`` is ignored if :prop_tgt:`NO_SONAME` property is set.

Windows Versions
^^^^^^^^^^^^^^^^

For shared libraries and executables on Windows the :prop_tgt:`VERSION`
attribute is parsed to extract a ``<major>.<minor>`` version number.
These numbers are used as the image version of the binary.

Mach-O Versions
^^^^^^^^^^^^^^^

For shared libraries and executables on Mach-O systems (e.g. OS X, iOS),
the ``SOVERSION`` property corresponds to *compatibility version* and
:prop_tgt:`VERSION` to *current version*.  See the :prop_tgt:`FRAMEWORK` target
property for an example.  Versions of Mach-O binaries may be checked with the
``otool -L <binary>`` command.
