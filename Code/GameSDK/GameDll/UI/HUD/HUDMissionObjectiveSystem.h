// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: CHUDMissionObjectives manages the status of a mission objective.
Also the description of the objective is saved here.
CHUDMissionObjectiveSystem encapsulates all objectives of a level.

-------------------------------------------------------------------------
History:
- 01/02:2006: Created by Jan Müller

*************************************************************************/
#ifndef __HUD_MISSION_OBJECTIVE_SYSTEM_H__
#define __HUD_MISSION_OBJECTIVE_SYSTEM_H__

#pragma once

#include <CryEntitySystem/IEntitySystem.h>
#include "UI/UITypes.h"

struct SSingleObjectiveAnalysisData
{
	SSingleObjectiveAnalysisData()
		: m_id("")
		, m_fPercent(0.0f)
	{
	}

	CryFixedStringT<64> m_id;
	float m_fPercent;
};

struct SObjectiveAnalysisData
{
	SObjectiveAnalysisData()
		: m_iOverrideIndex(-1)
		, m_bAttachInWorld(false)
	{
	}

	CryFixedArray<SSingleObjectiveAnalysisData, OBJECTIVEANALYSIS_MAXITEMS> m_singleObjectiveData;
	int m_iOverrideIndex;
	bool m_bAttachInWorld;
};

class CHUDMissionObjectiveSystem;
class CHUDMissionObjective
{
	friend class CHUDMissionObjectiveSystem;

#if ENABLE_HUD_EXTRA_DEBUG
	friend class CHUDSpammer;
#endif // ENABLE_HUD_EXTRA_DEBUG

public:
	enum HUDMissionStatus
	{
		FIRST = 0,
		DEACTIVATED = FIRST,
		COMPLETED,
		FAILED,
		ACTIVATED,
		LAST = ACTIVATED
	};

	CHUDMissionObjective()
		: m_pMOS(nullptr)
		, m_trackedEntityId(0)
		, m_eStatus(DEACTIVATED)
		, m_lastTimeChanged(0)
		, m_secondary(false)
		, m_silent(false)
	{}

	CHUDMissionObjective(CHUDMissionObjectiveSystem* pMOS, const char* id, const char* shortMsg, const char* msg = 0, bool secondaryObjective = false)
											: m_pMOS(pMOS), m_shortMessage(shortMsg), m_screenMessage(msg), m_id(id), m_silent(false)
	{
		m_eStatus = DEACTIVATED;
		m_trackedEntityId = 0;
		m_lastTimeChanged = 0;
		m_secondary = secondaryObjective;
	}

	HUDMissionStatus GetStatus() const
	{
		return m_eStatus;
	}

	void SetStatus(HUDMissionStatus);
	void NotifyStatusChange();

	void SendRadarEvent();

	ILINE void SetSilent(bool silent) { m_silent = silent; }
	ILINE bool IsSilent() const { return m_silent; }

	ILINE bool IsSecondary() const { return m_secondary; }

	ILINE EntityId GetTrackedEntity() const
	{
		if (!m_trackedEntityId && !m_trackedEntity.empty())
		{
			IEntity *pEntity = gEnv->pEntitySystem->FindEntityByName(m_trackedEntity.c_str());
			m_trackedEntityId = (pEntity ? pEntity->GetId() : 0);
		}
		return m_trackedEntityId;
	}
	void SetTrackedEntity(EntityId entityId);
	void SetTrackedEntity(const char* szEntityName);
	void ClearTrackedEntity();

	ILINE bool IsActivated() const
	{
		return (m_eStatus == ACTIVATED);
	}

	ILINE const char* GetShortDescription() const
	{
		return m_shortMessage.c_str();
	}

	ILINE const char* GetID() const
	{
		return m_id.c_str();
	}

	ILINE const char* GetMessage() const
	{
		return m_screenMessage.c_str();
	}

	ILINE float GetLastTimeChanged() const
	{
		return m_lastTimeChanged;
	}

	void Serialize(TSerialize ser);

	void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddObject(m_shortMessage);
		pSizer->AddObject(m_screenMessage);
		pSizer->AddObject(m_id);
	}

private:
	CHUDMissionObjectiveSystem* m_pMOS;
	string						m_shortMessage;
	string						m_screenMessage;
	string						m_id;
	string						m_trackedEntity;
	mutable EntityId			m_trackedEntityId;
	HUDMissionStatus			m_eStatus;
	float						m_lastTimeChanged;
	bool						m_silent;
	bool						m_secondary;
};

class CHUDMissionObjectiveSystem : public IEntitySystemSink
{
#if ENABLE_HUD_EXTRA_DEBUG
	friend class CHUDSpammer;
#endif // ENABLE_HUD_EXTRA_DEBUG

public:
	//this is a list of the current level's mission objectives ...
	typedef std::vector<CHUDMissionObjective> TObjectives;

	CHUDMissionObjectiveSystem();
	virtual ~CHUDMissionObjectiveSystem();

	void Reset();

	// IEntitySystemSink
	virtual bool OnBeforeSpawn( SEntitySpawnParams &params ) { return true; }
	virtual void OnSpawn( IEntity *pEntity,SEntitySpawnParams &params ) { }
	virtual bool OnRemove( IEntity *pEntity ) { return true; }
	virtual void OnReused( IEntity *pEntity, SEntitySpawnParams &params );
	//~IEntitySystemSink

	const TObjectives& GetObjectives() const
	{
		return m_currentMissionObjectives;
	}

	void GetMemoryUsage( ICrySizer *pSizer ) const;

	//loads the level's mission objectives from a XML file
	void LoadLevelObjectives(const char* levelpath, bool forceReloading = false);
	void LoadLevelObjectivesInternal(const char* levelpath);

	//deactivate all loaded objectives
	void DeactivateObjectives(bool leaveOutRecentlyChanged = false);

	//get a pointer to the objective (NULL if not available)
	//TODO: don't return ptr into a vector! If vector changes, ptr is trash!
	CHUDMissionObjective* GetMissionObjective(const char* id);

	CHUDMissionObjective* GetMissionObjectiveByEntityId(EntityId id);

	void Serialize(TSerialize ser);
	void PostSerialize();

	bool StartObjectiveAnalysis(const char* szKey, const EObjectiveAnalysisMode analysisMode);
	void StopObjectiveAnalysis(const bool bSignalResultingObjective = true);
	const SObjectiveAnalysisData* GetObjectiveAnalysisData(const char* szKey) const;

private:
	void LoadObjectiveAnalysisData(const char* levelpath);
	void ClearObjectiveAnalysisData();

	typedef std::map<CryFixedStringT<64>, SObjectiveAnalysisData> TObjectiveAnalysisData;
	TObjectiveAnalysisData m_objectiveAnalysisData;
	TObjectives m_currentMissionObjectives;
	CryFixedStringT<64> m_currentAnalysis;
	bool m_bLoadedObjectives;
};

#endif
