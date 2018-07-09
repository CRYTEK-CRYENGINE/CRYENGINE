// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: recording of procedural breaks
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __PROCEDURALBREAKRECORDINGLISTENER_H__
#define __PROCEDURALBREAKRECORDINGLISTENER_H__

#pragma once

#if !NET_USE_SIMPLE_BREAKAGE

	#include "ProceduralBreakingBaseListener.h"

class CProceduralBreakingRecordingListener : public CProceduralBreakingBaseListener
{
public:
	CProceduralBreakingRecordingListener(IProceduralBreakTypePtr pBreakType);
	~CProceduralBreakingRecordingListener();

	bool AttemptAbsorb(const IProceduralBreakTypePtr& pBT);

private:
	virtual void        GotJointBreak(int idxRef, int id, int epicenter);
	virtual void        GotJointBreakParticle(const EventPhysCreateEntityPart* pEvent);
	virtual int         GotBreakOp(ENetBreakOperation op, int idxRef, int partid, EntityId to);
	virtual void        FinishBreakOp(ENetBreakOperation op, int idxRef, EntityId to, int virtId);
	virtual void        Complete();
	int                 GetEntRef(EntityId id);
	virtual bool        AllowComplete();
	virtual const char* GetListenerName() { return "Recording"; }

	DynArray<EntityId>               m_ents;
	DynArray<SJointBreakRec>         m_jointBreaks;
	DynArray<SProceduralSpawnRec>    m_spawnRecs;
	DynArray<SJointBreakParticleRec> m_gibs;                  // An unclean attempt to sync the initial particle velocities
	std::map<int, int>               m_jointBreaksForIds;
	#if BREAK_HIERARCHICAL_TRACKING
	uint16                           m_frame;
	std::set<int>                    m_openIds;
	#else
	bool                             m_gotRemove;
	#endif

	int m_orderId;
};

#endif // !NET_USE_SIMPLE_BREAKAGE

#endif
