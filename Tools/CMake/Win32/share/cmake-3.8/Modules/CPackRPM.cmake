# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# CPackRPM
# --------
#
# The built in (binary) CPack RPM generator (Unix only)
#
# Variables specific to CPack RPM generator
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# CPackRPM may be used to create RPM packages using :module:`CPack`.
# CPackRPM is a :module:`CPack` generator thus it uses the ``CPACK_XXX``
# variables used by :module:`CPack`.
#
# CPackRPM has specific features which are controlled by the specifics
# :code:`CPACK_RPM_XXX` variables.
#
# :code:`CPACK_RPM_<COMPONENT>_XXXX` variables may be used in order to have
# **component** specific values.  Note however that ``<COMPONENT>`` refers to the
# **grouping name** written in upper case. It may be either a component name or
# a component GROUP name. Usually those variables correspond to RPM spec file
# entities. One may find information about spec files here
# http://www.rpm.org/wiki/Docs
#
# .. note::
#
#  `<COMPONENT>` part of variables is preferred to be in upper case (for e.g. if
#  component is named `foo` then use `CPACK_RPM_FOO_XXXX` variable name format)
#  as is with other `CPACK_<COMPONENT>_XXXX` variables.
#  For the purposes of back compatibility (CMake/CPack version 3.5 and lower)
#  support for same cased component (e.g. `fOo` would be used as
#  `CPACK_RPM_fOo_XXXX`) is still supported for variables defined in older
#  versions of CMake/CPack but is not guaranteed for variables that
#  will be added in the future. For the sake of back compatibility same cased
#  component variables also override upper cased versions where both are
#  present.
#
# Here are some CPackRPM wiki resources that are here for historic reasons and
# are no longer maintained but may still prove useful:
#
#  - https://cmake.org/Wiki/CMake:CPackConfiguration
#  - https://cmake.org/Wiki/CMake:CPackPackageGenerators#RPM_.28Unix_Only.29
#
# List of CPackRPM specific variables:
#
# .. variable:: CPACK_RPM_COMPONENT_INSTALL
#
#  Enable component packaging for CPackRPM
#
#  * Mandatory : NO
#  * Default   : OFF
#
#  If enabled (ON) multiple packages are generated. By default a single package
#  containing files of all components is generated.
#
# .. variable:: CPACK_RPM_PACKAGE_SUMMARY
#               CPACK_RPM_<component>_PACKAGE_SUMMARY
#
#  The RPM package summary.
#
#  * Mandatory : YES
#  * Default   : :variable:`CPACK_PACKAGE_DESCRIPTION_SUMMARY`
#
# .. variable:: CPACK_RPM_PACKAGE_NAME
#               CPACK_RPM_<component>_PACKAGE_NAME
#
#  The RPM package name.
#
#  * Mandatory : YES
#  * Default   : :variable:`CPACK_PACKAGE_NAME`
#
# .. variable:: CPACK_RPM_FILE_NAME
#               CPACK_RPM_<component>_FILE_NAME
#
#  Package file name.
#
#  * Mandatory : YES
#  * Default   : ``<CPACK_PACKAGE_FILE_NAME>[-<component>].rpm`` with spaces
#                replaced by '-'
#
#  This may be set to ``RPM-DEFAULT`` to allow rpmbuild tool to generate package
#  file name by itself.
#  Alternatively provided package file name must end with ``.rpm`` suffix.
#
#  .. note::
#
#    By using user provided spec file, rpm macro extensions such as for
#    generating debuginfo packages or by simply using multiple components more
#    than one rpm file may be generated, either from a single spec file or from
#    multiple spec files (each component execution produces it's own spec file).
#    In such cases duplicate file names may occur as a result of this variable
#    setting or spec file content structure. Duplicate files get overwritten
#    and it is up to the packager to set the variables in a manner that will
#    prevent such errors.
#
# .. variable:: CPACK_RPM_MAIN_COMPONENT
#
#  Main component that is packaged without component suffix.
#
#  * Mandatory : NO
#  * Default   : -
#
#  This variable can be set to any component or group name so that component or
#  group rpm package is generated without component suffix in filename and
#  package name.
#
# .. variable:: CPACK_RPM_PACKAGE_VERSION
#
#  The RPM package version.
#
#  * Mandatory : YES
#  * Default   : :variable:`CPACK_PACKAGE_VERSION`
#
# .. variable:: CPACK_RPM_PACKAGE_ARCHITECTURE
#               CPACK_RPM_<component>_PACKAGE_ARCHITECTURE
#
#  The RPM package architecture.
#
#  * Mandatory : YES
#  * Default   : Native architecture output by ``uname -m``
#
#  This may be set to ``noarch`` if you know you are building a noarch package.
#
# .. variable:: CPACK_RPM_PACKAGE_RELEASE
#
#  The RPM package release.
#
#  * Mandatory : YES
#  * Default   : 1
#
#  This is the numbering of the RPM package itself, i.e. the version of the
#  packaging and not the version of the content (see
#  :variable:`CPACK_RPM_PACKAGE_VERSION`). One may change the default value if
#  the previous packaging was buggy and/or you want to put here a fancy Linux
#  distro specific numbering.
#
# .. note::
#
#  This is the string that goes into the RPM ``Release:`` field. Some distros
#  (e.g. Fedora, CentOS) require ``1%{?dist}`` format and not just a number.
#  ``%{?dist}`` part can be added by setting :variable:`CPACK_RPM_PACKAGE_RELEASE_DIST`.
#
# .. variable:: CPACK_RPM_PACKAGE_RELEASE_DIST
#
#  The dist tag that is added  RPM ``Release:`` field.
#
#  * Mandatory : NO
#  * Default   : OFF
#
#  This is the reported ``%{dist}`` tag from the current distribution or empty
#  ``%{dist}`` if RPM macro is not set. If this variable is set then RPM
#  ``Release:`` field value is set to ``${CPACK_RPM_PACKAGE_RELEASE}%{?dist}``.
#
# .. variable:: CPACK_RPM_PACKAGE_LICENSE
#
#  The RPM package license policy.
#
#  * Mandatory : YES
#  * Default   : "unknown"
#
# .. variable:: CPACK_RPM_PACKAGE_GROUP
#               CPACK_RPM_<component>_PACKAGE_GROUP
#
#  The RPM package group.
#
#  * Mandatory : YES
#  * Default   : "unknown"
#
# .. variable:: CPACK_RPM_PACKAGE_VENDOR
#
#  The RPM package vendor.
#
#  * Mandatory : YES
#  * Default   : CPACK_PACKAGE_VENDOR if set or "unknown"
#
# .. variable:: CPACK_RPM_PACKAGE_URL
#               CPACK_RPM_<component>_PACKAGE_URL
#
#  The projects URL.
#
#  * Mandatory : NO
#  * Default   : -
#
# .. variable:: CPACK_RPM_PACKAGE_DESCRIPTION
#               CPACK_RPM_<component>_PACKAGE_DESCRIPTION
#
#  RPM package description.
#
#  * Mandatory : YES
#  * Default : :variable:`CPACK_COMPONENT_<compName>_DESCRIPTION` (component
#    based installers only) if set, :variable:`CPACK_PACKAGE_DESCRIPTION_FILE`
#    if set or "no package description available"
#
# .. variable:: CPACK_RPM_COMPRESSION_TYPE
#
#  RPM compression type.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to override RPM compression type to be used to build the
#  RPM. For example some Linux distribution now default to lzma or xz
#  compression whereas older cannot use such RPM. Using this one can enforce
#  compression type to be used.
#
#  Possible values are:
#
#  - lzma
#  - xz
#  - bzip2
#  - gzip
#
# .. variable:: CPACK_RPM_PACKAGE_AUTOREQ
#               CPACK_RPM_<component>_PACKAGE_AUTOREQ
#
#  RPM spec autoreq field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to enable (1, yes) or disable (0, no) automatic shared libraries
#  dependency detection. Dependencies are added to requires list.
#
#  .. note::
#
#    By default automatic dependency detection is enabled by rpm generator.
#
# .. variable:: CPACK_RPM_PACKAGE_AUTOPROV
#               CPACK_RPM_<component>_PACKAGE_AUTOPROV
#
#  RPM spec autoprov field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to enable (1, yes) or disable (0, no) automatic listing of shared
#  libraries that are provided by the package. Shared libraries are added to
#  provides list.
#
#  .. note::
#
#    By default automatic provides detection is enabled by rpm generator.
#
# .. variable:: CPACK_RPM_PACKAGE_AUTOREQPROV
#               CPACK_RPM_<component>_PACKAGE_AUTOREQPROV
#
#  RPM spec autoreqprov field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  Variable enables/disables autoreq and autoprov at the same time.
#  See :variable:`CPACK_RPM_PACKAGE_AUTOREQ` and :variable:`CPACK_RPM_PACKAGE_AUTOPROV`
#  for more details.
#
#  .. note::
#
#    By default automatic detection feature is enabled by rpm.
#
# .. variable:: CPACK_RPM_PACKAGE_REQUIRES
#               CPACK_RPM_<component>_PACKAGE_REQUIRES
#
#  RPM spec requires field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to set RPM dependencies (requires). Note that you must enclose
#  the complete requires string between quotes, for example::
#
#   set(CPACK_RPM_PACKAGE_REQUIRES "python >= 2.5.0, cmake >= 2.8")
#
#  The required package list of an RPM file could be printed with::
#
#   rpm -qp --requires file.rpm
#
# .. variable:: CPACK_RPM_PACKAGE_CONFLICTS
#               CPACK_RPM_<component>_PACKAGE_CONFLICTS
#
#  RPM spec conflicts field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to set negative RPM dependencies (conflicts). Note that you must
#  enclose the complete requires string between quotes, for example::
#
#   set(CPACK_RPM_PACKAGE_CONFLICTS "libxml2")
#
#  The conflicting package list of an RPM file could be printed with::
#
#   rpm -qp --conflicts file.rpm
#
# .. variable:: CPACK_RPM_PACKAGE_REQUIRES_PRE
#               CPACK_RPM_<component>_PACKAGE_REQUIRES_PRE
#
#  RPM spec requires(pre) field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to set RPM preinstall dependencies (requires(pre)). Note that
#  you must enclose the complete requires string between quotes, for example::
#
#   set(CPACK_RPM_PACKAGE_REQUIRES_PRE "shadow-utils, initscripts")
#
# .. variable:: CPACK_RPM_PACKAGE_REQUIRES_POST
#               CPACK_RPM_<component>_PACKAGE_REQUIRES_POST
#
#  RPM spec requires(post) field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to set RPM postinstall dependencies (requires(post)). Note that
#  you must enclose the complete requires string between quotes, for example::
#
#   set(CPACK_RPM_PACKAGE_REQUIRES_POST "shadow-utils, initscripts")
#
# .. variable:: CPACK_RPM_PACKAGE_REQUIRES_POSTUN
#               CPACK_RPM_<component>_PACKAGE_REQUIRES_POSTUN
#
#  RPM spec requires(postun) field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to set RPM postuninstall dependencies (requires(postun)). Note
#  that you must enclose the complete requires string between quotes, for
#  example::
#
#   set(CPACK_RPM_PACKAGE_REQUIRES_POSTUN "shadow-utils, initscripts")
#
# .. variable:: CPACK_RPM_PACKAGE_REQUIRES_PREUN
#               CPACK_RPM_<component>_PACKAGE_REQUIRES_PREUN
#
#  RPM spec requires(preun) field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to set RPM preuninstall dependencies (requires(preun)). Note that
#  you must enclose the complete requires string between quotes, for example::
#
#   set(CPACK_RPM_PACKAGE_REQUIRES_PREUN "shadow-utils, initscripts")
#
# .. variable:: CPACK_RPM_PACKAGE_SUGGESTS
#               CPACK_RPM_<component>_PACKAGE_SUGGESTS
#
#  RPM spec suggest field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to set weak RPM dependencies (suggests). Note that you must
#  enclose the complete requires string between quotes.
#
# .. variable:: CPACK_RPM_PACKAGE_PROVIDES
#               CPACK_RPM_<component>_PACKAGE_PROVIDES
#
#  RPM spec provides field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to set RPM dependencies (provides). The provided package list
#  of an RPM file could be printed with::
#
#   rpm -qp --provides file.rpm
#
# .. variable:: CPACK_RPM_PACKAGE_OBSOLETES
#               CPACK_RPM_<component>_PACKAGE_OBSOLETES
#
#  RPM spec obsoletes field.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to set RPM packages that are obsoleted by this one.
#
# .. variable:: CPACK_RPM_PACKAGE_RELOCATABLE
#
#  build a relocatable RPM.
#
#  * Mandatory : NO
#  * Default   : CPACK_PACKAGE_RELOCATABLE
#
#  If this variable is set to TRUE or ON CPackRPM will try
#  to build a relocatable RPM package. A relocatable RPM may
#  be installed using::
#
#   rpm --prefix or --relocate
#
#  in order to install it at an alternate place see rpm(8). Note that
#  currently this may fail if :variable:`CPACK_SET_DESTDIR` is set to ``ON``. If
#  :variable:`CPACK_SET_DESTDIR` is set then you will get a warning message but
#  if there is file installed with absolute path you'll get unexpected behavior.
#
# .. variable:: CPACK_RPM_SPEC_INSTALL_POST
#
#  Deprecated - use :variable:`CPACK_RPM_POST_INSTALL_SCRIPT_FILE` instead.
#
#  * Mandatory : NO
#  * Default   : -
#  * Deprecated: YES
#
#  This way of specifying post-install script is deprecated, use
#  :variable:`CPACK_RPM_POST_INSTALL_SCRIPT_FILE`.
#  May be used to set an RPM post-install command inside the spec file.
#  For example setting it to ``/bin/true`` may be used to prevent
#  rpmbuild to strip binaries.
#
# .. variable:: CPACK_RPM_SPEC_MORE_DEFINE
#
#  RPM extended spec definitions lines.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to add any ``%define`` lines to the generated spec file.
#
# .. variable:: CPACK_RPM_PACKAGE_DEBUG
#
#  Toggle CPackRPM debug output.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be set when invoking cpack in order to trace debug information
#  during CPack RPM run. For example you may launch CPack like this::
#
#   cpack -D CPACK_RPM_PACKAGE_DEBUG=1 -G RPM
#
# .. variable:: CPACK_RPM_USER_BINARY_SPECFILE
#               CPACK_RPM_<componentName>_USER_BINARY_SPECFILE
#
#  A user provided spec file.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be set by the user in order to specify a USER binary spec file
#  to be used by CPackRPM instead of generating the file.
#  The specified file will be processed by configure_file( @ONLY).
#
# .. variable:: CPACK_RPM_GENERATE_USER_BINARY_SPECFILE_TEMPLATE
#
#  Spec file template.
#
#  * Mandatory : NO
#  * Default   : -
#
#  If set CPack will generate a template for USER specified binary
#  spec file and stop with an error. For example launch CPack like this::
#
#   cpack -D CPACK_RPM_GENERATE_USER_BINARY_SPECFILE_TEMPLATE=1 -G RPM
#
#  The user may then use this file in order to hand-craft is own
#  binary spec file which may be used with
#  :variable:`CPACK_RPM_USER_BINARY_SPECFILE`.
#
# .. variable:: CPACK_RPM_PRE_INSTALL_SCRIPT_FILE
#               CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE
#
#  Path to file containing pre (un)install script.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to embed a pre (un)installation script in the spec file.
#  The referred script file (or both) will be read and directly
#  put after the ``%pre`` or ``%preun`` section
#  If :variable:`CPACK_RPM_COMPONENT_INSTALL` is set to ON the (un)install
#  script for each component can be overridden with
#  ``CPACK_RPM_<COMPONENT>_PRE_INSTALL_SCRIPT_FILE`` and
#  ``CPACK_RPM_<COMPONENT>_PRE_UNINSTALL_SCRIPT_FILE``.
#  One may verify which scriptlet has been included with::
#
#   rpm -qp --scripts  package.rpm
#
# .. variable:: CPACK_RPM_POST_INSTALL_SCRIPT_FILE
#               CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE
#
#  Path to file containing post (un)install script.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to embed a post (un)installation script in the spec file.
#  The referred script file (or both) will be read and directly
#  put after the ``%post`` or ``%postun`` section.
#  If :variable:`CPACK_RPM_COMPONENT_INSTALL` is set to ON the (un)install
#  script for each component can be overridden with
#  ``CPACK_RPM_<COMPONENT>_POST_INSTALL_SCRIPT_FILE`` and
#  ``CPACK_RPM_<COMPONENT>_POST_UNINSTALL_SCRIPT_FILE``.
#  One may verify which scriptlet has been included with::
#
#   rpm -qp --scripts  package.rpm
#
# .. variable:: CPACK_RPM_USER_FILELIST
#               CPACK_RPM_<COMPONENT>_USER_FILELIST
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to explicitly specify ``%(<directive>)`` file line
#  in the spec file. Like ``%config(noreplace)`` or any other directive
#  that be found in the ``%files`` section. You can have multiple directives
#  per line, as in ``%attr(600,root,root) %config(noreplace)``. Since
#  CPackRPM is generating the list of files (and directories) the user
#  specified files of the ``CPACK_RPM_<COMPONENT>_USER_FILELIST`` list will
#  be removed from the generated list. If referring to directories do
#  not add a trailing slash.
#
# .. variable:: CPACK_RPM_CHANGELOG_FILE
#
#  RPM changelog file.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to embed a changelog in the spec file.
#  The referred file will be read and directly put after the ``%changelog``
#  section.
#
# .. variable:: CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST
#
#  list of path to be excluded.
#
#  * Mandatory : NO
#  * Default   : /etc /etc/init.d /usr /usr/share /usr/share/doc /usr/bin /usr/lib /usr/lib64 /usr/include
#
#  May be used to exclude path (directories or files) from the auto-generated
#  list of paths discovered by CPack RPM. The defaut value contains a
#  reasonable set of values if the variable is not defined by the user. If the
#  variable is defined by the user then CPackRPM will NOT any of the default
#  path. If you want to add some path to the default list then you can use
#  :variable:`CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION` variable.
#
# .. variable:: CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
#
#  additional list of path to be excluded.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to add more exclude path (directories or files) from the initial
#  default list of excluded paths. See
#  :variable:`CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST`.
#
# .. variable:: CPACK_RPM_RELOCATION_PATHS
#
#  Packages relocation paths list.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to specify more than one relocation path per relocatable RPM.
#  Variable contains a list of relocation paths that if relative are prefixed
#  by the value of :variable:`CPACK_RPM_<COMPONENT>_PACKAGE_PREFIX` or by the
#  value of :variable:`CPACK_PACKAGING_INSTALL_PREFIX` if the component version
#  is not provided.
#  Variable is not component based as its content can be used to set a different
#  path prefix for e.g. binary dir and documentation dir at the same time.
#  Only prefixes that are required by a certain component are added to that
#  component - component must contain at least one file/directory/symbolic link
#  with :variable:`CPACK_RPM_RELOCATION_PATHS` prefix for a certain relocation
#  path to be added. Package will not contain any relocation paths if there are
#  no files/directories/symbolic links on any of the provided prefix locations.
#  Packages that either do not contain any relocation paths or contain
#  files/directories/symbolic links that are outside relocation paths print
#  out an ``AUTHOR_WARNING`` that RPM will be partially relocatable.
#
# .. variable:: CPACK_RPM_<COMPONENT>_PACKAGE_PREFIX
#
#  Per component relocation path install prefix.
#
#  * Mandatory : NO
#  * Default   : CPACK_PACKAGING_INSTALL_PREFIX
#
#  May be used to set per component :variable:`CPACK_PACKAGING_INSTALL_PREFIX`
#  for relocatable RPM packages.
#
# .. variable:: CPACK_RPM_NO_INSTALL_PREFIX_RELOCATION
#               CPACK_RPM_NO_<COMPONENT>_INSTALL_PREFIX_RELOCATION
#
#  Removal of default install prefix from relocation paths list.
#
#  * Mandatory : NO
#  * Default   : CPACK_PACKAGING_INSTALL_PREFIX or CPACK_RPM_<COMPONENT>_PACKAGE_PREFIX
#                are treated as one of relocation paths
#
#  May be used to remove CPACK_PACKAGING_INSTALL_PREFIX and CPACK_RPM_<COMPONENT>_PACKAGE_PREFIX
#  from relocatable RPM prefix paths.
#
# .. variable:: CPACK_RPM_ADDITIONAL_MAN_DIRS
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to set additional man dirs that could potentially be compressed
#  by brp-compress RPM macro. Variable content must be a list of regular
#  expressions that point to directories containing man files or to man files
#  directly. Note that in order to compress man pages a path must also be
#  present in brp-compress RPM script and that brp-compress script must be
#  added to RPM configuration by the operating system.
#
#  Regular expressions that are added by default were taken from brp-compress
#  RPM macro:
#
#  - /usr/man/man.*
#  - /usr/man/.*/man.*
#  - /usr/info.*
#  - /usr/share/man/man.*
#  - /usr/share/man/.*/man.*
#  - /usr/share/info.*
#  - /usr/kerberos/man.*
#  - /usr/X11R6/man/man.*
#  - /usr/lib/perl5/man/man.*
#  - /usr/share/doc/.*/man/man.*
#  - /usr/lib/.*/man/man.*
#
# .. variable:: CPACK_RPM_DEFAULT_USER
#               CPACK_RPM_<compName>_DEFAULT_USER
#
#  default user ownership of RPM content
#
#  * Mandatory : NO
#  * Default   : root
#
#  Value should be user name and not UID.
#  Note that <compName> must be in upper-case.
#
# .. variable:: CPACK_RPM_DEFAULT_GROUP
#               CPACK_RPM_<compName>_DEFAULT_GROUP
#
#  default group ownership of RPM content
#
#  * Mandatory : NO
#  * Default   : root
#
#  Value should be group name and not GID.
#  Note that <compName> must be in upper-case.
#
# .. variable:: CPACK_RPM_DEFAULT_FILE_PERMISSIONS
#               CPACK_RPM_<compName>_DEFAULT_FILE_PERMISSIONS
#
#  default permissions used for packaged files
#
#  * Mandatory : NO
#  * Default   : - (system default)
#
#  Accepted values are lists with ``PERMISSIONS``. Valid permissions
#  are:
#
#  - OWNER_READ
#  - OWNER_WRITE
#  - OWNER_EXECUTE
#  - GROUP_READ
#  - GROUP_WRITE
#  - GROUP_EXECUTE
#  - WORLD_READ
#  - WORLD_WRITE
#  - WORLD_EXECUTE
#
#  Note that <compName> must be in upper-case.
#
# .. variable:: CPACK_RPM_DEFAULT_DIR_PERMISSIONS
#               CPACK_RPM_<compName>_DEFAULT_DIR_PERMISSIONS
#
#  default permissions used for packaged directories
#
#  * Mandatory : NO
#  * Default   : - (system default)
#
#  Accepted values are lists with PERMISSIONS. Valid permissions
#  are the same as for :variable:`CPACK_RPM_DEFAULT_FILE_PERMISSIONS`.
#  Note that <compName> must be in upper-case.
#
# Packaging of Symbolic Links
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# CPackRPM supports packaging of symbolic links::
#
#   execute_process(COMMAND ${CMAKE_COMMAND}
#     -E create_symlink <relative_path_location> <symlink_name>)
#   install(FILES ${CMAKE_CURRENT_BINARY_DIR}/<symlink_name>
#     DESTINATION <symlink_location> COMPONENT libraries)
#
# Symbolic links will be optimized (paths will be shortened if possible)
# before being added to the package or if multiple relocation paths are
# detected, a post install symlink relocation script will be generated.
#
# Symbolic links may point to locations that are not packaged by the same
# package (either a different component or even not packaged at all) but
# those locations will be treated as if they were a part of the package
# while determining if symlink should be either created or present in a
# post install script - depending on relocation paths.
#
# Symbolic links that point to locations outside packaging path produce a
# warning and are treated as non relocatable permanent symbolic links.
#
# Currently there are a few limitations though:
#
# * For component based packaging component interdependency is not checked
#   when processing symbolic links. Symbolic links pointing to content of
#   a different component are treated the same way as if pointing to location
#   that will not be packaged.
#
# * Symbolic links pointing to a location through one or more intermediate
#   symbolic links will not be handled differently - if the intermediate
#   symbolic link(s) is also on a relocatable path, relocating it during
#   package installation may cause initial symbolic link to point to an
#   invalid location.
#
# Packaging of debug information
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# Debuginfo packages contain debug symbols and sources for debugging packaged
# binaries.
#
# Debuginfo RPM packaging has it's own set of variables:
#
# .. variable:: CPACK_RPM_DEBUGINFO_PACKAGE
#               CPACK_RPM_<component>_DEBUGINFO_PACKAGE
#
#  Enable generation of debuginfo RPM package(s).
#
#  * Mandatory : NO
#  * Default   : OFF
#
# .. note::
#
#  Binaries must contain debug symbols before packaging so use either ``Debug``
#  or ``RelWithDebInfo`` for :variable:`CMAKE_BUILD_TYPE` variable value.
#
# .. note::
#
#  Packages generated from packages without binary files, with binary files but
#  without execute permissions or without debug symbols will be empty.
#
# .. variable:: CPACK_BUILD_SOURCE_DIRS
#
#  Provides locations of root directories of source files from which binaries
#  were built.
#
#  * Mandatory : YES if :variable:`CPACK_RPM_DEBUGINFO_PACKAGE` is set
#  * Default   : -
#
# .. note::
#
#  For CMake project :variable:`CPACK_BUILD_SOURCE_DIRS` is set by default to
#  point to :variable:`CMAKE_SOURCE_DIR` and :variable:`CMAKE_BINARY_DIR` paths.
#
# .. note::
#
#  Sources with path prefixes that do not fall under any location provided with
#  :variable:`CPACK_BUILD_SOURCE_DIRS` will not be present in debuginfo package.
#
# .. variable:: CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX
#               CPACK_RPM_<component>_BUILD_SOURCE_DIRS_PREFIX
#
#  Prefix of location where sources will be placed during package installation.
#
#  * Mandatory : YES if :variable:`CPACK_RPM_DEBUGINFO_PACKAGE` is set
#  * Default   : "/usr/src/debug/<CPACK_PACKAGE_FILE_NAME>" and
#                for component packaging "/usr/src/debug/<CPACK_PACKAGE_FILE_NAME>-<component>"
#
# .. note::
#
#  Each source path prefix is additionaly suffixed by ``src_<index>`` where
#  index is index of the path used from :variable:`CPACK_BUILD_SOURCE_DIRS`
#  variable. This produces ``<CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX>/src_<index>``
#  replacement path.
#  Limitation is that replaced path part must be shorter or of equal
#  length than the length of its replacement. If that is not the case either
#  :variable:`CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX` variable has to be set to
#  a shorter path or source directories must be placed on a longer path.
#
# .. variable:: CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS
#
#  Directories containing sources that should be excluded from debuginfo packages.
#
#  * Mandatory : NO
#  * Default   : "/usr /usr/src /usr/src/debug"
#
#  Listed paths are owned by other RPM packages and should therefore not be
#  deleted on debuginfo package uninstallation.
#
# .. variable:: CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS_ADDITION
#
#  Paths that should be appended to :variable:`CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS`
#  for exclusion.
#
#  * Mandatory : NO
#  * Default   : -
#
# .. variable:: CPACK_RPM_DEBUGINFO_SINGLE_PACKAGE
#
#  Create a single debuginfo package even if components packaging is set.
#
#  * Mandatory : NO
#  * Default   : OFF
#
#  When this variable is enabled it produces a single debuginfo package even if
#  component packaging is enabled.
#
#  When using this feature in combination with components packaging and there is
#  more than one component this variable requires :variable:`CPACK_RPM_MAIN_COMPONENT`
#  to be set.
#
# .. note::
#
#  If none of the :variable:`CPACK_RPM_<component>_DEBUGINFO_PACKAGE` variables
#  is set then :variable:`CPACK_RPM_DEBUGINFO_PACKAGE` is automatically set to
#  ``ON`` when :variable:`CPACK_RPM_DEBUGINFO_SINGLE_PACKAGE` is set.
#
# Packaging of sources (SRPM)
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# SRPM packaging is enabled by setting :variable:`CPACK_RPM_PACKAGE_SOURCES`
# variable while usually using :variable:`CPACK_INSTALLED_DIRECTORIES` variable
# to provide directory containing CMakeLists.txt and source files.
#
# For CMake projects SRPM package would be product by executing:
#
# ``cpack -G RPM --config ./CPackSourceConfig.cmake``
#
# .. note::
#
#  Produced SRPM package is expected to be built with :manual:`cmake(1)` executable
#  and packaged with :manual:`cpack(1)` executable so CMakeLists.txt has to be
#  located in root source directory and must be able to generate binary rpm
#  packages by executing ``cpack -G`` command. The two executables as well as
#  rpmbuild must also be present when generating binary rpm packages from the
#  produced SRPM package.
#
# Once the SRPM package is generated it can be used to generate binary packages
# by creating a directory structure for rpm generation and executing rpmbuild
# tool:
#
# ``mkdir -p build_dir/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}``
# ``rpmbuild --define "_topdir <path_to_build_dir>" --rebuild <SRPM_file_name>``
#
# Generated packages will be located in build_dir/RPMS directory or its sub
# directories.
#
# .. note::
#
#  SRPM package internally uses CPack/RPM generator to generate binary packages
#  so CMakeScripts.txt can decide during the SRPM to binary rpm generation step
#  what content the package(s) should have as well as how they should be packaged
#  (monolithic or components). CMake can decide this for e.g. by reading environment
#  variables set by the package manager before starting the process of generating
#  binary rpm packages. This way a single SRPM package can be used to produce
#  different binary rpm packages on different platforms depending on the platform's
#  packaging rules.
#
# Source RPM packaging has it's own set of variables:
#
# .. variable:: CPACK_RPM_PACKAGE_SOURCES
#
#  Should the content be packaged as a source rpm (default is binary rpm).
#
#  * Mandatory : NO
#  * Default   : OFF
#
# .. note::
#
#  For cmake projects :variable:`CPACK_RPM_PACKAGE_SOURCES` variable is set
#  to ``OFF`` in CPackConfig.cmake and ``ON`` in CPackSourceConfig.cmake
#  generated files.
#
# .. variable:: CPACK_RPM_SOURCE_PKG_BUILD_PARAMS
#
#  Additional command-line parameters provided to :manual:`cmake(1)` executable.
#
#  * Mandatory : NO
#  * Default   : -
#
# .. variable:: CPACK_RPM_SOURCE_PKG_PACKAGING_INSTALL_PREFIX
#
#  Packaging install prefix that would be provided in :variable:`CPACK_PACKAGING_INSTALL_PREFIX`
#  variable for producing binary RPM packages.
#
#  * Mandatory : YES
#  * Default   : "/"
#
# .. VARIABLE:: CPACK_RPM_BUILDREQUIRES
#
#  List of source rpm build dependencies.
#
#  * Mandatory : NO
#  * Default   : -
#
#  May be used to set source RPM build dependencies (BuildRequires). Note that
#  you must enclose the complete build requirements string between quotes, for
#  example::
#
#   set(CPACK_RPM_BUILDREQUIRES "python >= 2.5.0, cmake >= 2.8")

# Author: Eric Noulard with the help of Alexander Neundorf.

function(get_unix_permissions_octal_notation PERMISSIONS_VAR RETURN_VAR)
  set(PERMISSIONS ${${PERMISSIONS_VAR}})
  list(LENGTH PERMISSIONS PERM_LEN_PRE)
  list(REMOVE_DUPLICATES PERMISSIONS)
  list(LENGTH PERMISSIONS PERM_LEN_POST)

  if(NOT ${PERM_LEN_PRE} EQUAL ${PERM_LEN_POST})
    message(FATAL_ERROR "${PERMISSIONS_VAR} contains duplicate values.")
  endif()

  foreach(PERMISSION_TYPE "OWNER" "GROUP" "WORLD")
    set(${PERMISSION_TYPE}_PERMISSIONS 0)

    foreach(PERMISSION ${PERMISSIONS})
      if("${PERMISSION}" STREQUAL "${PERMISSION_TYPE}_READ")
        math(EXPR ${PERMISSION_TYPE}_PERMISSIONS "${${PERMISSION_TYPE}_PERMISSIONS} + 4")
      elseif("${PERMISSION}" STREQUAL "${PERMISSION_TYPE}_WRITE")
        math(EXPR ${PERMISSION_TYPE}_PERMISSIONS "${${PERMISSION_TYPE}_PERMISSIONS} + 2")
      elseif("${PERMISSION}" STREQUAL "${PERMISSION_TYPE}_EXECUTE")
        math(EXPR ${PERMISSION_TYPE}_PERMISSIONS "${${PERMISSION_TYPE}_PERMISSIONS} + 1")
      elseif(PERMISSION MATCHES "${PERMISSION_TYPE}.*")
        message(FATAL_ERROR "${PERMISSIONS_VAR} contains invalid values.")
      endif()
    endforeach()
  endforeach()

  set(${RETURN_VAR} "${OWNER_PERMISSIONS}${GROUP_PERMISSIONS}${WORLD_PERMISSIONS}" PARENT_SCOPE)
endfunction()

function(cpack_rpm_prepare_relocation_paths)
  # set appropriate prefix, remove possible trailing slash and convert backslashes to slashes
  if(CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_PACKAGE_PREFIX)
    file(TO_CMAKE_PATH "${CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_PACKAGE_PREFIX}" PATH_PREFIX)
  elseif(CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_PACKAGE_PREFIX)
    file(TO_CMAKE_PATH "${CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_PACKAGE_PREFIX}" PATH_PREFIX)
  else()
    file(TO_CMAKE_PATH "${CPACK_PACKAGING_INSTALL_PREFIX}" PATH_PREFIX)
  endif()

  set(RPM_RELOCATION_PATHS "${CPACK_RPM_RELOCATION_PATHS}")
  list(REMOVE_DUPLICATES RPM_RELOCATION_PATHS)

  # set base path prefix
  if(EXISTS "${WDIR}/${PATH_PREFIX}")
    if(NOT CPACK_RPM_NO_INSTALL_PREFIX_RELOCATION AND
       NOT CPACK_RPM_NO_${CPACK_RPM_PACKAGE_COMPONENT}_INSTALL_PREFIX_RELOCATION AND
       NOT CPACK_RPM_NO_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_INSTALL_PREFIX_RELOCATION)
      string(APPEND TMP_RPM_PREFIXES "Prefix: ${PATH_PREFIX}\n")
      list(APPEND RPM_USED_PACKAGE_PREFIXES "${PATH_PREFIX}")

      if(CPACK_RPM_PACKAGE_DEBUG)
        message("CPackRPM:Debug: removing '${PATH_PREFIX}' from relocation paths")
      endif()
    endif()
  endif()

  # set other path prefixes
  foreach(RELOCATION_PATH ${RPM_RELOCATION_PATHS})
    if(IS_ABSOLUTE "${RELOCATION_PATH}")
      set(PREPARED_RELOCATION_PATH "${RELOCATION_PATH}")
    else()
      set(PREPARED_RELOCATION_PATH "${PATH_PREFIX}/${RELOCATION_PATH}")
    endif()

    if(EXISTS "${WDIR}/${PREPARED_RELOCATION_PATH}")
      string(APPEND TMP_RPM_PREFIXES "Prefix: ${PREPARED_RELOCATION_PATH}\n")
      list(APPEND RPM_USED_PACKAGE_PREFIXES "${PREPARED_RELOCATION_PATH}")
    endif()
  endforeach()

  # warn about all the paths that are not relocatable
  cmake_policy(PUSH)
    # Tell file(GLOB_RECURSE) not to follow directory symlinks
    # even if the project does not set this policy to NEW.
    cmake_policy(SET CMP0009 NEW)
    file(GLOB_RECURSE FILE_PATHS_ "${WDIR}/*")
  cmake_policy(POP)
  foreach(TMP_PATH ${FILE_PATHS_})
    string(LENGTH "${WDIR}" WDIR_LEN)
    string(SUBSTRING "${TMP_PATH}" ${WDIR_LEN} -1 TMP_PATH)
    unset(TMP_PATH_FOUND_)

    foreach(RELOCATION_PATH ${RPM_USED_PACKAGE_PREFIXES})
      file(RELATIVE_PATH REL_PATH_ "${RELOCATION_PATH}" "${TMP_PATH}")
      string(SUBSTRING "${REL_PATH_}" 0 2 PREFIX_)

      if(NOT "${PREFIX_}" STREQUAL "..")
        set(TPM_PATH_FOUND_ TRUE)
        break()
      endif()
    endforeach()

    if(NOT TPM_PATH_FOUND_)
      message(AUTHOR_WARNING "CPackRPM:Warning: Path ${TMP_PATH} is not on one of the relocatable paths! Package will be partially relocatable.")
    endif()
  endforeach()

  set(RPM_USED_PACKAGE_PREFIXES "${RPM_USED_PACKAGE_PREFIXES}" PARENT_SCOPE)
  set(TMP_RPM_PREFIXES "${TMP_RPM_PREFIXES}" PARENT_SCOPE)
endfunction()

function(cpack_rpm_prepare_content_list)
  # get files list
  cmake_policy(PUSH)
    cmake_policy(SET CMP0009 NEW)
    file(GLOB_RECURSE CPACK_RPM_INSTALL_FILES LIST_DIRECTORIES true RELATIVE "${WDIR}" "${WDIR}/*")
  cmake_policy(POP)
  set(CPACK_RPM_INSTALL_FILES "/${CPACK_RPM_INSTALL_FILES}")
  string(REPLACE ";" ";/" CPACK_RPM_INSTALL_FILES "${CPACK_RPM_INSTALL_FILES}")

  # if we are creating a relocatable package, omit parent directories of
  # CPACK_RPM_PACKAGE_PREFIX. This is achieved by building a "filter list"
  # which is passed to the find command that generates the content-list
  if(CPACK_RPM_PACKAGE_RELOCATABLE)
    # get a list of the elements in CPACK_RPM_PACKAGE_PREFIXES that are
    # destinct parent paths of other relocation paths and remove the
    # final element (so the install-prefix dir itself is not omitted
    # from the RPM's content-list)
    list(SORT RPM_USED_PACKAGE_PREFIXES)
    set(_DISTINCT_PATH "NOT_SET")
    foreach(_RPM_RELOCATION_PREFIX ${RPM_USED_PACKAGE_PREFIXES})
      if(NOT "${_RPM_RELOCATION_PREFIX}" MATCHES "${_DISTINCT_PATH}/.*")
        set(_DISTINCT_PATH "${_RPM_RELOCATION_PREFIX}")

        string(REPLACE "/" ";" _CPACK_RPM_PACKAGE_PREFIX_ELEMS " ${_RPM_RELOCATION_PREFIX}")
        cmake_policy(PUSH)
          cmake_policy(SET CMP0007 NEW)
          list(REMOVE_AT _CPACK_RPM_PACKAGE_PREFIX_ELEMS -1)
        cmake_policy(POP)
        unset(_TMP_LIST)
        # Now generate all of the parent dirs of the relocation path
        foreach(_PREFIX_PATH_ELEM ${_CPACK_RPM_PACKAGE_PREFIX_ELEMS})
          list(APPEND _TMP_LIST "${_PREFIX_PATH_ELEM}")
          string(REPLACE ";" "/" _OMIT_DIR "${_TMP_LIST}")
          separate_arguments(_OMIT_DIR)
          list(APPEND _RPM_DIRS_TO_OMIT ${_OMIT_DIR})
        endforeach()
      endif()
    endforeach()
  endif()

  if(CPACK_RPM_PACKAGE_DEBUG)
    message("CPackRPM:Debug: Initial list of path to OMIT in RPM: ${_RPM_DIRS_TO_OMIT}")
  endif()

  if(NOT DEFINED CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST)
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST /etc /etc/init.d /usr /usr/share /usr/share/doc /usr/bin /usr/lib /usr/lib64 /usr/include)
    if(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION)
      if(CPACK_RPM_PACKAGE_DEBUG)
        message("CPackRPM:Debug: Adding ${CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION} to builtin omit list.")
      endif()
      list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST "${CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION}")
    endif()
  endif()

  if(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST)
    if(CPACK_RPM_PACKAGE_DEBUG)
      message("CPackRPM:Debug: CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST= ${CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST}")
    endif()
    list(APPEND _RPM_DIRS_TO_OMIT ${CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST})
  endif()

  if(CPACK_RPM_PACKAGE_DEBUG)
    message("CPackRPM:Debug: Final list of path to OMIT in RPM: ${_RPM_DIRS_TO_OMIT}")
  endif()

  list(REMOVE_ITEM CPACK_RPM_INSTALL_FILES ${_RPM_DIRS_TO_OMIT})

  # add man paths that will be compressed
  # (copied from /usr/lib/rpm/brp-compress - script that does the actual
  # compressing)
  list(APPEND MAN_LOCATIONS "/usr/man/man.*" "/usr/man/.*/man.*" "/usr/info.*"
    "/usr/share/man/man.*" "/usr/share/man/.*/man.*" "/usr/share/info.*"
    "/usr/kerberos/man.*" "/usr/X11R6/man/man.*" "/usr/lib/perl5/man/man.*"
    "/usr/share/doc/.*/man/man.*" "/usr/lib/.*/man/man.*")

  if(CPACK_RPM_ADDITIONAL_MAN_DIRS)
    if(CPACK_RPM_PACKAGE_DEBUG)
      message("CPackRPM:Debug: CPACK_RPM_ADDITIONAL_MAN_DIRS= ${CPACK_RPM_ADDITIONAL_MAN_DIRS}")
    endif()
    list(APPEND MAN_LOCATIONS ${CPACK_RPM_ADDITIONAL_MAN_DIRS})
  endif()

  foreach(PACK_LOCATION IN LISTS CPACK_RPM_INSTALL_FILES)
    foreach(MAN_LOCATION IN LISTS MAN_LOCATIONS)
      # man pages are files inside a certain location
      if(PACK_LOCATION MATCHES "${MAN_LOCATION}/"
        AND NOT IS_DIRECTORY "${WDIR}${PACK_LOCATION}"
        AND NOT IS_SYMLINK "${WDIR}${PACK_LOCATION}")
        list(FIND CPACK_RPM_INSTALL_FILES "${PACK_LOCATION}" INDEX)
        # insert file location that covers compressed man pages
        # even if using a wildcard causes duplicates as those are
        # handled by RPM and we still keep the same file list
        # in spec file - wildcard only represents file type (e.g. .gz)
        list(INSERT CPACK_RPM_INSTALL_FILES ${INDEX} "${PACK_LOCATION}*")
        # remove file location that doesn't cover compressed man pages
        math(EXPR INDEX ${INDEX}+1)
        list(REMOVE_AT CPACK_RPM_INSTALL_FILES ${INDEX})

        break()
      endif()
    endforeach()
  endforeach()

  set(CPACK_RPM_INSTALL_FILES "${CPACK_RPM_INSTALL_FILES}" PARENT_SCOPE)
endfunction()

function(cpack_rpm_symlink_get_relocation_prefixes LOCATION PACKAGE_PREFIXES RETURN_VARIABLE)
  foreach(PKG_PREFIX IN LISTS PACKAGE_PREFIXES)
    string(REGEX MATCH "^${PKG_PREFIX}/.*" FOUND_ "${LOCATION}")
    if(FOUND_)
      list(APPEND TMP_PREFIXES "${PKG_PREFIX}")
    endif()
  endforeach()

  set(${RETURN_VARIABLE} "${TMP_PREFIXES}" PARENT_SCOPE)
endfunction()

function(cpack_rpm_symlink_create_relocation_script PACKAGE_PREFIXES)
  list(LENGTH PACKAGE_PREFIXES LAST_INDEX)
  set(SORTED_PACKAGE_PREFIXES "${PACKAGE_PREFIXES}")
  list(SORT SORTED_PACKAGE_PREFIXES)
  list(REVERSE SORTED_PACKAGE_PREFIXES)
  math(EXPR LAST_INDEX ${LAST_INDEX}-1)

  foreach(SYMLINK_INDEX RANGE ${LAST_INDEX})
    list(GET SORTED_PACKAGE_PREFIXES ${SYMLINK_INDEX} SRC_PATH)
    list(FIND PACKAGE_PREFIXES "${SRC_PATH}" SYMLINK_INDEX) # reverse magic
    string(LENGTH "${SRC_PATH}" SRC_PATH_LEN)

    set(PARTS_CNT 0)
    set(SCRIPT_PART "if [ \"$RPM_INSTALL_PREFIX${SYMLINK_INDEX}\" != \"${SRC_PATH}\" ]; then\n")

    # both paths relocated
    foreach(POINT_INDEX RANGE ${LAST_INDEX})
      list(GET SORTED_PACKAGE_PREFIXES ${POINT_INDEX} POINT_PATH)
      list(FIND PACKAGE_PREFIXES "${POINT_PATH}" POINT_INDEX) # reverse magic
      string(LENGTH "${POINT_PATH}" POINT_PATH_LEN)

      if(_RPM_RELOCATION_SCRIPT_${SYMLINK_INDEX}_${POINT_INDEX})
        if("${SYMLINK_INDEX}" EQUAL "${POINT_INDEX}")
          set(INDENT "")
        else()
          string(APPEND SCRIPT_PART "  if [ \"$RPM_INSTALL_PREFIX${POINT_INDEX}\" != \"${POINT_PATH}\" ]; then\n")
          set(INDENT "  ")
        endif()

        foreach(RELOCATION_NO IN LISTS _RPM_RELOCATION_SCRIPT_${SYMLINK_INDEX}_${POINT_INDEX})
          math(EXPR PARTS_CNT ${PARTS_CNT}+1)

          math(EXPR RELOCATION_INDEX ${RELOCATION_NO}-1)
          list(GET _RPM_RELOCATION_SCRIPT_PAIRS ${RELOCATION_INDEX} RELOCATION_SCRIPT_PAIR)
          string(FIND "${RELOCATION_SCRIPT_PAIR}" ":" SPLIT_INDEX)

          math(EXPR SRC_PATH_END ${SPLIT_INDEX}-${SRC_PATH_LEN})
          string(SUBSTRING ${RELOCATION_SCRIPT_PAIR} ${SRC_PATH_LEN} ${SRC_PATH_END} SYMLINK_)

          math(EXPR POINT_PATH_START ${SPLIT_INDEX}+1+${POINT_PATH_LEN})
          string(SUBSTRING ${RELOCATION_SCRIPT_PAIR} ${POINT_PATH_START} -1 POINT_)

          string(APPEND SCRIPT_PART "  ${INDENT}if [ -z \"$CPACK_RPM_RELOCATED_SYMLINK_${RELOCATION_INDEX}\" ]; then\n")
          string(APPEND SCRIPT_PART "    ${INDENT}ln -s \"$RPM_INSTALL_PREFIX${POINT_INDEX}${POINT_}\" \"$RPM_INSTALL_PREFIX${SYMLINK_INDEX}${SYMLINK_}\"\n")
          string(APPEND SCRIPT_PART "    ${INDENT}CPACK_RPM_RELOCATED_SYMLINK_${RELOCATION_INDEX}=true\n")
          string(APPEND SCRIPT_PART "  ${INDENT}fi\n")
        endforeach()

        if(NOT "${SYMLINK_INDEX}" EQUAL "${POINT_INDEX}")
          string(APPEND SCRIPT_PART "  fi\n")
        endif()
      endif()
    endforeach()

    # source path relocated
    if(_RPM_RELOCATION_SCRIPT_${SYMLINK_INDEX}_X)
      foreach(RELOCATION_NO IN LISTS _RPM_RELOCATION_SCRIPT_${SYMLINK_INDEX}_X)
        math(EXPR PARTS_CNT ${PARTS_CNT}+1)

        math(EXPR RELOCATION_INDEX ${RELOCATION_NO}-1)
        list(GET _RPM_RELOCATION_SCRIPT_PAIRS ${RELOCATION_INDEX} RELOCATION_SCRIPT_PAIR)
        string(FIND "${RELOCATION_SCRIPT_PAIR}" ":" SPLIT_INDEX)

        math(EXPR SRC_PATH_END ${SPLIT_INDEX}-${SRC_PATH_LEN})
        string(SUBSTRING ${RELOCATION_SCRIPT_PAIR} ${SRC_PATH_LEN} ${SRC_PATH_END} SYMLINK_)

        math(EXPR POINT_PATH_START ${SPLIT_INDEX}+1)
        string(SUBSTRING ${RELOCATION_SCRIPT_PAIR} ${POINT_PATH_START} -1 POINT_)

        string(APPEND SCRIPT_PART "  if [ -z \"$CPACK_RPM_RELOCATED_SYMLINK_${RELOCATION_INDEX}\" ]; then\n")
        string(APPEND SCRIPT_PART "    ln -s \"${POINT_}\" \"$RPM_INSTALL_PREFIX${SYMLINK_INDEX}${SYMLINK_}\"\n")
        string(APPEND SCRIPT_PART "    CPACK_RPM_RELOCATED_SYMLINK_${RELOCATION_INDEX}=true\n")
        string(APPEND SCRIPT_PART "  fi\n")
      endforeach()
    endif()

    if(PARTS_CNT)
      set(SCRIPT "${SCRIPT_PART}")
      string(APPEND SCRIPT "fi\n")
    endif()
  endforeach()

  # point path relocated
  foreach(POINT_INDEX RANGE ${LAST_INDEX})
    list(GET SORTED_PACKAGE_PREFIXES ${POINT_INDEX} POINT_PATH)
    list(FIND PACKAGE_PREFIXES "${POINT_PATH}" POINT_INDEX) # reverse magic
    string(LENGTH "${POINT_PATH}" POINT_PATH_LEN)

    if(_RPM_RELOCATION_SCRIPT_X_${POINT_INDEX})
      string(APPEND SCRIPT "if [ \"$RPM_INSTALL_PREFIX${POINT_INDEX}\" != \"${POINT_PATH}\" ]; then\n")

      foreach(RELOCATION_NO IN LISTS _RPM_RELOCATION_SCRIPT_X_${POINT_INDEX})
        math(EXPR RELOCATION_INDEX ${RELOCATION_NO}-1)
        list(GET _RPM_RELOCATION_SCRIPT_PAIRS ${RELOCATION_INDEX} RELOCATION_SCRIPT_PAIR)
        string(FIND "${RELOCATION_SCRIPT_PAIR}" ":" SPLIT_INDEX)

        string(SUBSTRING ${RELOCATION_SCRIPT_PAIR} 0 ${SPLIT_INDEX} SYMLINK_)

        math(EXPR POINT_PATH_START ${SPLIT_INDEX}+1+${POINT_PATH_LEN})
        string(SUBSTRING ${RELOCATION_SCRIPT_PAIR} ${POINT_PATH_START} -1 POINT_)

        string(APPEND SCRIPT "  if [ -z \"$CPACK_RPM_RELOCATED_SYMLINK_${RELOCATION_INDEX}\" ]; then\n")
        string(APPEND SCRIPT "    ln -s \"$RPM_INSTALL_PREFIX${POINT_INDEX}${POINT_}\" \"${SYMLINK_}\"\n")
        string(APPEND SCRIPT "    CPACK_RPM_RELOCATED_SYMLINK_${RELOCATION_INDEX}=true\n")
        string(APPEND SCRIPT "  fi\n")
      endforeach()

      string(APPEND SCRIPT "fi\n")
    endif()
  endforeach()

  # no path relocated
  if(_RPM_RELOCATION_SCRIPT_X_X)
    foreach(RELOCATION_NO IN LISTS _RPM_RELOCATION_SCRIPT_X_X)
      math(EXPR RELOCATION_INDEX ${RELOCATION_NO}-1)
      list(GET _RPM_RELOCATION_SCRIPT_PAIRS ${RELOCATION_INDEX} RELOCATION_SCRIPT_PAIR)
      string(FIND "${RELOCATION_SCRIPT_PAIR}" ":" SPLIT_INDEX)

      string(SUBSTRING ${RELOCATION_SCRIPT_PAIR} 0 ${SPLIT_INDEX} SYMLINK_)

      math(EXPR POINT_PATH_START ${SPLIT_INDEX}+1)
      string(SUBSTRING ${RELOCATION_SCRIPT_PAIR} ${POINT_PATH_START} -1 POINT_)

      string(APPEND SCRIPT "if [ -z \"$CPACK_RPM_RELOCATED_SYMLINK_${RELOCATION_INDEX}\" ]; then\n")
      string(APPEND SCRIPT "  ln -s \"${POINT_}\" \"${SYMLINK_}\"\n")
      string(APPEND SCRIPT "fi\n")
    endforeach()
  endif()

  set(RPM_SYMLINK_POSTINSTALL "${SCRIPT}" PARENT_SCOPE)
endfunction()

function(cpack_rpm_symlink_add_for_relocation_script PACKAGE_PREFIXES SYMLINK SYMLINK_RELOCATION_PATHS POINT POINT_RELOCATION_PATHS)
  list(LENGTH SYMLINK_RELOCATION_PATHS SYMLINK_PATHS_COUTN)
  list(LENGTH POINT_RELOCATION_PATHS POINT_PATHS_COUNT)

  list(APPEND _RPM_RELOCATION_SCRIPT_PAIRS "${SYMLINK}:${POINT}")
  list(LENGTH _RPM_RELOCATION_SCRIPT_PAIRS PAIR_NO)

  if(SYMLINK_PATHS_COUTN)
    foreach(SYMLINK_RELOC_PATH IN LISTS SYMLINK_RELOCATION_PATHS)
      list(FIND PACKAGE_PREFIXES "${SYMLINK_RELOC_PATH}" SYMLINK_INDEX)

      # source path relocated
      list(APPEND _RPM_RELOCATION_SCRIPT_${SYMLINK_INDEX}_X "${PAIR_NO}")
      list(APPEND RELOCATION_VARS "_RPM_RELOCATION_SCRIPT_${SYMLINK_INDEX}_X")

      foreach(POINT_RELOC_PATH IN LISTS POINT_RELOCATION_PATHS)
        list(FIND PACKAGE_PREFIXES "${POINT_RELOC_PATH}" POINT_INDEX)

        # both paths relocated
        list(APPEND _RPM_RELOCATION_SCRIPT_${SYMLINK_INDEX}_${POINT_INDEX} "${PAIR_NO}")
        list(APPEND RELOCATION_VARS "_RPM_RELOCATION_SCRIPT_${SYMLINK_INDEX}_${POINT_INDEX}")

        # point path relocated
        list(APPEND _RPM_RELOCATION_SCRIPT_X_${POINT_INDEX} "${PAIR_NO}")
        list(APPEND RELOCATION_VARS "_RPM_RELOCATION_SCRIPT_X_${POINT_INDEX}")
      endforeach()
    endforeach()
  elseif(POINT_PATHS_COUNT)
    foreach(POINT_RELOC_PATH IN LISTS POINT_RELOCATION_PATHS)
      list(FIND PACKAGE_PREFIXES "${POINT_RELOC_PATH}" POINT_INDEX)

      # point path relocated
      list(APPEND _RPM_RELOCATION_SCRIPT_X_${POINT_INDEX} "${PAIR_NO}")
      list(APPEND RELOCATION_VARS "_RPM_RELOCATION_SCRIPT_X_${POINT_INDEX}")
    endforeach()
  endif()

  # no path relocated
  list(APPEND _RPM_RELOCATION_SCRIPT_X_X "${PAIR_NO}")
  list(APPEND RELOCATION_VARS "_RPM_RELOCATION_SCRIPT_X_X")

  # place variables into parent scope
  foreach(VAR IN LISTS RELOCATION_VARS)
    set(${VAR} "${${VAR}}" PARENT_SCOPE)
  endforeach()
  set(_RPM_RELOCATION_SCRIPT_PAIRS "${_RPM_RELOCATION_SCRIPT_PAIRS}" PARENT_SCOPE)
  set(REQUIRES_SYMLINK_RELOCATION_SCRIPT "true" PARENT_SCOPE)
  set(DIRECTIVE "%ghost " PARENT_SCOPE)
endfunction()

function(cpack_rpm_prepare_install_files INSTALL_FILES_LIST WDIR PACKAGE_PREFIXES IS_RELOCATABLE)
  # Prepend directories in ${CPACK_RPM_INSTALL_FILES} with %dir
  # This is necessary to avoid duplicate files since rpmbuild does
  # recursion on its own when encountering a pathname which is a directory
  # which is not flagged as %dir
  string(STRIP "${INSTALL_FILES_LIST}" INSTALL_FILES_LIST)
  string(REPLACE "\n" ";" INSTALL_FILES_LIST
                          "${INSTALL_FILES_LIST}")
  string(REPLACE "\"" "" INSTALL_FILES_LIST
                          "${INSTALL_FILES_LIST}")
  string(LENGTH "${WDIR}" WDR_LEN_)

  list(SORT INSTALL_FILES_LIST) # make file order consistent on all platforms

  foreach(F IN LISTS INSTALL_FILES_LIST)
    unset(DIRECTIVE)

    if(IS_SYMLINK "${WDIR}/${F}")
      if(IS_RELOCATABLE)
        # check that symlink has relocatable format
        get_filename_component(SYMLINK_LOCATION_ "${WDIR}/${F}" DIRECTORY)
        execute_process(COMMAND ls -la "${WDIR}/${F}"
                  WORKING_DIRECTORY "${WDIR}"
                  OUTPUT_VARIABLE SYMLINK_POINT_
                  OUTPUT_STRIP_TRAILING_WHITESPACE)

        string(FIND "${SYMLINK_POINT_}" "->" SYMLINK_POINT_INDEX_ REVERSE)
        math(EXPR SYMLINK_POINT_INDEX_ ${SYMLINK_POINT_INDEX_}+3)
        string(LENGTH "${SYMLINK_POINT_}" SYMLINK_POINT_LENGTH_)

        # get destination path
        string(SUBSTRING "${SYMLINK_POINT_}" ${SYMLINK_POINT_INDEX_} ${SYMLINK_POINT_LENGTH_} SYMLINK_POINT_)

        # check if path is relative or absolute
        string(SUBSTRING "${SYMLINK_POINT_}" 0 1 SYMLINK_IS_ABSOLUTE_)

        if(${SYMLINK_IS_ABSOLUTE_} STREQUAL "/")
          # prevent absolute paths from having /../ or /./ section inside of them
          get_filename_component(SYMLINK_POINT_ "${SYMLINK_POINT_}" ABSOLUTE)
        else()
          # handle relative path
          get_filename_component(SYMLINK_POINT_ "${SYMLINK_LOCATION_}/${SYMLINK_POINT_}" ABSOLUTE)
        endif()

        # recalculate path length after conversion to canonical form
        string(LENGTH "${SYMLINK_POINT_}" SYMLINK_POINT_LENGTH_)

        if(SYMLINK_POINT_ MATCHES "${WDIR}/.*")
          # only symlinks that are pointing inside the packaging structure should be checked for relocation
          string(SUBSTRING "${SYMLINK_POINT_}" ${WDR_LEN_} -1 SYMLINK_POINT_WD_)
          cpack_rpm_symlink_get_relocation_prefixes("${F}" "${PACKAGE_PREFIXES}" "SYMLINK_RELOCATIONS")
          cpack_rpm_symlink_get_relocation_prefixes("${SYMLINK_POINT_WD_}" "${PACKAGE_PREFIXES}" "POINT_RELOCATIONS")

          list(LENGTH SYMLINK_RELOCATIONS SYMLINK_RELOCATIONS_COUNT)
          list(LENGTH POINT_RELOCATIONS POINT_RELOCATIONS_COUNT)
        else()
          # location pointed to is ouside WDR so it should be treated as a permanent symlink
          set(SYMLINK_POINT_WD_ "${SYMLINK_POINT_}")

          unset(SYMLINK_RELOCATIONS)
          unset(POINT_RELOCATIONS)
          unset(SYMLINK_RELOCATIONS_COUNT)
          unset(POINT_RELOCATIONS_COUNT)

          message(AUTHOR_WARNING "CPackRPM:Warning: Symbolic link '${F}' points to location that is outside packaging path! Link will possibly not be relocatable.")
        endif()

        if(SYMLINK_RELOCATIONS_COUNT AND POINT_RELOCATIONS_COUNT)
          # find matching
          foreach(SYMLINK_RELOCATION_PREFIX IN LISTS SYMLINK_RELOCATIONS)
            list(FIND POINT_RELOCATIONS "${SYMLINK_RELOCATION_PREFIX}" FOUND_INDEX)
            if(NOT ${FOUND_INDEX} EQUAL -1)
              break()
            endif()
          endforeach()

          if(NOT ${FOUND_INDEX} EQUAL -1)
            # symlinks have the same subpath
            if(${SYMLINK_RELOCATIONS_COUNT} EQUAL 1 AND ${POINT_RELOCATIONS_COUNT} EQUAL 1)
              # permanent symlink
              get_filename_component(SYMLINK_LOCATION_ "${F}" DIRECTORY)
              file(RELATIVE_PATH FINAL_PATH_ ${SYMLINK_LOCATION_} ${SYMLINK_POINT_WD_})
              execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "${FINAL_PATH_}" "${WDIR}/${F}")
            else()
              # relocation subpaths
              cpack_rpm_symlink_add_for_relocation_script("${PACKAGE_PREFIXES}" "${F}" "${SYMLINK_RELOCATIONS}"
                  "${SYMLINK_POINT_WD_}" "${POINT_RELOCATIONS}")
            endif()
          else()
            # not on the same relocation path
            cpack_rpm_symlink_add_for_relocation_script("${PACKAGE_PREFIXES}" "${F}" "${SYMLINK_RELOCATIONS}"
                "${SYMLINK_POINT_WD_}" "${POINT_RELOCATIONS}")
          endif()
        elseif(POINT_RELOCATIONS_COUNT)
          # point is relocatable
          cpack_rpm_symlink_add_for_relocation_script("${PACKAGE_PREFIXES}" "${F}" "${SYMLINK_RELOCATIONS}"
              "${SYMLINK_POINT_WD_}" "${POINT_RELOCATIONS}")
        else()
          # is not relocatable or points to non relocatable path - permanent symlink
          execute_process(COMMAND "${CMAKE_COMMAND}" -E create_symlink "${SYMLINK_POINT_WD_}" "${WDIR}/${F}")
        endif()
      endif()
    elseif(IS_DIRECTORY "${WDIR}/${F}")
      set(DIRECTIVE "%dir ")
    endif()

    string(APPEND INSTALL_FILES "${DIRECTIVE}\"${F}\"\n")
  endforeach()

  if(REQUIRES_SYMLINK_RELOCATION_SCRIPT)
    cpack_rpm_symlink_create_relocation_script("${PACKAGE_PREFIXES}")
  endif()

  set(RPM_SYMLINK_POSTINSTALL "${RPM_SYMLINK_POSTINSTALL}" PARENT_SCOPE)
  set(CPACK_RPM_INSTALL_FILES "${INSTALL_FILES}" PARENT_SCOPE)
endfunction()

if(CMAKE_BINARY_DIR)
  message(FATAL_ERROR "CPackRPM.cmake may only be used by CPack internally.")
endif()

if(NOT UNIX)
  message(FATAL_ERROR "CPackRPM.cmake may only be used under UNIX.")
endif()

# We need to check if the binaries were compiled with debug symbols
# because without them the package will be useless
function(cpack_rpm_debugsymbol_check INSTALL_FILES WORKING_DIR)
  if(NOT CPACK_BUILD_SOURCE_DIRS)
    message(FATAL_ERROR "CPackRPM: CPACK_BUILD_SOURCE_DIRS variable is not set!"
      " Required for debuginfo packaging. See documentation of"
      " CPACK_RPM_DEBUGINFO_PACKAGE variable for details.")
  endif()

  # With objdump we should check the debug symbols
  find_program(OBJDUMP_EXECUTABLE objdump)
  if(NOT OBJDUMP_EXECUTABLE)
    message(FATAL_ERROR "CPackRPM: objdump binary could not be found!"
      " Required for debuginfo packaging. See documentation of"
      " CPACK_RPM_DEBUGINFO_PACKAGE variable for details.")
  endif()

  # With debugedit we prepare source files list
  find_program(DEBUGEDIT_EXECUTABLE debugedit "/usr/lib/rpm/")
  if(NOT DEBUGEDIT_EXECUTABLE)
    message(FATAL_ERROR "CPackRPM: debugedit binary could not be found!"
      " Required for debuginfo packaging. See documentation of"
      " CPACK_RPM_DEBUGINFO_PACKAGE variable for details.")
  endif()

  unset(mkdir_list_)
  unset(cp_list_)
  unset(additional_sources_)

  foreach(F IN LISTS INSTALL_FILES)
    if(IS_DIRECTORY "${WORKING_DIR}/${F}" OR IS_SYMLINK "${WORKING_DIR}/${F}")
      continue()
    endif()

    execute_process(COMMAND "${OBJDUMP_EXECUTABLE}" -h ${WORKING_DIR}/${F}
                    WORKING_DIRECTORY "${CPACK_TOPLEVEL_DIRECTORY}"
                    RESULT_VARIABLE OBJDUMP_EXEC_RESULT
                    OUTPUT_VARIABLE OBJDUMP_OUT
                    ERROR_QUIET)
    # Check that if the given file was executable or not
    if(NOT OBJDUMP_EXEC_RESULT)
      string(FIND "${OBJDUMP_OUT}" "debug" FIND_RESULT)
      if(FIND_RESULT GREATER -1)
        set(index_ 0)
        foreach(source_dir_ IN LISTS CPACK_BUILD_SOURCE_DIRS)
          string(LENGTH "${source_dir_}" source_dir_len_)
          string(LENGTH "${CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX}/src_${index_}" debuginfo_dir_len)
          if(source_dir_len_ LESS debuginfo_dir_len)
            message(FATAL_ERROR "CPackRPM: source dir path '${source_dir_}' is"
              " shorter than debuginfo sources dir path '${CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX}/src_${index_}'!"
              " Source dir path must be longer than debuginfo sources dir path."
              " Set CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX variable to a shorter value"
              " or make source dir path longer."
              " Required for debuginfo packaging. See documentation of"
              " CPACK_RPM_DEBUGINFO_PACKAGE variable for details.")
          endif()

          file(REMOVE "${CPACK_RPM_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}${CPACK_RPM_PACKAGE_COMPONENT_PART_PATH}/debugsources_add.list")
          execute_process(COMMAND "${DEBUGEDIT_EXECUTABLE}" -b "${source_dir_}" -d "${CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX}/src_${index_}" -i -l "${CPACK_RPM_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}${CPACK_RPM_PACKAGE_COMPONENT_PART_PATH}/debugsources_add.list" "${WORKING_DIR}/${F}"
              RESULT_VARIABLE res_
              OUTPUT_VARIABLE opt_
              ERROR_VARIABLE err_
            )

          file(STRINGS
            "${CPACK_RPM_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}${CPACK_RPM_PACKAGE_COMPONENT_PART_PATH}/debugsources_add.list"
            sources_)
          list(REMOVE_DUPLICATES sources_)

          foreach(source_ IN LISTS sources_)
            if(EXISTS "${source_dir_}/${source_}" AND NOT IS_DIRECTORY "${source_dir_}/${source_}")
              get_filename_component(path_part_ "${source_}" DIRECTORY)
              list(APPEND mkdir_list_ "%{buildroot}${CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX}/src_${index_}/${path_part_}")
              list(APPEND cp_list_ "cp \"${source_dir_}/${source_}\" \"%{buildroot}${CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX}/src_${index_}/${path_part_}\"")

              list(APPEND additional_sources_ "${CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX}/src_${index_}/${source_}")
            endif()
          endforeach()

          math(EXPR index_ "${index_} + 1")
        endforeach()
      else()
        message(WARNING "CPackRPM: File: ${F} does not contain debug symbols. They will possibly be missing from debuginfo package!")
      endif()
    endif()
  endforeach()

  list(LENGTH mkdir_list_ len_)
  if(len_)
    list(REMOVE_DUPLICATES mkdir_list_)
    unset(TMP_RPM_DEBUGINFO_INSTALL)
    foreach(part_ IN LISTS mkdir_list_)
      string(APPEND TMP_RPM_DEBUGINFO_INSTALL "mkdir -p \"${part_}\"\n")
    endforeach()
  endif()

  list(LENGTH cp_list_ len_)
  if(len_)
    list(REMOVE_DUPLICATES cp_list_)
    foreach(part_ IN LISTS cp_list_)
      string(APPEND TMP_RPM_DEBUGINFO_INSTALL "${part_}\n")
    endforeach()
  endif()

  if(NOT DEFINED CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS)
    set(CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS /usr /usr/src /usr/src/debug)
    if(CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS_ADDITION)
      if(CPACK_RPM_PACKAGE_DEBUG)
        message("CPackRPM:Debug: Adding ${CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS_ADDITION} to builtin omit list.")
      endif()
      list(APPEND CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS "${CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS_ADDITION}")
    endif()
  endif()
  if(CPACK_RPM_PACKAGE_DEBUG)
    message("CPackRPM:Debug: CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS= ${CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS}")
  endif()

  list(LENGTH additional_sources_ len_)
  if(len_)
    list(REMOVE_DUPLICATES additional_sources_)
    unset(additional_sources_all_)
    foreach(source_ IN LISTS additional_sources_)
      string(REPLACE "/" ";" split_source_ " ${source_}")
      list(REMOVE_AT split_source_ 0)
      unset(tmp_path_)
      # Now generate all segments of the path
      foreach(segment_ IN LISTS split_source_)
        string(APPEND tmp_path_ "/${segment_}")
        list(APPEND additional_sources_all_ "${tmp_path_}")
      endforeach()
    endforeach()

    list(REMOVE_DUPLICATES additional_sources_all_)
    list(REMOVE_ITEM additional_sources_all_
      ${CPACK_RPM_DEBUGINFO_EXCLUDE_DIRS})

    unset(TMP_DEBUGINFO_ADDITIONAL_SOURCES)
    foreach(source_ IN LISTS additional_sources_all_)
      string(APPEND TMP_DEBUGINFO_ADDITIONAL_SOURCES "${source_}\n")
    endforeach()
  endif()

  set(TMP_RPM_DEBUGINFO_INSTALL "${TMP_RPM_DEBUGINFO_INSTALL}" PARENT_SCOPE)
  set(TMP_DEBUGINFO_ADDITIONAL_SOURCES "${TMP_DEBUGINFO_ADDITIONAL_SOURCES}"
    PARENT_SCOPE)
endfunction()

function(cpack_rpm_variable_fallback OUTPUT_VAR_NAME)
  set(FALLBACK_VAR_NAMES ${ARGN})

  foreach(variable_name IN LISTS FALLBACK_VAR_NAMES)
    if(${variable_name})
      set(${OUTPUT_VAR_NAME} "${${variable_name}}" PARENT_SCOPE)
      break()
    endif()
  endforeach()
endfunction()

function(cpack_rpm_generate_package)
  # rpmbuild is the basic command for building RPM package
  # it may be a simple (symbolic) link to rpm command.
  find_program(RPMBUILD_EXECUTABLE rpmbuild)

  # Check version of the rpmbuild tool this would be easier to
  # track bugs with users and CPackRPM debug mode.
  # We may use RPM version in order to check for available version dependent features
  if(RPMBUILD_EXECUTABLE)
    execute_process(COMMAND ${RPMBUILD_EXECUTABLE} --version
                    OUTPUT_VARIABLE _TMP_VERSION
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX REPLACE "^.* " ""
           RPMBUILD_EXECUTABLE_VERSION
           ${_TMP_VERSION})
    if(CPACK_RPM_PACKAGE_DEBUG)
      message("CPackRPM:Debug: rpmbuild version is <${RPMBUILD_EXECUTABLE_VERSION}>")
    endif()
  endif()

  if(NOT RPMBUILD_EXECUTABLE)
    message(FATAL_ERROR "RPM package requires rpmbuild executable")
  endif()

  # Display lsb_release output if DEBUG mode enable
  # This will help to diagnose problem with CPackRPM
  # because we will know on which kind of Linux we are
  if(CPACK_RPM_PACKAGE_DEBUG)
    find_program(LSB_RELEASE_EXECUTABLE lsb_release)
    if(LSB_RELEASE_EXECUTABLE)
      execute_process(COMMAND ${LSB_RELEASE_EXECUTABLE} -a
                      OUTPUT_VARIABLE _TMP_LSB_RELEASE_OUTPUT
                      ERROR_QUIET
                      OUTPUT_STRIP_TRAILING_WHITESPACE)
      string(REGEX REPLACE "\n" ", "
             LSB_RELEASE_OUTPUT
             ${_TMP_LSB_RELEASE_OUTPUT})
    else ()
      set(LSB_RELEASE_OUTPUT "lsb_release not installed/found!")
    endif()
    message("CPackRPM:Debug: LSB_RELEASE  = ${LSB_RELEASE_OUTPUT}")
  endif()

  # We may use RPM version in the future in order
  # to shut down warning about space in buildtree
  # some recent RPM version should support space in different places.
  # not checked [yet].
  if(CPACK_TOPLEVEL_DIRECTORY MATCHES ".* .*")
    message(FATAL_ERROR "${RPMBUILD_EXECUTABLE} can't handle paths with spaces, use a build directory without spaces for building RPMs.")
  endif()

  # If rpmbuild is found
  # we try to discover alien since we may be on non RPM distro like Debian.
  # In this case we may try to to use more advanced features
  # like generating RPM directly from DEB using alien.
  # FIXME feature not finished (yet)
  find_program(ALIEN_EXECUTABLE alien)
  if(ALIEN_EXECUTABLE)
    message(STATUS "alien found, we may be on a Debian based distro.")
  endif()

  # Are we packaging components ?
  if(CPACK_RPM_PACKAGE_COMPONENT)
    string(TOUPPER ${CPACK_RPM_PACKAGE_COMPONENT} CPACK_RPM_PACKAGE_COMPONENT_UPPER)
  endif()

  set(WDIR "${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}${CPACK_RPM_PACKAGE_COMPONENT_PART_PATH}")

  #
  # Use user-defined RPM specific variables value
  # or generate reasonable default value from
  # CPACK_xxx generic values.
  # The variables comes from the needed (mandatory or not)
  # values found in the RPM specification file aka ".spec" file.
  # The variables which may/should be defined are:
  #

  # CPACK_RPM_PACKAGE_SUMMARY (mandatory)

  if(CPACK_RPM_PACKAGE_COMPONENT)
    cpack_rpm_variable_fallback("CPACK_RPM_PACKAGE_SUMMARY"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_PACKAGE_SUMMARY"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_PACKAGE_SUMMARY")
  endif()

  if(NOT CPACK_RPM_PACKAGE_SUMMARY)
    if(CPACK_PACKAGE_DESCRIPTION_SUMMARY)
      set(CPACK_RPM_PACKAGE_SUMMARY ${CPACK_PACKAGE_DESCRIPTION_SUMMARY})
    else()
      # if neither var is defined lets use the name as summary
      string(TOLOWER "${CPACK_PACKAGE_NAME}" CPACK_RPM_PACKAGE_SUMMARY)
    endif()
  endif()

  # CPACK_RPM_PACKAGE_NAME (mandatory)
  if(NOT CPACK_RPM_PACKAGE_NAME)
    string(TOLOWER "${CPACK_PACKAGE_NAME}" CPACK_RPM_PACKAGE_NAME)
  endif()

  if(CPACK_RPM_PACKAGE_COMPONENT)
    string(TOUPPER "${CPACK_RPM_MAIN_COMPONENT}"
      CPACK_RPM_MAIN_COMPONENT_UPPER)

    if(NOT CPACK_RPM_MAIN_COMPONENT_UPPER STREQUAL CPACK_RPM_PACKAGE_COMPONENT_UPPER)
      string(APPEND CPACK_RPM_PACKAGE_NAME "-${CPACK_RPM_PACKAGE_COMPONENT}")

      cpack_rpm_variable_fallback("CPACK_RPM_PACKAGE_NAME"
        "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_PACKAGE_NAME"
        "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_PACKAGE_NAME")
    endif()
  endif()

  # CPACK_RPM_PACKAGE_VERSION (mandatory)
  if(NOT CPACK_RPM_PACKAGE_VERSION)
    if(NOT CPACK_PACKAGE_VERSION)
      message(FATAL_ERROR "RPM package requires a package version")
    endif()
    set(CPACK_RPM_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION})
  endif()
  # Replace '-' in version with '_'
  # '-' character is  an Illegal RPM version character
  # it is illegal because it is used to separate
  # RPM "Version" from RPM "Release"
  string(REPLACE "-" "_" CPACK_RPM_PACKAGE_VERSION ${CPACK_RPM_PACKAGE_VERSION})

  # CPACK_RPM_PACKAGE_ARCHITECTURE (mandatory)
  if(NOT CPACK_RPM_PACKAGE_ARCHITECTURE)
    execute_process(COMMAND uname "-m"
                    OUTPUT_VARIABLE CPACK_RPM_PACKAGE_ARCHITECTURE
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
  else()
    if(CPACK_RPM_PACKAGE_DEBUG)
      message("CPackRPM:Debug: using user-specified build arch = ${CPACK_RPM_PACKAGE_ARCHITECTURE}")
    endif()
  endif()

  if(CPACK_RPM_PACKAGE_COMPONENT)
    cpack_rpm_variable_fallback("CPACK_RPM_PACKAGE_ARCHITECTURE"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_PACKAGE_ARCHITECTURE"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_PACKAGE_ARCHITECTURE")

    if(CPACK_RPM_PACKAGE_DEBUG)
      message("CPackRPM:Debug: using component build arch = ${CPACK_RPM_PACKAGE_ARCHITECTURE}")
    endif()
  endif()

  if(${CPACK_RPM_PACKAGE_ARCHITECTURE} STREQUAL "noarch")
    set(TMP_RPM_BUILDARCH "Buildarch: ${CPACK_RPM_PACKAGE_ARCHITECTURE}")
  else()
    set(TMP_RPM_BUILDARCH "")
  endif()

  # CPACK_RPM_PACKAGE_RELEASE
  # The RPM release is the numbering of the RPM package ITSELF
  # this is the version of the PACKAGING and NOT the version
  # of the CONTENT of the package.
  # You may well need to generate a new RPM package release
  # without changing the version of the packaged software.
  # This is the case when the packaging is buggy (not) the software :=)
  # If not set, 1 is a good candidate
  if(NOT CPACK_RPM_PACKAGE_RELEASE)
    set(CPACK_RPM_PACKAGE_RELEASE "1")
  endif()

  if(CPACK_RPM_PACKAGE_RELEASE_DIST)
    string(APPEND CPACK_RPM_PACKAGE_RELEASE "%{?dist}")
  endif()

  # CPACK_RPM_PACKAGE_LICENSE
  if(NOT CPACK_RPM_PACKAGE_LICENSE)
    set(CPACK_RPM_PACKAGE_LICENSE "unknown")
  endif()

  # CPACK_RPM_PACKAGE_GROUP
  if(CPACK_RPM_PACKAGE_COMPONENT)
    cpack_rpm_variable_fallback("CPACK_RPM_PACKAGE_GROUP"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_PACKAGE_GROUP"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_PACKAGE_GROUP")
  endif()

  if(NOT CPACK_RPM_PACKAGE_GROUP)
    set(CPACK_RPM_PACKAGE_GROUP "unknown")
  endif()

  # CPACK_RPM_PACKAGE_VENDOR
  if(NOT CPACK_RPM_PACKAGE_VENDOR)
    if(CPACK_PACKAGE_VENDOR)
      set(CPACK_RPM_PACKAGE_VENDOR "${CPACK_PACKAGE_VENDOR}")
    else()
      set(CPACK_RPM_PACKAGE_VENDOR "unknown")
    endif()
  endif()

  # CPACK_RPM_PACKAGE_SOURCE
  # The name of the source tarball in case we generate a source RPM

  # CPACK_RPM_PACKAGE_DESCRIPTION
  # The variable content may be either
  #   - explicitly given by the user or
  #   - filled with the content of CPACK_PACKAGE_DESCRIPTION_FILE
  #     if it is defined
  #   - set to a default value
  #

  if(CPACK_RPM_PACKAGE_COMPONENT)
    cpack_rpm_variable_fallback("CPACK_RPM_PACKAGE_DESCRIPTION"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_PACKAGE_DESCRIPTION"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_PACKAGE_DESCRIPTION"
      "CPACK_COMPONENT_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_DESCRIPTION")
  endif()

  if(NOT CPACK_RPM_PACKAGE_DESCRIPTION)
    if(CPACK_PACKAGE_DESCRIPTION_FILE)
      file(READ ${CPACK_PACKAGE_DESCRIPTION_FILE} CPACK_RPM_PACKAGE_DESCRIPTION)
    else ()
      set(CPACK_RPM_PACKAGE_DESCRIPTION "no package description available")
    endif ()
  endif ()

  # CPACK_RPM_COMPRESSION_TYPE
  #
  if (CPACK_RPM_COMPRESSION_TYPE)
     if(CPACK_RPM_PACKAGE_DEBUG)
       message("CPackRPM:Debug: User Specified RPM compression type: ${CPACK_RPM_COMPRESSION_TYPE}")
     endif()
     if(CPACK_RPM_COMPRESSION_TYPE STREQUAL "lzma")
       set(CPACK_RPM_COMPRESSION_TYPE_TMP "%define _binary_payload w9.lzdio")
     endif()
     if(CPACK_RPM_COMPRESSION_TYPE STREQUAL "xz")
       set(CPACK_RPM_COMPRESSION_TYPE_TMP "%define _binary_payload w7.xzdio")
     endif()
     if(CPACK_RPM_COMPRESSION_TYPE STREQUAL "bzip2")
       set(CPACK_RPM_COMPRESSION_TYPE_TMP "%define _binary_payload w9.bzdio")
     endif()
     if(CPACK_RPM_COMPRESSION_TYPE STREQUAL "gzip")
       set(CPACK_RPM_COMPRESSION_TYPE_TMP "%define _binary_payload w9.gzdio")
     endif()
  else()
     set(CPACK_RPM_COMPRESSION_TYPE_TMP "")
  endif()

  if(NOT CPACK_RPM_PACKAGE_SOURCES)
    if(CPACK_PACKAGE_RELOCATABLE OR CPACK_RPM_PACKAGE_RELOCATABLE)
      if(CPACK_RPM_PACKAGE_DEBUG)
        message("CPackRPM:Debug: Trying to build a relocatable package")
      endif()
      if(CPACK_SET_DESTDIR AND (NOT CPACK_SET_DESTDIR STREQUAL "I_ON"))
        message("CPackRPM:Warning: CPACK_SET_DESTDIR is set (=${CPACK_SET_DESTDIR}) while requesting a relocatable package (CPACK_RPM_PACKAGE_RELOCATABLE is set): this is not supported, the package won't be relocatable.")
        set(CPACK_RPM_PACKAGE_RELOCATABLE FALSE)
      else()
        set(CPACK_RPM_PACKAGE_PREFIX ${CPACK_PACKAGING_INSTALL_PREFIX}) # kept for back compatibility (provided external RPM spec files)
        cpack_rpm_prepare_relocation_paths()
        set(CPACK_RPM_PACKAGE_RELOCATABLE TRUE)
      endif()
    endif()
  else()
    if(CPACK_RPM_PACKAGE_COMPONENT)
      message(FATAL_ERROR "CPACK_RPM_PACKAGE_SOURCES parameter can not be used"
        " in combination with CPACK_RPM_PACKAGE_COMPONENT parameter!")
    endif()

    set(CPACK_RPM_PACKAGE_RELOCATABLE FALSE) # disable relocatable option if building source RPM
  endif()

  execute_process(
    COMMAND "${RPMBUILD_EXECUTABLE}" --querytags
    OUTPUT_VARIABLE RPMBUILD_TAG_LIST
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(REPLACE "\n" ";" RPMBUILD_TAG_LIST "${RPMBUILD_TAG_LIST}")

  # Check if additional fields for RPM spec header are given
  # There may be some COMPONENT specific variables as well
  # If component specific var is not provided we use the global one
  # for each component
  foreach(_RPM_SPEC_HEADER URL REQUIRES SUGGESTS PROVIDES OBSOLETES PREFIX CONFLICTS AUTOPROV AUTOREQ AUTOREQPROV REQUIRES_PRE REQUIRES_POST REQUIRES_PREUN REQUIRES_POSTUN)
    if(CPACK_RPM_PACKAGE_DEBUG)
      message("CPackRPM:Debug: processing ${_RPM_SPEC_HEADER}")
    endif()

    if(CPACK_RPM_PACKAGE_COMPONENT)
      cpack_rpm_variable_fallback("CPACK_RPM_PACKAGE_${_RPM_SPEC_HEADER}"
        "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_PACKAGE_${_RPM_SPEC_HEADER}"
        "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_PACKAGE_${_RPM_SPEC_HEADER}")
    endif()

    if(DEFINED CPACK_RPM_PACKAGE_${_RPM_SPEC_HEADER})
      cmake_policy(PUSH)
        cmake_policy(SET CMP0057 NEW)
        # Prefix can be replaced by Prefixes but the old version stil works so we'll ignore it for now
        # Requires* is a special case because it gets transformed to Requires(pre/post/preun/postun)
        # Auto* is a special case because the tags can not be queried by querytags rpmbuild flag
        set(special_case_tags_ PREFIX REQUIRES_PRE REQUIRES_POST REQUIRES_PREUN REQUIRES_POSTUN AUTOPROV AUTOREQ AUTOREQPROV)
        if(NOT _RPM_SPEC_HEADER IN_LIST RPMBUILD_TAG_LIST AND NOT _RPM_SPEC_HEADER IN_LIST special_case_tags_)
          cmake_policy(POP)
          message(AUTHOR_WARNING "CPackRPM:Warning: ${_RPM_SPEC_HEADER} not "
              "supported in provided rpmbuild. Tag will not be used.")
          continue()
        endif()
      cmake_policy(POP)

      if(CPACK_RPM_PACKAGE_DEBUG)
        message("CPackRPM:Debug: using CPACK_RPM_PACKAGE_${_RPM_SPEC_HEADER}")
      endif()

      set(CPACK_RPM_PACKAGE_${_RPM_SPEC_HEADER}_TMP ${CPACK_RPM_PACKAGE_${_RPM_SPEC_HEADER}})
    endif()

    # Treat the RPM Spec keyword iff it has been properly defined
    if(DEFINED CPACK_RPM_PACKAGE_${_RPM_SPEC_HEADER}_TMP)
      # Transform NAME --> Name e.g. PROVIDES --> Provides
      # The Upper-case first letter and lowercase tail is the
      # appropriate value required in the final RPM spec file.
      string(SUBSTRING ${_RPM_SPEC_HEADER} 1 -1 _PACKAGE_HEADER_TAIL)
      string(TOLOWER "${_PACKAGE_HEADER_TAIL}" _PACKAGE_HEADER_TAIL)
      string(SUBSTRING ${_RPM_SPEC_HEADER} 0 1 _PACKAGE_HEADER_NAME)
      string(APPEND _PACKAGE_HEADER_NAME "${_PACKAGE_HEADER_TAIL}")
      # The following keywords require parentheses around the "pre" or "post" suffix in the final RPM spec file.
      set(SCRIPTS_REQUIREMENTS_LIST REQUIRES_PRE REQUIRES_POST REQUIRES_PREUN REQUIRES_POSTUN)
      list(FIND SCRIPTS_REQUIREMENTS_LIST ${_RPM_SPEC_HEADER} IS_SCRIPTS_REQUIREMENT_FOUND)
      if(NOT ${IS_SCRIPTS_REQUIREMENT_FOUND} EQUAL -1)
        string(REPLACE "_" "(" _PACKAGE_HEADER_NAME "${_PACKAGE_HEADER_NAME}")
        string(APPEND _PACKAGE_HEADER_NAME ")")
      endif()
      if(CPACK_RPM_PACKAGE_DEBUG)
        message("CPackRPM:Debug: User defined ${_PACKAGE_HEADER_NAME}:\n ${CPACK_RPM_PACKAGE_${_RPM_SPEC_HEADER}_TMP}")
      endif()
      set(TMP_RPM_${_RPM_SPEC_HEADER} "${_PACKAGE_HEADER_NAME}: ${CPACK_RPM_PACKAGE_${_RPM_SPEC_HEADER}_TMP}")
      unset(CPACK_RPM_PACKAGE_${_RPM_SPEC_HEADER}_TMP)
    endif()
  endforeach()

  # CPACK_RPM_SPEC_INSTALL_POST
  # May be used to define a RPM post intallation script
  # for example setting it to "/bin/true" may prevent
  # rpmbuild from stripping binaries.
  if(CPACK_RPM_SPEC_INSTALL_POST)
    if(CPACK_RPM_PACKAGE_DEBUG)
      message("CPackRPM:Debug: User defined CPACK_RPM_SPEC_INSTALL_POST = ${CPACK_RPM_SPEC_INSTALL_POST}")
    endif()
    set(TMP_RPM_SPEC_INSTALL_POST "%define __spec_install_post ${CPACK_RPM_SPEC_INSTALL_POST}")
  endif()

  # CPACK_RPM_POST_INSTALL_SCRIPT_FILE (or CPACK_RPM_<COMPONENT>_POST_INSTALL_SCRIPT_FILE)
  # CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE (or CPACK_RPM_<COMPONENT>_POST_UNINSTALL_SCRIPT_FILE)
  # May be used to embed a post (un)installation script in the spec file.
  # The refered script file(s) will be read and directly
  # put after the %post or %postun section
  # ----------------------------------------------------------------
  # CPACK_RPM_PRE_INSTALL_SCRIPT_FILE (or CPACK_RPM_<COMPONENT>_PRE_INSTALL_SCRIPT_FILE)
  # CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE (or CPACK_RPM_<COMPONENT>_PRE_UNINSTALL_SCRIPT_FILE)
  # May be used to embed a pre (un)installation script in the spec file.
  # The refered script file(s) will be read and directly
  # put after the %pre or %preun section
  foreach(RPM_SCRIPT_FILE_TYPE_ "INSTALL" "UNINSTALL")
    foreach(RPM_SCRIPT_FILE_TIME_ "PRE" "POST")
      set("CPACK_RPM_${RPM_SCRIPT_FILE_TIME_}_${RPM_SCRIPT_FILE_TYPE_}_READ_FILE"
        "${CPACK_RPM_${RPM_SCRIPT_FILE_TIME_}_${RPM_SCRIPT_FILE_TYPE_}_SCRIPT_FILE}")

      if(CPACK_RPM_PACKAGE_COMPONENT)
        cpack_rpm_variable_fallback("CPACK_RPM_${RPM_SCRIPT_FILE_TIME_}_${RPM_SCRIPT_FILE_TYPE_}_READ_FILE"
          "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_${RPM_SCRIPT_FILE_TIME_}_${RPM_SCRIPT_FILE_TYPE_}_SCRIPT_FILE"
          "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_${RPM_SCRIPT_FILE_TIME_}_${RPM_SCRIPT_FILE_TYPE_}_SCRIPT_FILE")
      endif()

      # Handle file if it has been specified
      if(CPACK_RPM_${RPM_SCRIPT_FILE_TIME_}_${RPM_SCRIPT_FILE_TYPE_}_READ_FILE)
        if(EXISTS ${CPACK_RPM_${RPM_SCRIPT_FILE_TIME_}_${RPM_SCRIPT_FILE_TYPE_}_READ_FILE})
          file(READ ${CPACK_RPM_${RPM_SCRIPT_FILE_TIME_}_${RPM_SCRIPT_FILE_TYPE_}_READ_FILE}
            "CPACK_RPM_SPEC_${RPM_SCRIPT_FILE_TIME_}${RPM_SCRIPT_FILE_TYPE_}")
        else()
          message("CPackRPM:Warning: CPACK_RPM_${RPM_SCRIPT_FILE_TIME_}_${RPM_SCRIPT_FILE_TYPE_}_SCRIPT_FILE <${CPACK_RPM_${RPM_SCRIPT_FILE_TIME_}_${RPM_SCRIPT_FILE_TYPE_}_READ_FILE}> does not exists - ignoring")
        endif()
      else()
        # reset SPEC var value if no file has been specified
        # (either globally or component-wise)
        set("CPACK_RPM_SPEC_${RPM_SCRIPT_FILE_TIME_}${RPM_SCRIPT_FILE_TYPE_}" "")
      endif()
    endforeach()
  endforeach()

  # CPACK_RPM_CHANGELOG_FILE
  # May be used to embed a changelog in the spec file.
  # The refered file will be read and directly put after the %changelog section
  if(CPACK_RPM_CHANGELOG_FILE)
    if(EXISTS ${CPACK_RPM_CHANGELOG_FILE})
      file(READ ${CPACK_RPM_CHANGELOG_FILE} CPACK_RPM_SPEC_CHANGELOG)
    else()
      message(SEND_ERROR "CPackRPM:Warning: CPACK_RPM_CHANGELOG_FILE <${CPACK_RPM_CHANGELOG_FILE}> does not exists - ignoring")
    endif()
  else()
    set(CPACK_RPM_SPEC_CHANGELOG "* Sun Jul 4 2010 Eric Noulard <eric.noulard@gmail.com> - ${CPACK_RPM_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}\n  Generated by CPack RPM (no Changelog file were provided)")
  endif()

  # CPACK_RPM_SPEC_MORE_DEFINE
  # This is a generated spec rpm file spaceholder
  if(CPACK_RPM_SPEC_MORE_DEFINE)
    if(CPACK_RPM_PACKAGE_DEBUG)
      message("CPackRPM:Debug: User defined more define spec line specified:\n ${CPACK_RPM_SPEC_MORE_DEFINE}")
    endif()
  endif()

  # Now we may create the RPM build tree structure
  set(CPACK_RPM_ROOTDIR "${CPACK_TOPLEVEL_DIRECTORY}")
  message(STATUS "CPackRPM:Debug: Using CPACK_RPM_ROOTDIR=${CPACK_RPM_ROOTDIR}")
  # Prepare RPM build tree
  file(MAKE_DIRECTORY ${CPACK_RPM_ROOTDIR})
  file(MAKE_DIRECTORY ${CPACK_RPM_ROOTDIR}/tmp)
  file(MAKE_DIRECTORY ${CPACK_RPM_ROOTDIR}/BUILD)
  file(MAKE_DIRECTORY ${CPACK_RPM_ROOTDIR}/RPMS)
  file(MAKE_DIRECTORY ${CPACK_RPM_ROOTDIR}/SOURCES)
  file(MAKE_DIRECTORY ${CPACK_RPM_ROOTDIR}/SPECS)
  file(MAKE_DIRECTORY ${CPACK_RPM_ROOTDIR}/SRPMS)

  # it seems rpmbuild can't handle spaces in the path
  # neither escaping (as below) nor putting quotes around the path seem to help
  #string(REGEX REPLACE " " "\\\\ " CPACK_RPM_DIRECTORY "${CPACK_TOPLEVEL_DIRECTORY}")
  set(CPACK_RPM_DIRECTORY "${CPACK_TOPLEVEL_DIRECTORY}")

  cpack_rpm_prepare_content_list()

  # In component case, put CPACK_ABSOLUTE_DESTINATION_FILES_<COMPONENT>
  #                   into CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL
  #         otherwise, put CPACK_ABSOLUTE_DESTINATION_FILES
  # This must be done BEFORE the CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL handling
  if(CPACK_RPM_PACKAGE_COMPONENT)
    if(CPACK_ABSOLUTE_DESTINATION_FILES)
      cpack_rpm_variable_fallback("COMPONENT_FILES_TAG"
        "CPACK_ABSOLUTE_DESTINATION_FILES_${CPACK_RPM_PACKAGE_COMPONENT}"
        "CPACK_ABSOLUTE_DESTINATION_FILES_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}")
      set(CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL "${${COMPONENT_FILES_TAG}}")
      if(CPACK_RPM_PACKAGE_DEBUG)
        message("CPackRPM:Debug: Handling Absolute Destination Files: <${CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL}>")
        message("CPackRPM:Debug: in component = ${CPACK_RPM_PACKAGE_COMPONENT}")
      endif()
    endif()
  else()
    if(CPACK_ABSOLUTE_DESTINATION_FILES)
      set(CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL "${CPACK_ABSOLUTE_DESTINATION_FILES}")
    endif()
  endif()

  # In component case, set CPACK_RPM_USER_FILELIST_INTERNAL with CPACK_RPM_<COMPONENT>_USER_FILELIST.
  set(CPACK_RPM_USER_FILELIST_INTERNAL "")
  if(CPACK_RPM_PACKAGE_COMPONENT)
    cpack_rpm_variable_fallback("CPACK_RPM_USER_FILELIST_INTERNAL"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_USER_FILELIST"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_USER_FILELIST")

    if(CPACK_RPM_PACKAGE_DEBUG AND CPACK_RPM_USER_FILELIST_INTERNAL)
      message("CPackRPM:Debug: Handling User Filelist: <${CPACK_RPM_USER_FILELIST_INTERNAL}>")
      message("CPackRPM:Debug: in component = ${CPACK_RPM_PACKAGE_COMPONENT}")
    endif()
  elseif(CPACK_RPM_USER_FILELIST)
    set(CPACK_RPM_USER_FILELIST_INTERNAL "${CPACK_RPM_USER_FILELIST}")
  endif()

  # Handle user specified file line list in CPACK_RPM_USER_FILELIST_INTERNAL
  # Remove those files from CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL
  #                      or CPACK_RPM_INSTALL_FILES,
  # hence it must be done before these auto-generated lists are processed.
  if(CPACK_RPM_USER_FILELIST_INTERNAL)
    if(CPACK_RPM_PACKAGE_DEBUG)
      message("CPackRPM:Debug: Handling User Filelist: <${CPACK_RPM_USER_FILELIST_INTERNAL}>")
    endif()

    # Create CMake list from CPACK_RPM_INSTALL_FILES
    string(STRIP "${CPACK_RPM_INSTALL_FILES}" CPACK_RPM_INSTALL_FILES_LIST)
    string(REPLACE "\n" ";" CPACK_RPM_INSTALL_FILES_LIST
                            "${CPACK_RPM_INSTALL_FILES_LIST}")
    string(REPLACE "\"" "" CPACK_RPM_INSTALL_FILES_LIST
                            "${CPACK_RPM_INSTALL_FILES_LIST}")

    set(CPACK_RPM_USER_INSTALL_FILES "")
    foreach(F IN LISTS CPACK_RPM_USER_FILELIST_INTERNAL)
      string(REGEX REPLACE "%[A-Za-z]+(\\([^()]*\\))? " "" F_PATH ${F})
      string(REGEX MATCH "(%[A-Za-z]+(\\([^()]*\\))? )*" F_PREFIX ${F})
      string(STRIP ${F_PREFIX} F_PREFIX)

      if(CPACK_RPM_PACKAGE_DEBUG)
        message("CPackRPM:Debug: F_PREFIX=<${F_PREFIX}>, F_PATH=<${F_PATH}>")
      endif()
      if(F_PREFIX)
        string(APPEND F_PREFIX " ")
      endif()
      # Rebuild the user list file
      string(APPEND CPACK_RPM_USER_INSTALL_FILES "${F_PREFIX}\"${F_PATH}\"\n")

      # Remove from CPACK_RPM_INSTALL_FILES and CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL
      list(REMOVE_ITEM CPACK_RPM_INSTALL_FILES_LIST ${F_PATH})
      # ABSOLUTE destination files list may not exists at all
      if (CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL)
        list(REMOVE_ITEM CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL ${F_PATH})
      endif()
    endforeach()

    # Rebuild CPACK_RPM_INSTALL_FILES
    set(CPACK_RPM_INSTALL_FILES "")
    foreach(F IN LISTS CPACK_RPM_INSTALL_FILES_LIST)
      string(APPEND CPACK_RPM_INSTALL_FILES "\"${F}\"\n")
    endforeach()
  else()
    set(CPACK_RPM_USER_INSTALL_FILES "")
  endif()

  if (CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL)
    if(CPACK_RPM_PACKAGE_DEBUG)
      message("CPackRPM:Debug: Handling Absolute Destination Files: ${CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL}")
    endif()
    # Remove trailing space
    string(STRIP "${CPACK_RPM_INSTALL_FILES}" CPACK_RPM_INSTALL_FILES_LIST)
    # Transform endline separated - string into CMake List
    string(REPLACE "\n" ";" CPACK_RPM_INSTALL_FILES_LIST "${CPACK_RPM_INSTALL_FILES_LIST}")
    # Remove unecessary quotes
    string(REPLACE "\"" "" CPACK_RPM_INSTALL_FILES_LIST "${CPACK_RPM_INSTALL_FILES_LIST}")
    # Remove ABSOLUTE install file from INSTALL FILE LIST
    list(REMOVE_ITEM CPACK_RPM_INSTALL_FILES_LIST ${CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL})
    # Rebuild INSTALL_FILES
    set(CPACK_RPM_INSTALL_FILES "")
    foreach(F IN LISTS CPACK_RPM_INSTALL_FILES_LIST)
      string(APPEND CPACK_RPM_INSTALL_FILES "\"${F}\"\n")
    endforeach()
    # Build ABSOLUTE_INSTALL_FILES
    set(CPACK_RPM_ABSOLUTE_INSTALL_FILES "")
    foreach(F IN LISTS CPACK_ABSOLUTE_DESTINATION_FILES_INTERNAL)
      string(APPEND CPACK_RPM_ABSOLUTE_INSTALL_FILES "%config \"${F}\"\n")
    endforeach()
    if(CPACK_RPM_PACKAGE_DEBUG)
      message("CPackRPM:Debug: CPACK_RPM_ABSOLUTE_INSTALL_FILES=${CPACK_RPM_ABSOLUTE_INSTALL_FILES}")
      message("CPackRPM:Debug: CPACK_RPM_INSTALL_FILES=${CPACK_RPM_INSTALL_FILES}")
    endif()
  else()
    # reset vars in order to avoid leakage of value(s) from one component to another
    set(CPACK_RPM_ABSOLUTE_INSTALL_FILES "")
  endif()

  cpack_rpm_variable_fallback("CPACK_RPM_DEBUGINFO_PACKAGE"
    "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_DEBUGINFO_PACKAGE"
    "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_DEBUGINFO_PACKAGE"
    "CPACK_RPM_DEBUGINFO_PACKAGE")
  if(CPACK_RPM_DEBUGINFO_PACKAGE OR (CPACK_RPM_DEBUGINFO_SINGLE_PACKAGE AND NOT GENERATE_SPEC_PARTS))
    cpack_rpm_variable_fallback("CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_BUILD_SOURCE_DIRS_PREFIX"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_BUILD_SOURCE_DIRS_PREFIX"
      "CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX")
    if(NOT CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX)
      set(CPACK_RPM_BUILD_SOURCE_DIRS_PREFIX "/usr/src/debug/${CPACK_PACKAGE_FILE_NAME}${CPACK_RPM_PACKAGE_COMPONENT_PART_PATH}")
    endif()

    if(CPACK_RPM_DEBUGINFO_SINGLE_PACKAGE AND GENERATE_SPEC_PARTS)
      file(WRITE "${CPACK_RPM_ROOTDIR}/SPECS/${CPACK_RPM_PACKAGE_COMPONENT}.files"
        "${CPACK_RPM_INSTALL_FILES}")
    else()
      if(CPACK_RPM_DEBUGINFO_SINGLE_PACKAGE AND CPACK_RPM_PACKAGE_COMPONENT)
        # this part is only required by components packaging - with monolithic
        # packages we can be certain that there are no other components present
        # so CPACK_RPM_DEBUGINFO_SINGLE_PACKAGE is a noop
        if(CPACK_RPM_DEBUGINFO_PACKAGE)
          # only add current package files to debuginfo list if debuginfo
          # generation is enabled for current package
          string(STRIP "${CPACK_RPM_INSTALL_FILES}" install_files_)
          string(REPLACE "\n" ";" install_files_ "${install_files_}")
          string(REPLACE "\"" "" install_files_ "${install_files_}")
        else()
          unset(install_files_)
        endif()

        file(GLOB files_ "${CPACK_RPM_DIRECTORY}/SPECS/*.files")

        foreach(f_ IN LISTS files_)
          file(READ "${f_}" tmp_)
          string(APPEND install_files_ ";${tmp_}")
        endforeach()

        # if there were other components/groups so we need to move files from them
        # to current component otherwise those files won't be found
        file(GLOB components_ LIST_DIRECTORIES true RELATIVE
          "${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}"
          "${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}/*")
        foreach(component_ IN LISTS components_)
          string(TOUPPER "${component_}" component_dir_upper_)
          if(component_dir_upper_ STREQUAL CPACK_RPM_PACKAGE_COMPONENT_UPPER)
            # skip current component
            continue()
          endif()

          cmake_policy(PUSH)
            cmake_policy(SET CMP0009 NEW)
            file(GLOB_RECURSE files_for_move_ LIST_DIRECTORIES false RELATIVE
              "${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}/${component_}"
              "${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}/${component_}/*")
          cmake_policy(POP)

          foreach(f_ IN LISTS files_for_move_)
            get_filename_component(dir_path_ "${f_}" DIRECTORY)
            set(src_file_
              "${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}/${component_}/${f_}")

            # check that we are not overriding an existing file that doesn't
            # match the file that we want to copy
            if(EXISTS "${src_file_}" AND EXISTS "${WDIR}/${f_}")
              execute_process(
                  COMMAND ${CMAKE_COMMAND} -E compare_files "${src_file_}" "${WDIR}/${f_}"
                  RESULT_VARIABLE res_
                )
              if(res_)
                message(FATAL_ERROR "CPackRPM:Error: File on path '${WDIR}/${f_}'"
                  " already exists but is a different than the one in component"
                  " '${component_}'! Packages will not be generated.")
              endif()
            endif()

            file(MAKE_DIRECTORY "${WDIR}/${dir_path_}")
            file(RENAME "${src_file_}"
              "${WDIR}/${f_}")
          endforeach()
        endforeach()

        cpack_rpm_debugsymbol_check("${install_files_}" "${WDIR}")
      else()
        string(STRIP "${CPACK_RPM_INSTALL_FILES}" install_files_)
        string(REPLACE "\n" ";" install_files_ "${install_files_}")
        string(REPLACE "\"" "" install_files_ "${install_files_}")

        cpack_rpm_debugsymbol_check("${install_files_}" "${WDIR}")
      endif()

      if(TMP_DEBUGINFO_ADDITIONAL_SOURCES)
        set(TMP_RPM_DEBUGINFO "
# Modified version of %%debug_package macro
# defined in /usr/lib/rpm/macros as that one
# can't handle injection of extra source files.
%ifnarch noarch
%global __debug_package 1
%package debuginfo
Summary: Debug information for package %{name}
Group: Development/Debug
AutoReqProv: 0
%description debuginfo
This package provides debug information for package %{name}.
Debug information is useful when developing applications that use this
package or when debugging this package.
%files debuginfo -f debugfiles.list
%defattr(-,root,root)
${TMP_DEBUGINFO_ADDITIONAL_SOURCES}
%endif
")
      elseif(CPACK_RPM_DEBUGINFO_SINGLE_PACKAGE)
        message(AUTHOR_WARNING "CPackRPM:Warning: debuginfo package was requested"
          " but will not be generated as no source files were found!")
      else()
        message(AUTHOR_WARNING "CPackRPM:Warning: debuginfo package was requested"
          " but will not be generated as no source files were found! Component: '"
          "${CPACK_RPM_PACKAGE_COMPONENT}'.")
      endif()
    endif()
  endif()

  # Prepare install files
  cpack_rpm_prepare_install_files(
      "${CPACK_RPM_INSTALL_FILES}"
      "${WDIR}"
      "${RPM_USED_PACKAGE_PREFIXES}"
      "${CPACK_RPM_PACKAGE_RELOCATABLE}"
    )

  # set default user and group
  foreach(_PERM_TYPE "USER" "GROUP")
    if(CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_DEFAULT_${_PERM_TYPE})
      set(TMP_DEFAULT_${_PERM_TYPE} "${CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_DEFAULT_${_PERM_TYPE}}")
    elseif(CPACK_RPM_DEFAULT_${_PERM_TYPE})
      set(TMP_DEFAULT_${_PERM_TYPE} "${CPACK_RPM_DEFAULT_${_PERM_TYPE}}")
    else()
      set(TMP_DEFAULT_${_PERM_TYPE} "root")
    endif()
  endforeach()

  # set default file and dir permissions
  foreach(_PERM_TYPE "FILE" "DIR")
    if(CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_DEFAULT_${_PERM_TYPE}_PERMISSIONS)
      get_unix_permissions_octal_notation("CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_DEFAULT_${_PERM_TYPE}_PERMISSIONS" "TMP_DEFAULT_${_PERM_TYPE}_PERMISSIONS")
      set(_PERMISSIONS_VAR "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_DEFAULT_${_PERM_TYPE}_PERMISSIONS")
    elseif(CPACK_RPM_DEFAULT_${_PERM_TYPE}_PERMISSIONS)
      get_unix_permissions_octal_notation("CPACK_RPM_DEFAULT_${_PERM_TYPE}_PERMISSIONS" "TMP_DEFAULT_${_PERM_TYPE}_PERMISSIONS")
      set(_PERMISSIONS_VAR "CPACK_RPM_DEFAULT_${_PERM_TYPE}_PERMISSIONS")
    else()
      set(TMP_DEFAULT_${_PERM_TYPE}_PERMISSIONS "-")
    endif()
  endforeach()

  # The name of the final spec file to be used by rpmbuild
  set(CPACK_RPM_BINARY_SPECFILE "${CPACK_RPM_ROOTDIR}/SPECS/${CPACK_RPM_PACKAGE_NAME}.spec")

  # Print out some debug information if we were asked for that
  if(CPACK_RPM_PACKAGE_DEBUG)
     message("CPackRPM:Debug: CPACK_TOPLEVEL_DIRECTORY          = ${CPACK_TOPLEVEL_DIRECTORY}")
     message("CPackRPM:Debug: CPACK_TOPLEVEL_TAG                = ${CPACK_TOPLEVEL_TAG}")
     message("CPackRPM:Debug: CPACK_TEMPORARY_DIRECTORY         = ${CPACK_TEMPORARY_DIRECTORY}")
     message("CPackRPM:Debug: CPACK_OUTPUT_FILE_NAME            = ${CPACK_OUTPUT_FILE_NAME}")
     message("CPackRPM:Debug: CPACK_OUTPUT_FILE_PATH            = ${CPACK_OUTPUT_FILE_PATH}")
     message("CPackRPM:Debug: CPACK_PACKAGE_FILE_NAME           = ${CPACK_PACKAGE_FILE_NAME}")
     message("CPackRPM:Debug: CPACK_RPM_BINARY_SPECFILE         = ${CPACK_RPM_BINARY_SPECFILE}")
     message("CPackRPM:Debug: CPACK_PACKAGE_INSTALL_DIRECTORY   = ${CPACK_PACKAGE_INSTALL_DIRECTORY}")
     message("CPackRPM:Debug: CPACK_TEMPORARY_PACKAGE_FILE_NAME = ${CPACK_TEMPORARY_PACKAGE_FILE_NAME}")
  endif()

  #
  # USER generated/provided spec file handling.
  #

  # We can have a component specific spec file.
  if(CPACK_RPM_PACKAGE_COMPONENT)
    cpack_rpm_variable_fallback("CPACK_RPM_USER_BINARY_SPECFILE"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_USER_BINARY_SPECFILE"
      "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_USER_BINARY_SPECFILE")
  endif()

  cpack_rpm_variable_fallback("CPACK_RPM_FILE_NAME"
    "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT}_FILE_NAME"
    "CPACK_RPM_${CPACK_RPM_PACKAGE_COMPONENT_UPPER}_FILE_NAME"
    "CPACK_RPM_FILE_NAME")
  if(NOT CPACK_RPM_FILE_NAME STREQUAL "RPM-DEFAULT")
    if(CPACK_RPM_FILE_NAME)
      cmake_policy(PUSH)
        cmake_policy(SET CMP0010 NEW)
        if(NOT CPACK_RPM_FILE_NAME MATCHES ".*\\.rpm")
      cmake_policy(POP)
          message(FATAL_ERROR "'${CPACK_RPM_FILE_NAME}' is not a valid RPM package file name as it must end with '.rpm'!")
        endif()
      cmake_policy(POP)
    else()
      # old file name format for back compatibility
      string(TOUPPER "${CPACK_RPM_MAIN_COMPONENT}"
        CPACK_RPM_MAIN_COMPONENT_UPPER)

      if(CPACK_RPM_MAIN_COMPONENT_UPPER STREQUAL CPACK_RPM_PACKAGE_COMPONENT_UPPER)
        # this is the main component so ignore the component filename part
        set(CPACK_RPM_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.rpm")
      else()
        set(CPACK_RPM_FILE_NAME "${CPACK_OUTPUT_FILE_NAME}")
      endif()
    endif()
    # else example:
    #set(CPACK_RPM_FILE_NAME "${CPACK_RPM_PACKAGE_NAME}-${CPACK_RPM_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}-${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")

    if(CPACK_RPM_DEBUGINFO_SINGLE_PACKAGE AND GENERATE_SPEC_PARTS)
      string(TOLOWER "${CPACK_RPM_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.*\\.rpm" expected_filename_)

      file(WRITE "${CPACK_RPM_ROOTDIR}/SPECS/${CPACK_RPM_PACKAGE_COMPONENT}.rpm_name"
        "${expected_filename_};${CPACK_RPM_FILE_NAME}")
    elseif(NOT CPACK_RPM_DEBUGINFO_PACKAGE)
      set(FILE_NAME_DEFINE "%define _rpmfilename ${CPACK_RPM_FILE_NAME}")
    endif()
  endif()

  if(CPACK_RPM_PACKAGE_SOURCES) # source rpm
    set(archive_name_ "${CPACK_RPM_PACKAGE_NAME}-${CPACK_RPM_PACKAGE_VERSION}")

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar "cfvz" "${CPACK_RPM_DIRECTORY}/SOURCES/${archive_name_}.tar.gz" "${CPACK_PACKAGE_FILE_NAME}"
        WORKING_DIRECTORY ${CPACK_RPM_DIRECTORY}
      )
    set(TMP_RPM_SOURCE "Source: ${archive_name_}.tar.gz")

    if(CPACK_RPM_BUILDREQUIRES)
      set(TMP_RPM_BUILD_REQUIRES "BuildRequires: ${CPACK_RPM_BUILDREQUIRES}")
    endif()

    # Disable debuginfo packages - srpm generates invalid packages due to
    # releasing controll to cpack to generate binary packages.
    # Note however that this doesn't prevent cpack to generate debuginfo
    # packages when run from srpm with --rebuild.
    set(TMP_RPM_DISABLE_DEBUGINFO "%define debug_package %{nil}")

    if(NOT CPACK_RPM_SOURCE_PKG_PACKAGING_INSTALL_PREFIX)
      set(CPACK_RPM_SOURCE_PKG_PACKAGING_INSTALL_PREFIX "/")
    endif()

    set(TMP_RPM_BUILD
      "
%build
mkdir cpack_rpm_build_dir
cd cpack_rpm_build_dir
cmake ${CPACK_RPM_SOURCE_PKG_BUILD_PARAMS} -DCPACK_PACKAGING_INSTALL_PREFIX=${CPACK_RPM_SOURCE_PKG_PACKAGING_INSTALL_PREFIX} ../${CPACK_PACKAGE_FILE_NAME}
make %{?_smp_mflags}" # %{?_smp_mflags} -> -j option
      )
    set(TMP_RPM_INSTALL
      "
cd cpack_rpm_build_dir
cpack -G RPM
mv *.rpm %_rpmdir"
      )
    set(TMP_RPM_PREP "%setup -c")

    set(RPMBUILD_FLAGS "-bs")

     file(WRITE ${CPACK_RPM_BINARY_SPECFILE}.in
      "# -*- rpm-spec -*-
BuildRoot:      %_topdir/\@CPACK_PACKAGE_FILE_NAME\@
Summary:        \@CPACK_RPM_PACKAGE_SUMMARY\@
Name:           \@CPACK_RPM_PACKAGE_NAME\@
Version:        \@CPACK_RPM_PACKAGE_VERSION\@
Release:        \@CPACK_RPM_PACKAGE_RELEASE\@
License:        \@CPACK_RPM_PACKAGE_LICENSE\@
Group:          \@CPACK_RPM_PACKAGE_GROUP\@
Vendor:         \@CPACK_RPM_PACKAGE_VENDOR\@

\@TMP_RPM_SOURCE\@
\@TMP_RPM_BUILD_REQUIRES\@
\@TMP_RPM_BUILDARCH\@
\@TMP_RPM_PREFIXES\@

\@TMP_RPM_DISABLE_DEBUGINFO\@

%define _rpmdir %_topdir/RPMS
%define _srcrpmdir %_topdir/SRPMS
\@FILE_NAME_DEFINE\@
%define _unpackaged_files_terminate_build 0
\@TMP_RPM_SPEC_INSTALL_POST\@
\@CPACK_RPM_SPEC_MORE_DEFINE\@
\@CPACK_RPM_COMPRESSION_TYPE_TMP\@

%description
\@CPACK_RPM_PACKAGE_DESCRIPTION\@

# This is a shortcutted spec file generated by CMake RPM generator
# we skip _install step because CPack does that for us.
# We do only save CPack installed tree in _prepr
# and then restore it in build.
%prep
\@TMP_RPM_PREP\@

\@TMP_RPM_BUILD\@

#p build

%install
\@TMP_RPM_INSTALL\@

%clean

%changelog
\@CPACK_RPM_SPEC_CHANGELOG\@
"
    )

  elseif(GENERATE_SPEC_PARTS) # binary rpm with single debuginfo package
    file(WRITE ${CPACK_RPM_BINARY_SPECFILE}.in
        "# -*- rpm-spec -*-
%package -n \@CPACK_RPM_PACKAGE_NAME\@
Summary:        \@CPACK_RPM_PACKAGE_SUMMARY\@
Version:        \@CPACK_RPM_PACKAGE_VERSION\@
Release:        \@CPACK_RPM_PACKAGE_RELEASE\@
License:        \@CPACK_RPM_PACKAGE_LICENSE\@
Group:          \@CPACK_RPM_PACKAGE_GROUP\@
Vendor:         \@CPACK_RPM_PACKAGE_VENDOR\@

\@TMP_RPM_URL\@
\@TMP_RPM_REQUIRES\@
\@TMP_RPM_REQUIRES_PRE\@
\@TMP_RPM_REQUIRES_POST\@
\@TMP_RPM_REQUIRES_PREUN\@
\@TMP_RPM_REQUIRES_POSTUN\@
\@TMP_RPM_PROVIDES\@
\@TMP_RPM_OBSOLETES\@
\@TMP_RPM_CONFLICTS\@
\@TMP_RPM_SUGGESTS\@
\@TMP_RPM_AUTOPROV\@
\@TMP_RPM_AUTOREQ\@
\@TMP_RPM_AUTOREQPROV\@
\@TMP_RPM_BUILDARCH\@
\@TMP_RPM_PREFIXES\@

%description -n \@CPACK_RPM_PACKAGE_NAME\@
\@CPACK_RPM_PACKAGE_DESCRIPTION\@

%files -n \@CPACK_RPM_PACKAGE_NAME\@
%defattr(\@TMP_DEFAULT_FILE_PERMISSIONS\@,\@TMP_DEFAULT_USER\@,\@TMP_DEFAULT_GROUP\@,\@TMP_DEFAULT_DIR_PERMISSIONS\@)
\@CPACK_RPM_INSTALL_FILES\@
\@CPACK_RPM_ABSOLUTE_INSTALL_FILES\@
\@CPACK_RPM_USER_INSTALL_FILES\@
"
    )

  else()  # binary rpm
    if(CPACK_RPM_DEBUGINFO_SINGLE_PACKAGE)
      # find generated spec file and take its name
      file(GLOB spec_files_ "${CPACK_RPM_DIRECTORY}/SPECS/*.spec")

      foreach(f_ IN LISTS spec_files_)
        file(READ "${f_}" tmp_)
        string(APPEND TMP_OTHER_COMPONENTS "\n${tmp_}\n")
      endforeach()
    endif()

    # We should generate a USER spec file template:
    #  - either because the user asked for it : CPACK_RPM_GENERATE_USER_BINARY_SPECFILE_TEMPLATE
    #  - or the user did not provide one : NOT CPACK_RPM_USER_BINARY_SPECFILE
    set(RPMBUILD_FLAGS "-bb")
    if(CPACK_RPM_GENERATE_USER_BINARY_SPECFILE_TEMPLATE OR NOT CPACK_RPM_USER_BINARY_SPECFILE)

      file(WRITE ${CPACK_RPM_BINARY_SPECFILE}.in
        "# -*- rpm-spec -*-
BuildRoot:      %_topdir/\@CPACK_PACKAGE_FILE_NAME\@\@CPACK_RPM_PACKAGE_COMPONENT_PART_PATH\@
Summary:        \@CPACK_RPM_PACKAGE_SUMMARY\@
Name:           \@CPACK_RPM_PACKAGE_NAME\@
Version:        \@CPACK_RPM_PACKAGE_VERSION\@
Release:        \@CPACK_RPM_PACKAGE_RELEASE\@
License:        \@CPACK_RPM_PACKAGE_LICENSE\@
Group:          \@CPACK_RPM_PACKAGE_GROUP\@
Vendor:         \@CPACK_RPM_PACKAGE_VENDOR\@

\@TMP_RPM_URL\@
\@TMP_RPM_REQUIRES\@
\@TMP_RPM_REQUIRES_PRE\@
\@TMP_RPM_REQUIRES_POST\@
\@TMP_RPM_REQUIRES_PREUN\@
\@TMP_RPM_REQUIRES_POSTUN\@
\@TMP_RPM_PROVIDES\@
\@TMP_RPM_OBSOLETES\@
\@TMP_RPM_CONFLICTS\@
\@TMP_RPM_SUGGESTS\@
\@TMP_RPM_AUTOPROV\@
\@TMP_RPM_AUTOREQ\@
\@TMP_RPM_AUTOREQPROV\@
\@TMP_RPM_BUILDARCH\@
\@TMP_RPM_PREFIXES\@

\@TMP_RPM_DEBUGINFO\@

%define _rpmdir %_topdir/RPMS
%define _srcrpmdir %_topdir/SRPMS
\@FILE_NAME_DEFINE\@
%define _unpackaged_files_terminate_build 0
\@TMP_RPM_SPEC_INSTALL_POST\@
\@CPACK_RPM_SPEC_MORE_DEFINE\@
\@CPACK_RPM_COMPRESSION_TYPE_TMP\@

%description
\@CPACK_RPM_PACKAGE_DESCRIPTION\@

# This is a shortcutted spec file generated by CMake RPM generator
# we skip _install step because CPack does that for us.
# We do only save CPack installed tree in _prepr
# and then restore it in build.
%prep
mv $RPM_BUILD_ROOT %_topdir/tmpBBroot

%install
if [ -e $RPM_BUILD_ROOT ];
then
  rm -rf $RPM_BUILD_ROOT
fi
mv %_topdir/tmpBBroot $RPM_BUILD_ROOT

\@TMP_RPM_DEBUGINFO_INSTALL\@

%clean

%post
\@RPM_SYMLINK_POSTINSTALL\@
\@CPACK_RPM_SPEC_POSTINSTALL\@

%postun
\@CPACK_RPM_SPEC_POSTUNINSTALL\@

%pre
\@CPACK_RPM_SPEC_PREINSTALL\@

%preun
\@CPACK_RPM_SPEC_PREUNINSTALL\@

%files
%defattr(\@TMP_DEFAULT_FILE_PERMISSIONS\@,\@TMP_DEFAULT_USER\@,\@TMP_DEFAULT_GROUP\@,\@TMP_DEFAULT_DIR_PERMISSIONS\@)
\@CPACK_RPM_INSTALL_FILES\@
\@CPACK_RPM_ABSOLUTE_INSTALL_FILES\@
\@CPACK_RPM_USER_INSTALL_FILES\@

%changelog
\@CPACK_RPM_SPEC_CHANGELOG\@

\@TMP_OTHER_COMPONENTS\@
"
      )
    endif()

    # Stop here if we were asked to only generate a template USER spec file
    # The generated file may then be used as a template by user who wants
    # to customize their own spec file.
    if(CPACK_RPM_GENERATE_USER_BINARY_SPECFILE_TEMPLATE)
      message(FATAL_ERROR "CPackRPM: STOP here Generated USER binary spec file template is: ${CPACK_RPM_BINARY_SPECFILE}.in")
    endif()
  endif()

  # After that we may either use a user provided spec file
  # or generate one using appropriate variables value.
  if(CPACK_RPM_USER_BINARY_SPECFILE)
    # User may have specified SPECFILE just use it
    message("CPackRPM: Will use USER specified spec file: ${CPACK_RPM_USER_BINARY_SPECFILE}")
    # The user provided file is processed for @var replacement
    configure_file(${CPACK_RPM_USER_BINARY_SPECFILE} ${CPACK_RPM_BINARY_SPECFILE} @ONLY)
  else()
    # No User specified spec file, will use the generated spec file
    message("CPackRPM: Will use GENERATED spec file: ${CPACK_RPM_BINARY_SPECFILE}")
    # Note the just created file is processed for @var replacement
    configure_file(${CPACK_RPM_BINARY_SPECFILE}.in ${CPACK_RPM_BINARY_SPECFILE} @ONLY)
  endif()

  if(NOT GENERATE_SPEC_PARTS) # generate package
    if(RPMBUILD_EXECUTABLE)
      # Now call rpmbuild using the SPECFILE
      execute_process(
        COMMAND "${RPMBUILD_EXECUTABLE}" ${RPMBUILD_FLAGS}
                --define "_topdir ${CPACK_RPM_DIRECTORY}"
                --buildroot "%_topdir/${CPACK_PACKAGE_FILE_NAME}${CPACK_RPM_PACKAGE_COMPONENT_PART_PATH}"
                --target "${CPACK_RPM_PACKAGE_ARCHITECTURE}"
                "${CPACK_RPM_BINARY_SPECFILE}"
        WORKING_DIRECTORY "${CPACK_TOPLEVEL_DIRECTORY}/${CPACK_PACKAGE_FILE_NAME}${CPACK_RPM_PACKAGE_COMPONENT_PART_PATH}"
        RESULT_VARIABLE CPACK_RPMBUILD_EXEC_RESULT
        ERROR_FILE "${CPACK_TOPLEVEL_DIRECTORY}/rpmbuild${CPACK_RPM_PACKAGE_NAME}.err"
        OUTPUT_FILE "${CPACK_TOPLEVEL_DIRECTORY}/rpmbuild${CPACK_RPM_PACKAGE_NAME}.out")
      if(CPACK_RPM_PACKAGE_DEBUG OR CPACK_RPMBUILD_EXEC_RESULT)
        file(READ ${CPACK_TOPLEVEL_DIRECTORY}/rpmbuild${CPACK_RPM_PACKAGE_NAME}.err RPMBUILDERR)
        file(READ ${CPACK_TOPLEVEL_DIRECTORY}/rpmbuild${CPACK_RPM_PACKAGE_NAME}.out RPMBUILDOUT)
        message("CPackRPM:Debug: You may consult rpmbuild logs in: ")
        message("CPackRPM:Debug:    - ${CPACK_TOPLEVEL_DIRECTORY}/rpmbuild${CPACK_RPM_PACKAGE_NAME}.err")
        message("CPackRPM:Debug: *** ${RPMBUILDERR} ***")
        message("CPackRPM:Debug:    - ${CPACK_TOPLEVEL_DIRECTORY}/rpmbuild${CPACK_RPM_PACKAGE_NAME}.out")
        message("CPackRPM:Debug: *** ${RPMBUILDOUT} ***")
      endif()
    else()
      if(ALIEN_EXECUTABLE)
        message(FATAL_ERROR "RPM packaging through alien not done (yet)")
      endif()
    endif()

    # find generated rpm files and take their names
    cmake_policy(PUSH)
      # Tell file(GLOB_RECURSE) not to follow directory symlinks
      # even if the project does not set this policy to NEW.
      cmake_policy(SET CMP0009 NEW)
      file(GLOB_RECURSE GENERATED_FILES "${CPACK_RPM_DIRECTORY}/RPMS/*.rpm"
        "${CPACK_RPM_DIRECTORY}/SRPMS/*.rpm")
    cmake_policy(POP)

    if(NOT GENERATED_FILES)
      message(FATAL_ERROR "RPM package was not generated! ${CPACK_RPM_DIRECTORY}")
    endif()

    unset(expected_filenames_)
    unset(filenames_)
    if(CPACK_RPM_DEBUGINFO_PACKAGE AND NOT CPACK_RPM_FILE_NAME STREQUAL "RPM-DEFAULT")
      string(TOLOWER "${CPACK_RPM_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.*\\.rpm" efn_)
      list(APPEND expected_filenames_ "${efn_}")
      list(APPEND filenames_ "${CPACK_RPM_FILE_NAME}")
    endif()

    # check if other files have to be renamed
    file(GLOB rename_files_ "${CPACK_RPM_DIRECTORY}/SPECS/*.rpm_name")
    if(rename_files_)
      foreach(f_ IN LISTS rename_files_)
        file(READ "${f_}" tmp_)
        list(GET tmp_ 0 efn_)
        list(APPEND expected_filenames_ "${efn_}")
        list(GET tmp_ 1 fn_)
        list(APPEND filenames_ "${fn_}")
      endforeach()
    endif()

    if(expected_filenames_)
      foreach(F IN LISTS GENERATED_FILES)
        unset(matched_)
        foreach(expected_ IN LISTS expected_filenames_)
          if(F MATCHES ".*/${expected_}")
            list(FIND expected_filenames_ "${expected_}" idx_)
            list(GET filenames_ ${idx_} filename_)
            get_filename_component(FILE_PATH "${F}" DIRECTORY)
            file(RENAME "${F}" "${FILE_PATH}/${filename_}")
            list(APPEND new_files_list_ "${FILE_PATH}/${filename_}")
            set(matched_ "YES")

            break()
          endif()
        endforeach()

        if(NOT matched_)
          list(APPEND new_files_list_ "${F}")
        endif()
      endforeach()

      set(GENERATED_FILES "${new_files_list_}")
    endif()
  endif()

  set(GEN_CPACK_OUTPUT_FILES "${GENERATED_FILES}" PARENT_SCOPE)

  if(CPACK_RPM_PACKAGE_DEBUG)
     message("CPackRPM:Debug: GEN_CPACK_OUTPUT_FILES = ${GENERATED_FILES}")
  endif()
endfunction()

cpack_rpm_generate_package()
