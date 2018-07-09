// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "cvars.h"
#include <CrySerialization/Enum.h>
//need global var as the singleton approach is too expensive
Console g_Consoleself;
namespace
{
int g_ConsoleInstanceCount;
}

Console::Console()
	: ca_CharEditModel(nullptr)
	, ca_FilterJoints(nullptr)
	, ca_DebugCommandBufferFilter(nullptr)
{
	if (g_ConsoleInstanceCount++)
		abort();
}

SERIALIZATION_ENUM_BEGIN(CA_Interpolation_Type, "Interpolation Type")
	SERIALIZATION_ENUM(CA_Interpolation_Type::Linear, "linear", "Linear")
	SERIALIZATION_ENUM(CA_Interpolation_Type::QuadraticIn, "quadraticIn", "QuadraticIn")
	SERIALIZATION_ENUM(CA_Interpolation_Type::QuadraticOut, "quadraticOut", "QuadraticOut")
	SERIALIZATION_ENUM(CA_Interpolation_Type::QuadraticInOut, "quadraticInOut", "QuadraticInOut")
	SERIALIZATION_ENUM(CA_Interpolation_Type::SineIn, "sineIn", "SineIn")
	SERIALIZATION_ENUM(CA_Interpolation_Type::SineOut, "sineOut", "SineOut")
	SERIALIZATION_ENUM(CA_Interpolation_Type::SineInOut, "sineInOut", "SineInOut")
SERIALIZATION_ENUM_END()

static void CADebugText(IConsoleCmdArgs* pArgs)
{
#ifndef CONSOLE_CONST_CVAR_MODE
	if (pArgs->GetArgCount() == 1)
	{
		CryLogAlways("%d", Console::GetInst().ca_DebugText);
	}
	else if (pArgs->GetArgCount() >= 2)
	{
		if (strcmp(pArgs->GetArg(1), "0") == 0)
		{
			Console::GetInst().ca_DebugText = 0;
			Console::GetInst().ca_DebugTextTarget = "";
			Console::GetInst().ca_DebugTextLayer = 0xffffffff;
		}
		else if (atoi(pArgs->GetArg(1)) > 0)
		{
			Console::GetInst().ca_DebugText = 1;
			Console::GetInst().ca_DebugTextTarget = "";
			Console::GetInst().ca_DebugTextLayer = 0xffffffff;
		}
		else
		{
			Console::GetInst().ca_DebugText = 1;
			Console::GetInst().ca_DebugTextTarget = pArgs->GetArg(1);
			Console::GetInst().ca_DebugTextLayer = 0xffffffff;
		}

		if (pArgs->GetArgCount() > 2)
		{
			Console::GetInst().ca_DebugTextLayer = atoi(pArgs->GetArg(2));
		}
	}
#endif
}

static void CADefaultTransitionInterpolationType(IConsoleCmdArgs *pArgs)
{
#ifndef CONSOLE_CONST_CVAR_MODE
	const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<CA_Interpolation_Type>(); 
	if(pArgs->GetArgCount() == 1)
	{
		CryLogAlways("%s", enumDescription.labelByIndex(g_DefaultTransitionInterpolationType));
	}
	else if(pArgs->GetArgCount() > 1)
	{
		stack_string arg = pArgs->GetArg(1);

		for (int i = 0; i < enumDescription.count(); i++)
		{
			if (arg.compareNoCase(enumDescription.labelByIndex(i)) == 0)
			{
				g_DefaultTransitionInterpolationType = i;
			}
		}
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
struct SDefaultTransitionInterpolationTypeAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const override
	{
		const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<CA_Interpolation_Type>();
		return enumDescription.count();
	}

	virtual const char* GetValue(int nIndex) const override
	{
		const Serialization::EnumDescription& enumDescription = Serialization::getEnumDescription<CA_Interpolation_Type>();
		if (nIndex >= 0 && nIndex < enumDescription.count())
			return enumDescription.labelByIndex(nIndex);

		return "";
	}
};

static SDefaultTransitionInterpolationTypeAutoComplete s_defaultTransitionInterpolationTypeAutoComplete;

//////////////////////////////////////////////////////////////////////////
void Console::Init()
{
#ifndef _RELEASE
	REGISTER_CVAR(ca_DebugAnimUsageOnFileAccess, 0, 0, "shows what animation assets are used in the level, triggered by key fileAccess events");
	REGISTER_CVAR(ca_AttachmentTextureMemoryBudget, 100, 0, "texture budget for e_debugdraw 20 - in megabytes");
#endif

	ca_CharEditModel = "objects/characters/human/sdk_player/sdk_player.cdf";
	REGISTER_STRING("ca_CharEditModel", ca_CharEditModel, VF_NULL, "");
	REGISTER_STRING("ca_FilterJoints", ca_FilterJoints, VF_NULL, "");
	REGISTER_STRING("ca_DrawPose", NULL, VF_NULL, "");
	REGISTER_CVAR2("ca_DebugCommandBufferFilter", &ca_DebugCommandBufferFilter, "", VF_NULL, "Limits the command buffer debug output to a cdf containing the given string.");
	assert(this);

	DefineConstIntCVar(ca_DrawAllSimulatedSockets, 0, VF_CHEAT, "if set to 1, the own bounding box of the character is drawn");
	DefineConstIntCVar(ca_DrawBBox, 0, VF_CHEAT, "if set to 1, the own bounding box of the character is drawn");
	DefineConstIntCVar(ca_DrawSkeleton, 0, VF_CHEAT, "if set to 1, the skeleton is drawn");
	DefineConstIntCVar(ca_DrawDecalsBBoxes, 0, VF_CHEAT, "if set to 1, the decals bboxes are drawn");
	DefineConstIntCVar(ca_DrawPositionPost, 0, VF_CHEAT, "draws the world position of the character (after update)");
	DefineConstIntCVar(ca_DrawEmptyAttachments, 0, VF_CHEAT, "draws a wireframe cube if there is no object linked to an attachment");
	DefineConstIntCVar(ca_DrawWireframe, 0, VF_CHEAT, "draws a wireframe on top of the rendered character");
	DefineConstIntCVar(ca_DrawAimPoses, 0, VF_CHEAT, "draws the wireframe of the aim poses");
	DefineConstIntCVar(ca_DrawLookIK, 0, VF_CHEAT, "draws a visualization of look ik");
	DefineConstIntCVar(ca_DrawTangents, 0, VF_CHEAT, "draws the tangents of the rendered character");
	DefineConstIntCVar(ca_DrawBinormals, 0, VF_CHEAT, "draws the binormals of the rendered character");
	DefineConstIntCVar(ca_DrawNormals, 0, VF_CHEAT, "draws the normals of the rendered character");
	DefineConstIntCVar(ca_DrawAttachments, 1, VF_CHEAT, "if this is 0, will not draw the attachments objects");
	DefineConstIntCVar(ca_DrawAttachmentsMergedForShadows, 1, VF_REQUIRE_APP_RESTART, "if this is 1 we merge attachments for shadow generation");
	REGISTER_CVAR(ca_AttachmentMergingMemoryBudget, 25 * 1024 * 1024, VF_NULL, "amount of memory (in bytes) dedicated to merged character attachments");
	DefineConstIntCVar(ca_DrawAttachmentOBB, 0, VF_CHEAT, "if this is 0, will not draw the attachments objects");
	DefineConstIntCVar(ca_DrawAttachmentProjection, 0, VF_CHEAT, "if this is 0, will not draw the attachment projections");
	DefineConstIntCVar(ca_DrawBaseMesh, 1, VF_CHEAT, "if this is 0, will not draw the characters");
	DefineConstIntCVar(ca_DrawLocator, 0, 0, "if this is 1, we will draw the body and move-direction. If this is 2, we will also print out the move direction");
	DefineConstIntCVar(ca_DrawCGA, 1, VF_CHEAT, "if this is 0, will not draw the CGA characters");
	DefineConstIntCVar(ca_DrawCHR, 1, VF_CHEAT, "if this is 0, will not draw the CHR characters");
	DefineConstIntCVar(ca_DrawCC, 1, VF_CHEAT, "if this is 0, will not draw the CC characters");
	DefineConstIntCVar(ca_DebugSWSkinning, 0, VF_CHEAT | VF_DUMPTODISK, "if this is 1, then we will see a green wireframe on top of software skinned meshes");
	DefineConstIntCVar(ca_DebugAnimMemTracking, 0, VF_CHEAT | VF_DUMPTODISK, "if this is 1, then its shows the anim-key allocations");
	REGISTER_COMMAND("ca_DebugText", (ConsoleCommandFunc)CADebugText, VF_CHEAT | VF_DUMPTODISK, "if this is 1, it will print some debug text on the screen\nif you give a file path or part of it instead, only the info for that character will appear");
	ca_DebugTextLayer = 0xffffffff;
	DefineConstIntCVar(ca_DebugCommandBuffer, 0, VF_CHEAT | VF_DUMPTODISK, "if this is 1, it will print the amount of commands for the blend-buffer");
	DefineConstIntCVar(ca_DebugAnimationStreaming, 0, VF_CHEAT | VF_DUMPTODISK, "if this is 1, then it shows what animations are streamed in");
	DefineConstIntCVar(ca_DebugAttachmentsProxies, 0, 0, "draw characters attachments proxies: 0 - disabled; 1 - dynamic proxies; 2 - auxiliary proxies; 4 - cloth proxies; 8 - ragdoll proxies");
	DefineConstIntCVar(ca_LoadUncompressedChunks, 0, VF_CHEAT, "If this 1, then uncompressed chunks prefer compressed while loading");
	DefineConstIntCVar(ca_UseMorph, 1, VF_CHEAT, "the morph skinning step is skipped (it's part of overall skinning during rendering)");
	DefineConstIntCVar(ca_NoAnim, 0, VF_CHEAT, "the animation isn't updated (the characters remain in the same pose)");
	DefineConstIntCVar(ca_UsePhysics, 1, VF_CHEAT, "the physics is not applied (effectively, no IK)");
	DefineConstIntCVar(ca_DisableAuxPhysics, 0, 0, "disable simulation of character ropes and cloth");
	DefineConstIntCVar(ca_UseLookIK, 1, VF_CHEAT, "If this is set to 1, then we are adding a look-at animation to the skeleton");
	DefineConstIntCVar(ca_UseAimIK, 1, VF_CHEAT, "If this is set to 1, then we are adding a look-at animation to the skeleton");
	DefineConstIntCVar(ca_UseRecoil, 1, VF_CHEAT, "If this is set to 1, then we enable procedural recoil");
	DefineConstIntCVar(ca_SnapToVGrid, 0, VF_CHEAT, "if set to 1, we snap the control parameter to the closest VCell");
	DefineConstIntCVar(ca_DebugSegmentation, 0, VF_CHEAT, "if set to 1, we can see the timing and the segment-counter of all assets in a BSpace");
	DefineConstIntCVar(ca_DrawAimIKVEGrid, 0, VF_CHEAT, "if set to 1, we will the the grid with the virtual examples");
	DefineConstIntCVar(ca_UseFacialAnimation, 1, VF_CHEAT, "If this is set to 1, we can play facial animations");
	DefineConstIntCVar(ca_UseScaling, 1, VF_CHEAT, "If set to 1, enables the scaling pipeline in the animation system.");
	DefineConstIntCVar(ca_LockFeetWithIK, 1, VF_CHEAT,
	                   "If this is set to 1, then we lock the feet to prevent sliding when additive animations are used"
	                   );
	DefineConstIntCVar(ca_ForceUpdateSkeletons, 0, VF_CHEAT, "Always update all skeletons, even if not visible.");
	DefineConstIntCVar(ca_ApplyJointVelocitiesMode, 2, VF_CHEAT, "Joint velocity preservation code mode: 0=Disabled, 1=Physics-driven, 2=Animation-driven");
	DefineConstIntCVar(ca_UseDecals, 0, 0, "if set to 0, effectively disables creation of decals on characters\n2 - alternative method of calculating/building the decals");
	DefineConstIntCVar(ca_DebugModelCache, 0, VF_CHEAT, "shows what models are currently loaded and how much memory they take");
	DefineConstIntCVar(ca_ReloadAllCHRPARAMS, 0, VF_CHEAT, "reload all CHRPARAMS");
	DefineConstIntCVar(ca_DebugAnimUpdates, 0, VF_CHEAT, "shows the amount of skeleton-updates");
	DefineConstIntCVar(ca_DebugAnimUsage, 0, VF_CHEAT, "shows what animation assets are used in the level (bitmask)"
                       "\n BIT 0 - Display general information"
                       "\n BIT 1 - Display CAF files used"
                       "\n BIT 2 - Dump CAF files used to console (bit will be reset after use)"
                       "\n BIT 3 - Display BlendSpaces used"
                       "\n BIT 4 - Dump BlendSpaces used to console (bit will be reset after use)"
                      );
	DefineConstIntCVar(ca_StoreAnimNamesOnLoad, 0, VF_CHEAT, "stores the names of animations during load to allow name lookup for debugging");
	DefineConstIntCVar(ca_DumpUsedAnims, 0, VF_CHEAT, "writes animation asset statistics to the disk");
	DefineConstIntCVar(ca_AnimWarningLevel, 3, VF_CHEAT | VF_DUMPTODISK,
	                   "if you set this to 0, there won't be any\nfrequest warnings from the animation system");
	DefineConstIntCVar(ca_KeepModels, 0, VF_CHEAT,
	                   "If set to 1, will prevent models from unloading from memory\nupon destruction of the last referencing character");
	// if this is not empty string, the animations of characters with the given model will be logged
	DefineConstIntCVar(ca_DebugSkeletonEffects, 0, VF_CHEAT, "If true, dump log messages when skeleton effects are handled.");
	DefineConstIntCVar(ca_lipsync_phoneme_offset, 20, VF_CHEAT, "Offset phoneme start time by this value in milliseconds");
	DefineConstIntCVar(ca_lipsync_phoneme_crossfade, 70, VF_CHEAT, "Cross fade time between phonemes in milliseconds");
	DefineConstIntCVar(ca_eyes_procedural, 1, 0, "Enables/Disables procedural eyes animation");
	DefineConstIntCVar(ca_lipsync_debug, 0, VF_CHEAT, "Enables facial animation debug draw");
	DefineConstIntCVar(ca_DebugFacial, 0, VF_CHEAT, "Debug facial playback info");
	DefineConstIntCVar(ca_DebugFacialEyes, 0, VF_CHEAT, "Debug facial eyes info");
	DefineConstIntCVar(ca_useADIKTargets, 1, VF_CHEAT, "Use Animation Driven Ik Targets.");
	DefineConstIntCVar(ca_DebugADIKTargets, 0, VF_CHEAT, "If 1, then it will show if there are animation-driven IK-Targets for this model.");
	DefineConstIntCVar(ca_SaveAABB, 0, 0, "if the AABB is invalid, replace it by the default AABB");
	DefineConstIntCVar(ca_DebugCriticalErrors, 0, VF_CHEAT, "if 1, then we stop with a Fatal-Error if we detect a serious issue");
	DefineConstIntCVar(ca_UseIMG_CAF, 1, VF_CHEAT, "if 1, then we use the IMG file. In development mode it is suppose to be off");
	DefineConstIntCVar(ca_UseIMG_AIM, 1, VF_CHEAT, "if 1, then we use the IMG file. In development mode it is suppose to be off");
	DefineConstIntCVar(ca_UnloadAnimationCAF, 1, VF_DUMPTODISK, "unloading streamed CAFs as soon as they are not used");
	DefineConstIntCVar(ca_UnloadAnimationDBA, 1, VF_NULL, "if 1, then unload DBA if not used");
	DefineConstIntCVar(ca_MinInPlaceCAFStreamSize, 128 * 1024, VF_CHEAT, "min size a caf should be for in-place streaming");
	DefineConstIntCVar(ca_cloth_vars_reset, 2, 0, "1 - load the values from the next char, 1 - apply normally, 2+ - ignore");
	DefineConstIntCVar(ca_SerializeSkeletonAnim, 0, VF_CHEAT, "Turn on CSkeletonAnim Serialization.");
	DefineConstIntCVar(ca_AllowMultipleEffectsOfSameName, 1, VF_CHEAT, "Allow a skeleton animation to spawn more than one instance of an effect with the same name on the same instance.");
	DefineConstIntCVar(ca_UseAssetDefinedLod, 0, VF_CHEAT, "Lowers render LODs for characters with respect to \"consoles_lod0\" UDP. Requires characters to be reloaded.");
	DefineConstIntCVar(ca_ForceAnimationLod, -1, VF_CHEAT, "Forces a specific LOD to be used for animation updates."
						"\n-1    - don't force any lod, use the lod system (default)"
						"\n 0..n - animation lod level");
	DefineConstIntCVar(ca_Validate, 0, VF_CHEAT, "if set to 1, will run validation on animation data");
	// animation transition interpolation mode
	REGISTER_COMMAND("ca_DefaultTransitionInterpolationType", (ConsoleCommandFunc)CADefaultTransitionInterpolationType, VF_CHEAT, "changes transition interpolation method.");
	gEnv->pConsole->RegisterAutoComplete("ca_DefaultTransitionInterpolationType", &s_defaultTransitionInterpolationTypeAutoComplete);

#if USE_FACIAL_ANIMATION_FRAMERATE_LIMITING
	DefineConstIntCVar(ca_FacialAnimationFramerate, 20, VF_CHEAT, "Update facial system at a maximum framerate of n. This framerate falls off linearly to zero with the distance.");
#endif

	// Command Buffer
	DefineConstIntCVar(ca_UseJointMasking, 1, VF_DUMPTODISK, "Use Joint Masking to speed up motion decoding.");

	// Multi-Threading
	DefineConstIntCVar(ca_disable_thread, 1, VF_DUMPTODISK, "TEMP Disable Animation Thread.");
	DefineConstIntCVar(ca_thread, 1, VF_DUMPTODISK, "If >0 enables Animation Multi-Threading.");
	DefineConstIntCVar(ca_thread0Affinity, 5, VF_DUMPTODISK, "Affinity of first Animation Thread.");
	DefineConstIntCVar(ca_thread1Affinity, 3, VF_DUMPTODISK, "Affinity of second Animation Thread.");

	// DBA unloading timings
	DefineConstIntCVar(ca_DBAUnloadUnregisterTime, 2, VF_CHEAT, "DBA Unload Timing: CAF Unregister Time.");
	DefineConstIntCVar(ca_DBAUnloadRemoveTime, 4, VF_CHEAT, "DBA Unload Timing: DBA Remove Time.");

	// Animation data caching behavior (PC specific)
	DefineConstIntCVar(ca_PrecacheAnimationSets, 0, VF_NULL, "Enable Precaching of Animation Sets per Character.");
	DefineConstIntCVar(ca_DisableAnimationUnloading, 0, VF_NULL, "Disable Animation Unloading.");

	DefineConstIntCVar(ca_PreloadAllCAFs, 0, VF_NULL, "Preload all CAFs during level preloading.");

	// vars in console .cfgs
	REGISTER_CVAR(ca_DrawVEGInfo, 0.0f, VF_CHEAT, "if set to 1, the VEG debug info is drawn");
	REGISTER_CVAR(ca_DecalSizeMultiplier, 1.0f, VF_CHEAT, "The multiplier for the decal sizes");

	REGISTER_CVAR(ca_physicsProcessImpact, 1, VF_CHEAT | VF_DUMPTODISK, "Process physics impact pulses.");
	REGISTER_CVAR(ca_MemoryUsageLog, 0, VF_CHEAT, "enables a memory usage log");

	const int32 defaultDefragPoolSize = 64 * 1024 * 1024;
	REGISTER_CVAR(ca_MemoryDefragPoolSize, defaultDefragPoolSize, 0, "Sets the upper limit on the defrag pool size");
	REGISTER_CVAR(ca_MemoryDefragEnabled, 1, 0, "Enables defragmentation of anim data");

	REGISTER_CVAR(ca_ParametricPoolSize, 64, 0, "Size of the parametric pool");

	REGISTER_CVAR(ca_StreamCHR, 1, 0, "Set to enable CHR streaming");
	REGISTER_CVAR(ca_StreamDBAInPlace, 1, 0, "Set to stream DBA files in place");

	//vertex animation
	REGISTER_CVAR(ca_vaEnable, 1, 0, "Enables Vertex Animation");
	REGISTER_CVAR(ca_vaProfile, 0, 0, "Enable Vertex Animation profile");
	REGISTER_CVAR(ca_vaBlendEnable, 1, 0, "Enables Vertex Animation blends");
	REGISTER_CVAR(ca_vaBlendPostSkinning, 0, 0, "Perform Vertex Animation blends post skinning");
	REGISTER_CVAR(ca_vaBlendCullingDebug, 0, 0, "Show Blend Shapes culling difference");
	REGISTER_CVAR(ca_vaBlendCullingThreshold, 1.f, 0, "Blend Shapes culling threshold");
	REGISTER_CVAR(ca_vaScaleFactor, 1.0f, 0, "Vertex Animation Weight Scale Factor");
	REGISTER_CVAR(ca_vaUpdateTangents, 1, 0, "Update Tangents on SKIN attachments that have the vertex color blue channel set to 255 and 8 weights");
	REGISTER_CVAR(ca_vaSkipVertexAnimationLOD, 0, 0, "Skip LOD 0 for characters using vertex animation");

	//motion blur
	REGISTER_CVAR(ca_MotionBlurMovementThreshold, 0.0f, 0, "\"advanced\" Set motion blur movement threshold for discarding skinned object");

	REGISTER_CVAR(ca_DeathBlendTime, 0.3f, VF_CHEAT, "Specifies the blending time between low-detail dead body skeleton and current skeleton");
	REGISTER_CVAR(ca_lipsync_vertex_drag, 1.2f, VF_CHEAT, "Vertex drag coefficient when blending morph targets");
	REGISTER_CVAR(ca_lipsync_phoneme_strength, 1.0f, 0, "LipSync phoneme strength");
	REGISTER_CVAR(ca_AttachmentCullingRation, 200.0f, 0, "ration between size of attachment and distance to camera");
	REGISTER_CVAR(ca_AttachmentCullingRationMP, 300.0f, 0, "ration between size of attachment and distance to camera for MP");

	// vcloth
	REGISTER_CVAR(ca_VClothMode, 1, 0, "0 - disabled (disable rendering & simulation)\n1 - enabled (default)\n2 - force skinning (disable simulation)");

	//cloth
	REGISTER_CVAR(ca_cloth_max_timestep, 0.0f, 0, "");
	REGISTER_CVAR(ca_cloth_max_safe_step, 0.0f, 0, "if a segment stretches more than this (in *relative* units), its length is reinforced");
	REGISTER_CVAR(ca_cloth_stiffness, 0.0f, 0, "stiffness for stretching");
	REGISTER_CVAR(ca_cloth_thickness, 0.0f, 0, "thickness for collision checks");
	REGISTER_CVAR(ca_cloth_friction, 0.0f, 0, "");
	REGISTER_CVAR(ca_cloth_stiffness_norm, 0.0f, 0, "stiffness for shape preservation along normals (\"convexity preservation\")");
	REGISTER_CVAR(ca_cloth_stiffness_tang, 0.0f, 0, "stiffness for shape preservation against tilting");
	REGISTER_CVAR(ca_cloth_damping, 0.0f, 0, "");
	REGISTER_CVAR(ca_cloth_air_resistance, 0.0f, 0, "\"advanced\" (more correct) version of damping");
	REGISTER_CVAR(ca_FacialAnimationRadius, 30.f, VF_CHEAT, "Maximum distance at which facial animations are updated - handles zooming correctly");

	//sampling
	REGISTER_CVAR(ca_ResetCulledJointsToBindPose, 1, 0, "Specifies whether culled joints should be reset to bind pose or preserve their transform from last evaluated frame (0 = preserve, 1 = reset).");
	DefineConstIntCVar(ca_SampleQuatHemisphereFromCurrentPose, 0, VF_NULL, "For override animation sampling, use current pose for quat hemisphere sign");

	DefineConstIntCVar(ca_DrawCloth, 1, VF_CHEAT, "bitfield: 2 shows particles, 4 shows proxies, 6 shows both");
	DefineConstIntCVar(ca_ClothBlending, 1, VF_CHEAT, "if this is 0 blending with animation is disabled");
	DefineConstIntCVar(ca_ClothBypassSimulation, 0, VF_CHEAT, "if this is 0 actual cloth simulation is disabled (wrap skinning still works)");
	DefineConstIntCVar(ca_ClothMaxChars, 20, VF_CHEAT, "max characters with cloth on screen");
	REGISTER_INT("ca_ClothForceSkinningAfterNFrames", 3, 0, "safety mechanism: if framerate falls below threshold for n-frames, skinning is forced to avoid performance issues");

}

bool Console::DrawPose(const char mode)
{
	static ICVar* pVar = gEnv->pSystem->GetIConsole()->GetCVar("ca_DrawPose");
	const char* poseDraw = pVar ? pVar->GetString() : NULL;
	if (!poseDraw)
		return false;

	for (; *poseDraw; ++poseDraw)
	{
		if (*poseDraw == mode)
			return true;
	}

	return false;
}
