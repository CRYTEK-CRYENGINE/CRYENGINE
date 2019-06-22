CMAKE_SYSTEM_PREFIX_PATH
------------------------

:ref:`;-list <CMake Language Lists>` of directories specifying installation
*prefixes* to be searched by the :command:`find_package`,
:command:`find_program`, :command:`find_library`, :command:`find_file`, and
:command:`find_path` commands.  Each command will add appropriate
subdirectories (like ``bin``, ``lib``, or ``include``) as specified in its own
documentation.

By default this contains the standard directories for the current system, the
:variable:`CMAKE_INSTALL_PREFIX`, and the :variable:`CMAKE_STAGING_PREFIX`.
The installation and staging prefixes may be excluded by setting
the :variable:`CMAKE_FIND_NO_INSTALL_PREFIX` variable.

``CMAKE_SYSTEM_PREFIX_PATH`` is *not* intended to be modified by the project;
use :variable:`CMAKE_PREFIX_PATH` for this.

See also :variable:`CMAKE_SYSTEM_INCLUDE_PATH`,
:variable:`CMAKE_SYSTEM_LIBRARY_PATH`, :variable:`CMAKE_SYSTEM_PROGRAM_PATH`,
and :variable:`CMAKE_SYSTEM_IGNORE_PATH`.
