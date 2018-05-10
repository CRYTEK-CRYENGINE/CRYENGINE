// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ActionScope.h"
#include "ActionController.h"

#include "AnimationDatabase.h"
#include <CryExtension/CryCreateClassInstance.h>
#include "MannequinDebug.h"

CActionScope::CActionScope(const char* _name, uint32 scopeID, CActionController& actionController, SAnimationContext& _context, SScopeContext& _scopeContext, int layer, int numLayers, const TagState& additionalTags)
	: m_name(_name)
	, m_id(scopeID)
	, m_context(_context)
	, m_scopeContext(_scopeContext)
	, m_actionController(actionController)
	, m_layer(layer)
	, m_numLayers(numLayers)
	, m_speedBias(1.f)
	, m_animWeight(1.f)
	, m_timeIncrement(0.0f)
	, m_additionalTags(additionalTags)
	, m_cachedFragmentTags(TAG_STATE_EMPTY)
	, m_cachedContextStateMask(TAG_STATE_EMPTY)
	, m_cachedaaID(FRAGMENT_ID_INVALID)
	, m_cachedTagSetIdx(TAG_SET_IDX_INVALID)
	, m_lastFragmentID(FRAGMENT_ID_INVALID)
	, m_sequenceFlags(0)
	, m_fragmentTime(0.0f)
	, m_fragmentDuration(0.0f)
	, m_transitionOutroDuration(0.0f)
	, m_transitionDuration(0.0f)
	, m_blendOutDuration(0.0f)
	, m_lastNormalisedTime(0.0f)
	, m_normalisedTime(0.0f)
	, m_mutedAnimLayerMask(0)
	, m_mutedProcLayerMask(0)
	, m_isOneShot(false)
	, m_fragmentInstalled(false)
{
	m_layerSequencers = new SSequencer[numLayers];
}

CActionScope::~CActionScope()
{
	delete[] m_layerSequencers;
}

bool CActionScope::InstallAnimation(int animID, const CryCharAnimationParams& animParams)
{
	ISkeletonAnim& skeletonAnimation = *m_scopeContext.pCharInst->GetISkeletonAnim();
	const bool startAnimationSuccess = skeletonAnimation.StartAnimationById(animID, animParams);
	skeletonAnimation.SetLayerPlaybackScale(animParams.m_nLayerID, m_speedBias);
	skeletonAnimation.SetLayerBlendWeight(animParams.m_nLayerID, m_animWeight);

	return startAnimationSuccess;
}

void CActionScope::StopAnimationOnLayer(uint32 layer, float blendTime)
{
	CRY_ASSERT_MESSAGE(layer < m_numLayers, "Overrunning scope!");

	ISkeletonAnim& skeletonAnimation = *m_scopeContext.pCharInst->GetISkeletonAnim();
	const uint32 actualLayer = layer + m_layer;
	skeletonAnimation.StopAnimationInLayer(actualLayer, blendTime);
}

float CActionScope::GetFragmentStartTime() const
{
	return m_transitionDuration - m_fragmentTime;
}

float CActionScope::CalculateFragmentTimeRemaining() const
{
	float ret = 0.0f;
	if ((m_numLayers > 0) && m_scopeContext.pCharInst)
	{
		SSequencer& sequencer = m_layerSequencers[0];

		if (!sequencer.sequence.empty())
		{
			const float timeTillInstall = sequencer.installTime;
			const uint32 numClips = sequencer.sequence.size();
			ret += max(0.0f, timeTillInstall);
			for (uint32 clip = sequencer.pos + 1; clip < numClips; clip++)
			{
				SAnimClip& animClip = sequencer.sequence[clip];
				ret += animClip.blend.exitTime;
			}

			if (sequencer.pos < numClips)
			{
				//--- Add on the final clip duration
				SAnimClip& animClip = sequencer.sequence[numClips - 1];
				ret += animClip.referenceLength;
			}

			return ret;
		}
	}

	//--- Fallback to the static times
	//--- TODO: Consider updating these values on tick and simplify this function down
	const float totalDuration = m_fragmentDuration + m_transitionDuration + m_transitionOutroDuration;

	return totalDuration - m_fragmentTime;
	;
}

float CActionScope::CalculateFragmentDuration(const CFragment& fragment) const
{
	IAnimationSet* pAnimSet = m_scopeContext.pCharInst ? m_scopeContext.pCharInst->GetIAnimationSet() : NULL;

	float ret = 0.0f;
	if (pAnimSet)
	{
		if (fragment.m_animLayers.size() > 0)
		{
			const TAnimClipSequence& animClipSeq = fragment.m_animLayers[0];

			uint32 numClips = animClipSeq.size();
			float lastDuration = 0.0f;
			for (uint32 i = 0; i < numClips; i++)
			{
				const SAnimClip& animClip = animClipSeq[0];

				if (i > 0)
				{
					if (animClip.blend.exitTime >= 0.0f)
					{
						ret += animClip.blend.exitTime;
					}
					else
					{
						ret += lastDuration;
					}
				}
				lastDuration = 0.0f;
				if (!animClip.animation.IsEmpty() && !(animClip.animation.flags & CA_LOOP_ANIMATION) && (animClip.animation.playbackSpeed > 0.0f))
				{
					int animID = pAnimSet->GetAnimIDByCRC(animClip.animation.animRef.crc);
					if (animID >= 0)
					{
						lastDuration = (pAnimSet->GetDuration_sec(animID) / animClip.animation.playbackSpeed);
					}
				}
			}

			ret += lastDuration;
		}
	}

	return ret;
}

const CAnimation* CActionScope::GetTopAnim(int layer) const
{
	const CAnimation* anim = NULL;
	if (m_scopeContext.pCharInst)
	{
		ISkeletonAnim& skeletonAnimation = *m_scopeContext.pCharInst->GetISkeletonAnim();
		int nAnimsInFIFO = skeletonAnimation.GetNumAnimsInFIFO(m_layer + layer);
		if (nAnimsInFIFO > 0)
		{
			anim = &skeletonAnimation.GetAnimFromFIFO(m_layer + layer, nAnimsInFIFO - 1);
		}
	}

	return anim;
}

CAnimation* CActionScope::GetTopAnim(int layer)
{
	CAnimation* anim = NULL;
	if (m_scopeContext.pCharInst)
	{
		ISkeletonAnim& skeletonAnimation = *m_scopeContext.pCharInst->GetISkeletonAnim();
		int nAnimsInFIFO = skeletonAnimation.GetNumAnimsInFIFO(m_layer + layer);
		if (nAnimsInFIFO > 0)
		{
			anim = &skeletonAnimation.GetAnimFromFIFO(m_layer + layer, nAnimsInFIFO - 1);
		}
	}

	return anim;
}

void CActionScope::IncrementTime(float timeDelta)
{
	m_timeIncrement = timeDelta;
}

void CActionScope::FillBlendQuery(SBlendQuery& query, FragmentID fragID, const SFragTagState& fragTagState, bool isHigherPriority, float* pLoopDuration) const
{
	query.fragmentFrom = m_lastFragmentID;
	query.fragmentTo = fragID;
	query.fragmentTime = m_fragmentTime;
	query.tagStateFrom = m_lastQueueTagState;
	query.tagStateTo.globalTags = m_context.controllerDef.m_tags.GetUnion(fragTagState.globalTags, m_additionalTags);
	query.tagStateTo.fragmentTags = fragTagState.fragmentTags;
	query.additionalTags = m_additionalTags;
	query.prevNormalisedTime = m_lastNormalisedTime;
	query.normalisedTime = m_normalisedTime;
	query.SetFlag(SBlendQuery::higherPriority, isHigherPriority);
	query.SetFlag(SBlendQuery::fromInstalled, m_fragmentInstalled);
	query.SetFlag(SBlendQuery::toInstalled, true);
	query.SetFlag(SBlendQuery::noTransitions, m_actionController.GetFlag(AC_NoTransitions));

	const CAnimation* pAnim = GetTopAnim(0);
	if (pAnim)
	{
		if (pLoopDuration)
		{
			*pLoopDuration = pAnim->GetCurrentSegmentExpectedDurationSeconds();
		}
		query.normalisedTime = pAnim->GetCurrentSegmentNormalizedTime();
	}
}

bool CActionScope::CanInstall(EPriorityComparison priorityComp, FragmentID fragID, const SFragTagState& fragTagState, bool isRequeue, float& timeRemaining) const
{
	IAction* const pCompareAction = m_pAction ? m_pAction.get() : m_pExitingAction.get();
	if (isRequeue || (pCompareAction && pCompareAction->CanBlendOut(priorityComp)) || (!pCompareAction))
	{
		const float totalDuration = m_fragmentDuration + m_transitionDuration + m_transitionOutroDuration;

		if ((priorityComp == Higher)
		    || !pCompareAction
		    || (pCompareAction->m_eStatus == IAction::Finished)
		    || (pCompareAction->m_eStatus == IAction::Exiting))
		{
			timeRemaining = 0.0f;
		}
		else if (m_fragmentTime <= 0.0f)
		{
			return false;
		}
		else if (m_scopeContext.pDatabase == NULL)
		{
			timeRemaining = totalDuration - m_fragmentTime;
		}
		else
		{
			SBlendQuery query;
			float loopDuration;
			FillBlendQuery(query, fragID, fragTagState, false, &loopDuration);

			SBlendQueryResult queryRes1, queryRes2;
			m_scopeContext.pDatabase->FindBestBlends(query, queryRes1, queryRes2);
			if (queryRes1.pFragmentBlend)
			{
				float startTime = queryRes1.pFragmentBlend->startTime;

				if (queryRes1.pFragmentBlend->flags & SFragmentBlend::Cyclic)
				{
					if (queryRes1.pFragmentBlend->flags & SFragmentBlend::CycleLocked)
					{
						float cycleDiff = startTime - m_normalisedTime;
						if ((m_lastNormalisedTime < startTime) || (m_lastNormalisedTime > m_normalisedTime))
						{
							cycleDiff = max(cycleDiff, 0.0f);
						}
						else
						{
							cycleDiff = (float)__fsel(cycleDiff, cycleDiff, 1.0f + cycleDiff);
						}

						timeRemaining = loopDuration * cycleDiff;
					}
					else
					{
						timeRemaining = 0.0f;
					}
				}
				else
				{
					timeRemaining = CalculateFragmentTimeRemaining() + startTime;
				}
			}
			else
			{
				timeRemaining = CalculateFragmentTimeRemaining();
			}
		}

		if (pCompareAction)
		{
			pCompareAction->m_flags |= IAction::TransitionPending;
		}

		return true;
	}
	else
	{
		return false;
	}
}

void CActionScope::InitAnimationParams(const SAnimationEntry& animEntry, const uint32 sequencerLayer, const SAnimBlend& animBlend, CryCharAnimationParams& paramsOut)
{
	CRY_ASSERT_MESSAGE(sequencerLayer < m_numLayers, "Overrunning scope!");

	paramsOut.m_fTransTime = animBlend.duration;
	paramsOut.m_nLayerID = m_layer + sequencerLayer;

	paramsOut.m_nFlags = animEntry.flags | CA_ALLOW_ANIM_RESTART;
	paramsOut.m_fPlaybackSpeed = animEntry.playbackSpeed;

	for (uint32 i = 0; i < MANN_NUMBER_BLEND_CHANNELS; i++)
	{
		paramsOut.m_fUserData[i] = animEntry.blendChannels[i];
	}

	const int animID = m_scopeContext.pCharInst->GetIAnimationSet()->GetAnimIDByCRC(animEntry.animRef.crc);
	const float animDuration = m_scopeContext.pCharInst->GetIAnimationSet()->GetDuration_sec(animID);
	if (0 < animDuration)
	{
		paramsOut.m_fKeyTime = animBlend.startTime / animDuration;
	}
	paramsOut.m_fPlaybackWeight = animEntry.playbackWeight;
#if defined(USE_PROTOTYPE_ABS_BLENDING)
	if (animEntry.weightList != 0)
	{
		const uint8 NUM_WEIGHTLISTS = 3;
		static SJointMask* spJointMask[NUM_WEIGHTLISTS] = { NULL, NULL, NULL };
		static bool sInitialised = false;

		if (!sInitialised)
		{
			sInitialised = true;

			spJointMask[0] = new SJointMask();
			spJointMask[0]->weightList.push_back(SJointMask::SJointWeight(15, 0.3f));
			spJointMask[0]->weightList.push_back(SJointMask::SJointWeight(20, 0.6f));
			spJointMask[0]->weightList.push_back(SJointMask::SJointWeight(22, 1.0f));

			spJointMask[1] = new SJointMask();
			spJointMask[1]->weightList.push_back(SJointMask::SJointWeight(22, 1.0f));

			spJointMask[2] = new SJointMask();
			spJointMask[2]->weightList.push_back(SJointMask::SJointWeight(25, 0.5f));
			spJointMask[2]->weightList.push_back(SJointMask::SJointWeight(37, 1.0f));
		}
		int maskidx = min(animEntry.weightList, NUM_WEIGHTLISTS) - 1;
		paramsOut.m_pJointMask = spJointMask[maskidx];
	}
#endif //!defined(USE_PROTOTYPE_ABS_BLENDING)
	//--- We never want the lower level systems to refuse an install or to pop off something for us
	if ((animEntry.flags & CA_LOOP_ANIMATION) == 0)
	{
		paramsOut.m_nFlags |= CA_REPEAT_LAST_KEY;
	}
	paramsOut.m_nUserToken = m_userToken;
}

bool CActionScope::InstallAnimation(const SAnimationEntry& animEntry, int layer, const SAnimBlend& animBlend)
{
	if ((BIT(layer) & m_mutedAnimLayerMask) == BIT(layer))
	{
		return false;
	}

	if (animEntry.animRef.IsEmpty())
	{
		StopAnimationOnLayer(layer, animBlend.duration);
		return true;
	}
	else
	{
		int animID = m_scopeContext.pCharInst->GetIAnimationSet()->GetAnimIDByCRC(animEntry.animRef.crc);

		if (animID >= 0)
		{
			//--- Cleared for install, install across scopes
			CryCharAnimationParams animParams;
			InitAnimationParams(animEntry, layer, animBlend, animParams);
			return InstallAnimation(animID, animParams);
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Invalid anim ref '%s' on scope '%s' in database '%s'. Skeleton '%s'", animEntry.animRef.c_str(), m_name.c_str(), m_scopeContext.pDatabase->GetFilename(), m_scopeContext.pCharInst->GetIDefaultSkeleton().GetModelFilePath());

			StopAnimationOnLayer(layer, animBlend.duration);
			return false;
		}
	}
}

void CActionScope::QueueAnimFromSequence(uint32 layer, uint32 pos, bool isPersistent)
{
	CRY_ASSERT_MESSAGE(layer < m_numLayers, "Invalid layer idx");
	SSequencer& sequencer = m_layerSequencers[layer];

	if (pos < sequencer.sequence.size())
	{
		const SAnimClip& animClip = sequencer.sequence[pos];
		const SAnimBlend& fragmentBlend = animClip.blend;

		sequencer.blend = fragmentBlend;
		sequencer.installTime = sequencer.blend.exitTime;
		if (pos > 0)
		{
			sequencer.referenceTime = sequencer.sequence[pos - 1].referenceLength;
		}
		sequencer.flags |= eSF_Queued;

		CRY_ASSERT_MESSAGE(sequencer.installTime >= 0.0f, "Invalid exit time!");
	}
	else if (!isPersistent)
	{
		if (pos > 0)
		{
			sequencer.referenceTime = sequencer.sequence[pos - 1].referenceLength;
		}
		sequencer.installTime = sequencer.referenceTime;
		sequencer.flags |= eSF_Queued;
	}
}

void CActionScope::QueueProcFromSequence(uint32 layer, uint32 pos)
{
	CRY_ASSERT_MESSAGE(layer < m_procSequencers.size(), "Invalid layer idx");
	SProcSequencer& sequencer = m_procSequencers[layer];

	if (pos < sequencer.sequence.size())
	{
		sequencer.blend = sequencer.sequence[pos].blend;
		sequencer.installTime = sequencer.blend.exitTime;
		sequencer.flags |= eSF_Queued;
	}
}

int CActionScope::GetNumAnimsInSequence(uint32 layer) const
{
	CRY_ASSERT_MESSAGE(layer < m_numLayers, "Invalid layer idx");
	SSequencer& sequencer = m_layerSequencers[layer];

	return sequencer.sequence.size();
}

void CActionScope::ClipInstalled(uint8 fragPart)
{
	EClipType clipType = m_partTypes[fragPart];
	if (m_pExitingAction && m_pExitingAction->IsTransitioningOut() && (clipType != eCT_TransitionOutro))
	{
		m_actionController.EndAction(*m_pExitingAction);
		m_pExitingAction = NULL;
		if (m_pAction)
		{
			m_actionController.StartAction(*m_pAction);
		}
	}

	if (m_pAction && (&m_pAction->GetRootScope() == this))
	{
		if (clipType == eCT_Transition)
		{
			if (!m_pAction->IsTransitioning())
			{
				m_pAction->TransitionStarted();
			}
		}
		else if ((clipType == eCT_Normal) && !m_pAction->IsPlayingFragment())
		{
			m_pAction->FragmentStarted();
		}
	}
}

void CActionScope::ApplyAnimWeight(uint32 layer, float weight)
{
	CRY_ASSERT_MESSAGE(layer < GetTotalLayers(), "Invalid layer");

	if (layer < GetTotalLayers())
	{
		SSequencer& sequencer = m_layerSequencers[layer];

		if (sequencer.pos > 0)
		{
			CAnimation* pAnimation = GetTopAnim(layer);

			if (pAnimation)
			{
				const SAnimClip& animClip = sequencer.sequence[sequencer.pos - 1];
				float baseWeight = animClip.animation.playbackWeight;

				pAnimation->SetPlaybackWeight(max(0.0f, weight * baseWeight));
			}
		}
	}
}

bool CActionScope::PlayPendingAnim(uint32 layer, float timePassed)
{
	CRY_ASSERT_MESSAGE(layer < m_numLayers, "Invalid layer idx");
	SSequencer& sequencer = m_layerSequencers[layer];
	IActionPtr pPlayingAction = GetPlayingAction();

	const bool isBlendingOut = (sequencer.flags & eSF_BlendingOut) != 0;

	sequencer.flags &= ~eSF_Queued;
	sequencer.installTime = 0.0f;
	sequencer.referenceTime = -1.0f;

	if ((sequencer.pos < sequencer.sequence.size()) || isBlendingOut)
	{
		uint8 fragPart = 0;
		bool isTransition = false;
		const SAnimationEntry* animation = NULL;
		SAnimationEntry animNull;
		animNull.animRef.SetByString(NULL);
		animNull.flags = 0;
		animNull.playbackSpeed = 1.0f;
		animNull.playbackWeight = 1.0f;
		if (!isBlendingOut)
		{
			const SAnimClip& animClip = sequencer.sequence[sequencer.pos];
			fragPart = animClip.part;
			animation = &animClip.animation;
			sequencer.pos++;
		}
		else
		{
			animation = &animNull;
			sequencer.flags &= ~eSF_BlendingOut;
		}

		InstallAnimation(*animation, layer, sequencer.blend);

		if (pPlayingAction && (&pPlayingAction->GetRootScope() == this) && !isBlendingOut)
		{
			ClipInstalled(fragPart);
		}

		QueueAnimFromSequence(layer, sequencer.pos, (animation->flags & CA_LOOP_ANIMATION) != 0);

		return true;
	}
	else
	{
		//--- Notify action about completion
		if (pPlayingAction)
		{
			pPlayingAction->OnSequenceFinished(layer, m_id);
		}

		return false;
	}
}

bool CActionScope::PlayPendingProc(uint32 layer)
{
	CRY_ASSERT_MESSAGE(layer < m_procSequencers.size(), "Invalid layer idx");
	SProcSequencer& sequencer = m_procSequencers[layer];

	sequencer.flags &= ~eSF_Queued;
	const TProcClipSequence& sequence = sequencer.sequence;
	const size_t numSequences = sequence.size();
	const bool bIsBlendOut = ((sequencer.flags & eSF_BlendingOut) != 0);
	if ((sequencer.pos < numSequences) || bIsBlendOut)
	{
		const bool bPlayingProc = !bIsBlendOut;
		if (bPlayingProc)
		{
			const SProceduralEntry& proceduralEntry = sequence[sequencer.pos];
			++sequencer.pos;
			const float duration = (sequencer.pos < numSequences) ? sequence[sequencer.pos].blend.exitTime : -1.0f;
			InstallProceduralClip(proceduralEntry, layer, sequencer.blend, duration);
			ClipInstalled(proceduralEntry.part);
		}
		else
		{
			SProceduralEntry proc;
			InstallProceduralClip(proc, layer, sequencer.blend, -1.f);
			sequencer.flags &= ~eSF_BlendingOut;
		}

		QueueProcFromSequence(layer, sequencer.pos);
		return bPlayingProc;
	}

	return false;
}

bool CActionScope::QueueFragment(FragmentID fragID, const SFragTagState& fragTagState, uint32 optionIdx, float startTime, uint32 userToken, bool isRootScope, bool isHigherPriority, bool principleContext)
{
	if (m_scopeContext.pDatabase == NULL)
	{
		return false;
	}

	SBlendQuery query;
	FillBlendQuery(query, fragID, fragTagState, isHigherPriority, NULL);
	query.SetFlag(SBlendQuery::toInstalled, principleContext);

	SFragmentData fragData;
	IAnimationSet* pAnimSet = m_scopeContext.pCharInst ? m_scopeContext.pCharInst->GetIAnimationSet() : NULL;
	m_sequenceFlags = m_scopeContext.pDatabase->Query(fragData, query, optionIdx, pAnimSet, &m_lastFragSelection);
	m_lastQueueTagState = query.tagStateTo;
	m_lastNormalisedTime = m_normalisedTime = 0.0f;
	m_isOneShot = fragData.isOneShot && ((fragID == FRAGMENT_ID_INVALID) || ((m_context.controllerDef.GetFragmentDef(fragID).flags & SFragmentDef::PERSISTENT) == 0));
	m_blendOutDuration = fragData.blendOutDuration;
	m_fragmentInstalled = principleContext;

	const bool fragmentInstalled = HasFragment();

#if MANNEQUIN_DEBUGGING_ENABLED
	CryStackStringT<char, 128> sTagList = "No Match";
	CryStackStringT<char, 128> sFragTagList;

	if (!fragmentInstalled && (m_layer == 0) && (m_numLayers > 0))
	{
		const char* fragmentName = fragID != FRAGMENT_ID_INVALID ? m_context.controllerDef.m_fragmentIDs.GetTagName(fragID) : "None";
		m_context.controllerDef.m_tags.FlagsToTagList(fragTagState.globalTags, sTagList);
		const CTagDefinition* pFragTagDef = (fragID != FRAGMENT_ID_INVALID) ? m_context.controllerDef.GetFragmentTagDef(fragID) : NULL;
		if (pFragTagDef)
		{
			pFragTagDef->FlagsToTagList(fragTagState.fragmentTags, sFragTagList);
		}
		MannLog(GetActionController(), "Warning - Missing root level fragment %s(%s)", fragmentName, sTagList.c_str());
	}
	if (m_actionController.DebugFragments(principleContext) && fragmentInstalled)
	{
		if (HasFragment())
		{
			m_context.controllerDef.m_tags.FlagsToTagList(m_lastFragSelection.tagState.globalTags, sTagList);
			const CTagDefinition* pFragTagDef = m_context.controllerDef.GetFragmentTagDef(fragID);
			if (pFragTagDef)
			{
				pFragTagDef->FlagsToTagList(m_lastFragSelection.tagState.fragmentTags, sFragTagList);
			}
		}
		const char* fragmentName = fragID != FRAGMENT_ID_INVALID ? m_context.controllerDef.m_fragmentIDs.GetTagName(fragID) : "None";
		const char* prevFragmentName = query.fragmentFrom != FRAGMENT_ID_INVALID ? m_context.controllerDef.m_fragmentIDs.GetTagName(query.fragmentFrom) : "None";

		MannLog(GetActionController(), "Frag %s (%s,%s) queued on %s for action %s", fragmentName, sTagList.c_str(), sFragTagList.c_str(), m_name.c_str(), m_pAction ? m_pAction->GetName() : "None");

		CryStackStringT<char, 128> sTagStateFrom;
		CryStackStringT<char, 128> sTagStateTo;

		SBlendQueryResult queryRes1, queryRes2;
		m_scopeContext.pDatabase->FindBestBlends(query, queryRes1, queryRes2);

		if (queryRes1.pFragmentBlend)
		{
			MannLog(GetActionController(), "Transition from (%s -> %s) %s", (queryRes1.fragmentFrom != FRAGMENT_ID_INVALID) ? prevFragmentName : "Any", (queryRes1.fragmentTo != FRAGMENT_ID_INVALID) ? fragmentName : "Any", queryRes1.pFragmentBlend->IsExitTransition() ? "Exit" : "");

			m_actionController.GetContext().controllerDef.m_tags.FlagsToTagList(queryRes1.tagStateFrom.globalTags, sTagStateFrom);
			m_actionController.GetContext().controllerDef.m_tags.FlagsToTagList(queryRes1.tagStateTo.globalTags, sTagStateTo);

			MannLog(GetActionController(), "Transition tags (%s -> %s)", sTagStateFrom.c_str(), sTagStateTo.c_str());
		}
		if (queryRes2.pFragmentBlend)
		{
			MannLog(GetActionController(), "And Transition from (%s -> %s) %s", (queryRes2.fragmentFrom != FRAGMENT_ID_INVALID) ? prevFragmentName : "Any", (queryRes2.fragmentTo != FRAGMENT_ID_INVALID) ? fragmentName : "Any", queryRes2.pFragmentBlend->IsExitTransition() ? "Exit" : "");

			m_actionController.GetContext().controllerDef.m_tags.FlagsToTagList(queryRes2.tagStateFrom.globalTags, sTagStateFrom);
			m_actionController.GetContext().controllerDef.m_tags.FlagsToTagList(queryRes2.tagStateTo.globalTags, sTagStateTo);

			MannLog(GetActionController(), "Transition tags (%s -> %s)", sTagStateFrom.c_str(), sTagStateTo.c_str());
		}
	}
#endif //MANNEQUIN_DEBUGGING_ENABLED

	m_fragmentDuration = m_transitionOutroDuration = m_transitionDuration = 0.0f;
	for (uint32 i = 0; i < SFragmentData::PART_TOTAL; i++)
	{
		m_partTypes[i] = fragData.transitionType[i];
		switch (fragData.transitionType[i])
		{
		case eCT_Normal:
			m_fragmentDuration += fragData.duration[i];
			break;
		case eCT_Transition:
			m_transitionDuration += fragData.duration[i];
			break;
		case eCT_TransitionOutro:
			m_transitionOutroDuration += fragData.duration[i];
			break;
		}
	}
	if (!isRootScope)
	{
		if (m_sequenceFlags & (eSF_Transition | eSF_TransitionOutro))
		{
			startTime = 0.0f;
		}
		else
		{
			startTime = max(startTime - (m_transitionOutroDuration + m_transitionDuration), 0.0f);
		}
	}
	m_lastFragmentID = fragID;
	m_fragmentTime = -startTime;

	const uint32 numAnimLayers = fragData.animLayers.size();
	const uint32 numScopeLayers = m_numLayers;
	const uint32 numLayers = min(numAnimLayers, numScopeLayers);

	CRY_ASSERT_MESSAGE(numLayers <= m_numLayers, "Invalid layer count");

	m_userToken = userToken;

	uint32 nLayer = 0;
	for (nLayer = 0; nLayer < numLayers; nLayer++)
	{
		const bool hasClips = fragData.animLayers[nLayer].size() > 0;
		SSequencer& sequencer = m_layerSequencers[nLayer];

		sequencer.pos = 0;
		sequencer.referenceTime = -1.0f;

		if (hasClips)
		{
			sequencer.sequence = fragData.animLayers[nLayer];
			sequencer.blend = sequencer.sequence[0].blend;
			sequencer.installTime = startTime + sequencer.blend.exitTime;
			sequencer.flags = eSF_Queued;
		}
		else
		{
			sequencer.sequence.resize(0);

			sequencer.blend = SAnimBlend();
			sequencer.installTime = startTime;
			sequencer.flags = eSF_Queued | eSF_BlendingOut;
		}
	}

	for (; nLayer < numScopeLayers; nLayer++)
	{
		//--- Layers that are not touched by the new fragment blend out using default blends
		SSequencer& sequencer = m_layerSequencers[nLayer];
		sequencer.sequence.resize(0);
		sequencer.pos = 0;

		sequencer.blend = SAnimBlend();
		sequencer.installTime = startTime;
		sequencer.flags = eSF_Queued | eSF_BlendingOut;
	}

	const size_t numProcSequencers = fragData.procLayers.size();
	const size_t totNumProcSequencers = max(numProcSequencers, m_procSequencers.size());
	m_procSequencers.resize(totNumProcSequencers);
	for (nLayer = 0; nLayer < numProcSequencers; nLayer++)
	{
		const bool hasClips = fragData.procLayers[nLayer].size() > 0;
		SProcSequencer& sequencerPush = m_procSequencers[nLayer];
		sequencerPush.pos = 0;

		if (hasClips)
		{
			sequencerPush.sequence = fragData.procLayers[nLayer];

			const float layerBlendTime = sequencerPush.sequence[0].blend.exitTime;
			sequencerPush.installTime = startTime;
			sequencerPush.blend = sequencerPush.sequence[0].blend;
			sequencerPush.flags = eSF_Queued;
			if (layerBlendTime > 0.0f)
			{
				sequencerPush.blend = SAnimBlend();
				sequencerPush.flags |= eSF_BlendingOut;
			}
		}
		else
		{
			sequencerPush.sequence.resize(0);

			sequencerPush.blend = SAnimBlend();
			sequencerPush.installTime = startTime;
			sequencerPush.flags = eSF_Queued | eSF_BlendingOut;
		}
	}
	for (; nLayer < totNumProcSequencers; nLayer++)
	{
		SProcSequencer& sequencerPush = m_procSequencers[nLayer];

		sequencerPush.sequence.resize(0);
		sequencerPush.pos = 0;

		sequencerPush.blend = SAnimBlend();
		sequencerPush.installTime = startTime;
		sequencerPush.flags = eSF_Queued | eSF_BlendingOut;
	}

	return fragmentInstalled;
}

bool CActionScope::IsDifferent(const FragmentID aaID, const TagState& fragmentTags, const TagID subContext /*= TAG_ID_INVALID*/) const
{
	if (m_lastFragmentID != aaID)
	{
		return true;
	}
	else
	{
		uint32 tagSetIdx;
		if (aaID == m_cachedaaID &&
		    fragmentTags == m_cachedFragmentTags &&
		    m_context.state.GetMask() == m_cachedContextStateMask)
		{
			tagSetIdx = m_cachedTagSetIdx;
		}
		else
		{
			SFragmentQuery fragQuery(aaID, SFragTagState(m_context.state.GetMask(), fragmentTags));
			if (subContext != TAG_ID_INVALID)
			{
				const SSubContext subContextDef = m_context.controllerDef.m_subContext[subContext];
				fragQuery.tagState.globalTags = m_context.controllerDef.m_tags.GetUnion(fragQuery.tagState.globalTags, m_context.subStates[subContext].GetMask());
				fragQuery.tagState.globalTags = m_context.controllerDef.m_tags.GetUnion(fragQuery.tagState.globalTags, subContextDef.additionalTags);
			}
			fragQuery.tagState.globalTags = m_context.controllerDef.m_tags.GetUnion(fragQuery.tagState.globalTags, m_additionalTags);
			fragQuery.requiredTags = m_additionalTags;

			// the database may not exist in case of full enslavement
			if (!m_scopeContext.pDatabase)
			{
				return false;
			}
			GetDatabase().FindBestMatchingTag(fragQuery, NULL, &tagSetIdx);

			//--- Store cache
			m_cachedaaID = aaID;
			m_cachedFragmentTags = fragmentTags;
			m_cachedContextStateMask = m_context.state.GetMask();
			m_cachedTagSetIdx = tagSetIdx;
		}

		return tagSetIdx != m_lastFragSelection.tagSetIdx;
	}

	return false;
}

void CActionScope::InstallProceduralClip(const SProceduralEntry& proc, int layer, const SAnimBlend& blend, float duration)
{
	if ((BIT(layer) & m_mutedProcLayerMask) == BIT(layer))
	{
		return;
	}

	SProcSequencer& procSeq = m_procSequencers[layer];
	if (procSeq.proceduralClip)
	{
		procSeq.proceduralClip->OnExit(blend.duration);
		procSeq.proceduralClip.reset();
	}

	const bool isNone = proc.IsNoneType();
	if (!isNone)
	{
		CRY_ASSERT(gEnv && gEnv->pGameFramework);
		static IProceduralClipFactory& s_proceduralClipFactory = gEnv->pGameFramework->GetMannequinInterface().GetProceduralClipFactory();

		procSeq.proceduralClip = s_proceduralClipFactory.CreateProceduralClip(proc.typeNameHash);

		if (procSeq.proceduralClip)
		{
			const CryClassID& contextId = procSeq.proceduralClip->GetContextID();
			if (contextId.hipart != 0 && contextId.lopart != 0)
			{
				if (IProceduralContext* context = m_actionController.FindOrCreateProceduralContext(contextId))
				{
					procSeq.proceduralClip->SetContext(context);
				}
			}

			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(GetEntityId()); // TODO: Handle invalid entity!
			procSeq.proceduralClip->Initialise(*pEntity, *GetCharInst(), *this, *m_pAction);
			procSeq.proceduralClip->INTERNAL_OnEnter(blend.duration, duration, proc.pProceduralParams);
		}
	}
}

void CActionScope::UpdateSequencers(float timePassed)
{
	ISkeletonAnim* pSkelAnim = m_scopeContext.pCharInst ? m_scopeContext.pCharInst->GetISkeletonAnim() : NULL;

	const bool hasIncrement = (m_timeIncrement != 0.0f);
	timePassed += m_timeIncrement;
	m_timeIncrement = 0.0f;

	const int k_MAX_INCREMENTS = 5;
	float queuedIncrements[k_MAX_INCREMENTS];
	int numQueued = 0;

	if (pSkelAnim)
	{
		const CAnimation* pRootAnim = GetTopAnim(0);
		if (pRootAnim)
		{
			m_lastNormalisedTime = m_normalisedTime;
			m_normalisedTime = pRootAnim->GetCurrentSegmentNormalizedTime();
		}

		for (uint32 i = 0; i < m_numLayers; i++)
		{
			SSequencer& sequencer = m_layerSequencers[i];
			float timeLeft = timePassed;
			while ((sequencer.flags & eSF_Queued) != 0)
			{
				const float timeTillInstall = sequencer.installTime;
				if (timeLeft >= timeTillInstall)
				{
					if (PlayPendingAnim(i, timeLeft - timeTillInstall))
					{
						timeLeft -= timeTillInstall;

						if (numQueued < k_MAX_INCREMENTS)
						{
							queuedIncrements[numQueued++] = timeLeft;
						}

						if (sequencer.pos >= sequencer.sequence.size())
							break;
					}
				}
				else
				{
					sequencer.installTime -= timeLeft;
					break;
				}
			}

			if (hasIncrement)
			{
				uint32 layer = m_layer + i;
				uint32 numAnims = pSkelAnim->GetNumAnimsInFIFO(layer);
				if (numAnims > 0)
				{
					for (uint32 anm = 0; anm < numAnims; anm++)
					{
						int timeIncID = (anm + numQueued) - numAnims;
						float timeIncrement = (timeIncID >= 0) ? queuedIncrements[timeIncID] : timePassed;

						if (timeIncrement > 0.0f)
						{
							CAnimation& anim = pSkelAnim->GetAnimFromFIFO(layer, anm);
							float segDuration = m_scopeContext.pCharInst->GetIAnimationSet()->GetDuration_sec(anim.GetAnimationId());
							if (segDuration > 0.0f)
							{
								const float segNormTime = anim.GetCurrentSegmentNormalizedTime();
								const float newTime = (segNormTime * segDuration) + (timeIncrement * anim.GetPlaybackScale());
								float newSegNormTime = newTime / segDuration;
								if (!anim.HasStaticFlag(CA_LOOP_ANIMATION))
								{
									newSegNormTime = min(newSegNormTime, 1.0f);
								}
								else
								{
									newSegNormTime = newSegNormTime - (float)((int)(newSegNormTime));
								}
								anim.SetCurrentSegmentNormalizedTime(newSegNormTime);
							}

							float transitionPriority = 0.0f;
							const float transitionTime = anim.GetTransitionTime();
							if ((transitionTime == 0.0f) || (anm < numAnims - 1))
							{
								transitionPriority = 1.0f;
							}
							else
							{
								transitionPriority = min(anim.GetTransitionPriority() + (timeIncrement / transitionTime), 1.0f);
							}
							anim.SetTransitionPriority(transitionPriority);
						}
					}
				}
			}
		}
	}

	const uint32 numProcSequencers = m_procSequencers.size();
	for (uint32 i = 0; i < numProcSequencers; i++)
	{
		SProcSequencer& sequencer = m_procSequencers[i];
		float timeLeft = timePassed;
		while ((sequencer.flags & eSF_Queued) != 0)
		{
			if (timeLeft >= sequencer.installTime)
			{
				timeLeft -= sequencer.installTime;
				sequencer.installTime = -1.0f;
				PlayPendingProc(i);
			}
			else
			{
				sequencer.installTime -= timeLeft;
				break;
			}
		}

		if (sequencer.proceduralClip)
		{
			sequencer.proceduralClip->Update(timeLeft);
		}
	}
}

void CActionScope::Update(float timePassed)
{
	IAction* const pPlayingAction = GetPlayingAction().get();
	if (pPlayingAction)
	{
		ISkeletonAnim* pSkelAnim = m_scopeContext.pCharInst ? m_scopeContext.pCharInst->GetISkeletonAnim() : NULL;

		const float newSpeedBias = pPlayingAction->GetSpeedBias();
		const float newAnimWeight = pPlayingAction->GetAnimWeight();

		if (m_speedBias != newSpeedBias)
		{
			m_speedBias = newSpeedBias;

			if (pSkelAnim)
			{
				for (uint32 layer = 0; layer < m_numLayers; layer++)
				{
					pSkelAnim->SetLayerPlaybackScale(m_layer + layer, newSpeedBias);
				}
			}
		}

		if (m_animWeight != newAnimWeight)
		{
			m_animWeight = newAnimWeight;

			if (pSkelAnim)
			{
				for (uint32 layer = 0; layer < m_numLayers; layer++)
				{
					pSkelAnim->SetLayerBlendWeight(m_layer + layer, newAnimWeight);
				}
			}
		}

		timePassed *= m_speedBias;

		m_fragmentTime += timePassed + m_timeIncrement;
	}

	UpdateSequencers(timePassed);
}

void CActionScope::BlendOutFragments()
{
	QueueFragment(FRAGMENT_ID_INVALID, SFragTagState(m_context.state.GetMask()), OPTION_IDX_RANDOM, 0.0f, m_userToken);
}

void CActionScope::ClearSequencers()
{
	for (uint32 i = 0; i < m_numLayers; i++)
	{
		SSequencer& sequencer = m_layerSequencers[i];
		sequencer.sequence.resize(0);
		sequencer.installTime = -1.0f;
		sequencer.pos = 0;
		sequencer.flags = 0;
	}
	const uint32 numProcClips = m_procSequencers.size();
	for (uint32 i = 0; i < numProcClips; i++)
	{
		SProcSequencer& sequencer = m_procSequencers[i];
		sequencer.sequence.resize(0);
		sequencer.installTime = -1.0f;
		sequencer.pos = 0;
		sequencer.flags = 0;
	}
}

void CActionScope::Flush(EFlushMethod flushMethod)
{
	ISkeletonAnim* pSkelAnim = m_scopeContext.pCharInst.get() ? m_scopeContext.pCharInst->GetISkeletonAnim() : NULL;

	for (uint32 i = 0; i < m_numLayers; i++)
	{
		SSequencer& sequencer = m_layerSequencers[i];
		//const uint32 numAnims = sequencer.sequence.size();
		sequencer.sequence.resize(0);
		sequencer.installTime = -1.0f;
		sequencer.pos = 0;
		sequencer.flags = 0;

		//clear FIFO regardless of whether actionscope believes it has animations in the sequence or not
		//fixes issue where clearing a scope context wouldn't clear all animations because an empty fragment gets queued first clearing the sequencer
		if (/*(numAnims > 0) &&*/ pSkelAnim && (flushMethod != FM_NormalLeaveAnimations))
		{
			pSkelAnim->ClearFIFOLayer(m_layer + i);
		}
	}
	const int numProcs = m_procSequencers.size();
	for (uint32 i = 0; i < numProcs; i++)
	{
		SProcSequencer& procSeq = m_procSequencers[i];
		if (procSeq.proceduralClip)
		{
			switch (flushMethod)
			{
			case FM_Normal:
			case FM_NormalLeaveAnimations:
				procSeq.proceduralClip->OnExit(0.0f);
				break;
			case FM_Failure:
				procSeq.proceduralClip->OnFail();
				break;
			default:
				CRY_ASSERT(false);
			}
		}
	}
	m_procSequencers.resize(0);

	m_lastFragmentID = FRAGMENT_ID_INVALID;
	m_fragmentTime = 0.0f;
	m_lastFragSelection = SFragmentSelection();
	m_lastQueueTagState = SFragTagState();
	m_sequenceFlags = 0;
}

void CActionScope::Pause()
{
	if (!m_scopeContext.pCharInst)
	{
		return;
	}

	ISkeletonAnim& skeletonAnimation = *m_scopeContext.pCharInst->GetISkeletonAnim();

	for (int i = 0; i < m_numLayers; ++i)
	{
		SSequencer& sequencer = m_layerSequencers[i];

		const int animationLayer = m_layer + i;

		const int animationsInLayer = skeletonAnimation.GetNumAnimsInFIFO(animationLayer);
		if (animationsInLayer == 0)
		{
			sequencer.savedAnimNormalisedTime = -1;
		}
		else
		{
			const int lastAnimationIndex = animationsInLayer - 1;
			const CAnimation& animation = skeletonAnimation.GetAnimFromFIFO(animationLayer, lastAnimationIndex);
			sequencer.savedAnimNormalisedTime = skeletonAnimation.GetAnimationNormalizedTime(&animation);
		}
	}
}

void CActionScope::Resume(float forcedBlendTime, uint32 resumeFlags)
{
	if (!m_scopeContext.pCharInst)
	{
		return;
	}

	IAnimationSet* pAnimSet = m_scopeContext.pCharInst->GetIAnimationSet();
	if (!pAnimSet)
	{
		return;
	}

	ISkeletonAnim& skeletonAnimation = *m_scopeContext.pCharInst->GetISkeletonAnim();

	const bool useDefaultBlendTime = (forcedBlendTime < 0);

	for (int i = 0; i < m_numLayers; ++i)
	{
		SSequencer& sequencer = m_layerSequencers[i];

		const int animationLayer = m_layer + i;
		const float blendTime = useDefaultBlendTime ? sequencer.blend.duration : forcedBlendTime;

		if (sequencer.savedAnimNormalisedTime < 0)
		{
			skeletonAnimation.StopAnimationInLayer(animationLayer, blendTime);
		}
		else
		{
			const uint32 pos = sequencer.pos - 1;
			if (pos < sequencer.sequence.size())
			{
				SAnimClip& clip = sequencer.sequence[pos];
				const int animID = pAnimSet->GetAnimIDByCRC(clip.animation.animRef.crc);
				if (0 <= animID)
				{
					CryCharAnimationParams params;
					InitAnimationParams(clip.animation, i, clip.blend, params);
					const bool installAnimationSuccess = InstallAnimation(animID, params);
					if (installAnimationSuccess)
					{
						const int animationsInLayer = skeletonAnimation.GetNumAnimsInFIFO(animationLayer);
						CRY_ASSERT(1 <= animationsInLayer);

						const bool loopingAnimation = ((clip.animation.flags & CA_LOOP_ANIMATION) != 0);
						const uint32 restoreAnimationTimeFlagToCheck = loopingAnimation ? IActionController::ERF_RestoreLoopingAnimationTime : IActionController::ERF_RestoreNonLoopingAnimationTime;
						const bool restoreAnimationTime = ((restoreAnimationTimeFlagToCheck & resumeFlags) != 0);
						if (restoreAnimationTime)
						{
							const int lastAnimationIndex = animationsInLayer - 1;
							CAnimation& animation = skeletonAnimation.GetAnimFromFIFO(animationLayer, lastAnimationIndex);
							skeletonAnimation.SetAnimationNormalizedTime(&animation, sequencer.savedAnimNormalisedTime);
						}
					}
				}
			}
		}
	}
}

void CActionScope::MuteLayers(uint32 mutedAnimLayerMask, uint32 mutedProcLayerMask)
{
	m_mutedAnimLayerMask = mutedAnimLayerMask;
	m_mutedProcLayerMask = mutedProcLayerMask;
}

bool SScopeContext::HasInvalidCharInst() const
{
	return pCharInst && (pCharInst->GetRefCount() <= 1);
}

bool SScopeContext::HasInvalidEntity() const
{
	IEntity* pExpectedEntity = gEnv->pEntitySystem->GetEntity(entityId);
	return (pExpectedEntity != pCachedEntity);
}
