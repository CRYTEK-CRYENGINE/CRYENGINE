// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a simple box-shaped part for mass distribution

   -------------------------------------------------------------------------
   History:
   - 17:10:2005: Created by MichaelR

*************************************************************************/
#include "StdAfx.h"

#include "IVehicleSystem.h"
#include "VehiclePartMassBox.h"
#include "CryAction.h"
#include "Vehicle.h"

//------------------------------------------------------------------------
CVehiclePartMassBox::CVehiclePartMassBox()
{
	m_drivingOffset = 0.f;
	m_Driving = eMASSBOXDRIVING_DEFAULT;
	m_hideCount = 1; // eg hidden
	m_size.zero();

	m_localTM.SetIdentity();
}

//------------------------------------------------------------------------
CVehiclePartMassBox::~CVehiclePartMassBox()
{
}

//------------------------------------------------------------------------
bool CVehiclePartMassBox::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo, eVPT_Massbox))
		return false;

	m_size.Set(1.f, 2.f, 0.75f); // default

	CVehicleParams subTable = table.findChild("MassBox");
	if (subTable)
	{
		subTable.getAttr("size", m_size);
		subTable.getAttr("drivingOffset", m_drivingOffset);
	}

	m_localTM.SetTranslation(m_pSharedParameters->m_position);

	m_bounds.min = -m_size;
	m_bounds.max = m_size;
	m_bounds.SetTransformedAABB(m_localTM, m_bounds);

	if (m_pSharedParameters->m_mass == 0 && m_pSharedParameters->m_density > 0)
		m_pVehicle->m_mass += m_pSharedParameters->m_density * 8 * m_size.x * m_size.y * m_size.z;

	m_state = eVGS_Default;

	if (m_drivingOffset != 0.f && m_pVehicle->GetWheelCount())
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartMassBox::Physicalize()
{
	if (!m_pSharedParameters->m_isPhysicalized)
		return;

	IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
	if (!pPhysics)
		return;

	// create geometry
	primitives::box box;
	box.Basis.SetIdentity();
	box.center.zero();
	box.size = m_size;

	IGeomManager* pGeomMan = gEnv->pPhysicalWorld->GetGeomManager();
	IGeometry* pGeom = pGeomMan->CreatePrimitive(primitives::box::type, &box);
	phys_geometry* pPhysGeom = pGeomMan->RegisterGeometry(pGeom);
	pGeom->Release();
	pPhysGeom->nRefCount = 0;

	pe_geomparams pg;
	pg.mass = m_pSharedParameters->m_mass;
	pg.density = m_pSharedParameters->m_density;
	pg.pos = m_pSharedParameters->m_position;
	pg.flags &= ~(geom_collides | geom_floats);
	pg.flagsCollider = 0;

	m_physId = pPhysics->AddGeometry(pPhysGeom, &pg, m_pVehicle->GetNextPhysicsSlot(true));
	SetupDriving(eMASSBOXDRIVING_DEFAULT);

	if (-1 == m_physId)
		CryLog("[CVehiclePartMassBox]: AddGeometry failed! <%s>", m_pVehicle->GetEntity()->GetName());
}

//------------------------------------------------------------------------
void CVehiclePartMassBox::Reset()
{
	SetupDriving(eMASSBOXDRIVING_DEFAULT);
	CVehiclePartBase::Reset();
}

//------------------------------------------------------------------------
void CVehiclePartMassBox::SetupDriving(EMassBoxDrivingType driving)
{
	if (m_Driving == driving)
		return;

	if (m_drivingOffset == 0.f || m_physId == -1)
		return;

	//CryLog("%s: setup driving (%i)", GetEntity()->GetName(), drive);

	IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();

	pe_params_part params;
	params.partid = m_physId;
	if (pPhysics->GetParams(&params))
	{

		pe_params_part newparams;
		newparams.partid = m_physId;
		newparams.pos = m_pSharedParameters->m_position;

		if (driving == eMASSBOXDRIVING_NORMAL || driving == eMASSBOXDRIVING_INTHEWATER)
			newparams.pos.z += m_drivingOffset;
		else if (driving == eMASSBOXDRIVING_INTHEAIR)
			newparams.pos.z -= m_drivingOffset;

		pPhysics->SetParams(&newparams);
		m_Driving = driving;
	}
}

//------------------------------------------------------------------------
void CVehiclePartMassBox::Update(const float frameTime)
{
	IVehicleMovement* pMovement = m_pVehicle->GetMovement();
	const SVehicleStatus& status = m_pVehicle->GetStatus();
	EMassBoxDrivingType driving;

	if (status.health == 0.0f)
	{
		driving = eMASSBOXDRIVING_DESTROYED;
	}
	else if (status.speed > 0.5f && status.passengerCount > 0)
	{
		int wheelContacts = pMovement ? pMovement->GetWheelContacts() : 0;
		bool contact = wheelContacts >= 0.5f * m_pVehicle->GetWheelCount() - 1;
		if (status.submergedRatio > 0.0f)
			driving = eMASSBOXDRIVING_INTHEWATER;
		else if (contact)
			driving = eMASSBOXDRIVING_NORMAL;
		else
			driving = eMASSBOXDRIVING_INTHEAIR;
	}
	else
	{
		driving = eMASSBOXDRIVING_DEFAULT;
	}

	SetupDriving(driving);
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartMassBox::GetLocalTM(bool relativeToParentPart, bool forced)
{
	return m_localTM;
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartMassBox::GetWorldTM()
{
	m_worldTM = GetEntity()->GetWorldTM() * GetLocalTM(false);
	return m_worldTM;
}

//------------------------------------------------------------------------
const AABB& CVehiclePartMassBox::GetLocalBounds()
{
	// relative to entity
	return m_bounds;
}

//------------------------------------------------------------------------
void CVehiclePartMassBox::Serialize(TSerialize ser, EEntityAspects aspects)
{
	CVehiclePartBase::Serialize(ser, aspects);

	if (ser.GetSerializationTarget() != eST_Network)
	{
		int driving = (int)m_Driving;
		ser.Value("driving", driving);
		SetupDriving((EMassBoxDrivingType)driving);
	}
}

DEFINE_VEHICLEOBJECT(CVehiclePartMassBox);
