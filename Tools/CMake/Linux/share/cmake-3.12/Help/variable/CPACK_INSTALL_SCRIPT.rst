CPACK_INSTALL_SCRIPT
--------------------

Extra CMake script provided by the user.

If set this CMake script will be executed by CPack during its local
[CPack-private] installation which is done right before packaging the
files.  The script is not called by e.g.: ``make install``.
