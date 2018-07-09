// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "TransitionQueue.h"

#include "CharacterInstance.h"
#include "ParametricSampler.h"
#include "CharacterManager.h"

namespace
{

bool IsAnimationLoaded(const CAnimationSet& animationSet, const CAnimation& animation)
{
	bool bInMemory = true;
	SParametricSamplerInternal* pParametric = (SParametricSamplerInternal*)animation.GetParametricSampler();
	if (pParametric == 0)
	{
		int32 nAnimID = animation.GetAnimationId();
		const ModelAnimationHeader* pAnim = animationSet.GetModelAnimationHeader(nAnimID);
		if (pAnim->m_nAssetType == CAF_File)
		{
			int32 nGlobalID = pAnim->m_nGlobalAnimId;
			GlobalAnimationHeaderCAF& rGAHeader = g_AnimationManager.m_arrGlobalCAF[nGlobalID];
			bInMemory = rGAHeader.IsAssetLoaded() == 1;
		}
		else if (pAnim->m_nAssetType == AIM_File)
		{
			int32 nGlobalID = pAnim->m_nGlobalAnimId;
			GlobalAnimationHeaderAIM& rGAHeader = g_AnimationManager.m_arrGlobalAIM[nGlobalID];
			bInMemory = !rGAHeader.IsAssetOnDemand() && rGAHeader.IsAssetLoaded();
		}

		return bInMemory;
	}

	const uint32 num = pParametric->m_numExamples;
	for (uint32 a = 0; (a < num) && bInMemory; ++a)
	{
		const int32 nAnimID = pParametric->m_nAnimID[a];
		const ModelAnimationHeader* pAnim = animationSet.GetModelAnimationHeader(nAnimID);
		if (pAnim->m_nAssetType == CAF_File)
		{
			int32 nGlobalID = pAnim->m_nGlobalAnimId;
			GlobalAnimationHeaderCAF& rGAHeader = g_AnimationManager.m_arrGlobalCAF[nGlobalID];
			bInMemory = rGAHeader.IsAssetLoaded() == 1;
		}
		if (pAnim->m_nAssetType == AIM_File)
		{
			int32 nGlobalID = pAnim->m_nGlobalAnimId;
			GlobalAnimationHeaderAIM& rGAHeader = g_AnimationManager.m_arrGlobalAIM[nGlobalID];
			bInMemory = !rGAHeader.IsAssetOnDemand() && rGAHeader.IsAssetLoaded();
		}
	}

	return bInMemory;
}

void TriggerEvent(const char* animationName, uint animationId, const CAnimEventData& event, CCharInstance& instance)
{
	instance.m_SkeletonAnim.m_LastAnimEvent.SetAnimEventData(event);

	if (instance.m_SkeletonAnim.m_pEventCallback)
	{
		(*instance.m_SkeletonAnim.m_pEventCallback)(&instance, instance.m_SkeletonAnim.m_pEventCallbackData);
	}
}

void TriggerEvents(const char* animationName, uint animationId, const IAnimEventList& events, float timeFrom, float timeTo, CCharInstance& instance)
{
	const uint32 eventCount = events.GetCount();
	for (uint32 i = 0; i < eventCount; ++i)
	{
		const CAnimEventData& animEvent = events.GetByIndex(i);
		float time = animEvent.GetNormalizedTime();
		if ((timeFrom <= time && timeTo >= time) || (timeFrom <= time + 1.0f && timeTo >= time + 1.0f))
			TriggerEvent(animationName, animationId, animEvent, instance);
	}
}

}

/*
   CTransitionQueue
 */

CTransitionQueue::CTransitionQueue() :
	m_pAnimationSet(nullptr),
	m_bActive(false)
{
	Reset();
}

//

void CTransitionQueue::Reset()
{
	m_fLayerPlaybackScale = 1.0f;
	m_fLayerTransitionWeight = 0.0f;
	m_fLayerTransitionTime = 0.0f;
	m_fLayerBlendWeight = 1.0f;
	m_manualMixingWeight = 0.0f;  //Only used by TrackView. Not sure if really needed
}

void CTransitionQueue::RemoveDelayConditions()
{
	const uint32 numAnimsInQueue = m_animations.size();

	// search for delay conditions and get rid of them
	uint32 nMaxActiveInQueue = min(MAX_EXEC_QUEUE, numAnimsInQueue);
	for (uint32 i = 0; i < nMaxActiveInQueue; ++i)
	{
		const CAnimation& anim = m_animations[i];

		// is anim in memory?
		bool bNotInMemory = !IsAnimationLoaded(*m_pAnimationSet, anim);

		if (!bNotInMemory)
		{
			// Remove flags that could delay the transition (must be in synch with CSkeletonAnim::AppendAnimationToQueue and
			// CSkeletonAnim::EvaluateTransitionFlags)
			m_animations[i].m_nStaticFlags &= ~(CA_START_AT_KEYTIME | CA_START_AFTER | CA_IDLE2MOVE | CA_MOVE2IDLE);

			// and mark it as activated
			m_animations[i].m_DynFlags[0] |= CA_ACTIVATED;
		}
		else
		{
			// Just get rid of it if not in memory
			if (RemoveAnimation(i))
			{
				--i;
				--nMaxActiveInQueue;
			}
		}
	}
}

void CTransitionQueue::PushAnimation(const CAnimation& animation)
{
	if (animation.HasStaticFlag(CA_FORCE_TRANSITION_TO_ANIM))
	{
		RemoveDelayConditions();
	}

	const uint32 numAnimsInQueue = m_animations.size();
	m_animations.push_back(animation);
	m_bActive = true;

	//activate assets immediately, if possible
	uint32 nMaxActiveInQueue = min(MAX_EXEC_QUEUE, numAnimsInQueue);
	for (uint32 i = 0; i < nMaxActiveInQueue; i++)
	{
		CAnimation& anim = m_animations[i];
		uint32 NotInMemory = !IsAnimationLoaded(*m_pAnimationSet, anim);
		if (NotInMemory == 0 && i == 0)
			anim.m_DynFlags[0] |= CA_ACTIVATED;

		if (i == 0)
			continue;

		const CAnimation& previousAnim = m_animations[i - 1];
		if (!previousAnim.IsActivated())
			break;

		uint32 StartAtKeytime = anim.HasStaticFlag(CA_START_AT_KEYTIME);
		uint32 StartAfter = anim.HasStaticFlag(CA_START_AFTER);
		uint32 Idle2Move = anim.HasStaticFlag(CA_IDLE2MOVE);
		uint32 Move2Idle = anim.HasStaticFlag(CA_MOVE2IDLE);
		if ((StartAtKeytime + StartAfter + Idle2Move + Move2Idle + NotInMemory) == 0)
			anim.m_DynFlags[0] |= CA_ACTIVATED;
	}
}

void CTransitionQueue::UnloadAnimationAssets(int index)
{
	CAnimation& animation = m_animations[index];

	int32 nAnimID = animation.GetAnimationId();
	int32 nGlobalID = m_pAnimationSet->GetGlobalIDByAnimID_Fast(nAnimID);
	const ModelAnimationHeader* pAnim = m_pAnimationSet->GetModelAnimationHeader(nAnimID);

	SParametricSamplerInternal* pParametric = (SParametricSamplerInternal*)animation.GetParametricSampler();
	if (pParametric == NULL)
	{
		if (pAnim->m_nAssetType == CAF_File)
		{
#ifdef _DEBUG
			const char* pname = pAnim->GetAnimName();
#endif
			GlobalAnimationHeaderCAF& rGAH = g_AnimationManager.m_arrGlobalCAF[nGlobalID];

			if (rGAH.m_nRef_at_Runtime)
			{
				rGAH.m_nRef_at_Runtime--;
			}
		}
	}
	else
	{
		uint32 numAnims0 = pParametric->m_numExamples;
		for (uint32 i = 0; i < numAnims0; i++)
		{
			nAnimID = pParametric->m_nAnimID[i];
			const ModelAnimationHeader* pSamplerAnim = m_pAnimationSet->GetModelAnimationHeader(nAnimID);
			if (pSamplerAnim && pSamplerAnim->m_nAssetType == CAF_File)
			{
#ifdef _DEBUG
				const char* pname = pSamplerAnim->GetAnimName();
#endif
				int32 nGlobalIDByAnimID = m_pAnimationSet->GetGlobalIDByAnimID_Fast(nAnimID);
				GlobalAnimationHeaderCAF& rGAH = g_AnimationManager.m_arrGlobalCAF[nGlobalIDByAnimID];

				if (rGAH.m_nRef_at_Runtime)
					rGAH.m_nRef_at_Runtime--;
			}
		}

		ptrdiff_t slot = pParametric - g_parametricPool;
		if ((slot >= 0) && (slot < g_totalParametrics))
		{
			CRY_ASSERT_TRACE(g_usedParametrics[slot], ("Releasing unallocated parametric %d", slot));
			g_usedParametrics[slot] = 0;
		}
		else
		{
			CRY_ASSERT_TRACE(0, ("Releasing unpooled parametric %d", pParametric));
		}

		animation.m_pParametricSampler = NULL;
	}
}

bool CTransitionQueue::RemoveAnimation(uint index, bool bForce)
{
	if (index >= (uint32)m_animations.size())
		return false;
	//If not forcing the removal, disallow the removal of active animations
	if (!bForce && m_animations[index].IsActivated())
		return false;

	UnloadAnimationAssets(index);
	m_animations.erase(m_animations.begin() + index);
	if (m_animations.empty())
		m_bActive = false;
	return true;
}

CAnimation& CTransitionQueue::GetAnimation(uint index)
{
	uint32 numAnimsInFifo = m_animations.size();
	if (index >= numAnimsInFifo)
		return g_DefaultAnim;
	return m_animations[index];
}

const CAnimation& CTransitionQueue::GetAnimation(uint index) const
{
	uint32 numAnimsInFifo = m_animations.size();
	if (index >= numAnimsInFifo)
		return g_DefaultAnim;
	return m_animations[index];
}

void CTransitionQueue::Clear()
{
	uint32 numAnimsInLayer = m_animations.size();
	for (size_t i = 0; i < numAnimsInLayer; i++)
		UnloadAnimationAssets(i);
	m_animations.resize(0);
	m_bActive = false;
}

// TEMP: Hack for TrackView.

void CTransitionQueue::ManualSeekAnimation(uint index, float time2, bool bTriggerEvents, CCharInstance& instance)
{
	CAnimation& animation = GetAnimation(index);
	if (animation.HasStaticFlag(CA_MANUAL_UPDATE) || animation.HasStaticFlag(CA_TRACK_VIEW_EXCLUSIVE))
	{
		if (animation.GetParametricSampler() == NULL)
		{
			// this is a CAF file
			int32 animID = animation.GetAnimationId();
			assert(animID >= 0);
			int32 globalID = m_pAnimationSet->GetGlobalIDByAnimID_Fast(animID);
			assert(globalID >= 0);

			uint32 numCAFs = g_AnimationManager.m_arrGlobalCAF.size();
			assert((uint32)globalID < numCAFs);
			if ((globalID >= 0) && (globalID < (int32)numCAFs))
			{
				const ModelAnimationHeader* pAnim = m_pAnimationSet->GetModelAnimationHeader(animID);
				if (pAnim->m_nAssetType == CAF_File)
				{
					const GlobalAnimationHeaderCAF& rGlobalAnimHeader = g_AnimationManager.m_arrGlobalCAF[globalID];

					f32 timeold = animation.GetCurrentSegmentNormalizedTime();
					f32 timenew = time2;

					// prevent sending events when animation is not playing
					// otherwise it may happen to get the event each frame
					bool bProgress = timeold < timenew;
					if (bProgress && bTriggerEvents)
					{
						TriggerEvents(rGlobalAnimHeader.GetFilePath(), animID, rGlobalAnimHeader.m_AnimEventsCAF, timeold, timenew, instance);
					}
				}
			}
		}
		else
		{
			const ModelAnimationHeader* pAnim = m_pAnimationSet->GetModelAnimationHeader(animation.GetAnimationId());
			assert(pAnim->m_nAssetType == LMG_File);

			int32 globalID = pAnim->m_nGlobalAnimId;
			assert(globalID >= 0 && globalID < int(g_AnimationManager.m_arrGlobalLMG.size()));
			if (globalID >= 0 && globalID < int(g_AnimationManager.m_arrGlobalLMG.size()))
			{
				const GlobalAnimationHeaderLMG& rGlobalAnimHeaderLMG = g_AnimationManager.m_arrGlobalLMG[globalID];

				f32 timeold = animation.GetCurrentSegmentNormalizedTime();
				f32 timenew = time2;

				// prevent sending events when animation is not playing
				// otherwise it may happen to get the event each frame
				bool bProgress = timeold < timenew;
				if (bProgress && bTriggerEvents)
				{
					TriggerEvents(rGlobalAnimHeaderLMG.GetFilePath(), animation.GetAnimationId(), rGlobalAnimHeaderLMG.m_AnimEventsLMG, timeold, timenew, instance);
				}
			}
		}

		// Previous time has to be updated as well, otherwise animation events will be incorrectly calculated since prev will be 0 at all times until manual update stops.
		animation.m_fAnimTimePrev[0] = time2;
		// Update the time.
		animation.m_fAnimTime[0] = time2;
	}
}

void CTransitionQueue::SetFirstLayer()
{
	m_fLayerTransitionWeight = 1.0f;
	SetInvalid(m_fLayerBlendWeight);
}

void CTransitionQueue::ApplyManualMixingWeight(uint numAnims)
{
	switch (numAnims)
	{
	case 2:
		m_animations[0].SetTransitionWeight(1.0f - m_manualMixingWeight);
		m_animations[1].SetTransitionWeight(m_manualMixingWeight);
		break;
	case 1:
		m_animations[0].SetTransitionWeight(1.0f);
		break;
	case 0:
		break;
	default:
		CryLog("Too many ( > 2 ) animations in transition queue in TrackViewExclusive mode");
	}
}

void CTransitionQueue::TransitionsBetweenSeveralAnimations(uint numAnims)
{
	// transitions between several animations

	// init time-warping
	for (uint32 i = 1; i < numAnims; i++)
	{
		CAnimation& rPrevAnimation = m_animations[i - 1];
		CAnimation& rCurAnimation = m_animations[i];

		assert(rCurAnimation.IsActivated());  // = true;

		//the new animation determines if we use time-warping or not
		uint32 timewarp = rCurAnimation.m_nStaticFlags & CA_TRANSITION_TIMEWARPING;
		if (timewarp)
		{
			//animations are time-warped, so we have to adjust the delta-time
			f32 tp = rCurAnimation.GetTransitionPriority();
			if (tp == 0)
			{

				if (rPrevAnimation.m_animationId == rCurAnimation.m_animationId)
				{
					if (rCurAnimation.m_pParametricSampler && rPrevAnimation.m_pParametricSampler)
					{
						*rCurAnimation.m_pParametricSampler = *rPrevAnimation.m_pParametricSampler;
					}
				}

				// copy the prev. time from previous
				rCurAnimation.m_fAnimTimePrev[0] = rPrevAnimation.m_fAnimTimePrev[0];
				// copy the time from previous
				rCurAnimation.m_fAnimTime[0] = rPrevAnimation.m_fAnimTime[0];
				assert(rCurAnimation.GetCurrentSegmentNormalizedTime() >= 0.0f && rCurAnimation.GetCurrentSegmentNormalizedTime() <= 1.0f);
				// don't copy the segment from previous
				//rCurAnimation.m_currentSegmentIndex[0]=0;
			}
		}
	}
}

void CTransitionQueue::AdjustTransitionWeights(uint numAnims)
{
	// here we adjust the the TRANSITION-WEIGHTS between all animations in the queue

	// the first in the queue will always have the highest priority
	m_animations[0].SetTransitionWeight(1.0f);
	for (uint32 i = 1; i < numAnims; i++)
	{
		CAnimation& rCurAnimation = m_animations[i];

		rCurAnimation.SetTransitionWeightRequested(rCurAnimation.GetTransitionPriority());
		f32 scale_previous = 1.0f - rCurAnimation.GetTransitionWeight();
		
		for (uint32 j = 0; j < i; j++)
			m_animations[j].SetTransitionWeight(m_animations[j].GetTransitionWeight() * scale_previous);
	}
}

void CTransitionQueue::UpdateTransitionTime(uint numAnims, float fDeltaTime, float trackViewExclusive, float originalDeltaTime)
{
	// update the TRANSITION-TIME of all animations in the queue
	m_animations[0].m_fTransitionPriority = 1.0f;
	for (uint32 i = 1; i < numAnims; i++)
	{
		CAnimation& rCurAnimation = m_animations[i];

		f32 fTransTime = rCurAnimation.m_fTransitionTime;

		// if the animation system is paused, and at the same time user pushes an animation into the queue, then we should set the priority to 1 instead of 0
		if (fDeltaTime == 0.0f && fTransTime == 0.0f)
		{
			rCurAnimation.m_fTransitionPriority = 1.0f;
			continue;
		}

		//we don't want DivByZero
		if (fTransTime == 0.0f)
			fTransTime = 0.0001f;

		f32 ttime = fabsf(fDeltaTime) / fTransTime;
		if (trackViewExclusive)
			ttime = originalDeltaTime / fTransTime;

		// update transition time
		f32 newPriority = min(rCurAnimation.GetTransitionPriority() + ttime, 1.f);
		rCurAnimation.m_fTransitionPriority = newPriority;
	}
}

void CTransitionQueue::AdjustAnimationTimeForTimeWarpedAnimations(uint numAnims)
{
	// In case animations are time-warped, we have to adjust the ANIMATION-TIME
	// in relation to the TRANSITION-WEIGHTS

	//m_animations[0].m_bTWFlagQQQ=false;
	m_animations[0].m_DynFlags[0] &= ~CA_TW_FLAG;
	for (uint32 i = 1; i < numAnims; i++)
	{
		CAnimation& rPrevAnimation = m_animations[i - 1];
		CAnimation& rCurAnimation = m_animations[i];

		bool timewarp = rCurAnimation.HasStaticFlag(CA_TRANSITION_TIMEWARPING);

		if (timewarp)
		{
			rPrevAnimation.m_DynFlags[0] |= CA_TW_FLAG;
			rCurAnimation.m_DynFlags[0] |= CA_TW_FLAG;
		}
		else
		{
			rCurAnimation.m_DynFlags[0] &= ~CA_TW_FLAG;
		}
	}

	f32 fTransitionDelta = 0;
	f32 fTransitionWeight = 0;
	uint32 start = 0;
	uint32 accumented = 0;
	for (uint32 i = 0; i < numAnims; i++)
	{
		const CAnimation& rCurAnimation = m_animations[i];
		if (rCurAnimation.GetUseTimeWarping())
		{
			if (accumented == 0)
				start = i;
			fTransitionWeight += rCurAnimation.GetTransitionWeight();
			fTransitionDelta += rCurAnimation.m_fCurrentDeltaTime * rCurAnimation.GetTransitionWeight();
			accumented++;
		}
	}

	f32 tt = 0.0f;
	if (fTransitionWeight)
		tt = fTransitionDelta / fTransitionWeight;

	//all time-warped animation will get the same delta-time
	for (uint32 a = start; a < (start + accumented); a++)
		m_animations[a].m_fCurrentDeltaTime = tt;
}

/*
   CPoseModifierQueue
 */

void CPoseModifierQueue::SEntry::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(poseModifier);
}

//

CPoseModifierQueue::CPoseModifierQueue() :
	m_currentIndex(false)
{
}

CPoseModifierQueue::~CPoseModifierQueue()
{
}

//

bool CPoseModifierQueue::Push(const IAnimationPoseModifierPtr pPoseModifier, const char* name, const bool bQueued)
{
	const int queueIndex = bQueued ? !m_currentIndex : m_currentIndex;
	const size_t poseModifierCount = m_entries[queueIndex].size();

	// NOTE: To prevent CommandBuffer overflow we currently limit the
	// number of PoseModifiers for each layer to 8.
	// The CommandBuffer allocation scheme should change, not relaying on
	// fixed size, but rather grow as it it being created by making use of
	// the frame-local pool.
	if (poseModifierCount < 8)
	{
		SEntry entry;
		entry.name = name ? name : "Unknown";
		entry.poseModifier = pPoseModifier;
		m_entries[queueIndex].push_back(entry);
		return true;
	}
	return false;
}

void CPoseModifierQueue::Prepare(const SAnimationPoseModifierParams& params)
{
	DynArray<SEntry>& currentQueue = m_entries[m_currentIndex];
	const uint count = currentQueue.size();
	for (uint i = 0; i < count; ++i)
	{
		currentQueue[i].poseModifier->Prepare(params);
	}
}

void CPoseModifierQueue::Synchronize()
{
	DynArray<SEntry>& currentQueue = m_entries[m_currentIndex];
	const uint count = currentQueue.size();
	for (uint i = 0; i < count; ++i)
	{
		currentQueue[i].poseModifier->Synchronize();
	}
}

//

void CPoseModifierQueue::SwapBuffersAndClearActive()
{
	m_entries[m_currentIndex].clear();
	m_currentIndex = !m_currentIndex;
}

void CPoseModifierQueue::ClearAllPoseModifiers()
{
	m_entries[0].clear();
	m_entries[1].clear();
}

//

uint32 CPoseModifierQueue::GetSize()
{
	return m_entries[0].get_alloc_size() + m_entries[1].get_alloc_size();
}

void CPoseModifierQueue::AddToSizer(ICrySizer* pSizer) const
{
	pSizer->AddObject(m_entries[0]);
	pSizer->AddObject(m_entries[1]);
}
