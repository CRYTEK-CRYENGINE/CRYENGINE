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
#ifndef __SIMULATEREMOVEENTITYPARTS_H__
#define __SIMULATEREMOVEENTITYPARTS_H__

#pragma once

#if !NET_USE_SIMPLE_BREAKAGE

	#include "ObjectSelector.h"

struct SSimulateRemoveEntityParts
{
	CObjectSelector ent;
	unsigned int    partIds[4];
	float           massOrg;

	void            SerializeWith(TSerialize ser);
};

struct SSimulateRemoveEntityPartsMessage : public SSimulateRemoveEntityParts
{
	SSimulateRemoveEntityPartsMessage() : breakId(-1) {}
	SSimulateRemoveEntityPartsMessage(const SSimulateRemoveEntityParts& p, int bid) : SSimulateRemoveEntityParts(p), breakId(bid) {}
	int breakId;

	void SerializeWith(TSerialize ser);
};

struct SSimulateRemoveEntityPartsInfo : public IBreakDescriptionInfo, public SSimulateRemoveEntityParts
{
	void GetAffectedRegion(AABB& aabb);
	void AddSendables(INetSendableSink* pSink, int32 brkId);
};

#endif // !NET_USE_SIMPLE_BREAKAGE

#endif
