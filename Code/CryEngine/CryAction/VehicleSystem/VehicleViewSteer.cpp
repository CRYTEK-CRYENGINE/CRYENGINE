// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a third person view for vehicles

*************************************************************************/
#include "StdAfx.h"

#include "IViewSystem.h"
#include "IVehicleSystem.h"
#include "VehicleSeat.h"
#include "IWeapon.h"
#include "VehicleViewSteer.h"
#include "IActorSystem.h"

#include <CryMath/Cry_GeoIntersect.h>

const char* CVehicleViewSteer::m_name = "SteerThirdPerson";

static float g_SteerCameraRadius = 0.2f;

//------------------------------------------------------------------------
CVehicleViewSteer::CVehicleViewSteer()
{
	m_pAimPart = NULL;
	m_position.zero();
	m_angReturnSpeed = 0.0f;
	m_angReturnSpeed1 = 0.0f;
	m_angReturnSpeed2 = 0.0f;
	m_angSpeedCorrection = 0.0f;
	m_angSpeedCorrection0 = 0.0f;
	m_lookAt0 = Vec3(0.0f, 0.0f, 1.5f);
	m_localSpaceCameraOffset = Vec3(0.0f, -3.0f, 1.0f);
	m_maxRotation.Set(10.0f, 0.0f, 90.0f);
	m_maxRotation2.zero();
	m_stickSensitivity.Set(0.5f, 0.5f, 0.5f);
	m_stickSensitivity2.Set(0.5f, 0.5f, 0.5f);
	m_inheritedElev = 0.2f;
	m_pivotOffset = 0.f;
	m_flags = eVCam_goingForwards | eVCam_canRotate;
	m_forwardFlags = eVCam_rotationSpring | eVCam_rotationClamp;
	m_backwardsFlags = m_forwardFlags;
	m_radiusRelaxRate = 10.f;
	m_radiusDampRate = 1.f;
	m_radiusVel = 0.0f;
	m_radiusVelInfluence = 0.1f;
	m_radiusMin = 0.9f;
	m_radiusMax = 1.1f;
	m_radius = 1.0f;
	m_curYaw = 0.0f;
}

//------------------------------------------------------------------------
CVehicleViewSteer::~CVehicleViewSteer()
{
}

static void getFlag(CVehicleParams& params, const char* key, int& flags, int flag)
{
	int tmp = flags & flag;
	params.getAttr(key, tmp);
	if (tmp)
		flags |= flag;
	else
		flags &= ~flag;
}

//------------------------------------------------------------------------
bool CVehicleViewSteer::Init(IVehicleSeat* pSeat, const CVehicleParams& table)
{
	if (!CVehicleViewBase::Init(pSeat, table))
		return false;

	m_pAimPart = pSeat->GetAimPart();

	CVehicleParams params = table.findChild(m_name);
	if (!params)
		return false;

	CVehicleParams posParams = params.findChild("Pos");
	CVehicleParams rotationParams = params.findChild("Rotation");
	CVehicleParams motionParams = params.findChild("Motion");
	CVehicleParams backwardsParams = params.findChild("Backwards");
	CVehicleParams radiusParams = params.findChild("Radius");

	if (posParams)
	{
		posParams.getAttr("offset", m_localSpaceCameraOffset);
		posParams.getAttr("aim", m_lookAt0);
		posParams.getAttr("pivotOffset", m_pivotOffset);
	}

	if (rotationParams)
	{
		rotationParams.getAttr("rotationMax", m_maxRotation);
		if (rotationParams.haveAttr("rotationMax2"))
		{
			rotationParams.getAttr("rotationMax2", m_maxRotation2);
		}
		else
		{
			m_maxRotation2 = m_maxRotation / 3.0f;
		}
		m_maxRotation = DEG2RAD(m_maxRotation);
		m_maxRotation2 = DEG2RAD(m_maxRotation2);
		rotationParams.getAttr("stickSensitivity", m_stickSensitivity);
		rotationParams.getAttr("stickSensitivity2", m_stickSensitivity2);
		rotationParams.getAttr("inheritedElev", m_inheritedElev);
		getFlag(rotationParams, "canRotate", m_flags, eVCam_canRotate);
		m_inheritedElev = clamp_tpl(m_inheritedElev, 0.f, 1.f);
	}

	if (motionParams)
	{
		motionParams.getAttr("returnSpeed", m_angReturnSpeed1);
		motionParams.getAttr("returnSpeed2", m_angReturnSpeed2);
		motionParams.getAttr("angFollow", m_angSpeedCorrection0);
	}

	if (backwardsParams)
	{
		getFlag(backwardsParams, "clamp", m_backwardsFlags, eVCam_rotationClamp);
		getFlag(backwardsParams, "returnSpring", m_backwardsFlags, eVCam_rotationSpring);
	}

	if (radiusParams)
	{
		radiusParams.getAttr("min", m_radiusMin);
		radiusParams.getAttr("max", m_radiusMax);
		radiusParams.getAttr("relaxRate", m_radiusRelaxRate);
		radiusParams.getAttr("dampRate", m_radiusDampRate);
		radiusParams.getAttr("velInfluence", m_radiusVelInfluence);
	}

	Reset();
	return true;
}

//------------------------------------------------------------------------
bool CVehicleViewSteer::Init(CVehicleSeat* pSeat)
{
	if (!CVehicleViewBase::Init(pSeat))
		return false;

	Reset();
	return true;
}

//------------------------------------------------------------------------
void CVehicleViewSteer::Reset()
{
	CVehicleViewBase::Reset();

	IEntity* pEntity = m_pVehicle->GetEntity();
	assert(pEntity);
	m_position = pEntity->GetWorldTM() * m_localSpaceCameraOffset;
	Vec3 entityPos = pEntity->GetPos();
	CalcLookAt(pEntity->GetWorldTM());
	m_lastOffset = m_position - m_lookAt;
	m_lastOffsetBeforeElev = m_lastOffset;
	m_angReturnSpeed = 0.f;
	m_angSpeedCorrection = 0.f;
	// Going Forwards
	m_flags &= ~m_backwardsFlags;
	m_flags |= eVCam_goingForwards | m_forwardFlags;
	m_radius = fabsf(m_localSpaceCameraOffset.y);
	m_curYaw = 0.f;
	m_lastVehVel.zero();
	m_radiusVel = 0.f;
}

void CVehicleViewSteer::CalcLookAt(const Matrix34& entityTM)
{
	Vec3 tmp(m_lookAt0.x, m_lookAt0.y, m_lookAt0.z + m_pivotOffset);
	m_lookAt = entityTM * tmp;
	m_lookAt.z -= m_pivotOffset;
}

//------------------------------------------------------------------------
void CVehicleViewSteer::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	CVehicleViewBase::OnAction(actionId, activationMode, value);

	if (!(m_flags & eVCam_canRotate))
	{
		m_rotatingAction.zero();
	}
}

//------------------------------------------------------------------------
void CVehicleViewSteer::OnStartUsing(EntityId passengerId)
{
	CVehicleViewBase::OnStartUsing(passengerId);
}

//------------------------------------------------------------------------
void CVehicleViewSteer::OnStopUsing()
{
	CVehicleViewBase::OnStopUsing();
}

//------------------------------------------------------------------------
void CVehicleViewSteer::Update(float dt)
{
	IEntity* pEntity = m_pVehicle->GetEntity();
	assert(pEntity);

	IVehicleMovement* pVehicleMovement = m_pVehicle->GetMovement();
	if (pVehicleMovement == NULL)
		return;

	IPhysicalEntity* pPhysEntity = pEntity->GetPhysics();
	if (!pPhysEntity)
		return;

	pe_status_dynamics dynStatus;
	pPhysEntity->GetStatus(&dynStatus);

	SMovementState movementState;
	pVehicleMovement->GetMovementState(movementState);
	const float pedal = pVehicleMovement->GetEnginePedal();
	const float maxSpeed = movementState.maxSpeed;
	const Matrix34& pose = m_pAimPart ? m_pAimPart->GetWorldTM() : pEntity->GetWorldTM();
	const Vec3 entityPos = pose.GetColumn3();
	const Vec3 xAxis = pose.GetColumn0();
	const Vec3 yAxis = pose.GetColumn1();
	const Vec3 zAxis = pose.GetColumn2();
	const float forwardSpeed = dynStatus.v.dot(yAxis);
	const float speedNorm = clamp_tpl(forwardSpeed / maxSpeed, 0.0f, 1.0f);
	const Vec3 maxRotation = m_maxRotation + speedNorm * (m_maxRotation2 - m_maxRotation);

	CalcLookAt(pose);
	if (m_lookAt.IsValid())
	{
		if (!m_lastOffset.IsValid())
		{
			m_position = pose * m_localSpaceCameraOffset;
			m_lastOffset = m_position - m_lookAt;
			m_lastOffsetBeforeElev = m_lastOffset;
		}

		Vec3 offset = m_lastOffsetBeforeElev;

		if (pedal < 0.1f && forwardSpeed < 1.0f)
		{
			// Going Backwards
			m_flags &= ~(eVCam_goingForwards | m_forwardFlags);
			m_flags |= m_backwardsFlags;
		}

		if (offset.dot(yAxis) < 0.8f && forwardSpeed > 1.f)
		{
			// Going Forwards
			m_flags &= ~m_backwardsFlags;
			m_flags |= eVCam_goingForwards | m_forwardFlags;
		}

		float sensitivity = (1.f - speedNorm) * m_stickSensitivity.z + speedNorm * m_stickSensitivity2.z;
		float rotate = -m_rotatingAction.z * sensitivity;
		rotate = rotate * dt;

		if (zAxis.z > 0.1f)
		{
			// Safe to update curYaw
			Vec3 projectedX = xAxis;
			projectedX.z = 0.f;
			Vec3 projectedY = yAxis;
			projectedY.z = 0.f;
			const float newYaw = atan2_tpl(offset.dot(projectedX), -(offset.dot(projectedY)));
			const float maxChange = DEG2RAD(270.f) * dt;
			const float delta = clamp_tpl(newYaw - m_curYaw, -maxChange, +maxChange);
			m_curYaw += delta;
		}

		// Rotation Action
		{
			if (m_flags & eVCam_rotationClamp)
			{
				float newYaw = clamp_tpl(m_curYaw + rotate, -maxRotation.z, +maxRotation.z);
				rotate = newYaw - m_curYaw;
				rotate = clamp_tpl(newYaw - m_curYaw, -fabsf(rotate), +fabsf(rotate));
				m_rotation.z += rotate;
			}
			else
			{
				m_rotation.z = 0.f;
			}

			if (speedNorm > 0.1f)
			{
				float reduce = dt * 1.f;
				m_rotation.z = m_rotation.z - reduce * m_rotation.z / (fabsf(m_rotation.z) + reduce);
			}
		}

		// Ang Spring
		{
			float angSpeedCorrection = dt * dt * m_angSpeedCorrection / (dt * m_angSpeedCorrection + 1.f) * dynStatus.w.z;
			if ((m_flags & eVCam_rotationSpring) == 0)
			{
				m_angReturnSpeed = 0.f;
				angSpeedCorrection = 0.f;
			}

			float difference = m_rotation.z - m_curYaw;
			float relax = difference * (m_angReturnSpeed * dt) / ((m_angReturnSpeed * dt) + 1.f);

			const float delta = +relax + angSpeedCorrection + rotate;
			m_curYaw += delta;

			Matrix33 rot = Matrix33::CreateRotationZ(delta);
			offset = rot * offset;

			// Lerp the spring speed
			float angSpeedTarget = m_angReturnSpeed1 + speedNorm * (m_angReturnSpeed2 - m_angReturnSpeed1);
			m_angReturnSpeed += (angSpeedTarget - m_angReturnSpeed) * (dt / (dt + 0.3f));
			m_angSpeedCorrection += (m_angSpeedCorrection0 - m_angSpeedCorrection) * (dt / (dt + 0.3f));
		}

		if (!offset.IsValid()) offset = m_lastOffset;

		// Velocity influence
		Vec3 displacement = -((2.f - speedNorm) * dt) * dynStatus.v;// - yAxis*(0.0f*speedNorm*(yAxis.dot(dynStatus.v))));

		float dot = offset.dot(displacement);
		if (dot < 0.f)
		{
			displacement = displacement + offset * -0.1f * (offset.dot(displacement) / offset.GetLengthSquared());
		}
		offset = offset + displacement;

		const float radius0 = fabsf(m_localSpaceCameraOffset.y);
		const float minRadius = radius0 * m_radiusMin;
		const float maxRadius = radius0 * m_radiusMax;
		float radiusXY = sqrtf(sqr(offset.x) + sqr(offset.y));

		Vec3 offsetXY = offset;
		offsetXY.z = 0.f;
		Vec3 accelerationV = (dynStatus.v - m_lastVehVel);
		float acceleration = offsetXY.dot(accelerationV) / radiusXY;

		m_lastVehVel = dynStatus.v;
		m_radiusVel -= acceleration;
		m_radius += m_radiusVel * dt - dt * m_radiusVelInfluence * offsetXY.dot(dynStatus.v) / radiusXY;
		m_radiusVel *= expf(-dt * m_radiusDampRate);
		m_radius += (radius0 - m_radius) * (dt * m_radiusRelaxRate) / (dt * m_radiusRelaxRate + 1.f);
		m_radius = clamp_tpl(m_radius, minRadius, maxRadius);
		offset = offset * (m_radius / radiusXY);

		// Vertical motion
		float targetOffsetHeight = m_localSpaceCameraOffset.z * (m_radius / radius0);
		float oldOffsetHeight = offset.z;
		offset.z += (targetOffsetHeight - offset.z) * (dt / (dt + 0.3f));
		Limit(offset.z, targetOffsetHeight - 2.f, targetOffsetHeight + 2.f);
		float verticalChange = offset.z - oldOffsetHeight;

		m_lastOffsetBeforeElev = offset;

		// Add up and down camera tilt
		{
			offset.z -= verticalChange;
			m_rotation.x += dt * m_stickSensitivity.x * m_rotatingAction.x;
			m_rotation.x = clamp_tpl(m_rotation.x, -maxRotation.x, +maxRotation.x);

			float elevAngleVehicle = m_inheritedElev * yAxis.z;     // yAxis.z == approx elevation angle

			float elevationAngle = m_rotation.x - elevAngleVehicle;

			float sinElev, cosElev;
			sincos_tpl(elevationAngle, &sinElev, &cosElev);
			float horizLen = sqrtf(offset.GetLengthSquared2D());
			float horizLenNew = horizLen * cosElev - sinElev * offset.z;
			if (horizLen > 1e-4f)
			{
				horizLenNew /= horizLen;
				offset.x *= horizLenNew;
				offset.y *= horizLenNew;
				offset.z = offset.z * cosElev + sinElev * horizLen;
			}
			offset.z += verticalChange;
		}

		if (!offset.IsValid()) offset = m_lastOffset;

		m_position = m_lookAt + offset;

		// Perform world intersection test.
		{
			// Initialise sphere and direction.
			primitives::sphere sphere;

			sphere.center = m_lookAt;
			sphere.r = g_SteerCameraRadius;

			Vec3 direction = m_position - m_lookAt;

			// Calculate camera bounds.
			AABB localBounds;

			//Get the bounding box of the model which is in the first place of the render nodes
			m_pVehicle->GetEntity()->GetRenderNode()->GetLocalBounds(localBounds);

			const float cameraBoundsScale = 0.75f;

			localBounds.min *= cameraBoundsScale;
			localBounds.max *= cameraBoundsScale;

			OBB cameraBounds;

			Matrix34 worldTM = m_pVehicle->GetEntity()->GetWorldTM();

			cameraBounds.SetOBBfromAABB(Matrix33(worldTM), localBounds);

			// Try to find point on edge of camera bounds to begin swept sphere intersection test.
			Vec3 rayBoxIntersect;

			if (Intersect::Ray_OBB(Ray(m_position, -direction), worldTM.GetTranslation(), cameraBounds, rayBoxIntersect) > 0)
			{
				Vec3 temp = m_position - rayBoxIntersect;

				if (direction.Dot(temp) > 0.0f)
				{
					sphere.center = rayBoxIntersect;
					direction = temp;
				}
			}

			// Perform swept sphere intersection test against world.
			geom_contact* pContact = NULL;

			IPhysicalEntity* pSkipEntities[10];

			float distance = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, direction, ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid,
				&pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable, 0, 0, 0,
				pSkipEntities, m_pVehicle->GetSkipEntities(pSkipEntities, 10));

			if (distance > 0.0f)
			{
				// Sweep intersects world so calculate new offset.
				offset = (sphere.center + (direction.GetNormalizedSafe() * distance)) - m_lookAt;
			}
		}

		Interpolate(m_lastOffset, offset, 10.f, dt);

		m_position = m_lookAt + m_lastOffset;
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "camera will fail because lookat position is invalid");
	}

	m_rotatingAction.zero();
}

//------------------------------------------------------------------------
void CVehicleViewSteer::UpdateView(SViewParams& viewParams, EntityId playerId)
{
	static bool doUpdate = true;

	if (!doUpdate) return;

	if (m_position.IsValid())
	{
		viewParams.position = m_position;
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "camera position invalid");
	}

	Vec3 dir = (m_lookAt - m_position).GetNormalizedSafe();
	if (dir.IsValid() && dir.GetLengthSquared() > 0.01f)
	{
		viewParams.rotation = Quat::CreateRotationVDir(dir);
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "camera rotation invalid");
	}

	// set view direction on actor
	IActor* pActor = m_pSeat->GetPassengerActor(true);
	if (pActor && pActor->IsClient())
	{
		pActor->SetViewInVehicle(viewParams.rotation);
	}
}

//------------------------------------------------------------------------
void CVehicleViewSteer::Serialize(TSerialize serialize, EEntityAspects aspects)
{
	CVehicleViewBase::Serialize(serialize, aspects);

	if (serialize.GetSerializationTarget() != eST_Network)
	{
		serialize.Value("position", m_position);
	}
}

void CVehicleViewSteer::OffsetPosition(const Vec3& delta)
{
	m_position += delta;
}

DEFINE_VEHICLEOBJECT(CVehicleViewSteer);
