// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a part for vehicles which uses static objects

   -------------------------------------------------------------------------
   History:
   - 23:08:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"

#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartStatic.h"

//------------------------------------------------------------------------
bool CVehiclePartStatic::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo, eVPT_Static))
		return false;

	CVehicleParams staticTable = table.findChild("Static");
	if (!staticTable)
		return false;

	if (!staticTable.haveAttr("filename"))
		return false;

	m_filename = staticTable.getAttr("filename");
	m_filenameDestroyed = staticTable.getAttr("filenameDestroyed");
	m_geometry = staticTable.getAttr("geometry");

	InitGeometry();

	m_state = eVGS_Default;

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartStatic::Release()
{
	if (m_slot != -1)
		m_pVehicle->GetEntity()->FreeSlot(m_slot);

	CVehiclePartBase::Release();
}

//------------------------------------------------------------------------
void CVehiclePartStatic::Reset()
{
	CVehiclePartBase::Reset();
}

//------------------------------------------------------------------------
void CVehiclePartStatic::InitGeometry()
{
	if (m_pSharedParameters->m_isPhysicalized && m_slot > -1)
		GetEntity()->UnphysicalizeSlot(m_slot);

	m_slot = GetEntity()->LoadGeometry(m_slot, m_filename, m_geometry);

	if (IStatObj* pStatObj = GetEntity()->GetStatObj(m_slot))
	{
		if (m_hideCount == 0)
			GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) | ENTITY_SLOT_RENDER);
		else
			GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) & ~(ENTITY_SLOT_RENDER | ENTITY_SLOT_RENDER_NEAREST));
	}

	if (!m_pSharedParameters->m_helperPosName.empty())
	{
		if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(m_pSharedParameters->m_helperPosName))
		{
			Matrix34 tm;
			pHelper->GetVehicleTM(tm);
			GetEntity()->SetSlotLocalTM(m_slot, tm);
		}
	}

	if (m_pParentPart)
	{
		CVehiclePartBase* pAttachToPart = (CVehiclePartBase*) m_pParentPart;

		IEntity* pEntity = pAttachToPart->GetEntity();
		if (pEntity == GetEntity())
		{
			const Matrix34& pParentTM = GetEntity()->GetSlotLocalTM(pAttachToPart->GetSlot(), false);
			const Matrix34& pChildTM = GetEntity()->GetSlotLocalTM(m_slot, false);

			GetEntity()->SetSlotLocalTM(m_slot, pParentTM.GetInverted() * pChildTM);
			GetEntity()->SetParentSlot(pAttachToPart->GetSlot(), m_slot);
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartStatic::Physicalize()
{
	if (m_pSharedParameters->m_isPhysicalized)
	{
		SEntityPhysicalizeParams params;
		params.mass = m_pSharedParameters->m_mass;
		params.density = m_pSharedParameters->m_density;
		params.nSlot = m_slot;
		m_physId = GetEntity()->PhysicalizeSlot(m_slot, params);

		CheckColltypeHeavy(m_physId);
	}
	else if (m_slot != -1)
	{
		GetEntity()->UnphysicalizeSlot(m_slot);
	}

	GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) | ENTITY_SLOT_IGNORE_PHYSICS);
}

//------------------------------------------------------------------------
void CVehiclePartStatic::Update(const float frameTime)
{
	CVehiclePartBase::Update(frameTime);

#if ENABLE_VEHICLE_DEBUG
	if (IsDebugParts())
	{
		//VehicleUtils::DrawTM(GetWorldTM());
	}
#endif
}

//------------------------------------------------------------------------
void CVehiclePartStatic::SetLocalTM(const Matrix34& localTM)
{
	CVehiclePartBase::SetLocalTM(localTM);

	// invalidate, entitysystem doesn't immediately update children otherwise
	GetEntity()->InvalidateTM();
}

DEFINE_VEHICLEOBJECT(CVehiclePartStatic);
