message
-------

Display a message to the user.

::

  message([<mode>] "message to display" ...)

The optional ``<mode>`` keyword determines the type of message:

::

  (none)         = Important information
  STATUS         = Incidental information
  WARNING        = CMake Warning, continue processing
  AUTHOR_WARNING = CMake Warning (dev), continue processing
  SEND_ERROR     = CMake Error, continue processing,
                                but skip generation
  FATAL_ERROR    = CMake Error, stop processing and generation
  DEPRECATION    = CMake Deprecation Error or Warning if variable
                   CMAKE_ERROR_DEPRECATED or CMAKE_WARN_DEPRECATED
                   is enabled, respectively, else no message.

The CMake command-line tool displays STATUS messages on stdout and all
other message types on stderr.  The CMake GUI displays all messages in
its log area.  The interactive dialogs (ccmake and CMakeSetup) show
STATUS messages one at a time on a status line and other messages in
interactive pop-up boxes.

CMake Warning and Error message text displays using a simple markup
language.  Non-indented text is formatted in line-wrapped paragraphs
delimited by newlines.  Indented text is considered pre-formatted.
