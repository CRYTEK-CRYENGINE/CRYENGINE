// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: basic information for procedural breaks
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/

#include "StdAfx.h"

#if !NET_USE_SIMPLE_BREAKAGE

	#include "ProceduralBreak.h"
	#include "BreakReplicator.h"
	#include "SerializeBits.h"
	#include "SerializeDirHelper.h"

void SProceduralSpawnRec::SerializeWith(TSerialize ser)
{
	ser.EnumValue("typ", op, eNBO_Create, eNBO_NUM_OPERATIONS);
	#define CSER(cond, val) if (BIT(op) & cond) ser.Value( # val, val); else \
	  val = -1
	CSER(OPS_WITH_PARTIDS, partid);
	CSER(OPS_REFERENCING_ENTS, idxRef);
	CSER(OPS_CAUSING_ENTS, idx);
	#undef CSER
}

void SJointBreakRec::SerializeWith(TSerialize ser)
{
	LOGBREAK("SJointBreakRec, %s", ser.IsReading() ? "Reading:" : "Writing");

	ser.Value("ref", idxRef);
	ser.Value("id", id);
	#if BREAK_HIERARCHICAL_TRACKING
	ser.Value("frame", frame);
	#endif
	ser.Value("epicenter", epicenter);
}

#pragma warning(push)
#pragma warning(disable : 6262)// 32k of stack space of CBitArray
void SJointBreakParticleRec::SerializeWith(TSerialize ser)
{
	LOGBREAK("SJointBreakParticleRec");
	CBitArray array(&ser);
	SerializeDirVector(array, vel, 20.f, 8, 8, 8);
	if (ser.IsWriting()) array.WriteToSerializer();
}
#pragma warning(pop)

void SProceduralBreak::AddProceduralSendables(int breakId, INetSendableSink* pSink)
{
	if (magicId >= 0)
		CBreakReplicator::SendSetMagicIdWith(SSetMagicId(breakId, magicId), pSink);
	for (int i = 0; i < jointBreaks.size(); i++)
	{
		CBreakReplicator::SendDeclareJointBreakRecWith(SDeclareJointBreakRec(breakId, jointBreaks[i]), pSink);
	}
	for (int i = 0; i < spawnRecs.size(); i++)
	{
		CBreakReplicator::SendDeclareProceduralSpawnRecWith(SDeclareProceduralSpawnRec(breakId, spawnRecs[i]), pSink);
	}
}

#endif // !NET_USE_SIMPLE_BREAKAGE
