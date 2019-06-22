LINK_DEPENDS
------------

Additional files on which a target binary depends for linking.

Specifies a semicolon-separated list of full-paths to files on which
the link rule for this target depends.  The target binary will be
linked if any of the named files is newer than it.

This property is ignored by non-Makefile generators.  It is intended
to specify dependencies on "linker scripts" for custom Makefile link
rules.
