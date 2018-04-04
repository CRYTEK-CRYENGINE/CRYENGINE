cmake_minimum_required(VERSION 3.5)
project(CryEngine_CSharp NONE)

if("${BUILD_PLATFORM}" STREQUAL "Win32")
	#message(STATUS "CESharp Build is 32-bit")
	SET(MONO_WIN64 FALSE)
elseif("${BUILD_PLATFORM}" STREQUAL "Win64")
	#message(STATUS "CESharp Build is 64-bit")
	SET(MONO_WIN64 TRUE)
endif()

option(OPTION_BUILD_CSHARP_WITH_MCS "Build C# with Mono Compiler" ON)

# .Compiler options for net assemblies (dll) using mono are similar to ms c# compiler options
# currently .net assemblies are compiled to "anycpu" 
# From microsoft :
# On a 64-bit Windows operating system:
# Assemblies compiled with /platform:x86 execute on the 32-bit CLR running under WOW64.
# A DLL compiled with the /platform:anycpu executes on the same CLR as the process into which it is loaded.
# Executables that are compiled with the /platform:anycpu execute on the 64-bit CLR.
# Executables compiled with /platform:anycpu32bitpreferred execute on the 32-bit CLR.
# The anycpu32bitpreferred setting is valid only for executable (.EXE) files, and it requires the .NET Framework 4.5.
if(OPTION_BUILD_CSHARP_WITH_MCS)
	create_mono_compiler_settings()
	message(STATUS "CESharp Mono cpu platform ${MONO_CPU_PLATFORM}")
	message(STATUS "CESharp Mono preprcoessor define ${MONO_PREPROCESSOR_DEFINE}")
	message(STATUS "CESharp MONO_LIB_PATH : ${MONO_LIB_PATH}")
	message(STATUS "CESharp MONO_LANGUAGE_VERSION : ${MONO_LANGUAGE_VERSION}")
endif()

if (OPTION_CRYMONO_SWIG)
	set(MONO_SWIG_OUTPUT_ASSEMBLY_REFERENCE "-r:${OUTPUT_DIRECTORY}/CryEngine.Common.dll")
else()
	set(MONO_SWIG_OUTPUT_ASSEMBLY_REFERENCE "")
endif()

add_subdirectory(Core)
add_subdirectory(Core.Tests)
add_subdirectory(Core.UI)