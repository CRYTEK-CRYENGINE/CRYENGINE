// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements common physics movement code for the helicopters

-------------------------------------------------------------------------
History:
	- Created by Stan Fichele

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include "IVehicleSystem.h"
#include <CryGame/GameUtils.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include "Utility/CryWatch.h"
#include "Vehicle/VehiclePhysicsHelicopter.h"
#include "VehicleMovementBase.h"
#include "Vehicle/VehicleUtils.h"

/*
=====================================================================
	CVehiclePhysicsHelicopter
=====================================================================
*/

CVehiclePhysicsHelicopter::CVehiclePhysicsHelicopter()
{
}

CVehiclePhysicsHelicopter::~CVehiclePhysicsHelicopter()
{
}

void CVehiclePhysicsHelicopter::Reset(IVehicle* pVehicle, IEntity* pEntity)
{
	m_chassis.angVel.zero();
	m_chassis.targetZ = pEntity->GetWorldTM().GetColumn2();
	m_chassis.timer = 0.f;
	m_chassis.angVelAdded.zero();
}

bool CVehiclePhysicsHelicopter::Init(IVehicle* pVehicle, const CVehicleParams& handlingParams)
{
	// NB: handlingParams == table.findChild("HandlingArcade")

	if (CVehicleParams speedParams = handlingParams.findChild("Speed"))
	{
		speedParams.getAttr("accelerationStrength", m_handling.accelerationStrength);
		speedParams.getAttr("acceleration", m_handling.acceleration);
		speedParams.getAttr("maxSpeedForward", m_handling.maxSpeedForward);
		speedParams.getAttr("maxSpeedBackward", m_handling.maxSpeedBackward);
		speedParams.getAttr("maxSpeedLeftRight", m_handling.maxSpeedLeftRight);
		speedParams.getAttr("maxSpeedUpDown", m_handling.maxSpeedUpDown);
		speedParams.getAttr("maxYawSpeed", m_handling.maxYawSpeed);
		speedParams.getAttr("damping", m_handling.damping);
		speedParams.getAttr("angDamping", m_handling.angDamping);
	}
	if (CVehicleParams autoPitchParams = handlingParams.findChild("AutoPitch"))
	{
		autoPitchParams.getAttr("autoPitchForward", m_handling.autoPitchForward);
		autoPitchParams.getAttr("autoPitchBack", m_handling.autoPitchBack);
		autoPitchParams.getAttr("autoRoll", m_handling.autoRoll);
		autoPitchParams.getAttr("changeSpeed", m_handling.autoPitchChangeSpeed);
		autoPitchParams.getAttr("alignToForwardVel", m_handling.alignToForwardVel);
		m_handling.autoPitchForward = DEG2RAD(m_handling.autoPitchForward);
		m_handling.autoPitchBack = DEG2RAD(m_handling.autoPitchBack);
		m_handling.autoRoll = DEG2RAD(m_handling.autoRoll);
	}
	if (CVehicleParams liftParams = handlingParams.findChild("LiftAndThrust"))
	{
		liftParams.getAttr("gravityCorrection", m_handling.gravityCorrection);
		liftParams.getAttr("horizontalFraction", m_handling.horizontalFraction);
		m_handling.gravityCorrection = clamp_tpl(m_handling.gravityCorrection, 0.f, 1.f);
		m_handling.horizontalFraction = clamp_tpl(m_handling.horizontalFraction, -1.f, 1.f);
	}
	if (CVehicleParams instabilityParams = handlingParams.findChild("Instability"))
	{
		instabilityParams.getAttr("instability", m_handling.instability);
		instabilityParams.getAttr("angInstability", m_handling.angInstability);
	}
	if (CVehicleParams aiParams = handlingParams.findChild("AI"))
	{
		aiParams.getAttr("yawResponse", m_handling.aiYawResponse);
		m_handling.aiYawResponse = DEG2RAD(m_handling.aiYawResponse);
	}
	return true;
}

void CVehiclePhysicsHelicopter::PostInit()
{
}

////////////////////////////////////////////
// NOTE: This function must be thread-safe.
void CVehiclePhysicsHelicopter::ProcessAI(SVehiclePhysicsHelicopterProcessAIParams& params)
{
	const float dt = params.dt;

	const Vec3 xAxis = params.pPhysStatus->q.GetColumn0();
	const Vec3 yAxis = params.pPhysStatus->q.GetColumn1();
	const Vec3 zAxis = params.pPhysStatus->q.GetColumn2();
	const Vec3 pos = params.pPhysStatus->centerOfMass;
	
	CMovementRequest* aiRequest = params.pAiRequest;
	SHelicopterAction* pInputAction = params.pInputAction;

	// ------- Move direction and speed ---------------------------------
	float desiredSpeed = aiRequest->HasDesiredSpeed() ? aiRequest->GetDesiredSpeed() : 0.0f;
	desiredSpeed  = clamp_tpl(desiredSpeed, -m_handling.maxSpeedBackward, +m_handling.maxSpeedForward);
	float accel = (float)__fsel(desiredSpeed - params.aiCurrentSpeed, 1.0f, 5.0f);
	LinearChange(params.aiCurrentSpeed, desiredSpeed, accel*dt);
	const Vec3 worldPos = pos;
	const Vec3 desiredMoveDir = aiRequest->HasMoveTarget() ? (aiRequest->GetMoveTarget() - worldPos).GetNormalizedSafe() : yAxis;

	// ------- Fake the input to give roll/pitch ------------------------
	float forwardSpeed = params.aiRequiredVel.dot(yAxis);
	float leftRightSpeed = params.aiRequiredVel.dot(xAxis);
	float maxForwardSpeed = (float)__fsel(forwardSpeed, m_handling.maxSpeedForward, m_handling.maxSpeedBackward);
	float invMaxSpeedBackward = (maxForwardSpeed!=0.f) ? 1.f/maxForwardSpeed : 0.f;
	float invMaxSpeedLeftRight = (m_handling.maxSpeedLeftRight!=0.f) ? 1.f/m_handling.maxSpeedLeftRight : 0.f;
	pInputAction->forwardBack = clamp_tpl(forwardSpeed * invMaxSpeedBackward, -1.f, 1.f);
	pInputAction->leftRight = clamp_tpl(leftRightSpeed * invMaxSpeedLeftRight, -1.f, 1.f);

	// ---------------------------- Yaw ----------------------------
	const Vec3 desiredLookDir = aiRequest->HasLookTarget() ? (aiRequest->GetLookTarget() - worldPos).GetNormalizedSafe() : desiredMoveDir;
	const Vec3 desiredLookDir2D = Vec3(desiredLookDir.x, desiredLookDir.y, 0.0f).GetNormalizedSafe();
	if (desiredLookDir2D.GetLengthSquared() > 0.0f)
	{
		Vec3 right = Vec3(xAxis.x, xAxis.y, 0.f);//.GetNormalized(); 
		Vec3 fwd = Vec3(yAxis.x, yAxis.y, 0.f).GetNormalized(); 
		float angle = acos_tpl(desiredLookDir2D.dot(fwd));
		float sinAngle = desiredLookDir2D.dot(right);
		pInputAction->yaw = clamp_tpl(angle*fsgnf(sinAngle)*m_handling.aiYawResponse, -1.f, +1.f)*params.aiYawResponseScalar;
	}

	params.aiRequiredVel = desiredMoveDir * params.aiCurrentSpeed;
}

////////////////////////////////////////////
// NOTE: This function must be thread-safe.
void CVehiclePhysicsHelicopter::ProcessMovement(SVehiclePhysicsHelicopterProcessParams& params)
{
	// NB: This function changes the params that are passed in!

	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	float dt = params.dt;

	if (dt<0.001f)
		return;

	const bool haveDriver = params.haveDriver;
	IPhysicalEntity* pPhysics = params.pPhysics;
	SVehiclePhysicsStatus* pPhysStatus = params.pPhysStatus;
	SHelicopterAction* pInputAction = params.pInputAction;

	// Calculate the basis axes
	const Vec3 xAxis = pPhysStatus->q.GetColumn0();
	const Vec3 yAxis = pPhysStatus->q.GetColumn1();
	const Vec3 zAxis = pPhysStatus->q.GetColumn2();
	const Vec3 pos = pPhysStatus->centerOfMass;

	// Forward, and left-right vectors in the xy plane
	const Vec3 up(0.f, 0.f, 1.f);
	Vec3 forward(yAxis.x, yAxis.y, 0.f); 
	forward.normalize();
	Vec3 right(forward.y, -forward.x, 0.f);

	Vec3 vel = pPhysStatus->v;
	Vec3 angVel = pPhysStatus->w;

	float damping = m_handling.damping * (0.1f + 0.9f*approxOneExp(vel.GetLengthSquared()*(1.f/sqr(5.f))));
	float velDamping = approxExp(damping*dt);
	vel.x *= velDamping;
	vel.y *= velDamping;
	angVel *= approxExp(m_handling.angDamping*dt);
	
	// Get Gravity
	pe_simulation_params paramsSim;
	pPhysics->GetParams(&paramsSim);

	// Bias the roll when rotating yaw
	float leftRightBias = pInputAction->forwardBack*pInputAction->yaw;
	leftRightBias = fsgnf(leftRightBias)*sqrtf(fabsf(leftRightBias));
	float inputLeftRight = clamp_tpl(leftRightBias+pInputAction->leftRight, -2.f, +2.f);
	float inputUpDown = haveDriver ? pInputAction->upDown : -1.f;

	{
		float x = m_chassis.timer*0.25f;
		float s1 = sinf(x);
		float s2 = sinf(x*2.f);
		float s3 = cosf(x*3.f);
		float s4 = sinf(x*4.f);
		float s5 = cosf(x*5.f);

		float angInstability = (s1 + s2 + s3) * m_handling.angInstability;

		float strength = m_handling.accelerationStrength;
		LerpAxisTimeIndependent(vel, up, inputUpDown * m_handling.maxSpeedUpDown, dt, strength);
		LerpAxisTimeIndependent(angVel, zAxis, -pInputAction->yaw * m_handling.maxYawSpeed + angInstability, dt, strength);
	
		// Instability and oscillations
		if (!params.isAI)
		{
			vel.z += m_handling.instability*dt*(s1 + s3);
			vel += right * (m_handling.instability*dt*(s2+s5));
			vel += forward * (m_handling.instability*dt*(s3+s4));
		}

		m_chassis.timer += dt;
		if (m_chassis.timer > (20.f*gf_PI))
			m_chassis.timer -= 20.f*gf_PI;
				
		float forwardSpeed = forward.dot(vel);

		// Calculate the tilting quaternion
		// Crucially calculation is done using the predicted zAxis not the current
		// Which greatly reduces instability and nutation
		if (haveDriver || params.isAI)
		{
			float targetPitchAngle = pInputAction->forwardBack * (pInputAction->forwardBack>=0.f ? m_handling.autoPitchForward : m_handling.autoPitchBack);
			float sinAnglePitch, cosAnglePitch;
			sincos_tpl(targetPitchAngle, &sinAnglePitch, &cosAnglePitch);

			Vec3 targetUpAxis = up * cosAnglePitch + forward * sinAnglePitch;

			Vec3 targetUpAxisFollowPath = up;
			Vec3 pathVel = vel;
			if (params.isAI && false) // disabled for now since it makes the heli stutter when the player uses it
				pathVel = params.aiRequiredVel;
			if (forwardSpeed>+0.5f)
				targetUpAxisFollowPath = (right.cross(pathVel)).GetNormalized();
			if (forwardSpeed<-0.5f)
				targetUpAxisFollowPath = (pathVel.cross(right)).GetNormalized();

			// Blend between an up axis that is based on forward thrust and one that is perpendicular to the forward vel
			targetUpAxis = targetUpAxis + (targetUpAxisFollowPath - targetUpAxis) * m_handling.alignToForwardVel;

			float sinAngleRoll, cosAngleRoll;
			float targetRollAngle = inputLeftRight * m_handling.autoRoll; 
			sincos_tpl(targetRollAngle, &sinAngleRoll, &cosAngleRoll);

			float z = targetUpAxis.z;
			targetUpAxis.z = z * cosAngleRoll;
			targetUpAxis += right * (z * sinAngleRoll);

			// Lerp the target up axis
			float t = approxOneExp(dt*m_handling.autoPitchChangeSpeed);
			m_chassis.targetZ = (m_chassis.targetZ + t * (targetUpAxis-m_chassis.targetZ)).normalize();

			// Lerp towards the target up, using the angVelocity
			Quat quatNew;
			angVel = angVel - m_chassis.angVelAdded;
			Predict(quatNew, pPhysStatus->q, angVel, dt);
			Quat q = Quat::CreateRotationV0V1(quatNew.GetColumn2(), m_chassis.targetZ);
			DampAxisTimeIndependentN(angVel, q.v, 1.f, 10000.f);
			m_chassis.angVelAdded = (approxOneExp(10.f*dt) * 2.f / dt) * q.v;
			angVel = angVel + m_chassis.angVelAdded;
		}
		else
		{
			m_chassis.angVelAdded.zero();
		}

		float forwardThrustRatio = 0.f, lateralThrustRatio = 0.f;
		float forwardThrust = zAxis.dot(forward);
		if (!params.isAI)
		{
			// Use the rotor angle to calculate the forward and left-right acceleration

			if ((pInputAction->forwardBack * forwardThrust)>0.f)
			{
				float targetPitchAngle = forwardThrust>=0.f ? m_handling.autoPitchForward : m_handling.autoPitchBack;
				forwardThrustRatio = clamp_tpl(forwardThrust/sinf(targetPitchAngle), -1.f, +1.f);
				forwardThrustRatio = fsgnf(forwardThrustRatio)*sqrtf(fabsf(forwardThrustRatio));
				float forwardMaxSpeed = forwardThrust>=0.f ? m_handling.maxSpeedForward : m_handling.maxSpeedBackward;
				float forwardSpeedRatio = clamp_tpl(forwardSpeed/forwardMaxSpeed, -1.f, +1.f);
				float forwardAcceleration = (1.f-0.9f*sqr(sqr(forwardSpeedRatio))) * m_handling.acceleration * forwardThrustRatio;
				Vec3 axis = yAxis + m_handling.horizontalFraction * (forward - yAxis);
				vel = vel + axis * (fabsf(pInputAction->forwardBack) * forwardAcceleration * dt);
			}

			float lateralThrust = zAxis.dot(right);
			if ((pInputAction->leftRight * lateralThrust)>0.f)
			{
				float targetRollAngle = m_handling.autoRoll;
				lateralThrustRatio = clamp_tpl(lateralThrust/sinf(targetRollAngle), -1.f, +1.f);
				lateralThrustRatio = fsgnf(lateralThrustRatio)*sqrt(fabsf(lateralThrustRatio));
				float lateralSpeed = right.dot(vel);
				float lateralMaxSpeed = m_handling.maxSpeedLeftRight;
				float lateralSpeedRatio = clamp_tpl(lateralSpeed/lateralMaxSpeed, -1.f, +1.f);
				float lateralAcceleration = (1.f-0.9f*sqr(sqr(lateralSpeedRatio))) * m_handling.acceleration * lateralThrustRatio;
				Vec3 axis = xAxis + m_handling.horizontalFraction * (right - xAxis);
				vel = vel + axis * (fabsf(inputLeftRight) * lateralAcceleration * dt);
			}

			// Combat gravity
			{
				float gravityCompensation = 1.f - sqrtf(max(sqr(forwardThrustRatio) + sqr(lateralThrustRatio) - 0.15f, 0.f));
				float gc = (m_handling.gravityCorrection) + (1.f-m_handling.gravityCorrection)*zAxis.z*gravityCompensation;
				vel.z = vel.z + fabsf(paramsSim.gravity.z)*dt*gc;
			}
		}
		else
		{
			// AI
			vel = params.aiRequiredVel;
			vel.z += fabsf(paramsSim.gravity.z)*dt;
		}
	}

	//=========================
	// Commit the velocity back 
	//=========================
	pPhysStatus->v = vel;
	pPhysStatus->w = angVel;
}


