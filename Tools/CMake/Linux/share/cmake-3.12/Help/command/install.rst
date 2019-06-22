install
-------

Specify rules to run at install time.

Synopsis
^^^^^^^^

.. parsed-literal::

  install(`TARGETS`_ <target>... [...])
  install({`FILES`_ | `PROGRAMS`_} <file>... DESTINATION <dir> [...])
  install(`DIRECTORY`_ <dir>... DESTINATION <dir> [...])
  install(`SCRIPT`_ <file> [...])
  install(`CODE`_ <code> [...])
  install(`EXPORT`_ <export-name> DESTINATION <dir> [...])

Introduction
^^^^^^^^^^^^

This command generates installation rules for a project.  Rules
specified by calls to this command within a source directory are
executed in order during installation.  The order across directories
is not defined.

There are multiple signatures for this command.  Some of them define
installation options for files and targets.  Options common to
multiple signatures are covered here but they are valid only for
signatures that specify them.  The common options are:

``DESTINATION``
  Specify the directory on disk to which a file will be installed.
  If a full path (with a leading slash or drive letter) is given
  it is used directly.  If a relative path is given it is interpreted
  relative to the value of the :variable:`CMAKE_INSTALL_PREFIX` variable.
  The prefix can be relocated at install time using the ``DESTDIR``
  mechanism explained in the :variable:`CMAKE_INSTALL_PREFIX` variable
  documentation.

``PERMISSIONS``
  Specify permissions for installed files.  Valid permissions are
  ``OWNER_READ``, ``OWNER_WRITE``, ``OWNER_EXECUTE``, ``GROUP_READ``,
  ``GROUP_WRITE``, ``GROUP_EXECUTE``, ``WORLD_READ``, ``WORLD_WRITE``,
  ``WORLD_EXECUTE``, ``SETUID``, and ``SETGID``.  Permissions that do
  not make sense on certain platforms are ignored on those platforms.

``CONFIGURATIONS``
  Specify a list of build configurations for which the install rule
  applies (Debug, Release, etc.). Note that the values specified for
  this option only apply to options listed AFTER the ``CONFIGURATIONS``
  option. For example, to set separate install paths for the Debug and
  Release configurations, do the following:

  .. code-block:: cmake

    install(TARGETS target
            CONFIGURATIONS Debug
            RUNTIME DESTINATION Debug/bin)
    install(TARGETS target
            CONFIGURATIONS Release
            RUNTIME DESTINATION Release/bin)

  Note that ``CONFIGURATIONS`` appears BEFORE ``RUNTIME DESTINATION``.

``COMPONENT``
  Specify an installation component name with which the install rule
  is associated, such as "runtime" or "development".  During
  component-specific installation only install rules associated with
  the given component name will be executed.  During a full installation
  all components are installed unless marked with ``EXCLUDE_FROM_ALL``.
  If ``COMPONENT`` is not provided a default component "Unspecified" is
  created.  The default component name may be controlled with the
  :variable:`CMAKE_INSTALL_DEFAULT_COMPONENT_NAME` variable.

``EXCLUDE_FROM_ALL``
  Specify that the file is excluded from a full installation and only
  installed as part of a component-specific installation

``RENAME``
  Specify a name for an installed file that may be different from the
  original file.  Renaming is allowed only when a single file is
  installed by the command.

``OPTIONAL``
  Specify that it is not an error if the file to be installed does
  not exist.

Command signatures that install files may print messages during
installation.  Use the :variable:`CMAKE_INSTALL_MESSAGE` variable
to control which messages are printed.

Installing Targets
^^^^^^^^^^^^^^^^^^

.. _TARGETS:

::

  install(TARGETS targets... [EXPORT <export-name>]
          [[ARCHIVE|LIBRARY|RUNTIME|OBJECTS|FRAMEWORK|BUNDLE|
            PRIVATE_HEADER|PUBLIC_HEADER|RESOURCE]
           [DESTINATION <dir>]
           [PERMISSIONS permissions...]
           [CONFIGURATIONS [Debug|Release|...]]
           [COMPONENT <component>]
           [NAMELINK_COMPONENT <component>]
           [OPTIONAL] [EXCLUDE_FROM_ALL]
           [NAMELINK_ONLY|NAMELINK_SKIP]
          ] [...]
          [INCLUDES DESTINATION [<dir> ...]]
          )

The ``TARGETS`` form specifies rules for installing targets from a
project.  There are several kinds of target files that may be installed:

``ARCHIVE``
  Static libraries are treated as ``ARCHIVE`` targets, except those
  marked with the ``FRAMEWORK`` property on OS X (see ``FRAMEWORK``
  below.) For DLL platforms (all Windows-based systems including
  Cygwin), the DLL import library is treated as an ``ARCHIVE`` target.

``LIBRARY``
  Module libraries are always treated as ``LIBRARY`` targets. For non-
  DLL platforms shared libraries are treated as ``LIBRARY`` targets,
  except those marked with the ``FRAMEWORK`` property on OS X (see
  ``FRAMEWORK`` below.)

``RUNTIME``
  Executables are treated as ``RUNTIME`` objects, except those marked
  with the ``MACOSX_BUNDLE`` property on OS X (see ``BUNDLE`` below.)
  For DLL platforms (all Windows-based systems including Cygwin), the
  DLL part of a shared library is treated as a ``RUNTIME`` target.

``OBJECTS``
  Object libraries (a simple group of object files) are always treated
  as ``OBJECTS`` targets.

``FRAMEWORK``
  Both static and shared libraries marked with the ``FRAMEWORK``
  property are treated as ``FRAMEWORK`` targets on OS X.

``BUNDLE``
  Executables marked with the ``MACOSX_BUNDLE`` property are treated as
  ``BUNDLE`` targets on OS X.

``PUBLIC_HEADER``
  Any ``PUBLIC_HEADER`` files associated with a library are installed in
  the destination specified by the ``PUBLIC_HEADER`` argument on non-Apple
  platforms. Rules defined by this argument are ignored for ``FRAMEWORK``
  libraries on Apple platforms because the associated files are installed
  into the appropriate locations inside the framework folder. See
  :prop_tgt:`PUBLIC_HEADER` for details.

``PRIVATE_HEADER``
  Similar to ``PUBLIC_HEADER``, but for ``PRIVATE_HEADER`` files. See
  :prop_tgt:`PRIVATE_HEADER` for details.

``RESOURCE``
  Similar to ``PUBLIC_HEADER`` and ``PRIVATE_HEADER``, but for
  ``RESOURCE`` files. See :prop_tgt:`RESOURCE` for details.

For each of these arguments given, the arguments following them only apply
to the target or file type specified in the argument. If none is given, the
installation properties apply to all target types. If only one is given then
only targets of that type will be installed (which can be used to install
just a DLL or just an import library.)

In addition to the common options listed above, each target can accept
the following additional arguments:

``NAMELINK_COMPONENT``
  On some platforms a versioned shared library has a symbolic link such
  as::

    lib<name>.so -> lib<name>.so.1

  where ``lib<name>.so.1`` is the soname of the library and ``lib<name>.so``
  is a "namelink" allowing linkers to find the library when given
  ``-l<name>``. The ``NAMELINK_COMPONENT`` option is similar to the
  ``COMPONENT`` option, but it changes the installation component of a shared
  library namelink if one is generated. If not specified, this defaults to the
  value of ``COMPONENT``. It is an error to use this parameter outside of a
  ``LIBRARY`` block.

  Consider the following example:

  .. code-block:: cmake

    install(TARGETS mylib
            LIBRARY
              DESTINATION lib
              COMPONENT Libraries
              NAMELINK_COMPONENT Development
            PUBLIC_HEADER
              DESTINATION include
              COMPONENT Development
           )

  In this scenario, if you choose to install only the ``Development``
  component, both the headers and namelink will be installed without the
  library. (If you don't also install the ``Libraries`` component, the
  namelink will be a dangling symlink, and projects that link to the library
  will have build errors.) If you install only the ``Libraries`` component,
  only the library will be installed, without the headers and namelink.

  This option is typically used for package managers that have separate
  runtime and development packages. For example, on Debian systems, the
  library is expected to be in the runtime package, and the headers and
  namelink are expected to be in the development package.

  See the :prop_tgt:`VERSION` and :prop_tgt:`SOVERSION` target properties for
  details on creating versioned shared libraries.

``NAMELINK_ONLY``
  This option causes the installation of only the namelink when a library
  target is installed. On platforms where versioned shared libraries do not
  have namelinks or when a library is not versioned, the ``NAMELINK_ONLY``
  option installs nothing. It is an error to use this parameter outside of a
  ``LIBRARY`` block.

  When ``NAMELINK_ONLY`` is given, either ``NAMELINK_COMPONENT`` or
  ``COMPONENT`` may be used to specify the installation component of the
  namelink, but ``COMPONENT`` should generally be preferred.

``NAMELINK_SKIP``
  Similar to ``NAMELINK_ONLY``, but it has the opposite effect: it causes the
  installation of library files other than the namelink when a library target
  is installed. When neither ``NAMELINK_ONLY`` or ``NAMELINK_SKIP`` are given,
  both portions are installed. On platforms where versioned shared libraries
  do not have symlinks or when a library is not versioned, ``NAMELINK_SKIP``
  installs the library. It is an error to use this parameter outside of a
  ``LIBRARY`` block.

  If ``NAMELINK_SKIP`` is specified, ``NAMELINK_COMPONENT`` has no effect. It
  is not recommended to use ``NAMELINK_SKIP`` in conjunction with
  ``NAMELINK_COMPONENT``.

The ``install(TARGETS)`` command can also accept the following options at the
top level:

``EXPORT``
  This option associates the installed target files with an export called
  ``<export-name>``.  It must appear before any target options.  To actually
  install the export file itself, call ``install(EXPORT)``, documented below.

``INCLUDES DESTINATION``
  This option specifies a list of directories which will be added to the
  :prop_tgt:`INTERFACE_INCLUDE_DIRECTORIES` target property of the
  ``<targets>`` when exported by the :command:`install(EXPORT)` command. If a
  relative path is specified, it is treated as relative to the
  ``$<INSTALL_PREFIX>``.

One or more groups of properties may be specified in a single call to
the ``TARGETS`` form of this command.  A target may be installed more than
once to different locations.  Consider hypothetical targets ``myExe``,
``mySharedLib``, and ``myStaticLib``.  The code:

.. code-block:: cmake

  install(TARGETS myExe mySharedLib myStaticLib
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION lib
          ARCHIVE DESTINATION lib/static)
  install(TARGETS mySharedLib DESTINATION /some/full/path)

will install ``myExe`` to ``<prefix>/bin`` and ``myStaticLib`` to
``<prefix>/lib/static``.  On non-DLL platforms ``mySharedLib`` will be
installed to ``<prefix>/lib`` and ``/some/full/path``.  On DLL platforms
the ``mySharedLib`` DLL will be installed to ``<prefix>/bin`` and
``/some/full/path`` and its import library will be installed to
``<prefix>/lib/static`` and ``/some/full/path``.

:ref:`Interface Libraries` may be listed among the targets to install.
They install no artifacts but will be included in an associated ``EXPORT``.
If :ref:`Object Libraries` are listed but given no destination for their
object files, they will be exported as :ref:`Interface Libraries`.
This is sufficient to satisfy transitive usage requirements of other
targets that link to the object libraries in their implementation.

Installing a target with the :prop_tgt:`EXCLUDE_FROM_ALL` target property
set to ``TRUE`` has undefined behavior.

The install destination given to the target install ``DESTINATION`` may
use "generator expressions" with the syntax ``$<...>``.  See the
:manual:`cmake-generator-expressions(7)` manual for available expressions.

Installing Files
^^^^^^^^^^^^^^^^

.. _FILES:
.. _PROGRAMS:

::

  install(<FILES|PROGRAMS> files... DESTINATION <dir>
          [PERMISSIONS permissions...]
          [CONFIGURATIONS [Debug|Release|...]]
          [COMPONENT <component>]
          [RENAME <name>] [OPTIONAL] [EXCLUDE_FROM_ALL])

The ``FILES`` form specifies rules for installing files for a project.
File names given as relative paths are interpreted with respect to the
current source directory.  Files installed by this form are by default
given permissions ``OWNER_WRITE``, ``OWNER_READ``, ``GROUP_READ``, and
``WORLD_READ`` if no ``PERMISSIONS`` argument is given.

The ``PROGRAMS`` form is identical to the ``FILES`` form except that the
default permissions for the installed file also include ``OWNER_EXECUTE``,
``GROUP_EXECUTE``, and ``WORLD_EXECUTE``.  This form is intended to install
programs that are not targets, such as shell scripts.  Use the ``TARGETS``
form to install targets built within the project.

The list of ``files...`` given to ``FILES`` or ``PROGRAMS`` may use
"generator expressions" with the syntax ``$<...>``.  See the
:manual:`cmake-generator-expressions(7)` manual for available expressions.
However, if any item begins in a generator expression it must evaluate
to a full path.

The install destination given to the files install ``DESTINATION`` may
use "generator expressions" with the syntax ``$<...>``.  See the
:manual:`cmake-generator-expressions(7)` manual for available expressions.

Installing Directories
^^^^^^^^^^^^^^^^^^^^^^

.. _DIRECTORY:

::

  install(DIRECTORY dirs... DESTINATION <dir>
          [FILE_PERMISSIONS permissions...]
          [DIRECTORY_PERMISSIONS permissions...]
          [USE_SOURCE_PERMISSIONS] [OPTIONAL] [MESSAGE_NEVER]
          [CONFIGURATIONS [Debug|Release|...]]
          [COMPONENT <component>] [EXCLUDE_FROM_ALL]
          [FILES_MATCHING]
          [[PATTERN <pattern> | REGEX <regex>]
           [EXCLUDE] [PERMISSIONS permissions...]] [...])

The ``DIRECTORY`` form installs contents of one or more directories to a
given destination.  The directory structure is copied verbatim to the
destination.  The last component of each directory name is appended to
the destination directory but a trailing slash may be used to avoid
this because it leaves the last component empty.  Directory names
given as relative paths are interpreted with respect to the current
source directory.  If no input directory names are given the
destination directory will be created but nothing will be installed
into it.  The ``FILE_PERMISSIONS`` and ``DIRECTORY_PERMISSIONS`` options
specify permissions given to files and directories in the destination.
If ``USE_SOURCE_PERMISSIONS`` is specified and ``FILE_PERMISSIONS`` is not,
file permissions will be copied from the source directory structure.
If no permissions are specified files will be given the default
permissions specified in the ``FILES`` form of the command, and the
directories will be given the default permissions specified in the
``PROGRAMS`` form of the command.

The ``MESSAGE_NEVER`` option disables file installation status output.

Installation of directories may be controlled with fine granularity
using the ``PATTERN`` or ``REGEX`` options.  These "match" options specify a
globbing pattern or regular expression to match directories or files
encountered within input directories.  They may be used to apply
certain options (see below) to a subset of the files and directories
encountered.  The full path to each input file or directory (with
forward slashes) is matched against the expression.  A ``PATTERN`` will
match only complete file names: the portion of the full path matching
the pattern must occur at the end of the file name and be preceded by
a slash.  A ``REGEX`` will match any portion of the full path but it may
use ``/`` and ``$`` to simulate the ``PATTERN`` behavior.  By default all
files and directories are installed whether or not they are matched.
The ``FILES_MATCHING`` option may be given before the first match option
to disable installation of files (but not directories) not matched by
any expression.  For example, the code

.. code-block:: cmake

  install(DIRECTORY src/ DESTINATION include/myproj
          FILES_MATCHING PATTERN "*.h")

will extract and install header files from a source tree.

Some options may follow a ``PATTERN`` or ``REGEX`` expression and are applied
only to files or directories matching them.  The ``EXCLUDE`` option will
skip the matched file or directory.  The ``PERMISSIONS`` option overrides
the permissions setting for the matched file or directory.  For
example the code

.. code-block:: cmake

  install(DIRECTORY icons scripts/ DESTINATION share/myproj
          PATTERN "CVS" EXCLUDE
          PATTERN "scripts/*"
          PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
                      GROUP_EXECUTE GROUP_READ)

will install the ``icons`` directory to ``share/myproj/icons`` and the
``scripts`` directory to ``share/myproj``.  The icons will get default
file permissions, the scripts will be given specific permissions, and any
``CVS`` directories will be excluded.

The list of ``dirs...`` given to ``DIRECTORY`` and the install destination
given to the directory install ``DESTINATION`` may use "generator expressions"
with the syntax ``$<...>``.  See the :manual:`cmake-generator-expressions(7)`
manual for available expressions.

Custom Installation Logic
^^^^^^^^^^^^^^^^^^^^^^^^^

.. _CODE:
.. _SCRIPT:

::

  install([[SCRIPT <file>] [CODE <code>]]
          [COMPONENT <component>] [EXCLUDE_FROM_ALL] [...])

The ``SCRIPT`` form will invoke the given CMake script files during
installation.  If the script file name is a relative path it will be
interpreted with respect to the current source directory.  The ``CODE``
form will invoke the given CMake code during installation.  Code is
specified as a single argument inside a double-quoted string.  For
example, the code

.. code-block:: cmake

  install(CODE "MESSAGE(\"Sample install message.\")")

will print a message during installation.

Installing Exports
^^^^^^^^^^^^^^^^^^

.. _EXPORT:

::

  install(EXPORT <export-name> DESTINATION <dir>
          [NAMESPACE <namespace>] [[FILE <name>.cmake]|
          [PERMISSIONS permissions...]
          [CONFIGURATIONS [Debug|Release|...]]
          [EXPORT_LINK_INTERFACE_LIBRARIES]
          [COMPONENT <component>]
          [EXCLUDE_FROM_ALL])
  install(EXPORT_ANDROID_MK <export-name> DESTINATION <dir> [...])

The ``EXPORT`` form generates and installs a CMake file containing code to
import targets from the installation tree into another project.
Target installations are associated with the export ``<export-name>``
using the ``EXPORT`` option of the ``install(TARGETS)`` signature
documented above.  The ``NAMESPACE`` option will prepend ``<namespace>`` to
the target names as they are written to the import file.  By default
the generated file will be called ``<export-name>.cmake`` but the ``FILE``
option may be used to specify a different name.  The value given to
the ``FILE`` option must be a file name with the ``.cmake`` extension.
If a ``CONFIGURATIONS`` option is given then the file will only be installed
when one of the named configurations is installed.  Additionally, the
generated import file will reference only the matching target
configurations.  The ``EXPORT_LINK_INTERFACE_LIBRARIES`` keyword, if
present, causes the contents of the properties matching
``(IMPORTED_)?LINK_INTERFACE_LIBRARIES(_<CONFIG>)?`` to be exported, when
policy :policy:`CMP0022` is ``NEW``.

When a ``COMPONENT`` option is given, the listed ``<component>`` implicitly
depends on all components mentioned in the export set. The exported
``<name>.cmake`` file will require each of the exported components to be
present in order for dependent projects to build properly. For example, a
project may define components ``Runtime`` and ``Development``, with shared
libraries going into the ``Runtime`` component and static libraries and
headers going into the ``Development`` component. The export set would also
typically be part of the ``Development`` component, but it would export
targets from both the ``Runtime`` and ``Development`` components. Therefore,
the ``Runtime`` component would need to be installed if the ``Development``
component was installed, but not vice versa. If the ``Development`` component
was installed without the ``Runtime`` component, dependent projects that try
to link against it would have build errors. Package managers, such as APT and
RPM, typically handle this by listing the ``Runtime`` component as a dependency
of the ``Development`` component in the package metadata, ensuring that the
library is always installed if the headers and CMake export file are present.

In addition to cmake language files, the ``EXPORT_ANDROID_MK`` mode maybe
used to specify an export to the android ndk build system.  This mode
accepts the same options as the normal export mode.  The Android
NDK supports the use of prebuilt libraries, both static and shared. This
allows cmake to build the libraries of a project and make them available
to an ndk build system complete with transitive dependencies, include flags
and defines required to use the libraries.

The ``EXPORT`` form is useful to help outside projects use targets built
and installed by the current project.  For example, the code

.. code-block:: cmake

  install(TARGETS myexe EXPORT myproj DESTINATION bin)
  install(EXPORT myproj NAMESPACE mp_ DESTINATION lib/myproj)
  install(EXPORT_ANDROID_MK myexp DESTINATION share/ndk-modules)

will install the executable myexe to ``<prefix>/bin`` and code to import
it in the file ``<prefix>/lib/myproj/myproj.cmake`` and
``<prefix>/share/ndk-modules/Android.mk``.  An outside project
may load this file with the include command and reference the ``myexe``
executable from the installation tree using the imported target name
``mp_myexe`` as if the target were built in its own tree.

.. note::
  This command supercedes the :command:`install_targets` command and
  the :prop_tgt:`PRE_INSTALL_SCRIPT` and :prop_tgt:`POST_INSTALL_SCRIPT`
  target properties.  It also replaces the ``FILES`` forms of the
  :command:`install_files` and :command:`install_programs` commands.
  The processing order of these install rules relative to
  those generated by :command:`install_targets`,
  :command:`install_files`, and :command:`install_programs` commands
  is not defined.

Generated Installation Script
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``install()`` command generates a file, ``cmake_install.cmake``, inside
the build directory, which is used internally by the generated install target
and by CPack. You can also invoke this script manually with ``cmake -P``. This
script accepts several variables:

``COMPONENT``
  Set this variable to install only a single CPack component as opposed to all
  of them. For example, if you only want to install the ``Development``
  component, run ``cmake -DCOMPONENT=Development -P cmake_install.cmake``.

``BUILD_TYPE``
  Set this variable to change the build type if you are using a multi-config
  generator. For example, to install with the ``Debug`` configuration, run
  ``cmake -DBUILD_TYPE=Debug -P cmake_install.cmake``.

``DESTDIR``
  This is an environment variable rather than a CMake variable. It allows you
  to change the installation prefix on UNIX systems. See :envvar:`DESTDIR` for
  details.
