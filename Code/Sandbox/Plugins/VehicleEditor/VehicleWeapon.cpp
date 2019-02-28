// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleWeapon.h"

#include "VehicleData.h"
#include "VehiclePrototype.h"
#include "VehiclePart.h"
#include "VehicleSeat.h"

REGISTER_CLASS_DESC(CVehicleWeaponClassDesc);

IMPLEMENT_DYNCREATE(CVehicleWeapon, CBaseObject)

//////////////////////////////////////////////////////////////////////////
CVehicleWeapon::CVehicleWeapon()
{
	m_pVar = 0;
	m_pSeat = 0;
	m_pVehicle = 0;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleWeapon::UpdateVarFromObject()
{
	if (!m_pVar || !m_pVehicle)
		return;

}

//////////////////////////////////////////////////////////////////////////
void CVehicleWeapon::Done()
{
	VeedLog("[CVehicleWeapon:Done] <%s>", GetName());
	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
bool CVehicleWeapon::HitTest(HitContext& hc)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CVehicleWeapon::Display(CObjectRenderHelper& objRenderHelper)
{
	// todo: draw at mount helper, add rotation limits from parts

	DrawDefault(objRenderHelper.GetDisplayContextRef());
}

//////////////////////////////////////////////////////////////////////////
void CVehicleWeapon::GetBoundBox(AABB& box)
{
	// Transform local bounding box into world space.
	GetLocalBounds(box);
	box.SetTransformedAABB(GetWorldTM(), box);
}

//////////////////////////////////////////////////////////////////////////
void CVehicleWeapon::GetLocalBounds(AABB& box)
{
	// todo
	// return local bounds
}

