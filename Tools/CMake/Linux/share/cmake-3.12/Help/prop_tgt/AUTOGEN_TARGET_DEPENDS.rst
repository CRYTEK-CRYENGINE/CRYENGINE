AUTOGEN_TARGET_DEPENDS
----------------------

Target dependencies of the corresponding ``_autogen`` target.

Targets which have their :prop_tgt:`AUTOMOC` or :prop_tgt:`AUTOUIC` property
``ON`` have a corresponding ``_autogen`` target which is used to auto generate
``moc`` and ``uic`` files.  As this ``_autogen`` target is created at
generate-time, it is not possible to define dependencies of it,
such as to create inputs for the ``moc`` or ``uic`` executable.

The :prop_tgt:`AUTOGEN_TARGET_DEPENDS` target property can be set instead to a
list of dependencies of the ``_autogen`` target.  Dependencies can be target
names or file names.

See the :manual:`cmake-qt(7)` manual for more information on using CMake
with Qt.

Use cases
^^^^^^^^^

If :prop_tgt:`AUTOMOC` or :prop_tgt:`AUTOUIC` depends on a file that is either

- a :prop_sf:`GENERATED` non C++ file (e.g. a :prop_sf:`GENERATED` ``.json``
  or ``.ui`` file) or
- a :prop_sf:`GENERATED` C++ file that isn't recognized by :prop_tgt:`AUTOMOC`
  and :prop_tgt:`AUTOUIC` because it's skipped by :prop_sf:`SKIP_AUTOMOC`,
  :prop_sf:`SKIP_AUTOUIC`, :prop_sf:`SKIP_AUTOGEN` or :policy:`CMP0071` or
- a file that isn't in the target's sources

it must added to :prop_tgt:`AUTOGEN_TARGET_DEPENDS`.
