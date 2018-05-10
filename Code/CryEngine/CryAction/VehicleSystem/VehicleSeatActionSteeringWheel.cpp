// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a seat action for the steering wheel

   -------------------------------------------------------------------------
   History:
   - 28:11:2005: Created by Mathieu Pinard
   - 14:06:2012: Re-factored to work with CryMannequin by Benito Gangoso Rodriguez

*************************************************************************/
#include "StdAfx.h"
#include "CryAction.h"
#include "IActorSystem.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimatedJoint.h"
#include "VehiclePartSubPartWheel.h"
#include "VehicleSeatActionSteeringWheel.h"

DEFINE_SHARED_PARAMS_TYPE_INFO(CVehicleSeatActionSteeringWheel::SSharedParams);

// Suppressed uninitMemberVar warnings due to use of BEGIN_SHARED_PARAMS macro
// cppcheck-suppress uninitMemberVar
CVehicleSeatActionSteeringWheel::CVehicleSeatActionSteeringWheel()
	: m_isBeingUsed(false)
	, m_isUsedSerialization(false)
	, m_steeringActions(ZERO)
	, m_steeringValues(ZERO)
	, m_wheelInvRotationMax(0.0f)
	, m_currentSteering(0.0f)
	, m_jitterOffset(0.0f)
	, m_jitterSuspensionResponse(0.0f)
{
}

//------------------------------------------------------------------------
bool CVehicleSeatActionSteeringWheel::Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;

	m_userId = 0;

	CVehicleParams steeringWheelTable = table.findChild("SteeringWheel");
	if (!steeringWheelTable)
		return false;

	CryFixedStringT<256> sharedParamsName;
	IEntity* pEntity = pVehicle->GetEntity();
	ISharedParamsManager* pSharedParamsManager = gEnv->pGameFramework->GetISharedParamsManager();
	CRY_ASSERT(pSharedParamsManager);
	sharedParamsName.Format("%s::%s::CVehicleSeatActionSteeringWheel", pEntity->GetClass()->GetName(), pVehicle->GetModification());
	m_pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Get(sharedParamsName));

	if (!m_pSharedParams)
	{
		SSharedParams params;

		params.steeringForce = 1.0f;
		params.steeringRelaxMult = 1.0f;

		params.jitterFreqLow = 0.f;
		params.jitterFreqHi = 0.f;
		params.jitterAmpLow = 0.f;
		params.jitterAmpHi = 0.f;
		params.jitterSuspAmp = 0.f;
		params.jitterSuspResponse = 0.f;

		if (CVehicleParams subTable = steeringWheelTable.findChild("Actions"))
		{
			params.steeringClass = eSC_Generic;

			subTable.getAttr("steeringRelaxMult", params.steeringRelaxMult);
			subTable.getAttr("steeringForce", params.steeringForce);
		}
		else if (CVehicleParams subTableCar = steeringWheelTable.findChild("Car"))
		{
			params.steeringClass = eSC_Car;

			subTableCar.getAttr("jitterFreqLow", params.jitterFreqLow);
			subTableCar.getAttr("jitterFreqHi", params.jitterFreqHi);
			subTableCar.getAttr("jitterAmpLow", params.jitterAmpLow);
			subTableCar.getAttr("jitterAmpHi", params.jitterAmpHi);
			subTableCar.getAttr("jitterSuspAmp", params.jitterSuspAmp);
			subTableCar.getAttr("jitterSuspResponse", params.jitterSuspResponse);
		}
		else
		{
			GameWarning("Vehicle, SeatActionSteeringWheel needs to have either a <Car> or <Actions> node");
			return false;
		}

		m_pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Register(sharedParamsName, params));
	}

	return true;
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::Reset()
{
	if (m_userId != 0)
	{
		StopUsing();
	}
	m_jitter.Setup(0.f, 0.f, 0x152f3);
	m_jitterOffset = 0.f;
	m_jitterSuspensionResponse = 0.f;
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::StartUsing(EntityId passengerId)
{
	m_userId = passengerId;
	m_isBeingUsed = true;

	if (m_pSharedParams->steeringClass == eSC_Generic)
	{
		m_steeringValues.zero();
		m_steeringActions.zero();
	}
	else if (m_pSharedParams->steeringClass == eSC_Car)
	{
		pe_params_car ppc;
		m_wheelInvRotationMax = 0.f;
		IPhysicalEntity* pPhysics = m_pVehicle->GetEntity()->GetPhysics();
		if (pPhysics && pPhysics->GetParams(&ppc) && ppc.maxSteer > DEG2RAD(1.f))
			m_wheelInvRotationMax = 1.f / ppc.maxSteer;
	}

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_PassengerUpdate);
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::StopUsing()
{
	if (m_isBeingUsed)
	{
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
	}

	m_userId = 0;
	m_isBeingUsed = false;
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::OnAction(const TVehicleActionId actionId, int activationMode, float value)
{
	if (m_pSharedParams->steeringClass == eSC_Generic)
	{
		if (actionId == eVAI_TurnLeft)
		{
			m_steeringActions.y = -value;
		}
		else if (actionId == eVAI_TurnRight)
		{
			m_steeringActions.y = value;
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::Update(float frameTime)
{
	IActionController* pActionController = m_pVehicle->GetAnimationComponent().GetActionController();
	const bool doUpdate = (m_userId != 0) && (m_isBeingUsed) && (pActionController != NULL);

	if (doUpdate)
	{
		// NB: the 'steeringTime' param is set on the controller which will pass it onto the active actions
		// As the seat already has an idle action it will pass it to that

		const SSharedParams* params = m_pSharedParams.get();

		if (params->steeringClass == eSC_Car)
		{
			float offset = 0.f;

			if (IVehicleMovement* pMovement = m_pVehicle->GetMovement())
			{
				// The vehicle status, speed, accel etc
				SVehicleMovementStatusGeneral status;
				pMovement->GetStatus(&status);
				SVehicleMovementStatusWheeled wheeledStatus;
				pMovement->GetStatus(&wheeledStatus);

				if (status.speed > 2.f)
				{
					// Smooth the response, when increasing let it increase fast, when decreasing, decrease slowly
					float suspensionResponse = clamp_tpl(wheeledStatus.suspensionCompressionRate * params->jitterSuspResponse, -1.f, +1.f);
					float suspensionChange = (float)__fsel(fabsf(suspensionResponse) - fabsf(m_jitterSuspensionResponse), 30.f, 3.f);
					m_jitterSuspensionResponse += (suspensionResponse - m_jitterSuspensionResponse) * approxOneExp(frameTime * suspensionChange);

					offset = params->jitterSuspAmp * m_jitterSuspensionResponse;

					float invTopSpeed = 1.f / (status.topSpeed + 1.f);
					float speedNorm = min(1.f, status.speed * invTopSpeed);
					float accelNorm = min(1.f, status.localAccel.GetLengthSquared() * sqr(invTopSpeed));

					float jitterAmp = params->jitterAmpLow + accelNorm * (params->jitterAmpHi - params->jitterAmpLow);
					float jitterFreq = params->jitterFreqLow + m_jitterSuspensionResponse * (params->jitterFreqHi - params->jitterFreqLow);

					m_jitter.SetAmpFreq(jitterAmp, jitterFreq);
					offset += m_jitter.Update(frameTime);
				}
			}

			m_jitterOffset += (offset - m_jitterOffset) * approxOneExp(frameTime * 30.f);

			// Get the steering from the physics and add a small amount of filtering
			pe_status_vehicle psv;
			IPhysicalEntity* pPhysics = m_pVehicle->GetEntity()->GetPhysics();
			if (pPhysics && pPhysics->GetStatus(&psv))
				m_currentSteering += (psv.steer * m_wheelInvRotationMax + m_jitterOffset - m_currentSteering) * approxOneExp(frameTime * 30.f);

			float animationTime = clamp_tpl(m_currentSteering * 0.5f + 0.5f, 0.f, 1.f);
			pActionController->SetParam("steeringTime", animationTime);

			//float color[] = {1,1,1,1};
			//IRenderAuxText::Draw2dLabel(100,230,1.5,color,false,"animationTime: %.3f", animationTime);
			//IRenderAuxText::Draw2dLabel(100,250,1.5,color,false,"angle: %.2f", RAD2DEG(psv.steer));
		}
		else if (params->steeringClass == eSC_Generic)
		{
			m_steeringValues -= m_steeringValues * params->steeringRelaxMult * frameTime;
			m_steeringValues += m_steeringActions * params->steeringForce * frameTime;

			m_steeringValues.y = min(1.0f, max(-1.0f, m_steeringValues.y));

			const float animationTime = (m_steeringValues.y + 1.0f) * 0.5f;

			pActionController->SetParam("steeringTime", animationTime);

			//float color[] = {1,1,1,1};
			//IRenderAuxText::Draw2dLabel(100,230,1.5,color,false,"action: %.2f", m_steeringActions.y);
			//IRenderAuxText::Draw2dLabel(100,250,1.5,color,false,"value: %.2f", value);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.Value("steeringValues", m_steeringValues);
		ser.Value("steeringActions", m_steeringActions);

		m_isUsedSerialization = m_isBeingUsed;
		ser.Value("isUsed", m_isUsedSerialization);
	}
}

//------------------------------------------------------------------------
void CVehicleSeatActionSteeringWheel::PostSerialize()
{
	if (m_isUsedSerialization != m_isBeingUsed)
	{
		m_pVehicle->SetObjectUpdate(this, m_isUsedSerialization ? IVehicle::eVOU_PassengerUpdate : IVehicle::eVOU_NoUpdate);
		m_isBeingUsed = m_isUsedSerialization;
	}
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionSteeringWheel);
