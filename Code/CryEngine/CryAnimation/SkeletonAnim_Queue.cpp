// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SkeletonAnim.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include "CharacterInstance.h"
#include "Model.h"
#include "CharacterManager.h"
#include <float.h>
#include "LoaderDBA.h"
#include "ParametricSampler.h"

bool CSkeletonAnim::StartAnimation(const char* szAnimName, const struct CryCharAnimationParams& Params)
{
	if (szAnimName == 0)
	{
		g_pILog->LogError("No name for animation specified");
		assert(!"No name for animation specified");
		return false;
	}

	CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;
	int32 nAnimID = pAnimationSet->GetAnimIDByName(szAnimName);
	if (nAnimID < 0)
	{
		if (Console::GetInst().ca_AnimWarningLevel > 0)
		{
			gEnv->pLog->LogError("animation-name '%s' not in chrparams file for model '%s'", szAnimName, m_pInstance->m_pDefaultSkeleton->GetModelFilePath());
		}
		return 0;
	}

	return StartAnimationById(nAnimID, Params);
}

//--------------------------------------------------------------------------

bool CSkeletonAnim::StartAnimationById(int32 id, const struct CryCharAnimationParams& Params)
{
	ANIMATION_LIGHT_PROFILER();
	DEFINE_PROFILER_FUNCTION();

	if (m_pInstance->m_bEnableStartAnimation == 0)
		return 0;

	if (g_pCharacterManager->m_AllowStartOfAnimation == 0)
		return 0;

	if (Params.m_nLayerID >= numVIRTUALLAYERS || Params.m_nLayerID < 0)
		return 0;

	CDefaultSkeleton* pDefaultSkeleton = m_pInstance->m_pDefaultSkeleton;
	const char* RootName = pDefaultSkeleton->m_arrModelJoints[0].m_strJointName.c_str();
	if (RootName == 0)
		return 0;

	CryCharAnimationParams AnimPrams = Params;
	if (AnimPrams.m_nLayerID > 0)
	{
		//	AnimPrams.m_nFlags|=CA_PARTIAL_BODY_UPDATE;
		uint32 loop = AnimPrams.m_nFlags & CA_LOOP_ANIMATION;
		uint32 repeat = AnimPrams.m_nFlags & CA_REPEAT_LAST_KEY;
		if (loop == 0 && repeat == 0)
		{
			AnimPrams.m_nFlags |= CA_REPEAT_LAST_KEY;
			AnimPrams.m_nFlags |= CA_FADEOUT;
		}
	}

	//----------------------------------------------------------------------------
	
	AnimPrams.m_nInterpolationType = CA_Interpolation_Type(g_DefaultTransitionInterpolationType);

	//----------------------------------------------------------------------------

	uint32 nDisableMultilayer = AnimPrams.m_nFlags & CA_DISABLE_MULTILAYER;
	if (nDisableMultilayer)
	{
		AnimPrams.m_fAllowMultilayerAnim = 0.0f;
	}

	//----------------------------------------------------------------------------

	uint32 TrackViewExclusive = Params.m_nFlags & CA_TRACK_VIEW_EXCLUSIVE;
	if (m_TrackViewExclusive && !TrackViewExclusive)
		return 0;

	const ModelAnimationHeader* pAnim = NULL;
	int32 nAnimID = id;

	const ModelAnimationHeader* pAnimAim0 = NULL;
	int32 nAnimAimID0 = -1;
	const ModelAnimationHeader* pAnimAim1 = NULL;
	int32 nAnimAimID1 = -1;

	CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;

	assert(Params.m_fTransTime < 60.0f); //transition times longer than 1 minute are useless

	if (nAnimID < 0)
		return 0;

	//--------------------------------------------------------------
	//---                evaluate the 1st animation              ---
	//--------------------------------------------------------------

	pAnim = pAnimationSet->GetModelAnimationHeader(nAnimID);

	if (pAnim == 0)
	{
		assert(0);
		return 0;
	}

	const char* szAnimName = pAnim->GetAnimName();
	uint32 nGlobalID = pAnim->m_nGlobalAnimId;

	//-----------------------------------------------------------------------------------------------------------------------
	//--- CAF start     -----------------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------------------------------
	if (pAnim->m_nAssetType == CAF_File)
	{
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[nGlobalID];

		uint32 IsCreated = rCAF.IsAssetCreated();
		if (IsCreated == 0)
		{
			if (Console::GetInst().ca_AnimWarningLevel > 0)
				gEnv->pLog->LogError("Asset for animation-name '%s' does not exist for Model %s", pAnim->GetAnimName(), m_pInstance->m_pDefaultSkeleton->GetModelFilePath());
			return 0;
		}
		if (rCAF.m_nControllers2 == 0)
		{
			gEnv->pLog->LogError("Asset for animation-name '%s' has no controllers for Model %s", pAnim->GetAnimName(), m_pInstance->m_pDefaultSkeleton->GetModelFilePath());
			return 0;
		}

		if (rCAF.IsAssetLoaded() == 0 && rCAF.IsAssetRequested() == 0)
			rCAF.StartStreamingCAF();
	}

	//-----------------------------------------------------------------------------------------------------------------------
	//--- CAF start     -----------------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------------------------------
	if (pAnim->m_nAssetType == AIM_File)
	{
		GlobalAnimationHeaderAIM& rAIM = g_AnimationManager.m_arrGlobalAIM[nGlobalID];
		uint32 IsCreated = rAIM.IsAssetCreated();
		if (IsCreated == 0)
		{
			if (Console::GetInst().ca_AnimWarningLevel > 0)
				gEnv->pLog->LogError("Asset for animation-name '%s' does not exist for Model %s", pAnim->GetAnimName(), m_pInstance->m_pDefaultSkeleton->GetModelFilePath());
			return 0;
		}

		if (rAIM.IsAimposeUnloaded())
		{
			CPoseBlenderAim* pPBAim = static_cast<CPoseBlenderAim*>(m_pSkeletonPose->m_PoseBlenderAim.get());
			if (pPBAim)
			{
				uint32 nLayerAim = pPBAim->m_blender.m_Set.nDirLayer;
				if (Params.m_nLayerID != nLayerAim)
				{
					if (Console::GetInst().ca_AnimWarningLevel > 0)
						gEnv->pLog->LogError("trying to play an unloaded Aim-Pose '%s'  for Model %s", szAnimName, m_pInstance->m_pDefaultSkeleton->GetModelFilePath());
					return 0;
				}
			}

			CPoseBlenderLook* pPBLook = static_cast<CPoseBlenderLook*>(m_pSkeletonPose->m_PoseBlenderLook.get());
			if (pPBLook)
			{
				uint32 nLayerLook = pPBLook->m_blender.m_Set.nDirLayer;
				if (Params.m_nLayerID != nLayerLook)
				{
					if (Console::GetInst().ca_AnimWarningLevel > 0)
						gEnv->pLog->LogError("trying to play an unloaded Look-Pose '%s'  for Model %s", szAnimName, m_pInstance->m_pDefaultSkeleton->GetModelFilePath());
					return 0;
				}
			}
		}
	}

	//-----------------------------------------------------------------------------------------------------------------------
	//--- LMG start     -----------------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------------------------------
	if (pAnim->m_nAssetType == LMG_File)
	{
		GlobalAnimationHeaderLMG& rLMG = g_AnimationManager.m_arrGlobalLMG[nGlobalID];
		assert(rLMG.IsAssetLMG());
		uint32 IsCreated = rLMG.IsAssetCreated();
		if (IsCreated == 0)
		{
			if (Console::GetInst().ca_AnimWarningLevel > 0)
				gEnv->pLog->LogError("Asset for animation-name '%s' does not exist for Model: %s", pAnim->GetAnimName(), m_pInstance->m_pDefaultSkeleton->GetModelFilePath());
			return 0;
		}
#if BLENDSPACE_VISUALIZATION
		if (rLMG.IsAssetInternalType())
		{
			uint32 n0 = rLMG.m_arrParameter[0].m_animName.m_name.size();
			uint32 n1 = rLMG.m_arrParameter[1].m_animName.m_name.size();
			if (n0 == 0 || n1 == 0)
				return 0; //EXIT. Animation-names for internal LMG-Type not specified
		}
#endif

		if (rLMG.IsAssetLMGValid() == 0)
		{
			ANIM_ASSET_CHECK_TRACE(rLMG.IsAssetLMGValid() != 0, ("Blendspace is invalid: '%s'", szAnimName));
			return 0;
		}

		uint32 numLMGAnims = rLMG.m_numExamples; //m_arrBSAnimations2.size();
		for (uint32 i = 0; i < numLMGAnims; i++)
		{
			int local = pAnimationSet->GetAnimIDByCRC(rLMG.m_arrParameter[i].m_animName.m_CRC32);
			int global = pAnimationSet->GetGlobalIDByAnimID(local);
			if (global != -1)
			{
				GlobalAnimationHeaderCAF& rGAH = g_AnimationManager.m_arrGlobalCAF[global];
				if (rGAH.IsAssetLoaded() == 0 && rGAH.IsAssetRequested() == 0)
					rGAH.StartStreamingCAF();
			}
		}
	}

	bool result = AnimationToQueue(pAnim, nAnimID, -1, AnimPrams) > 0;

	if (result)
	{
		if (TrackViewExclusive)
		{
			int32 Layer = Params.m_nLayerID;
			int32 numAnims = m_layers[Layer].m_transitionQueue.m_animations.size() - 1;
			for (int32 i = 0; i < numAnims; i++)
			{
				m_layers[Layer].m_transitionQueue.m_animations[i].m_fTransitionTime = fabsf(m_layers[Layer].m_transitionQueue.m_animations[i].m_fTransitionTime * 0.01f);
			}
		}

		m_layers[Params.m_nLayerID].m_transitionQueue.m_fLayerTransitionTime = fabsf(Params.m_fTransTime);
	}

	return result;
}

// stops the animation at the given layer, and returns true if the animation was
// actually stopped (if the layer existed and the animation was played there)
bool CSkeletonAnim::StopAnimationInLayer(int32 nLayer, f32 fBlendOutTime)
{
	if (nLayer < 0 || nLayer >= numVIRTUALLAYERS)
	{
		gEnv->pLog->LogError("illegal layer index used in function StopAnimationInLayer: '%d'  for Model: %s", nLayer, m_pInstance->m_pDefaultSkeleton->GetModelFilePath());
		assert(0);
		return 0;
	}

	//	const char* mname = m_pInstance->GetFilePath();
	//	if ( strcmp(mname,"objects/library/architecture/multiplayer/prototype_factory/pf_factory_gate.cga")==0 )
	//		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,mname,	"StopAnimationInLayer");
	SetActiveLayer(nLayer, 0);
	m_layers[nLayer].m_transitionQueue.m_fLayerTransitionTime = fBlendOutTime;
	if (nLayer)
		return 1;

	ClearFIFOLayer(nLayer);
	return 1;
}

// stops the animation at the given layer, and returns true if the animation was
// actually stopped (if the layer existed and the animation was played there)
bool CSkeletonAnim::StopAnimationsAllLayers()
{
	//	const char* mname = m_pInstance->GetFilePath();
	//	if ( strcmp(mname,"objects/library/architecture/multiplayer/prototype_factory/pf_factory_gate.cga")==0 )
	//		g_pISystem->Warning( VALIDATOR_MODULE_ANIMATION,VALIDATOR_WARNING, VALIDATOR_FLAG_FILE,mname,	"StopAnimationsAllLayers");

	for (uint32 i = 0; i < numVIRTUALLAYERS; i++)
		ClearFIFOLayer(i);

	return 1;
}

uint32 CSkeletonAnim::AnimationToQueue(const ModelAnimationHeader* pAnim, int nAnimID, f32 btime, const CryCharAnimationParams& AnimParams)
{
	const int MAX_ANIMATIONS_IN_QUEUE = 0x10;

	const uint32 numAnimsInQueue = m_layers[AnimParams.m_nLayerID].m_transitionQueue.m_animations.size();
	if (numAnimsInQueue >= MAX_ANIMATIONS_IN_QUEUE)
	{
		gEnv->pLog->LogError("Animation-queue overflow. More then %u entries. This is a serious performance problem. Model: %s. CharInst: %p.", numAnimsInQueue, m_pInstance->m_pDefaultSkeleton->GetModelFilePath(), m_pInstance);
		return 0;
	}

	CDefaultSkeleton* pDefaultSkeleton = m_pInstance->m_pDefaultSkeleton;
	CAnimationSet* pAnimationSet = pDefaultSkeleton->m_pAnimationSet;
	uint32 status = 0;

	if (pAnim)
	{
		uint32 nGlobalAnimId = pAnim->m_nGlobalAnimId;
		if (pAnim->m_nAssetType == LMG_File)
		{
			GlobalAnimationHeaderLMG& rLMG = g_AnimationManager.m_arrGlobalLMG[nGlobalAnimId];
			if (rLMG.IsAssetLMG())
				status |= 1;
			rLMG.m_nTouchedCounter++;
		}

	}

	uint32 aar = AnimParams.m_nFlags & CA_ALLOW_ANIM_RESTART;
	if (aar == 0)
	{
		//don't allow to start the same animation twice
		uint32 numAnimsOnStack = m_layers[AnimParams.m_nLayerID].m_transitionQueue.m_animations.size();
		if (numAnimsOnStack)
		{
			//check is it the same animation only if the aim poses are different
			const CAnimation& lastAnimation = m_layers[AnimParams.m_nLayerID].m_transitionQueue.m_animations[numAnimsOnStack - 1];
			int32 id0 = lastAnimation.GetAnimationId();
			if (0 <= id0)
				if (id0 == nAnimID)
					return 0;
		}
	}

	if (AnimParams.m_nFlags & CA_REMOVE_FROM_FIFO)
	{
		RemoveAnimFromFIFO(AnimParams.m_nLayerID, 0, true);
	}

	CAnimation AnimOnStack;
	if (pAnim)
	{
		AnimOnStack.m_animationId = nAnimID;
		if (pAnim->m_nAssetType == LMG_File)
		{
			GlobalAnimationHeaderLMG& rLMG = g_AnimationManager.m_arrGlobalLMG[pAnim->m_nGlobalAnimId];
			assert(rLMG.IsAssetLMG());
			if (rLMG.IsAssetLMG() == 0)
				return 0; //--- probably data mismatch

			AnimOnStack.m_pParametricSampler = AllocateRuntimeParametric();
			SParametricSamplerInternal* pParametricSampler = (SParametricSamplerInternal*)AnimOnStack.GetParametricSampler();
			if (pParametricSampler == 0)
				return 0; //--- Paranoia exit, empty pool

			pParametricSampler->m_numDimensions = uint8(rLMG.m_Dimensions);
			pParametricSampler->m_nParametricType = 0xff;
			for (uint32 d = 0; d < rLMG.m_Dimensions; d++)
			{
				pParametricSampler->m_MotionParameterID[d] = rLMG.m_DimPara[d].m_ParaID;
				pParametricSampler->m_MotionParameterFlags[d] = rLMG.m_DimPara[d].m_nDimensionFlags;
			}

			if (rLMG.m_numExamples)
			{
				pParametricSampler->m_numExamples = int32(rLMG.m_numExamples);
				for (uint32 i = 0; i < pParametricSampler->m_numExamples; i++)
				{
					uint32 nLMGAnimID = pAnimationSet->GetAnimIDByCRC(rLMG.m_arrParameter[i].m_animName.m_CRC32);
					pParametricSampler->m_nAnimID[i] = nLMGAnimID;
					pParametricSampler->m_fPlaybackScale[i] = rLMG.m_arrParameter[i].m_fPlaybackScale;
					pParametricSampler->m_nLMGAnimIdx[i] = i;
					pParametricSampler->m_nSegmentCounter[0][i] = 0;
					pParametricSampler->m_nSegmentCounter[1][i] = 0;
					const ModelAnimationHeader* pAnimHeader = pAnimationSet->GetModelAnimationHeader(nLMGAnimID);
					if (pAnimHeader == 0)
					{
						gEnv->pLog->LogError("Missing blendspace example animation while starting playback: '%s' for BSpace: '%s', Model '%s'", rLMG.m_arrParameter[i].m_animName.GetName_DEBUG(), rLMG.GetFilePath(), m_pInstance->GetFilePath());
						return 0;
					}
				}
			}
			else
			{
				uint32 totalExamples = 0;

				uint32 numBlendSpaces = rLMG.m_arrCombinedBlendSpaces.size();
				pParametricSampler->m_numExamples = 0;
				for (uint32 bs = 0; bs < numBlendSpaces; bs++)
				{
					int32 nGlobalID = rLMG.m_arrCombinedBlendSpaces[bs].m_ParaGroupID;
					if (nGlobalID >= 0)
					{
						GlobalAnimationHeaderLMG& rsubLMG = g_AnimationManager.m_arrGlobalLMG[nGlobalID];
						if (totalExamples + rsubLMG.m_numExamples >= MAX_LMG_EXAMPLES)
						{
							CryFatalError("Exceeding max examples in %s, CRC %u", pAnim->GetAnimName(), pAnim->m_CRC32Name);
						}

						for (uint32 i = 0; i < rsubLMG.m_numExamples; i++)
						{
							uint32 nLMGAnimID = pAnimationSet->GetAnimIDByCRC(rsubLMG.m_arrParameter[i].m_animName.m_CRC32);
							f32 fPlaybackScale = rsubLMG.m_arrParameter[i].m_fPlaybackScale;
							uint32 found = 0;
							for (uint32 g = 0; g < pParametricSampler->m_numExamples; g++)
							{
								if (pParametricSampler->m_nAnimID[g] == nLMGAnimID && pParametricSampler->m_fPlaybackScale[g] == fPlaybackScale)
								{
									pParametricSampler->m_nLMGAnimIdx[totalExamples++] = g;
									found = 1;
									break; //already in list
								}
							}

							if (found == 0)
							{
								pParametricSampler->m_nAnimID[pParametricSampler->m_numExamples] = nLMGAnimID;
								pParametricSampler->m_fPlaybackScale[pParametricSampler->m_numExamples] = fPlaybackScale;
								pParametricSampler->m_nLMGAnimIdx[totalExamples++] = pParametricSampler->m_numExamples;
								pParametricSampler->m_numExamples++;
							}
						}
					}
				}
			}

		}

	}

	SParametricSamplerInternal* pParametric = (SParametricSamplerInternal*)AnimOnStack.GetParametricSampler();
	if (pParametric == NULL)
	{
		AnimOnStack.m_currentSegmentIndex[0] = 0;
		AnimOnStack.m_currentSegmentIndex[1] = 0;
		const ModelAnimationHeader* pAnimHdr = pAnimationSet->GetModelAnimationHeader(nAnimID);
		if (pAnimHdr->m_nAssetType == CAF_File)
		{
			int32 GlobalID = pAnimationSet->GetGlobalIDByAnimID_Fast(nAnimID);
			GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[GlobalID];
			rCAF.m_nTouchedCounter++;
			rCAF.m_nRef_at_Runtime++;
			AnimOnStack.SetExpectedTotalDurationSeconds(rCAF.m_fTotalDuration);

			if (!rCAF.IsAssetOnDemand() && rCAF.IsAssetLoaded())
			{
				//The DBA is in memory. Just reset the DBA counter
				size_t numDBA_Files = g_AnimationManager.m_arrGlobalHeaderDBA.size();
				for (uint32 d = 0; d < numDBA_Files; d++)
				{
					CGlobalHeaderDBA& pGlobalHeaderDBA = g_AnimationManager.m_arrGlobalHeaderDBA[d];
					if (rCAF.m_FilePathDBACRC32 != pGlobalHeaderDBA.m_FilePathDBACRC32)
						continue;
					if (pGlobalHeaderDBA.m_pDatabaseInfo)
						pGlobalHeaderDBA.m_nLastUsedTimeDelta = 0;
				}
			}
		}
	}
	else
	{
		// AnimParams is not initialized in the constructor anymore.
		// BuildRealTimeLMG does NOT initialize AnimParams implicitly, for cases where the caller overwrite stuff anyway.
		AnimOnStack.m_currentSegmentIndex[0] = -1; //for BSpaces always -1
		AnimOnStack.m_currentSegmentIndex[1] = -1; //for BSpaces always -1
		const uint32 numAnims0 = pParametric->m_numExamples;
		for (uint32 i = 0; i < numAnims0; i++)
		{
			const int32 nParaAnimID = pParametric->m_nAnimID[i];
			const ModelAnimationHeader* pAnimHdr = pAnimationSet->GetModelAnimationHeader(nParaAnimID);
			if (pAnimHdr && pAnimHdr->m_nAssetType == CAF_File)
			{
				int32 GlobalID = pAnimationSet->GetGlobalIDByAnimID_Fast(pParametric->m_nAnimID[i]);
				GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[GlobalID];
				rCAF.m_nTouchedCounter++;
				rCAF.m_nRef_at_Runtime++;

				if (!rCAF.IsAssetOnDemand() && rCAF.IsAssetLoaded())
				{
					//The DBA is in memory. Just reset the DBA counter
					size_t numDBA_Files = g_AnimationManager.m_arrGlobalHeaderDBA.size();
					for (uint32 d = 0; d < numDBA_Files; d++)
					{
						CGlobalHeaderDBA& pGlobalHeaderDBA = g_AnimationManager.m_arrGlobalHeaderDBA[d];
						if (rCAF.m_FilePathDBACRC32 != pGlobalHeaderDBA.m_FilePathDBACRC32)
							continue;
						if (pGlobalHeaderDBA.m_pDatabaseInfo)
							pGlobalHeaderDBA.m_nLastUsedTimeDelta = 0;
					}
				}
			}
		}
	}

	AnimOnStack.m_nStaticFlags = AnimParams.m_nFlags;                 // Combination of flags defined above.
	AnimOnStack.m_DynFlags[0] = 0;

	AnimOnStack.m_fStartTime = clamp_tpl(AnimParams.m_fKeyTime, 0.0f, 1.0f);          //normalized time to start a transition animation.
	AnimOnStack.m_fAnimTime[0] = AnimOnStack.m_fStartTime;
	AnimOnStack.m_fAnimTimePrev[0] = AnimOnStack.m_fStartTime;
	AnimOnStack.m_fTransitionTime = max(0.0f, AnimParams.m_fTransTime);         //transition time between two animations. Negative values are not allowed
	AnimOnStack.m_fTransitionPriority = 0.0f;
	AnimOnStack.SetTransitionWeight(0.0f);
	AnimOnStack.m_nInterpolationType = AnimParams.m_nInterpolationType;
	AnimOnStack.m_fPlaybackScale = max(0.0f, AnimParams.m_fPlaybackSpeed);      // multiplier for animation-update. Negative values are not allowed
	AnimOnStack.m_fPlaybackWeight = AnimParams.m_fPlaybackWeight;
#if defined(USE_PROTOTYPE_ABS_BLENDING)
	AnimOnStack.m_pJointMask = AnimParams.m_pJointMask;
#endif //!defined(USE_PROTOTYPE_ABS_BLENDING)

	AnimOnStack.m_nUserToken = AnimParams.m_nUserToken;
	for (int i = 0; i < NUM_ANIMATION_USER_DATA_SLOTS; i++)
		AnimOnStack.m_fUserData[i] = AnimParams.m_fUserData[i];

	AppendAnimationToQueue(AnimParams.m_nLayerID, AnimOnStack);

	// only report when an animation fills the queue to prevent spam
	if (numAnimsInQueue == MAX_ANIMATIONS_IN_QUEUE - 1)
	{
		if (Console::GetInst().ca_AnimWarningLevel >= 2)
		{
			gEnv->pLog->LogError("Animation-queue filled up to %d entries. This is a serious performance problem. Model: %s. CharInst: %p.", MAX_ANIMATIONS_IN_QUEUE, m_pInstance->m_pDefaultSkeleton->GetModelFilePath(), m_pInstance);
			DebugLogTransitionQueueState();
		}
	}

	return 1;
}

//---------------------------------------------------------------------------------

const CAnimation* CSkeletonAnim::FindAnimInFIFO(uint32 nUserToken, int nLayer) const
{
	int startLayer = 0;
	int endLayer = numVIRTUALLAYERS;
	if (nLayer >= 0)
	{
		assert(nLayer >= 0 && nLayer < numVIRTUALLAYERS);
		startLayer = nLayer;
		endLayer = nLayer + 1;
	}

	for (int lyr = startLayer; lyr < endLayer; lyr++)
	{
		const DynArray<CAnimation>& fifo = m_layers[lyr].m_transitionQueue.m_animations;
		for (int i = 0, num = (int)fifo.size(); i < num; i++)
		{
			if (fifo[i].HasUserToken(nUserToken))
			{
				return &fifo[i];
			}
		}
	}

	return NULL;
}

CAnimation* CSkeletonAnim::FindAnimInFIFO(uint32 nUserToken, int nLayer)
{
	const CSkeletonAnim* pConstThis = this;
	return const_cast<CAnimation*>(pConstThis->FindAnimInFIFO(nUserToken, nLayer));
}
