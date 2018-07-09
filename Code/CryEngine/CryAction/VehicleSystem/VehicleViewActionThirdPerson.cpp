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
#include "VehicleViewActionThirdPerson.h"
#include "VehicleCVars.h"
#include "Vehicle.h"

#include <CryMath/Cry_GeoIntersect.h>
#include <CryMath/Cry_GeoDistance.h>

#if ENABLE_VEHICLE_DEBUG
	#define DEBUG_CAMERA(x) x
#else
	#define DEBUG_CAMERA(x)
#endif

const char* CVehicleViewActionThirdPerson::m_name = "ActionThirdPerson";
static float cameraRadius = 0.42f;

//------------------------------------------------------------------------
CVehicleViewActionThirdPerson::CVehicleViewActionThirdPerson()
	: m_heightAboveWater(0.0f)
{
	m_pAimPart = NULL;
	m_zoom = 1.0f;
	m_actionZoom = 0.0f;
	m_zoomTarget = 1.f;
	m_lagSpeed = 1.f;
	m_extraLag.zero();
	m_worldViewPos.zero();
	m_worldCameraPos.zero();
	m_worldCameraAim.zero();
	m_localCameraPos.zero();
	m_localCameraAim.zero();
	m_velocityMult.Set(1.f, 1.f, 1.f);
	m_cameraOffset.zero();
	m_verticalFilter = 0.2f;
	m_verticalFilterOffset = 0.f;
}

//------------------------------------------------------------------------
CVehicleViewActionThirdPerson::~CVehicleViewActionThirdPerson()
{
}

//------------------------------------------------------------------------
bool CVehicleViewActionThirdPerson::Init(IVehicleSeat* pSeat, const CVehicleParams& table)
{
	if (!CVehicleViewBase::Init(pSeat, table))
		return false;

	m_pSeat = static_cast<CVehicleSeat*>(pSeat);
	m_pVehicle = m_pSeat->GetVehicle();
	m_pAimPart = m_pSeat->GetAimPart();

	if (CVehicleParams paramsTable = table.findChild(m_name))
	{
		paramsTable.getAttr("heightAboveWater", m_heightAboveWater);

		float heightOffset = 1.5f; // default offset (suggested by designers)
		paramsTable.getAttr("heightOffset", heightOffset);

		AABB bounds;
		if (m_pAimPart)
		{
			bounds = m_pAimPart->GetLocalBounds();
			if (bounds.IsReset())
			{
				bounds.min.zero();
				bounds.max.zero();
			}
			bounds.SetTransformedAABB(m_pAimPart->GetLocalTM(false).GetInverted(), bounds);
		}
		else
			m_pVehicle->GetEntity()->GetLocalBounds(bounds);

		m_localCameraPos.Set(0.0f, bounds.min.y, bounds.max.z + heightOffset);
		m_localCameraAim.Set(0.0f, bounds.max.y, bounds.max.z * 0.5f);

		Vec3 offset;

		if (paramsTable.getAttr("cameraAimOffset", offset))
			m_localCameraAim += offset;

		if (paramsTable.getAttr("cameraPosOffset", offset))
			m_localCameraPos += offset;

		paramsTable.getAttr("lagSpeed", m_lagSpeed);
		paramsTable.getAttr("velocityMult", m_velocityMult);
		paramsTable.getAttr("verticalFilter", m_verticalFilter);
		paramsTable.getAttr("verticalFilterOffset", m_verticalFilterOffset);
		m_verticalFilter = clamp_tpl(m_verticalFilter, 0.f, 1.f);
	}

	Reset();
	return (true);
}

//------------------------------------------------------------------------
void CVehicleViewActionThirdPerson::Reset()
{
	CVehicleViewBase::Reset();

	m_actionZoom = 0.0f;
	m_zoomTarget = 1.0f;
	m_zoom = 1.0f;

	m_extraLag.zero();
}

//------------------------------------------------------------------------
void CVehicleViewActionThirdPerson::OnStartUsing(EntityId passengerId)
{
	CVehicleViewBase::OnStartUsing(passengerId);

	Vec3 worldPos = m_pVehicle->GetEntity()->GetWorldPos();

	m_worldCameraPos = gEnv->pSystem->GetViewCamera().GetPosition();
	m_worldViewPos = m_worldCameraPos;
	m_worldCameraAim = worldPos;
	m_cameraOffset = m_worldCameraPos - worldPos;

	CryLog("Entity position=%.2f %.2f %.2f, initial camera position=%.2f %.2f %.2f, offset=%.2f %.2f %.2f", worldPos.x, worldPos.y, worldPos.z, m_worldViewPos.x, m_worldViewPos.y, m_worldViewPos.z, m_cameraOffset.x, m_cameraOffset.y, m_cameraOffset.z);

	m_zoomTarget = 1.0f;
	m_zoom = 1.0f;
}

//------------------------------------------------------------------------
void CVehicleViewActionThirdPerson::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	CVehicleViewBase::OnAction(actionId, activationMode, value);

	if (actionId == eVAI_ZoomIn)
		m_actionZoom -= value;
	else if (actionId == eVAI_ZoomOut)
		m_actionZoom += value;
}

//------------------------------------------------------------------------
void CVehicleViewActionThirdPerson::Update(float frameTimeIn)
{
	// Use the physics frame time, but only if non zero!
	const float physFrameTime = static_cast<CVehicle*>(m_pVehicle)->GetPhysicsFrameTime();
	const float frameTime = (physFrameTime > 0.f) ? min(physFrameTime, frameTimeIn) : frameTimeIn;

	CVehicleViewBase::Update(frameTime);

#if ENABLE_VEHICLE_DEBUG
	Vec3 debugAim0, debugAim1, debugAim2, debugAim3;
#endif

	// apply any zoom changes
	m_zoomTarget += m_actionZoom;
	m_zoomTarget = clamp_tpl(m_zoomTarget, 0.5f, 5.0f);
	Interpolate(m_zoom, m_zoomTarget, 2.5f, frameTime);
	m_actionZoom = 0.0f;

	// compute extra camera pos lag
	const SVehicleStatus& status = m_pVehicle->GetStatus();

	Vec3 localVel = m_pVehicle->GetEntity()->GetWorldRotation().GetInverted() * status.vel;

	Interpolate(m_extraLag.x, localVel.x * m_velocityMult.x, m_lagSpeed, frameTime);
	Interpolate(m_extraLag.y, localVel.y * m_velocityMult.y, m_lagSpeed, frameTime);
	Interpolate(m_extraLag.z, localVel.z * m_velocityMult.z, m_lagSpeed, frameTime);

	Matrix34 worldTM;

	if (m_pAimPart)
		worldTM = m_pAimPart->GetWorldTM();
	else
		worldTM = m_pVehicle->GetEntity()->GetWorldTM();

	Matrix34 lagTM;
	lagTM.SetIdentity();
	lagTM.SetTranslation(m_extraLag);

	// update both aim pos and camera pos

	Ang3 worldAngles(worldTM);
	float rot = worldAngles.z + m_rotation.z;

	m_worldCameraPos = (worldTM * lagTM) * m_localCameraPos;

	Vec3 camWorldPos = m_worldCameraPos;

	float distance = Vec3(m_localCameraPos - m_localCameraAim).GetLength();
	distance *= 0.5f;

	Vec3 localCameraAim = m_localCameraAim;
	localCameraAim.z = m_verticalFilterOffset;

	// Straight-forwrard aim position, transformed to world
	Vec3 worldCameraAim1 = worldTM * m_localCameraAim;
	DEBUG_CAMERA(debugAim0 = worldCameraAim1);

	// Aim position, taken vertically from the entity's zero z offset
	Vec3 worldCameraAim2 = worldTM * localCameraAim;
	DEBUG_CAMERA(debugAim1 = worldCameraAim2);
	worldCameraAim2.z += m_localCameraAim.z - m_verticalFilterOffset;
	DEBUG_CAMERA(debugAim2 = worldCameraAim2);

	// Interpolated final world aim pos
	m_worldCameraAim = worldCameraAim1 + (worldCameraAim2 - worldCameraAim1) * m_verticalFilter;
	DEBUG_CAMERA(debugAim3 = worldCameraAim2);

	float cosPitch = cosf(-m_rotation.x);
	camWorldPos.x = cosPitch * distance * m_zoom * cosf(rot - gf_PI * 0.5f) + camWorldPos.x;
	camWorldPos.y = cosPitch * distance * m_zoom * sinf(rot - gf_PI * 0.5f) + camWorldPos.y;
	camWorldPos.z += distance * m_zoom * sinf(-m_rotation.x);

	if (!iszero(m_heightAboveWater))
	{
		float waterLevel = gEnv->p3DEngine->GetWaterLevel(&camWorldPos);
		camWorldPos.z = max(waterLevel + m_heightAboveWater, camWorldPos.z);
	}

	m_worldCameraPos = camWorldPos;
	Vec3 newPos = m_worldCameraPos;

	AABB bounds;
	m_pVehicle->GetEntity()->GetWorldBounds(bounds);

	Vec3 center = worldTM.GetTranslation();
	center.z += 1.0f;

	IPhysicalEntity* pSkipEntities[10];
	int nSkip = 0;
	if (m_pVehicle)
	{
		nSkip = m_pVehicle->GetSkipEntities(pSkipEntities, 10);
	}

	primitives::sphere sphere;
	sphere.center = center;
	sphere.r = cameraRadius;
	Vec3 dir = newPos - center;

	geom_contact* pContact = 0;
	float hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, dir, ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid,
	                                                                 &pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0, pSkipEntities, nSkip);
	if (hitDist > 0.0f)
	{
		newPos = center + hitDist * dir.GetNormalizedSafe();
	}

	// check new pos is outside the vehicle bounds...
	bounds.Expand(Vec3(cameraRadius, cameraRadius, 0.0f));
	if (bounds.IsContainPoint(newPos))
	{
		// nope, still inside.
		// Take the height of the aim pos and sweep a sphere downwards to the requested pos. Places the camera
		//	on top of the vehicle.
		ray_hit hit;
		sphere.center = newPos;
		sphere.center.z = center.z + 3.0f;
		Vec3 newdir = Vec3(0, 0, -5);
		if (gEnv->pPhysicalWorld->CollideEntityWithPrimitive(m_pVehicle->GetEntity()->GetPhysics(), sphere.type, &sphere, newdir, &hit))
		{
			newPos = hit.pt;
			newPos.z += cameraRadius;
		}
	}

	// interpolate the offset, not the camera position (ensures camera moves with vehicle - reduces perceived jitter)
	static float interpSpeed = 5.0f;
	Interpolate(m_cameraOffset, newPos - center, interpSpeed, frameTime);
	m_worldViewPos = center + m_cameraOffset;

#if ENABLE_VEHICLE_DEBUG
	if (m_isDebugView)
	{
		IRenderer* pRenderer = gEnv->pRenderer;
		IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		SAuxGeomRenderFlags flags = pAuxGeom->GetRenderFlags();
		SAuxGeomRenderFlags oldFlags = pAuxGeom->GetRenderFlags();
		flags.SetDepthWriteFlag(e_DepthWriteOff);
		flags.SetDepthTestFlag(e_DepthTestOff);
		pAuxGeom->SetRenderFlags(flags);
		pAuxGeom->DrawSphere(debugAim0, 0.06f, ColorB(255, 255, 255, 255));
		pAuxGeom->DrawSphere(debugAim1, 0.06f, ColorB(0, 255, 0, 255));
		pAuxGeom->DrawLine(debugAim1, ColorB(0, 255, 0, 255), debugAim2, ColorB(0, 255, 0, 255));
		pAuxGeom->DrawSphere(debugAim2, 0.06f, ColorB(0, 255, 0, 255));
		pAuxGeom->DrawSphere(debugAim3, 0.06f, ColorB(0, 0, 255, 255));
	}
#endif
}

//------------------------------------------------------------------------
void CVehicleViewActionThirdPerson::UpdateView(SViewParams& viewParams, EntityId playerId)
{
	Matrix33 cameraTM = Matrix33::CreateRotationVDir((m_worldCameraAim - m_worldViewPos).GetNormalizedSafe());

	viewParams.rotation = Quat(cameraTM);

	// set view direction on actor
	IActor* pActor = CCryAction::GetCryAction()->GetIActorSystem()->GetActor(playerId);
	if (pActor && pActor->IsClient())
	{
		pActor->SetViewInVehicle(viewParams.rotation);
	}

	viewParams.position = m_worldViewPos;
	viewParams.nearplane = cameraRadius + 0.1f;
}

//------------------------------------------------------------------------
void CVehicleViewActionThirdPerson::Serialize(TSerialize serialize, EEntityAspects aspects)
{
	CVehicleViewBase::Serialize(serialize, aspects);

	if (serialize.GetSerializationTarget() != eST_Network)
	{
		serialize.Value("zoom", m_zoom);
	}
}

//------------------------------------------------------------------------
void CVehicleViewActionThirdPerson::OffsetPosition(const Vec3& delta)
{
	m_worldViewPos += delta;
	m_worldCameraPos += delta;
}

DEFINE_VEHICLEOBJECT(CVehicleViewActionThirdPerson);
