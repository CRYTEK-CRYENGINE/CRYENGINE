if(OPTION_BUILD_CSHARP_WITH_CSC)
	project(CryEngine.Tests NONE)

	set(SDK_DIR "${CMAKE_SOURCE_DIR}/Code/SDKs")
	create_mono_compiler_settings()

	set(Entity_FILES Entity/IconsTest.cs)
	source_group("Entity" FILES ${Entity_FILES})


	set(Math_FILES Math/MathHelpersTest.cs Math/Matrix3x3Test.cs Math/Matrix3x4Test.cs Math/Matrix4x4Test.cs Math/QuaternionTest.cs Math/Vector2Test.cs Math/Vector3Test.cs Math/Vector4Test.cs)
	source_group("Math" FILES ${Math_FILES})


	set(Properties_FILES Properties/AssemblyInfo.cs)
	source_group("Properties" FILES ${Properties_FILES})


	set(Serialization_FILES Serialization/DomainSerializerTests.cs)
	source_group("Serialization" FILES ${Serialization_FILES})


	set(SOURCE ${Entity_FILES} ${Math_FILES} ${Properties_FILES} ${Serialization_FILES})

	add_custom_target(CryEngine.Core.Tests
		COMMAND "${CSC_PATH}" /target:library /langversion:${MONO_LANGUAGE_VERSION} /platform:${MONO_CPU_PLATFORM} /optimize /debug:portable /define:${MONO_PREPROCESSOR_DEFINE} /reference:System.dll /reference:System.Data.dll /reference:System.Drawing.dll /reference:System.Runtime.Serialization.dll /reference:System.Windows.Forms.dll /reference:System.Xml.dll /reference:"${SDK_DIR}/nunit/framework/3.4.1.0/net-4.5/nunit.framework.dll" /reference:"${OUTPUT_DIRECTORY}/CryEngine.Common.dll" /reference:"${OUTPUT_DIRECTORY}/CryEngine.Core.dll" ${SOURCE} "/out:${OUTPUT_DIRECTORY}/CryEngine.Core.Tests.dll"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		SOURCES ${SOURCE})
else()
	include_external_msproject(CryEngine.Core.Tests "${CMAKE_CURRENT_LIST_DIR}/Core.Tests.csproj")
endif()

add_dependencies(CryEngine.Core.Tests CryEngine.Core)

set_property(TARGET CryEngine.Core.Tests PROPERTY FOLDER "CryMono")
