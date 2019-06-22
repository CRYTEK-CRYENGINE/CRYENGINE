# Ask xcode-select where to find /Developer or fall back to ancient location.
execute_process(COMMAND xcode-select -print-path
  OUTPUT_VARIABLE _stdout
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_VARIABLE _stderr
  RESULT_VARIABLE _failed)
if(NOT _failed AND IS_DIRECTORY ${_stdout})
  set(OSX_DEVELOPER_ROOT ${_stdout})
elseif(IS_DIRECTORY "/Developer")
  set(OSX_DEVELOPER_ROOT "/Developer")
else()
  set(OSX_DEVELOPER_ROOT "")
endif()

execute_process(COMMAND sw_vers -productVersion
  OUTPUT_VARIABLE CURRENT_OSX_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE)

# Save CMAKE_OSX_ARCHITECTURES from the environment.
set(CMAKE_OSX_ARCHITECTURES "$ENV{CMAKE_OSX_ARCHITECTURES}" CACHE STRING
  "Build architectures for OSX")

# macOS, iOS, tvOS, and watchOS should lookup compilers from
# Platform/Apple-${CMAKE_CXX_COMPILER_ID}-<LANG>
set(CMAKE_EFFECTIVE_SYSTEM_NAME "Apple")

#----------------------------------------------------------------------------
# _CURRENT_OSX_VERSION - as a two-component string: 10.5, 10.6, ...
#
string(REGEX REPLACE "^([0-9]+\\.[0-9]+).*$" "\\1"
  _CURRENT_OSX_VERSION "${CURRENT_OSX_VERSION}")

#----------------------------------------------------------------------------
# CMAKE_OSX_DEPLOYMENT_TARGET

# Set cache variable - end user may change this during ccmake or cmake-gui configure.
if(_CURRENT_OSX_VERSION VERSION_GREATER 10.3)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "$ENV{MACOSX_DEPLOYMENT_TARGET}" CACHE STRING
    "Minimum OS X version to target for deployment (at runtime); newer APIs weak linked. Set to empty string for default value.")
endif()

#----------------------------------------------------------------------------
# CMAKE_OSX_SYSROOT

if(CMAKE_OSX_SYSROOT)
  # Use the existing value without further computation to choose a default.
  set(_CMAKE_OSX_SYSROOT_DEFAULT "${CMAKE_OSX_SYSROOT}")
elseif(NOT "x$ENV{SDKROOT}" STREQUAL "x" AND
        (NOT "x$ENV{SDKROOT}" MATCHES "/" OR IS_DIRECTORY "$ENV{SDKROOT}"))
  # Use the value of SDKROOT from the environment.
  set(_CMAKE_OSX_SYSROOT_DEFAULT "$ENV{SDKROOT}")
elseif("${CMAKE_GENERATOR}" MATCHES Xcode
       OR CMAKE_OSX_DEPLOYMENT_TARGET
       OR CMAKE_OSX_ARCHITECTURES MATCHES "[^;]"
       OR NOT EXISTS "/usr/include/sys/types.h")
  # Find installed SDKs in either Xcode-4.3+ or pre-4.3 SDKs directory.
  set(_CMAKE_OSX_SDKS_DIR "")
  if(OSX_DEVELOPER_ROOT)
    foreach(d Platforms/MacOSX.platform/Developer/SDKs SDKs)
      file(GLOB _CMAKE_OSX_SDKS ${OSX_DEVELOPER_ROOT}/${d}/*)
      if(_CMAKE_OSX_SDKS)
        set(_CMAKE_OSX_SDKS_DIR ${OSX_DEVELOPER_ROOT}/${d})
        break()
      endif()
    endforeach()
  endif()

  if(_CMAKE_OSX_SDKS_DIR)
    # Select SDK for current OSX version accounting for the known
    # specially named SDKs.
    set(_CMAKE_OSX_SDKS_VER_SUFFIX_10.4 "u")
    set(_CMAKE_OSX_SDKS_VER_SUFFIX_10.3 ".9")

    # find the latest SDK
    set(_CMAKE_OSX_LATEST_SDK_VERSION "0.0")
    file(GLOB _CMAKE_OSX_SDKS RELATIVE "${_CMAKE_OSX_SDKS_DIR}" "${_CMAKE_OSX_SDKS_DIR}/MacOSX*.sdk")
    foreach(_SDK ${_CMAKE_OSX_SDKS})
      if(_SDK MATCHES "MacOSX([0-9]+\\.[0-9]+)[^/]*\\.sdk" AND CMAKE_MATCH_1 VERSION_GREATER ${_CMAKE_OSX_LATEST_SDK_VERSION})
        set(_CMAKE_OSX_LATEST_SDK_VERSION "${CMAKE_MATCH_1}")
      endif()
    endforeach()

    # pick an SDK that works
    set(_CMAKE_OSX_SYSROOT_DEFAULT)
    foreach(ver ${CMAKE_OSX_DEPLOYMENT_TARGET}
                ${_CURRENT_OSX_VERSION}
                ${_CMAKE_OSX_LATEST_SDK_VERSION})
      set(_CMAKE_OSX_DEPLOYMENT_TARGET ${ver})
      set(_CMAKE_OSX_SDKS_VER ${_CMAKE_OSX_DEPLOYMENT_TARGET}${_CMAKE_OSX_SDKS_VER_SUFFIX_${_CMAKE_OSX_DEPLOYMENT_TARGET}})
      set(_CMAKE_OSX_SYSROOT_CHECK "${_CMAKE_OSX_SDKS_DIR}/MacOSX${_CMAKE_OSX_SDKS_VER}.sdk")
      if(IS_DIRECTORY "${_CMAKE_OSX_SYSROOT_CHECK}")
        set(_CMAKE_OSX_SYSROOT_DEFAULT "${_CMAKE_OSX_SYSROOT_CHECK}")
        break()
      endif()
    endforeach()

    if(NOT CMAKE_CROSSCOMPILING AND NOT CMAKE_OSX_DEPLOYMENT_TARGET AND _CURRENT_OSX_VERSION VERSION_LESS _CMAKE_OSX_DEPLOYMENT_TARGET)
      set(CMAKE_OSX_DEPLOYMENT_TARGET ${_CURRENT_OSX_VERSION} CACHE STRING
        "Minimum OS X version to target for deployment (at runtime); newer APIs weak linked. Set to empty string for default value." FORCE)
    endif()
  else()
    # Assume developer files are in root (such as Xcode 4.5 command-line tools).
    set(_CMAKE_OSX_SYSROOT_DEFAULT "")
  endif()
endif()

# Set cache variable - end user may change this during ccmake or cmake-gui configure.
# Choose the type based on the current value.
set(_CMAKE_OSX_SYSROOT_TYPE STRING)
foreach(v CMAKE_OSX_SYSROOT _CMAKE_OSX_SYSROOT_DEFAULT)
  if("x${${v}}" MATCHES "/")
    set(_CMAKE_OSX_SYSROOT_TYPE PATH)
    break()
  endif()
endforeach()
set(CMAKE_OSX_SYSROOT "${_CMAKE_OSX_SYSROOT_DEFAULT}" CACHE ${_CMAKE_OSX_SYSROOT_TYPE}
  "The product will be built against the headers and libraries located inside the indicated SDK.")

# Transform the cached value to something we can use.
set(_CMAKE_OSX_SYSROOT_PATH "")
if(CMAKE_OSX_SYSROOT)
  if("x${CMAKE_OSX_SYSROOT}" MATCHES "/")
    # This is a path to the SDK.  Make sure it exists.
    if(NOT IS_DIRECTORY "${CMAKE_OSX_SYSROOT}")
      message(WARNING "Ignoring CMAKE_OSX_SYSROOT value:\n ${CMAKE_OSX_SYSROOT}\n"
        "because the directory does not exist.")
      set(CMAKE_OSX_SYSROOT "")
    endif()
    set(_CMAKE_OSX_SYSROOT_PATH "${CMAKE_OSX_SYSROOT}")
  else()
    # Transform the sdk name into a path.
    execute_process(
      COMMAND xcodebuild -sdk ${CMAKE_OSX_SYSROOT} -version Path
      OUTPUT_VARIABLE _stdout
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_VARIABLE _stderr
      RESULT_VARIABLE _failed
      )
    if(NOT _failed AND IS_DIRECTORY "${_stdout}")
      set(_CMAKE_OSX_SYSROOT_PATH "${_stdout}")
      # For non-Xcode generators use the path.
      if(NOT "${CMAKE_GENERATOR}" MATCHES "Xcode")
        set(CMAKE_OSX_SYSROOT "${_CMAKE_OSX_SYSROOT_PATH}")
      endif()
    endif()
  endif()
endif()
