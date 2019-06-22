CMAKE_<LANG>_FLAGS_INIT
-----------------------

Value used to initialize the :variable:`CMAKE_<LANG>_FLAGS` cache entry
the first time a build tree is configured for language ``<LANG>``.
This variable is meant to be set by a :variable:`toolchain file
<CMAKE_TOOLCHAIN_FILE>`.  CMake may prepend or append content to
the value based on the environment and target platform.

See also the configuration-specific
:variable:`CMAKE_<LANG>_FLAGS_<CONFIG>_INIT` variable.
