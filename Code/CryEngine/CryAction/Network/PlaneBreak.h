// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: 2d plane breaks
   -------------------------------------------------------------------------
   History:
   - 13/08/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __PLANEBREAK_H__
#define __PLANEBREAK_H__

#pragma once

#if !NET_USE_SIMPLE_BREAKAGE

	#include "ProceduralBreak.h"
	#include "ActionGame.h"
	#include "IProceduralBreakType.h"

struct SPlaneBreakParams
{
	SPlaneBreakParams() : breakId(-1) {}
	SPlaneBreakParams(int bid, const SBreakEvent& bev) : breakId(bid), breakEvent(bev) {}
	int         breakId;
	SBreakEvent breakEvent;

	void        SerializeWith(TSerialize ser);
};

struct SPlaneBreak : public SProceduralBreak
{
	DynArray<SBreakEvent> breakEvents;

	virtual void GetAffectedRegion(AABB& pos);
	virtual void AddSendables(INetSendableSink* pSink, int32 brkId);
};

class CPlaneBreak : public IProceduralBreakType
{
public:
	CPlaneBreak(const SBreakEvent& be);

	bool                         AttemptAbsorb(const IProceduralBreakTypePtr& pBT);
	void                         AbsorbStep();
	int                          GetVirtualId(IPhysicalEntity* pEnt);
	CObjectSelector              GetObjectSelector(int idx);
	_smart_ptr<SProceduralBreak> CompleteSend();
	bool                         GotExplosiveObjectState(const SExplosiveObjectState* pState);
	void                         PreparePlayback();
	void                         BeginPlayback(bool hasJointBreaks);
	const char*                  GetName()                                                                                       { return "PlaneBreak"; }
	void                         PatchRecording(DynArray<SProceduralSpawnRec>& spawnRecs, DynArray<SJointBreakRec>& jointBreaks) {}

	virtual bool                 AllowComplete(const SProceduralBreakRecordingState& state);

	virtual bool                 SendOnlyOnClientJoin() { assert(m_bes.size() > 0); return m_bes[0].bFirstBreak == 0; }

private:
	DynArray<SBreakEvent> m_bes;
	IPhysicalEntity*      m_pPhysEnt;
	int                   m_absorbIdx;

	void PreparePlaybackForEvent(int event);

	void PrepareSlot(int idx);

	bool IsOurStatObj(IRenderNode* rn)
	{
		if (m_bes[0].itype == PHYS_FOREIGN_ID_STATIC)
			return m_bes[0].hash == CObjectSelector::CalculateHash(rn);
		else
			return false;
	}

	const EventPhysCollision& GeneratePhysicsEvent(const SBreakEvent& bev);
};

#endif // !NET_USE_SIMPLE_BREAKAGE

#endif
