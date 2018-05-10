// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Contains an agent's target tracks and handles updating them

   -------------------------------------------------------------------------
   History:
   - 02:01:2010: Created by Kevin Kirst

*************************************************************************/

#ifndef __TARGET_TRACK_GROUP_H__
#define __TARGET_TRACK_GROUP_H__

#include "TargetTrackCommon.h"

class CTargetTrack;
struct IDebugHistoryManager;

// Needed for CreateDummyPotentialTarget
#include <CryAISystem/IPerceptionHandler.h>

class CTargetTrackGroup
{
public:
	CTargetTrackGroup(TargetTrackHelpers::ITargetTrackPoolProxy* pTrackPoolProxy, tAIObjectID aiObjectId, uint32 uConfigHash, int nTargetLimit);
	~CTargetTrackGroup();

	tAIObjectID GetAIObjectID() const       { return m_aiObjectId; }
	tAIObjectID GetLastBestTargetID() const { return m_aiLastBestTargetId; }
	uint32      GetConfigHash() const       { return m_uConfigHash; }
	int         GetTargetLimit() const      { return m_nTargetLimit; }
	bool        IsEnabled() const           { return m_bEnabled; }
	void        SetEnabled(bool bEnabled)   { m_bEnabled = bEnabled; }

	void        Reset();
	void        Serialize_Write(TSerialize ser);
	void        Serialize_Read(TSerialize ser);

	void        Update(TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy);

	bool        HandleStimulusEvent(const TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, uint32 uStimulusNameHash);
	bool        TriggerPulse(tAIObjectID targetID, uint32 uStimulusNameHash, uint32 uPulseNameHash);
	bool        TriggerPulse(uint32 uStimulusNameHash, uint32 uPulseNameHash);
	bool        GetDesiredTarget(TargetTrackHelpers::EDesiredTargetMethod eMethod, CWeakRef<CAIObject>& outTarget, SAIPotentialTarget*& pOutTargetInfo);
	uint32      GetBestTrack(TargetTrackHelpers::EDesiredTargetMethod eMethod, CTargetTrack** tracks, uint32 maxCount);

	bool        IsPotentialTarget(tAIObjectID aiTargetId) const;
	bool        IsDesiredTarget(tAIObjectID aiTargetId) const;

#ifdef TARGET_TRACK_DEBUG
	// Debugging
	void DebugDrawTracks(TargetTrackHelpers::ITargetTrackConfigProxy* pConfigProxy, bool bLastDraw);
	void DebugDrawTargets(int nMode, int nTargetedCount, bool bExtraInfo = false);
#endif //TARGET_TRACK_DEBUG

	typedef VectorMap<tAIObjectID, CTargetTrack*> TTargetTrackContainer;

	TTargetTrackContainer& GetTargetTracks() { return m_TargetTracks; }

private:
	// Copy-construction and assignment not supported
	CTargetTrackGroup(CTargetTrackGroup const&);
	CTargetTrackGroup& operator=(CTargetTrackGroup const&);

	void               UpdateSortedTracks(bool bForced = false);
	bool               TestTrackAgainstFilters(CTargetTrack* pTrack, TargetTrackHelpers::EDesiredTargetMethod eMethod) const;

	// Stimulus handling helpers
	bool HandleStimulusEvent_All(const TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, uint32 uStimulusNameHash);
	bool HandleStimulusEvent_Target(const TargetTrackHelpers::STargetTrackStimulusEvent& stimulusEvent, uint32 uStimulusNameHash, CTargetTrack* pTrack);

	// Returns the target track to be used for the given id
	CTargetTrack* GetTargetTrack(tAIObjectID aiTargetId);
	void          DeleteTargetTracks();

	//void UpdateTargetRepresentation(const CTargetTrack *pBestTrack, tAIObjectID &outTargetId, SAIPotentialTarget* &pOutTargetInfo);
	void UpdateTargetRepresentation(const CTargetTrack* pBestTrack, CWeakRef<CAIObject>& outTarget, SAIPotentialTarget*& pOutTargetInfo);

	void InitDummy(); // set common dummy properties after creation.

private:
	TTargetTrackContainer m_TargetTracks;

	typedef std::vector<CTargetTrack*> TSortedTracks;
	TSortedTracks                              m_SortedTracks;

	TargetTrackHelpers::ITargetTrackPoolProxy* m_pTrackPoolProxy;
	tAIObjectID                                m_aiObjectId;
	tAIObjectID                                m_aiLastBestTargetId;
	uint32             m_uConfigHash;
	int                m_nTargetLimit;
	bool               m_bNeedSort;
	bool               m_bEnabled;

	SAIPotentialTarget m_dummyPotentialTarget;

#ifdef TARGET_TRACK_DEBUG
	// Debugging
	bool FindFreeGraphSlot(uint32& outIndex) const;

	float                 m_fLastGraphUpdate;
	IDebugHistoryManager* m_pDebugHistoryManager;
	enum { DEBUG_GRAPH_OCCUPIED_SIZE = 16 };
	bool                  m_bDebugGraphOccupied[DEBUG_GRAPH_OCCUPIED_SIZE];
#endif //TARGET_TRACK_DEBUG
};

#endif //__TARGET_TRACK_GROUP_H__
