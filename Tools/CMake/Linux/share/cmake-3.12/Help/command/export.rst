export
------

Export targets from the build tree for use by outside projects.

::

  export(EXPORT <export-name> [NAMESPACE <namespace>] [FILE <filename>])

Create a file ``<filename>`` that may be included by outside projects to
import targets from the current project's build tree.  This is useful
during cross-compiling to build utility executables that can run on
the host platform in one project and then import them into another
project being compiled for the target platform.  If the ``NAMESPACE``
option is given the ``<namespace>`` string will be prepended to all target
names written to the file.

Target installations are associated with the export ``<export-name>``
using the ``EXPORT`` option of the :command:`install(TARGETS)` command.

The file created by this command is specific to the build tree and
should never be installed.  See the :command:`install(EXPORT)` command to
export targets from an installation tree.

The properties set on the generated IMPORTED targets will have the
same values as the final values of the input TARGETS.

::

  export(TARGETS [target1 [target2 [...]]] [NAMESPACE <namespace>]
         [APPEND] FILE <filename> [EXPORT_LINK_INTERFACE_LIBRARIES])

This signature is similar to the ``EXPORT`` signature, but targets are listed
explicitly rather than specified as an export-name.  If the APPEND option is
given the generated code will be appended to the file instead of overwriting it.
The EXPORT_LINK_INTERFACE_LIBRARIES keyword, if present, causes the
contents of the properties matching
``(IMPORTED_)?LINK_INTERFACE_LIBRARIES(_<CONFIG>)?`` to be exported, when
policy CMP0022 is NEW.  If a library target is included in the export
but a target to which it links is not included the behavior is
unspecified.

.. note::

  :ref:`Object Libraries` under :generator:`Xcode` have special handling if
  multiple architectures are listed in :variable:`CMAKE_OSX_ARCHITECTURES`.
  In this case they will be exported as :ref:`Interface Libraries` with
  no object files available to clients.  This is sufficient to satisfy
  transitive usage requirements of other targets that link to the
  object libraries in their implementation.

::

  export(PACKAGE <name>)

Store the current build directory in the CMake user package registry
for package ``<name>``.  The find_package command may consider the
directory while searching for package ``<name>``.  This helps dependent
projects find and use a package from the current project's build tree
without help from the user.  Note that the entry in the package
registry that this command creates works only in conjunction with a
package configuration file (``<name>Config.cmake``) that works with the
build tree. In some cases, for example for packaging and for system
wide installations, it is not desirable to write the user package
registry. If the :variable:`CMAKE_EXPORT_NO_PACKAGE_REGISTRY` variable
is enabled, the ``export(PACKAGE)`` command will do nothing.

::

  export(TARGETS [target1 [target2 [...]]]  [ANDROID_MK <filename>])

This signature exports cmake built targets to the android ndk build system
by creating an Android.mk file that references the prebuilt targets. The
Android NDK supports the use of prebuilt libraries, both static and shared.
This allows cmake to build the libraries of a project and make them available
to an ndk build system complete with transitive dependencies, include flags
and defines required to use the libraries. The signature takes a list of
targets and puts them in the Android.mk file specified by the ``<filename>``
given. This signature can only be used if policy CMP0022 is NEW for all
targets given. A error will be issued if that policy is set to OLD for one
of the targets.
