CMAKE_EXPORT_NO_PACKAGE_REGISTRY
--------------------------------

Disable the :command:`export(PACKAGE)` command.

In some cases, for example for packaging and for system wide
installations, it is not desirable to write the user package registry.
If the :variable:`CMAKE_EXPORT_NO_PACKAGE_REGISTRY` variable is enabled,
the :command:`export(PACKAGE)` command will do nothing.

See also :ref:`Disabling the Package Registry`.
