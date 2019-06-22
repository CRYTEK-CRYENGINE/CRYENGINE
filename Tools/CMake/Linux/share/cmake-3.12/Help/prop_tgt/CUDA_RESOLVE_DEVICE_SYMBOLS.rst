CUDA_RESOLVE_DEVICE_SYMBOLS
---------------------------

CUDA only: Enables device linking for the specific static library target

If set this will enable device linking on this static library target. Normally
device linking is deferred until a shared library or executable is generated,
allowing for multiple static libraries to resolve device symbols at the same
time.

For instance:

.. code-block:: cmake

  set_property(TARGET mystaticlib PROPERTY CUDA_RESOLVE_DEVICE_SYMBOLS ON)
