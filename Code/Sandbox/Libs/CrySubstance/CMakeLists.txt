start_sources()
sources_platform(ALL)
add_sources("NoUberFile"
	SOURCE_GROUP "Code"
    "SubstanceCommon.h"
    "SubstanceCommon.cpp"
    "SubstancePreset.h"
    "SubstancePreset.cpp"
    "ISubstanceManager.h"
    "SubstanceManager.h"
    "SubstanceManager.cpp"    
    "SerializationSubstanceEnums.inl"
    "RCConfigParser.inl"
    "ISubstanceInstanceRenderer.h" 
	SOURCE_GROUP "Root"
		"StdAfx.cpp"
		"StdAfx.h"
        "CrySubstanceAPI.h"
   
)

end_sources()

if(NOT RESOURCE_COMPILER)
    CreateDynamicModule(CrySubstance PCH StdAfx.cpp SOLUTION_FOLDER "Sandbox/Libs")
    target_link_libraries(CrySubstance PRIVATE Substance)
    target_compile_definitions(${THIS_PROJECT} PRIVATE -DCrySubstanceEXPORTS -DWIN32 -DNOT_USE_CRY_MEMORY_MANAGER)
else()
    CommonLib(CrySubstance PCH StdAfx.cpp SOLUTION_FOLDER "RC")
    SET(LIB_DIR "${SDK_DIR}/SubstanceEngines/lib/win32-msvc2015-64")
    SET(DLL_DIR "${SDK_DIR}/SubstanceEngines/bin/win32-msvc2015-64")
    SET(TINYXML_LIB_DIR "${SDK_DIR}/SubstanceEngines/3rdparty/tinyxml/lib/win32-msvc2015-64")
    target_compile_options(${THIS_PROJECT} PRIVATE "/MT$<$<CONFIG:Debug>:d>")
    target_include_directories(${THIS_PROJECT} PRIVATE "${SDK_DIR}/SubstanceEngines/include" "${SDK_DIR}/SubstanceEngines/3rdparty/tinyxml")
    target_compile_definitions(${THIS_PROJECT} PRIVATE -DCRY_SUBSTANCE_STATIC -DWIN32 -DNOT_USE_CRY_MEMORY_MANAGER -DRESOURCE_COMPILER)
endif()


target_link_libraries(${THIS_PROJECT} PRIVATE dwmapi gdi32 Ole32)

target_compile_options(${THIS_PROJECT} PRIVATE 
	/EHsc
	/GR
	/wd4251
	/wd4275
)



