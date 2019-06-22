CMAKE_EXPORT_COMPILE_COMMANDS
-----------------------------

Enable/Disable output of compile commands during generation.

If enabled, generates a ``compile_commands.json`` file containing the exact
compiler calls for all translation units of the project in machine-readable
form.  The format of the JSON file looks like:

.. code-block:: javascript

  [
    {
      "directory": "/home/user/development/project",
      "command": "/usr/bin/c++ ... -c ../foo/foo.cc",
      "file": "../foo/foo.cc"
    },

    ...

    {
      "directory": "/home/user/development/project",
      "command": "/usr/bin/c++ ... -c ../foo/bar.cc",
      "file": "../foo/bar.cc"
    }
  ]

.. note::
  This option is implemented only by :ref:`Makefile Generators`
  and the :generator:`Ninja`.  It is ignored on other generators.
