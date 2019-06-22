COMPILE_FEATURES
----------------

Compiler features enabled for this target.

The list of features in this property are a subset of the features listed
in the :variable:`CMAKE_CXX_COMPILE_FEATURES` variable.

Contents of ``COMPILE_FEATURES`` may use "generator expressions" with the
syntax ``$<...>``.  See the :manual:`cmake-generator-expressions(7)` manual for
available expressions.  See the :manual:`cmake-compile-features(7)` manual
for information on compile features and a list of supported compilers.
