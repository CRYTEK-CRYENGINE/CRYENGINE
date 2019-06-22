IMPORTED_LOCATION
-----------------

Full path to the main file on disk for an IMPORTED target.

Set this to the location of an IMPORTED target file on disk.  For
executables this is the location of the executable file.  For bundles
on OS X this is the location of the executable file inside
Contents/MacOS under the application bundle folder.  For static
libraries and modules this is the location of the library or module.
For shared libraries on non-DLL platforms this is the location of the
shared library.  For frameworks on OS X this is the location of the
library file symlink just inside the framework folder.  For DLLs this
is the location of the ".dll" part of the library.  For UNKNOWN
libraries this is the location of the file to be linked.  Ignored for
non-imported targets.

Projects may skip IMPORTED_LOCATION if the configuration-specific
property IMPORTED_LOCATION_<CONFIG> is set.  To get the location of an
imported target read one of the LOCATION or LOCATION_<CONFIG>
properties.
