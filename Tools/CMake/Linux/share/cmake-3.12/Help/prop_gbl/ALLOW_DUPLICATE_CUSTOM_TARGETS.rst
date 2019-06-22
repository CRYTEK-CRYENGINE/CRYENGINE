ALLOW_DUPLICATE_CUSTOM_TARGETS
------------------------------

Allow duplicate custom targets to be created.

Normally CMake requires that all targets built in a project have
globally unique logical names (see policy CMP0002).  This is necessary
to generate meaningful project file names in Xcode and VS IDE
generators.  It also allows the target names to be referenced
unambiguously.

Makefile generators are capable of supporting duplicate custom target
names.  For projects that care only about Makefile generators and do
not wish to support Xcode or VS IDE generators, one may set this
property to true to allow duplicate custom targets.  The property
allows multiple add_custom_target command calls in different
directories to specify the same target name.  However, setting this
property will cause non-Makefile generators to produce an error and
refuse to generate the project.
