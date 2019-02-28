// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Cry3DEngine/IIndexedMesh.h>
#include <CryPhysics/IPhysics.h>
#include <CryPhysics/IDeferredCollisionEvent.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryEntitySystem/IBreakableManager.h>

class CDelayedPlaneBreak : public IDeferredPhysicsEvent
{
public:
	enum EStatus
	{
		eStatus_NONE,
		eStatus_STARTED,
		eStatus_DONE,
	};

	CDelayedPlaneBreak()
		: m_status(eStatus_NONE)
		, m_count(0)
		, m_iBreakEvent(0)
		, m_idx(0)
		, m_bMeshPrepOnly(false)
		, m_jobState()
	{
	}

	CDelayedPlaneBreak(const CDelayedPlaneBreak& src)
		: m_status(src.m_status)
		, m_islandIn(src.m_islandIn)
		, m_islandOut(src.m_islandOut)
		, m_iBreakEvent(src.m_iBreakEvent)
		, m_idx(src.m_idx)
		, m_count(src.m_count)
		, m_bMeshPrepOnly(src.m_bMeshPrepOnly)
		, m_epc(src.m_epc)
	{
	}

	CDelayedPlaneBreak& operator=(const CDelayedPlaneBreak& src)
	{
		m_status = src.m_status;
		m_islandIn = src.m_islandIn;
		m_islandOut = src.m_islandOut;
		m_iBreakEvent = src.m_iBreakEvent;
		m_idx = src.m_idx;
		m_count = src.m_count;
		m_bMeshPrepOnly = src.m_bMeshPrepOnly;
		m_epc = src.m_epc;
		return *this;
	}

	virtual void              Start()            {}
	virtual void              ExecuteAsJob();
	virtual int               Result(EventPhys*) { return 0; }
	virtual void              Sync()             { gEnv->GetJobManager()->WaitForJob(m_jobState); }
	virtual bool              HasFinished()      { return true; }
	virtual DeferredEventType GetType() const    { return PhysCallBack_OnCollision; }
	virtual EventPhys*        PhysicsEvent()     { return &m_epc; }

	virtual void              Execute();

	volatile int          m_status;
	SExtractMeshIslandIn  m_islandIn;
	SExtractMeshIslandOut m_islandOut;
	int                   m_iBreakEvent;
	int                   m_idx;
	int                   m_count;

	bool                  m_bMeshPrepOnly;

	EventPhysCollision    m_epc;
	JobManager::SJobState m_jobState;
};
