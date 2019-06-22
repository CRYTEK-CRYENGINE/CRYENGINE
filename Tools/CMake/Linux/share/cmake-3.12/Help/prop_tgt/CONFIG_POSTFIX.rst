<CONFIG>_POSTFIX
----------------

Postfix to append to the target file name for configuration <CONFIG>.

When building with configuration <CONFIG> the value of this property
is appended to the target file name built on disk.  For non-executable
targets, this property is initialized by the value of the variable
CMAKE_<CONFIG>_POSTFIX if it is set when a target is created.  This
property is ignored on the Mac for Frameworks and App Bundles.
