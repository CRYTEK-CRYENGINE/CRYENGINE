RESOURCE_LOCK
-------------

Specify a list of resources that are locked by this test.

If multiple tests specify the same resource lock, they are guaranteed
not to run concurrently.

See also :prop_test:`FIXTURES_REQUIRED` if the resource requires any setup or
cleanup steps.
