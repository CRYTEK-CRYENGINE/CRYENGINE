CMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY
---------------------------------------------

Skip :ref:`System Package Registry` in :command:`find_package` calls.

In some cases, it is not desirable to use the
:ref:`System Package Registry` when searching for packages. If the
:variable:`CMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY` variable is
enabled, all the :command:`find_package` commands will skip
the :ref:`System Package Registry` as if they were called with the
``NO_CMAKE_SYSTEM_PACKAGE_REGISTRY`` argument.

See also :ref:`Disabling the Package Registry`.
