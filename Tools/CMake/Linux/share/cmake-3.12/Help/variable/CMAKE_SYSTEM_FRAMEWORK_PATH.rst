CMAKE_SYSTEM_FRAMEWORK_PATH
---------------------------

Search path for OS X frameworks used by the :command:`find_library`,
:command:`find_package`, :command:`find_path`, and :command:`find_file`
commands.  By default it contains the standard directories for the
current system.  It is *not* intended to be modified by the project,
use :variable:`CMAKE_FRAMEWORK_PATH` for this.
