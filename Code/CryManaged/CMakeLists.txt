cmake_minimum_required(VERSION 3.5)

option(OPTION_BUILD_CSHARP_WITH_CSC "Build C# with Roslyn Compiler" ON)
set(CSC_PATH "${SDK_DIR}/Mono/bin/csc.bat")
set(MSBUILD_PATH "${SDK_DIR}/Mono/bin/msbuild.bat")

macro(create_mono_compiler_settings)
	set(MONO_LANGUAGE_VERSION 7)
	if("${BUILD_PLATFORM}" STREQUAL "Win32")
		set(MONO_CPU_PLATFORM x86)
		set(MONO_PREPROCESSOR_DEFINE WIN32)
	elseif("${BUILD_PLATFORM}" STREQUAL "Win64")
		set(MONO_CPU_PLATFORM anycpu)
		set(MONO_PREPROCESSOR_DEFINE WIN64)
	endif()
	set(MONO_PREPROCESSOR_DEFINE ${MONO_PREPROCESSOR_DEFINE}$<SEMICOLON>$<UPPER_CASE:$<CONFIG>>)
endmacro()

# .Compiler options for net assemblies (dll) using mono are similar to ms c# compiler options
# currently .net assemblies are compiled to "anycpu"
# From microsoft :
# On a 64-bit Windows operating system:
# Assemblies compiled with /platform:x86 execute on the 32-bit CLR running under WOW64.
# A DLL compiled with the /platform:anycpu executes on the same CLR as the process into which it is loaded.
# Executables that are compiled with the /platform:anycpu execute on the 64-bit CLR.
# Executables compiled with /platform:anycpu32bitpreferred execute on the 32-bit CLR.
# The anycpu32bitpreferred setting is valid only for executable (.EXE) files, and it requires the .NET Framework 4.5.
if(OPTION_BUILD_CSHARP_WITH_CSC)
	create_mono_compiler_settings()
	message(STATUS "C# Mono platform ${MONO_CPU_PLATFORM}")
	message(STATUS "C# Mono preprocessor define ${MONO_PREPROCESSOR_DEFINE}")
	message(STATUS "C# Language Version : ${MONO_LANGUAGE_VERSION}")
endif()

if (OPTION_CRYMONO_SWIG)
	set(MONO_SWIG_OUTPUT_ASSEMBLY_NAME "CryEngine.Common")
	set(MONO_SWIG_OUTPUT_ASSEMBLY_REFERENCE "/reference:${OUTPUT_DIRECTORY}/${MONO_SWIG_OUTPUT_ASSEMBLY_NAME}.dll")
else()
	set(MONO_SWIG_OUTPUT_ASSEMBLY_REFERENCE "")
endif()

add_subdirectory(CESharp)
add_subdirectory(CryMonoBridge)