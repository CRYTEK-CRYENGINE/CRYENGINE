// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a wheel vehicle part

   -------------------------------------------------------------------------
   History:
   - 26:08:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"

#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartSubPartWheel.h"
#include "VehicleUtils.h"

#define THREAD_SAFE 1

//------------------------------------------------------------------------
CVehiclePartSubPartWheel::CVehiclePartSubPartWheel()
{
	m_physGeomParams.bCanBrake = 1;
	m_physGeomParams.bCanSteer = 1;
	m_physGeomParams.kStiffnessWeight = 1.f;
	m_physGeomParams.kStiffness = 0.f;
	m_physGeomParams.minFriction = m_physGeomParams.maxFriction = 1.f;
	m_physGeomParams.kLatFriction = 1.f;
	m_physGeomParams.pivot.zero();
}

//------------------------------------------------------------------------
bool CVehiclePartSubPartWheel::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartSubPart::Init(pVehicle, table, parent, initInfo, eVPT_Wheel))
		return false;

	CVehicleParams subTable = table.findChild("SubPartWheel");
	if (!subTable)
		return false;

	bool bTemp = false;

	subTable.getAttr("axle", m_physGeomParams.iAxle);
	subTable.getAttr("density", m_physGeomParams.density);
	subTable.getAttr("damping", m_physGeomParams.kDamping);
	subTable.getAttr("lenInit", m_physGeomParams.lenInitial);
	subTable.getAttr("lenMax", m_physGeomParams.lenMax);
	subTable.getAttr("maxFriction", m_physGeomParams.maxFriction);
	subTable.getAttr("minFriction", m_physGeomParams.minFriction);
	subTable.getAttr("latFriction", m_physGeomParams.kLatFriction);
	subTable.getAttr("stiffness", m_physGeomParams.kStiffness);
	subTable.getAttr("surfaceId", m_physGeomParams.surface_idx);

	// If a stiffness weight is supplied force kStiffness = 0 so suspension is autocalculated
	if (subTable.getAttr("stiffnessWeight"))
	{
		subTable.getAttr("stiffnessWeight", m_physGeomParams.kStiffnessWeight);
		m_physGeomParams.kStiffness = 0.f;
	}

	if (subTable.getAttr("canBrake", bTemp))
		m_physGeomParams.bCanBrake = bTemp;
	if (subTable.getAttr("driving", bTemp))
		m_physGeomParams.bDriving = bTemp;
	if (subTable.getAttr("rayCast", bTemp))
		m_physGeomParams.bRayCast = bTemp;
	if (subTable.getAttr("canSteer", bTemp))
		m_physGeomParams.bCanSteer = bTemp;

	m_physGeomParams.mass = m_pSharedParameters->m_mass;

	// wheel index is saved in the shared params, so just update the vehicle properties here
	reinterpret_cast<CVehicle*>(pVehicle)->m_wheelCount++;

	InitGeometry();

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartSubPartWheel::Reset()
{
	CVehiclePartSubPart::Reset();
}

//------------------------------------------------------------------------
void CVehiclePartSubPartWheel::OnEvent(const SVehiclePartEvent& event)
{
	CVehiclePartSubPart::OnEvent(event);
}

//------------------------------------------------------------------------
void CVehiclePartSubPartWheel::InitGeometry()
{
	if (m_slot == -1)
	{
		m_slot = 0;

		while (GetEntity()->IsSlotValid(m_slot))
			m_slot++;
	}

	ChangeState(eVGS_Default);
}

//------------------------------------------------------------------------
bool CVehiclePartSubPartWheel::ChangeState(EVehiclePartState state, int flags)
{
	EVehiclePartState old = m_state;

	if (state == eVGS_Default)
	{
		if (IPhysicalEntity* pPhysics = m_pVehicle->GetEntity()->GetPhysics())
		{
			pe_params_car params_car;

			if (pPhysics->GetParams(&params_car))
			{
				if (m_pSharedParameters->m_wheelIndex < params_car.nWheels)
				{
					pe_params_wheel wheelParams;

					wheelParams.iWheel = m_pSharedParameters->m_wheelIndex;
					wheelParams.suspLenMax = m_physGeomParams.lenMax;
					wheelParams.suspLenInitial = m_pSharedParameters->m_suspLength;

					pPhysics->SetParams(&wheelParams, THREAD_SAFE);
				}
				else
				{
					CRY_ASSERT_MESSAGE(false, "Wheel index exceeds number of wheels");
				}
			}
		}
	}

	if (state == eVGS_Destroyed)
	{
		if (IPhysicalEntity* pPhysics = m_pVehicle->GetEntity()->GetPhysics())
		{
			pe_params_car params_car;

			if (pPhysics->GetParams(&params_car))
			{
				if (m_pSharedParameters->m_wheelIndex < params_car.nWheels)
				{
					pe_params_wheel wheelParams;

					wheelParams.iWheel = m_pSharedParameters->m_wheelIndex;
					wheelParams.suspLenMax = 0.0f;
					wheelParams.suspLenInitial = 0.0f;

					pPhysics->SetParams(&wheelParams, THREAD_SAFE);
				}
				else
				{
					CRY_ASSERT_MESSAGE(false, "Wheel index exceeds number of wheels");
				}
			}
		}
	}

	if (!CVehiclePartSubPart::ChangeState(state, flags))
		return false;

	if (old == eVGS_Damaged1 && m_state == eVGS_Default)
	{
		// tire repaired
		if (IPhysicalEntity* pPhysics = m_pVehicle->GetEntity()->GetPhysics())
		{
			pe_action_impulse imp;
			imp.impulse = m_pVehicle->GetEntity()->GetWorldRotation() * Vec3(0.f, 0.f, m_pVehicle->GetMass() / m_pVehicle->GetWheelCount());
			imp.point = GetWorldTM().GetTranslation();
			pPhysics->Action(&imp);
		}
	}

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartSubPartWheel::GetGeometryName(EVehiclePartState state, string& name)
{
	if (state == eVGS_Damaged1)
	{
		name = GetName();
		name.append("_rim");
		return;
	}
	else
		CVehiclePartSubPart::GetGeometryName(state, name);
}

//------------------------------------------------------------------------
void CVehiclePartSubPartWheel::Physicalize()
{
	IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
	if (!pPhysics)
		return;

	IStatObj* pStatObj = GetEntity()->GetStatObj(m_slot);
	phys_geometry* pPhysGeom = pStatObj ? pStatObj->GetPhysGeom() : 0;
	bool updated = false;

	if (m_slot != -1 && m_physId != -1)
	{
		pe_status_pos status;
		status.partid = m_physId;
		if (pPhysics->GetStatus(&status))
		{
			if (pStatObj && pPhysGeom)
			{
				Matrix34 tm(m_savedTM);
				pe_params_part params;
				params.partid = m_physId;
				params.pPhysGeom = pPhysGeom;
				params.pMtx3x4 = &tm;
				pPhysics->SetParams(&params);
			}
			else
			{
				pPhysics->RemoveGeometry(m_physId);
				m_physId = -1;
			}

			updated = true;
		}
	}

	if (!updated && pStatObj && pPhysGeom)
	{
		Matrix34 slotTM = VALIDATE_MAT(m_savedTM);

		m_physGeomParams.pMtx3x4 = &slotTM;
		m_physGeomParams.pivot = slotTM.GetTranslation();
		m_physGeomParams.lenInitial = m_pSharedParameters->m_suspLength;
		m_physGeomParams.flags &= ~geom_floats;

		if (m_pVehicle->GetMass() >= VEHICLE_MASS_COLLTYPE_HEAVY)
		{
			m_physGeomParams.flags &= ~geom_colltype_debris;
			m_physGeomParams.flagsCollider = geom_colltype_vehicle;
		}

		m_physGeomParams.flagsCollider |= geom_colltype6; // vehicle-only colltype

		// use copy, physics eventually writes to the passed struct
		pe_cargeomparams params;
		memcpy(&params, &m_physGeomParams, sizeof(params));
		m_physId = pPhysics->AddGeometry(pPhysGeom, &params, m_slot);

		if (m_physId < 0)
			GameWarning("AddGeometry failed on <%s> (part %s)", m_pVehicle->GetEntity()->GetName(), GetName());
	}

	if (m_physId != -1)
	{
		pe_params_wheel wp;
		wp.iWheel = m_pSharedParameters->m_wheelIndex;
		wp.Tscale = m_pSharedParameters->m_torqueScale;
		pPhysics->SetParams(&wp);
	}

#if ENABLE_VEHICLE_DEBUG
	if (IsDebugParts())
		CryLog("[CVehicleSubPartWheel]: <%s> got physId %i", GetName(), m_physId);
#endif
}

//------------------------------------------------------------------------
void CVehiclePartSubPartWheel::Serialize(TSerialize ser, EEntityAspects aspects)
{
	CVehiclePartBase::Serialize(ser, aspects);

	if (ser.GetSerializationTarget() != eST_Network)
	{
		IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
		if (!pPhysics)
			return;

		float maxFrictionRatio = 0.0f, minFrictionRatio = 0.0f;

		if (ser.IsWriting())
		{
			pe_params_wheel wheelParams;
			wheelParams.iWheel = m_pSharedParameters->m_wheelIndex;
			if (pPhysics->GetParams(&wheelParams))
			{
				if (m_physGeomParams.maxFriction != 0.f)
					maxFrictionRatio = wheelParams.maxFriction / m_physGeomParams.maxFriction;
				if (m_physGeomParams.minFriction != 0.f)
					minFrictionRatio = wheelParams.minFriction / m_physGeomParams.minFriction;

				ser.Value("maxFrictionRatio", maxFrictionRatio, 'vWFr');
				ser.Value("minFrictionRatio", minFrictionRatio, 'vWFr');
			}
		}
		else if (ser.IsReading())
		{
			ser.Value("maxFrictionRatio", maxFrictionRatio, 'vWFr');
			ser.Value("minFrictionRatio", minFrictionRatio, 'vWFr');

			pe_params_wheel wheelParams;
			wheelParams.iWheel = m_pSharedParameters->m_wheelIndex;
			wheelParams.maxFriction = m_physGeomParams.maxFriction * maxFrictionRatio;
			wheelParams.minFriction = m_physGeomParams.minFriction * minFrictionRatio;
			pPhysics->SetParams(&wheelParams);
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartSubPartWheel::PostSerialize()
{
	CVehiclePartBase::PostSerialize();
}

//------------------------------------------------------------------------
float CVehiclePartSubPartWheel::GetSlipFrictionMod(float slip) const
{
	if (GetState() != eVGS_Default) // 0 for damaged wheels
		return 0.f;

	float mod = IsFront() ? VehicleCVars().v_slipFrictionModFront : VehicleCVars().v_slipFrictionModRear;
	if (mod <= 0.f)
		mod = m_pSharedParameters->m_slipFrictionMod;

	float slope = IsFront() ? VehicleCVars().v_slipSlopeFront : VehicleCVars().v_slipSlopeRear;
	if (slope <= 0.f)
		slope = m_pSharedParameters->m_slipSlope;

	return (slip * mod) / (slip + mod * slope);
}

DEFINE_VEHICLEOBJECT(CVehiclePartSubPartWheel);
