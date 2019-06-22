CROSSCOMPILING_EMULATOR
-----------------------

Use the given emulator to run executables created when crosscompiling.
This command will be added as a prefix to :command:`add_test`,
:command:`add_custom_command`, and :command:`add_custom_target` commands
for built target system executables.

This property is initialized by the value of the
:variable:`CMAKE_CROSSCOMPILING_EMULATOR` variable if it is set when a target
is created.
