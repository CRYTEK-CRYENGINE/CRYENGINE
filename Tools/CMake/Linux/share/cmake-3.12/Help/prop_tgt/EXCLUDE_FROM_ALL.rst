EXCLUDE_FROM_ALL
----------------

Exclude the target from the all target.

A property on a target that indicates if the target is excluded from
the default build target.  If it is not, then with a Makefile for
example typing make will cause this target to be built.  The same
concept applies to the default build of other generators.  Installing
a target with EXCLUDE_FROM_ALL set to true has undefined behavior.
