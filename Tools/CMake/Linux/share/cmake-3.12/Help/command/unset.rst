unset
-----

Unset a variable, cache variable, or environment variable.

::

  unset(<variable> [CACHE | PARENT_SCOPE])

Removes the specified variable causing it to become undefined.  If
``CACHE`` is present then the variable is removed from the cache instead
of the current scope.

If ``PARENT_SCOPE`` is present then the variable is removed from the scope
above the current scope.  See the same option in the :command:`set` command
for further details.

``<variable>`` can be an environment variable such as:

::

  unset(ENV{LD_LIBRARY_PATH})

in which case the variable will be removed from the current
environment.
