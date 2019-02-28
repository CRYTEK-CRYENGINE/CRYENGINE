// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SkeletonAnim.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include "CharacterInstance.h"
#include "Model.h"
#include "CharacterManager.h"
#include "ParametricSampler.h"

// Function used to display animation flags as text
const char* AnimFlags(uint32 flags)
{
	static char result[33];

	/*
	 + CA_FORCE_TRANSITION_TO_ANIM	=0x008000
	   y	CA_FULL_ROOT_PRIORITY				=0x004000
	   b	CA_REMOVE_FROM_FIFO					=0x002000
	   V	CA_TRACK_VIEW_EXCLUSIVE     =0x001000
	   F	CA_FORCE_SKELETON_UPDATE    =0x000800
	   x	CA_DISABLE_MULTILAYER       =0x000400
	   3 CA_KEYFRAME_SAMPLE_30Hz			=0x000200
	   n	CA_ALLOW_ANIM_RESTART				=0x000100
	   S	CA_MOVE2IDLE								=0x000080
	   I	CA_IDLE2MOVE								=0x000040
	   A	CA_START_AFTER	            =0x000020
	   K	CA_START_AT_KEYTIME					=0x000010
	   T	CA_TRANSITION_TIMEWARPING		=0x000008
	   R	CA_REPEAT_LAST_KEY					=0x000004
	   L	CA_LOOP_ANIMATION						=0x000002
	   M	CA_MANUAL_UPDATE						=0x000001
	 */

	const char codes[] = "+ybVFx3nSIAKTRLM";
	for (int i = 0; result[i] = codes[i]; ++i)
		if ((flags & (1 << (sizeof(codes) - i - 2))) == 0)
			result[i] = '-';
	return result;
}

/////////////////////////////////////////////////////////////////////////////////
// this function is handling animation-update, interpolation and transitions   //
/////////////////////////////////////////////////////////////////////////////////
uint32 CSkeletonAnim::BlendManager(f32 fDeltaTime, DynArray<CAnimation>& arrAFIFO, uint32 nLayer)
{
	DEFINE_PROFILER_FUNCTION();

	uint32 NumAnimsInQueue = arrAFIFO.size();
	assert(NumAnimsInQueue > 0);
	assert(nLayer < numVIRTUALLAYERS);

	CAnimation* arrAnimFiFo = &arrAFIFO[0];
	SParametricSamplerInternal* pParametric = (SParametricSamplerInternal*)arrAnimFiFo[0].GetParametricSampler();

	//the first animation in the queue MUST be activated
	if (!arrAnimFiFo[0].IsActivated())
	{
		uint32 allLoaded = 1;
		CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;
		if (pParametric == NULL)
		{
			allLoaded = CheckIsCAFLoaded(pAnimationSet, arrAnimFiFo[0].GetAnimationId());
		}
		else
		{
			uint32 counter = 0;
			uint32 num = pParametric->m_numExamples;
			for (uint32 a = 0; a < num; a++)
			{
				int32 nAnimID = pParametric->m_nAnimID[a];
				allLoaded &= CheckIsCAFLoaded(pAnimationSet, nAnimID);
			}
		}

		if (allLoaded)
			arrAnimFiFo[0].m_DynFlags[0] |= CA_ACTIVATED;
	}

	if (!arrAnimFiFo[0].IsActivated())
		return 0;

	//	uint32 nMaxActiveInQueue=2;
	uint32 nMaxActiveInQueue = MAX_EXEC_QUEUE;

	if (nMaxActiveInQueue > NumAnimsInQueue)
		nMaxActiveInQueue = NumAnimsInQueue;

	if (NumAnimsInQueue > nMaxActiveInQueue)
		NumAnimsInQueue = nMaxActiveInQueue;

	uint32 aq = EvaluateTransitionFlags(arrAnimFiFo, NumAnimsInQueue);

	//-----------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------

	if (aq > nMaxActiveInQueue)
		aq = nMaxActiveInQueue;

	nMaxActiveInQueue = aq;

#ifdef _DEBUG
	for (uint32 a = 0; a < nMaxActiveInQueue; a++)
	{
		assert(arrAnimFiFo[a].IsActivated());
		f32 t = arrAnimFiFo[a].m_fAnimTime[0];
		assert(t >= 0.0f && t <= 1.0f);  //most likely this is coming from outside if CryAnimation
	}
#endif

	if (!m_TrackViewExclusive)
	{
		m_layers[nLayer].m_transitionQueue.TransitionsBetweenSeveralAnimations(nMaxActiveInQueue);
		m_layers[nLayer].m_transitionQueue.UpdateTransitionTime(nMaxActiveInQueue, fDeltaTime, m_TrackViewExclusive, m_pInstance->m_fOriginalDeltaTime);
		m_layers[nLayer].m_transitionQueue.AdjustTransitionWeights(nMaxActiveInQueue);
	}
	else
		m_layers[nLayer].m_transitionQueue.ApplyManualMixingWeight(nMaxActiveInQueue);

#ifdef _DEBUG
	for (uint32 j = 0; j < nMaxActiveInQueue; j++)
	{
		if (arrAnimFiFo[j].GetTransitionWeight() < 0.0f)
			assert(0);
		if (arrAnimFiFo[j].GetTransitionWeight() > 1.0f)
			assert(0);
		if (arrAnimFiFo[j].GetTransitionPriority() < 0.0f)
			assert(0);
		if (arrAnimFiFo[j].GetTransitionPriority() > 1.0f)
			assert(0);
	}
#endif

	f32 TotalWeights = 0.0f;
	for (uint32 i = 0; i < nMaxActiveInQueue; i++)
		TotalWeights += arrAnimFiFo[i].GetTransitionWeight();
	//		assert( fabsf(TotalWeights-1.0f) < 0.001f );

	f32 fTotal = fabsf(TotalWeights - 1.0f);
	if (fTotal > 0.01f)
	{
		assert(!"sum of transition-weights is wrong");
		CryFatalError("CryAnimation: sum of transition-weights is wrong: %f %s", fTotal, m_pInstance->m_pDefaultSkeleton->GetModelFilePath());
	}

	for (uint32 a = 0; a < nMaxActiveInQueue; a++)
	{
		const CAnimation& rCurAnim = arrAnimFiFo[a];
		assert(rCurAnim.IsActivated());
		if (rCurAnim.HasStaticFlag(CA_FORCE_SKELETON_UPDATE))
		{
			m_pSkeletonPose->m_bFullSkeletonUpdate = 1; //This means we have to do a full-skeleton update
			//	float fColor[4] = {1,1,0,1};
			//	g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.7f, fColor, false,"CA_FORCE_SKELETON_UPDATE" );
			//	g_YLine+=16.0f;
		}
	}

	UpdateParameters(arrAnimFiFo, nMaxActiveInQueue, nLayer, fDeltaTime);

	m_layers[nLayer].m_transitionQueue.AdjustAnimationTimeForTimeWarpedAnimations(nMaxActiveInQueue);

	//update AnimTime for all active animation in the queue
	for (uint32 a = 0; a < nMaxActiveInQueue; a++)
	{
		CAnimation& rAnimation = arrAnimFiFo[a];
		assert(rAnimation.IsActivated());

		UpdateAnimationTime(rAnimation, nLayer, NumAnimsInQueue, a, 0);

		rAnimation.m_currentSegmentIndex[1] = rAnimation.m_currentSegmentIndex[0];
		rAnimation.m_fAnimTime[1] = rAnimation.m_fAnimTime[0];
		UpdateAnimationTime(rAnimation, nLayer, NumAnimsInQueue, a, 1);

		AnimCallback(rAnimation);
	}

	return 0;
}

uint32 CSkeletonAnim::CheckIsCAFLoaded(CAnimationSet* pAnimationSet, int32 nAnimID)
{
	const ModelAnimationHeader* pAnimCAF = pAnimationSet->GetModelAnimationHeader(nAnimID);
	if (pAnimCAF == 0)
		return 0;

	if (pAnimCAF->m_nAssetType == CAF_File)
	{
		int32 nGlobalID = pAnimationSet->GetGlobalIDByAnimID_Fast(nAnimID);
		GlobalAnimationHeaderCAF& rGAHeader = g_AnimationManager.m_arrGlobalCAF[nGlobalID];

		rGAHeader.ConnectCAFandDBA();
		uint32 loaded = rGAHeader.IsAssetLoaded();
		if (loaded == 0)
			rGAHeader.StartStreamingCAF();

		return (loaded != 0);
	}

	if (pAnimCAF->m_nAssetType == AIM_File)
		return 1;

	return 0;
}

void CSkeletonAnim::UpdateParameters(CAnimation* arrAnimFiFo, uint32 nMaxActiveInQueue, uint32 nLayer, f32 fFrameDeltaTime)
{
	DEFINE_PROFILER_FUNCTION();

	CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;
	for (uint32 a = 0; a < nMaxActiveInQueue; a++)
	{
		CAnimation& rCurAnim = arrAnimFiFo[a];
		assert(rCurAnim.IsActivated());

		SParametricSamplerInternal* pParametric = (SParametricSamplerInternal*)rCurAnim.GetParametricSampler();
		if (pParametric)
		{
			//this is a regular parametric group

#if BLENDSPACE_VISUALIZATION
			if (nMaxActiveInQueue != 1 && Console::GetInst().ca_DrawVEGInfo && m_pInstance->GetCharEditMode() && g_pCharacterManager->HasAnyDebugInstancesCreated())
			{
				uint32 nMaxDebugInstances = g_pCharacterManager->m_arrCharacterBase.size();
				for (uint32 i = 0; i < nMaxDebugInstances; i++)
				{
					ISkeletonAnim* pISkeletonAnim = g_pCharacterManager->m_arrCharacterBase[i].m_pInst->GetISkeletonAnim();
					pISkeletonAnim->StopAnimationsAllLayers();
					g_pCharacterManager->m_arrCharacterBase[i].m_GridLocation.q.SetRotationZ(-gf_PI * 0.5f);
					g_pCharacterManager->m_arrCharacterBase[i].m_GridLocation.t = Vec3(ZERO);
				}
			}
#endif

			//Update the Blend Weights for this Parametric Group and do return the time-warped delta time.
			bool nAllowDebugging = nMaxActiveInQueue == 1 && Console::GetInst().ca_DrawVEGInfo && m_pInstance->m_CharEditMode;
			rCurAnim.m_fCurrentDeltaTime = pParametric->Parameterizer(pAnimationSet, m_pInstance->m_pDefaultSkeleton, rCurAnim, fFrameDeltaTime, m_pInstance->m_fPlaybackScale, nAllowDebugging);
			rCurAnim.m_fCurrentDeltaTime *= rCurAnim.m_fPlaybackScale;

			f32 fTWDuration = 0.0f;
			f32 totalTime = 0.0f;
			for (uint32 i = 0; i < pParametric->m_numExamples; i++)
			{
				if (pParametric->m_fBlendWeight[i] == 0)
					continue;
				int32 nAnimIDPS = pParametric->m_nAnimID[i];
				if (nAnimIDPS < 0)
					CryFatalError("CryAnimation: negative");
				const ModelAnimationHeader* pAnimPS = pAnimationSet->GetModelAnimationHeader(nAnimIDPS);
				GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pAnimPS->m_nGlobalAnimId];
				int32 segcount = pParametric->m_nSegmentCounter[0][i];
				f32 fSegDuration = rCAF.GetSegmentDuration(segcount);
				fSegDuration = max(fSegDuration, 1.0f / ANIMATION_30Hz);
				fTWDuration += pParametric->m_fBlendWeight[i] * fSegDuration;

				const uint32 totSegs = rCAF.GetTotalSegments();
				for (uint32 s = 0; s < totSegs; s++)
				{
					f32 fCurSegDuration = rCAF.GetSegmentDuration(s);
					fCurSegDuration = max(fCurSegDuration, 1.0f / ANIMATION_30Hz);
					totalTime += pParametric->m_fBlendWeight[i] * fCurSegDuration;
				}
			}
			if (rCurAnim.m_fCurrentDeltaTime > 0.0f)
			{
				totalTime *= (fFrameDeltaTime / (rCurAnim.m_fCurrentDeltaTime * fTWDuration));
			}
			const float expectedSegmentDuration = max(0.0001f, fTWDuration);
			rCurAnim.SetCurrentSegmentExpectedDurationSeconds(expectedSegmentDuration);
			const float expectedTotalDuration = max(0.0001f, totalTime);
			rCurAnim.SetExpectedTotalDurationSeconds(expectedTotalDuration);

		}
		else
		{
			//this is a regular CAF
			const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(rCurAnim.GetAnimationId());
			assert(pAnim);
			if (pAnim->m_nAssetType == CAF_File)
			{
				GlobalAnimationHeaderCAF& rGAH = g_AnimationManager.m_arrGlobalCAF[pAnim->m_nGlobalAnimId];
				const int32 segcount = rCurAnim.m_currentSegmentIndex[0];
				f32 fSegTime = rGAH.GetSegmentDuration(segcount);
				fSegTime = max(fSegTime, 1.0f / ANIMATION_30Hz);
				rCurAnim.SetCurrentSegmentExpectedDurationSeconds(fSegTime);
				rCurAnim.m_fCurrentDeltaTime = (rCurAnim.m_fPlaybackScale * fFrameDeltaTime) / fSegTime;
			}
			if (pAnim->m_nAssetType == AIM_File)
			{
				GlobalAnimationHeaderAIM& rAIM = g_AnimationManager.m_arrGlobalAIM[pAnim->m_nGlobalAnimId];
				const int32 segcount = rCurAnim.m_currentSegmentIndex[0];
				f32 fSegTime = max(rAIM.m_fTotalDuration, 1.0f / ANIMATION_30Hz);
				fSegTime = max(fSegTime, 1.0f / ANIMATION_30Hz);
				rCurAnim.SetCurrentSegmentExpectedDurationSeconds(fSegTime);
				rCurAnim.m_fCurrentDeltaTime = (rCurAnim.m_fPlaybackScale * fFrameDeltaTime) / fSegTime;
			}
		}
	}
}

void CSkeletonAnim::UpdateAnimationTime(CAnimation& rAnimation, uint32 nLayer, uint32 NumAnimsInQueue, uint32 AnimNo, uint32 idx)
{
	DEFINE_PROFILER_FUNCTION();

	assert(rAnimation.m_fAnimTime[idx] <= 2.0f);

	const bool ManualUpdate = rAnimation.HasStaticFlag(CA_MANUAL_UPDATE);
	if (ManualUpdate)
	{
		rAnimation.m_fCurrentDeltaTime = 0.0f;
		if ((rAnimation.GetTransitionWeight() == 0.0f) && (AnimNo == 0))
		{
			if (m_TrackViewExclusive == 0)
				rAnimation.m_DynFlags[idx] |= CA_REMOVE_FROM_QUEUE;
		}
		return;
	}

	//---------------------------------------------------------------------------
	const bool LoopAnimation = rAnimation.HasStaticFlag(CA_LOOP_ANIMATION);
	bool RepeatLastKey = rAnimation.HasStaticFlag(CA_REPEAT_LAST_KEY);
	if (LoopAnimation)
		RepeatLastKey = false;

	rAnimation.m_fCurrentDeltaTime = max(rAnimation.m_fCurrentDeltaTime, 0.0f);     //nagative delta-time not allowed any more

	SParametricSamplerInternal* pParametric = (SParametricSamplerInternal*)rAnimation.GetParametricSampler();
	if (pParametric)
	{
		for (uint32 i = 0; i < pParametric->m_numExamples; i++)
			pParametric->m_nSegmentCounterPrev[idx][i] = pParametric->m_nSegmentCounter[idx][i];
	}
	rAnimation.m_currentSegmentIndexPrev[idx] = rAnimation.m_currentSegmentIndex[idx];
	rAnimation.m_fAnimTimePrev[idx] = rAnimation.m_fAnimTime[idx];
	rAnimation.m_fAnimTime[idx] = rAnimation.m_fAnimTime[idx] + rAnimation.m_fCurrentDeltaTime;

	rAnimation.m_DynFlags[idx] &= ~CA_EOC; //always delete
	rAnimation.m_DynFlags[idx] &= ~CA_LOOPED_THIS_UPDATE;
	rAnimation.m_DynFlags[idx] &= ~CA_NEGATIVE_EOC; //always delete
	CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;

	int numLoops = (int)rAnimation.m_fAnimTime[idx];
	if (numLoops > 0)
	{
		rAnimation.m_fAnimTime[idx] = rAnimation.m_fAnimTime[idx] - ((float)numLoops);  //warp fAnimTime when time is running forward
		assert(rAnimation.m_fAnimTime[idx] >= 0.0f && rAnimation.m_fAnimTime[idx] <= 1.0f);
		rAnimation.m_DynFlags[idx] |= CA_EOC;
		rAnimation.m_DynFlags[idx] |= CA_LOOPED; //..and set loop flag

		if (LoopAnimation == 0)
		{
			if (pParametric)
			{
				uint32 nClamped = 0;
				const uint32 numAnims = pParametric->m_numExamples;
				for (uint32 i = 0; i < numAnims; i++)
				{
					const int32 nAnimID = pParametric->m_nAnimID[i];
					const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(nAnimID);
					if (pAnim->m_nAssetType != CAF_File)  CryFatalError("CryAnimation: weird error");   //can happen only in weird cases (mem-corruption, etc)
					GlobalAnimationHeaderCAF& rParaCAF = g_AnimationManager.m_arrGlobalCAF[pAnim->m_nGlobalAnimId];
					if (++pParametric->m_nSegmentCounter[idx][i] >= rParaCAF.m_Segments)
					{
						pParametric->m_nSegmentCounter[idx][i] = rParaCAF.m_Segments - 1;
						nClamped++;
					}
					//float fColor[4] = {1,1,0,1};
					//g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.7f, fColor, false,"SegCounter: %d  idx: %d  nClamped: %d",pParametric->m_nSegmentCounter[idx][i],idx,nClamped );
					//g_YLine+=16.0f;
				}

				if (nClamped == numAnims)
				{
					rAnimation.m_DynFlags[idx] &= ~CA_EOC; //always delete
					rAnimation.m_fAnimTime[idx] = 1.0f;    //clamp time
					if (RepeatLastKey)
						rAnimation.m_DynFlags[idx] |= CA_REPEAT; //...and set the repeat-mode;
					else
						rAnimation.m_DynFlags[idx] |= CA_REMOVE_FROM_QUEUE;
				}
			}
			else
			{
				const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(rAnimation.m_animationId);
				if (pAnim->m_nAssetType == CAF_File)
				{
					GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pAnim->m_nGlobalAnimId];
					if (++rAnimation.m_currentSegmentIndex[idx] >= rCAF.m_Segments)
					{
						rAnimation.m_currentSegmentIndex[idx] = rCAF.m_Segments - 1; //clamp segments to the highest
						rAnimation.m_fAnimTime[idx] = 1.0f;                          //clamp time
						rAnimation.m_DynFlags[idx] &= ~CA_EOC;                       //always delete
						if (RepeatLastKey)
							rAnimation.m_DynFlags[idx] |= CA_REPEAT; //...and set the repeat-mode;
						else
							rAnimation.m_DynFlags[idx] |= CA_REMOVE_FROM_QUEUE;
					}
				}
				if (pAnim->m_nAssetType == AIM_File)
					rAnimation.m_currentSegmentIndex[idx] = 0;
			}

			//just for multi-layer
			//Automatic Fadeout: could this be a job for CryMannequin???
			if (NumAnimsInQueue == 1 && nLayer)
			{
				uint32 flags = rAnimation.m_nStaticFlags;
				uint32 rlk = flags & CA_REPEAT_LAST_KEY;
				uint32 fo = flags & CA_FADEOUT;  //this is something that CryMannequin should do
				if (rlk && fo)
					StopAnimationInLayer(nLayer, 0.5f);
			}
		}

		//----------------------------------------------------------------------

		if (LoopAnimation == 1 && RepeatLastKey == 0)
		{
			if (pParametric)
			{
				//int nEOC				=	rAnimation.GetEndOfCycle(); //value from the last frame
				//float fColor[4] = {1,1,0,1};
				//g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.7f, fColor, false,"fAnimTime: %f  EOC: %d  idx: %d",rAnimation.m_fAnimTime[idx],nEOC, idx );
				//g_YLine+=16.0f;
				const uint32 numAnims = pParametric->m_numExamples;
				for (uint32 i = 0; i < numAnims; i++)
				{
					const int32 nAnimID = pParametric->m_nAnimID[i];
					const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(nAnimID);
					if (pAnim->m_nAssetType != CAF_File)  CryFatalError("CryAnimation: weird error");   //can happen only in weird cases (mem-corruption, etc)
					GlobalAnimationHeaderCAF& rParaCAF = g_AnimationManager.m_arrGlobalCAF[pAnim->m_nGlobalAnimId];
					if (++pParametric->m_nSegmentCounter[idx][i] >= rParaCAF.m_Segments)
						pParametric->m_nSegmentCounter[idx][i] = 0;
				}

				rAnimation.m_DynFlags[idx] |= CA_LOOPED_THIS_UPDATE;
				
				// Update motion parameter for next iteration
				for (uint32 i{0}; i<MAX_LMG_DIMENSIONS; ++i)
				{
					pParametric->m_MotionParameter[i] = pParametric->m_MotionParameterForNextIteration[i];
					pParametric->m_MotionParameterFlags[i] |= CA_Dim_Initialized;
				}
			}
			else
			{
				const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(rAnimation.m_animationId);
				if (pAnim->m_nAssetType == CAF_File)
				{
					GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pAnim->m_nGlobalAnimId];
					if (++rAnimation.m_currentSegmentIndex[idx] >= rCAF.m_Segments)
					{
						rAnimation.m_currentSegmentIndex[idx] = 0;
						rAnimation.m_DynFlags[idx] |= CA_LOOPED_THIS_UPDATE;
					}
				}
				if (pAnim->m_nAssetType == AIM_File)
				{
					rAnimation.m_currentSegmentIndex[idx] = 0;
					rAnimation.m_DynFlags[idx] |= CA_LOOPED_THIS_UPDATE;
				}
			}
		}
	}

	//--- TSB: Added AnimNo check to stop us flagging animations to be removed before they've started in paused situations
	if ((rAnimation.GetTransitionWeight() == 0.0f) && (AnimNo == 0))
	{
		if (m_TrackViewExclusive == 0)
			rAnimation.m_DynFlags[idx] |= CA_REMOVE_FROM_QUEUE;
	}

	assert(rAnimation.m_fAnimTime[idx] >= 0.0f && rAnimation.m_fAnimTime[idx] <= 1.0f);
	return;
}

uint32 CSkeletonAnim::GetMaxSegments(const CAnimation& rAnimation) const
{
	uint32 nMaxSegments = 1;
	CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;
	const SParametricSamplerInternal* pParametric = static_cast<const SParametricSamplerInternal*>(rAnimation.GetParametricSampler());
	if (pParametric)
	{
		const uint32 numAnims = pParametric->m_numExamples;
		for (uint32 i = 0; i < numAnims; i++)
		{
			const int32 nAnimID = pParametric->m_nAnimID[i];
			const f32 fBlendWeight = fabsf(pParametric->m_fBlendWeight[i]);
			if (fBlendWeight < 0.001)
				continue;
			const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(nAnimID);
			if (pAnim->m_nAssetType != CAF_File)
				CryFatalError("CryAnimation: weird error"); //can happen only in weird cases (mem-corruption, etc)

			const int32 globalID = pAnim->m_nGlobalAnimId;
			GlobalAnimationHeaderCAF& rParaCAF = g_AnimationManager.m_arrGlobalCAF[globalID];
			if (nMaxSegments < rParaCAF.m_Segments)
				nMaxSegments = rParaCAF.m_Segments;
		}
	}
	else
	{
		const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(rAnimation.m_animationId);
		if (pAnim)
		{
			if (pAnim->m_nAssetType == CAF_File)
			{
				const int32 globalID = pAnim->m_nGlobalAnimId;
				GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[globalID];
				if (nMaxSegments < rCAF.m_Segments)
					nMaxSegments = rCAF.m_Segments;
			}
		}
	}

	return nMaxSegments;
}

struct SAnimCallbackParams
{
	const IAnimEventList* pAnimEventList;

	int32                 nAnimID;
};

// (sendAnimEventsForTimeOld == true)  => Triggers the events in the range [normalizedTimeOld, normalizedTimeNew]
// (sendAnimEventsForTimeOld == false) => Triggers the events in the range (normalizedTimeOld, normalizedTimeNew]
void CSkeletonAnim::AnimCallbackInternal(bool sendAnimEventsForTimeOld, f32 normalizedTimeOld, f32 normalizedTimeNew, SAnimCallbackParams& params)
{
	assert(0.f <= normalizedTimeOld);
	assert(normalizedTimeOld <= 1.f);

	assert(0.f <= normalizedTimeNew);
	assert(normalizedTimeNew <= 1.f);

	/*
	   assert(
	   ( sendAnimEventsForTimeOld && normalizedTimeOld <= normalizedTimeNew) ||
	   (!sendAnimEventsForTimeOld && normalizedTimeOld <  normalizedTimeNew)
	   );
	 */

	assert(params.pAnimEventList);

	assert(m_pEventCallback);

	const uint32 animEventCount = params.pAnimEventList->GetCount();
	for (uint32 i = 0; i < animEventCount; ++i)
	{
		const CAnimEventData& animEventData = params.pAnimEventList->GetByIndex(i);

		const f32 animEventNormalizedTime = animEventData.GetNormalizedTime();

		bool isAnimEventInRange = ((normalizedTimeOld < animEventNormalizedTime) && (animEventNormalizedTime <= normalizedTimeNew));
		if (sendAnimEventsForTimeOld)
		{
			isAnimEventInRange |= (animEventNormalizedTime == normalizedTimeOld);
		}

		if (isAnimEventInRange)
		{
			const char* const strModelName = animEventData.GetModelName();
			assert(strModelName);

			const char* const strFilePath = m_pInstance->CCharInstance::GetFilePath();
			assert(strFilePath);

			const bool equal = (strModelName[0] == 0 || strncmp(strModelName, strFilePath, 256) == 0);
			if (equal)
			{
				m_LastAnimEvent.SetAnimEventData(animEventData);
				(*m_pEventCallback)(m_pInstance, m_pEventCallbackData);
			}
		}
	}
}

void CSkeletonAnim::AnimCallback(CAnimation& rAnimation)
{
	if (!m_pEventCallback)
	{
		return;
	}

	const bool looped = ((rAnimation.m_DynFlags[0] & CA_LOOPED_THIS_UPDATE) != 0);
	const f32 segmentNormalizedTimeOld = rAnimation.m_fAnimTimePrev[0];
	const f32 segmentNormalizedTimeNew = rAnimation.m_fAnimTime[0];
	const uint8 segmentIndexOld = rAnimation.m_currentSegmentIndexPrev[0];
	const uint8 segmentIndexNew = rAnimation.m_currentSegmentIndex[0];

	// prevent sending events when animation is not playing
	// like when there's an event at time 1.0f and repeat-last-key flag is used
	// otherwise it may happen to get the event each frame
	const bool timeIsStationary = ((segmentNormalizedTimeOld == segmentNormalizedTimeNew) && (segmentIndexOld == segmentIndexNew) && (!looped));
	if (timeIsStationary)
	{
		return;
	}

	const bool animEventsEvaluatedOnce = ((rAnimation.m_DynFlags[0] & CA_ANIMEVENTS_EVALUATED_ONCE) != 0);
	rAnimation.m_DynFlags[0] |= CA_ANIMEVENTS_EVALUATED_ONCE;

	int32 nAnimID = -1;
	f32 normalizedTimeOld = 0.0f;
	f32 normalizedTimeNew = 0.0f;
	const char* pFilePath = nullptr;
	const IAnimEventList* pAnimEventList = nullptr;

	if (rAnimation.GetParametricSampler() == nullptr)
	{
		const CAnimationSet* const pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;
		assert(pAnimationSet);

		const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(rAnimation.GetAnimationId());
		if (pAnim->m_nAssetType == CAF_File)
		{
			GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pAnim->m_nGlobalAnimId];

			nAnimID = rAnimation.GetAnimationId();
			normalizedTimeOld = rCAF.GetNTimeforEntireClip(segmentIndexOld, segmentNormalizedTimeOld);
			normalizedTimeNew = rCAF.GetNTimeforEntireClip(segmentIndexNew, segmentNormalizedTimeNew);
			pFilePath = rCAF.GetFilePath();
			pAnimEventList = &rCAF.m_AnimEventsCAF;
			assert(normalizedTimeNew >= normalizedTimeOld || looped);
		}
	}
	else
	{
		assert(rAnimation.GetAnimationId() >= 0);

		const CAnimationSet* const pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;
		assert(pAnimationSet);

		const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(rAnimation.GetAnimationId());
		if (pAnim->m_nAssetType == LMG_File)
		{
			GlobalAnimationHeaderLMG& rGlobalAnimHeaderLMG = g_AnimationManager.m_arrGlobalLMG[pAnim->m_nGlobalAnimId];

			nAnimID = -1;
			normalizedTimeOld = segmentNormalizedTimeOld;
			normalizedTimeNew = segmentNormalizedTimeNew;
			pFilePath = rGlobalAnimHeaderLMG.GetFilePath();
			pAnimEventList = &rGlobalAnimHeaderLMG.m_AnimEventsLMG;

			{
				// TODO: This code partially duplicates other similar routines in this translation unit, particularily parts of CSkeletonAnim::GetAnimationNormalizedTime().
				// All of them should be merged together (and probably abstracted by the GAH LMG class).

				assert(segmentIndexOld == 0xff); // Note: CAnimation segment indices are not used and always set to 0xff in case of a parametric blendspaces.
				assert(segmentIndexNew == 0xff); // We're making sure here that this invariant always holds, but animation state should ideally be unified and hidden behind a consistent abstraction layer.

				const GlobalAnimationHeaderCAF* pLongestCaf = nullptr;
				uint32 longestCafIndexOld = 0;
				uint32 longestCafIndexNew = 0;
				{
					const SParametricSamplerInternal* pParametricSampler = static_cast<const SParametricSamplerInternal*>(rAnimation.GetParametricSampler());
					for (uint32 i = 0; i < pParametricSampler->m_numExamples; ++i)
					{
						const GlobalAnimationHeaderCAF& caf = g_AnimationManager.m_arrGlobalCAF[pAnimationSet->GetModelAnimationHeader(pParametricSampler->m_nAnimID[i])->m_nGlobalAnimId];
						if (!pLongestCaf || pLongestCaf->GetTotalSegments() < caf.GetTotalSegments())
						{
							pLongestCaf = &caf;
							longestCafIndexNew = pParametricSampler->m_nSegmentCounter[0][i];
							longestCafIndexOld = pParametricSampler->m_nSegmentCounterPrev[0][i];
						}
					}
				}

				normalizedTimeOld = pLongestCaf ? pLongestCaf->GetNTimeforEntireClip(longestCafIndexOld, segmentNormalizedTimeOld) : 0.0f;
				normalizedTimeNew = pLongestCaf ? pLongestCaf->GetNTimeforEntireClip(longestCafIndexNew, segmentNormalizedTimeNew) : 0.0f;
				assert(normalizedTimeNew >= normalizedTimeOld || looped);
			}
		}
	}

	if (pAnimEventList)
	{
		SAnimCallbackParams params;
		params.nAnimID = nAnimID;
		params.pAnimEventList = pAnimEventList;

		const bool sendAnimEventsForTimeOld = !animEventsEvaluatedOnce;
		if (looped)
		{
			const bool timeOldIsExactlyAnimationEnd = (normalizedTimeOld == 1.0f);
			const bool rangeFromTimeOldToAnimationEndIsValid = (sendAnimEventsForTimeOld || !timeOldIsExactlyAnimationEnd);
			if (rangeFromTimeOldToAnimationEndIsValid)
			{
				AnimCallbackInternal(sendAnimEventsForTimeOld, normalizedTimeOld, 1.f, params);
			}
			AnimCallbackInternal(true, 0.f, normalizedTimeNew, params);
		}
		else
		{
			AnimCallbackInternal(sendAnimEventsForTimeOld, normalizedTimeOld, normalizedTimeNew, params);
		}
	}
}

//this function is used only internally for BSpace visualization
void CSkeletonAnim::SetLayerNormalizedTimeAndSegment(f32 normalizedTime, int nEOC, int32 nAnimID0, int32 nAnimID1, uint8 nSegment0, uint8 nSegment1)
{
	const uint animationCountInLayer = m_layers[0].m_transitionQueue.GetAnimationCount();
	if (animationCountInLayer == 1)
	{
		CAnimation* pAnimation = &m_layers[0].m_transitionQueue.GetAnimation(0);
		pAnimation->m_DynFlags[0] = nEOC;
		pAnimation->m_fAnimTimePrev[0] = pAnimation->m_fAnimTime[0];
		pAnimation->m_fAnimTime[0] = clamp_tpl(normalizedTime, 0.0f, 1.0f);

		SParametricSamplerInternal* pPara = static_cast<SParametricSamplerInternal*>(pAnimation->GetParametricSampler());
		if (pPara == 0)
		{
			pAnimation->m_currentSegmentIndexPrev[0] = pAnimation->m_currentSegmentIndex[0];
			pAnimation->m_currentSegmentIndex[0] = nSegment0; //set the seg-counter
		}
		else
		{
			pAnimation->m_currentSegmentIndex[0] = -1; //segment counters are not used for BSpaces
			uint32 numExamples = pPara->m_numExamples;
			for (uint32 i = 0; i < numExamples; i++)
			{
				if (pPara->m_nAnimID[i] == nAnimID0)
					pPara->m_nSegmentCounterPrev[0][i] = pPara->m_nSegmentCounter[0][i], pPara->m_nSegmentCounter[0][i] = nSegment0;  //set the seg-counter on the asset directly
				if (pPara->m_nAnimID[i] == nAnimID1)
					pPara->m_nSegmentCounterPrev[0][i] = pPara->m_nSegmentCounter[0][i], pPara->m_nSegmentCounter[0][i] = nSegment1;  //set the seg-counter on the asset directly
			}
		}
	}
}

void CSkeletonAnim::SetLayerNormalizedTime(uint32 layer, f32 normalizedTime)
{
	const uint animationCountInLayer = m_layers[layer].m_transitionQueue.GetAnimationCount();
	if (animationCountInLayer == 1)
	{
		CAnimation* pAnimation = &m_layers[layer].m_transitionQueue.GetAnimation(0);
		SetAnimationNormalizedTime(pAnimation, normalizedTime);
	}
}

f32 CSkeletonAnim::GetLayerNormalizedTime(uint32 layer) const
{
	const bool animationLayerEmpty = (m_layers[layer].m_transitionQueue.GetAnimationCount() < 1);
	if (animationLayerEmpty)
	{
		return 0.f;
	}
	return GetAnimationNormalizedTime(layer, 0);
}

void CSkeletonAnim::SetAnimationNormalizedTime(CAnimation* pAnimation, f32 uncheckedNormalizedTime, bool entireClip)
{
	if (!pAnimation)
		return;

	const f32 fNormalizedTime = clamp_tpl(uncheckedNormalizedTime, 0.0f, 0.99999f);
	const int16 nAnimId = pAnimation->GetAnimationId();
	const CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;
	const ModelAnimationHeader* pModelAnimationHeader = pAnimationSet->GetModelAnimationHeader(nAnimId);
	if (pModelAnimationHeader->m_nAssetType == AIM_File)
	{
		pAnimation->m_fAnimTime[0] = fNormalizedTime;
		return;
	}

	SParametricSamplerInternal* pParametricSampler = static_cast<SParametricSamplerInternal*>(pAnimation->GetParametricSampler());

	if (!entireClip)
	{
		pAnimation->m_fAnimTime[0] = fNormalizedTime;

		return;
	}

	//------------------------------------------------------------
	if (pParametricSampler)
	{
		//find biggest segment
		uint32 nMaxSegCount = 0;
		int32 nMaxAnimID = -1;
		uint8 nSegmentIndexPrev = 0;
		f32 fAccBWeights = 0;
		f32 fEntireTime = 0;
		for (uint32 i = 0; i < pParametricSampler->m_numExamples; i++)
		{
			fAccBWeights += pParametricSampler->m_fBlendWeight[i];
			if (pParametricSampler->m_fBlendWeight[i] == 0.0f)
				continue;
			int32 nAnimIDPS = pParametricSampler->m_nAnimID[i];
			int8 nsip = pParametricSampler->m_nSegmentCounter[0][i];
			const ModelAnimationHeader* pABS = pAnimationSet->GetModelAnimationHeader(nAnimIDPS);
			GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pABS->m_nGlobalAnimId];
			uint32 sc = rCAF.GetTotalSegments();
			if (nMaxSegCount < sc)
				nMaxSegCount = sc, nMaxAnimID = nAnimIDPS, nSegmentIndexPrev = nsip, fEntireTime = rCAF.GetNTimeforEntireClip(nsip, pAnimation->m_fAnimTime[0]);
		}
		if (fAccBWeights == 0)
			return; //all BWeights are zero. Setting the time is not possible
		if (nMaxAnimID < 0)
			return; //couldn't read the values. Setting the time is not possible

		const ModelAnimationHeader* pAnimBS = pAnimationSet->GetModelAnimationHeader(nMaxAnimID);
		const int32 globalAnimationId = pAnimBS->m_nGlobalAnimId;
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[globalAnimationId];
		uint32 nMaxSegments = rCAF.GetTotalSegments();

		uint8 nSegmentIndexNew = 0;
		for (uint32 i = 0; i < nMaxSegments; ++i)
		{
			const f32 nextSegmentTime = rCAF.m_SegmentsTime[i + 1];
			if (fNormalizedTime <= nextSegmentTime)
			{
				nSegmentIndexNew = i;
				break;
			}
		}

		if (nSegmentIndexNew != nSegmentIndexPrev)
			pAnimation->m_DynFlags[0] |= CA_EOC;
		if (fNormalizedTime < fEntireTime)
			pAnimation->m_DynFlags[0] |= CA_NEGATIVE_EOC;

		const f32 segmentStartTime = rCAF.m_SegmentsTime[nSegmentIndexNew];
		const f32 segmentEndTime = rCAF.m_SegmentsTime[nSegmentIndexNew + 1];
		const f32 segmentDuration = segmentEndTime - segmentStartTime;
		if (segmentDuration <= 0)
		{
			pAnimation->m_fAnimTime[0] = fNormalizedTime;  //some error in the .AnimEvent file! we should never end up here
			return;
		}
		const f32 segmentTime = fNormalizedTime - segmentStartTime;
		const f32 segmentNormalizedPosition = segmentTime / segmentDuration;
		pAnimation->m_fAnimTime[0] = segmentNormalizedPosition;

		for (uint32 i = 0; i < pParametricSampler->m_numExamples; i++)
		{
			int32 nAnimIDPS = pParametricSampler->m_nAnimID[i];
			const ModelAnimationHeader* pABS = pAnimationSet->GetModelAnimationHeader(nAnimIDPS);
			GlobalAnimationHeaderCAF& rExampleCAF = g_AnimationManager.m_arrGlobalCAF[pABS->m_nGlobalAnimId];
			uint32 sc = rExampleCAF.GetTotalSegments();
			pParametricSampler->m_nSegmentCounter[0][i] = nSegmentIndexNew % sc;
			pParametricSampler->m_nSegmentCounter[1][i] = nSegmentIndexNew % sc;
		}
	}
	else
	{
		//uint8 oldsi = pAnimation->m_currentSegmentIndex[0];
		const int32 globalAnimationId = pModelAnimationHeader->m_nGlobalAnimId;
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[globalAnimationId];
		uint32 nMaxSegments = rCAF.GetTotalSegments();
		uint8 currentSegmentIndex = 0;
		for (uint32 i = 0; i < nMaxSegments; ++i)
		{
			const f32 nextSegmentTime = rCAF.m_SegmentsTime[i + 1];
			if (fNormalizedTime <= nextSegmentTime)
			{
				currentSegmentIndex = i;
				break;
			}
		}

		pAnimation->m_currentSegmentIndex[0] = currentSegmentIndex;

		const f32 segmentStartTime = rCAF.m_SegmentsTime[currentSegmentIndex];
		const f32 segmentEndTime = rCAF.m_SegmentsTime[currentSegmentIndex + 1];
		const f32 segmentDuration = segmentEndTime - segmentStartTime;
		if (segmentDuration <= 0)
		{
			pAnimation->m_fAnimTime[0] = fNormalizedTime;  //some error in the .AnimEvent file! we should never end up here
			return;
		}
		const f32 segmentTime = fNormalizedTime - segmentStartTime;
		const f32 segmentNormalizedPosition = segmentTime / segmentDuration;
		pAnimation->m_fAnimTime[0] = segmentNormalizedPosition;
	}
}

f32 CSkeletonAnim::GetAnimationNormalizedTime(uint32 layer, uint32 index) const
{
	const CAnimation* pAnimation = &m_layers[layer].m_transitionQueue.GetAnimation(index);
	return GetAnimationNormalizedTime(pAnimation);
}

f32 CSkeletonAnim::GetAnimationNormalizedTime(const CAnimation* pAnimation) const
{
	if (!pAnimation)
		return -1.f;

	const int16 nAnimID = pAnimation->GetAnimationId();
	const CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;
	const ModelAnimationHeader* pModelAnimationHeader = pAnimationSet->GetModelAnimationHeader(nAnimID);
	if (pModelAnimationHeader == 0)
		return 0.0f;
	if (pModelAnimationHeader->m_nAssetType == AIM_File)
		return pAnimation->m_fAnimTime[0];

	const uint32 nMaxSegments = GetMaxSegments(*pAnimation);
	if (nMaxSegments <= 1)
		return pAnimation->m_fAnimTime[0];

	const SParametricSamplerInternal* pParametricSampler = static_cast<const SParametricSamplerInternal*>(pAnimation->GetParametricSampler());
	if (pParametricSampler)
	{
		//find biggest segment
		uint32 nMaxSegCount = 0;
		uint32 nCurSegIndex = 0;
		int32 nMaxAnimID = -1;
		f32 fAccBWeights = 0;
		for (uint32 i = 0; i < pParametricSampler->m_numExamples; i++)
		{
			fAccBWeights += pParametricSampler->m_fBlendWeight[i];
			if (pParametricSampler->m_fBlendWeight[i] == 0.0f)
				continue;
			int32 nAnimIDPS = pParametricSampler->m_nAnimID[i];
			const ModelAnimationHeader* pABS = pAnimationSet->GetModelAnimationHeader(nAnimIDPS);
			GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pABS->m_nGlobalAnimId];
			uint32 sc = rCAF.GetTotalSegments();
			if (nMaxSegCount < sc)
				nMaxSegCount = sc, nMaxAnimID = nAnimIDPS, nCurSegIndex = pParametricSampler->m_nSegmentCounter[0][i];
		}

		if (fAccBWeights == 0)
			return 0; //all BWeights are zero. Setting the time is not possible
		if (nMaxAnimID < 0)
			return 0; //couldn't read the values. Setting the time is not possible

		const ModelAnimationHeader* pAnimBS = pAnimationSet->GetModelAnimationHeader(nMaxAnimID);
		const int32 nGlobalId = pAnimBS->m_nGlobalAnimId;

		const f32 currentSegmentNormalizedPosition = pAnimation->m_fAnimTime[0];
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[nGlobalId];
		return rCAF.GetNTimeforEntireClip(nCurSegIndex, currentSegmentNormalizedPosition); //this is a percentage value between 0-1 for the ENTIRE animation
	}
	else
	{
		const uint8 currentSegmentIndex = pAnimation->m_currentSegmentIndex[0];
		const f32 currentSegmentNormalizedPosition = pAnimation->m_fAnimTime[0];
		GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pModelAnimationHeader->m_nGlobalAnimId];
		return rCAF.GetNTimeforEntireClip(currentSegmentIndex, currentSegmentNormalizedPosition); //this is a percentage value between 0-1 for the ENTIRE animation
	}
}

// Euclidean algorithm
static uint32 GreatestCommonDivisor(uint32 a, uint32 b)
{
	uint32 smallest = min(a, b);
	uint32 largest = max(a, b);
	uint32 remainder = largest % smallest;

	while (remainder != 0)
	{
		largest = smallest;
		smallest = remainder;
		remainder = largest % smallest;
	}
	return smallest;
}

static uint32 LeastCommonMultiple(uint8 (&segments)[MAX_LMG_ANIMS], uint32 count)
{
	uint32 result = 1;
	for (int i = 0; i < count; ++i)
	{
		if (segments[i] <= 1)
			continue;
		result *= segments[i] / GreatestCommonDivisor(result, segments[i]);
	}
	return result;
}

f32 CSkeletonAnim::CalculateCompleteBlendSpaceDuration(const CAnimation& rAnimation) const
{
	CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;

	SParametricSamplerInternal* pParametric = (SParametricSamplerInternal*)rAnimation.GetParametricSampler();
	if (pParametric)
	{
		uint8 segmentCount[MAX_LMG_ANIMS] = { 0 };
		for (uint32 i = 0; i < pParametric->m_numExamples; ++i)
		{
			if (pParametric->m_fBlendWeight[i] == 0.0f)
				continue;
			const ModelAnimationHeader* pHeader = pAnimationSet->GetModelAnimationHeader(pParametric->m_nAnimID[i]);
			GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pHeader->m_nGlobalAnimId];
			segmentCount[i] = rCAF.m_Segments;
		}

		int totalSegmentCount = LeastCommonMultiple(segmentCount, pParametric->m_numExamples);

		f32 fTotalDuration = 0.0f;
		for (uint32 i = 0; i < pParametric->m_numExamples; ++i)
		{
			if (pParametric->m_fBlendWeight[i] == 0.0f)
				continue;
			int32 nAnimID = pParametric->m_nAnimID[i];
			const ModelAnimationHeader* pHeader = pAnimationSet->GetModelAnimationHeader(nAnimID);
			const GlobalAnimationHeaderCAF& rCAF = g_AnimationManager.m_arrGlobalCAF[pHeader->m_nGlobalAnimId];

			for (uint32 s = 0; s < segmentCount[i]; ++s)
			{
				int32 repetitionCount = totalSegmentCount / segmentCount[i];
				f32 fBlendWeight = pParametric->m_fBlendWeight[i];
				f32 fPlaybackScale = pParametric->m_fPlaybackScale[i];
				float fSegmentDuration = rCAF.GetSegmentDuration(s);
				fTotalDuration += fSegmentDuration * fBlendWeight * fPlaybackScale * repetitionCount;
			}
		}

		return fTotalDuration;
	}
	else
	{
		return rAnimation.GetExpectedTotalDurationSeconds();
	}
}

void CSkeletonAnim::SetAnimationDrivenMotion(uint32 ts)
{
	m_AnimationDrivenMotion = ts;
};
void CSkeletonAnim::SetMirrorAnimation(uint32 ts)
{
	m_MirrorAnimation = ts;
};

// sets the animation speed scale for layers
void CSkeletonAnim::SetLayerPlaybackScale(int32 nLayer, f32 fSpeed)
{
	if (nLayer >= numVIRTUALLAYERS || nLayer < 0)
	{
		g_pILog->LogError("SetLayerPlaybackScale() was using invalid layer id: %d", nLayer);
		return;
	}
	m_layers[nLayer].m_transitionQueue.m_fLayerPlaybackScale = max(0.0f, fSpeed);
}

f32 CSkeletonAnim::GetLayerBlendWeight(int32 nLayer)
{
	if (nLayer>=numVIRTUALLAYERS || nLayer<0)
	{
		g_pILog->LogError ("invalid layer id: %d", nLayer);
		return -1.0f;
	}
	return m_layers[nLayer].m_transitionQueue.m_fLayerBlendWeight;
}

void CSkeletonAnim::SetLayerBlendWeight(int32 nLayer, f32 fMult)
{
	if (nLayer >= numVIRTUALLAYERS || nLayer < 0)
	{
		g_pILog->LogError("invalid layer id: %d", nLayer);
		return;
	}
	m_layers[nLayer].m_transitionQueue.m_fLayerBlendWeight = fMult;
}

uint32 CSkeletonAnim::EvaluateTransitionFlags(CAnimation* arrAnimFiFo, uint32 numAnims)
{
	CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;

	//-------------------------------------------------------------------------------
	//---                            evaluate transition flags                    ---
	//-------------------------------------------------------------------------------
	uint32 aq = 0;
	for (aq = 1; aq < numAnims; aq++)
	{
		CAnimation& rPrevAnimation = arrAnimFiFo[aq - 1];
		CAnimation& rCurAnimation = arrAnimFiFo[aq];

		if (rCurAnimation.IsActivated())
			continue; //an activated motion will stay activated

		uint32 IsLooping = rPrevAnimation.m_nStaticFlags & CA_LOOP_ANIMATION;

		uint32 StartAtKeytime = rCurAnimation.m_nStaticFlags & CA_START_AT_KEYTIME;
		uint32 StartAfter = rCurAnimation.m_nStaticFlags & CA_START_AFTER;
		uint32 Idle2Move = rCurAnimation.m_nStaticFlags & CA_IDLE2MOVE;
		//		uint32 Move2Idle			= rCurAnimation.m_nStaticFlags & CA_MOVE2IDLE;
		uint32 InMemory = IsAnimationInMemory(pAnimationSet, &rCurAnimation);

		if (Console::GetInst().ca_DebugAnimationStreaming && InMemory == 0)
		{
			const char* pName = 0;
			const ModelAnimationHeader* anim = &pAnimationSet->GetModelAnimationHeaderRef(rCurAnimation.GetAnimationId());

			pName = anim->GetAnimName();

			float fC1[4] = { 1, 1, 0, 1 };
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 2.6f, fC1, false, "Streaming Assets: %s ", pName);
			g_YLine += 16.0f;

			g_pISystem->Warning(VALIDATOR_MODULE_ANIMATION, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, m_pInstance->GetFilePath(), "CryAnimation: streaming file: %s", pName);
		}

		//can we use the "Idle2Move" flag?
		if (Idle2Move)
		{
			Idle2Move = 0;  //lets be pessimistic and assume we can't use this flag at all
			assert(!rCurAnimation.IsActivated());

			if (rPrevAnimation.GetParametricSampler() != NULL)
			{
				const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(rPrevAnimation.GetAnimationId());
				assert(pAnim->m_nAssetType == LMG_File);
				GlobalAnimationHeaderLMG& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalLMG[pAnim->m_nGlobalAnimId];
				if (rGlobalAnimHeader.m_VEG_Flags & CA_VEG_I2M)
					Idle2Move = 2;  //for this VEG we can use it
			}
		}

		//can we use the "StartAfter" flag?
		if (IsLooping && StartAfter)
			StartAfter = 0; //if first animation is looping, then start transition immediately to prevent a hang in the FIFO

		//-------------------------------------------------------------------------------------------------
		//-- If there are several animations in the FIFO it depends on the transition flags whether      --
		//-- we can start a transition immediately. Maybe we want to play one animation after another.   --
		//-------------------------------------------------------------------------------------------------

		//all thisn stuff belongs into CryManequinn
		uint32 TransitionDelay = StartAtKeytime + StartAfter + Idle2Move + (InMemory == 0);
		if (TransitionDelay)
		{
			if (InMemory == 0)
				break;  //impossible to activate in this frame

			if (StartAtKeytime)
			{
				assert(!rCurAnimation.IsActivated());
				f32 atnew = GetAnimationNormalizedTime(&rPrevAnimation);
				f32 atold = atnew - 0.000001f; // FIXME
				f32 keyTime = rCurAnimation.m_fStartTime;
				if (atold < keyTime && keyTime < atnew)
					rCurAnimation.m_DynFlags[0] |= CA_ACTIVATED;
			}

			if (StartAfter)
			{
				assert(!rCurAnimation.IsActivated());
				if (rPrevAnimation.GetRepeat())
					rCurAnimation.m_DynFlags[0] |= CA_ACTIVATED;
			}

			if (rPrevAnimation.GetParametricSampler() != NULL)
			{
				if (Idle2Move == 2)
				{
					SParametricSamplerInternal& lmg = *(SParametricSamplerInternal*)rPrevAnimation.GetParametricSampler();

					//Its a VEG and its activated
					const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(rPrevAnimation.GetAnimationId());
					if (pAnim->m_nAssetType != LMG_File)
						CryFatalError("CryAnimation: no VEG"); //can happen only in weird cases (mem-corruption, etc)

					uint32 numLMGs = g_AnimationManager.m_arrGlobalLMG.size();
					if (uint32(pAnim->m_nGlobalAnimId) >= numLMGs)
						CryFatalError("CryAnimation: VEG-ID out of range"); //can happen only in weird cases (mem-corruption, etc)

					GlobalAnimationHeaderLMG& rLMG = g_AnimationManager.m_arrGlobalLMG[pAnim->m_nGlobalAnimId];

					int32 dim = -1;
					for (int32 d = 0; d < rLMG.m_Dimensions; d++)
					{
						if (rLMG.m_DimPara[d].m_ParaID == eMotionParamID_TurnAngle)
						{
							dim = d;
							break;
						}
					}
					if (dim < 0)
						CryFatalError("CryAnimation: I2M without Turn-Parameter"); //bad VEG

					uint32 nSegCount = lmg.m_nSegmentCounter[0][0];
					PREFAST_SUPPRESS_WARNING(6385)
					f32 vLockedTurnAngle = rPrevAnimation.GetParametricSampler()->m_MotionParameter[dim];
					if (vLockedTurnAngle > 0)
					{
						if (nSegCount)
							rCurAnimation.m_DynFlags[0] |= CA_ACTIVATED;  //move to the left
					}
					else
					{
						if (nSegCount && rPrevAnimation.m_fAnimTime[0] > 0.50f)
							rCurAnimation.m_DynFlags[0] |= CA_ACTIVATED;  //move to the right
					}
				}
			}

			if (rCurAnimation.IsActivated() == 0)
				break; //all other animations in the FIFO will remain not activated
		}
		else
		{
			//No transition-delay flag set. Thats means we can activate the transition immediately
			rCurAnimation.m_DynFlags[0] |= CA_ACTIVATED;
		}
	}

	return aq;
}

uint32 CSkeletonAnim::IsAnimationInMemory(CAnimationSet* pAnimationSet, CAnimation* pAnimation)
{
	SParametricSamplerInternal* pParametric = (SParametricSamplerInternal*)pAnimation->GetParametricSampler();
	if (pParametric == NULL)
	{
		return CheckIsCAFLoaded(pAnimationSet, pAnimation->GetAnimationId());
	}
	else
	{
		uint32 numExamples = pParametric->m_numExamples;
		for (uint32 a = 0; a < numExamples; a++)
		{
			int32 nAnimID = pParametric->m_nAnimID[a];
			CheckIsCAFLoaded(pAnimationSet, nAnimID); //if not, then start streaming
		}

		for (uint32 a = 0; a < numExamples; a++)
		{
			int32 nAnimID = pParametric->m_nAnimID[a];
			if (CheckIsCAFLoaded(pAnimationSet, nAnimID) == 0)
				return 0;
		}
	}

	return 1;
}

void CSkeletonAnim::BlendManagerDebug()
{
#ifndef _RELEASE
	if (Console::GetInst().ca_DebugTextTarget.empty() == false
	    && strstr(m_pInstance->GetFilePath(), Console::GetInst().ca_DebugTextTarget.c_str()) == NULL)
		return;

	{
		f32 fColor[4] = { 1, .5f, .15f, 1 };
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "Queue for '%s':", m_pInstance->m_strFilePath.c_str());
		g_YLine += 0x0e;
	}

	for (uint32 nVLayerNo = 0; nVLayerNo < numVIRTUALLAYERS; nVLayerNo++)
	{
		DynArray<CAnimation>& rCurLayer = m_layers[nVLayerNo].m_transitionQueue.m_animations;
		uint32 numAnimsPerLayer = rCurLayer.size();

		BlendManagerDebug(rCurLayer, nVLayerNo);
	}

	f32 fColor[4] = { 1, 0, 0, 1 };

	const CDefaultSkeleton& modelSkeleton = *m_pInstance->m_pDefaultSkeleton;
	const uint32 adikTargetCount = modelSkeleton.m_ADIKTargets.size();

	uint32 adikLineLength = 0;
	for (uint32 adikIndex = 0; adikIndex < adikTargetCount; ++adikIndex)
	{
		const ADIKTarget& adikTarget = modelSkeleton.m_ADIKTargets[adikIndex];

		const int32 targetWeightJointIndex = adikTarget.m_idxWeight;
		if (targetWeightJointIndex < 0)
			continue;

		if (adikLineLength == 0)
		{
			g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "ADIK");
		}

		const QuatT& targetWeightJoint = m_pInstance->m_SkeletonPose.GetPoseData().GetJointRelative(targetWeightJointIndex);
		const float targetBlendWeight = targetWeightJoint.t.x;

		const float textWidthMultiplier = 6.f;
		const uint32 startOffsetTextLength = 6;
		g_pAuxGeom->Draw2dLabel(textWidthMultiplier * (startOffsetTextLength + adikLineLength), g_YLine, 1.2f, fColor, false, "%s: %.3f", adikTarget.m_strTarget, targetBlendWeight);
		const uint32 fixedLineTextLength = 10;
		adikLineLength += strlen(adikTarget.m_strTarget) + fixedLineTextLength;
		const uint32 maxLineLength = 120;
		if (maxLineLength < adikLineLength)
		{
			g_YLine += 0x0e;
			adikLineLength = 0;
		}
	}

	if (0 < adikLineLength)
	{
		g_YLine += 0x0e;
	}

#endif
}

uint32 CSkeletonAnim::BlendManagerDebug(DynArray<CAnimation>& arrAFIFO, uint32 nVLayer)
{
#ifndef _RELEASE
	f32 fColor[4] = { 1, 0, 0, 1 };

	if (Console::GetInst().ca_DebugTextLayer != 0xffffffff
	    && nVLayer != Console::GetInst().ca_DebugTextLayer)
		return 0;

	CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;

	uint32 NumAnimsInQueue = arrAFIFO.size();
	for (uint32 i = 0; i < NumAnimsInQueue; i++)
	{
		const CAnimation& animation = arrAFIFO[i];
		const SParametricSamplerInternal* pParametric = (SParametricSamplerInternal*)animation.GetParametricSampler();
		const f32 transitionWeight = animation.GetTransitionWeight();
		if (animation.IsActivated())
		{	
			fColor[0] = transitionWeight;
			fColor[1] = (nVLayer > 0);// % 2 ? arrAFIFO[i].m_fTransitionWeight * 0.3f : 0.0f;
			fColor[2] = 0.0f;
			fColor[3] = transitionWeight;
			if (nVLayer)
				fColor[3 ]= m_layers[nVLayer].m_transitionQueue.m_fLayerBlendWeight*transitionWeight;
		}
		else
		{
			fColor[0] = 0.0f;
			fColor[1] = 0.0f;
			fColor[2] = 0.0f;
			fColor[3] = 0.5f;
		}

		fColor[3] = (fColor[3] + 1) * 0.5f;
		const char* pAnimName = pAnimationSet->GetModelAnimationHeaderRef(animation.GetAnimationId()).GetAnimName();

		g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.2f, fColor, false,"AnimInAFIFO %02d:  t: %d  %s  ATime: %.3f (%.3fs/%.3fs)  ASpd: %.3f Flag: %08x (%s)  TTime: %.3f  TWght: %.3f  PWght: %.3f  seg: %02d  inmem: %i", nVLayer, animation.m_nUserToken, pAnimName, animation.m_fAnimTime[0], animation.m_fAnimTime[0] * animation.GetCurrentSegmentExpectedDurationSeconds(), animation.GetCurrentSegmentExpectedDurationSeconds(), animation.m_fPlaybackScale, animation.m_nStaticFlags, AnimFlags(animation.m_nStaticFlags), animation.m_fTransitionTime, transitionWeight, animation.GetPlaybackWeight(), animation.m_currentSegmentIndex[0], IsAnimationInMemory(pAnimationSet, &arrAFIFO[i]) ); 
		g_YLine += 0x0e;
		if (nVLayer == 0)
		{
			uint32 nUseDirIK = 0;
			f32 fDirIKBlend = 0.0f;
			f32 fDirIKInfluence = 0.0f;
			CPoseBlenderAim* pPBAim = static_cast<CPoseBlenderAim*>(m_pSkeletonPose->m_PoseBlenderAim.get());
			if (pPBAim)
			{
				nUseDirIK = pPBAim->m_blender.m_Set.bUseDirIK;
				fDirIKBlend = pPBAim->m_blender.m_Get.fDirIKBlend;
				fDirIKInfluence = pPBAim->m_blender.m_Get.fDirIKInfluence;
			}

			uint32 nUseLookIK = 0;
			f32 fLookIKBlend = 0.0f;
			f32 fLookIKInfluence = 0.0f;
			CPoseBlenderLook* pPBLook = static_cast<CPoseBlenderLook*>(m_pSkeletonPose->m_PoseBlenderLook.get());
			if (pPBLook)
			{
				nUseLookIK = pPBLook->m_blender.m_Set.bUseDirIK;
				fLookIKBlend = pPBLook->m_blender.m_Get.fDirIKBlend;
				fLookIKInfluence = pPBLook->m_blender.m_Get.fDirIKInfluence;
			}

			g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "(Try)UseAimIK: %d  AimIKBlend: %.3f  AimIKInfluence: %.3f   (Try)UseLookIK: %d  LookIKBlend: %.3f  LookIKInfluence: %.3f", nUseDirIK, fDirIKBlend, fDirIKInfluence, nUseLookIK, fLookIKBlend, fLookIKInfluence);
			g_YLine += 0x0e;
		}
		else
		{
			g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.2f, fColor, false,"LayerBlendWeight: %.3f  LayerTransitionWeight: %.3f", m_layers[nVLayer].m_transitionQueue.m_fLayerBlendWeight, m_layers[nVLayer].m_transitionQueue.m_fLayerTransitionWeight);
			g_YLine += 0x0e;
		}

		if (pParametric)
		{
			for (uint32 d = 0; d < pParametric->m_numDimensions; d++)
			{
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_TravelSpeed)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "MoveSpeed: %f  locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_TurnSpeed)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "TurnSpeed: %f  locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_TravelSlope)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "Slope: %f   locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_TravelAngle)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "TravelAngle: %f  locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_TravelDist)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "TravelDist: %f  locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_TurnAngle)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "TurnAngle: %f  locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_StopLeg)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "StopLeg: %f   locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_BlendWeight)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "BlendWeight: %f   locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_BlendWeight2)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "BlendWeight2: %f   locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_BlendWeight3)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "BlendWeight3: %f   locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_BlendWeight4)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "BlendWeight4: %f   locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_BlendWeight5)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "BlendWeight5: %f   locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_BlendWeight6)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "BlendWeight6: %f   locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
				if (pParametric->m_MotionParameterID[d] == eMotionParamID_BlendWeight7)
					g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, fColor, false, "BlendWeight7: %f   locked: %d", pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter), g_YLine += 0x0e;
			}
		}
		g_YLine += 0x02;

	}

	uint poseModifierCount = uint(m_layers[nVLayer].m_poseModifierQueue.GetEntryCount());
	for (uint i = 0; i < poseModifierCount; ++i)
	{
		g_pAuxGeom->Draw2dLabel(1, g_YLine, 1.2f, ColorF(1.0f, 1.0f, 1.0f, 1.0f), false,
		                        "PM class: %s, name %s",
		                        m_layers[nVLayer].m_poseModifierQueue.GetEntries()[i].poseModifier->GetFactory()->GetName(),
		                        m_layers[nVLayer].m_poseModifierQueue.GetEntries()[i].name);
		g_YLine += 0x0e;
	}

#endif

	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSkeletonAnim::DebugLogTransitionQueueState()
{
#ifndef _RELEASE

	CAnimationSet* pAnimationSet = m_pInstance->m_pDefaultSkeleton->m_pAnimationSet;

	for (uint32 nVLayer = 0; nVLayer < numVIRTUALLAYERS; nVLayer++)
	{
		DynArray<CAnimation>& arrAFIFO = m_layers[nVLayer].m_transitionQueue.m_animations;
		if (!arrAFIFO.empty())
		{
			const uint32 numAnimsInQueue = arrAFIFO.size();
			for (uint32 i = 0; i < numAnimsInQueue; i++)
			{
				CAnimation& animation = arrAFIFO[i];
				const SParametricSamplerInternal* pParametric = (SParametricSamplerInternal*)animation.GetParametricSampler();

				const char* pAnimName = pAnimationSet->GetModelAnimationHeaderRef(animation.GetAnimationId()).GetAnimName();

				CryLogAlways("AnimInAFIFO %02u: t:%u  '%s'  ATime:%.2f (%.2fs/%.2fs) ASpd:%.2f Flag:%08x (%s) TTime:%.2f TWght:%.2f PWght:%.2f seg:%02u inmem:%u",
				             nVLayer,
				             animation.m_nUserToken,
				             pAnimName,
				             animation.m_fAnimTime[0],
				             animation.m_fAnimTime[0] * animation.GetCurrentSegmentExpectedDurationSeconds(),
				             animation.GetCurrentSegmentExpectedDurationSeconds(),
				             animation.m_fPlaybackScale,
				             animation.m_nStaticFlags,
				             AnimFlags(animation.m_nStaticFlags),
				             animation.m_fTransitionTime,
				             animation.GetTransitionWeight(),
				             animation.GetPlaybackWeight(),
				             animation.m_currentSegmentIndex[0],
				             IsAnimationInMemory(pAnimationSet, &arrAFIFO[i])
				             );

				if (nVLayer == 0)
				{
					uint32 nUseDirIK = 0;
					f32 fDirIKBlend = 0.0f;
					f32 fDirIKInfluence = 0.0f;
					CPoseBlenderAim* pPBAim = static_cast<CPoseBlenderAim*>(m_pSkeletonPose->m_PoseBlenderAim.get());
					if (pPBAim)
					{
						nUseDirIK = pPBAim->m_blender.m_Set.bUseDirIK;
						fDirIKBlend = pPBAim->m_blender.m_Get.fDirIKBlend;
						fDirIKInfluence = pPBAim->m_blender.m_Get.fDirIKInfluence;
					}

					uint32 nUseLookIK = 0;
					f32 fLookIKBlend = 0.0f;
					f32 fLookIKInfluence = 0.0f;
					CPoseBlenderLook* pPBLook = static_cast<CPoseBlenderLook*>(m_pSkeletonPose->m_PoseBlenderLook.get());
					if (pPBLook)
					{
						nUseLookIK = pPBLook->m_blender.m_Set.bUseDirIK;
						fLookIKBlend = pPBLook->m_blender.m_Get.fDirIKBlend;
						fLookIKInfluence = pPBLook->m_blender.m_Get.fDirIKInfluence;
					}
					CryLogAlways("(Try)UseAimIK: %u  AimIKBlend: %.2f  AimIKInfluence: %.2f   (Try)UseLookIK: %u  LookIKBlend: %.2f  LookIKInfluence: %.2f", nUseDirIK, fDirIKBlend, fDirIKInfluence, nUseLookIK, fLookIKBlend, fLookIKInfluence);
				}

				if (pParametric)
				{
					for (uint32 d = 0; d < pParametric->m_numDimensions; d++)
					{
						stack_string motionParamName;
						switch (pParametric->m_MotionParameterID[d])
						{
						case eMotionParamID_TravelSpeed:
							motionParamName = "MoveSpeed";
							break;
						case eMotionParamID_TurnSpeed:
							motionParamName = "TurnSpeed";
							break;
						case eMotionParamID_TravelSlope:
							motionParamName = "TravelSlope";
							break;
						case eMotionParamID_TravelAngle:
							motionParamName = "TravelAngle";
							break;
						case eMotionParamID_TravelDist:
							motionParamName = "TravelDist";
							break;
						case eMotionParamID_TurnAngle:
							motionParamName = "TurnAngle";
							break;
						case eMotionParamID_StopLeg:
							motionParamName = "StopLeg";
							break;
						case eMotionParamID_BlendWeight:
							motionParamName = "BlendWeight";
							break;
						case eMotionParamID_BlendWeight2:
							motionParamName = "BlendWeight2";
							break;
						case eMotionParamID_BlendWeight3:
							motionParamName = "BlendWeight3";
							break;
						case eMotionParamID_BlendWeight4:
							motionParamName = "BlendWeight4";
							break;
						case eMotionParamID_BlendWeight5:
							motionParamName = "BlendWeight5";
							break;
						case eMotionParamID_BlendWeight6:
							motionParamName = "BlendWeight6";
							break;
						case eMotionParamID_BlendWeight7:
							motionParamName = "BlendWeight7";
							break;
						default:
							motionParamName.Format("<Unknown>(%u)", pParametric->m_MotionParameterID[d]);
							break;
						}

						CryLogAlways("%s: %f, locked: %d", motionParamName.c_str(), pParametric->m_MotionParameter[d], pParametric->m_MotionParameterFlags[d] & CA_Dim_LockedParameter);
					}
				}
			}
		}

		const uint poseModifierCount = uint(m_layers[nVLayer].m_poseModifierQueue.GetEntryCount());
		for (uint i = 0; i < poseModifierCount; ++i)
		{
			CryLogAlways("PM class: %s, name %s",
			             m_layers[nVLayer].m_poseModifierQueue.GetEntries()[i].poseModifier->GetFactory()->GetName(),
			             m_layers[nVLayer].m_poseModifierQueue.GetEntries()[i].name);
		}
	}

	const CDefaultSkeleton& modelSkeleton = *m_pInstance->m_pDefaultSkeleton;
	const uint32 adikTargetCount = modelSkeleton.m_ADIKTargets.size();
	for (uint32 adikIndex = 0; adikIndex < adikTargetCount; ++adikIndex)
	{
		const ADIKTarget& adikTarget = modelSkeleton.m_ADIKTargets[adikIndex];

		const int32 targetWeightJointIndex = adikTarget.m_idxWeight;
		if (targetWeightJointIndex < 0)
			continue;

		const QuatT& targetWeightJoint = m_pInstance->m_SkeletonPose.GetPoseData().GetJointRelative(targetWeightJointIndex);
		const float targetBlendWeight = targetWeightJoint.t.x;

		CryLogAlways("ADIK: target: %s weight: %.2f", adikTarget.m_strTarget, targetBlendWeight);
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////
//             this function is handling blending between layers               //
/////////////////////////////////////////////////////////////////////////////////
void CSkeletonAnim::LayerBlendManager(f32 fDeltaTime, uint32 nLayer)
{
	if (nLayer == 0)
		return;

	int8 status = GetActiveLayer(nLayer);

	CTransitionQueue& transitionQueue = m_layers[nLayer].m_transitionQueue;

	f32 time = transitionQueue.m_fLayerTransitionTime;

	f32 fCurLayerBlending = transitionQueue.m_fLayerTransitionWeight;
	if (time < 0)
	{
		fCurLayerBlending = 1.0f;
	}
	else
	{
		if (fDeltaTime == 0.0f)
			fDeltaTime = m_pInstance->m_fOriginalDeltaTime;
	}

	if (time < 0.00001f)
		time = 0.00001f;

	//f32 fColor[4] = {0.5f,0.0f,0.5f,1};
	//g_pAuxGeom->Draw2dLabel( 1,g_YLine, 1.3f, fColor, false,"nLayer: %02d  status: %1d, blend: %f  ", nLayer, status, fCurLayerBlending[nLayer]  );
	//g_YLine+=0x10;
	if (status)
		fCurLayerBlending += fDeltaTime / time;
	else
		fCurLayerBlending -= fDeltaTime / time;

	if (fCurLayerBlending > 1.0f)
		fCurLayerBlending = 1.0f;
	if (fCurLayerBlending < 0.0f)
		fCurLayerBlending = 0.0f;

	uint32 numAnims = transitionQueue.m_animations.size();

	CRY_ASSERT(!status || numAnims > 0);

	if (numAnims && !status && (fCurLayerBlending == 0.0f))
	{
		transitionQueue.m_animations[0].m_DynFlags[0] |= CA_REMOVE_FROM_QUEUE;
	}

	transitionQueue.m_fLayerTransitionWeight = fCurLayerBlending;
	
	for (auto& anim: transitionQueue.m_animations)
	{
		anim.SetTransitionWeight(anim.GetTransitionWeight() * fCurLayerBlending);
	}

	return;
}

const QuatT& CSkeletonAnim::GetRelMovement() const
{
	if (!m_bCachedRelativeMovementValid)
	{
		m_cachedRelativeMovement = CalculateRelativeMovement(m_pInstance->m_fDeltaTime, 0);
		m_bCachedRelativeMovementValid = true;
	}

	return m_cachedRelativeMovement;
}

#ifdef EDITOR_PCDEBUGCODE
bool CSkeletonAnim::ExportHTRAndICAF(const char* szAnimationName, const char* savePath) const
{
	const CDefaultSkeleton* pDefaultSkeleton = m_pInstance->m_pDefaultSkeleton;
	const CAnimationSet* pAnimationSet = pDefaultSkeleton->m_pAnimationSet;

	GlobalAnimationHeaderCAF* pCAFHead = pAnimationSet->GetGAH_CAF(szAnimationName);
	if (pCAFHead)
		return pCAFHead->Export2HTR(szAnimationName, savePath, pDefaultSkeleton);

	const GlobalAnimationHeaderLMG* pLMGHead = pAnimationSet->GetGAH_LMG(szAnimationName);
	if (pLMGHead)
	{
		int32 nAnimID = pAnimationSet->GetAnimIDByName(szAnimationName);
		if (nAnimID < 0)
		{
			gEnv->pLog->LogError("animation-name '%s' not in chrparams file for Model: %s", szAnimationName, m_pInstance->GetIDefaultSkeleton().GetModelFilePath());
			return 0; //fail
		}
		const ModelAnimationHeader* pAnim = pAnimationSet->GetModelAnimationHeader(nAnimID);
		if (pAnim == 0)
			return 0; //fail
		return pLMGHead->Export2HTR(szAnimationName, savePath, pDefaultSkeleton, this);
	}
	return false;

}

bool CSkeletonAnim::ExportVGrid(const char* szAnimationName) const
{
	const CDefaultSkeleton* pDefaultSkeleton = m_pInstance->m_pDefaultSkeleton;
	const CAnimationSet* pAnimationSet = pDefaultSkeleton->m_pAnimationSet;
	const GlobalAnimationHeaderLMG* pLMGHead = pAnimationSet->GetGAH_LMG(szAnimationName);
	if (pLMGHead)
	{
		int32 nAnimID = pAnimationSet->GetAnimIDByName(szAnimationName);
		if (nAnimID < 0)
		{
			gEnv->pLog->LogError("animation-name '%s' not in animation list for Model: %s", szAnimationName, m_pInstance->GetIDefaultSkeleton().GetModelFilePath());
			return false; //fail
		}
		return pLMGHead->ExportVGrid();
	}
	return false;
}
#endif
