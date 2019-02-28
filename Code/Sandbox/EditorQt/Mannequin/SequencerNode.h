// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SEQUENCER_NODE_h__
#define __SEQUENCER_NODE_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "ISequencerSystem.h"
#include "MannequinBase.h"

struct SControllerDef;
class CSequencerSequence;
class CSequencerTrack;

class CSequencerNode
	: virtual public _i_reference_target_t
{
public:
	enum ESupportedParamFlags
	{
		PARAM_MULTIPLE_TRACKS = 0x01, // Set if parameter can be assigned multiple tracks.
	};

	struct SParamInfo
	{
		SParamInfo() : name(""), paramId(SEQUENCER_PARAM_UNDEFINED), flags(0) {};
		SParamInfo(const char* _name, ESequencerParamType _paramId, int _flags) : name(_name), paramId(_paramId), flags(_flags) {};

		const char*         name;
		ESequencerParamType paramId;
		int                 flags;
	};

public:
	CSequencerNode(CSequencerSequence* sequence, const SControllerDef& controllerDef);
	virtual ~CSequencerNode();

	void                       SetName(const char* name);
	const char*                GetName();

	virtual ESequencerNodeType GetType() const;

	void                       SetSequence(CSequencerSequence* pSequence);
	CSequencerSequence*        GetSequence();

	virtual IEntity*           GetEntity();

	void                       UpdateKeys();

	int                        GetTrackCount() const;
	void                       AddTrack(ESequencerParamType param, CSequencerTrack* track);
	bool                       RemoveTrack(CSequencerTrack* pTrack);
	CSequencerTrack*           GetTrackByIndex(int nIndex) const;

	virtual CSequencerTrack*   CreateTrack(ESequencerParamType nParamId);
	CSequencerTrack*           GetTrackForParameter(ESequencerParamType nParamId) const;
	CSequencerTrack*           GetTrackForParameter(ESequencerParamType nParamId, uint32 index) const;

	void                       SetTimeRange(Range timeRange);

	bool                       GetStartExpanded() const
	{
		return m_startExpanded;
	}

	virtual void InsertMenuOptions(CMenu& menu);
	virtual void ClearMenuOptions(CMenu& menu);
	virtual void OnMenuOption(int menuOption);

	virtual bool CanAddTrackForParameter(ESequencerParamType nParamId) const;

	virtual int  GetParamCount() const;
	virtual bool GetParamInfo(int nIndex, SParamInfo& info) const;

	bool         GetParamInfoFromId(ESequencerParamType paramId, SParamInfo& info) const;
	bool         IsParamValid(ESequencerParamType paramId) const;

	void         Mute(bool bMute) { m_muted = bMute; }
	bool         IsMuted() const  { return m_muted; }

	virtual void UpdateMutedLayerMasks(uint32 mutedAnimLayerMask, uint32 mutedProcLayerMask);

protected:
	string                m_name;
	CSequencerSequence*   m_sequence;
	const SControllerDef& m_controllerDef;

	bool                  m_muted;

	bool                  m_startExpanded;

	// Tracks.
	struct TrackDesc
	{
		ESequencerParamType         paramId; // Track parameter id.
		_smart_ptr<CSequencerTrack> track;
	};

	std::vector<TrackDesc> m_tracks;
};

#endif // __SEQUENCER_NODE_h__

