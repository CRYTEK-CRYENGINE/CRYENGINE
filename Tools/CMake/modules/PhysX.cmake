if(WIN64)

function(PhysXaddLib version extension)

	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PhysX_3.4/Bin/${version}/PhysX3${extension}_x64.dll")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PhysX_3.4/Bin/${version}/PhysX3CharacterKinematic${extension}_x64.dll")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PhysX_3.4/Bin/${version}/PhysX3Common${extension}_x64.dll")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PhysX_3.4/Bin/${version}/PhysX3Cooking${extension}_x64.dll")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PxShared/Bin/${version}/PxFoundation${extension}_x64.dll")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PxShared/Bin/${version}/PxPvdSDK${extension}_x64.dll")

	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PhysX_3.4/Lib/${version}/PhysX3${extension}_x64.lib")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PhysX_3.4/Lib/${version}/PhysX3Common${extension}_x64.lib")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PhysX_3.4/Lib/${version}/PhysX3Extensions${extension}.lib")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PhysX_3.4/Lib/${version}/PhysX3Cooking${extension}_x64.lib")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PhysX_3.4/Lib/${version}/PhysX3CharacterKinematic${extension}_x64.lib")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PhysX_3.4/Lib/${version}/PhysX3Vehicle${extension}.lib")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PxShared/Lib/${version}/PxFoundation${extension}.lib")
	set_target_properties(PHYSX PROPERTIES IMPORTED_LOCATION "${SDK_DIR}/PhysX/PxShared/Lib/${version}/PxPvdSDK${extension}.lib")
	 
	deploy_runtime_files("${SDK_DIR}/PhysX/PhysX_3.4/Bin/${version}/PhysX3${extension}_x64.dll")
	deploy_runtime_files("${SDK_DIR}/PhysX/PhysX_3.4/Bin/${version}/PhysX3CharacterKinematic${extension}_x64.dll")
	deploy_runtime_files("${SDK_DIR}/PhysX/PhysX_3.4/Bin/${version}/PhysX3Common${extension}_x64.dll")
	deploy_runtime_files("${SDK_DIR}/PhysX/PhysX_3.4/Bin/${version}/PhysX3Cooking${extension}_x64.dll")
	deploy_runtime_files("${SDK_DIR}/PhysX/PxShared/Bin/${version}/PxFoundation${extension}_x64.dll")
	deploy_runtime_files("${SDK_DIR}/PhysX/PxShared/Bin/${version}/PxPvdSDK${extension}_x64.dll")

endfunction()

add_library(PHYSX SHARED IMPORTED GLOBAL)

if (MSVC_VERSION GREATER 1900)     # Visual Studio > 2015
	set(PHYSX_VS_VERSION vc14win64)
elseif (MSVC_VERSION EQUAL 1900)     # Visual Studio 2015
	set(PHYSX_VS_VERSION vc14win64)
elseif (MSVC_VERSION EQUAL 1800) # Visual Studio 2013
	set(PHYSX_VS_VERSION vc12win64)
elseif (MSVC_VERSION EQUAL 1700) # Visual Studio 2012
	set(PHYSX_VS_VERSION vc11win64)
endif()

if (NOT DEFINED PHYSX_VS_VERSION)
	MESSAGE(STATUS "PHYSX: UNKNOWN VISUAL STUDIO VERSION")
endif()

if ("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Debug.*")
	PhysXaddLib(PHYSX_VS_VERSION "DEBUG")
endif()
if ("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Profile.*")
	PhysXaddLib(PHYSX_VS_VERSION "PROFILE")
endif()
if ("${CMAKE_CONFIGURATION_TYPES}" MATCHES ".*Release.*")
	# no extension needed in this case
	PhysXaddLib(PHYSX_VS_VERSION "")
endif()

set_target_properties(PHYSX PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SDK_DIR}/PhysX/PhysX_3.4/Include")
set_target_properties(PHYSX PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SDK_DIR}/PhysX/PxShared/Include")

endif() # if(WIN64)
