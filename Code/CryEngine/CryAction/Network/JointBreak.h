// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: simple joint breaks
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __JOINTBREAK_H__
#define __JOINTBREAK_H__

#pragma once

#if !NET_USE_SIMPLE_BREAKAGE

	#include "ProceduralBreak.h"
	#include "ActionGame.h"
	#include "IProceduralBreakType.h"
	#include "ExplosiveObjectState.h"
	#include "DebugBreakage.h"

struct SJointBreak : public SProceduralBreak
{
	std::vector<SExplosiveObjectState> states;
	AABB                               region;

	void GetAffectedRegion(AABB& rgn)
	{
		rgn = region;
	}
	void AddSendables(INetSendableSink* pSink, int32 brkId);
};

struct SJointBreakParams
{
	SJointBreakParams() : breakId(-1) {}
	SJointBreakParams(int bid) : breakId(bid) {}
	int  breakId;

	void SerializeWith(TSerialize ser)
	{
		LOGBREAK("SJointBreakParams, %s", ser.IsReading() ? "Reading:" : "Writing");
		ser.Value("breakId", breakId, 'brId');
	}
};

class CJointBreak : public IProceduralBreakType
{
public:
	CJointBreak();
	CJointBreak(IPhysicalEntity* pEnt);

	bool                         AttemptAbsorb(const IProceduralBreakTypePtr& pBT);
	void                         AbsorbStep() {}
	int                          GetVirtualId(IPhysicalEntity* pEnt);
	CObjectSelector              GetObjectSelector(int idx);
	_smart_ptr<SProceduralBreak> CompleteSend();
	bool                         GotExplosiveObjectState(const SExplosiveObjectState* pState);
	void                         PreparePlayback();
	void                         BeginPlayback(bool hasJointBreaks);
	const char*                  GetName() { return "JointBreak"; }
	void                         PatchRecording(DynArray<SProceduralSpawnRec>& spawnRecs, DynArray<SJointBreakRec>& jointBreaks);

	bool                         AllowComplete(const SProceduralBreakRecordingState& state);

private:
	AABB                   m_rgn;
	typedef std::vector<SExplosiveObjectState> TExplosiveObjectStates;
	TExplosiveObjectStates m_states;
};

#endif // !NET_USE_SIMPLE_BREAKAGE

#endif
