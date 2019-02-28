// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: state of an object pre-explosion
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __EXPLOSIVEOBJECTSTATE_H__
#define __EXPLOSIVEOBJECTSTATE_H__

#pragma once

struct SExplosiveObjectState
{
	bool     isEnt;
	// for entity
	EntityId entId;
	Vec3     entPos;
	Quat     entRot;
	Vec3     entScale;
	// for statobj
	Vec3     eventPos;
	uint32   hash;
};

bool ExplosiveObjectStateFromPhysicalEntity(SExplosiveObjectState& out, IPhysicalEntity* pEnt);

struct SDeclareExplosiveObjectState : public SExplosiveObjectState
{
	SDeclareExplosiveObjectState() : breakId(-1) {}
	SDeclareExplosiveObjectState(int bid, const SExplosiveObjectState& eos) : breakId(bid), SExplosiveObjectState(eos) {}
	int breakId;

	void SerializeWith(TSerialize ser);
};

#endif
