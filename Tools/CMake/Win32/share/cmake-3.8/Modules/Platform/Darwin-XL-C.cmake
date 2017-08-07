set(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "-qmkshrobj")
set(CMAKE_SHARED_MODULE_CREATE_C_FLAGS "-bundle")

# Enable shared library versioning.
set(CMAKE_SHARED_LIBRARY_SONAME_C_FLAG "-Wl,-install_name")
