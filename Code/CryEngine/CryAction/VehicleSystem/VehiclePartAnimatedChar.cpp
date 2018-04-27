// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a part for vehicles which uses animated characters

   -------------------------------------------------------------------------
   History:
   - 24:08:2011: Created by Richard Semmens

*************************************************************************/
#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"

#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimated.h"
#include "VehiclePartAnimatedChar.h"
#include "VehicleUtils.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

//------------------------------------------------------------------------
CVehiclePartAnimatedChar::CVehiclePartAnimatedChar()
{
}

//------------------------------------------------------------------------
CVehiclePartAnimatedChar::~CVehiclePartAnimatedChar()
{
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedChar::Physicalize()
{
	if (m_pSharedParameters->m_isPhysicalized && GetEntity()->GetPhysics())
	{
		if (m_slot != -1)
			GetEntity()->UnphysicalizeSlot(m_slot);

		SEntityPhysicalizeParams params;
		params.mass = m_pSharedParameters->m_mass;
		params.density = m_pSharedParameters->m_density;

		//TODO: Check civcar for 'proper' slot allocation
		params.nSlot = 0; //m_slot;
		params.type = PE_LIVING;
		GetEntity()->PhysicalizeSlot(m_slot, params); // always returns -1 for chars

		if (m_pCharInstance)
		{
			FlagSkeleton(m_pCharInstance->GetISkeletonPose(), m_pCharInstance->GetIDefaultSkeleton());
		}

		m_pVehicle->RequestPhysicalization(this, false);
	}

	GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) | ENTITY_SLOT_IGNORE_PHYSICS);
}

DEFINE_VEHICLEOBJECT(CVehiclePartAnimatedChar);
