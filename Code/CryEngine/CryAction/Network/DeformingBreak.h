// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: deforming breaks
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __DEFORMINGBREAK_H__
#define __DEFORMINGBREAK_H__

#pragma once

#include "ProceduralBreak.h"
#include "ActionGame.h"
#include "IProceduralBreakType.h"

void LogDeformPhysicalEntity(const char* from, IPhysicalEntity* pEnt, const Vec3& p, const Vec3& n, float energy);

#if !NET_USE_SIMPLE_BREAKAGE

struct SDeformingBreakParams
{
	SDeformingBreakParams() : breakId(-1) {}
	SDeformingBreakParams(int bid, const SBreakEvent& bev) : breakId(bid), breakEvent(bev)
	{
		breakEvent.iState = eBES_Generated;
		breakEvent.time = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	}
	int         breakId;
	SBreakEvent breakEvent;

	void        SerializeWith(TSerialize ser);
};

struct SDeformingBreak : public SProceduralBreak
{
	DynArray<SBreakEvent> breakEvents;

	virtual void GetAffectedRegion(AABB& pos);
	virtual void AddSendables(INetSendableSink* pSink, int32 brkId);
};

class CDeformingBreak : public IProceduralBreakType
{
public:
	CDeformingBreak(const SBreakEvent& be);

	bool                         AttemptAbsorb(const IProceduralBreakTypePtr& pBT);
	void                         AbsorbStep();
	int                          GetVirtualId(IPhysicalEntity* pEnt);
	CObjectSelector              GetObjectSelector(int idx);
	_smart_ptr<SProceduralBreak> CompleteSend();
	bool                         GotExplosiveObjectState(const SExplosiveObjectState* pState);
	void                         PreparePlayback();
	void                         BeginPlayback(bool hasJointBreaks);
	const char*                  GetName()                                                                                       { return "DeformingBreak"; }
	void                         PatchRecording(DynArray<SProceduralSpawnRec>& spawnRecs, DynArray<SJointBreakRec>& jointBreaks) {}

	virtual bool                 AllowComplete(const SProceduralBreakRecordingState& state);

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
};

#endif // !NET_USE_SIMPLE_BREAKAGE

#endif
