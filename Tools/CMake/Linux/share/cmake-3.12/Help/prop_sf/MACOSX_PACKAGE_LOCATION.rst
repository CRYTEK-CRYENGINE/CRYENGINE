MACOSX_PACKAGE_LOCATION
-----------------------

Place a source file inside a Application Bundle
(:prop_tgt:`MACOSX_BUNDLE`), Core Foundation Bundle (:prop_tgt:`BUNDLE`),
or Framework Bundle (:prop_tgt:`FRAMEWORK`).  It is applicable for OS X
and iOS.

Executable targets with the :prop_tgt:`MACOSX_BUNDLE` property set are
built as OS X or iOS application bundles on Apple platforms.  Shared
library targets with the :prop_tgt:`FRAMEWORK` property set are built as
OS X or iOS frameworks on Apple platforms.  Module library targets with
the :prop_tgt:`BUNDLE` property set are built as OS X ``CFBundle`` bundles
on Apple platforms.  Source files listed in the target with this property
set will be copied to a directory inside the bundle or framework content
folder specified by the property value.  For OS X Application Bundles the
content folder is ``<name>.app/Contents``.  For OS X Frameworks the
content folder is ``<name>.framework/Versions/<version>``.  For OS X
CFBundles the content folder is ``<name>.bundle/Contents`` (unless the
extension is changed).  See the :prop_tgt:`PUBLIC_HEADER`,
:prop_tgt:`PRIVATE_HEADER`, and :prop_tgt:`RESOURCE` target properties for
specifying files meant for ``Headers``, ``PrivateHeaders``, or
``Resources`` directories.

If the specified location is equal to ``Resources``, the resulting location
will be the same as if the :prop_tgt:`RESOURCE` property had been used. If
the specified location is a sub-folder of ``Resources``, it will be placed
into the respective sub-folder. Note: For iOS Apple uses a flat bundle layout
where no ``Resources`` folder exist. Therefore CMake strips the ``Resources``
folder name from the specified location.
