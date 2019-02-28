// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SequenceFlowNodes.h"
#include "SequenceManager.h"
#include "SequenceAgent.h"
#include "GoalPipe.h"
#include "PipeUser.h"
#include "Movement/MoveOp.h"
#include "GoalOps/TeleportOp.h"
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IMovementSystem.h>
#include <CryGame/IGameFramework.h>

namespace AIActionSequence
{

CFlowNode_AISequenceStart::~CFlowNode_AISequenceStart()
{
	if (!GetAssignedSequenceId())
		return;

	GetAISystem()->GetSequenceManager()->UnregisterSequence(GetAssignedSequenceId());
}

void CFlowNode_AISequenceStart::Serialize(SActivationInfo* pActInfo, TSerialize ser)
{
	if (ser.IsWriting())
	{
		bool sequenceIsActive = false;
		SequenceId assignedSequenceId = GetAssignedSequenceId();
		if (assignedSequenceId)
		{
			sequenceIsActive = GetAISystem()->GetSequenceManager()->IsSequenceActive(assignedSequenceId);
		}
		ser.Value("SequenceActive", sequenceIsActive);
	}
	else
	{
		ser.Value("SequenceActive", m_restartOnPostSerialize);
	}

	ser.Value("WaitingForTheForwardingEntityToBeSetOnAllTheNodes", m_waitingForTheForwardingEntityToBeSetOnAllTheNodes);
}

void CFlowNode_AISequenceStart::PostSerialize(SActivationInfo* pActInfo)
{
	if (m_restartOnPostSerialize)
	{
		m_restartOnPostSerialize = false;
		InitializeSequence(pActInfo);
	}
}

void CFlowNode_AISequenceStart::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Start"),
		InputPortConfig<bool>("Interruptible",          true,  _HELP("The AI sequence will automatically stop when the agent is not in the idle state")),
		InputPortConfig<bool>("ResumeAfterInterruption",true,  _HELP("The AI sequence will automatically resume from the start or the lastest bookmark if the agent returns to the idle state.")),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Link"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.sDescription = _HELP("");
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceStart::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		{
			m_actInfo = *pActInfo;
		}
		break;
	case eFE_Activate:
		{
			if (IsPortActive(pActInfo, InputPort_Start))
			{
				// Since the forwarding entity is only going to be set on the other flownodes
				// later in this frame, it is necessary to wait of the next frame before
				// initializing the sequence.
				m_waitingForTheForwardingEntityToBeSetOnAllTheNodes = true;
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			}
		}
		break;
	case eFE_Update:
		{
			if (m_waitingForTheForwardingEntityToBeSetOnAllTheNodes)
			{
				m_waitingForTheForwardingEntityToBeSetOnAllTheNodes = false;
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

				InitializeSequence(pActInfo);
			}
		}
		break;
	}
}

void CFlowNode_AISequenceStart::HandleSequenceEvent(SequenceEvent sequenceEvent)
{
	if (sequenceEvent == StartSequence)
	{
		ActivateOutput(&m_actInfo, OutputPort_Link, true);
	}
}

void CFlowNode_AISequenceStart::InitializeSequence(SActivationInfo* pActInfo)
{
	if (!pActInfo->pEntity)
	{
		CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_ERROR, "FG node CFlowNode_AISequenceStart: did you forget to assign an entity to this node?");
		return;
	}

	IFlowNodeData* flowNodeData = pActInfo->pGraph->GetNodeData(pActInfo->myID);
	assert(flowNodeData);
	if (!flowNodeData)
		return;

	EntityId associatedEntityId;
	if (EntityId forwardingEntity = flowNodeData->GetCurrentForwardingEntity())
		associatedEntityId = forwardingEntity;
	else
		associatedEntityId = pActInfo->pEntity->GetId();

	if (ISequenceManager* sequenceManager = GetAISystem()->GetSequenceManager())
	{
		SequenceProperties sequenceProperties(GetPortBool(pActInfo, InputPort_Interruptible), GetPortBool(pActInfo, InputPort_ResumeAfterInterruption));
		if (sequenceManager->RegisterSequence(associatedEntityId, pActInfo->myID, sequenceProperties, pActInfo->pGraph))
		{
			sequenceManager->StartSequence(GetAssignedSequenceId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CFlowNode_AISequenceEnd::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("End"),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Done"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.sDescription = _HELP("");
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_END;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceEnd::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Activate:
		{
			if (IsPortActive(pActInfo, InputPort_End))
			{
				const SequenceId assignedSequenceId = GetAssignedSequenceId();
				if (!assignedSequenceId)
				{
					CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_WARNING, "AISequence:End node doesn't have any sequence assigned.");
					return;
				}

				GetAISystem()->GetSequenceManager()->CancelSequence(assignedSequenceId);
				GetAISystem()->GetSequenceManager()->UnregisterSequence(assignedSequenceId);

				ActivateOutput(pActInfo, OutputPort_Done, true);
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////

void CFlowNode_AISequenceBookmark::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Set"),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Link"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.sDescription = _HELP("Sets a bookmark for the sequence to resume from.");
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceBookmark::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		{
			m_actInfo = *pActInfo;
		}
		break;

	case eFE_Activate:
		{
			if (IsPortActive(pActInfo, InputPort_Set))
			{
				if (const SequenceId assignedSequenceId = GetAssignedSequenceId())
				{
					GetAISystem()->GetSequenceManager()->SetBookmark(assignedSequenceId, pActInfo->myID);
					ActivateOutput(pActInfo, OutputPort_Link, true);
				}
			}
		}
		break;
	}
}

void CFlowNode_AISequenceBookmark::HandleSequenceEvent(SequenceEvent sequenceEvent)
{
	switch (sequenceEvent)
	{
	case TriggerBookmark:
		{
			ActivateOutput(&m_actInfo, OutputPort_Link, true);
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////

void CFlowNode_AISequenceActionMove::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Start"),
		InputPortConfig<int>("Speed",                 0,                                                                                            0, 0, _UICONFIG("enum_int:Walk=0,Run=1,Sprint=2")),
		InputPortConfig<int>("Stance",                0,                                                                                            0, 0, _UICONFIG("enum_int:Relaxed=0,Alert=1,Combat=2,Crouch=3")),
		InputPortConfig<EntityId>("DestinationEntity",_HELP("Entity to use as destination. It will override the position and direction inputs.")),
		InputPortConfig<Vec3>("Position"),
		InputPortConfig<Vec3>("Direction"),
		InputPortConfig<float>("EndDistance",         0.0f),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Done"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.sDescription = _HELP("");
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceActionMove::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Update:
	{
		if (m_stopRadiusSqr > 0.0f)
		{
			const EntityId agentEntityId = GetAssignedEntityId();
			
			//TODO: cannot we just take position from the entity? 
			if (CAIActor* pActor = CastToCAIActorSafe(pActInfo->pEntity->GetAI()))
			{
				if (pActor->GetPos().GetSquaredDistance(m_destPosition) <= m_stopRadiusSqr)
				{
					if (m_movementRequestID)
					{
						gEnv->pAISystem->GetMovementSystem()->CancelRequest(m_movementRequestID);
						m_movementRequestID = 0;
					}

					MovementRequest movementRequest;
					movementRequest.entityID = agentEntityId;
					movementRequest.type = MovementRequest::Stop;
					gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);

					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

					GetAISystem()->GetSequenceManager()->ActionCompleted(GetAssignedSequenceId());
					ActivateOutput(&m_actInfo, OutputPort_Done, true);
				}
			}
		}
		break;
	}
	case eFE_Initialize:
	{
		m_actInfo = *pActInfo;
		break;
	}
	case eFE_Activate:
	{
		m_actInfo = *pActInfo;
		m_movementRequestID = 0;
		m_stopRadiusSqr = 0.0f;

		const SequenceId assignedSequenceId = GetAssignedSequenceId();
		//assert(assignedSequenceId);
		if (!assignedSequenceId)
			return;

		if (IsPortActive(pActInfo, InputPort_Start))
		{
			GetAISystem()->GetSequenceManager()->RequestActionStart(assignedSequenceId, m_actInfo.myID);
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		}
		break;
	}
	}
}

void CFlowNode_AISequenceActionMove::HandleSequenceEvent(SequenceEvent sequenceEvent)
{
	switch (sequenceEvent)
	{
	case StartAction:
		{
			GetPositionAndDirectionForDestination(m_destPosition, m_destDirection);

			m_stopRadiusSqr = sqr(GetPortFloat(&m_actInfo, InputPort_EndDistance));

			MovementRequest movementRequest;
			movementRequest.entityID = GetAssignedEntityId();
			movementRequest.destination = m_destPosition;
			movementRequest.callback = functor(*this, &CFlowNode_AISequenceActionMove::MovementRequestCallback);
			movementRequest.style.SetSpeed((MovementStyle::Speed)GetPortInt(&m_actInfo, InputPort_Speed));
			movementRequest.style.SetStance((MovementStyle::Stance)GetPortInt(&m_actInfo, InputPort_Stance));
			movementRequest.dangersFlags = eMNMDangers_None;

			m_movementRequestID = gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);
		}
		break;
	}
}

void CFlowNode_AISequenceActionMove::MovementRequestCallback(const MovementRequestResult& result)
{
	CRY_ASSERT(m_movementRequestID == result.requestID);

	m_movementRequestID = 0;

	if (result.result != MovementRequestResult::ReachedDestination)
	{
		// Teleport to destination
		AIQueueBubbleMessage(
			"AISequenceActionMove",
			GetAssignedEntityId(),
			"Sequence Move Action failed to find a path to the destination. To make sure the sequence can keep running, the character was teleported to the destination.",
			eBNS_Balloon | eBNS_LogWarning);

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(GetAssignedEntityId());
		if (pEntity)
		{
			Matrix34 transform = pEntity->GetWorldTM();
			const float rotationAngleInRadians = atan2f(-m_destDirection.x, m_destDirection.y);
			transform.SetRotationZ(rotationAngleInRadians, m_destPosition);
			pEntity->SetWorldTM(transform);
		}
	}

	GetAISystem()->GetSequenceManager()->ActionCompleted(GetAssignedSequenceId());
	ActivateOutput(&m_actInfo, OutputPort_Done, true);
}

void CFlowNode_AISequenceActionMove::GetPositionAndDirectionForDestination(Vec3& position, Vec3& direction)
{
	EntityId destinationEntityId = GetPortEntityId(&m_actInfo, InputPort_DestinationEntity);
	IEntity* destinationEntity = gEnv->pEntitySystem->GetEntity(destinationEntityId);

	if (destinationEntity)
	{
		position = destinationEntity->GetWorldPos();
		direction = destinationEntity->GetForwardDir();
	}
	else
	{
		position = GetPortVec3(&m_actInfo, InputPort_Position);
		direction = GetPortVec3(&m_actInfo, InputPort_Direction);
		direction.Normalize();
	}
}

//////////////////////////////////////////////////////////////////////////

void CFlowNode_AISequenceActionMoveAlongPath::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Start"),
		InputPortConfig<int>("Speed",      0,                                                       0, 0, _UICONFIG("enum_int:Walk=0,Run=1,Sprint=2")),
		InputPortConfig<int>("Stance",     0,                                                       0, 0, _UICONFIG("enum_int:Relaxed=0,Alert=1,Combat=2,Crouch=3")),
		InputPortConfig<string>("PathName",_HELP("Name of the path the agent needs to follow. ")),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Done"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.sDescription = _HELP("This node is used to force the AI movement along a path created by level designer.");
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceActionMoveAlongPath::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Initialize:
		{
			m_actInfo = *pActInfo;
		}
		break;

	case eFE_Activate:
		{
			m_actInfo = *pActInfo;
			m_movementRequestID = 0;

			if (const SequenceId assignedSequenceId = GetAssignedSequenceId())
			{
				if (IsPortActive(pActInfo, InputPort_Start))
				{
					GetAISystem()->GetSequenceManager()->RequestActionStart(assignedSequenceId, m_actInfo.myID);
				}
			}
		}
		break;
	}
}

void CFlowNode_AISequenceActionMoveAlongPath::HandleSequenceEvent(SequenceEvent sequenceEvent)
{
	switch (sequenceEvent)
	{
	case StartAction:
		{
			if (CAIActor* pActor = CastToCAIActorSafe(m_actInfo.pEntity->GetAI()))
			{
				stack_string pathToFollowName = GetPortString(&m_actInfo, InputPort_PathName);
				pActor->SetPathToFollow(pathToFollowName.c_str());

				MovementStyle movementStyle;
				movementStyle.SetSpeed((MovementStyle::Speed)GetPortInt(&m_actInfo, InputPort_Speed));
				movementStyle.SetStance((MovementStyle::Stance)GetPortInt(&m_actInfo, InputPort_Stance));
				movementStyle.SetMovingAlongDesignedPath(true);

				MovementRequest movementRequest;
				movementRequest.entityID = GetAssignedEntityId();
				movementRequest.callback = functor(*this, &CFlowNode_AISequenceActionMoveAlongPath::MovementRequestCallback);
				movementRequest.style.SetSpeed((MovementStyle::Speed)GetPortInt(&m_actInfo, InputPort_Speed));
				movementRequest.style.SetStance((MovementStyle::Stance)GetPortInt(&m_actInfo, InputPort_Stance));
				movementRequest.style.SetMovingAlongDesignedPath(true);
				movementRequest.dangersFlags = eMNMDangers_None;

				m_movementRequestID = gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);
			}
		}
		break;
	}
}

void CFlowNode_AISequenceActionMoveAlongPath::GetTeleportEndPositionAndDirection(const char* pathName, Vec3& position, Vec3& direction)
{
	SShape pathToFollowShape;
	if (gAIEnv.pNavigation->GetDesignerPath(pathName, pathToFollowShape))
	{
		assert(pathToFollowShape.shape.size() > 2);
		if (pathToFollowShape.shape.size() > 2)
		{
			ListPositions::const_reverse_iterator lastPosition = pathToFollowShape.shape.rbegin();
			position = *lastPosition;
			const Vec3 secondLastPosition(*(++lastPosition));
			direction = position - secondLastPosition;
		}

	}
}

void CFlowNode_AISequenceActionMoveAlongPath::MovementRequestCallback(const MovementRequestResult& result)
{
	CRY_ASSERT(m_movementRequestID == result.requestID);

	m_movementRequestID = 0;

	if (result.result != MovementRequestResult::ReachedDestination)
	{
		// Teleport to destination

		AIQueueBubbleMessage(
			"AISequenceActionMove",
			GetAssignedEntityId(),
			"Sequence MoveAlongPath Action failed to followed the specified path. To make sure the sequence can keep running, the character was teleported to the destination.",
			eBNS_Balloon | eBNS_LogWarning);

		const EntityId agentEntityId = GetAssignedEntityId();

		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(agentEntityId);
		if (pEntity)
		{
			stack_string pathToFollowName = GetPortString(&m_actInfo, InputPort_PathName);

			Vec3 teleportEndPosition(ZERO);
			Vec3 teleportEndDirection(ZERO);
			GetTeleportEndPositionAndDirection(pathToFollowName.c_str(), teleportEndPosition, teleportEndDirection);

			Matrix34 transform = pEntity->GetWorldTM();
			const float rotationAngleInRadians = atan2f(-teleportEndDirection.x, teleportEndDirection.y);
			transform.SetRotationZ(rotationAngleInRadians, teleportEndPosition);
			pEntity->SetWorldTM(transform);
		}
	}

	GetAISystem()->GetSequenceManager()->ActionCompleted(GetAssignedSequenceId());
	ActivateOutput(&m_actInfo, OutputPort_Done, true);
}

//////////////////////////////////////////////////////////////////////////

void CFlowNode_AISequenceActionAnimation::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Start"),
		InputPortConfig_Void("Stop"),
		InputPortConfig<string>("animstateEx_Animation"),
		InputPortConfig<EntityId>("DestinationEntity"),
		InputPortConfig<Vec3>("Position"),
		InputPortConfig<Vec3>("Direction"),
		InputPortConfig<int>("Speed",                    0,        0,                                                                                                 0, _UICONFIG("enum_int:Walk=0,Run=1,Sprint=2")),
		InputPortConfig<int>("Stance",                   0,        0,                                                                                                 0, _UICONFIG("enum_int:Relaxed=0,Alert=1,Combat=2,Crouch=3")),
		InputPortConfig<bool>("OneShot",                 true,     _HELP("True if it's an one shot animation (signal)\nFalse if it's a looping animation (action)")),
		InputPortConfig<float>("StartRadius",            0.1f),
		InputPortConfig<float>("DirectionTolerance",     180.0f),
		InputPortConfig<float>("LoopDuration",           -1.0f,    _HELP("Duration of looping part of animation\nIgnored for OneShot animations\n-1 = forever")),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Done"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.sDescription = _HELP("");
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceActionAnimation::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Update:
	{
		if (m_bTeleportWhenNotMoving)
		{
			if (CAIActor* pActor = CastToCAIActorSafe(pActInfo->pEntity->GetAI()))
			{
				if (!pActor->QueryBodyInfo().isMoving)
				{
					pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

					GetAISystem()->GetSequenceManager()->ActionCompleted(GetAssignedSequenceId());
					ActivateOutput(&m_actInfo, OutputPort_Done, true);
				}
			}
		}
		break;
	}
	case eFE_Initialize:
	{
		m_actInfo = *pActInfo;
		break;
	}
	case eFE_Activate:
	{
		m_actInfo = *pActInfo;

		m_movementRequestID = 0;
		m_bTeleportWhenNotMoving = false;

		const SequenceId assignedSequenceId = GetAssignedSequenceId();
		if (!assignedSequenceId)
			return;

		if (IsPortActive(pActInfo, InputPort_Start))
		{
			GetAISystem()->GetSequenceManager()->RequestActionStart(assignedSequenceId, m_actInfo.myID);
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
		}

		if (m_running && IsPortActive(pActInfo, InputPort_Stop))
		{
			ClearAnimation(false);
			m_running = false;

			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

			GetAISystem()->GetSequenceManager()->ActionCompleted(assignedSequenceId);
			ActivateOutput(&m_actInfo, OutputPort_Done, true);
		}
		break;
	}
	}
}

void CFlowNode_AISequenceActionAnimation::GetPositionAndDirectionForDestination(Vec3& position, Vec3& direction)
{
	EntityId destinationEntityId = GetPortEntityId(&m_actInfo, InputPort_DestinationEntity);
	IEntity* destinationEntity = gEnv->pEntitySystem->GetEntity(destinationEntityId);
	if (destinationEntity)
	{
		position = destinationEntity->GetWorldPos();
		direction = destinationEntity->GetForwardDir();
	}
	else
	{
		position = GetPortVec3(&m_actInfo, InputPort_Position);
		direction = GetPortVec3(&m_actInfo, InputPort_Direction);
		direction.Normalize();
	}
}

void CFlowNode_AISequenceActionAnimation::HandleSequenceEvent(SequenceEvent sequenceEvent)
{
	switch (sequenceEvent)
	{
	case StartAction:
		{
			const EntityId agentEntityId = GetAssignedEntityId();
			
			GetPositionAndDirectionForDestination(m_destPosition, m_destDirection);
			
			MovementRequest movementRequest;
			movementRequest.style.SetSpeed((MovementStyle::Speed)GetPortInt(&m_actInfo, InputPort_Speed));
			movementRequest.style.SetStance((MovementStyle::Stance)GetPortInt(&m_actInfo, InputPort_Stance));

			if (!m_destPosition.IsZero())
			{
				SAIActorTargetRequest req;

				req.approachLocation = m_destPosition;
				req.approachDirection = m_destDirection;
				req.approachDirection.NormalizeSafe(FORWARD_DIRECTION);
				req.animLocation = req.approachLocation;
				req.animDirection = req.approachDirection;
				//if (!pipeUser->IsUsing3DNavigation()) this is a CPipeUser feature, not in IPipeUser
				//{
				//	req.animDirection.z = 0.0f;
				//	req.animDirection.NormalizeSafe(FORWARD_DIRECTION);
				//}
				req.directionTolerance = DEG2RAD(GetPortFloat(&m_actInfo, InputPort_DirectionTolerance));
				req.startArcAngle = 0;
				req.startWidth = GetPortFloat(&m_actInfo, InputPort_StartRadius);
				req.signalAnimation = GetPortBool(&m_actInfo, InputPort_OneShot);
				req.useAssetAlignment = false;
				req.animation = GetPortString(&m_actInfo, InputPort_Animation);

				if (!req.signalAnimation)
				{
					const float loopDuration = GetPortFloat(&m_actInfo, InputPort_LoopDuration);
					if ((loopDuration >= 0) || (loopDuration == -1))
					{
						req.loopDuration = loopDuration;
					}
				}
				movementRequest.style.SetExactPositioningRequest(&req);
			}

			movementRequest.entityID = agentEntityId;
			movementRequest.destination = m_destPosition;
			movementRequest.callback = functor(*this, &CFlowNode_AISequenceActionAnimation::MovementRequestCallback);
			movementRequest.dangersFlags = eMNMDangers_None;

			m_movementRequestID = gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);
			m_running = true;
		}
		break;

	case SequenceStopped:
		{
			ClearAnimation(true);
			m_running = false;
		}
		break;
	}
}

void CFlowNode_AISequenceActionAnimation::MovementRequestCallback(const MovementRequestResult& result)
{
	CRY_ASSERT(m_movementRequestID == result.requestID);

	m_movementRequestID = 0;
	m_running = false;

	if (result.result != MovementRequestResult::ReachedDestination)
	{
		// Teleport to destination
		AIQueueBubbleMessage(
			"AISequenceActionMove",
			GetAssignedEntityId(),
			"Sequence Animation Action failed to find a path to the destination. To make sure the sequence can keep running, the character was teleported to the destination.",
			eBNS_Balloon | eBNS_LogWarning);

		if (CAIActor* pActor = CastToCAIActorSafe(m_actInfo.pEntity->GetAI()))
		{
			if (pActor->QueryBodyInfo().isMoving)
			{
				m_bTeleportWhenNotMoving = true;
				return;
			}
		}
		
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(GetAssignedEntityId());
		if (m_actInfo.pEntity)
		{
			Matrix34 transform = pEntity->GetWorldTM();
			const float rotationAngleInRadians = atan2f(-m_destDirection.x, m_destDirection.y);
			transform.SetRotationZ(rotationAngleInRadians, m_destPosition);
			pEntity->SetWorldTM(transform);
		}
	}

	GetAISystem()->GetSequenceManager()->ActionCompleted(GetAssignedSequenceId());
	ActivateOutput(&m_actInfo, OutputPort_Done, true);
}

void CFlowNode_AISequenceActionAnimation::ClearAnimation(bool bHurry)
{
	IAIObject* aiObject = GetAssignedEntityAIObject();
	assert(aiObject);
	IF_UNLIKELY (!aiObject)
		return;

	IAIActorProxy* aiActorProxy = aiObject->GetProxy();
	assert(aiActorProxy);
	IF_UNLIKELY (!aiActorProxy)
		return;

	if (GetPortBool(&m_actInfo, InputPort_OneShot))
	{
		aiActorProxy->SetAGInput(AIAG_SIGNAL, "none", true); // the only way to 'stop' a signal immediately is to hurry
	}
	else
	{
		aiActorProxy->SetAGInput(AIAG_ACTION, "idle", bHurry);
	}
}

IAIObject* CFlowNode_AISequenceActionAnimation::GetAssignedEntityAIObject()
{
	EntityId agentEntityId = GetAssignedEntityId();
	IEntity* agentEntity = gEnv->pEntitySystem->GetEntity(agentEntityId);

	assert(agentEntity);
	if (agentEntity)
	{
		return agentEntity->GetAI();
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void CFlowNode_AISequenceActionWait::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Start"),
		InputPortConfig<float>("Time",1.0f),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Done"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceActionWait::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Update:
	{
		const CTimeValue timeElapsed = GetAISystem()->GetFrameStartTime() - m_startTime;
		if (timeElapsed.GetMilliSecondsAsInt64() > m_waitTimeMs)
		{
			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);
			
			GetAISystem()->GetSequenceManager()->ActionCompleted(GetAssignedSequenceId());
			ActivateOutput(&m_actInfo, OutputPort_Done, true);
		}
		break;
	}
	case eFE_Initialize:
	{
		m_actInfo = *pActInfo;
		break;
	}
	case eFE_Activate:
	{
		m_waitTimeMs = 0;
		
		if (const SequenceId assignedSequenceId = GetAssignedSequenceId())
		{
			if (IsPortActive(pActInfo, InputPort_Start))
			{
				GetAISystem()->GetSequenceManager()->RequestActionStart(assignedSequenceId, pActInfo->myID);
				pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, true);
			}
		}
		break;
	}
	}
}

void CFlowNode_AISequenceActionWait::HandleSequenceEvent(SequenceEvent sequenceEvent)
{
	switch (sequenceEvent)
	{
	case AIActionSequence::StartAction:
		m_waitTimeMs = int(1000.0f * GetPortFloat(&m_actInfo, InputPort_Time));
		m_startTime = GetAISystem()->GetFrameStartTime();
		break;
	case AIActionSequence::SequenceStopped:
		break;
	default:
		assert(0);
		break;
	}
}

//////////////////////////////////////////////////////////////////////////

void CFlowNode_AISequenceActionShoot::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Start"),
		InputPortConfig<EntityId>("TargetEntity"),
		InputPortConfig<Vec3>("TargetPosition"),
		InputPortConfig<float>("Duration",        3.0f),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Done"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceActionShoot::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{
	case eFE_Update:
	{
		CTimeValue timeElapsed = GetAISystem()->GetFrameStartTime() - m_startTime;
		if (m_fireTimeMS < 0 || m_fireTimeMS < timeElapsed.GetMilliSecondsAsInt64())
		{
			if (CPipeUser* pPipeUser = CastToCPipeUserSafe(m_actInfo.pEntity->GetAI()))
			{
				// stop firing if was timed
				if (m_fireTimeMS > 0)
					pPipeUser->SetFireMode(FIREMODE_OFF);

				pPipeUser->SetFireTarget(NILREF);
			}

			pActInfo->pGraph->SetRegularlyUpdated(pActInfo->myID, false);

			GetAISystem()->GetSequenceManager()->ActionCompleted(GetAssignedSequenceId());
			ActivateOutput(&m_actInfo, OutputPort_Done, true);
		}
		break;
	}
	case eFE_Initialize:
		{
			m_actInfo = *pActInfo;
		}
		break;

	case eFE_Activate:
		{
			m_actInfo = *pActInfo;

			if (const SequenceId assignedSequenceId = GetAssignedSequenceId())
			{
				if (IsPortActive(pActInfo, InputPort_Start))
				{
					GetAISystem()->GetSequenceManager()->RequestActionStart(assignedSequenceId, pActInfo->myID);
				}
			}
		}
		break;
	}
}

void CFlowNode_AISequenceActionShoot::HandleSequenceEvent(SequenceEvent sequenceEvent)
{
	switch (sequenceEvent)
	{
	case StartAction:
		{
			if (m_actInfo.pEntity)
			{
				CPipeUser* pPipeUser = CastToCPipeUserSafe(m_actInfo.pEntity->GetAI());
				if (pPipeUser)
				{
					IAIObject* pRefPoint = pPipeUser->GetRefPoint();
					if (pRefPoint)
					{
						Vec3 position;

						EntityId targetEntityId = GetPortEntityId(&m_actInfo, InputPort_TargetEntity);
						if (IEntity* targetEntity = gEnv->pEntitySystem->GetEntity(targetEntityId))
						{
							if (IAIObject* targetEntityAIObject = targetEntity->GetAI())
								position = targetEntityAIObject->GetPos();
							else
								position = targetEntity->GetWorldPos();
						}
						else
						{
							position = GetPortVec3(&m_actInfo, InputPort_TargetPosition);
						}

						// It's not possible to shoot from the relaxed state, so it is necessary to set the stance to combat.
						if (IAIActor* aiActor = CastToIAIActorSafe(m_actInfo.pEntity->GetAI()))
						{
							if (IAIActorProxy* aiActorProxy = aiActor->GetProxy())
							{
								SAIBodyInfo bodyInfo;
								aiActorProxy->QueryBodyInfo(bodyInfo);
								if (bodyInfo.stance == STANCE_RELAXED)
								{
									aiActor->GetState().bodystate = STANCE_STAND;
								}
							}
						}
						
						pRefPoint->SetPos(position);
						pRefPoint->SetEntityID(targetEntityId);

						pPipeUser->SetFireMode(FIREMODE_FORCED);
						pPipeUser->SetFireTarget(GetWeakRef(static_cast<CAIObject*>(pRefPoint)));

						m_startTime = GetAISystem()->GetFrameStartTime();
						m_fireTimeMS = int(1000.0f * GetPortFloat(&m_actInfo, InputPort_Duration));

						m_actInfo.pGraph->SetRegularlyUpdated(m_actInfo.myID, true);
						return;
					}
				}
			}
			GetAISystem()->GetSequenceManager()->ActionCompleted(GetAssignedSequenceId());
			ActivateOutput(&m_actInfo, OutputPort_Done, true);
			break;
		}
	case SequenceStopped:
		{
			SequenceAgent agent(GetAssignedEntityId());
			if (IPipeUser* pipeUser = agent.GetPipeUser())
			{
				pipeUser->SetFireMode(FIREMODE_OFF);
			}
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CFlowNode_AISequenceHoldFormation::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Start"),
		InputPortConfig<string>("formation_FormationName"),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig<EntityId>("Done", _HELP("It's triggered as soon as the formation is created. It's outputting also the EntityId of the leader.")),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceHoldFormation::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{

	case eFE_Initialize:
		{
			m_actInfo = *pActInfo;
		}
		break;

	case eFE_Activate:
		{
			if (IsPortActive(pActInfo, InputPort_Start))
			{
				EntityId entityID = GetAssignedEntityId();
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityID);
				if (!pEntity)
					return;

				IAIObject* pAIObject = pEntity->GetAI();
				if (!pAIObject)
					return;

				pAIObject->CreateFormation(GetPortString(&m_actInfo, InputPort_FormationName), Vec3Constants<float>::fVec3_Zero);
				ActivateOutput(pActInfo, OutputPort_Done, entityID);
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////

void CFlowNode_AISequenceJoinFormation::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Start"),
		InputPortConfig<EntityId>("LeaderId"),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Done"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceJoinFormation::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	switch (event)
	{

	case eFE_Initialize:
		{
			m_actInfo = *pActInfo;
		}
		break;

	case eFE_Activate:
		{
			bool isSignalSent = false;
			if (pActInfo->pEntity && IsPortActive(pActInfo, InputPort_Start))
			{
				EntityId entityID = pActInfo->pEntity->GetId();
				SequenceAgent nodeAgent(entityID);

				EntityId entityIdToFollow = GetPortEntityId(pActInfo, InputPort_LeaderId);
				SequenceAgent agentToFollow(entityIdToFollow);

				nodeAgent.SendSignal("ACT_JOIN_FORMATION", agentToFollow.GetEntity());
				isSignalSent = true;
			}
			ActivateOutput(pActInfo, OutputPort_Done, isSignalSent);
		}
		break;
	}
}

void CFlowNode_AISequenceJoinFormation::SendSignal(IAIActor* pIAIActor, const char* signalName, IEntity* pSender)
{
	IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
	const int goalPipeId = gEnv->pAISystem->AllocGoalPipeId();
	pData->iValue = goalPipeId;
	pIAIActor->SetSignal(AISIGNAL_ALLOW_DUPLICATES, signalName, pSender, pData);
}

//////////////////////////////////////////////////////////////////////////

void CFlowNode_AISequenceAction_Stance::GetConfiguration(SFlowNodeConfig& config)
{
	static const SInputPortConfig inputPortConfig[] =
	{
		InputPortConfig_Void("Start"),
		InputPortConfig<int>("Stance",0,  0, 0, _UICONFIG("enum_int:Relaxed=3,Alerted=6,Combat=0,Crouch=1")),
		{ 0 }
	};

	static const SOutputPortConfig outputPortConfig[] =
	{
		OutputPortConfig_Void("Done"),
		{ 0 }
	};

	config.pInputPorts = inputPortConfig;
	config.pOutputPorts = outputPortConfig;
	config.nFlags |= EFLN_TARGET_ENTITY | EFLN_AISEQUENCE_ACTION;
	config.SetCategory(EFLN_APPROVED);
}

void CFlowNode_AISequenceAction_Stance::ProcessEvent(EFlowEvent event, SActivationInfo* pActInfo)
{
	if (event == eFE_Activate)
	{
		if (IsPortActive(pActInfo, InputPort_Start))
		{
			m_actInfo = *pActInfo;

			if (const SequenceId assignedSequenceId = GetAssignedSequenceId())
			{
				GetAISystem()->GetSequenceManager()->RequestActionStart(assignedSequenceId, m_actInfo.myID);
			}
		}
	}
}

void CFlowNode_AISequenceAction_Stance::HandleSequenceEvent(SequenceEvent sequenceEvent)
{
	switch (sequenceEvent)
	{
	case StartAction:
		{
			if (m_actInfo.pEntity)
			{
				if (IAIActor* aiActor = CastToIAIActorSafe(m_actInfo.pEntity->GetAI()))
				{
					aiActor->GetState().bodystate = GetPortInt(&m_actInfo, InputPort_Stance);
					GetAISystem()->GetSequenceManager()->ActionCompleted(GetAssignedSequenceId());
					ActivateOutput(&m_actInfo, OutputPort_Done, true);
					return;
				}
			}

			CryWarning(VALIDATOR_MODULE_AI, VALIDATOR_ERROR, "CFlowNode_AISequenceAction_Stance - Failed to set the stance.");
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////

REGISTER_FLOW_NODE("AISequence:Start", CFlowNode_AISequenceStart);
REGISTER_FLOW_NODE("AISequence:End", CFlowNode_AISequenceEnd);
REGISTER_FLOW_NODE("AISequence:Bookmark", CFlowNode_AISequenceBookmark);
REGISTER_FLOW_NODE("AISequence:Move", CFlowNode_AISequenceActionMove);
REGISTER_FLOW_NODE("AISequence:MoveAlongPath", CFlowNode_AISequenceActionMoveAlongPath);
REGISTER_FLOW_NODE("AISequence:Animation", CFlowNode_AISequenceActionAnimation);
REGISTER_FLOW_NODE("AISequence:Wait", CFlowNode_AISequenceActionWait);
REGISTER_FLOW_NODE("AISequence:Shoot", CFlowNode_AISequenceActionShoot);
REGISTER_FLOW_NODE("AISequence:HoldFormation", CFlowNode_AISequenceHoldFormation);
REGISTER_FLOW_NODE("AISequence:JoinFormation", CFlowNode_AISequenceJoinFormation);
REGISTER_FLOW_NODE("AISequence:Stance", CFlowNode_AISequenceAction_Stance);

} // namespace AIActionSequence
