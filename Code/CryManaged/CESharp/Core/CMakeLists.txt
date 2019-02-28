if(OPTION_BUILD_CSHARP_WITH_CSC)
	project(CESharp NONE)

	set(SDK_DIR "${CMAKE_SOURCE_DIR}/Code/SDKs")
	create_mono_compiler_settings()

	set(AI_FILES AI/Attributes.cs AI/BehaviorTreeNode.cs AI/BehaviorTreeNodeFactory.cs AI/LastingMovementRequest.cs)
	source_group("AI" FILES ${AI_FILES})


	set(Animations_FILES Animations/ActionController.cs Animations/AnimationContext.cs Animations/AnimationContextAction.cs Animations/AnimationDatabase.cs Animations/AnimationDatabaseManager.cs Animations/AnimationTag.cs Animations/Character.cs Animations/CharacterAttachment.cs Animations/ControllerDefinition.cs Animations/Mannequin.cs Animations/MotionParameterID.cs Animations/TagState.cs)
	source_group("Animations" FILES ${Animations_FILES})


	set(Audio_Native_FILES Audio/Native/NativeAudioMethods.cs)
	source_group("Audio\\Native" FILES ${Audio_Native_FILES})


	set(Audio_FILES Audio/Audio.cs Audio/AudioFile.cs Audio/AudioListener.cs Audio/AudioManager.cs Audio/AudioObject.cs Audio/AudioObjectTransformation.cs Audio/AudioRequestInfo.cs)
	source_group("Audio" FILES ${Audio_FILES})


	set(Compilation_FILES Compilation/CompilationFailedException.cs Compilation/Compiler.cs)
	source_group("Compilation" FILES ${Compilation_FILES})


	set(Console_Native_FILES Console/Native/NativeConsoleMethods.cs)
	source_group("Console\\Native" FILES ${Console_Native_FILES})


	set(Console_FILES Console/Attributes.cs Console/ConsoleCommand.cs Console/ConsoleVariable.cs)
	source_group("Console" FILES ${Console_FILES})


	set(Debugging_FILES Debugging/DebugDraw.cs Debugging/ExceptionHandler.cs Debugging/ExceptionMessage.cs Debugging/ExceptionMessage.Designer.cs Debugging/Log.cs)
	source_group("Debugging" FILES ${Debugging_FILES})


	set(EntitySystem_Attributes_FILES EntitySystem/Attributes/EntityClassAttribute.cs EntitySystem/Attributes/EntityComponentAttribute.cs EntitySystem/Attributes/SchematycMethodAttribute.cs EntitySystem/Attributes/SchematycSignalAttribute.cs)
	source_group("EntitySystem\\Attributes" FILES ${EntitySystem_Attributes_FILES})


	set(EntitySystem_Enums_FILES EntitySystem/Enums/Primitives.cs)
	source_group("EntitySystem\\Enums" FILES ${EntitySystem_Enums_FILES})


	set(EntitySystem_Native_FILES EntitySystem/Native/NativeEntityMethods.cs)
	source_group("EntitySystem\\Native" FILES ${EntitySystem_Native_FILES})


	set(EntitySystem_FILES EntitySystem/Entity.cs EntitySystem/EntityComponent.cs EntitySystem/EntityId.cs EntitySystem/EntityProperty.cs EntitySystem/EntitySlotFlags.cs EntitySystem/EntityStatics.cs EntitySystem/PhysicsEntity.cs)
	source_group("EntitySystem" FILES ${EntitySystem_FILES})


	set(FileSystem_FILES FileSystem/CryPakFile.cs FileSystem/Directory.cs FileSystem/File.cs FileSystem/FileMode.cs FileSystem/Path.cs)
	source_group("FileSystem" FILES ${FileSystem_FILES})


	set(Input_Enums_FILES Input/Enums/InputDeviceType.cs Input/Enums/InputModifierFlags.cs Input/Enums/InputState.cs Input/Enums/KeyId.cs)
	source_group("Input\\Enums" FILES ${Input_Enums_FILES})


	set(Input_FILES Input/ActionHandler.cs Input/Input.cs Input/InputEvent.cs Input/Mouse.cs)
	source_group("Input" FILES ${Input_FILES})


	set(Lights_FILES Lights/DynamicLight.cs Lights/RenderLight.cs)
	source_group("Lights" FILES ${Lights_FILES})


	set(Math_FILES Math/Angles3.cs Math/Color.cs Math/MathHelpers.cs Math/Matrix3x3.cs Math/Matrix3x4.cs Math/Matrix4x4.cs Math/Primitives.cs Math/Quaternion.cs Math/Vector2.cs Math/Vector3.cs Math/Vector4.cs)
	source_group("Math" FILES ${Math_FILES})


	set(Particles_FILES Particles/ParticleAttributes.cs Particles/ParticleEffect.cs Particles/ParticleEffectListener.cs Particles/ParticleEmitter.cs Particles/ParticleLocation.cs Particles/ParticleManager.cs Particles/SpawnParameters.cs)
	source_group("Particles" FILES ${Particles_FILES})


	set(Physics_Enums_FILES Physics/Enums/AreaType.cs Physics/Enums/EntityRaycastFlags.cs Physics/Enums/PhysicalizationType.cs Physics/Enums/PhysicsEntityFlags.cs)
	source_group("Physics\\Enums" FILES ${Physics_Enums_FILES})


	set(Physics_Parameters_FILES Physics/Parameters/AreaParameters.cs Physics/Parameters/BasePhysicsParameters.cs Physics/Parameters/BuoyancyParameters.cs Physics/Parameters/CarParameters.cs Physics/Parameters/ParticleParameters.cs Physics/Parameters/PlayerDimensionsParameters.cs Physics/Parameters/PlayerDynamicsParameters.cs)
	source_group("Physics\\Parameters" FILES ${Physics_Parameters_FILES})


	set(Physics_PhysicalizeParameters_FILES Physics/PhysicalizeParameters/AreaPhysicalizeParams.cs Physics/PhysicalizeParameters/ArticulatedPhysicalizeParams.cs Physics/PhysicalizeParameters/EntityPhysicalizeParams.cs Physics/PhysicalizeParameters/LivingPhysicalizeParams.cs Physics/PhysicalizeParameters/ParticlePhysicalizeParams.cs Physics/PhysicalizeParameters/RigidPhysicalizeParams.cs Physics/PhysicalizeParameters/RopePhysicalizeParams.cs Physics/PhysicalizeParameters/SoftPhysicalizeParams.cs Physics/PhysicalizeParameters/StaticPhysicalizeParams.cs Physics/PhysicalizeParameters/WheeledVehiclePhysicalizeParams.cs)
	source_group("Physics\\PhysicalizeParameters" FILES ${Physics_PhysicalizeParameters_FILES})


	set(Physics_PhysicsStatus_FILES Physics/PhysicsStatus/BasePhysicsStatus.cs Physics/PhysicsStatus/DynamicsStatus.cs Physics/PhysicsStatus/LivingStatus.cs Physics/PhysicsStatus/VehicleStatus.cs)
	source_group("Physics\\PhysicsStatus" FILES ${Physics_PhysicsStatus_FILES})


	set(Physics_FILES Physics/Physics.cs Physics/Ray.cs Physics/RaycastHit.cs)
	source_group("Physics" FILES ${Physics_FILES})


	set(Properties_FILES Properties/AssemblyInfo.cs)
	source_group("Properties" FILES ${Properties_FILES})


	set(Rendering_FILES Rendering/Renderer.cs)
	source_group("Rendering" FILES ${Rendering_FILES})


	set(Resources_FILES Resources/Texture.cs)
	source_group("Resources" FILES ${Resources_FILES})


	set(Serialization_FILES Serialization/CachedTypeInfo.cs Serialization/DomainSerializer.cs Serialization/ISerializedObject.cs Serialization/ObjectReader.cs Serialization/ObjectWriter.cs Serialization/SerializeValueAttribute.cs)
	source_group("Serialization" FILES ${Serialization_FILES})


	set(Util_FILES Util/Attribute.cs Util/AutoPinner.cs Util/Event.cs Util/IconType.cs Util/ReflectionHelper.cs Util/Tools.cs Util/View.cs)
	source_group("Util" FILES ${Util_FILES})


	set(Core_FILES Camera.cs Engine.cs Extensions.cs GameFramework.cs ICryEnginePlugin.cs LevelSystem.cs Random.cs SystemEventHandler.cs Timer.cs)
	source_group("Core" FILES ${Core_FILES})


	set(SOURCE ${AI_FILES} ${Animations_FILES} ${Audio_Native_FILES} ${Audio_FILES} ${Compilation_FILES} ${Console_Native_FILES} ${Console_FILES} ${Debugging_FILES} ${EntitySystem_Attributes_FILES} ${EntitySystem_Enums_FILES} ${EntitySystem_Native_FILES} ${EntitySystem_FILES} ${FileSystem_FILES} ${Input_Enums_FILES} ${Input_FILES} ${Lights_FILES} ${Math_FILES} ${Particles_FILES} ${Physics_Enums_FILES} ${Physics_Parameters_FILES} ${Physics_PhysicalizeParameters_FILES} ${Physics_PhysicsStatus_FILES} ${Physics_FILES} ${Properties_FILES} ${Rendering_FILES} ${Resources_FILES} ${Serialization_FILES} ${Util_FILES} ${Core_FILES})

	add_custom_target(CryEngine.Core ALL
		COMMAND "${CSC_PATH}" /target:library /langversion:${MONO_LANGUAGE_VERSION} /platform:${MONO_CPU_PLATFORM} /optimize /debug:portable /define:${MONO_PREPROCESSOR_DEFINE} /reference:System.dll /reference:System.Data.dll /reference:System.Drawing.dll /reference:System.Runtime.Serialization.dll /reference:System.Windows.Forms.dll /reference:System.Xml.dll "${MONO_SWIG_OUTPUT_ASSEMBLY_REFERENCE}" ${SOURCE} /out:"${OUTPUT_DIRECTORY}/CryEngine.Core.dll" /doc:"${OUTPUT_DIRECTORY}/CryEngine.Core.xml"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		SOURCES ${SOURCE})

	#Doesn't seem to do anything...
	set_target_properties(CryEngine.Core PROPERTIES VS_DOTNET_REFERENCES "System;System.Data")
else()
	include_external_msproject(CryEngine.Core "${CMAKE_CURRENT_LIST_DIR}/Core.csproj")
endif()

if (OPTION_CRYMONO)
	add_dependencies(CryEngine.Core CryMonoBridge)
endif()

set_property(TARGET CryEngine.Core PROPERTY FOLDER "CryMono")
