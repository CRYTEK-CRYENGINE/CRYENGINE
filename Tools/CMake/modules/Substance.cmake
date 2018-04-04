if(WIN64)
   add_library(Substance SHARED IMPORTED GLOBAL)
   SET(LIB_DIR "${SDK_DIR}/SubstanceEngines/lib/win32-msvc2015-64")
   SET(DLL_DIR "${SDK_DIR}/SubstanceEngines/bin/win32-msvc2015-64")
   SET(TINYXML_LIB_DIR "${SDK_DIR}/SubstanceEngines/3rdparty/tinyxml/lib/win32-msvc2015-64")
   set_property(TARGET Substance APPEND PROPERTY IMPORTED_CONFIGURATIONS PROFILE)
   set_property(TARGET Substance APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG) 
   set_target_properties(Substance PROPERTIES IMPORTED_IMPLIB_PROFILE "${LIB_DIR}/release_md/substance_framework.lib")
   set_target_properties(Substance PROPERTIES IMPORTED_IMPLIB_DEBUG "${LIB_DIR}/debug_md/substance_framework.lib")
   set_property(TARGET Substance PROPERTY INTERFACE_LINK_LIBRARIES  "${LIB_DIR}/$<$<NOT:$<CONFIG:Debug>>:release_md>$<$<CONFIG:Debug>:debug_md>/substance_linker.lib"
                                                                    "${LIB_DIR}/$<$<NOT:$<CONFIG:Debug>>:release_md>$<$<CONFIG:Debug>:debug_md>/substance_sse2_blend.lib"
                                                                    "${TINYXML_LIB_DIR}/$<$<NOT:$<CONFIG:Debug>>:release_md>$<$<CONFIG:Debug>:debug_md>/tinyxml.lib") 
 
    
    set_property(TARGET Substance APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${SDK_DIR}/SubstanceEngines/include")
    set_property(TARGET Substance APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${SDK_DIR}/SubstanceEngines/3rdparty/tinyxml")


endif() # if(WIN64)

macro(copy_substance) 
	if (WIN64)
        file(TO_NATIVE_PATH "${OUTPUT_DIRECTORY}" NATIVE_OUTDIR)
        file(TO_NATIVE_PATH "${DLL_DIR}" NATIVE_DLLDIR)
        add_custom_command(TARGET ${THIS_PROJECT} PRE_BUILD
        COMMAND copy /Y "${NATIVE_DLLDIR}\\$<$<NOT:$<CONFIG:Debug>>:release_md>$<$<CONFIG:Debug>:debug_md>\\substance_sse2_blend.dll" "${NATIVE_OUTDIR}"
        COMMAND copy /Y "${NATIVE_DLLDIR}\\$<$<NOT:$<CONFIG:Debug>>:release_md>$<$<CONFIG:Debug>:debug_md>\\substance_linker.dll" "${NATIVE_OUTDIR}")
    else()
		message(WARNING "PortAudio library copy not implemented for this platform!")
	endif()
endmacro()
