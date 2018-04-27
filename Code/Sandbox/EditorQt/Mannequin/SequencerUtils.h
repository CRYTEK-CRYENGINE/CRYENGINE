// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SequencerUtils_h__
#define __SequencerUtils_h__
#pragma once

#if _MSC_VER > 1000
	#pragma once
#endif

#include "ISequencerSystem.h"

//////////////////////////////////////////////////////////////////////////
class CSequencerUtils
{
public:
	struct SelectedKey
	{
		enum EKeyTimeType
		{
			eKeyBegin = 0,
			eKeyBlend,
			eKeyLoop,
			eKeyEnd
		};
		SelectedKey() : pNode(NULL), pTrack(NULL), nKey(-1), fTime(0.0f), eTimeType(eKeyBegin) {}
		SelectedKey(CSequencerNode* node, CSequencerTrack* track, int key, float time, EKeyTimeType timeType) :
			pNode(node),
			pTrack(track),
			nKey(key),
			fTime(time),
			eTimeType(timeType)
		{
		}

		_smart_ptr<CSequencerNode>  pNode;
		_smart_ptr<CSequencerTrack> pTrack;
		int                         nKey;
		float                       fTime;
		EKeyTimeType                eTimeType;
	};
	struct SelectedKeys
	{
		std::vector<SelectedKey> keys;
	};

	struct SelectedTrack
	{
		_smart_ptr<CSequencerNode>  pNode;
		_smart_ptr<CSequencerTrack> pTrack;
		int                         m_nSubTrackIndex;
	};
	struct SelectedTracks
	{
		std::vector<SelectedTrack> tracks;
	};

	// Return array of selected keys from the given sequence.
	static int  GetSelectedKeys(CSequencerSequence* pSequence, SelectedKeys& selectedKeys);
	static int  GetAllKeys(CSequencerSequence* pSequence, SelectedKeys& selectedKeys);
	static int  GetSelectedTracks(CSequencerSequence* pSequence, SelectedTracks& selectedTracks);
	// Check whether only one track or subtracks belong to one same track is/are selected.
	static bool IsOneTrackSelected(const SelectedTracks& selectedTracks);
	static int  GetKeysInTimeRange(const CSequencerSequence* pSequence, SelectedKeys& selectedKeys, const float t0, const float t1);
	static bool CanAnyKeyBeMoved(const SelectedKeys& selectedKeys);
};

//////////////////////////////////////////////////////////////////////////
struct ISequencerEventsListener
{
	// Called when Key selection changes.
	virtual void OnKeySelectionChange() = 0;
};

#endif //__SequencerUtils_h__

