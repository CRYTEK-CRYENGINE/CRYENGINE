// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SequencerUtils.h"
#include "SequencerSequence.h"
#include "SequencerNode.h"

//////////////////////////////////////////////////////////////////////////
static void AddBeginKeyTime(const int nkey,
                            CSequencerNode* pAnimNode, CSequencerTrack* pAnimTrack,
                            CSequencerUtils::SelectedKeys& selectedKeys,
                            const bool selectedOnly, const bool noTimeRange,
                            const float t0 = 0.0f, const float t1 = 0.0f)
{
	const float keyTime = pAnimTrack->GetKeyTime(nkey);
	const bool timeRangeOk = noTimeRange || (t0 <= keyTime && keyTime <= t1);
	if ((!selectedOnly || pAnimTrack->IsKeySelected(nkey)) && timeRangeOk)
	{
		selectedKeys.keys.push_back(CSequencerUtils::SelectedKey(pAnimNode, pAnimTrack, nkey, keyTime, CSequencerUtils::SelectedKey::eKeyBegin));
	}
}

static void AddSecondarySelPtTimes(const int nkey,
                                   CSequencerNode* pAnimNode, CSequencerTrack* pAnimTrack,
                                   CSequencerUtils::SelectedKeys& selectedKeys,
                                   const bool selectedOnly, const bool noTimeRange,
                                   const float t0 = 0.0f, const float t1 = 0.0f)
{
	const int num2ndPts = pAnimTrack->GetNumSecondarySelPts(nkey);
	for (int n2ndPt = 1; n2ndPt <= num2ndPts; n2ndPt++)
	{
		const float f2ndPtTime = pAnimTrack->GetSecondaryTime(nkey, n2ndPt);
		const bool timeRangeOk = noTimeRange || (t0 <= (f2ndPtTime) && (f2ndPtTime) <= t1);
		if ((!selectedOnly || pAnimTrack->IsKeySelected(nkey)) && timeRangeOk)
		{
			selectedKeys.keys.push_back(CSequencerUtils::SelectedKey(pAnimNode, pAnimTrack, nkey, f2ndPtTime, CSequencerUtils::SelectedKey::eKeyBlend));
		}
	}
}

static void AddEndKeyTime(const int nkey,
                          CSequencerNode* pAnimNode, CSequencerTrack* pAnimTrack,
                          CSequencerUtils::SelectedKeys& selectedKeys,
                          const bool selectedOnly, const bool noTimeRange,
                          const float t0 = 0.0f, const float t1 = 0.0f)
{
	const float keyTime = pAnimTrack->GetKeyTime(nkey);
	const float duration = pAnimTrack->GetKeyDuration(nkey);
	const bool timeRangeOk = noTimeRange || (t0 <= (keyTime + duration) && (keyTime + duration) <= t1);
	if ((!selectedOnly || pAnimTrack->IsKeySelected(nkey)) && timeRangeOk)
	{
		selectedKeys.keys.push_back(CSequencerUtils::SelectedKey(pAnimNode, pAnimTrack, nkey, keyTime + duration, CSequencerUtils::SelectedKey::eKeyEnd));
	}
}

//////////////////////////////////////////////////////////////////////////
static int GetKeys(const CSequencerSequence* pSequence, CSequencerUtils::SelectedKeys& selectedKeys, const bool selectedOnly, const float t0 = 0, const float t1 = 0)
{
	if (pSequence == NULL)
	{
		assert("(pSequence == NULL) in CSequencerUtils::GetSelectedKeys()" && false);
		return 0;
	}

	int trackType = -1;
	const bool noTimeRange = !(t0 < t1);

	int nNodeCount = pSequence->GetNodeCount();
	for (int node = 0; node < nNodeCount; node++)
	{
		CSequencerNode* pAnimNode = pSequence->GetNode(node);

		int nTrackCount = pAnimNode->GetTrackCount();
		for (int track = 0; track < nTrackCount; track++)
		{
			CSequencerTrack* pAnimTrack = pAnimNode->GetTrackByIndex(track);

			int nKeyCount = pAnimTrack->GetNumKeys();
			for (int nkey = 0; nkey < nKeyCount; nkey++)
			{
				AddBeginKeyTime(nkey, pAnimNode, pAnimTrack, selectedKeys, selectedOnly, noTimeRange, t0, t1);
			}
		}
	}

	return (int)selectedKeys.keys.size();
}

//////////////////////////////////////////////////////////////////////////
static int GetKeysByAnyTimeMarker(const CSequencerSequence* pSequence, CSequencerUtils::SelectedKeys& selectedKeys, const bool selectedOnly, const float t0 = 0.0f, const float t1 = 0.0f)
{
	if (pSequence == NULL)
	{
		assert("(pSequence == NULL) in CSequencerUtils::GetSelectedKeys()" && false);
		return 0;
	}

	int trackType = -1;
	const bool noTimeRange = !(t0 < t1);

	const int nNodeCount = pSequence->GetNodeCount();
	for (int node = 0; node < nNodeCount; node++)
	{
		CSequencerNode* pAnimNode = pSequence->GetNode(node);

		const int nTrackCount = pAnimNode->GetTrackCount();
		for (int track = 0; track < nTrackCount; track++)
		{
			CSequencerTrack* pAnimTrack = pAnimNode->GetTrackByIndex(track);

			const int nKeyCount = pAnimTrack->GetNumKeys();
			for (int nkey = 0; nkey < nKeyCount; nkey++)
			{
				AddBeginKeyTime(nkey, pAnimNode, pAnimTrack, selectedKeys, selectedOnly, noTimeRange, t0, t1);
				AddSecondarySelPtTimes(nkey, pAnimNode, pAnimTrack, selectedKeys, selectedOnly, noTimeRange, t0, t1);
				AddEndKeyTime(nkey, pAnimNode, pAnimTrack, selectedKeys, selectedOnly, noTimeRange, t0, t1);
			}
		}
	}

	return (int)selectedKeys.keys.size();
}

//////////////////////////////////////////////////////////////////////////
int CSequencerUtils::GetSelectedKeys(CSequencerSequence* pSequence, SelectedKeys& selectedKeys)
{
	return GetKeys(pSequence, selectedKeys, true);
}

//////////////////////////////////////////////////////////////////////////
int CSequencerUtils::GetAllKeys(CSequencerSequence* pSequence, SelectedKeys& selectedKeys)
{
	return GetKeys(pSequence, selectedKeys, false);
}

//////////////////////////////////////////////////////////////////////////
int CSequencerUtils::GetSelectedTracks(CSequencerSequence* pSequence, SelectedTracks& selectedTracks)
{
	int trackType = -1;

	int nNodeCount = pSequence ? pSequence->GetNodeCount() : 0;
	for (int node = 0; node < nNodeCount; node++)
	{
		CSequencerNode* pAnimNode = pSequence->GetNode(node);

		int nTrackCount = pAnimNode->GetTrackCount();
		for (int track = 0; track < nTrackCount; track++)
		{
			CSequencerTrack* pAnimTrack = pAnimNode->GetTrackByIndex(track);

			if (pAnimTrack->GetFlags() & CSequencerTrack::SEQUENCER_TRACK_SELECTED)
			{
				SelectedTrack t;
				t.pNode = pAnimNode;
				t.pTrack = pAnimTrack;
				t.m_nSubTrackIndex = -1;
				selectedTracks.tracks.push_back(t);
			}
		}
	}

	return (int)selectedTracks.tracks.size();
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerUtils::IsOneTrackSelected(const SelectedTracks& selectedTracks)
{
	if (selectedTracks.tracks.size() == 0)
		return false;

	if (selectedTracks.tracks.size() == 1)
		return true;

	int baseIndex = -1;
	for (int i = 0; i < (int)selectedTracks.tracks.size(); ++i)
	{
		// It's not a subtrack.
		if (selectedTracks.tracks[i].m_nSubTrackIndex < 0)
			return false;
		if (i == 0)
			baseIndex = selectedTracks.tracks[i].pTrack->GetParameterType()
			            - selectedTracks.tracks[i].m_nSubTrackIndex;
		else
		{
			// It's not a subtrack belong to a same parent track.
			if (baseIndex !=
			    selectedTracks.tracks[i].pTrack->GetParameterType()
			    - selectedTracks.tracks[i].m_nSubTrackIndex)
				return false;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
int CSequencerUtils::GetKeysInTimeRange(const CSequencerSequence* pSequence, SelectedKeys& selectedKeys, const float t0, const float t1)
{
	return GetKeysByAnyTimeMarker(pSequence, selectedKeys, false, t0, t1);
}

//////////////////////////////////////////////////////////////////////////
bool CSequencerUtils::CanAnyKeyBeMoved(const SelectedKeys& selectedKeys)
{
	bool canAnyKeyBeMoved = false;
	for (int k = 0; k < (int)selectedKeys.keys.size(); ++k)
	{
		const CSequencerUtils::SelectedKey& skey = selectedKeys.keys[k];
		canAnyKeyBeMoved |= skey.pTrack->CanMoveKey(skey.nKey);
	}
	return canAnyKeyBeMoved;
}

