if(OPTION_BUILD_CSHARP_WITH_MCS)
	project(CryEngine.Tests NONE)
	
	set(SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../SDKs")
	set(MCS_PATH "${SDK_DIR}/Mono/bin/mcs.bat")
	
	create_mono_compiler_settings()
	set(Serialization_FILES Serialization/DomainSerializerTests.cs)
	source_group("Serialization" FILES ${Serialization_FILES})
	
	set (ObjectIcon_FILES Entity/IconsTest.cs)
	source_group("IconFiles" FILES ${ObjectIcon_FILES})
	
	set(Math_FILES Math/QuaternionTest.cs Math/Vector2Test.cs Math/Vector3Test.cs Math/Vector4Test.cs Math/Matrix3x3Test.cs Math/Matrix3x4Test.cs Math/Matrix4x4Test.cs Math/MathHelpersTest.cs)
	#set(Math_FILES Math/MathHelpersTest.cs Math/Vector2Test.cs Math/Vector3Test.cs Math/Vector4Test.cs Math/Matrix3x3Test.cs Math/Matrix3x4Test.cs Math/Matrix4x4Test.cs)
	source_group("Math" FILES ${Math_FILES})

	set(SOURCE ${Serialization_FILES} ${ObjectIcon_FILES} ${Math_FILES})

	add_custom_target(CryEngine.Core.Tests
		COMMAND "${MCS_PATH}" -target:library -langversion:${MONO_LANGUAGE_VERSION} -platform:${MONO_CPU_PLATFORM} -optimize -debug -define:${MONO_PREPROCESSOR_DEFINE} -lib:${MONO_LIB_PATH} -r:System -r:System.Data -r:System.Drawing -r:System.Runtime.Serialization -r:System.Windows.Forms -r:System.Xml -r "${SDK_DIR}/nunit/framework/3.4.1.0/net-4.5/nunit.framework.dll" "-r:${OUTPUT_DIRECTORY}/CryEngine.Common.dll" "-r:${OUTPUT_DIRECTORY}/CryEngine.Core.dll" ${SOURCE} "-out:${OUTPUT_DIRECTORY}/CryEngine.Core.Tests.dll"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		SOURCES ${SOURCE})
else()
	include_external_msproject(CryEngine.Core.Tests "${CMAKE_CURRENT_LIST_DIR}/Core.Tests.csproj")
endif()

add_dependencies(CryEngine.Core.Tests CryEngine.Core)

set_property(TARGET CryEngine.Core.Tests PROPERTY FOLDER "CryMono")
