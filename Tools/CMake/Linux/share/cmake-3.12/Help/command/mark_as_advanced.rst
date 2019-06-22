mark_as_advanced
----------------

Mark cmake cached variables as advanced.

::

  mark_as_advanced([CLEAR|FORCE] VAR [VAR2 ...])

Mark the named cached variables as advanced.  An advanced variable
will not be displayed in any of the cmake GUIs unless the show
advanced option is on.  If ``CLEAR`` is the first argument advanced
variables are changed back to unadvanced.  If ``FORCE`` is the first
argument, then the variable is made advanced.  If neither ``FORCE`` nor
``CLEAR`` is specified, new values will be marked as advanced, but if the
variable already has an advanced/non-advanced state, it will not be
changed.

It does nothing in script mode.
