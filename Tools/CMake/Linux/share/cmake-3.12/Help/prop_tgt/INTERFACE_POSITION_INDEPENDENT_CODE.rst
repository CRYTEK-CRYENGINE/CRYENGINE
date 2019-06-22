INTERFACE_POSITION_INDEPENDENT_CODE
-----------------------------------

Whether consumers need to create a position-independent target

The ``INTERFACE_POSITION_INDEPENDENT_CODE`` property informs consumers of
this target whether they must set their
:prop_tgt:`POSITION_INDEPENDENT_CODE` property to ``ON``.  If this
property is set to ``ON``, then the :prop_tgt:`POSITION_INDEPENDENT_CODE`
property on  all consumers will be set to ``ON``. Similarly, if this
property is set to ``OFF``, then the :prop_tgt:`POSITION_INDEPENDENT_CODE`
property on all consumers will be set to ``OFF``.  If this property is
undefined, then consumers will determine their
:prop_tgt:`POSITION_INDEPENDENT_CODE` property by other means.  Consumers
must ensure that the targets that they link to have a consistent
requirement for their ``INTERFACE_POSITION_INDEPENDENT_CODE`` property.
