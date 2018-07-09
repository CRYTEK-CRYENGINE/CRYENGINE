// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a third person view for vehicles

   -------------------------------------------------------------------------
   History:
   - 02:05:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include "IViewSystem.h"
#include "IVehicleSystem.h"
#include "VehicleSeat.h"
#include "VehicleViewThirdPerson.h"

#include <CryMath/Cry_GeoIntersect.h>

const char* CVehicleViewThirdPerson::m_name = "ThirdPerson";
float CVehicleViewThirdPerson::m_defaultDistance = 0.f;
float CVehicleViewThirdPerson::m_defaultHeight = 0.f;

static float g_CameraRadius = 0.5f;

//------------------------------------------------------------------------
CVehicleViewThirdPerson::CVehicleViewThirdPerson()
	: m_pEntitySystem(nullptr)
	, m_p3DEngine(nullptr)
	, m_pAimPart(nullptr)
	, m_targetEntityId(0)
	, m_heightOffset(0.0f)
	, m_height(0.0f)
	, m_distance(0.0f)
	, m_boundSwitchAngle(0.0f)
	, m_rot(0.0f)
	, m_interpolationSpeed(5.0f) // default speed
	, m_zoom(1.0f)
	, m_zoomMult(0.0f)
	, m_actionZoom(0.0f)
	, m_actionZoomSpeed(1.0f)
	, m_position(ZERO)
	, m_worldPos(ZERO)
	, m_isUpdatingPos(false)
{
}

//------------------------------------------------------------------------
CVehicleViewThirdPerson::~CVehicleViewThirdPerson()
{
}

//------------------------------------------------------------------------
bool CVehicleViewThirdPerson::Init(IVehicleSeat* pISeat, const CVehicleParams& table)
{
	CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(pISeat);
	if (!CVehicleViewBase::Init(pSeat, table))
		return false;

	m_p3DEngine = gEnv->p3DEngine;
	m_pEntitySystem = gEnv->pEntitySystem;

	m_pSeat = pSeat;
	m_pVehicle = pSeat->GetVehicle();
	m_targetEntityId = 0;
	m_pAimPart = pSeat->GetAimPart();

	CVehicleParams params = table.findChild(m_name);
	if (!params)
		return false;

	params.getAttr("distance", m_distance);
	params.getAttr("speed", m_interpolationSpeed);

	if (!params.getAttr("heightOffset", m_heightOffset))
		m_heightOffset = 1.5f; // added default offset (suggested by designers)

	Reset();
	return true;
}

//------------------------------------------------------------------------
bool CVehicleViewThirdPerson::Init(CVehicleSeat* pSeat)
{
	if (!CVehicleViewBase::Init(pSeat))
		return false;

	m_p3DEngine = gEnv->p3DEngine;
	m_pEntitySystem = gEnv->pEntitySystem;

	m_pSeat = pSeat;
	m_pVehicle = pSeat->GetVehicle();
	m_targetEntityId = 0;
	m_pAimPart = NULL;

	m_name = "ThirdPerson";

	AABB vehicleBounds;
	pSeat->GetVehicle()->GetEntity()->GetLocalBounds(vehicleBounds);

	m_distance = (vehicleBounds.max - vehicleBounds.min).GetLength();
	m_interpolationSpeed = 5.0f;
	m_heightOffset = 1.5f;

	Reset();
	return true;
}

//------------------------------------------------------------------------
void CVehicleViewThirdPerson::Reset()
{
	CVehicleViewBase::Reset();

	m_actionZoom = 0.0f;
	m_actionZoomSpeed = 1.0f;

	m_rot = 0.0f;
	m_zoom = 1.0f;
	m_zoomMult = 0.0f;

	m_isUpdatingPos = true;
}

//------------------------------------------------------------------------
void CVehicleViewThirdPerson::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	CVehicleViewBase::OnAction(actionId, activationMode, value);

	if (actionId == eVAI_ZoomIn)
		m_actionZoom -= value;
	else if (actionId == eVAI_ZoomOut)
		m_actionZoom += value;
}

//------------------------------------------------------------------------
void CVehicleViewThirdPerson::OnStartUsing(EntityId passengerId)
{
	CVehicleViewBase::OnStartUsing(passengerId);
	m_position = gEnv->pSystem->GetViewCamera().GetPosition();
	//m_position.z -= (m_defaultHeight != 0) ? m_defaultHeight : m_heightOffset;
	m_zoomMult = 0.0f;
}

//------------------------------------------------------------------------
void CVehicleViewThirdPerson::Update(float frameTime)
{
	CVehicleViewBase::Update(frameTime);

	// apply any zoom changes
	Interpolate(m_zoomMult, m_actionZoom, 2.5f, frameTime);
	if (!iszero(m_zoomMult))
		m_zoom = min(3.5f, max(0.75f, m_zoom + m_zoomMult));
	m_actionZoom = 0.0f;

	Interpolate(m_rot, m_rotation.z, m_interpolationSpeed, frameTime);

	Matrix34 worldTM;
	if (m_pAimPart)
		worldTM = m_pAimPart->GetWorldTM();
	else if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_targetEntityId))
		worldTM = pEntity->GetWorldTM();
	else
		worldTM = m_pVehicle->GetEntity()->GetWorldTM();

	if (m_isUpdatingPos)
		m_worldPos = worldTM.GetTranslation();

	Ang3 worldAngles = Ang3::GetAnglesXYZ(Matrix33(worldTM));
	float rot = worldAngles.z + m_rot;
	float distance = (m_defaultDistance != 0) ? m_defaultDistance : m_distance;

	Vec3 goal;
	goal.x = distance * m_zoom * cosf(rot + gf_PI * 1.5f) + m_worldPos.x;
	goal.y = distance * m_zoom * sinf(rot - gf_PI / 2.0f) + m_worldPos.y;

	AABB vehicleBounds;
	m_pVehicle->GetEntity()->GetLocalBounds(vehicleBounds);
	goal.z = vehicleBounds.max.z;
	goal.z += m_pVehicle->GetEntity()->GetWorldPos().z;

	// intersection check...
	{
		IPhysicalEntity* pSkipEntities[10];
		int nSkip = 0;
		if (m_pVehicle)
		{
			nSkip = m_pVehicle->GetSkipEntities(pSkipEntities, 10);
		}

		primitives::sphere sphere;
		sphere.center = m_worldPos;
		sphere.r = g_CameraRadius;
		Vec3 dir = goal - m_worldPos;

		geom_contact* pContact = 0;
		float hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, dir, ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid,
		                                                                 &pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, nSkip);
		if (hitDist > 0.0f)
		{
			goal = m_worldPos + hitDist * dir.GetNormalizedSafe();
		}
	}

	Interpolate(m_position, goal, 5.0f, frameTime);
}

//------------------------------------------------------------------------
void CVehicleViewThirdPerson::UpdateView(SViewParams& viewParams, EntityId playerId)
{
	viewParams.position = m_position;
	viewParams.position.z += (m_defaultHeight != 0) ? m_defaultHeight : m_heightOffset;

	Matrix33 rotation = Matrix33::CreateRotationVDir((m_worldPos - m_position).GetNormalizedSafe());
	viewParams.rotation = Quat(rotation);

	// set view direction on actor
	IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(playerId);
	if (pActor && pActor->IsClient())
	{
		pActor->SetViewInVehicle(viewParams.rotation);
	}

	viewParams.nearplane = g_CameraRadius + 0.1f;
}

//------------------------------------------------------------------------
void CVehicleViewThirdPerson::Serialize(TSerialize serialize, EEntityAspects aspects)
{
	CVehicleViewBase::Serialize(serialize, aspects);

	if (serialize.GetSerializationTarget() != eST_Network)
	{
		serialize.Value("zoom", m_zoom);
		serialize.Value("position", m_position);
		serialize.Value("worldPosition", m_worldPos);
	}
}

//------------------------------------------------------------------------
void CVehicleViewThirdPerson::SetDefaultDistance(float dist)
{
	m_defaultDistance = dist;
}

//------------------------------------------------------------------------
void CVehicleViewThirdPerson::SetDefaultHeight(float height)
{
	m_defaultHeight = height;
}

//------------------------------------------------------------------------
void CVehicleViewThirdPerson::OffsetPosition(const Vec3& delta)
{
	m_position += delta;
	m_worldPos += delta;
}

DEFINE_VEHICLEOBJECT(CVehicleViewThirdPerson);
