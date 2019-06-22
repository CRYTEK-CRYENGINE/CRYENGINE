GENERATOR_IS_MULTI_CONFIG
-------------------------

Read-only property that is true on multi-configuration generators.

True when using a multi-configuration generator
(such as :ref:`Visual Studio Generators` or :generator:`Xcode`).
Multi-config generators use :variable:`CMAKE_CONFIGURATION_TYPES`
as the set of configurations and ignore :variable:`CMAKE_BUILD_TYPE`.
