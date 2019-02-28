if(OPTION_BUILD_CSHARP_WITH_CSC)
	project(CESharp NONE)

	set(SDK_DIR "${CMAKE_SOURCE_DIR}/Code/SDKs")
	create_mono_compiler_settings()

	set(Components_FILES Components/ButtonCtrl.cs Components/CheckBoxCtrl.cs Components/CollapseCtrl.cs Components/ComboBoxCtrl.cs Components/Image.cs Components/RectTransform.cs Components/ScrollBarCtrl.cs Components/SplitBarCtrl.cs Components/Text.cs Components/TextCtrl.cs Components/UIComponent.cs)
	source_group("Components" FILES ${Components_FILES})


	set(Properties_FILES Properties/AssemblyInfo.cs)
	source_group("Properties" FILES ${Properties_FILES})


	set(Resources_FILES Resources/Graphic.cs Resources/ImageSource.cs Resources/Resource.cs)
	source_group("Resources" FILES ${Resources_FILES})


	set(Core.UI_FILES Button.cs Canvas.cs CheckBox.cs ComboBox.cs HorizontalLayoutGroup.cs IUIComponent.cs LayoutGroup.cs Panel.cs SceneManager.cs SceneObject.cs ScrollPanel.cs Splitter.cs TextElement.cs TextInput.cs Transform.cs UIElement.cs VerticalLayoutGroup.cs Window.cs)
	source_group("Core.UI" FILES ${Core.UI_FILES})


	set(SOURCE ${Components_FILES} ${Properties_FILES} ${Resources_FILES} ${Core.UI_FILES})

	add_custom_target(CryEngine.Core.UI ALL
		COMMAND "${CSC_PATH}" /target:library /langversion:${MONO_LANGUAGE_VERSION} /platform:${MONO_CPU_PLATFORM} /optimize+ /debug:portable /define:${MONO_PREPROCESSOR_DEFINE} /reference:System.dll /reference:System.Data.dll /reference:System.Drawing.dll /reference:System.Runtime.Serialization.dll /reference:System.Windows.Forms.dll /reference:System.Xml.dll ${MONO_SWIG_OUTPUT_ASSEMBLY_REFERENCE} /reference:${OUTPUT_DIRECTORY}/CryEngine.Core.dll ${SOURCE} "/out:${OUTPUT_DIRECTORY}/CryEngine.Core.UI.dll" "/doc:${OUTPUT_DIRECTORY}/CryEngine.Core.UI.xml"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		SOURCES ${SOURCE})

	set_target_properties(CryEngine.Core.UI PROPERTIES VS_DOTNET_REFERENCES "System;System.Data")
else()
	include_external_msproject(CryEngine.Core.UI ${CMAKE_CURRENT_LIST_DIR}/Core.UI.csproj)
endif()

add_dependencies(CryEngine.Core.UI CryEngine.Core)

set_property(TARGET CryEngine.Core.UI PROPERTY FOLDER "CryMono")
