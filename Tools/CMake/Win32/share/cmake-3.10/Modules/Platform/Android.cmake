include(Platform/Linux)

set(ANDROID 1)

# Android has soname, but binary names must end in ".so" so we cannot append
# a version number.  Also we cannot portably represent symlinks on the host.
set(CMAKE_PLATFORM_NO_VERSIONED_SONAME 1)

# Android reportedly ignores RPATH, and we cannot predict the install
# location anyway.
set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG "")

# Nsight Tegra Visual Studio Edition takes care of
# prefixing library names with '-l'.
if(CMAKE_VS_PLATFORM_NAME STREQUAL "Tegra-Android")
  set(CMAKE_LINK_LIBRARY_FLAG "")
endif()
