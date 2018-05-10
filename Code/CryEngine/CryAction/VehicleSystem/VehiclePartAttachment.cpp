// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a part for vehicles which is the an attachment
   of a parent Animated part

   -------------------------------------------------------------------------
   History:
   - 25:08:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"

#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimated.h"
#include "VehiclePartAttachment.h"
#include "VehicleUtils.h"

//------------------------------------------------------------------------
bool CVehiclePartEntityAttachment::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo, eVPT_Attachment))
		return false;

	m_attachmentId = 0;
	m_localTM.SetIdentity();
	m_worldTM.SetIdentity();

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartEntityAttachment::PostInit()
{
	if (!m_pSharedParameters->m_helperPosName.empty())
	{
		if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(m_pSharedParameters->m_helperPosName))
			SetLocalTM(pHelper->GetLocalTM());
		else
		{
			Matrix34 tm(IDENTITY);
			SetLocalTM(tm);
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartEntityAttachment::Update(const float frameTime)
{
	CVehiclePartBase::Update(frameTime);

	UpdateAttachment();
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartEntityAttachment::GetLocalTM(bool relativeToParentPart, bool forced)
{
	if (relativeToParentPart)
		return m_localTM;
	else
	{
		const Matrix34& tm = LocalToVehicleTM(m_localTM);
		return VALIDATE_MAT(tm);
	}
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartEntityAttachment::GetWorldTM()
{
	const Matrix34& tm = GetLocalTM(false);
	m_worldTM = GetEntity()->GetWorldTM() * tm;

	return VALIDATE_MAT(m_worldTM);
}

//------------------------------------------------------------------------
void CVehiclePartEntityAttachment::SetLocalTM(const Matrix34& localTM)
{
	m_localTM = VALIDATE_MAT(localTM);

	UpdateAttachment();
}

//------------------------------------------------------------------------
const AABB& CVehiclePartEntityAttachment::GetLocalBounds()
{
	IEntity* pEntity = GetAttachmentEntity();

	if (pEntity)
	{
		pEntity->GetLocalBounds(m_bounds);
	}
	else
	{
		m_bounds.Reset();
	}

	return m_bounds;
}

//------------------------------------------------------------------------
IEntity* CVehiclePartEntityAttachment::GetAttachmentEntity()
{
	if (0 == m_attachmentId)
		return 0;

	return gEnv->pEntitySystem->GetEntity(m_attachmentId);
}

//------------------------------------------------------------------------
void CVehiclePartEntityAttachment::UpdateAttachment()
{
	IEntity* pEntity = GetAttachmentEntity();

	if (pEntity)
	{
		const Matrix34& vehicleTM = GetLocalTM(false);
		pEntity->SetLocalTM(vehicleTM);

#if ENABLE_VEHICLE_DEBUG
		if (IsDebugParts())
		{
			VehicleUtils::DrawTM(GetWorldTM());
		}
#endif
	}
}

//------------------------------------------------------------------------
void CVehiclePartEntityAttachment::DetachAttachment()
{
	if (m_attachmentId)
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);

	IEntity* pEntity = GetAttachmentEntity();

	if (pEntity)
	{
		pEntity->DetachThis();
	}

	m_attachmentId = 0;
}

//------------------------------------------------------------------------
void CVehiclePartEntityAttachment::SetAttachmentEntity(EntityId entityId)
{
	DetachAttachment();

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);

	if (!pEntity)
		return;

	m_attachmentId = entityId;
	m_pVehicle->GetEntity()->AttachChild(pEntity);

	UpdateAttachment();

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
}

DEFINE_VEHICLEOBJECT(CVehiclePartEntityAttachment);
