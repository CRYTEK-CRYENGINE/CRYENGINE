set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "-qmkshrobj")
set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS "-bundle")

# Enable shared library versioning.
set(CMAKE_SHARED_LIBRARY_SONAME_CXX_FLAG "-Wl,-install_name")
