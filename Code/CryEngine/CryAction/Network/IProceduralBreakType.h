// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: handle different methods of procedurally breaking something
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __IPROCEDURALBREAKTYPE_H__
#define __IPROCEDURALBREAKTYPE_H__

#pragma once

#if !NET_USE_SIMPLE_BREAKAGE

	#include "ProceduralBreak.h"
	#include "ObjectSelector.h"

struct SExplosiveObjectState;

enum EProceduralBreakTypeFlags
{
	ePBTF_NoJointBreaks = BIT(0),
	ePBTF_ChainBreaking = BIT(1)
};

struct IProceduralBreakType;
typedef _smart_ptr<IProceduralBreakType> IProceduralBreakTypePtr;

struct SProceduralBreakRecordingState
{
	SProceduralBreakRecordingState() : gotRemove(false), numEmptySteps(0) {}
	bool gotRemove;
	int  numEmptySteps;
};

struct IProceduralBreakType : public _reference_target_t
{
	const uint8 flags;
	const char  type;

	IProceduralBreakType(uint8 f, char t) : flags(f), type(t) {}

	virtual bool                         AttemptAbsorb(const IProceduralBreakTypePtr& pBT) = 0;
	virtual void                         AbsorbStep() = 0;
	virtual int                          GetVirtualId(IPhysicalEntity* pEnt) = 0;
	virtual bool                         GotExplosiveObjectState(const SExplosiveObjectState* pState) = 0;
	virtual _smart_ptr<SProceduralBreak> CompleteSend() = 0;
	virtual void                         PreparePlayback() = 0;
	virtual void                         BeginPlayback(bool hasJointBreaks) = 0;
	virtual CObjectSelector              GetObjectSelector(int idx) = 0;
	virtual const char*                  GetName() = 0;

	virtual void                         PatchRecording(DynArray<SProceduralSpawnRec>& spawnRecs, DynArray<SJointBreakRec>& jointBreaks) = 0;

	virtual bool                         AllowComplete(const SProceduralBreakRecordingState& state) = 0;

	virtual bool                         SendOnlyOnClientJoin() { return false; }
};

#endif // !NET_USE_SIMPLE_BREAKAGE

#endif
