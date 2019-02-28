// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: generic 'remove parts' events - try not to use
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/

#include "StdAfx.h"
#if !NET_USE_SIMPLE_BREAKAGE
	#include "SimulateRemoveEntityParts.h"
	#include "BreakReplicator.h"

void SSimulateRemoveEntityParts::SerializeWith(TSerialize ser)
{
	CryFatalError("THIS CODE IS DEFUNCT");
	//ent.SerializeWith(ser);
	ser.Value("partIds0", partIds[0]);
	ser.Value("partIds1", partIds[1]);
	ser.Value("partIds2", partIds[2]);
	ser.Value("partIds3", partIds[3]);
	ser.Value("massOrg", massOrg);

	LOGBREAK("SSimulateRemoveEntityParts: %s", ser.IsReading() ? "Reading:" : "Writing");

	LOGBREAK("partIds0: %d", partIds[0]);
	LOGBREAK("partIds1: %d", partIds[1]);
	LOGBREAK("partIds2: %d", partIds[2]);
	LOGBREAK("partIds3: %d", partIds[3]);
	LOGBREAK("massOrg: %f", massOrg);
}

void SSimulateRemoveEntityPartsMessage::SerializeWith(TSerialize ser)
{
	LOGBREAK("SSimulateRemoveEntityPartsMessage: %s", ser.IsReading() ? "Reading:" : "Writing");

	SSimulateRemoveEntityParts::SerializeWith(ser);
	ser.Value("breakId", breakId, 'brId');

	LOGBREAK("breakId: %d", breakId);
}

void SSimulateRemoveEntityPartsInfo::GetAffectedRegion(AABB& aabb)
{
	SMessagePositionInfo center;
	ent.GetPositionInfo(center);
	aabb.min = center.position - Vec3(20);
	aabb.max = center.position + Vec3(20);
}

void SSimulateRemoveEntityPartsInfo::AddSendables(INetSendableSink* pSink, int32 brkId)
{
	CBreakReplicator::SendSimulateRemoveEntityPartsWith(SSimulateRemoveEntityPartsMessage(*this, brkId), pSink);
}
#endif // !NET_USE_SIMPLE_BREAKAGE
