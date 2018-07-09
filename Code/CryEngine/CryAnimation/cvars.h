// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// console variable interfaces.
// In an instance of this class (singleton for now), there are all the interfaces
// of all the variables needed for the Animation subsystem.
//////////////////////////////////////////////////////////////////////////
struct Console;

extern Console g_Consoleself;

#if CRY_PLATFORM_MOBILE
	#define USE_FACIAL_ANIMATION_FRAMERATE_LIMITING 1
#else
	#define USE_FACIAL_ANIMATION_FRAMERATE_LIMITING 0
#endif

#define USE_FACIAL_ANIMATION_EFFECTOR_BALANCE 0

struct CRY_ALIGN(128) Console
{
	Console();
	//The base class of the character-manager is a SINGLETON object
	//Its impossible to remove this object or to create another instance of the console
	static ILINE Console& GetInst()
	{
		return g_Consoleself;
	}

	enum {nDefaultAnimWarningLevel = 2};

	enum
	{
		ApplyJointVelocitiesDisabled = 0,
		ApplyJointVelocitiesPhysics,
		ApplyJointVelocitiesAnimation,
	};

	void Init();
	const char* ca_CharEditModel;
	const char* ca_FilterJoints;
	const char* ca_DebugCommandBufferFilter;
	string ca_DebugTextTarget;
	uint32 ca_DebugTextLayer;

#ifndef _RELEASE
	int32 ca_DebugAnimUsageOnFileAccess;
	int32 ca_AttachmentTextureMemoryBudget;
#endif

	DeclareConstIntCVar(ca_SnapToVGrid, 0);
	DeclareConstIntCVar(ca_DebugSegmentation, 0);
	DeclareConstIntCVar(ca_DrawAimIKVEGrid, 0);
	DeclareConstIntCVar(ca_DrawAimPoses, 0);
	DeclareConstIntCVar(ca_DrawBBox, 0);
	DeclareConstIntCVar(ca_DrawAllSimulatedSockets, 0);
	DeclareConstIntCVar(ca_DrawSkeleton, 0);
	DeclareConstIntCVar(ca_DrawDecalsBBoxes, 0);
	DeclareConstIntCVar(ca_DrawPositionPost, 0);
	DeclareConstIntCVar(ca_DrawEmptyAttachments, 0);
	DeclareConstIntCVar(ca_DrawLookIK, 0);
	DeclareConstIntCVar(ca_DrawWireframe, 0);
	DeclareConstIntCVar(ca_DrawTangents, 0);
	DeclareConstIntCVar(ca_DrawBinormals, 0);
	DeclareConstIntCVar(ca_DrawNormals, 0);
	DeclareConstIntCVar(ca_DrawAttachments, 1);
	DeclareConstIntCVar(ca_DrawAttachmentsMergedForShadows, 1);
	DeclareConstIntCVar(ca_DrawAttachmentOBB, 0);
	DeclareConstIntCVar(ca_DrawAttachmentProjection, 0);
	DeclareConstIntCVar(ca_DrawBaseMesh, 1);
	DeclareConstIntCVar(ca_DrawLocator, 0);
	DeclareConstIntCVar(ca_DrawCGA, 1);
	DeclareConstIntCVar(ca_DrawCHR, 1);
	DeclareConstIntCVar(ca_DrawCC, 1);
	DeclareConstIntCVar(ca_DebugAnimMemTracking, 0);
	DeclareConstIntCVar(ca_DebugSWSkinning, 0);
	DeclareConstIntCVar(ca_DebugText, 0);
	DeclareConstIntCVar(ca_DebugCommandBuffer, 0);
	DeclareConstIntCVar(ca_DebugFacial, 0);
	DeclareConstIntCVar(ca_DebugFacialEyes, 0);
	DeclareConstIntCVar(ca_DebugAnimationStreaming, 0);
	DeclareConstIntCVar(ca_DebugAttachmentsProxies, 0);
	DeclareConstIntCVar(ca_DebugCriticalErrors, 0);
	DeclareConstIntCVar(ca_UseIMG_CAF, 1);
	DeclareConstIntCVar(ca_UseIMG_AIM, 1);
	DeclareConstIntCVar(ca_UseMorph, 1);
	DeclareConstIntCVar(ca_NoAnim, 0);
	DeclareConstIntCVar(ca_UsePhysics, 1);
	DeclareConstIntCVar(ca_DisableAuxPhysics, 0);
	DeclareConstIntCVar(ca_UseLookIK, 1);
	DeclareConstIntCVar(ca_UseAimIK, 1);
	DeclareConstIntCVar(ca_UseRecoil, 1);
	DeclareConstIntCVar(ca_UseFacialAnimation, 1);
	DeclareConstIntCVar(ca_UseScaling, 1);
	DeclareConstIntCVar(ca_ForceUpdateSkeletons, 0);
	DeclareConstIntCVar(ca_ApplyJointVelocitiesMode, 2);
	DeclareConstIntCVar(ca_UseDecals, 0);
	DeclareConstIntCVar(ca_AnimWarningLevel, 3);
	DeclareConstIntCVar(ca_KeepModels, 0);
	DeclareConstIntCVar(ca_DebugModelCache, 0);
	DeclareConstIntCVar(ca_ReloadAllCHRPARAMS, 0);
	DeclareConstIntCVar(ca_DebugAnimUpdates, 0);
	DeclareConstIntCVar(ca_DebugAnimUsage, 0);
	DeclareConstIntCVar(ca_StoreAnimNamesOnLoad, 0);
	DeclareConstIntCVar(ca_UnloadAnimationCAF, 1);
	DeclareConstIntCVar(ca_UnloadAnimationDBA, 1);
	DeclareConstIntCVar(ca_MinInPlaceCAFStreamSize, 128 * 1024);
	DeclareConstIntCVar(ca_DumpUsedAnims, 0);
	DeclareConstIntCVar(ca_LoadUncompressedChunks, 0);
	DeclareConstIntCVar(ca_eyes_procedural, 1);
	DeclareConstIntCVar(ca_lipsync_phoneme_offset, 20);
	DeclareConstIntCVar(ca_lipsync_phoneme_crossfade, 70);
	DeclareConstIntCVar(ca_lipsync_debug, 0);
	DeclareConstIntCVar(ca_useADIKTargets, 1);
	DeclareConstIntCVar(ca_DebugADIKTargets, 0);
	DeclareConstIntCVar(ca_SaveAABB, 0);
	DeclareConstIntCVar(ca_DebugSkeletonEffects, 0);
	DeclareConstIntCVar(ca_cloth_vars_reset, 2);
	DeclareConstIntCVar(ca_SerializeSkeletonAnim, 0);
	DeclareConstIntCVar(ca_LockFeetWithIK, 1);
	DeclareConstIntCVar(ca_AllowMultipleEffectsOfSameName, 1);
	DeclareConstIntCVar(ca_UseAssetDefinedLod, 0);
	DeclareConstIntCVar(ca_ForceAnimationLod, -1);
	DeclareConstIntCVar(ca_PrecacheAnimationSets, 0);
	DeclareConstIntCVar(ca_DisableAnimationUnloading, 0);
	DeclareConstIntCVar(ca_PreloadAllCAFs, 0);
	DeclareConstIntCVar(ca_SampleQuatHemisphereFromCurrentPose, 0);

#if USE_FACIAL_ANIMATION_FRAMERATE_LIMITING
	DeclareConstIntCVar(ca_FacialAnimationFramerate, 20);
#endif

	DeclareConstIntCVar(ca_Validate, 0);

	// Command Buffer
	DeclareConstIntCVar(ca_UseJointMasking, 1);

	// Multi-Threading
	DeclareConstIntCVar(ca_disable_thread, 1);
	DeclareConstIntCVar(ca_thread, 1);
	DeclareConstIntCVar(ca_thread0Affinity, 5);
	DeclareConstIntCVar(ca_thread1Affinity, 3);

	// DBA unloading timings
	DeclareConstIntCVar(ca_DBAUnloadUnregisterTime, 2);
	DeclareConstIntCVar(ca_DBAUnloadRemoveTime, 4);

	// vars in console .cfgs
	f32 ca_DrawVEGInfo;
	f32 ca_DecalSizeMultiplier;

	int32 ca_physicsProcessImpact;
	int32 ca_MemoryUsageLog;
	int32 ca_MemoryDefragPoolSize;
	int32 ca_MemoryDefragEnabled;
	int32 ca_ParametricPoolSize;

	int32 ca_StreamCHR;
	int32 ca_StreamDBAInPlace;

	int32 ca_VClothMode;

	f32 ca_lipsync_vertex_drag;
	f32 ca_lipsync_phoneme_strength;
	f32 ca_DeathBlendTime;
	f32 ca_FacialAnimationRadius;
	f32 ca_AttachmentCullingRation;
	f32 ca_AttachmentCullingRationMP;
	f32 ca_cloth_max_timestep;
	f32 ca_cloth_max_safe_step;
	f32 ca_cloth_stiffness;
	f32 ca_cloth_thickness;
	f32 ca_cloth_friction;
	f32 ca_cloth_stiffness_norm;
	f32 ca_cloth_stiffness_tang;
	f32 ca_cloth_damping;
	f32 ca_cloth_air_resistance;

	int32 ca_ResetCulledJointsToBindPose;

	f32 ca_MotionBlurMovementThreshold;

	int32 ca_vaEnable;
	int32 ca_vaProfile;
	int32 ca_vaBlendEnable;
	int32 ca_vaBlendPostSkinning;
	int32 ca_vaBlendCullingDebug;
	f32 ca_vaScaleFactor;
	f32 ca_vaBlendCullingThreshold;

	int32 ca_vaUpdateTangents;
	int32 ca_vaSkipVertexAnimationLOD;
	int32 ca_AttachmentMergingMemoryBudget;

	DeclareConstIntCVar(ca_DrawCloth, 1);
	DeclareConstIntCVar(ca_ClothBlending, 1);
	DeclareConstIntCVar(ca_ClothBypassSimulation, 0);
	DeclareConstIntCVar(ca_ClothMaxChars, 20);
	DeclareConstIntCVar(ca_ClothForceSkinningAfterNFrames, 3);
	bool DrawPose(const char mode);

private:

	//	~Console() {};
	//	Console(const Console&);
	void operator=(const Console&);

};
