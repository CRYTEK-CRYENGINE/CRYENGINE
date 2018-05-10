// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   FlowNodesAIActionSequence.cpp
//  Version:     v1.00
//  Created:     2014-04-29 by Christian Werle.
//  Description: A bunch of FlowNodes that are AI-Sequence-compatible and can only be instantiated from within such one.
//               Having them AI-Sequence-compatible is an "intended restriction" for a higher-level decision system to manage
//               what is currently executing on an AI in a more controllable way which ultimately boils down to being able to
//               distinguish between autonomous behavior vs. scripting via FlowNodes.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "VehicleSystem/VehicleSeat.h"
#include "VehicleSystem/VehicleCVars.h"
#include "VehicleSystem/Vehicle.h"
#include "VehicleSystem/VehicleSeatActionRotateTurret.h"

#include <CryAISystem/IAIActionSequence.h>
#include <CryAISystem/IMovementSystem.h>
#include <CryAISystem/MovementRequest.h>
#include <CryFlowGraph/IFlowBaseNode.h>

//////////////////////////////////////////////////////////////////////////
//
// CryAction_AIActionSequenceFlowNodeBase
//
//////////////////////////////////////////////////////////////////////////
class CryAction_AIActionSequenceFlowNodeBase    // prefixed with "CryAction" to prevent naming clashes in monolithic builds
	: public CFlowBaseNode<eNCT_Instanced>
	  , public AIActionSequence::SequenceActionBase
{
protected:
	CryAction_AIActionSequenceFlowNodeBase();
	void FinishSequenceActionAndActivateOutputPort(int port);
	void CancelSequenceAndActivateOutputPort(int port);

	SActivationInfo m_actInfo;
	bool            m_stopFlowGraphNodeFromGettingUpdatedWhenSequenceActionFinishesOrCancels; // just a convenient way for derived classes to stop updating the FG node when the AISequence-Action gets finished or canceled in many different places

private:
	void SequenceActionFinishedOrCanceled();
};

CryAction_AIActionSequenceFlowNodeBase::CryAction_AIActionSequenceFlowNodeBase()
{
	m_stopFlowGraphNodeFromGettingUpdatedWhenSequenceActionFinishesOrCancels = false;
}

void CryAction_AIActionSequenceFlowNodeBase::FinishSequenceActionAndActivateOutputPort(int port)
{
	gEnv->pAISystem->GetSequenceManager()->ActionCompleted(GetAssignedSequenceId());
	SequenceActionFinishedOrCanceled();
	ActivateOutput(&m_actInfo, port, true);
}

void CryAction_AIActionSequenceFlowNodeBase::CancelSequenceAndActivateOutputPort(int port)
{
	gEnv->pAISystem->GetSequenceManager()->CancelSequence(GetAssignedSequenceId());
	SequenceActionFinishedOrCanceled();
	ActivateOutput(&m_actInfo, port, true);
}

void CryAction_AIActionSequenceFlowNodeBase::SequenceActionFinishedOrCanceled()
{
	if (m_stopFlowGraphNodeFromGettingUpdatedWhenSequenceActionFinishesOrCancels && m_actInfo.pGraph)
	{
		m_actInfo.pGraph->SetRegularlyUpdated(m_actInfo.myID, false);
	}
}

//////////////////////////////////////////////////////////////////////////
//
// CFlowNode_AISequenceAction_ApproachAndEnterVehicle
//
//////////////////////////////////////////////////////////////////////////
class CFlowNode_AISequenceAction_ApproachAndEnterVehicle
	: public CryAction_AIActionSequenceFlowNodeBase
	  , public IVehicleEventListener
{
public:
	CFlowNode_AISequenceAction_ApproachAndEnterVehicle(SActivationInfo* pActInfo);
	~CFlowNode_AISequenceAction_ApproachAndEnterVehicle();

	// IFlowNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo);
	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const;
	// ~IFlowNode

	// AIActionSequence::SequenceActionBase
	virtual void HandleSequenceEvent(AIActionSequence::SequenceEvent sequenceEvent);
	// ~AIActionSequence::SequenceActionBase

	// IVehicleEventListener
	virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params);
	// ~IVehicleEventListener

private:
	enum
	{
		InputPort_Start,
		InputPort_VehicleId,
		InputPort_Seat,
		InputPort_Speed,
		InputPort_Stance,
		InputPort_Fast,
	};

	enum
	{
		OutputPort_Done
	};

	IEntity*      GetEntity();
	IVehicle*     GetVehicle(bool alsoCheckForCrewHostility);
	IVehicleSeat* GetVehicleSeat(IVehicle* pVehicleHint);
	bool          GetAnimationTransitionEnabled() const;
	void          MovementRequestCallback(const MovementRequestResult& result);
	void          TeleportToVehicleSeat();
	void          EnterVehicleSeat(bool animationTransitionEnabled, IVehicleSeat* pVehicleSeatHint);
	void          UnregisterFromVehicleEvent(IVehicleSeat* pVehicleSeatHint);

	EntityId          m_entityId;
	EntityId          m_vehicleId;
	int               m_seatNumber;
	Vec3              m_vehicleSeatEnterPosition;
	MovementRequestID m_movementRequestID;  // used for moving to the vehicle seat that we wanna enter
	bool              m_fast;               // if true: skip approaching and enter-animation
};

CFlowNode_AISequenceAction_ApproachAndEnterVehicle::CFlowNode_AISequenceAction_ApproachAndEnterVehicle(SActivationInfo* pActInfo)
	: m_entityId(0)
	, m_vehicleId(0)
	, m_seatNumber(0)
	, m_vehicleSeatEnterPosition(ZERO)
	, m_fast(false)
{
}

CFlowNode_AISequenceAction_ApproachAndEnterVehicle::~CFlowNode_AISequenceAction_ApproachAndEnterVehicle()
{
}

IFlowNodePtr CFlowNode_AISequenceAction_ApproachAndEnterVehicle::Clone(SActivationInfo* pActInfo)
{
	return new CFlowNode_AISequenceAction_ApproachAndEnterVehicle(pActInfo);
}

void CFlowNode_AISequenceAction_ApproachAndEnterVehicle::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Start"),
		InputPortConfig<EntityId>("VehicleId",_HELP("Vehicle to be entered")),
		InputPortConfig<string>("SeatNumber", _HELP("Seat to be entered"),                       _HELP("Seat"),_UICONFIG("dt=vehicleSeats, ref_entity=VehicleId")),
		InputPortConfig<int>("Speed",         _HELP("Speed to approach the vehicle with"),       NULL,      _UICONFIG("enum_int:Walk=0,Run=1,Sprint=2")),
		InputPortConfig<int>("Stance",        _HELP("Stance while approaching the vehicle"),     NULL,      _UICONFIG("enum_int:Relaxed=0,Alert=1,Combat=2")),
		InputPortConfig<bool>("Fast",         _HELP("Skip approaching and entering animation")),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Done"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.sDescription = _HELP("Allows the AI agent to move to the specified vehicle and to then enter the specified seat");
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceAction_ApproachAndEnterVehicle::ProcessEvent(IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo)
{
	if (!pActInfo->pEntity)
		return;

	switch (event)
	{
	case eFE_Activate:
		{
			m_actInfo = *pActInfo;
			m_stopFlowGraphNodeFromGettingUpdatedWhenSequenceActionFinishesOrCancels = true;
			m_movementRequestID = MovementRequestID::Invalid();

			if (IsPortActive(pActInfo, InputPort_Start))
			{
				if (const AIActionSequence::SequenceId assignedSequenceId = GetAssignedSequenceId())
				{
					gEnv->pAISystem->GetSequenceManager()->RequestActionStart(assignedSequenceId, pActInfo->myID);
					m_actInfo.pGraph->SetRegularlyUpdated(m_actInfo.myID, true);
				}
			}
		}
		break;
	}
}

void CFlowNode_AISequenceAction_ApproachAndEnterVehicle::GetMemoryUsage(ICrySizer* sizer) const
{
	sizer->Add(*this);
}

void CFlowNode_AISequenceAction_ApproachAndEnterVehicle::HandleSequenceEvent(AIActionSequence::SequenceEvent sequenceEvent)
{
	switch (sequenceEvent)
	{
	case AIActionSequence::StartAction:
		{
			if (!m_actInfo.pEntity)
			{
				// the entity has gone for some reason, at least make sure the action gets finished properly and the FG continues
				UnregisterFromVehicleEvent(NULL);
				CancelSequenceAndActivateOutputPort(OutputPort_Done);
				return;
			}

			m_entityId = m_actInfo.pEntity->GetId();
			m_vehicleId = GetPortEntityId(&m_actInfo, InputPort_VehicleId);
			m_fast = GetPortBool(&m_actInfo, InputPort_Fast);

			const bool alsoCheckForCrewHostility = true;
			IVehicle* pVehicle = GetVehicle(alsoCheckForCrewHostility);
			if (!pVehicle)
			{
				CryLog("Actor %s failed to enter vehicle (specified vehicle not found or its crew is hostile towards the actor returned true)", m_actInfo.pEntity->GetName());
				CancelSequenceAndActivateOutputPort(OutputPort_Done);
				return;
			}

			m_seatNumber = pVehicle->GetSeatId(GetPortString(&m_actInfo, InputPort_Seat));
			IVehicleSeat* pSeat = GetVehicleSeat(pVehicle);
			if (!pSeat)
			{
				CryLog("Actor %s failed to enter vehicle (bad seat number provided: %i)", m_actInfo.pEntity->GetName(), m_seatNumber);
				CancelSequenceAndActivateOutputPort(OutputPort_Done);
				return;
			}

			const IVehicleHelper* pEnterHelper = static_cast<CVehicleSeat*>(pSeat)->GetEnterHelper();
			if (!pEnterHelper)
			{
				CryLog("Actor %s failed to enter vehicle (vehicle has no enter-helper)", m_actInfo.pEntity->GetName());
				CancelSequenceAndActivateOutputPort(OutputPort_Done);
				return;
			}

			m_vehicleSeatEnterPosition = pEnterHelper->GetWorldSpaceTranslation();

			assert(gEnv && gEnv->pGameFramework && gEnv->pGameFramework->GetIActorSystem());
			IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_actInfo.pEntity->GetId());

			// if it's the player, have him enter quickly (we assume that the user moved him close enough to the vehicle)
			if (pActor && pActor->IsPlayer())
			{
				EnterVehicleSeat(true, pSeat);
			}
			else if (m_actInfo.pEntity->GetAI())
			{
				if (m_fast)
				{
					TeleportToVehicleSeat();
					EnterVehicleSeat(false, pSeat);
				}
				else
				{
					MovementRequest request;
					request.callback = functor(*this, &CFlowNode_AISequenceAction_ApproachAndEnterVehicle::MovementRequestCallback);
					request.entityID = m_actInfo.pEntity->GetId();
					request.type = MovementRequest::MoveTo;
					request.destination = m_vehicleSeatEnterPosition;
					request.style.SetSpeed((MovementStyle::Speed)GetPortInt(&m_actInfo, InputPort_Speed));
					request.style.SetStance((MovementStyle::Stance)GetPortInt(&m_actInfo, InputPort_Stance));
					m_movementRequestID = gEnv->pAISystem->GetMovementSystem()->QueueRequest(request);
				}
			}
			else if (pActor)
			{
				pActor->HolsterItem(true);
				pActor->MountedGunControllerEnabled(false);
				pActor->GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
				TeleportToVehicleSeat();
				EnterVehicleSeat(GetAnimationTransitionEnabled(), pSeat);
			}
			else
			{
				CRY_ASSERT_MESSAGE(0, "no compatible entity was provided");
				CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, "Actor %s failed to enter vehicle (no compatible entity was provided)", m_actInfo.pEntity->GetName());
				CancelSequenceAndActivateOutputPort(OutputPort_Done);
			}
		}
		break;

	case AIActionSequence::SequenceStopped:
		{
			if (m_movementRequestID)
			{
				gEnv->pAISystem->GetMovementSystem()->CancelRequest(m_movementRequestID);
				m_movementRequestID = MovementRequestID::Invalid();
				UnregisterFromVehicleEvent(NULL);
			}
		}
		break;
	}
}

IEntity* CFlowNode_AISequenceAction_ApproachAndEnterVehicle::GetEntity()
{
	assert(gEnv && gEnv->pEntitySystem);
	return gEnv->pEntitySystem->GetEntity(m_entityId);
}

IVehicle* CFlowNode_AISequenceAction_ApproachAndEnterVehicle::GetVehicle(bool alsoCheckForCrewHostility)
{
	if (!m_vehicleId)
		return NULL;

	assert(gEnv && gEnv->pGameFramework && gEnv->pGameFramework->GetIVehicleSystem());
	IVehicle* pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);
	if (!pVehicle)
		return NULL;

	if (alsoCheckForCrewHostility && pVehicle->IsCrewHostile(m_entityId))
		return NULL;

	return pVehicle;
}

IVehicleSeat* CFlowNode_AISequenceAction_ApproachAndEnterVehicle::GetVehicleSeat(IVehicle* pVehicleHint)
{
	if (!pVehicleHint)
	{
		const bool alsoCheckForCrewHostility = false;
		pVehicleHint = GetVehicle(alsoCheckForCrewHostility);
	}
	return pVehicleHint ? pVehicleHint->GetSeatById(m_seatNumber) : NULL;
}

bool CFlowNode_AISequenceAction_ApproachAndEnterVehicle::GetAnimationTransitionEnabled() const
{
	bool transitionEnabled = !m_fast;

	// check for suppressed animation transition via cvar v_transitionAnimations
	if (VehicleCVars().v_transitionAnimations == 0)
		transitionEnabled = false;

	return transitionEnabled;
}

void CFlowNode_AISequenceAction_ApproachAndEnterVehicle::MovementRequestCallback(const MovementRequestResult& result)
{
	assert(m_movementRequestID == result.requestID);

	switch (result.result)
	{
	case MovementRequestResult::Failure:
		{
			// could not reach the vehicle seat (probably no path found)
			// => just teleport there
			TeleportToVehicleSeat();
		}
	// fall through
	case MovementRequestResult::ReachedDestination:
		{
			m_movementRequestID = MovementRequestID::Invalid();
			EnterVehicleSeat(GetAnimationTransitionEnabled(), NULL);
		}
		break;
	}
}

void CFlowNode_AISequenceAction_ApproachAndEnterVehicle::TeleportToVehicleSeat()
{
	if (IEntity* pEntity = GetEntity())
	{
		Matrix34 transform = pEntity->GetWorldTM();
		transform.SetTranslation(m_vehicleSeatEnterPosition);
		pEntity->SetWorldTM(transform);
	}
}

void CFlowNode_AISequenceAction_ApproachAndEnterVehicle::EnterVehicleSeat(bool animationTransitionEnabled, IVehicleSeat* pVehicleSeatHint)
{
	if (!pVehicleSeatHint)
		pVehicleSeatHint = GetVehicleSeat(NULL);

	if (pVehicleSeatHint)
	{
		pVehicleSeatHint->GetVehicle()->RegisterVehicleEventListener(this, "CFlowNode_AISequenceAction_ApproachAndEnterVehicle");
		pVehicleSeatHint->Enter(m_entityId, animationTransitionEnabled);
	}
	else
	{
		// prematurely finish the action as the seat or entity has magically disappeared
		CancelSequenceAndActivateOutputPort(OutputPort_Done);
	}
}

void CFlowNode_AISequenceAction_ApproachAndEnterVehicle::UnregisterFromVehicleEvent(IVehicleSeat* pVehicleSeatHint)
{
	IVehicle* pVehicle = NULL;

	if (pVehicleSeatHint)
	{
		pVehicle = pVehicleSeatHint->GetVehicle();
	}
	else
	{
		const bool alsoCheckForCrewHostility = false;
		pVehicle = GetVehicle(alsoCheckForCrewHostility);
	}

	if (pVehicle)
	{
		pVehicle->UnregisterVehicleEventListener(this);
	}
}

void CFlowNode_AISequenceAction_ApproachAndEnterVehicle::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	if (event == eVE_PassengerEnter)
	{
		UnregisterFromVehicleEvent(NULL);
		FinishSequenceActionAndActivateOutputPort(OutputPort_Done);
	}
}

//////////////////////////////////////////////////////////////////////////
//
// CFlowNode_AISequenceAction_VehicleRotateTurret
//
//////////////////////////////////////////////////////////////////////////
class CFlowNode_AISequenceAction_VehicleRotateTurret
	: public CryAction_AIActionSequenceFlowNodeBase
{
public:
	CFlowNode_AISequenceAction_VehicleRotateTurret(SActivationInfo* pActInfo);
	~CFlowNode_AISequenceAction_VehicleRotateTurret();

	// IFlowNode
	virtual IFlowNodePtr Clone(SActivationInfo* pActInfo);
	virtual void         GetConfiguration(SFlowNodeConfig& config);
	virtual void         ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo);
	virtual void         GetMemoryUsage(ICrySizer* sizer) const;
	// ~IFlowNode

	// AIActionSequence::SequenceActionBase
	virtual void HandleSequenceEvent(AIActionSequence::SequenceEvent sequenceEvent);
	// ~AIActionSequence::SequenceActionBase

private:
	enum
	{
		InputPort_Start,
		InputPort_AimPos,
		InputPort_ThresholdPitch,
		InputPort_ThresholdYaw
	};

	enum
	{
		OutputPort_Done
	};

	CVehicleSeatActionRotateTurret* m_pActionRotateTurret;
	float                           m_pitchThreshold;
	float                           m_yawThreshold;
};

CFlowNode_AISequenceAction_VehicleRotateTurret::CFlowNode_AISequenceAction_VehicleRotateTurret(SActivationInfo* pActInfo)
	: m_pActionRotateTurret(NULL)
	, m_pitchThreshold(0.0f)
	, m_yawThreshold(0.0f)
{
}

CFlowNode_AISequenceAction_VehicleRotateTurret::~CFlowNode_AISequenceAction_VehicleRotateTurret()
{
}

IFlowNodePtr CFlowNode_AISequenceAction_VehicleRotateTurret::Clone(SActivationInfo* pActInfo)
{
	return new CFlowNode_AISequenceAction_VehicleRotateTurret(pActInfo);
}

void CFlowNode_AISequenceAction_VehicleRotateTurret::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Start"),
		InputPortConfig<Vec3>("AimPos",         _HELP("Position to aim at with the turret")),
		InputPortConfig<float>("ThresholdPitch",_HELP("Threshold of the pitch angle (in degrees) before triggering the output port. Must be used in conjunction with the ThresholdYaw input port.")),
		InputPortConfig<float>("ThresholdYaw",  _HELP("Threshold of the yaw angle (in degrees) before triggering the output port. Must be used in conjunction with the ThresholdPitch input port.")),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Done"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.sDescription = _HELP("Allows an AI agent to align the vehicle's turret to an aiming position.");
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceAction_VehicleRotateTurret::ProcessEvent(IFlowNode::EFlowEvent event, IFlowNode::SActivationInfo* pActInfo)
{
	if (!pActInfo->pEntity)
		return;

	switch (event)
	{
	case eFE_Activate:
		{
			m_actInfo = *pActInfo;
			m_stopFlowGraphNodeFromGettingUpdatedWhenSequenceActionFinishesOrCancels = true;
			m_pActionRotateTurret = NULL;

			if (IsPortActive(pActInfo, InputPort_Start))
			{
				if (const AIActionSequence::SequenceId assignedSequenceId = GetAssignedSequenceId())
				{
					gEnv->pAISystem->GetSequenceManager()->RequestActionStart(assignedSequenceId, pActInfo->myID);
					m_actInfo.pGraph->SetRegularlyUpdated(m_actInfo.myID, true);
				}
			}
		}
		break;

	case eFE_Update:
		{
			if (m_pActionRotateTurret)
			{
				float pitch, yaw;
				m_pActionRotateTurret->GetRemainingAnglesToAimGoalInDegrees(pitch, yaw);
				if (fabsf(pitch) <= m_pitchThreshold && fabs(yaw) <= m_yawThreshold)
				{
					FinishSequenceActionAndActivateOutputPort(OutputPort_Done);
				}
			}
		}
		break;
	}
}

void CFlowNode_AISequenceAction_VehicleRotateTurret::GetMemoryUsage(ICrySizer* sizer) const
{
	sizer->Add(*this);
}

void CFlowNode_AISequenceAction_VehicleRotateTurret::HandleSequenceEvent(AIActionSequence::SequenceEvent sequenceEvent)
{
	switch (sequenceEvent)
	{
	case AIActionSequence::StartAction:
		{
			if (!m_actInfo.pEntity)
			{
				// the entity has gone for some reason, at least make sure the action gets finished properly and the FG continues
				CancelSequenceAndActivateOutputPort(OutputPort_Done);
				return;
			}

			assert(gEnv && gEnv->pGameFramework && gEnv->pGameFramework->GetIActorSystem());
			IActor* pActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(m_actInfo.pEntity->GetId());

			// ensure the FG entity is an IActor
			if (!pActor)
			{
				CRY_ASSERT_MESSAGE(0, "no compatible entity was provided");
				CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, "Actor %s failed to enter vehicle (no compatible entity was provided)", m_actInfo.pEntity->GetName());
				CancelSequenceAndActivateOutputPort(OutputPort_Done);
				return;
			}

			// get the vehicle the actor is linked to
			IVehicle* pVehicle = pActor->GetLinkedVehicle();
			if (!pVehicle)
			{
				CRY_ASSERT_MESSAGE(0, "agent is not linked to a vehicle");
				CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, "Actor %s is not linked to a vehicle", m_actInfo.pEntity->GetName());
				CancelSequenceAndActivateOutputPort(OutputPort_Done);
				return;
			}

			// get the seat the actor is sitting on
			CVehicleSeat* pSeat = static_cast<CVehicleSeat*>(pVehicle->GetSeatForPassenger(m_actInfo.pEntity->GetId()));
			if (!pSeat)
			{
				CRY_ASSERT_MESSAGE(0, "agent is not sitting in the vehicle it is linked to");
				CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, "Actor %s is not sitting in the vehicle it is linked to", m_actInfo.pEntity->GetName());
				CancelSequenceAndActivateOutputPort(OutputPort_Done);
				return;
			}

			// scan for the seat-action that allows rotating the turret
			TVehicleSeatActionVector& seatActions = pSeat->GetSeatActions();
			for (TVehicleSeatActionVector::iterator it = seatActions.begin(); it != seatActions.end(); ++it)
			{
				IVehicleSeatAction* pSeatAction = it->pSeatAction;
				if ((m_pActionRotateTurret = CAST_VEHICLEOBJECT(CVehicleSeatActionRotateTurret, pSeatAction)))
				{
					break;
				}
			}

			// ensure the vehicle-seat provided the correct action
			if (!m_pActionRotateTurret)
			{
				CRY_ASSERT_MESSAGE(0, "a CVehicleSeatActionRotateTurret is not provided by the vehicle or someone else in the vehicle has reserved that action already.");
				CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, "Actor %s could not find a CVehicleSeatActionRotateTurret or that action is already reserved by someone else", m_actInfo.pEntity->GetName());
				CancelSequenceAndActivateOutputPort(OutputPort_Done);
				return;
			}

			m_pitchThreshold = GetPortFloat(&m_actInfo, InputPort_ThresholdPitch);
			m_yawThreshold = GetPortFloat(&m_actInfo, InputPort_ThresholdYaw);

			Vec3 aimPos = GetPortVec3(&m_actInfo, InputPort_AimPos);
			m_pActionRotateTurret->SetAimGoal(aimPos);
		}
		break;

	case AIActionSequence::SequenceStopped:
		{
			if (m_pActionRotateTurret)
			{
				// cancel the rotation action
				m_pActionRotateTurret->SetAimGoal(Vec3Constants<float>::fVec3_Zero);
				m_pActionRotateTurret = NULL;
			}
		}
		break;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

REGISTER_FLOW_NODE("AISequence:ApproachAndEnterVehicle", CFlowNode_AISequenceAction_ApproachAndEnterVehicle)
REGISTER_FLOW_NODE("AISequence:VehicleRotateTurret", CFlowNode_AISequenceAction_VehicleRotateTurret)
