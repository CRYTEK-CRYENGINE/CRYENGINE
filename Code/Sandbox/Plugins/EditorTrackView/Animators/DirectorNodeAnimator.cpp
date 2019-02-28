// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// CryEngine Source File.
// Copyright (C), Crytek, 1999-2014.

#include "StdAfx.h"
#include "DirectorNodeAnimator.h"
#include "Nodes/TrackViewAnimNode.h"
#include "Nodes/TrackViewTrack.h"
#include "TrackViewSequenceManager.h"
#include "TrackViewPlugin.h"

CDirectorNodeAnimator::CDirectorNodeAnimator(CTrackViewAnimNode* pDirectorNode)
	: m_pDirectorNode(pDirectorNode)
{
	assert(m_pDirectorNode != nullptr);
}

void CDirectorNodeAnimator::Animate(CTrackViewAnimNode* pNode, const SAnimContext& animContext)
{
	if (!pNode->IsActiveDirector())
	{
		// Don't animate if it's not the sequence track of the active director
		return;
	}

	const CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();

	CTrackViewTrack* pSequenceTrack = pNode->GetTrackForParameter(eAnimParamType_Sequence);
	if (pSequenceTrack && !pSequenceTrack->IsDisabled())
	{
		const unsigned int numSequences = pSequenceManager->GetCount();

		std::vector<CTrackViewSequence*> inactiveSequences;
		std::vector<CTrackViewSequence*> activeSequences;

		// Construct sets of sequences that need to be bound/unbound at this point
		const SAnimTime time = animContext.time;
		const unsigned int numKeys = pSequenceTrack->GetKeyCount();
		for (unsigned int i = 0; i < numKeys; ++i)
		{
			CTrackViewKeyHandle keyHandle = pSequenceTrack->GetKey(i);

			SSequenceKey sequenceKey;
			keyHandle.GetKey(&sequenceKey);

			CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByGUID(sequenceKey.m_sequenceGUID);
			if (pSequence)
			{
				if (sequenceKey.m_time <= time)
				{
					stl::push_back_unique(activeSequences, pSequence);
					stl::find_and_erase(inactiveSequences, pSequence);
				}
				else
				{
					if (!stl::find(activeSequences, pSequence))
					{
						stl::push_back_unique(inactiveSequences, pSequence);
					}
				}
			}
		}

		// Unbind must occur before binding, because entities can be referenced in multiple sequences
		for (auto iter = inactiveSequences.begin(); iter != inactiveSequences.end(); ++iter)
		{
			CTrackViewSequence* pSequence = *iter;
			if (pSequence->IsBoundToEditorObjects())
			{
				// No notifications because unbinding would call ForceAnimation again
				CTrackViewSequenceNoNotificationContext context(pSequence);
				pSequence->UnBindFromEditorObjects();
			}
		}

		// Now bind sequences
		for (auto iter = activeSequences.begin(); iter != activeSequences.end(); ++iter)
		{
			CTrackViewSequence* pSequence = *iter;
			if (!pSequence->IsBoundToEditorObjects())
			{
				// No notifications because binding would call ForceAnimation again
				CTrackViewSequenceNoNotificationContext context(pSequence);
				pSequence->BindToEditorObjects();
			}
		}

		// Animate sub sequences
		ForEachActiveSequence(animContext, pSequenceTrack, true,
		                      [&](CTrackViewSequence* pSequence, const SAnimContext& newAnimContext)
		{
			pSequence->Animate(newAnimContext);
		},
		                      [&](CTrackViewSequence* pSequence, const SAnimContext& newAnimContext)
		{
			pSequence->Reset(false);
		}
		                      );
	}
}

void CDirectorNodeAnimator::Render(CTrackViewAnimNode* pNode, const SAnimContext& animContext)
{
	if (!pNode->IsActiveDirector())
	{
		// Don't animate if it's not the sequence track of the active director
		return;
	}

	CTrackViewTrack* pSequenceTrack = pNode->GetTrackForParameter(eAnimParamType_Sequence);
	if (pSequenceTrack && !pSequenceTrack->IsDisabled())
	{
		// Render sub sequences
		ForEachActiveSequence(animContext, pSequenceTrack, false,
		                      [&](CTrackViewSequence* pSequence, const SAnimContext& newAnimContext)
		{
			pSequence->Render(newAnimContext);
		},
		                      [&](CTrackViewSequence* pSequence, const SAnimContext& newAnimContext) {}
		                      );
	}
}

void CDirectorNodeAnimator::ForEachActiveSequence(const SAnimContext& animContext, CTrackViewTrack* pSequenceTrack,
                                                  const bool bHandleOtherKeys, std::function<void(CTrackViewSequence*, const SAnimContext&)> animateFunction,
                                                  std::function<void(CTrackViewSequence*, const SAnimContext&)> resetFunction)
{
	const SAnimTime time = animContext.time;
	const unsigned int numKeys = pSequenceTrack->GetKeyCount();

	if (bHandleOtherKeys)
	{
		// Reset all non-active sequences first
		for (unsigned int i = 0; i < numKeys; ++i)
		{
			CTrackViewKeyHandle keyHandle = pSequenceTrack->GetKey(i);

			SSequenceKey sequenceKey;
			keyHandle.GetKey(&sequenceKey);

			const CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
			CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByGUID(sequenceKey.m_sequenceGUID);
			if (pSequence)
			{
				SAnimContext newAnimContext = animContext;
				const SAnimTime sequenceTime = animContext.time - sequenceKey.m_time + sequenceKey.m_startTime;
				const SAnimTime actualDuration = sequenceKey.m_endTime == SAnimTime(0) ? pSequence->GetTimeRange().Length() : sequenceKey.m_endTime;
				const SAnimTime sequenceDuration = actualDuration + sequenceKey.m_startTime;

				newAnimContext.time = std::min(sequenceTime, sequenceDuration);
				const bool bInsideKeyRange = (sequenceTime >= SAnimTime(0.0f)) && (sequenceTime <= sequenceDuration);

				if (!bInsideKeyRange)
				{
					if (animContext.bForcePlay && sequenceTime >= SAnimTime(0.0f) && newAnimContext.time != pSequence->GetTime())
					{
						// If forcing animation force previous keys to their last playback position
						animateFunction(pSequence, newAnimContext);
					}

					resetFunction(pSequence, newAnimContext);
				}
			}
		}
	}

	for (unsigned int i = 0; i < numKeys; ++i)
	{
		CTrackViewKeyHandle keyHandle = pSequenceTrack->GetKey(i);

		SSequenceKey sequenceKey;
		keyHandle.GetKey(&sequenceKey);

		const CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();
		CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByGUID(sequenceKey.m_sequenceGUID);
		if (pSequence)
		{
			SAnimContext newAnimContext = animContext;
			const SAnimTime sequenceTime = animContext.time - sequenceKey.m_time + sequenceKey.m_startTime;
			const SAnimTime actualDuration = sequenceKey.m_endTime == SAnimTime(0) ? pSequence->GetTimeRange().Length() : sequenceKey.m_endTime;
			const SAnimTime sequenceDuration = actualDuration + sequenceKey.m_startTime;

			newAnimContext.time = (sequenceTime < sequenceDuration) ? sequenceTime : sequenceDuration;
			const bool bInsideKeyRange = (sequenceTime >= SAnimTime(0.0f)) && (sequenceTime <= sequenceDuration);

			if ((bInsideKeyRange && (newAnimContext.time != pSequence->GetTime() || animContext.bForcePlay)))
			{
				animateFunction(pSequence, newAnimContext);
			}
		}
	}
}

void CDirectorNodeAnimator::UnBind(CTrackViewAnimNode* pNode)
{
	const CTrackViewSequenceManager* pSequenceManager = CTrackViewPlugin::GetSequenceManager();

	const unsigned int numSequences = pSequenceManager->GetCount();
	for (unsigned int sequenceIndex = 0; sequenceIndex < numSequences; ++sequenceIndex)
	{
		CTrackViewSequence* pSequence = pSequenceManager->GetSequenceByIndex(sequenceIndex);

		if (pSequence->IsActiveSequence())
		{
			// Don't care about the active sequence
			continue;
		}

		if (pSequence->IsBoundToEditorObjects())
		{
			pSequence->UnBindFromEditorObjects();
		}
	}
}

