CMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY
--------------------------------------

Skip :ref:`User Package Registry` in :command:`find_package` calls.

In some cases, for example to locate only system wide installations, it
is not desirable to use the :ref:`User Package Registry` when searching
for packages. If the :variable:`CMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY`
variable is enabled, all the :command:`find_package` commands will skip
the :ref:`User Package Registry` as if they were called with the
``NO_CMAKE_PACKAGE_REGISTRY`` argument.

See also :ref:`Disabling the Package Registry`.
