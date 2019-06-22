IOS_INSTALL_COMBINED
--------------------

Build a combined (device and simulator) target when installing.

When this property is set to set to false (which is the default) then it will
either be built with the device SDK or the simulator SDK depending on the SDK
set. But if this property is set to true then the target will at install time
also be built for the corresponding SDK and combined into one library.

This feature requires at least Xcode version 6.
