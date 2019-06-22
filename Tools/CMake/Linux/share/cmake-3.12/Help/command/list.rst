list
----

List operations.

Synopsis
^^^^^^^^

.. parsed-literal::

  `Reading`_
    list(`LENGTH`_ <list> <out-var>)
    list(`GET`_ <list> <element index> [<index> ...] <out-var>)
    list(`JOIN`_ <list> <glue> <out-var>)
    list(`SUBLIST`_ <list> <begin> <length> <out-var>)

  `Search`_
    list(`FIND`_ <list> <value> <out-var>)

  `Modification`_
    list(`APPEND`_ <list> [<element>...])
    list(`FILTER`_ <list> {INCLUDE | EXCLUDE} REGEX <regex>)
    list(`INSERT`_ <list> <index> [<element>...])
    list(`REMOVE_ITEM`_ <list> <value>...)
    list(`REMOVE_AT`_ <list> <index>...)
    list(`REMOVE_DUPLICATES`_ <list>)
    list(`TRANSFORM`_ <list> <ACTION> [...])

  `Ordering`_
    list(`REVERSE`_ <list>)
    list(`SORT`_ <list>)

Introduction
^^^^^^^^^^^^

The list subcommands ``APPEND``, ``INSERT``, ``FILTER``, ``REMOVE_AT``,
``REMOVE_ITEM``, ``REMOVE_DUPLICATES``, ``REVERSE`` and ``SORT`` may create
new values for the list within the current CMake variable scope.  Similar to
the :command:`set` command, the LIST command creates new variable values in
the current scope, even if the list itself is actually defined in a parent
scope.  To propagate the results of these operations upwards, use
:command:`set` with ``PARENT_SCOPE``, :command:`set` with
``CACHE INTERNAL``, or some other means of value propagation.

.. note::

  A list in cmake is a ``;`` separated group of strings.  To create a
  list the set command can be used.  For example, ``set(var a b c d e)``
  creates a list with ``a;b;c;d;e``, and ``set(var "a b c d e")`` creates a
  string or a list with one item in it.   (Note macro arguments are not
  variables, and therefore cannot be used in LIST commands.)

.. note::

  When specifying index values, if ``<element index>`` is 0 or greater, it
  is indexed from the beginning of the list, with 0 representing the
  first list element.  If ``<element index>`` is -1 or lesser, it is indexed
  from the end of the list, with -1 representing the last list element.
  Be careful when counting with negative indices: they do not start from
  0.  -0 is equivalent to 0, the first list element.

Reading
^^^^^^^

.. _LENGTH:

::

  list(LENGTH <list> <output variable>)

Returns the list's length.

.. _GET:

::

  list(GET <list> <element index> [<element index> ...] <output variable>)

Returns the list of elements specified by indices from the list.

.. _JOIN:

::

  list(JOIN <list> <glue> <output variable>)

Returns a string joining all list's elements using the glue string.
To join multiple strings, which are not part of a list, use ``JOIN`` operator
from :command:`string` command.

.. _SUBLIST:

::

  list(SUBLIST <list> <begin> <length> <output variable>)

Returns a sublist of the given list.
If ``<length>`` is 0, an empty list will be returned.
If ``<length>`` is -1 or the list is smaller than ``<begin>+<length>`` then
the remaining elements of the list starting at ``<begin>`` will be returned.

Search
^^^^^^

.. _FIND:

::

  list(FIND <list> <value> <output variable>)

Returns the index of the element specified in the list or -1
if it wasn't found.

Modification
^^^^^^^^^^^^

.. _APPEND:

::

  list(APPEND <list> [<element> ...])

Appends elements to the list.

.. _FILTER:

::

  list(FILTER <list> <INCLUDE|EXCLUDE> REGEX <regular_expression>)

Includes or removes items from the list that match the mode's pattern.
In ``REGEX`` mode, items will be matched against the given regular expression.

For more information on regular expressions see also the
:command:`string` command.

.. _INSERT:

::

  list(INSERT <list> <element_index> <element> [<element> ...])

Inserts elements to the list to the specified location.

.. _REMOVE_ITEM:

::

  list(REMOVE_ITEM <list> <value> [<value> ...])

Removes the given items from the list.

.. _REMOVE_AT:

::

  list(REMOVE_AT <list> <index> [<index> ...])

Removes items at given indices from the list.

.. _REMOVE_DUPLICATES:

::

  list(REMOVE_DUPLICATES <list>)

Removes duplicated items in the list.

.. _TRANSFORM:

::

  list(TRANSFORM <list> <ACTION> [<SELECTOR>]
                        [OUTPUT_VARIABLE <output variable>])

Transforms the list by applying an action to all or, by specifying a
``<SELECTOR>``, to the selected elements of the list, storing result in-place
or in the specified output variable.

.. note::

   ``TRANSFORM`` sub-command does not change the number of elements of the
   list. If a ``<SELECTOR>`` is specified, only some elements will be changed,
   the other ones will remain same as before the transformation.

``<ACTION>`` specify the action to apply to the elements of list.
The actions have exactly the same semantics as sub-commands of
:command:`string` command.

The ``<ACTION>`` may be one of:

``APPEND``, ``PREPEND``: Append, prepend specified value to each element of
the list. ::

  list(TRANSFORM <list> <APPEND|PREPEND> <value> ...)

``TOUPPER``, ``TOLOWER``: Convert each element of the list to upper, lower
characters. ::

  list(TRANSFORM <list> <TOLOWER|TOUPPER> ...)

``STRIP``: Remove leading and trailing spaces from each element of the
list. ::

  list(TRANSFORM <list> STRIP ...)

``GENEX_STRIP``: Strip any
:manual:`generator expressions <cmake-generator-expressions(7)>` from each
element of the list. ::

  list(TRANSFORM <list> GENEX_STRIP ...)

``REPLACE``: Match the regular expression as many times as possible and
substitute the replacement expression for the match for each element
of the list
(Same semantic as ``REGEX REPLACE`` from :command:`string` command). ::

  list(TRANSFORM <list> REPLACE <regular_expression>
                                <replace_expression> ...)

``<SELECTOR>`` select which elements of the list will be transformed. Only one
type of selector can be specified at a time.

The ``<SELECTOR>`` may be one of:

``AT``: Specify a list of indexes. ::

  list(TRANSFORM <list> <ACTION> AT <index> [<index> ...] ...)

``FOR``: Specify a range with, optionally, an increment used to iterate over
the range. ::

  list(TRANSFORM <list> <ACTION> FOR <start> <stop> [<step>] ...)

``REGEX``: Specify a regular expression. Only elements matching the regular
expression will be transformed. ::

  list(TRANSFORM <list> <ACTION> REGEX <regular_expression> ...)


Ordering
^^^^^^^^

.. _REVERSE:

::

  list(REVERSE <list>)

Reverses the contents of the list in-place.

.. _SORT:

::

  list(SORT <list>)


Sorts the list in-place alphabetically.
