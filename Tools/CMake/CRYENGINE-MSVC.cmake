
set(MSVC_COMMON_FLAGS 
	/nologo	    # Don't show version info
	/W3         # Enable warning level 3
	/fp:fast    # Use fast floating point precision model
	/Zc:wchar_t # Parse wchar_t as internal type
	/GF         # Eliminate Duplicate Strings
	/Gy         # Enable function level linking
	/GR-        # Disable RTTI
	/utf-8      # Set source and execution charset to utf-8
	/Wv:18      # Disable warnings until SDK depedencies switch to UTF-8/ASCII.
	/MP         # Build with multiple processes
	/bigobj     # Allow larger .obj files

	/WX         # Treat warnings as errors
	/wd4653     # Ignore PCH for any individual file that has different optimization settings
)
string(REPLACE ";" " " MSVC_COMMON_FLAGS "${MSVC_COMMON_FLAGS}")
 
# Override cxx flags
set(CMAKE_CXX_FLAGS "${MSVC_COMMON_FLAGS}" CACHE STRING "C++ Common Flags" FORCE)

set(CMAKE_C_FLAGS_DEBUG "/MDd /Zi /Zo /Od /Ob0 /Oy- /RTC1 /GS /DDEBUG /D_DEBUG" CACHE STRING "C Flags" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Zo /Od /Ob0 /Oy- /RTC1 /GS /DDEBUG /D_DEBUG" CACHE STRING "C++ Flags" FORCE)

# Create PDBs (/Zi)
# Create better debug info (/Zo)
# Enable full optimization (/Ox) Same as /Ob2 /Oi /Ot /Oy
# Don't omit frame pointer (/Oy-)
# Disable buffer security checks (/GS-)
set(CMAKE_C_FLAGS_PROFILE "/Zi /Zo /MD /Ox /Oy- /GS- /DNDEBUG /D_PROFILE" CACHE STRING "C Flags" FORCE)
set(CMAKE_CXX_FLAGS_PROFILE "/Zi /Zo /MD /Ox /Oy- /GS- /DNDEBUG /D_PROFILE" CACHE STRING "C++ Flags" FORCE)

set(CMAKE_C_FLAGS_RELEASE "/Zi /Zo /MD /Ox /GS- /DNDEBUG /D_RELEASE" CACHE STRING "C Flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "/Zi /Zo /MD /Ox /GS- /DNDEBUG /D_RELEASE" CACHE STRING "C++ Flags" FORCE)

set(CMAKE_SHARED_LINKER_FLAGS_PROFILE "/debug" CACHE STRING "Profile link flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_PROFILE    "/debug" CACHE STRING "Profile link flags" FORCE)
set(CMAKE_MODULE_LINKER_FLAGS_PROFILE "/debug /INCREMENTAL" CACHE STRING "Profile link flags" FORCE)

set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/debug" CACHE STRING "Release link flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE    "/debug" CACHE STRING "Release link flags" FORCE)
set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "/debug /INCREMENTAL" CACHE STRING "Release link flags" FORCE)

function (wrap_whole_archive project target source)
	set(${target} "${${source}}" PARENT_SCOPE)
	set( whole_archive_link_flags "")
	foreach(module ${${source}})
		# Get Type of module target, (STATIC_LIBRARY,SHARED_LIBRARY,EXECUTABLE)
		get_target_property(ModuleProjectType ${module} TYPE)
		if (ModuleProjectType STREQUAL "STATIC_LIBRARY")
			# Only Static library needs a /WHOLEARCHIVE linker switch to prevent linker from optimizing static factories
			list( APPEND whole_archive_link_flags "/WHOLEARCHIVE:${module}")
		endif()
	endforeach()
	string (REPLACE ";" " " whole_archive_link_flags "${whole_archive_link_flags}")
	#message(STATUS "${whole_archive_link_flags}")
	set_target_properties(${project} PROPERTIES LINK_FLAGS ${whole_archive_link_flags})
endfunction()

