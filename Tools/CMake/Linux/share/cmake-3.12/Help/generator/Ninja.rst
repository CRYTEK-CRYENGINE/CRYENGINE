Ninja
-----

Generates build.ninja files.

A build.ninja file is generated into the build tree.  Recent versions
of the ninja program can build the project through the "all" target.
An "install" target is also provided.

For each subdirectory ``sub/dir`` of the project, additional targets
are generated:

``sub/dir/all``
  Depends on all targets required by the subdirectory.

``sub/dir/install``
  Runs the install step in the subdirectory, if any.

``sub/dir/test``
  Runs the test step in the subdirectory, if any.

``sub/dir/package``
  Runs the package step in the subdirectory, if any.

Fortran Support
^^^^^^^^^^^^^^^

The ``Ninja`` generator conditionally supports Fortran when the ``ninja``
tool has the required features.  As of this version of CMake the needed
features have not been integrated into upstream Ninja.  Kitware maintains
a branch of Ninja with the required features on `github.com/Kitware/ninja`_.

.. _`github.com/Kitware/ninja`: https://github.com/Kitware/ninja/tree/features-for-fortran#readme
