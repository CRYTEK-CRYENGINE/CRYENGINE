CMAKE_ANDROID_NDK_TOOLCHAIN_VERSION
-----------------------------------

When :ref:`Cross Compiling for Android with the NDK`, this variable
may be set to specify the version of the toolchain to be used
as the compiler.  The variable must be set to one of these forms:

* ``<major>.<minor>``: GCC of specified version
* ``clang<major>.<minor>``: Clang of specified version
* ``clang``: Clang of most recent available version

A toolchain of the requested version will be selected automatically to
match the ABI named in the :variable:`CMAKE_ANDROID_ARCH_ABI` variable.

If not specified, the default will be a value that selects the latest
available GCC toolchain.
