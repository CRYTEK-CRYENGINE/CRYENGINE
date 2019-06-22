CUDA_PTX_COMPILATION
--------------------

Compile CUDA sources to ``.ptx`` files instead of ``.obj`` files
within :ref:`Object Libraries`.

For example:

.. code-block:: cmake

  add_library(myptx OBJECT a.cu b.cu)
  set_property(TARGET myptx PROPERTY CUDA_PTX_COMPILATION ON)
