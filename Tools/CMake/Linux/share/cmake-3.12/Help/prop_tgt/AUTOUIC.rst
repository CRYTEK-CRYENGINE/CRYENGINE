AUTOUIC
-------

Should the target be processed with autouic (for Qt projects).

``AUTOUIC`` is a boolean specifying whether CMake will handle
the Qt ``uic`` code generator automatically, i.e. without having to use
the :module:`QT4_WRAP_UI() <FindQt4>` or ``QT5_WRAP_UI()`` macro. Currently
Qt4 and Qt5 are supported.

When this property is ``ON``, CMake will scan the source files at build time
and invoke ``uic`` accordingly.  If an ``#include`` statement like
``#include "ui_foo.h"`` is found in ``source.cpp``, a ``foo.ui`` file is
searched for first in the vicinity of ``source.cpp`` and afterwards in the
optional :prop_tgt:`AUTOUIC_SEARCH_PATHS` of the target.
``uic`` is run on the ``foo.ui`` file to generate ``ui_foo.h`` in the directory
``<AUTOGEN_BUILD_DIR>/include``,
which is automatically added to the target's :prop_tgt:`INCLUDE_DIRECTORIES`.

* For :prop_gbl:`multi configuration generators <GENERATOR_IS_MULTI_CONFIG>`,
  the include directory is ``<AUTOGEN_BUILD_DIR>/include_<CONFIG>``.

* See :prop_tgt:`AUTOGEN_BUILD_DIR`.

This property is initialized by the value of the :variable:`CMAKE_AUTOUIC`
variable if it is set when a target is created.

Additional command line options for ``uic`` can be set via the
:prop_sf:`AUTOUIC_OPTIONS` source file property on the ``foo.ui`` file.
The global property :prop_gbl:`AUTOGEN_TARGETS_FOLDER` can be used to group the
autouic targets together in an IDE, e.g. in MSVS.

Source files can be excluded from :prop_tgt:`AUTOUIC` processing by
enabling :prop_sf:`SKIP_AUTOUIC` or the broader :prop_sf:`SKIP_AUTOGEN`.

The number of parallel ``uic`` processes to start can be modified by
setting :prop_tgt:`AUTOGEN_PARALLEL`.

See the :manual:`cmake-qt(7)` manual for more information on using CMake
with Qt.
