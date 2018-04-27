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

#include "StdAfx.h"
#include "ExplosiveObjectState.h"
#include "ObjectSelector.h"
#include "DebugBreakage.h"
#include "CryAction.h"
#include "GameContext.h"

void SDeclareExplosiveObjectState::SerializeWith(TSerialize ser)
{
	LOGBREAK("SDeclareExplosiveObjectState: %s", ser.IsReading() ? "Reading:" : "Writing");

	ser.Value("breakId", breakId, 'brId');
	ser.Value("isEnt", isEnt);
	if (isEnt)
	{
		if (ser.IsWriting())
			CRY_ASSERT(CCryAction::GetCryAction()->GetGameContext()->GetNetContext()->IsBound(entId));
		ser.Value("entid", entId, 'eid');
		ser.Value("entpos", entPos);
		ser.Value("entrot", entRot);
		ser.Value("entscale", entScale);
	}
	else
	{
		ser.Value("eventPos", eventPos);
		ser.Value("hash", hash);
	}
}

bool ExplosiveObjectStateFromPhysicalEntity(SExplosiveObjectState& s, IPhysicalEntity* pPEnt)
{
	bool ok = false;
	switch (pPEnt->GetiForeignData())
	{
	case PHYS_FOREIGN_ID_ENTITY:
		if (IEntity* pEnt = (IEntity*) pPEnt->GetForeignData(PHYS_FOREIGN_ID_ENTITY))
		{
			s.isEnt = true;
			s.entId = pEnt->GetId();
			s.entPos = pEnt->GetWorldPos();
			s.entRot = pEnt->GetWorldRotation();
			s.entScale = pEnt->GetScale();
			ok = true;
		}
		break;
	case PHYS_FOREIGN_ID_STATIC:
		if (IRenderNode* rn = (IRenderNode*)pPEnt->GetForeignData(PHYS_FOREIGN_ID_STATIC))
		{
			s.isEnt = false;
			s.eventPos = rn->GetBBox().GetCenter();
			s.hash = CObjectSelector::CalculateHash(rn);
			ok = true;
		}
		break;
	}
	return ok;
}
