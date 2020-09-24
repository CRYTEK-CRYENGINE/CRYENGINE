// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BehaviorTreeNodes_AI.h"

#include "AIActor.h" // Big one, but needed for timestamp collection
#include "Puppet.h"  // Big one, but needed for the posture manager
#include "AIBubblesSystem/AIBubblesSystem.h"
#include "AIGroup.h"
#include "Group/GroupManager.h"
#include <CryAISystem/BehaviorTree/Action.h>
#include <CryAISystem/BehaviorTree/Decorator.h>
#include <CryAISystem/BehaviorTree/IBehaviorTree.h>
#include <CrySystem/Timer.h>
#include "Cover/CoverSystem.h"
#include "GoalPipeXMLReader.h"
#include "GoalOps/ShootOp.h"
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IMovementSystem.h>
#include <CryAISystem/MovementRequest.h>
#include <CryAISystem/MovementStyle.h>
#include <CrySerialization/SerializationUtils.h>
#include "PipeUser.h"                                // Big one, but needed for GetProxy and SetBehavior
#include "TacticalPointSystem/TacticalPointSystem.h" // Big one, but needed for InitQueryContextFromActor
#include "TargetSelection/TargetTrackManager.h"
#include <CryString/CryName.h>
#include <CryGame/IGameFramework.h>
#include "BehaviorTreeManager.h"

#include <../CryAction/ICryMannequin.h>

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	#include <CrySerialization/Enum.h>
	#include <CrySerialization/ClassFactory.h>
#endif

namespace
{
CPipeUser* GetPipeUser(const BehaviorTree::Node& node, const BehaviorTree::UpdateContext& context)
{
	IAIObject * aiObject = context.entity.GetAI();

	if (!aiObject)
	{
		const string errorMessage = "Node requires an AI Object to work properly because it uses PipeUser. Behavior Tree may not behave as expected.";
		CRY_ASSERT(aiObject, errorMessage.c_str());
		BehaviorTree::ErrorReporter(node, context).LogError(errorMessage.c_str());
		return nullptr;
	}

	return aiObject->CastToCPipeUser();
}

CPuppet* GetPuppet(const BehaviorTree::Node& node, const BehaviorTree::UpdateContext& context)
{
	IAIObject * aiObject = context.entity.GetAI();

	if (!aiObject)
	{
		const string errorMessage = "Node requires an AI Object to work properly because it uses Puppet. Behavior Tree may not behave as expected.";
		CRY_ASSERT(aiObject, errorMessage.c_str());
		BehaviorTree::ErrorReporter(node, context).LogError(errorMessage.c_str());
		return nullptr;
	}

	return aiObject->CastToCPuppet();
}
}

namespace BehaviorTree
{
unsigned int TreeLocalGoalPipeCounter = 0;

class GoalPipe : public Action
{
public:
	struct RuntimeData : public IGoalPipeListener
	{
		EntityId entityToRemoveListenerFrom;
		int      goalPipeId;
		bool     goalPipeIsRunning;

		RuntimeData()
			: entityToRemoveListenerFrom(0)
			, goalPipeId(0)
			, goalPipeIsRunning(false)
		{
		}

		~RuntimeData()
		{
			UnregisterGoalPipeListener();
		}

		void UnregisterGoalPipeListener()
		{
			if (this->entityToRemoveListenerFrom != 0)
			{
				assert(this->goalPipeId != 0);

				if (IEntity* entity = gEnv->pEntitySystem->GetEntity(this->entityToRemoveListenerFrom))
				{
					IAIObject* ai = entity->GetAI();
					if (CPipeUser* pipeUser = ai ? ai->CastToCPipeUser() : NULL)
					{
						pipeUser->UnRegisterGoalPipeListener(this, this->goalPipeId);
					}
				}

				this->entityToRemoveListenerFrom = 0;
			}
		}

		// Overriding IGoalPipeListener
		virtual void OnGoalPipeEvent(IPipeUser* pPipeUser, EGoalPipeEvent event, int goalPipeId, bool& unregisterListenerAfterEvent)
		{
			if (event == ePN_Finished || event == ePN_Deselected)
			{
				this->goalPipeIsRunning = false;
				unregisterListenerAfterEvent = true;
				this->entityToRemoveListenerFrom = 0;
			}
		}
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadName(xml, context);

		CGoalPipeXMLReader reader;
		reader.ParseGoalPipe(m_goalPipeName, xml, CPipeManager::SilentlyReplaceDuplicate);

		return LoadSuccess;
	}

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText = m_goalPipeName;
	}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		IPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.goalPipeId = gEnv->pAISystem->AllocGoalPipeId();

		pPipeUser->SelectPipe(AIGOALPIPE_RUN_ONCE, m_goalPipeName, NULL, runtimeData.goalPipeId);
		pPipeUser->RegisterGoalPipeListener(&runtimeData, runtimeData.goalPipeId, "GoalPipeBehaviorTreeNode");

		runtimeData.entityToRemoveListenerFrom = context.entityId;

		runtimeData.goalPipeIsRunning = true;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		IPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.UnregisterGoalPipeListener();
		pPipeUser->SelectPipe(AIGOALPIPE_LOOP, "_first_");
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		return runtimeData.goalPipeIsRunning ? Running : Success;
	}

private:
	void LoadName(const XmlNodeRef& goalPipeXml, const LoadContext& loadContext)
	{
		m_goalPipeName = goalPipeXml->getAttr("name");

		if (m_goalPipeName.empty())
		{
			m_goalPipeName.Format("%s%d_", loadContext.treeName, GetNodeID());
		}
	}

	string m_goalPipeName;
};

//////////////////////////////////////////////////////////////////////////

class LuaBehavior : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData : public IActorBehaviorListener
	{
		EntityId entityToRemoveListenerFrom;
		bool     behaviorIsRunning;

		RuntimeData()
			: entityToRemoveListenerFrom(0)
			, behaviorIsRunning(false)
		{
		}

		~RuntimeData()
		{
			UnregisterBehaviorListener();
		}

		void UnregisterBehaviorListener()
		{
			if (this->entityToRemoveListenerFrom != 0)
			{
				if (IEntity* entity = gEnv->pEntitySystem->GetEntity(this->entityToRemoveListenerFrom))
				{
					IAIObject* ai = entity->GetAI();
					if (CPipeUser* pipeUser = ai ? ai->CastToCPipeUser() : NULL)
					{
						pipeUser->UnregisterBehaviorListener(this);
					}
				}

				this->entityToRemoveListenerFrom = 0;
			}
		}

		// Overriding IActorBehaviorListener
		virtual void BehaviorEvent(IAIObject* actor, EBehaviorEvent event) override
		{
			switch (event)
			{
			case BehaviorFinished:
			case BehaviorFailed:
			case BehaviorInterrupted:
				this->behaviorIsRunning = false;
				break;
			default:
				break;
			}
		}

		// Overriding IActorBehaviorListener
		virtual void BehaviorChanged(IAIObject* actor, const char* current, const char* previous) override
		{
		}
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		m_behaviorName = xml->getAttr("name");

		if (m_behaviorName.empty())
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("LuaBehavior", "name").c_str());
			result = LoadFailure;
		}

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("LuaBehavior");
		xml->setAttr("name", m_behaviorName);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_behaviorName, "name", "^Name");
		archive.doc("Name of the Lua behavior");

		if (m_behaviorName.empty())
		{
			archive.error(m_behaviorName, SerializationUtils::Messages::ErrorEmptyValue("Name"));
		}

		BaseClass::Serialize(archive);
	}
#endif


#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText += m_behaviorName;
	}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		IAIActorProxy* proxy = pPipeUser->GetProxy();

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (proxy)
		{
			proxy->SetBehaviour(m_behaviorName);
			runtimeData.behaviorIsRunning = true;

			pPipeUser->RegisterBehaviorListener(&runtimeData);
			runtimeData.entityToRemoveListenerFrom = context.entityId;
		}
		else
		{
			runtimeData.behaviorIsRunning = false;
		}
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.UnregisterBehaviorListener();

		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		IAIActorProxy* proxy = pPipeUser->GetProxy();
		if (proxy)
		{
			proxy->SetBehaviour("");
		}
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		return runtimeData.behaviorIsRunning ? Running : Success;
	}

private:
	string m_behaviorName;
};

//////////////////////////////////////////////////////////////////////////

class Bubble : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

	Bubble()
		: m_duration(2.0)
		, m_balloonFlags(0)
		, m_baloonFlag(false)
		, m_logFlag(false)
	{
	}

	virtual Status Update(const UpdateContext& context)
	{
		AIQueueBubbleMessage("Behavior Bubble", context.entityId, m_message, m_balloonFlags, m_duration, SAIBubbleRequest::eRT_PrototypeDialog);
		return Success;
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode)
	{
		const LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		m_message = xml->getAttr("message");
		m_duration = 2.0f;
		xml->getAttr("duration", m_duration);
		xml->getAttr("balloon", m_baloonFlag);
		xml->getAttr("log", m_logFlag);

		m_balloonFlags = eBNS_None;

		if (m_baloonFlag) m_balloonFlags |= eBNS_Balloon;
		if (m_logFlag) m_balloonFlags |= eBNS_Log;

		if (m_message.empty())
		{
			ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("Bubble", "message").c_str());
		}

		if (m_duration < 0)
		{
			ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("Bubble", "duration", ToString(m_duration), "Value must be greater or equal than 0").c_str());
		}

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Bubble");
		xml->setAttr("message", m_message);
		xml->setAttr("duration", m_duration);
		xml->setAttr("balloon", m_baloonFlag ? "true" : "false");
		xml->setAttr("log", m_logFlag ? "true" : "false");

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_message, "message", "+Message");
		archive.doc("Message to be shown in the bubble");

		archive(m_duration, "duration", "+Duration");
		archive.doc("Time in seconds that the bubble will last");

		archive(m_baloonFlag, "ballonOn", "+Ballon flag");
		archive.doc("When enabled, shows the message in a 'baloon' above the agent");

		archive(m_logFlag, "logOn", "+Log flag");
		archive.doc("When enabled, writes the message in the general purpose log");


		if (m_message.empty())
		{
			archive.warning(m_message, SerializationUtils::Messages::ErrorEmptyValue("Name"));
		}

		if (m_duration < 0)
		{
			archive.warning(m_duration, SerializationUtils::Messages::ErrorInvalidValueWithReason("Duration", ToString(m_duration), "Value must be greater or equal than 0"));
		}

		BaseClass::Serialize(archive);
	}
#endif

private:
	string m_message;
	float  m_duration;
	uint32 m_balloonFlags;

	bool m_baloonFlag;
	bool m_logFlag;
};

//////////////////////////////////////////////////////////////////////////


class Move : public Action
{
	typedef Action BaseClass;

public:
	enum DestinationType
	{
		Target,
		Cover,
		ReferencePoint,
		LastOp,
		InitialPosition,
	};

	struct RuntimeData
	{
		Vec3              destinationAtTimeOfMovementRequest;
		MovementRequestID movementRequestID;
		Status            pendingStatus;
		float             effectiveStopDistanceSq;

		RuntimeData()
			: destinationAtTimeOfMovementRequest(ZERO)
			, movementRequestID(0)
			, pendingStatus(Running)
			, effectiveStopDistanceSq(0.0f)
		{
		}

		~RuntimeData()
		{
			ReleaseCurrentMovementRequest();
		}

		void ReleaseCurrentMovementRequest()
		{
			if (this->movementRequestID)
			{
				gEnv->pAISystem->GetMovementSystem()->UnsuscribeFromRequestCallback(this->movementRequestID);
				this->movementRequestID = MovementRequestID();
			}
		}

		void MovementRequestCallback(const MovementRequestResult& result)
		{
			assert(this->movementRequestID == result.requestID);

			this->movementRequestID = MovementRequestID();

			if (result == MovementRequestResult::ReachedDestination)
			{
				this->pendingStatus = Success;
			}
			else
			{
				this->pendingStatus = Failure;
			}
		}
	};

private:

	struct Dictionaries
	{
		CXMLAttrReader<DestinationType> to;
		Serialization::StringList       destinationsSerialization;

		Dictionaries()
		{
			to.Reserve(5);
			to.Add("Target", Target);
			to.Add("Cover", Cover);
			to.Add("RefPoint", ReferencePoint);
			to.Add("LastOp", LastOp);
			to.Add("InitialPosition", InitialPosition);

			destinationsSerialization.reserve(5);
			destinationsSerialization.push_back("Target");
			destinationsSerialization.push_back("Cover");
			destinationsSerialization.push_back("RefPoint");
			destinationsSerialization.push_back("LastOp");
			destinationsSerialization.push_back("InitialPosition");
		}
	};

public:
	Move()
		: m_stopWithinDistance(0.0f)
		, m_stopDistanceVariation(0.0f)
		, m_destination(Target)
		, m_fireMode(FIREMODE_OFF)
		, m_dangersFlags(eMNMDangers_None)
		, m_considerActorsAsPathObstacles(false)
		, m_lengthToTrimFromThePathEnd(0.0f)
		, m_shouldTurnTowardsMovementDirectionBeforeMoving(false)
		, m_shouldStrafe(false)
		, m_shouldGlanceInMovementDirection(false)
		, m_avoidDangers(true)
		, m_avoidGroupMates(false)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		const LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		xml->getAttr("stopWithinDistance", m_stopWithinDistance);
		if (m_stopWithinDistance > 0.0f)
		{
			xml->getAttr("stopDistanceVariation", m_stopDistanceVariation);
		}

		m_movementStyle.ReadFromXml(xml);

		s_dictionaries.to.Get(xml, "to", m_destination, true);
		m_movementStyle.SetMovingToCover(m_destination == Cover);

		const AgentDictionary agentDictionary;

		agentDictionary.fireModeXml.Get(xml, "fireMode", m_fireMode);

		xml->getAttr("avoidDangers", m_avoidDangers);

		SetupDangersFlagsForDestination(m_avoidDangers);

		bool avoidGroupMates = true;
		xml->getAttr("avoidGroupMates", avoidGroupMates);
		if (avoidGroupMates)
		{
			m_dangersFlags |= eMNMDangers_GroupMates;
		}

		xml->getAttr("considerActorsAsPathObstacles", m_considerActorsAsPathObstacles);
		xml->getAttr("lengthToTrimFromThePathEnd", m_lengthToTrimFromThePathEnd);

		if (m_stopWithinDistance < 0)
		{
			ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("Move", "stopWithinDistance", ToString(m_stopWithinDistance), "Value must be greater or equal than 0").c_str());
		}

		if (m_stopDistanceVariation < 0)
		{
			ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("Move", "stopDistanceVariation", ToString(m_stopDistanceVariation), "Value must be greater or equal than 0").c_str());
		}

		if (m_stopDistanceVariation > m_stopWithinDistance)
		{
			ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("Move", "stopDistanceVariation", ToString(m_stopDistanceVariation), "Value must be greater or equal than stopWithinDistance value").c_str());
		}

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		const MovementStyleDictionaryCollection movementStyleDictionaryCollection;

		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Move");
		xml->setAttr("stopWithinDistance", m_stopWithinDistance);
		xml->setAttr("stopDistanceVariation", m_stopDistanceVariation);

		m_movementStyle.WriteToXml(xml);

		xml->setAttr("to", Serialization::getEnumLabel<DestinationType>(m_destination));
		xml->setAttr("fireMode", Serialization::getEnumLabel<EFireMode>(m_fireMode));

		xml->setAttr("avoidDangers", m_avoidDangers);
		xml->setAttr("avoidGroupMates", m_avoidGroupMates);
		xml->setAttr("considerActorsAsPathObstacles", m_considerActorsAsPathObstacles);

		xml->getAttr("lengthToTrimFromThePathEnd", m_lengthToTrimFromThePathEnd);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_stopWithinDistance, "stopWithinDistance", "+Stop within distance");
		archive.doc("When real distance to target is smaller than Stop within distance, we stop moving");

		archive(m_stopDistanceVariation, "stopDistanceVariation", "+Stop within distance variation");
		archive.doc("Maximum variation applied to the 'Stop within distance' parameter. Effectively modifies the parameter value to be in the range [variation, Stop within distance + variation]");
		
		archive(m_lengthToTrimFromThePathEnd, "lengthToTrimFromThePathEnd", "+Length to trim from the path end");
		archive.doc("Distance to cut from the path. Positive value means distance from path end. Negative value means distance from path start.");

		const MovementStyleDictionaryCollection movementStyleDictionaryCollection;
		const AgentDictionary agentDictionary;

		MovementStyle::Speed newSpeed(m_movementStyle.GetSpeed());
		SerializationUtils::SerializeEnumList<MovementStyle::Speed>(archive, "speed", "+Speed", movementStyleDictionaryCollection.speedsSerialization, movementStyleDictionaryCollection.speedsXml, newSpeed);
		archive.doc("Speed mode while the node is active. Maximum speed may be limited by current stance");

		m_movementStyle.SetSpeed(newSpeed);

		MovementStyle::Stance newStance(m_movementStyle.GetStance());
		SerializationUtils::SerializeEnumList<MovementStyle::Stance>(archive, "stance", "+Stance", movementStyleDictionaryCollection.stancesSerialization, movementStyleDictionaryCollection.stancesXml, newStance);
		archive.doc("Body stance while the node is active");

		m_movementStyle.SetStance(newStance);

		EBodyOrientationMode newBodyOrientationMode(m_movementStyle.GetBodyOrientationMode());
		SerializationUtils::SerializeEnumList<EBodyOrientationMode>(archive, "bodyOrientation", "+Body orientation", agentDictionary.bodyOrientationsSerialization, agentDictionary.bodyOrientationsXml, newBodyOrientationMode);
		archive.doc("Specifies the body orientation while the moving action is being performed");
		m_movementStyle.SetBodyOrientationMode(newBodyOrientationMode);

		SerializationUtils::SerializeEnumList<DestinationType>(archive, "to", "+To", s_dictionaries.destinationsSerialization, s_dictionaries.to, m_destination);
		archive.doc("Destination of the movement request");
		
		SerializationUtils::SerializeEnumList<EFireMode>(archive, "fireMode", "+Fire mode", agentDictionary.fireModesSerialization, agentDictionary.fireModeXml, m_fireMode);
		archive.doc("Fire mode enabled while the node is active");

		archive(m_avoidDangers, "avoidDangers", "+Avoid dangers");
		archive.doc("When true, movement request avoids areas flagged as dangerous in pathfinding operation");
		
		archive(m_avoidGroupMates, "avoidGroupMates", "+Avoid group mates");
		archive.doc("When true, movement request avoids group mates in pathfinding operation");
		
		archive(m_shouldTurnTowardsMovementDirectionBeforeMoving, "turnTowardsMovementDirectionBeforeMoving", "+Turn towards movement direction before moving");
		archive.doc("When true, character fully turns towards movement direction before starting the move action");
		
		archive(m_shouldStrafe, "strafe", "+Strafe");
		archive.doc("When true, character will apply a strafing behavior  while node is active");
		
		archive(m_shouldGlanceInMovementDirection, "glanceInMovementDirection", "+Glance in movement direction");
		archive.doc("When true, character glances in movement direction");

		m_movementStyle.SetMovingToCover(m_destination == DestinationType::Cover);
		m_movementStyle.SetTurnTowardsMovementDirectionBeforeMoving(m_shouldTurnTowardsMovementDirectionBeforeMoving);
		m_movementStyle.SetShouldStrafe(m_shouldStrafe);
		m_movementStyle.SetShouldGlanceInMovementDirection(m_shouldGlanceInMovementDirection);

		archive(m_considerActorsAsPathObstacles, "considerActorsAsPathObstacles", "+Consider actors as path obstacles");
		archive.doc("When true, other actors as considered obstacles during the pathfinding operation, avoiding collisions.");

		if (m_stopWithinDistance < 0)
		{
			archive.warning(m_stopWithinDistance, SerializationUtils::Messages::ErrorInvalidValueWithReason("Stop Within Distance", ToString(m_stopWithinDistance), "Value must be greater or equal than 0"));
		}

		if (m_stopDistanceVariation < 0)
		{
			archive.warning(m_stopDistanceVariation, SerializationUtils::Messages::ErrorInvalidValueWithReason("Stop Distance Variation", ToString(m_stopDistanceVariation), "Value must be greater or equal than 0"));
		}

		if (m_stopDistanceVariation > m_stopWithinDistance)
		{
			archive.warning(m_stopDistanceVariation, SerializationUtils::Messages::ErrorInvalidValueWithReason("Stop Distance Variation", ToString(m_stopDistanceVariation), "Value must be greater or equal than Stop Within Distance"));
		}

		BaseClass::Serialize(archive);
	}
#endif
	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		CPipeUser* pipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY (!pipeUser)
		{
			return;
		}

		if (m_fireMode != FIREMODE_OFF)
			pipeUser->SetFireMode(m_fireMode);

		runtimeData.pendingStatus = Running;

		const Vec3 destinationPosition = DestinationPositionFor(*pipeUser);

		runtimeData.effectiveStopDistanceSq = square(m_stopWithinDistance + cry_random(0.0f, m_stopDistanceVariation));

		if (Distance::Point_PointSq(pipeUser->GetPos(), destinationPosition) < runtimeData.effectiveStopDistanceSq)
		{
			runtimeData.pendingStatus = Success;
			return;
		}

		RequestMovementTo(destinationPosition, context.entityId, runtimeData,
		                  true); // First time we request a path.
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		if (m_fireMode != FIREMODE_OFF)
		{
			pPipeUser->SetFireMode(FIREMODE_OFF);
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.ReleaseCurrentMovementRequest();
	}

#if defined(DEBUG_MODULAR_BEHAVIOR_TREE) && defined(COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG)
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const override
	{
		const RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(updateContext);
		MovementRequestStatus status;
		gEnv->pAISystem->GetMovementSystem()->GetRequestStatus(runtimeData.movementRequestID, status);
		ConstructHumanReadableText(status, debugText);
	}
#endif

private:
	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.pendingStatus != Running)
			return runtimeData.pendingStatus;

		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}

		if (m_destination == Target || m_destination == LastOp || m_destination == ReferencePoint)
		{
			ChaseTarget(*pPipeUser, context.entityId, runtimeData);
		}

		const bool stopMovementWhenWithinCertainDistance = runtimeData.effectiveStopDistanceSq > 0.0f;

		if (stopMovementWhenWithinCertainDistance)
		{
			if (GetSquaredDistanceToDestination(*pPipeUser, runtimeData) < runtimeData.effectiveStopDistanceSq)
			{
				runtimeData.ReleaseCurrentMovementRequest();
				RequestStop(context.entityId);
				return Success;
			}
		}

		return Running;
	}

	void SetMovementStyle(MovementStyle movementStyle)
	{
		m_movementStyle = movementStyle;
	}

	Vec3 DestinationPositionFor(CPipeUser& pipeUser) const
	{
		switch (m_destination)
		{
		case Target:
			{
				if (IAIObject* target = pipeUser.GetAttentionTarget())
				{
					const Vec3 targetPosition = target->GetPosInNavigationMesh(pipeUser.GetNavigationTypeID());
					Vec3 targetVelocity = target->GetVelocity();
					targetVelocity.z = .0f;   // Don't consider z axe for the velocity since it could create problems when finding the location in the navigation mesh later
					return targetPosition + targetVelocity * 0.5f;
				}
				else
				{
					AIQueueBubbleMessage("Move Node Destination Position",
					                     pipeUser.GetEntityID(),
					                     "Move node could set the destination as the target position.",
					                     eBNS_LogWarning | eBNS_Balloon);
					return Vec3(ZERO);
				}
			}

		case Cover:
			{
				return GetCoverRegisterLocation(pipeUser);
			}

		case ReferencePoint:
			{
				return pipeUser.GetRefPoint()->GetPos();
			}

		case LastOp:
			{
				CAIObject* lastOpAIObejct = pipeUser.GetLastOpResult();
				if (lastOpAIObejct)
				{
					const Vec3 targetVelocity = lastOpAIObejct->GetVelocity();
					const Vec3 targetPosition = lastOpAIObejct->GetPosInNavigationMesh(pipeUser.GetNavigationTypeID());
					return targetPosition + targetVelocity * 0.5f;
				}
				else
				{
					assert(0);
					return Vec3(ZERO);
				}
			}

		case InitialPosition:
			{
				Vec3 initialPosition;
				bool initialPositionIsValid = pipeUser.GetInitialPosition(initialPosition);
				IF_UNLIKELY (!initialPositionIsValid)
				{
					return Vec3Constants<float>::fVec3_Zero;
				}
				return initialPosition;
			}

		default:
			{
				assert(0);
				return Vec3Constants<float>::fVec3_Zero;
			}
		}
	}

	Vec3 GetCoverRegisterLocation(const CPipeUser& pipeUser) const
	{
		if (CoverID coverID = pipeUser.GetCoverRegister())
		{
			const float distanceToCover = pipeUser.GetParameters().distanceToCover;
			return gAIEnv.pCoverSystem->GetCoverLocation(coverID, distanceToCover);
		}
		else
		{
			assert(0);
			AIQueueBubbleMessage("MoveOp:CoverLocation", pipeUser.GetEntityID(), "MoveOp failed to get the cover location due to an invalid Cover ID in the cover register.", eBNS_LogWarning | eBNS_Balloon | eBNS_BlockingPopup);
			return Vec3Constants<float>::fVec3_Zero;
		}
	}

	void RequestMovementTo(const Vec3& position, const EntityId entityID, RuntimeData& runtimeData, const bool firstRequest)
	{
		assert(!runtimeData.movementRequestID);

		MovementRequest movementRequest;
		movementRequest.entityID = entityID;
		movementRequest.destination = position;
		movementRequest.callback = functor(runtimeData, &RuntimeData::MovementRequestCallback);
		movementRequest.style = m_movementStyle;
		movementRequest.dangersFlags = m_dangersFlags;
		movementRequest.considerActorsAsPathObstacles = m_considerActorsAsPathObstacles;
		movementRequest.lengthToTrimFromThePathEnd = m_lengthToTrimFromThePathEnd;
		if (!firstRequest)
		{
			// While chasing we do not want to trigger a separate turn again while we were
			// already following an initial path.
			movementRequest.style.SetTurnTowardsMovementDirectionBeforeMoving(false);
		}

		runtimeData.movementRequestID = gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);

		runtimeData.destinationAtTimeOfMovementRequest = position;
	}

	void ChaseTarget(CPipeUser& pipeUser, const EntityId entityID, RuntimeData& runtimeData)
	{
		const Vec3 targetPosition = DestinationPositionFor(pipeUser);

		const float targetDeviation = targetPosition.GetSquaredDistance(runtimeData.destinationAtTimeOfMovementRequest);
		const float deviationThreshold = square(0.5f);

		if (targetDeviation > deviationThreshold)
		{
			runtimeData.ReleaseCurrentMovementRequest();
			RequestMovementTo(targetPosition, entityID, runtimeData,
			                  false); // No longer the first request.
		}
	}

	float GetSquaredDistanceToDestination(CPipeUser& pipeUser, RuntimeData& runtimeData)
	{
		const Vec3 destinationPosition = DestinationPositionFor(pipeUser);
		return destinationPosition.GetSquaredDistance(pipeUser.GetPos());
	}

	void RequestStop(const EntityId entityID)
	{
		MovementRequest movementRequest;
		movementRequest.entityID = entityID;
		movementRequest.type = MovementRequest::Stop;
		gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);
	}

	void SetupDangersFlagsForDestination(bool shouldAvoidDangers)
	{
		if (!shouldAvoidDangers)
		{
			m_dangersFlags = eMNMDangers_None;
			return;
		}

		switch (m_destination)
		{
		case Target:
			m_dangersFlags = eMNMDangers_Explosive;
			break;
		case Cover:
		case ReferencePoint:
		case LastOp:
		case InitialPosition:
			m_dangersFlags = eMNMDangers_AttentionTarget | eMNMDangers_Explosive;
			break;
		default:
			assert(0);
			m_dangersFlags = eMNMDangers_None;
			break;
		}
	}

private:
	static Dictionaries s_dictionaries;

	float               m_stopWithinDistance;
	float               m_stopDistanceVariation;
	MovementStyle       m_movementStyle;
	DestinationType     m_destination;
	EFireMode           m_fireMode;
	MNMDangersFlags     m_dangersFlags;
	bool                m_considerActorsAsPathObstacles;
	float               m_lengthToTrimFromThePathEnd;

	// Movement style helpers
	bool m_shouldTurnTowardsMovementDirectionBeforeMoving;
	bool m_shouldStrafe;
	bool m_shouldGlanceInMovementDirection;

	// Dangers Flags helpers
	bool m_avoidDangers;
	bool m_avoidGroupMates;
};


Move::Dictionaries Move::s_dictionaries;

//////////////////////////////////////////////////////////////////////////

class QueryTPS : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		CTacticalPointQueryInstance queryInstance;
		bool                        queryProcessing;
	};

	QueryTPS()
		: m_register(AI_REG_COVER)
		, m_queryID(0)
	{
	}

	struct Dictionaries
	{
		CXMLAttrReader<EAIRegister> registerXml;
		Serialization::StringList   registerSerialization;

		Dictionaries()
		{
			registerXml.Reserve(2);
			//reg.Add("LastOp",     AI_REG_LASTOP);
			registerXml.Add("RefPoint", AI_REG_REFPOINT);
			//reg.Add("AttTarget",  AI_REG_ATTENTIONTARGET);
			//reg.Add("Path",       AI_REG_PATH);
			registerXml.Add("Cover", AI_REG_COVER);

			registerSerialization.reserve(2);
			registerSerialization.push_back("RefPoint");
			registerSerialization.push_back("Cover");
		}
	};

	static Dictionaries s_dictionaries;

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		const char* queryName = xml->getAttr("name");
		m_queryID = queryName ? gEnv->pAISystem->GetTacticalPointSystem()->GetQueryID(queryName) : TPSQueryID(0);

		if (m_queryID == 0)
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("QueryTPS", "name", queryName, "Query does not exist").c_str());
			result = LoadFailure;
		}

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		m_tpsQueryName = queryName;
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

		if (!s_dictionaries.registerXml.Get(xml, "register", m_register))
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("QueryTPS", "register").c_str());
			result = LoadFailure;
		}

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("QueryTPS");
		xml->setAttr("name", m_tpsQueryName);
		xml->setAttr("register", Serialization::getEnumLabel<EAIRegister>(m_register));

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_tpsQueryName, "name", "^Name");
		archive.doc("Name of the TPS query. Query must already exist");

		SerializationUtils::SerializeEnumList<EAIRegister>(archive, "register", "^Register", s_dictionaries.registerSerialization, s_dictionaries.registerXml, m_register);
		archive.doc("AI Register used to store the result of the query");

		if (m_tpsQueryName.empty())
		{
			archive.error(m_tpsQueryName, SerializationUtils::Messages::ErrorEmptyValue("Name"));
		}
		else 
		{
			m_queryID = m_tpsQueryName ? gEnv->pAISystem->GetTacticalPointSystem()->GetQueryID(m_tpsQueryName) : TPSQueryID(0);
			if (m_queryID == 0)
			{
				archive.error(m_tpsQueryName, SerializationUtils::Messages::ErrorInvalidValueWithReason("Name", m_tpsQueryName, "Query does not exist"));
			}
		}

		BaseClass::Serialize(archive);
	}
#endif


#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText = m_tpsQueryName;
	}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		CommitQuery(*pPipeUser, context);
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CancelQuery(context);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		const ETacticalPointQueryState state = runtimeData.queryInstance.GetStatus();

		switch (state)
		{
		case eTPSQS_InProgress:
			return Running;

		case eTPSQS_Success:
			return HandleSuccess(context, runtimeData);

		case eTPSQS_Fail:
		case eTPSQS_Error:
			return Failure;

		default:
			assert(false);
			return Failure;
		}
	}

private:
	void CommitQuery(CPipeUser& pipeUser, const UpdateContext& context)
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		QueryContext queryContext;
		InitQueryContextFromActor(&pipeUser, queryContext);
		runtimeData.queryInstance.SetQueryID(m_queryID);
		runtimeData.queryInstance.SetContext(queryContext);
		runtimeData.queryInstance.Execute(eTPQF_LockResults);
		runtimeData.queryProcessing = true;
	}

	void CancelQuery(const UpdateContext& context)
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.queryProcessing)
		{
			runtimeData.queryInstance.UnlockResults();
			runtimeData.queryInstance.Cancel();
			runtimeData.queryProcessing = false;
		}
	}

	Status HandleSuccess(const UpdateContext& context, RuntimeData& runtimeData)
	{
		runtimeData.queryInstance.UnlockResults();
		assert(runtimeData.queryInstance.GetOptionUsed() >= 0);

		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}

		STacticalPointResult point = runtimeData.queryInstance.GetBestResult();
		assert(point.IsValid());

		if (m_register == AI_REG_REFPOINT)
		{
			if (point.flags & eTPDF_AIObject)
			{
				if (CAIObject* pAIObject = gAIEnv.pObjectContainer->GetAIObject(point.aiObjectId))
				{
					pPipeUser->SetRefPointPos(pAIObject->GetPos(), pAIObject->GetEntityDir());
					return Success;
				}
				else
				{
					return Failure;
				}
			}
			else if (point.flags & eTPDF_CoverID)
			{
				pPipeUser->SetRefPointPos(point.vPos, Vec3Constants<float>::fVec3_OneY);
				return Success;
			}

			// we can expect a position. vObjDir may not be set if this is not a hidespot, but should be zero.
			pPipeUser->SetRefPointPos(point.vPos, point.vObjDir);
			return Success;
		}
		else if (m_register == AI_REG_COVER)
		{
			assert(point.flags & eTPDF_CoverID);

			if (gAIEnv.pCoverSystem->IsCoverOccupied(point.coverID) && gAIEnv.pCoverSystem->GetCoverOccupant(point.coverID) != pPipeUser->GetEntityID())
			{
				// Found cover was occupied by someone else in the meantime
				return Failure;
			}

			pPipeUser->SetCoverRegister(point.coverID);
			return Success;
		}
		else
		{
			assert(false);
			return Failure;
		}
	}

	EAIRegister m_register;
	TPSQueryID  m_queryID;

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	string m_tpsQueryName;
#endif // DEBUG_MODULAR_BEHAVIOR_TREE
};

QueryTPS::Dictionaries QueryTPS::s_dictionaries;

//////////////////////////////////////////////////////////////////////////

// A gate that is open if a snippet of Lua code returns true,
// and closed if the Lua code returns false.
class LuaGate : public Decorator
{
public:
	typedef Decorator BaseClass;

	struct RuntimeData
	{
		bool gateIsOpen;

		RuntimeData() : gateIsOpen(false)
		{
		}
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = LoadSuccess;

		m_code = xml->getAttr("code");

		if (m_code.empty())
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("LuaGate", "code").c_str());
			result = LoadFailure;
		}

		m_scriptFunction = SmartScriptFunction(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(m_code.c_str(), m_code.length(), "LuaGate"));
		if (!m_scriptFunction)
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("LuaGate", "code", m_code, "Could not compile code").c_str());
			result = LoadFailure;
		}

		const LoadResult childResult = LoadChildFromXml(xml, context, strictMode);

		if (result == LoadSuccess && childResult == LoadSuccess)
		{
			return result;
		}
		else
		{
			return LoadFailure;
		}
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("LuaGate");
		xml->setAttr("code", m_code);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_code, "code", "^Code");
		archive.doc("Lua code used as a condition for the gate. Code should return a boolean value");

		if (m_code.empty())
		{
			archive.error(m_code, SerializationUtils::Messages::ErrorEmptyValue("Code"));
		}

		m_scriptFunction = SmartScriptFunction(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(m_code.c_str(), m_code.length(), "LuaGate"));
		if (!m_scriptFunction)
		{
			archive.error(m_code, SerializationUtils::Messages::ErrorInvalidValueWithReason("Code", m_code, "Could not compile code"));
		}

		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		assert(m_scriptFunction != NULL);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.gateIsOpen = false;

		if (IEntity* entity = gEnv->pEntitySystem->GetEntity(context.entityId))
		{
			gEnv->pScriptSystem->SetGlobalValue("entity", entity->GetScriptTable());

			bool luaReturnValue = false;
			Script::CallReturn(gEnv->pScriptSystem, m_scriptFunction, luaReturnValue);
			if (luaReturnValue)
				runtimeData.gateIsOpen = true;

			gEnv->pScriptSystem->SetGlobalToNull("entity");

		}
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.gateIsOpen)
			return BaseClass::Update(context);
		else
			return Failure;
	}

private:
	SmartScriptFunction m_scriptFunction;
	stack_string m_code;
};

//////////////////////////////////////////////////////////////////////////

class AdjustCoverStance : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		Timer timer;
	};

	AdjustCoverStance()
		: m_duration(-1.0f)
		, m_variation(0.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode)
	{
		const stack_string str = xml->getAttr("duration");

		if (str == "continuous")
			m_duration = -1.0;
		else
		{
			xml->getAttr("duration", m_duration);
			xml->getAttr("variation", m_variation);

			if (m_duration < 0)
			{
				ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("AdjustCoverStance", "duration", ToString(m_duration), "Value must be greater or equal than 0").c_str());
			}

			if (m_variation < 0)
			{
				ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("AdjustCoverStance", "variation", ToString(m_variation), "Value must be greater or equal than 0").c_str());
			}

		}

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("AdjustCoverStance");
		xml->setAttr("duration", m_duration);
		xml->setAttr("variation", m_variation);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_duration, "duration", "+Duration");
		archive.doc("Time in seconds before the node exits yielding Success. If value is -1 runs forever.");

		archive(m_variation, "variation", "+Variation");
		archive.doc("Maximum variation time in seconds applied to the duration parameter. Effectively modifies the duration to be in the range [duration, duration + variation]	");

		if (m_duration < 0)
		{
			archive.warning(m_duration, SerializationUtils::Messages::ErrorInvalidValueWithReason("Duration", ToString(m_duration), "Value must be greater or equal than 0"));
		}

		if (m_variation < 0)
		{
			archive.warning(m_variation, SerializationUtils::Messages::ErrorInvalidValueWithReason("Variation", ToString(m_variation), "Value must be greater or equal than 0"));
		}

		BaseClass::Serialize(archive);
	}
#endif

	virtual void OnInitialize(const UpdateContext& context)
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		if (m_duration >= 0.0f)
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			runtimeData.timer.Reset(m_duration, m_variation);
		}

		ClearCoverPosture(pPipeUser);
	}

	virtual Status Update(const UpdateContext& context)
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}

		if (pPipeUser->IsInCover())
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

			const CoverHeight coverHeight = pPipeUser->CalculateEffectiveCoverHeight();
			pPipeUser->m_State.bodystate = (coverHeight == HighCover) ? STANCE_HIGH_COVER : STANCE_LOW_COVER;
			return runtimeData.timer.Elapsed() ? Success : Running;
		}
		else
		{
			return Failure;
		}
	}

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		if (m_duration == -1.0f)
		{
			debugText.Format("continuous");
		}
		else
		{
			const RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(updateContext);
			debugText.Format("%0.1f (%0.1f)", runtimeData.timer.GetSecondsLeft(), m_duration);
		}
	}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

private:
	void ClearCoverPosture(CPipeUser* pipeUser)
	{
		assert(pipeUser);
		pipeUser->GetProxy()->ResetAGInput(AIAG_ACTION);
		pipeUser->m_State.lean = 0;
		pipeUser->m_State.peekOver = 0;
		pipeUser->m_State.coverRequest.ClearCoverAction();
	}

	float m_duration;
	float m_variation;
	Timer m_timer;
};

//////////////////////////////////////////////////////////////////////////

class SetAlertness : public Action
{
public:
	typedef Action BaseClass;

	struct RuntimeData
	{
	};

	SetAlertness()
		: m_alertness(0)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		xml->getAttr("value", m_alertness);

		if (m_alertness < 0 || m_alertness > 2)
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("SetAlertness", "value", "", "Valid range is between 0 and 2.").c_str());
			result = LoadFailure;
		}
		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("SetAlertness");
		xml->setAttr("value", m_alertness);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_alertness, "value", "^Value");
		archive.doc("Set alertness value between [0-2]. 0 represents minimum alertness; 2 maximum.");

		if (m_alertness < 0 || m_alertness > 2)
		{
			archive.error(m_alertness, SerializationUtils::Messages::ErrorInvalidValueWithReason("Value", ToString(m_alertness), "Alertness valid range is [0-2]"));
		}

		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}

		IAIActorProxy* proxy = pPipeUser->GetProxy();

		if (proxy)
			proxy->SetAlertnessState(m_alertness);

		return Success;
	}

private:
	int m_alertness;
};

//////////////////////////////////////////////////////////////////////////

class Communicate : public Action
{
	typedef Action BaseClass;
public:
	struct RuntimeData : public ICommunicationManager::ICommInstanceListener
	{
		CTimeValue startTime;
		CommPlayID playID;
		bool       commFinished; // Set on communication end

		RuntimeData()
			: startTime(0.0f)
			, playID(0)
			, commFinished(false)
		{
		}

		~RuntimeData()
		{
			UnregisterListener();
		}

		void UnregisterListener()
		{
			if (this->playID)
			{
				gEnv->pAISystem->GetCommunicationManager()->RemoveInstanceListener(this->playID);
				this->playID = CommPlayID(0);
			}
		}

		void OnCommunicationEvent(
		  ICommunicationManager::ECommunicationEvent event,
		  EntityId actorID, const CommPlayID& _playID)
		{
			switch (event)
			{
			case ICommunicationManager::CommunicationCancelled:
			case ICommunicationManager::CommunicationFinished:
			case ICommunicationManager::CommunicationExpired:
				{
					if (this->playID == _playID)
					{
						this->commFinished = true;
					}
					else
					{
						AIWarning("Communicate behavior node received event for a communication not matching requested playID.");
					}
				}
				break;
			default:
				break;
			}
		}
	};

	Communicate()
		: m_channelID(0)
		, m_commID(0)
		, m_ordering(SCommunicationRequest::Unordered)
		, m_expiry(0.0f)
		, m_minSilence(-1.0f)
		, m_ignoreSound(false)
		, m_ignoreAnim(false)
		, m_timeout(8.0f)
		, m_waitUntilFinished(true)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		ICommunicationManager* communicationManager = gEnv->pAISystem->GetCommunicationManager();

		m_commName = xml->getAttr("name");
		if (!m_commName.empty())
		{
			m_commID = communicationManager->GetCommunicationID(m_commName);
			if (!communicationManager->GetCommunicationName(m_commID))
			{
				ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("Communicate", "name", m_commName, "Communication name does not exist").c_str());
				m_commID = CommID(0);
				result = LoadFailure;
			}
		}
		else
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("Communicate", "name").c_str());
			result = LoadFailure;
		}

		m_channelName = xml->getAttr("channel");
		if (!m_channelName.empty())
		{
			m_channelID = communicationManager->GetChannelID(m_channelName);
			if (!m_channelID)
			{
				ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("Communicate", "channel", m_channelName, "Channel name does not exist").c_str());
				result = LoadFailure;
			}
		}
		else
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("Communicate", "channel").c_str());
			result = LoadFailure;
		}

		if (xml->haveAttr("waitUntilFinished"))
			xml->getAttr("waitUntilFinished", m_waitUntilFinished);

		if (xml->haveAttr("timeout"))
			xml->getAttr("timeout", m_timeout);

		if (xml->haveAttr("expiry"))
			xml->getAttr("expiry", m_expiry);

		if (xml->haveAttr("minSilence"))
			xml->getAttr("minSilence", m_minSilence);

		if (xml->haveAttr("ignoreSound"))
			xml->getAttr("ignoreSound", m_ignoreSound);

		if (xml->haveAttr("ignoreAnim"))
			xml->getAttr("ignoreAnim", m_ignoreAnim);

		if (m_timeout < 0)
		{
			ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("Communicate", "timeout", ToString(m_timeout), "Value must be greater or equal than 0").c_str());
		}

		if (m_expiry < 0)
		{
			ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("Communicate", "expiry", ToString(m_expiry), "Value must be greater or equal than 0").c_str());
		}

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Communicate");
		xml->setAttr("name", m_commName);
		xml->setAttr("channel", m_channelName);
		xml->setAttr("waitUntilFinished", m_waitUntilFinished);
		xml->setAttr("timeout", m_timeout);
		xml->setAttr("expiry", m_expiry);
		xml->setAttr("minSilence", m_minSilence);
		xml->setAttr("ignoreSound", m_ignoreSound);
		xml->setAttr("ignoreAnim", m_ignoreAnim);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		ICommunicationManager* communicationManager = gEnv->pAISystem->GetCommunicationManager();

		archive(m_commName, "commName", "+Communication name");
		archive.doc("Name of the communication to be played.. Must already exists");

		m_commID = communicationManager->GetCommunicationID(m_commName);
		if (!communicationManager->GetCommunicationName(m_commID))
		{
			archive.error(m_commName, SerializationUtils::Messages::ErrorInvalidValueWithReason("Communication name", m_commName, "Communication name does not exist"));
			m_commID = CommID(0);
		}

		archive(m_channelName, "channelName", "+Channel name");
		archive.doc("Name of the channel on which the communication is to be set. Must exists already");
		m_channelID = communicationManager->GetChannelID(m_channelName);
		if (!m_channelID)
		{
			archive.error(m_channelName, SerializationUtils::Messages::ErrorInvalidValueWithReason("Channel name", m_channelName, "Channel name does not exist"));
		}

		archive(m_timeout, "timeout", "+Timeout");
		archive.doc("Time in seconds before the node exists yielding Success");

		archive(m_expiry, "expiry", "+Expiry");
		archive.doc("The amount of seconds the communication can wait for the channel to be clear");

		archive(m_minSilence, "minSilence", "+Minimum silence");
		archive.doc("The amount of seconds the channel will be silenced after the communication is played");

		archive(m_waitUntilFinished, "waitUntilFinished", "+Wait until finished");
		archive.doc("When enabled, node waits communication to finish before succeeding");

		archive(m_ignoreSound, "ignoreSound", "+Ignore sound");
		archive.doc("When enabled, sets the sound component of the communication to be ignored");

		archive(m_ignoreAnim, "ignoreAnim", "+Ignore animation");
		archive.doc("When enabled, sets the animation component of the communication to be ignored");

		if (m_timeout < 0)
		{
			archive.warning(m_timeout, SerializationUtils::Messages::ErrorInvalidValueWithReason("Timeout", ToString(m_timeout), "Value must be greater or equal than 0"));
		}

		if (m_expiry < 0)
		{
			archive.warning(m_expiry, SerializationUtils::Messages::ErrorInvalidValueWithReason("Expiry", ToString(m_expiry), "Value must be greater or equal than 0"));
		}

		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.startTime = context.frameStartTime;
		runtimeData.commFinished = false;

		ICommunicationManager& commManager = *gEnv->pAISystem->GetCommunicationManager();

		SCommunicationRequest request;
		request.channelID = m_channelID;
		request.commID = m_commID;
		request.contextExpiry = m_expiry;
		request.minSilence = m_minSilence;
		request.ordering = m_ordering;
		request.skipCommAnimation = m_ignoreAnim;
		request.skipCommSound = m_ignoreSound;

		if (m_waitUntilFinished)
			request.eventListener = &runtimeData;

		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		IAIActorProxy* proxy = pPipeUser->GetProxy();
		IF_UNLIKELY (!proxy)
		{
			ErrorReporter(*this, context).LogError("Communication failed to start, agent did not have a valid AI proxy.");
			return;
		}

		request.configID = commManager.GetConfigID(proxy->GetCommunicationConfigName());
		request.actorID = context.entityId;

		runtimeData.playID = commManager.PlayCommunication(request);

		if (!runtimeData.playID)
		{
			ErrorReporter(*this, context).LogWarning("Communication failed to start.");
		}
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.UnregisterListener();
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		// If we've received a callback, then return success
		if (runtimeData.commFinished || !m_waitUntilFinished)
		{
			return Success;
		}

		// Time out if angle threshold isn't reached for some time (to avoid deadlocks)
		float elapsedTime = context.frameStartTime.GetDifferenceInSeconds(runtimeData.startTime);
		if (elapsedTime > m_timeout)
		{
			ErrorReporter(*this, context).LogWarning("Communication timed out.");
			return Success;
		}

		return Running;
	}

private:

	string                           m_commName;
	string                           m_channelName;
	CommID                           m_commID;
	CommChannelID                    m_channelID;
	SCommunicationRequest::EOrdering m_ordering;
	float                            m_expiry;
	float                            m_minSilence;
	float                            m_timeout;
	bool                             m_ignoreSound;
	bool                             m_ignoreAnim;
	bool                             m_waitUntilFinished;
};

//////////////////////////////////////////////////////////////////////////

class Animate : public Action
{
	typedef Action BaseClass;
public:
	enum PlayMode
	{
		PlayOnce = 0,
		InfiniteLoop
	};

private:
	struct Dictionaries
	{
		CXMLAttrReader<PlayMode>    playModeXml;
		Serialization::StringList   playModeSerialization;

		Dictionaries()
		{
			playModeXml.Reserve(2);
			playModeXml.Add("PlayOnce", PlayMode::PlayOnce);
			playModeXml.Add("InfiniteLoop", PlayMode::InfiniteLoop);

			playModeSerialization.reserve(2);
			playModeSerialization.push_back("PlayOnce");
			playModeSerialization.push_back("InfiniteLoop");
		}
	};

	static Dictionaries s_dictionaries;

public:

	struct RuntimeData
	{
		bool signalWasSet;

		RuntimeData() : signalWasSet(false)
		{
		}
	};

	struct ConfigurationData
	{
		string   m_name;
		PlayMode m_playMode;
		bool     m_urgent;
		bool     m_setBodyDirectionTowardsAttentionTarget;

		ConfigurationData()
			: m_name("")
			, m_playMode(PlayOnce)
			, m_urgent(true)
			, m_setBodyDirectionTowardsAttentionTarget(false)
		{
		}

		LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context, const Animate& parent)
		{
			LoadResult result = LoadSuccess;

			m_name = xml->getAttr("name");
			IF_UNLIKELY (m_name.empty())
			{
				ErrorReporter(parent, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("Animate", "name").c_str());
				result = LoadFailure;
			}

			xml->getAttr("urgent", m_urgent);

			if (xml->haveAttr("playMode"))
			{
				if (!s_dictionaries.playModeXml.Get(xml, "playMode", m_playMode))
				{
					ErrorReporter(parent, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("Animate", "playMode").c_str());
					result = LoadFailure;
				}
			}
			else
			{
				// Legacy compatibility
				bool loop = false;
				xml->getAttr("loop", loop);
				m_playMode = loop ? InfiniteLoop : PlayOnce;

				ErrorReporter(parent, context).LogWarning("Loop value is deprecated. Please use playMode=InfiniteLoop or playMode=PlayOnce instead");
			}

			xml->getAttr("setBodyDirectionTowardsAttentionTarget", m_setBodyDirectionTowardsAttentionTarget);

			return result;
		}
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		const LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		const LoadResult configurationResult = m_configurationData.LoadFromXml(xml, context, *this);
		
		if (result == LoadSuccess && configurationResult == LoadSuccess)
		{
			return LoadSuccess;
		}
		else
		{
			return LoadFailure;
		}
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Animate");
		xml->setAttr("name", m_configurationData.m_name);
		xml->setAttr("playMode", Serialization::getEnumLabel<PlayMode>(m_configurationData.m_playMode));
		xml->setAttr("urgent", m_configurationData.m_urgent);
		xml->setAttr("setBodyDirectionTowardsAttentionTarget", m_configurationData.m_setBodyDirectionTowardsAttentionTarget);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_configurationData.m_name, "name", "+Name");
		archive.doc("Name of the animation to be played");

		if (m_configurationData.m_name.empty())
		{
			archive.error(m_configurationData.m_name, SerializationUtils::Messages::ErrorEmptyValue("Name"));
		}

		SerializationUtils::SerializeEnumList<PlayMode>(archive, "playMode", "+Play Mode", s_dictionaries.playModeSerialization, s_dictionaries.playModeXml, m_configurationData.m_playMode);
		archive.doc("Animation play mode. Can be run once or in a loop");

		archive(m_configurationData.m_urgent, "urgent", "+Urgent");
		archive.doc("When enabled flags the animation as urgent (high priority)");

		archive(m_configurationData.m_setBodyDirectionTowardsAttentionTarget, "setBodyDirectionTowardsAttentionTarget", "+Set body direction towards attention target");
		archive.doc("Orient body to face the attention target");

		BaseClass::Serialize(archive);
	}
#endif

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const override
	{
		debugText = m_configurationData.m_name;
	}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		IAIActorProxy* proxy = pPipeUser->GetProxy();
		runtimeData.signalWasSet = proxy->SetAGInput(GetAGInputMode(), m_configurationData.m_name, m_configurationData.m_urgent);

		assert(runtimeData.signalWasSet);
		IF_UNLIKELY (!runtimeData.signalWasSet)
			gEnv->pLog->LogError("Animate behavior tree node failed call to SetAGInput(%i, %s).", GetAGInputMode(), m_configurationData.m_name.c_str());

		if (m_configurationData.m_setBodyDirectionTowardsAttentionTarget)
		{
			if (IAIObject* target = pPipeUser->GetAttentionTarget())
			{
				pPipeUser->SetBodyTargetDir((target->GetPos() - pPipeUser->GetPos()).GetNormalizedSafe());
			}
		}
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		if (m_configurationData.m_setBodyDirectionTowardsAttentionTarget)
		{
			pPipeUser->ResetBodyTargetDir();
		}

		IAIActorProxy* proxy = pPipeUser->GetProxy();
		proxy->ResetAGInput(GetAGInputMode());

		switch (m_configurationData.m_playMode)
		{
		case PlayOnce:
			proxy->SetAGInput(AIAG_SIGNAL, "none", true);
			break;

		case InfiniteLoop:
			proxy->SetAGInput(AIAG_ACTION, "idle", true);
			break;
		}
	}

	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		IF_UNLIKELY (!runtimeData.signalWasSet)
			return Success;

		if (m_configurationData.m_playMode == PlayOnce)
		{
			IAIActorProxy* proxy = pPipeUser->GetProxy();
			if (proxy->IsSignalAnimationPlayed(m_configurationData.m_name))
				return Success;
		}

		return Running;
	}

private:
	EAIAGInput GetAGInputMode() const
	{
		switch (m_configurationData.m_playMode)
		{
		case PlayOnce:
			return AIAG_SIGNAL;

		case InfiniteLoop:
			return AIAG_ACTION;

		default:
			break;
		}

		// We should never get here!
		assert(false);
		return AIAG_SIGNAL;
	}

private:
	ConfigurationData m_configurationData;
};

Animate::Dictionaries Animate::s_dictionaries;
//////////////////////////////////////////////////////////////////////////

class Signal : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

private:
	struct Dictionaries
	{
		CXMLAttrReader<AISignals::ESignalFilter> filtersXml;
		Serialization::StringList     filtersSerialization;

		Dictionaries()
		{
			filtersXml.Reserve(3);
			filtersXml.Add("Sender", AISignals::SIGNALFILTER_SENDER);
			filtersXml.Add("Group", AISignals::SIGNALFILTER_GROUPONLY);
			filtersXml.Add("GroupExcludingSender", AISignals::SIGNALFILTER_GROUPONLY_EXCEPT);

			filtersSerialization.reserve(3);
			filtersSerialization.push_back("Sender");
			filtersSerialization.push_back("Group");
			filtersSerialization.push_back("GroupExcludingSender");
		}
	};

public:

	Signal()
		: m_filter(AISignals::SIGNALFILTER_SENDER)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		m_signalName = xml->getAttr("name");

		if (m_signalName.empty())
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("Signal", "name").c_str());
			result = LoadFailure;
		}

		s_signalDictionaries.filtersXml.Get(xml, "filter", m_filter);

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Signal");
		xml->setAttr("name", m_signalName);
		xml->setAttr("filter", Serialization::getEnumLabel<AISignals::ESignalFilter>(m_filter));

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_signalName, "name", "^Name");
		archive.doc("Name of the transition signal");

		SerializationUtils::SerializeEnumList<AISignals::ESignalFilter>(archive, "filter", "^Filter", s_signalDictionaries.filtersSerialization, s_signalDictionaries.filtersXml, m_filter);
		archive.doc("Filter used when sending the signal");

		if (m_signalName.empty())
		{
			archive.error(m_signalName, SerializationUtils::Messages::ErrorEmptyValue("Name"));
		}
		
		BaseClass::Serialize(archive);
	}
#endif

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const override
	{
		debugText = m_signalName;
	}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		SendSignal(context);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		return Success;
	}

	void SendSignal(const UpdateContext& context)
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		const AISignals::ISignalDescription& signalDesc = GetAISystem()->GetSignalManager()->GetSignalDescription(m_signalName.c_str());
		const AISignals::SignalSharedPtr pSignal = GetAISystem()->GetSignalManager()->CreateSignal(AISIGNAL_DEFAULT, signalDesc, pPipeUser->GetEntityID());

		GetAISystem()->SendSignal(m_filter, pSignal);
	}

private:
	static Dictionaries s_signalDictionaries;

	string              m_signalName;
	AISignals::ESignalFilter       m_filter;
};

Signal::Dictionaries Signal::s_signalDictionaries;

//////////////////////////////////////////////////////////////////////////

class SendTransitionSignal : public Signal
{
public:
	virtual Status Update(const UpdateContext& context) override
	{
		return Running;
	}
};

//////////////////////////////////////////////////////////////////////////

struct Dictionaries
{
	CXMLAttrReader<EStance>   stancesXml;
	Serialization::StringList stancesSerialization;

	Dictionaries()
	{
		stancesXml.Reserve(4);
		stancesXml.Add("Stand", STANCE_STAND);
		stancesXml.Add("Crouch", STANCE_CROUCH);
		stancesXml.Add("Relaxed", STANCE_RELAXED);
		stancesXml.Add("Alerted", STANCE_ALERTED);

		stancesSerialization.reserve(4);
		stancesSerialization.push_back("Stand");
		stancesSerialization.push_back("Crouch");
		stancesSerialization.push_back("Relaxed");
		stancesSerialization.push_back("Alerted");

	}
};

Dictionaries s_stanceDictionary;

class Stance : public Action
{
	typedef Action BaseClass;
public:
	struct RuntimeData
	{
	};

	Stance()
		: m_stance(STANCE_STAND)
		, m_stanceToUseIfSlopeIsTooSteep(STANCE_STAND)
		, m_allowedSlopeNormalDeviationFromUpInRadians(0.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		s_stanceDictionary.stancesXml.Get(xml, "name", m_stance);

		float degrees = 90.0f;
		xml->getAttr("allowedSlopeNormalDeviationFromUpInDegrees", degrees);
		m_allowedSlopeNormalDeviationFromUpInRadians = DEG2RAD(degrees);

		s_stanceDictionary.stancesXml.Get(xml, "stanceToUseIfSlopeIsTooSteep", m_stanceToUseIfSlopeIsTooSteep);

		if (m_allowedSlopeNormalDeviationFromUpInRadians < 0)
		{
			ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("Stance", "allowedSlopeNormalDeviationFromUpInDegrees", ToString(degrees), "Value must be greater or equal than 0").c_str());
		}

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Stance");

		xml->setAttr("name", Serialization::getEnumLabel<EStance>(m_stance));
		xml->setAttr("stanceToUseIfSlopeIsTooSteep", Serialization::getEnumLabel<EStance>(m_stanceToUseIfSlopeIsTooSteep));
		xml->setAttr("allowedSlopeNormalDeviationFromUpInDegrees", RAD2DEG(m_allowedSlopeNormalDeviationFromUpInRadians));

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		SerializationUtils::SerializeEnumList<EStance>(archive, "name", "+Stance", s_stanceDictionary.stancesSerialization, s_stanceDictionary.stancesXml, m_stance);
		archive.doc("Stance to use by default");

		SerializationUtils::SerializeEnumList<EStance>(archive, "stanceToUseIfSlopeIsTooSteep", "+Stance to use if slope is too steep", s_stanceDictionary.stancesSerialization, s_stanceDictionary.stancesXml, m_stanceToUseIfSlopeIsTooSteep);
		archive.doc("Stance to use if terrain slope is too steep.");

		float allowedSlopeNormalDeviationFromUpInDeg = RAD2DEG(m_allowedSlopeNormalDeviationFromUpInRadians);
		archive(allowedSlopeNormalDeviationFromUpInDeg, "allowedSlopeNormalDeviationFromUpInDegrees", "+Allowed slope normal deviation from up in degrees");
		archive.doc("If the real slope deviation is less than the allowed slope normal deviation, the character will use the default stance. Otherwise, it will use the stance set up for steep slopes");

		m_allowedSlopeNormalDeviationFromUpInRadians = DEG2RAD(allowedSlopeNormalDeviationFromUpInDeg);

		if (allowedSlopeNormalDeviationFromUpInDeg < 0)
		{
			archive.warning(allowedSlopeNormalDeviationFromUpInDeg, SerializationUtils::Messages::ErrorInvalidValueWithReason("Allowed slope normal deviation from up in degrees", ToString(allowedSlopeNormalDeviationFromUpInDeg), "Value must be greater or equal than 0"));
		}

		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}

		EStance slopeVerifiedStance = m_stance;

		if (IEntity* entity = gEnv->pEntitySystem->GetEntity(context.entityId))
		{
			if (IPhysicalEntity* physicalEntity = entity->GetPhysics())
			{
				const Vec3& up = Vec3Constants<float>::fVec3_OneZ;

				Vec3 groundNormal = up;

				if (physicalEntity && physicalEntity->GetType() == PE_LIVING)
				{
					pe_status_living status;
					if (physicalEntity->GetStatus(&status) > 0)
					{
						if (!status.groundSlope.IsZero() && status.groundSlope.IsValid())
						{
							groundNormal = status.groundSlope;
							groundNormal.NormalizeSafe(up);
						}
					}
				}

				if (acosf(clamp_tpl(groundNormal.Dot(up), -1.0f, +1.0f)) > m_allowedSlopeNormalDeviationFromUpInRadians)
				{
					slopeVerifiedStance = m_stanceToUseIfSlopeIsTooSteep;
				}
			}
		}

		pPipeUser->m_State.bodystate = slopeVerifiedStance;
		return Success;
	}

private:
	EStance m_stance;
	EStance m_stanceToUseIfSlopeIsTooSteep;
	float   m_allowedSlopeNormalDeviationFromUpInRadians;
};

//////////////////////////////////////////////////////////////////////////

class LuaWrapper : public Decorator
{
	typedef Decorator BaseClass;

public:
	struct RuntimeData
	{
		EntityId    entityId;
		bool        executeExitScriptIfDestructed;
		LuaWrapper* node;

		RuntimeData()
			: entityId(0)
			, executeExitScriptIfDestructed(false)
			, node(NULL)
		{
		}

		~RuntimeData()
		{
			if (this->executeExitScriptIfDestructed && this->node)
			{
				this->node->ExecuteExitScript(*this);

				this->node = NULL;
				this->entityId = 0;
				this->executeExitScriptIfDestructed = false;
			}
		}
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		stack_string enterCode = xml->getAttr("onEnter");
		stack_string exitCode = xml->getAttr("onExit");

		if (!enterCode.empty())
			m_enterScriptFunction.reset(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(enterCode.c_str(), enterCode.length(), "LuaWrapper - enter"));

		if (!exitCode.empty())
			m_exitScriptFunction.reset(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(exitCode.c_str(), exitCode.length(), "LuaWrapper - exit"));

		return LoadChildFromXml(xml, context, strictMode);
	}

	void ExecuteEnterScript(RuntimeData& runtimeData)
	{
		ExecuteScript(m_enterScriptFunction, runtimeData.entityId);

		runtimeData.entityId = runtimeData.entityId;
		runtimeData.executeExitScriptIfDestructed = true;
	}

	void ExecuteExitScript(RuntimeData& runtimeData)
	{
		ExecuteScript(m_exitScriptFunction, runtimeData.entityId);

		runtimeData.executeExitScriptIfDestructed = false;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		BaseClass::OnInitialize(context);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.node = this;
		runtimeData.entityId = context.entityId;
		ExecuteEnterScript(runtimeData);
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		BaseClass::OnTerminate(context);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		ExecuteExitScript(runtimeData);
	}

private:
	void ExecuteScript(SmartScriptFunction scriptFunction, EntityId entityId)
	{
		if (scriptFunction)
		{
			IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
			IF_UNLIKELY (!entity)
			{
				return;
			}

			IScriptSystem* scriptSystem = gEnv->pScriptSystem;
			scriptSystem->SetGlobalValue("entity", entity->GetScriptTable());
			Script::Call(scriptSystem, scriptFunction);
			scriptSystem->SetGlobalToNull("entity");
		}
	}

	SmartScriptFunction m_enterScriptFunction;
	SmartScriptFunction m_exitScriptFunction;
};

//////////////////////////////////////////////////////////////////////////
class ExecuteLua : public Action
{
	typedef Action BaseClass;
public:
	struct RuntimeData
	{
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		m_code = xml->getAttr("code");
		IF_UNLIKELY (m_code.empty())
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("ExecuteLua", "code").c_str());
			result = LoadFailure;
		}

		m_scriptFunction.reset(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(m_code.c_str(), m_code.length(), "ExecuteLua behavior tree node"));
		IF_UNLIKELY(!m_scriptFunction)
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("ExecuteLua", "code", m_code, "Code could not be compiled").c_str());
			result = LoadFailure;
		}

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("ExecuteLua");
		xml->setAttr("code", m_code);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_code, "code", "+Code");
		archive.doc("Lua code to be executed. Does not need to have boolean return type");

		if (m_code.empty())
		{
			archive.error(m_code, SerializationUtils::Messages::ErrorEmptyValue("Code"));
		}
		else
		{
			m_scriptFunction.reset(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(m_code.c_str(), m_code.length(), "Execute Lua behavior tree node"));
			if (!m_scriptFunction)
			{
				archive.error(m_code, SerializationUtils::Messages::ErrorInvalidValueWithReason("Code", m_code, "Failed to compile Lua code"));
			}
		}

		BaseClass::Serialize(archive);
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);
		ExecuteScript(m_scriptFunction, context.entityId);
		return Success;
	}

private:
	void ExecuteScript(SmartScriptFunction scriptFunction, const EntityId entityId) const
	{
		IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
		IF_UNLIKELY (!entity)
			return;

		IScriptSystem* scriptSystem = gEnv->pScriptSystem;
		scriptSystem->SetGlobalValue("entity", entity->GetScriptTable());
		Script::Call(scriptSystem, scriptFunction);
		scriptSystem->SetGlobalToNull("entity");
	}

	SmartScriptFunction m_scriptFunction;
	stack_string m_code;
};

//////////////////////////////////////////////////////////////////////////

// Executes Lua code and translates the return value of that code
// from true/false to success/failure. It can then be used to build
// preconditions in the modular behavior tree.
class AssertLua : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		m_code = xml->getAttr("code");
		IF_UNLIKELY (m_code.empty())
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("AssertLua", "code").c_str());
			result = LoadFailure;
		}

		m_scriptFunction.reset(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(m_code.c_str(), m_code.length(), "AssertLua behavior tree node"));
		IF_UNLIKELY (!m_scriptFunction)
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("AssertLua", "code", m_code, "Code could not be compiled").c_str());
			result = LoadFailure;
		}

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("AssertLua");
		xml->setAttr("code", m_code);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_code, "code", "+Code");
		archive.doc("Lua code used as a condition for the assert. If True, node Success. Otherwise, Fails.");

		if (m_code.empty())
		{
			archive.error(m_code, SerializationUtils::Messages::ErrorEmptyValue("Code"));
		}
		else
		{
			m_scriptFunction.reset(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(m_code.c_str(), m_code.length(), "AssertLua behavior tree node"));
			if(!m_scriptFunction)
			{
				archive.error(m_code, SerializationUtils::Messages::ErrorInvalidValueWithReason("Code", m_code, "Failed to compile Lua code"));
			}
		}

		BaseClass::Serialize(archive);
	}
#endif
protected:
	virtual Status Update(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		IEntity* entity = gEnv->pEntitySystem->GetEntity(context.entityId);
		IF_UNLIKELY (!entity)
			return Failure;

		bool luaReturnValue = false;
		gEnv->pScriptSystem->SetGlobalValue("entity", entity->GetScriptTable());
		Script::CallReturn(gEnv->pScriptSystem, m_scriptFunction, luaReturnValue);
		gEnv->pScriptSystem->SetGlobalToNull("entity");

		return luaReturnValue ? Success : Failure;
	}

private:
	SmartScriptFunction m_scriptFunction;
	stack_string m_code;
};

//////////////////////////////////////////////////////////////////////////

class GroupScope : public Decorator
{
public:
	typedef Decorator BaseClass;

	struct RuntimeData
	{
		EntityId     entityIdUsedToAquireToken;
		GroupScopeID scopeID;
		bool         gateIsOpen;

		RuntimeData()
			: entityIdUsedToAquireToken(0)
			, scopeID(0)
			, gateIsOpen(false)
		{
		}

		~RuntimeData()
		{
			ReleaseGroupScope();
		}

		void ReleaseGroupScope()
		{
			if (this->entityIdUsedToAquireToken != 0)
			{
				IEntity* entity = gEnv->pEntitySystem->GetEntity(this->entityIdUsedToAquireToken);
				if (entity)
				{
					const CAIActor* aiActor = CastToCAIActorSafe(entity->GetAI());
					if (aiActor)
					{
						CAIGroup* group = GetAISystem()->GetAIGroup(aiActor->GetGroupId());
						if (group)
							group->LeaveGroupScope(aiActor, this->scopeID);

						this->entityIdUsedToAquireToken = 0;
					}
				}
			}
		}
	};

	GroupScope()
		: m_scopeID(0)
		, m_allowedConcurrentUsers(1u)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = LoadSuccess;

		const stack_string scopeName = xml->getAttr("name");

		if (scopeName.empty())
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("GroupScope", "name").c_str());
			result = LoadFailure;
		}

		m_scopeID = CAIGroup::GetGroupScopeId(scopeName.c_str());

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		m_scopeName = scopeName.c_str();
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

		xml->getAttr("allowedConcurrentUsers", m_allowedConcurrentUsers);

		const LoadResult childResult = LoadChildFromXml(xml, context, strictMode);

		if (result == LoadSuccess && childResult == LoadSuccess)
		{
			return LoadSuccess;
		}
		else
		{
			return LoadFailure;
		}
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("GroupScope");
		xml->setAttr("name", m_scopeName);
		xml->setAttr("allowedConcurrentUsers", m_allowedConcurrentUsers);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_scopeName, "name", "^Name");
		archive.doc("Name of the group ");

		archive(m_allowedConcurrentUsers, "allowedConcurrentUsers", "^Allowed concurrent users");
		archive.doc("Maximum number of concurrent members in the group");

		if (m_scopeName.empty())
		{
			archive.error(m_scopeName, SerializationUtils::Messages::ErrorEmptyValue("Name"));
		}

		BaseClass::Serialize(archive);
	}
#endif

protected:

	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.entityIdUsedToAquireToken = 0;
		runtimeData.scopeID = m_scopeID;
		runtimeData.gateIsOpen = false;

		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(context.entityId))
		{
			if (const CAIActor* pActor = CastToCAIActorSafe(pEntity->GetAI()))
			{
				if (EnterGroupScope(*pActor, runtimeData))
				{
					runtimeData.gateIsOpen = true;
				}
			}
		}
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		BaseClass::OnTerminate(context);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.ReleaseGroupScope();
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.gateIsOpen)
			return BaseClass::Update(context);
		else
			return Failure;
	}

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText.Format("%s", m_scopeName.c_str());
	}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

private:

	bool EnterGroupScope(const CAIActor& actor, RuntimeData& runtimeData)
	{
		CAIGroup* pGroup = GetAISystem()->GetAIGroup(actor.GetGroupId());
		assert(pGroup);
		if (pGroup->EnterGroupScope(&actor, m_scopeID, m_allowedConcurrentUsers))
		{
			runtimeData.entityIdUsedToAquireToken = actor.GetEntityID();
			return true;
		}
		return false;
	}

	GroupScopeID m_scopeID;
	uint32       m_allowedConcurrentUsers;

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	string m_scopeName;
#endif // DEBUG_MODULAR_BEHAVIOR_TREE
};

//////////////////////////////////////////////////////////////////////////

class Look : public Action
{
	typedef Action BaseClass;

public:
	enum At
	{
		AttentionTarget,
		ClosestGroupMember,
		ReferencePoint,
	};

	struct RuntimeData
	{
		std::shared_ptr<Vec3> lookTarget;
	};

private:
	struct Dictionaries
	{
		CXMLAttrReader<At>        lookAtXml;
		Serialization::StringList lookAtSerialization;

		Dictionaries()
		{
			lookAtXml.Reserve(3);
			lookAtXml.Add("AttentionTarget", At::AttentionTarget);
			lookAtXml.Add("ClosestGroupMember", At::ClosestGroupMember);
			lookAtXml.Add("ReferencePoint", At::ReferencePoint);

			lookAtSerialization.reserve(3);
			lookAtSerialization.push_back("AttentionTarget");
			lookAtSerialization.push_back("ClosestGroupMember");
			lookAtSerialization.push_back("ReferencePoint");
		}
	};

public:
	Look() : m_targetType(AttentionTarget)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		IF_UNLIKELY (!s_lookDictionaries.lookAtXml.Get(xml, "at", m_targetType))
		{
			const string lookAtTarget = xml->getAttr("at");
			if (lookAtTarget == "Target")
			{
				m_targetType = At::AttentionTarget;
			}
			else
			{
				ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("Look", "at").c_str());
				result = LoadFailure;
			}
		}

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Look");
		xml->setAttr("at", Serialization::getEnumLabel<At>(m_targetType));

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		SerializationUtils::SerializeEnumList<At>(archive, "at", "^At", s_lookDictionaries.lookAtSerialization, s_lookDictionaries.lookAtXml, m_targetType);
		archive.doc("Where to look at");
		BaseClass::Serialize(archive);
	}
#endif
protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		GetRuntimeData<RuntimeData>(context).lookTarget = pPipeUser->CreateLookTarget();
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.lookTarget.reset();
	}

	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		assert(runtimeData.lookTarget.get());

		if (m_targetType == AttentionTarget)
		{
			if (IAIObject* target = pPipeUser->GetAttentionTarget())
			{
				*runtimeData.lookTarget = target->GetPos();
			}
		}
		else if (m_targetType == ClosestGroupMember)
		{
			const Group& group = gAIEnv.pGroupManager->GetGroup(pPipeUser->GetGroupId());
			const Group::Members& members = group.GetMembers();
			Group::Members::const_iterator it = members.begin();
			Group::Members::const_iterator end = members.end();
			float closestDistSq = std::numeric_limits<float>::max();
			tAIObjectID closestID = INVALID_AIOBJECTID;
			Vec3 closestPos(ZERO);
			const Vec3& myPosition = pPipeUser->GetPos();
			const tAIObjectID myAIObjectID = pPipeUser->GetAIObjectID();
			for (; it != end; ++it)
			{
				const tAIObjectID memberID = *it;
				if (memberID != myAIObjectID)
				{
					IAIObject* member = GetAISystem()->GetAIObjectManager()->GetAIObject(memberID);
					if (member)
					{
						const Vec3& memberPos = member->GetPos();
						const float distSq = myPosition.GetSquaredDistance(memberPos);
						if (distSq < closestDistSq)
						{
							closestID = memberID;
							closestDistSq = distSq;
							closestPos = memberPos;
						}
					}
				}
			}

			if (closestID)
				*runtimeData.lookTarget = closestPos;
			else
				*runtimeData.lookTarget = Vec3(ZERO);
		}
		else if (m_targetType == ReferencePoint)
		{
			if (IAIObject* rerefencePoint = pPipeUser->GetRefPoint())
			{
				*runtimeData.lookTarget = rerefencePoint->GetPos();
			}
		}

		return Running;
	}

private:
	static Dictionaries s_lookDictionaries;
	At                  m_targetType;
};

Look::Dictionaries Look::s_lookDictionaries;

//////////////////////////////////////////////////////////////////////////

class Aim : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		CStrongRef<CAIObject> fireTarget;
		CTimeValue            startTime;
		float                 timeSpentWithCorrectDirection;

		RuntimeData()
			: startTime(0.0f)
			, timeSpentWithCorrectDirection(0.0f)
		{
		}

		~RuntimeData()
		{
			if (fireTarget)
				fireTarget.Release();
		}
	};

	Aim()
		: m_angleThresholdCosine(0.0f)
		, m_durationOnceWithinThreshold(0.0f)   // -1 means "aim forever"
		, m_aimAtReferencePoint(false)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		const LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		float angleThresholdInDegrees = 30.0f;
		xml->getAttr("angleThreshold", angleThresholdInDegrees);
		m_angleThresholdCosine = cosf(DEG2RAD(angleThresholdInDegrees));

		const stack_string at = xml->getAttr("at");
		if (at == "RefPoint")
		{
			m_aimAtReferencePoint = true;
		}

		const stack_string str = xml->getAttr("durationOnceWithinThreshold");
		if (str == "forever")
		{
			m_durationOnceWithinThreshold = -1.0f;
		}
		else
		{
			m_durationOnceWithinThreshold = 0.2f;
			xml->getAttr("durationOnceWithinThreshold", m_durationOnceWithinThreshold);
		}

		if (m_durationOnceWithinThreshold != -1.0 && m_durationOnceWithinThreshold < 0)
		{
			ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("Aim", "durationOnceWithinThreshold", ToString(m_durationOnceWithinThreshold), "Value must be greater or equal than 0").c_str());
		}

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Aim");

		xml->setAttr("angleThreshold", RAD2DEG(acosf(m_angleThresholdCosine)));
		xml->setAttr("durationOnceWithinThreshold", m_durationOnceWithinThreshold);
		xml->setAttr("aimAtReferencePoint", m_aimAtReferencePoint);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		float angleThresholdInDegrees = RAD2DEG(acosf(m_angleThresholdCosine));
		archive(angleThresholdInDegrees, "angleThreshold", "+Angle threshold");
		archive.doc("Allowed error between desired aim direction and real aim direction");

		m_angleThresholdCosine = cosf(DEG2RAD(angleThresholdInDegrees));

		archive(m_durationOnceWithinThreshold, "durationOnceWithinThreshold", "+Duration once within threshold");
		archive.doc("Time in seconds before the node exists yielding Success once character orientation is already within threshold");

		archive(m_aimAtReferencePoint, "aimAtReferencePoint", "+Aim at reference point");
		archive.doc("When enable, aims at reference point. Otherwise, aims at Target.");


		if (m_durationOnceWithinThreshold != -1.0f && m_durationOnceWithinThreshold < 0)
		{
			archive.warning(m_durationOnceWithinThreshold, SerializationUtils::Messages::ErrorInvalidValueWithReason("Duration once within threshold", ToString(m_durationOnceWithinThreshold), "Value must be greater or equal than 0 or -1 to run forever"));
		}

		BaseClass::Serialize(archive);
	}
#endif
protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		Vec3 fireTargetPosition(ZERO);

		if (m_aimAtReferencePoint)
		{
			CAIObject* referencePoint = pPipeUser->GetRefPoint();
			IF_UNLIKELY (!referencePoint)
			{
				return;
			}

			fireTargetPosition = referencePoint->GetPos();
		}
		else if (IAIObject* target = pPipeUser->GetAttentionTarget())
		{
			fireTargetPosition = target->GetPos();
		}
		else
		{
			return;
		}

		gAIEnv.pAIObjectManager->CreateDummyObject(runtimeData.fireTarget);
		runtimeData.fireTarget->SetPos(fireTargetPosition);
		pPipeUser->SetFireTarget(runtimeData.fireTarget);
		pPipeUser->SetFireMode(FIREMODE_AIM);
		runtimeData.startTime = context.frameStartTime;
		runtimeData.timeSpentWithCorrectDirection = 0.0f;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.fireTarget)
			runtimeData.fireTarget.Release();

		pPipeUser->SetFireTarget(NILREF);
		pPipeUser->SetFireMode(FIREMODE_OFF);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}
		
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		IF_UNLIKELY (!runtimeData.fireTarget)
			return Success;
		CAIObject* fireTargetAIObject = runtimeData.fireTarget.GetAIObject();
		IF_UNLIKELY (fireTargetAIObject == NULL)
		{
			ErrorReporter(*this, context).LogError("Expected fire target!");
			CRY_ASSERT(false, "Behavior Tree node Aim suddenly lost its fire target!!");
			return Success;
		}

		const bool aimForever = m_durationOnceWithinThreshold < 0.0f;
		if (aimForever)
			return Running;

		const SAIBodyInfo& bodyInfo = pPipeUser->QueryBodyInfo();

		const Vec3 desiredDirection = (fireTargetAIObject->GetPos() - bodyInfo.vFirePos).GetNormalized();
		const Vec3 currentDirection = bodyInfo.vFireDir;

		// Wait until we aim in the desired direction
		const float dotProduct = currentDirection.Dot(desiredDirection);
		if (dotProduct > m_angleThresholdCosine)
		{
			const float timeSpentWithCorrectDirection = runtimeData.timeSpentWithCorrectDirection + context.frameDeltaTime;
			runtimeData.timeSpentWithCorrectDirection = timeSpentWithCorrectDirection;
			if (timeSpentWithCorrectDirection > m_durationOnceWithinThreshold)
				return Success;
		}

		if (context.frameStartTime > runtimeData.startTime + CTimeValue(8.0f))
		{
			gEnv->pLog->LogWarning("Agent '%s' failed to aim towards %f %f %f. Timed out...", pPipeUser->GetName(), fireTargetAIObject->GetPos().x, fireTargetAIObject->GetPos().y, fireTargetAIObject->GetPos().z);
			return Success;
		}

		return Running;
	}

private:
	float m_angleThresholdCosine;
	float m_durationOnceWithinThreshold;   // -1 means "aim forever"
	bool  m_aimAtReferencePoint;
};

//////////////////////////////////////////////////////////////////////////

class AimAroundWhileUsingAMachingGun : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		CStrongRef<CAIObject> fireTarget;
		CTimeValue            lastUpdateTime;
		Vec3                  initialDirection;
		Vec3                  mountedWeaponPivot;

		RuntimeData()
			: lastUpdateTime(0.0f)
			, initialDirection(ZERO)
			, mountedWeaponPivot(ZERO)
		{
		}
	};

	AimAroundWhileUsingAMachingGun()
		: m_maxAngleRange(.0f)
		, m_minSecondsBeweenUpdates(.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		float maxAngleRange = 30.0f;
		xml->getAttr("maxAngleRange", maxAngleRange);
		m_maxAngleRange = DEG2RAD(maxAngleRange);

		m_minSecondsBeweenUpdates = 2;
		xml->getAttr("minSecondsBeweenUpdates", m_minSecondsBeweenUpdates);

		bool useReferencePointForInitialDirectionAndPivotPosition = true;
		if (!xml->getAttr("useReferencePointForInitialDirectionAndPivotPosition", useReferencePointForInitialDirectionAndPivotPosition) ||
		    useReferencePointForInitialDirectionAndPivotPosition == false)
		{
			// This is currently not used but it forces the readability of the behavior of the node.
			// It forces to understand that the reference point is used to store the pivot position and the inizial direction
			// of the machine gun
			// Only accepted value is 1
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("AimAroundWhileUsingAMachingGun", "useReferencePointForInitialDirectionAndPivotPosition").c_str());
			result = LoadFailure;
		}

		return result;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		CAIObject* pRefPoint = pPipeUser->GetRefPoint();

		if (pRefPoint)
		{
			runtimeData.initialDirection = pRefPoint->GetEntityDir();
			runtimeData.mountedWeaponPivot = pRefPoint->GetPos();
		}
		else 
		{
			runtimeData.initialDirection = pPipeUser->GetEntityDir();
			runtimeData.mountedWeaponPivot = pPipeUser->GetPos();
		}

		runtimeData.initialDirection.Normalize();

		gAIEnv.pAIObjectManager->CreateDummyObject(runtimeData.fireTarget);
		pPipeUser->SetFireTarget(runtimeData.fireTarget);
		pPipeUser->SetFireMode(FIREMODE_AIM);

		UpdateAimingPosition(runtimeData);
		runtimeData.lastUpdateTime = context.frameStartTime;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.fireTarget)
			runtimeData.fireTarget.Release();

		if (pPipeUser)
		{
			pPipeUser->SetFireTarget(NILREF);
			pPipeUser->SetFireMode(FIREMODE_OFF);
		}
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		IF_UNLIKELY (!runtimeData.fireTarget)
			return Success;

		const float elapsedSecondsFromPreviousUpdate = context.frameStartTime.GetDifferenceInSeconds(runtimeData.lastUpdateTime);
		if (elapsedSecondsFromPreviousUpdate > m_minSecondsBeweenUpdates)
		{
			UpdateAimingPosition(runtimeData);
			runtimeData.lastUpdateTime = context.frameStartTime;;
		}

		return Running;
	}

private:

	void UpdateAimingPosition(RuntimeData& runtimeData)
	{
		const float offSetForInizialAimingPosition = 3.0f;

		Vec3 newFireTargetPos = runtimeData.mountedWeaponPivot + runtimeData.initialDirection * offSetForInizialAimingPosition;
		const float rotationAngle = cry_random(-m_maxAngleRange, m_maxAngleRange);
		Vec3 zAxis(0, 0, 1);
		newFireTargetPos = newFireTargetPos.GetRotated(runtimeData.mountedWeaponPivot, zAxis, cos_tpl(rotationAngle), sin_tpl(rotationAngle));
		runtimeData.fireTarget->SetPos(newFireTargetPos);
	}

	float m_maxAngleRange;
	float m_minSecondsBeweenUpdates;
};

//////////////////////////////////////////////////////////////////////////

class TurnBody : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		Vec3  desiredBodyDirection;
		float timeSpentAligning;
		float correctBodyDirectionTime;

		RuntimeData()
			: desiredBodyDirection(ZERO)
			, timeSpentAligning(0.0f)
			, correctBodyDirectionTime(0.0f)
		{
		}
	};

	TurnBody()
		: m_turnTarget(TurnTarget_Invalid)
		, m_alignWithTarget(false)
		, m_stopWithinAngleCosined(cosf(DEG2RAD(17.0f)))   // Backwards compatibility.
		, m_randomMinAngle(-1.0f)
		, m_randomMaxAngle(-1.0f)
		, m_randomTurnRightChance(-1.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		m_alignWithTarget = false;
		m_turnTarget = TurnTarget_Invalid;
		s_turnBodyDictionary.turnTarget.Get(xml, "towards", m_turnTarget);
		TurnTarget alignTurnTarget = TurnTarget_Invalid;
		s_turnBodyDictionary.turnTarget.Get(xml, "alignWith", alignTurnTarget);
		if (m_turnTarget == TurnTarget_Invalid)
		{
			m_turnTarget = alignTurnTarget;
			if (alignTurnTarget != TurnTarget_Invalid)
			{
				m_alignWithTarget = true;
			}
		}

		float stopWithinAngleDegrees;
		if (xml->getAttr("stopWithinAngle", stopWithinAngleDegrees))
		{
			if ((stopWithinAngleDegrees < 0.0f) || (stopWithinAngleDegrees > 180.0f))
			{
				ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("TurnBody", "stopWithinAngle", ToString(stopWithinAngleDegrees), "Valid range is between 0 and 180").c_str());
				result = LoadFailure;
			}
			m_stopWithinAngleCosined = cosf(DEG2RAD(stopWithinAngleDegrees));
		}

		// The turning randomization can be applied on any kind of target.
		m_randomMinAngle = -1.0f;
		m_randomMaxAngle = -1.0f;
		m_randomTurnRightChance = -1.0f;
		float angleDegrees = 0.0f;
		if (xml->getAttr("randomMinAngle", angleDegrees))
		{
			if ((angleDegrees < 0.0f) || (angleDegrees > 180.0f))
			{
				
				ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("TurnBody", "randomMinAngle", ToString(angleDegrees), "Valid range is between 0 and 180").c_str());
				result = LoadFailure;
			}
			m_randomMinAngle = DEG2RAD(angleDegrees);
		}
		angleDegrees = 0.0f;
		if (xml->getAttr("randomMaxAngle", angleDegrees))
		{
			if ((angleDegrees < 0.0f) || (angleDegrees > 180.0f))
			{
				ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("TurnBody", "randomMaxAngle", ToString(angleDegrees), "Valid range is between 0 and 180").c_str());
				result = LoadFailure;
			}
			m_randomMaxAngle = DEG2RAD(angleDegrees);
		}
		if ((m_randomMinAngle >= 0.0f) || (m_randomMaxAngle >= 0.0f))
		{
			if (m_randomMinAngle < 0.0f)
			{
				m_randomMinAngle = 0.0f;
			}
			if (m_randomMaxAngle < 0.0f)
			{
				m_randomMaxAngle = gf_PI;
			}
			if (m_randomMinAngle > m_randomMaxAngle)
			{
				ErrorReporter(*this, context).LogError("Min. and max. random angles are swapped");
				result = LoadFailure;
			}

			m_randomTurnRightChance = 0.5f;
			if (xml->getAttr("randomTurnRightChance", m_randomTurnRightChance))
			{
				if ((m_randomTurnRightChance < 0.0f) || (m_randomTurnRightChance > 1.0f))
				{
					ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("TurnBody", "randomTurnRightChance", ToString(m_randomTurnRightChance), "Valid range is between 0 and 1").c_str());
					result = LoadFailure;
				}
			}

			if (m_turnTarget == TurnTarget_Invalid)
			{
				// We will turn in a completely random direction.
				m_turnTarget = TurnTarget_Random;
			}
		}

		if (m_turnTarget == TurnTarget_Invalid)
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("TurnBody", "target").c_str());
			result = LoadFailure;
		}

		return result;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		Vec3 desiredBodyDirection(ZERO);
		switch (m_turnTarget)
		{
		case TurnTarget_Invalid:
		default:
			// We should never get here!
			assert(false);
			break;

		case TurnTarget_Random:
			desiredBodyDirection = pPipeUser->GetEntityDir();
			break;

		case TurnTarget_AttentionTarget:
			if (IAIObject* target = pPipeUser->GetAttentionTarget())
			{
				if (m_alignWithTarget)
				{
					desiredBodyDirection = pPipeUser->GetEntityDir();
				}
				else
				{
					desiredBodyDirection = target->GetPos() - pPipeUser->GetPhysicsPos();
				}
				desiredBodyDirection.z = 0.0f;
				desiredBodyDirection.Normalize();
			}
			break;

		case TurnTarget_RefPoint:
			CAIObject* refPointObject = pPipeUser->GetRefPoint();
			if (refPointObject != NULL)
			{
				if (m_alignWithTarget)
				{
					desiredBodyDirection = refPointObject->GetEntityDir();
				}
				else
				{
					desiredBodyDirection = refPointObject->GetPos() - pPipeUser->GetPhysicsPos();
				}
				desiredBodyDirection.z = 0.0f;
				desiredBodyDirection.Normalize();
			}
			break;
		}

		desiredBodyDirection = ApplyRandomTurnOnNormalXYPlane(desiredBodyDirection);

		if (!desiredBodyDirection.IsZero())
		{
			pPipeUser->SetBodyTargetDir(desiredBodyDirection);
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.desiredBodyDirection = desiredBodyDirection;
		runtimeData.timeSpentAligning = 0.0f;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		pPipeUser->ResetBodyTargetDir();
	}

	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}
		
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.desiredBodyDirection.IsZero())
		{
			ErrorReporter(*this, context).LogWarning("Desired body direction is invalid.");
			return Success;
		}

		// If animation body direction is within the angle threshold,
		// wait for some time and then mark the agent as ready to move
		const Vec3 actualBodyDir = pPipeUser->GetBodyInfo().vAnimBodyDir;
		const bool turnedTowardsDesiredDirection = (actualBodyDir.Dot(runtimeData.desiredBodyDirection) > m_stopWithinAngleCosined);
		if (turnedTowardsDesiredDirection)
			runtimeData.correctBodyDirectionTime += context.frameDeltaTime;
		else
			runtimeData.correctBodyDirectionTime = 0.0f;

		const float timeSpentAligning = runtimeData.timeSpentAligning + context.frameDeltaTime;
		runtimeData.timeSpentAligning = timeSpentAligning;

		if (runtimeData.correctBodyDirectionTime > 0.2f)
			return Success;

		const float timeout = 8.0f;
		if (timeSpentAligning > timeout)
		{
			ErrorReporter(*this, context).LogWarning(
			  "Failed to turn in direction %f %f %f within %f seconds. Proceeding anyway.",
			  runtimeData.desiredBodyDirection.x, runtimeData.desiredBodyDirection.y, runtimeData.desiredBodyDirection.z, timeout);
			return Success;
		}

		return Running;
	}

private:

	Vec3 ApplyRandomTurnOnNormalXYPlane(const Vec3& initialNormal) const
	{
		if ((m_randomMinAngle < 0.0f) || (m_randomMaxAngle <= 0.0f))
		{
			return initialNormal;
		}

		float randomAngle = cry_random(m_randomMinAngle, m_randomMaxAngle);
		if (cry_random(0.0f, 1.0f) < m_randomTurnRightChance)
		{
			randomAngle = -randomAngle;
		}

		float sinAngle, cosAngle;
		sincos_tpl(randomAngle, &sinAngle, &cosAngle);

		Vec3 randomizedNormal = Vec3(
		  (initialNormal.x * cosAngle) - (initialNormal.y * sinAngle),
		  (initialNormal.y * cosAngle) + (initialNormal.x * sinAngle),
		  0.0f);
		randomizedNormal.Normalize();

		return randomizedNormal;
	}

private:
	enum TurnTarget
	{
		TurnTarget_Invalid = 0,
		TurnTarget_Random,
		TurnTarget_AttentionTarget,
		TurnTarget_RefPoint
	};

	struct TurnBodyDictionary
	{
		CXMLAttrReader<TurnBody::TurnTarget> turnTarget;

		TurnBodyDictionary()
		{
			turnTarget.Reserve(2);
			turnTarget.Add("Target", TurnTarget_AttentionTarget);     // Name is a bit confusing but also used elsewhere.
			turnTarget.Add("RefPoint", TurnTarget_RefPoint);
		}
	};

	static TurnBodyDictionary s_turnBodyDictionary;

private:
	TurnTarget m_turnTarget;

	// If true then we should align with the target (take on the same rotation); false
	// if we should rotate towards it.
	bool m_alignWithTarget;

	// If we are within this threshold angle then we can stop the rotation (in radians)
	// in the range of [0.0f .. gf_PI]. This can be used to prevent rotations when
	// we already 'close enough' to the target angle. The value is already cosined.
	float m_stopWithinAngleCosined;

	// -1.0f if there is no randomization limit (otherwise should be in the range of
	// [0.0f .. gf_PI]).
	float m_randomMinAngle;
	float m_randomMaxAngle;

	// The chance to do a rightwards random turn in the range of [0.0f .. 1.0f],
	// with 0.0f = always turn left, 1.0f = always turn right (-1.0f if there
	// is no randomization).
	float m_randomTurnRightChance;
};

BehaviorTree::TurnBody::TurnBodyDictionary BehaviorTree::TurnBody::s_turnBodyDictionary;

//////////////////////////////////////////////////////////////////////////

class ClearTargets : public Action
{
	typedef Action BaseClass;
public:
	struct RuntimeData
	{
	};
#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		const MovementStyleDictionaryCollection movementStyleDictionaryCollection;

		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("ClearTargets");

		return xml;
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}

		gAIEnv.pTargetTrackManager->ResetAgent(pPipeUser->GetAIObjectID());
		pPipeUser->GetProxy()->ResendTargetSignalsNextFrame();

		return Success;
	}
};

//////////////////////////////////////////////////////////////////////////

class StopMovement : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		MovementRequestID movementRequestID;
		uint32            motionIdleScopeID;
		FragmentID        motionIdleFragmentID;
		bool              hasStopped;

		RuntimeData()
			: movementRequestID(0)
			, motionIdleScopeID(SCOPE_ID_INVALID)
			, motionIdleFragmentID(FRAGMENT_ID_INVALID)
			, hasStopped(false)
		{
		}

		~RuntimeData()
		{
			CancelMovementRequest();
		}

		void MovementRequestCallback(const MovementRequestResult& result)
		{
			this->hasStopped = true;
		}

		void CancelMovementRequest()
		{
			if (this->movementRequestID)
			{
				gEnv->pAISystem->GetMovementSystem()->UnsuscribeFromRequestCallback(this->movementRequestID);
				this->movementRequestID = 0;
			}
		}
	};

	StopMovement()
		: m_waitUntilStopped(false)
		, m_waitUntilIdleAnimation(false)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		const LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		m_waitUntilStopped = false;
		xml->getAttr("waitUntilStopped", m_waitUntilStopped);

		m_waitUntilIdleAnimation = false;
		xml->getAttr("waitUntilIdleAnimation", m_waitUntilIdleAnimation);

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("StopMovement");

		xml->setAttr("waitUntilStopped", m_waitUntilStopped);
		xml->setAttr("waitUntilIdleAnimation", m_waitUntilIdleAnimation);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		archive(m_waitUntilStopped, "waitUntilStopped", "+Wait until stopped");
		archive.doc("When enabled, waits until the Movement System has processed the request");

		archive(m_waitUntilIdleAnimation, "waitUntilIdleAnimation", "+Wait until idle animation");
		archive.doc("When enabled, waits until the Motion_Idle animation fragment started running in Mannquin");

		BaseClass::Serialize(archive);
	}
#endif
protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		MovementRequest movementRequest;
		movementRequest.entityID = context.entityId;
		movementRequest.type = MovementRequest::Stop;

		if (m_waitUntilStopped)
		{
			movementRequest.callback = functor(runtimeData, &RuntimeData::MovementRequestCallback);
			runtimeData.movementRequestID = gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);
		}
		else
		{
			gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);
		}

		runtimeData.hasStopped = false;

		if (m_waitUntilIdleAnimation &&
		    ((runtimeData.motionIdleScopeID == SCOPE_ID_INVALID) || (runtimeData.motionIdleFragmentID == FRAGMENT_ID_INVALID)))
		{
			m_waitUntilIdleAnimation = LinkWithMannequin(context.entityId, runtimeData);
		}
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.CancelMovementRequest();
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		bool waitingForIdleAni = false;
		if (m_waitUntilIdleAnimation)
		{
			waitingForIdleAni = !IsIdleFragmentRunning(context, runtimeData);
		}

		bool waitingForStopped = false;
		if (m_waitUntilStopped)
		{
			waitingForStopped = !runtimeData.hasStopped;
		}

		if (waitingForStopped || waitingForIdleAni)
		{
			return Running;
		}

		return Success;
	}

private:
	IActionController* GetActionController(const EntityId entityID) const
	{
#if !defined(SYS_ENV_AS_STRUCT)
		assert(gEnv != NULL);
#endif
		assert(gEnv->pGameFramework != NULL);

		IGameFramework* gameFramework = gEnv->pGameFramework;
		IF_UNLIKELY (gameFramework == NULL)
		{
			return NULL;
		}

		IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID);
		if (entity == NULL)
		{
			assert(false);
			return NULL;
		}

		IMannequin& mannequinInterface = gameFramework->GetMannequinInterface();
		return mannequinInterface.FindActionController(*entity);
	}

	bool IsIdleFragmentRunning(const UpdateContext& context, RuntimeData& runtimeData) const
	{
		IF_UNLIKELY (runtimeData.motionIdleScopeID == SCOPE_ID_INVALID)
		{
			return false;
		}

		IActionController* actionController = GetActionController(context.entityId);
		IF_UNLIKELY (actionController == NULL)
		{
			assert(false);
			return false;
		}

		const IScope* scope = actionController->GetScope(runtimeData.motionIdleScopeID);
		assert(scope != NULL);
		return (scope->GetLastFragmentID() == runtimeData.motionIdleFragmentID);
	}

	bool LinkWithMannequin(const EntityId entityID, RuntimeData& runtimeData)
	{
		IActionController* actionController = GetActionController(entityID);
		IF_UNLIKELY (actionController == NULL)
		{
			assert(false);
			return false;
		}

		const char MOTION_IDLE_SCOPE_NAME[] = "FullBody3P";
		runtimeData.motionIdleScopeID = actionController->GetScopeID(MOTION_IDLE_SCOPE_NAME);
		if (runtimeData.motionIdleScopeID == SCOPE_ID_INVALID)
		{
			IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID);
			gEnv->pLog->LogError("StopMovement behavior tree node: Unable to find scope '%s for entity '%s'.",
			                     MOTION_IDLE_SCOPE_NAME, (entity != NULL) ? entity->GetName() : "<INVALID>");
			return false;
		}

		const char MOTION_IDLE_FRAGMENT_ID_NAME[] = "Motion_Idle";
		SAnimationContext& animContext = actionController->GetContext();
		runtimeData.motionIdleFragmentID = animContext.controllerDef.m_fragmentIDs.Find(MOTION_IDLE_FRAGMENT_ID_NAME);
		if (runtimeData.motionIdleFragmentID == FRAGMENT_ID_INVALID)
		{
			IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID);
			gEnv->pLog->LogError("StopMovement behavior tree node: Unable to find fragment ID '%s' for entity '%s'.",
			                     MOTION_IDLE_FRAGMENT_ID_NAME, (entity != NULL) ? entity->GetName() : "<INVALID>");
			return false;
		}

		return true;
	}

	bool m_waitUntilStopped;
	bool m_waitUntilIdleAnimation;
};

//////////////////////////////////////////////////////////////////////////

// Teleport the character when the destination point and the source
// point are both outside of the camera view.
class TeleportStalker_Deprecated : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}

		IAIObject* referencePoint = pPipeUser->GetRefPoint();
		IEntity* entity = pPipeUser->GetEntity();

		IF_UNLIKELY (referencePoint == NULL || entity == NULL)
		{
			gEnv->pLog->LogError("Modular behavior tree node 'Teleport' tried to teleport to the reference point but either the reference point or the entity was NULL.");

			// Success and not failure because it's not the kind of failure
			// we want to handle in the modular behavior tree.
			// We just effectively teleported to our current position.
			return Success;
		}

		const Vec3 currentCharacterPosition = entity->GetPos();
		const Vec3 teleportDestination = referencePoint->GetPos();

		// Stalker specific part: make generic or move to game-side
		if (!InCameraView(currentCharacterPosition))
		{
			// Characters spawned at an angle (in something occluded) and close
			// enough to the camera will not occluded even if they are in view.
			const Vec3 destinationToCameraDelta = (GetISystem()->GetViewCamera().GetPosition() - teleportDestination);
			const Vec3 destinationToCameraDirection = destinationToCameraDelta.GetNormalized();
			const Vec3 destinationToCameraDirectionAlongGroundPlane = Vec3(destinationToCameraDirection.x, destinationToCameraDirection.y, 0.0f).GetNormalized();
			const float destinationToCameraDistanceSq = destinationToCameraDelta.GetLengthSquared2D();
			const float occludeAngleThresholdCos = cosf(DEG2RAD(35.0f));
			const float occludeDistanceThresholdSq = sqr(13.0f);
			const bool occludedByGrass =
			  (destinationToCameraDistanceSq > occludeDistanceThresholdSq) &&
			  (destinationToCameraDirection.Dot(destinationToCameraDirectionAlongGroundPlane) > occludeAngleThresholdCos);
			if (occludedByGrass || (!InCameraView(teleportDestination)))
			{
				// Boom! Teleport!
				Matrix34 transform = entity->GetWorldTM();
				transform.SetTranslation(teleportDestination);
				entity->SetWorldTM(transform);
				// TODO: Rotate the entity towards the attention target for extra pleasure ;)
				return Success;
			}
		}

		return Running;
	}

private:
	bool InCameraView(const Vec3& footPosition) const
	{
		const float radius = 0.5f;
		const float height = 1.8f;

		const CCamera& cam = GetISystem()->GetViewCamera();
		AABB aabb(AABB::RESET);
		aabb.Add(footPosition + Vec3(0, 0, radius), radius);
		aabb.Add(footPosition + Vec3(0, 0, height - radius), radius);

		return cam.IsAABBVisible_F(aabb);
	}
};

//////////////////////////////////////////////////////////////////////////

class SmartObjectStatesWrapper : public Decorator
{
	typedef Decorator BaseClass;

public:
	struct RuntimeData
	{
		SmartObjectStatesWrapper* node;
		EntityId                  entityId;

		RuntimeData() : node(NULL), entityId(0)
		{
		}

		~RuntimeData()
		{
			if (this->node && this->entityId)
			{
				this->node->TriggerExitStates(*this);
			}
		}
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		m_enterStates = xml->getAttr("onEnter");
		m_exitStates = xml->getAttr("onExit");

		return LoadChildFromXml(xml, context, strictMode);
	}

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText.Format("onEnter(%s) - onExit(%s)", m_enterStates.c_str(), m_exitStates.c_str());
	}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		BaseClass::OnInitialize(context);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.entityId = context.entityId;
		TriggerEnterStates(runtimeData);
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		BaseClass::OnTerminate(context);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		TriggerExitStates(runtimeData);
	}

	void TriggerEnterStates(RuntimeData& runtimeData)
	{
		ModifySmartObjectStates(m_enterStates, runtimeData.entityId);
	}

	void TriggerExitStates(RuntimeData& runtimeData)
	{
		ModifySmartObjectStates(m_exitStates, runtimeData.entityId);
		runtimeData.entityId = 0;
	}

private:
	void ModifySmartObjectStates(const string& statesToModify, EntityId entityId)
	{
		if (gAIEnv.pSmartObjectManager)
		{
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId))
			{
				gAIEnv.pSmartObjectManager->ModifySmartObjectStates(pEntity, statesToModify.c_str());
			}
		}
	}

	string m_enterStates;
	string m_exitStates;
};

//////////////////////////////////////////////////////////////////////////

// Checks if the agent's attention target can be reached.
// Succeeds if it can. Fails if it can't.
class CheckIfTargetCanBeReached : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		MNM::QueuedPathID queuedPathID;
		Status            pendingStatus;

		RuntimeData()
			: queuedPathID(0)
			, pendingStatus(Invalid)
		{
		}

		~RuntimeData()
		{
			ReleasePathfinderRequest();
		}

		void PathfinderCallback(const MNM::QueuedPathID& requestID, MNMPathRequestResult& result)
		{
			assert(requestID == this->queuedPathID);
			this->queuedPathID = 0;

			if (result.HasPathBeenFound())
				this->pendingStatus = Success;
			else
				this->pendingStatus = Failure;
		}

		void ReleasePathfinderRequest()
		{
			if (this->queuedPathID)
			{
				gAIEnv.pMNMPathfinder->CancelPathRequest(this->queuedPathID);
				this->queuedPathID = 0;
			}
		}
	};

	CheckIfTargetCanBeReached()
		: m_mode(UseAttentionTarget)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		const LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		const stack_string modeStr = xml->getAttr("mode");
		if (modeStr == "UseLiveTarget")
		{
			m_mode = UseLiveTarget;
		}
		else
		{
			m_mode = UseAttentionTarget;
		}

		return result;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.pendingStatus = Failure;

		Vec3 targetPosition(ZERO);
		if (GetTargetPosition(*pPipeUser, OUT targetPosition))
		{
			if (gEnv->pAISystem->GetNavigationSystem()->IsLocationValidInNavigationMesh(pPipeUser->GetNavigationTypeID(), targetPosition, nullptr, 5.0f, 0.5f))
			{
				const MNMPathRequest request(pPipeUser->GetEntity()->GetWorldPos(),
				                             targetPosition, ZERO, -1, 0.0f, 0.0f, true,
				                             functor(runtimeData, &RuntimeData::PathfinderCallback),
				                             pPipeUser->GetNavigationTypeID(), eMNMDangers_None);

				runtimeData.queuedPathID = gAIEnv.pMNMPathfinder->RequestPathTo(pPipeUser->GetEntityID(), request);

				if (runtimeData.queuedPathID)
					runtimeData.pendingStatus = Running;
			}
		}
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.ReleasePathfinderRequest();
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		return runtimeData.pendingStatus;
	}

private:
	bool GetTargetPosition(CPipeUser& pipeUser, OUT Vec3& targetPosition) const
	{
		if (const IAIObject* target = pipeUser.GetAttentionTarget())
		{
			if (m_mode == UseLiveTarget)
			{
				if (const IAIObject* liveTarget = CPipeUser::GetLiveTarget(static_cast<const CAIObject*>(target)))
				{
					target = liveTarget;
				}
				else
				{
					return false;
				}
			}

			targetPosition = ((CAIObject*)target)->GetPhysicsPos();
			return true;
		}
		else
		{
			AIQueueBubbleMessage("CheckIfTargetCanBeReached Target Position",
			                     pipeUser.GetEntityID(),
			                     "CheckIfTargetCanBeReached: Agent has no target.",
			                     eBNS_LogWarning | eBNS_Balloon);
			return false;
		}
	}

	enum Mode
	{
		UseAttentionTarget,
		UseLiveTarget,
	};

	Mode m_mode;
};

//////////////////////////////////////////////////////////////////////////

class AnimationTagWrapper : public Decorator
{
	typedef Decorator BaseClass;

public:
	struct RuntimeData
	{
	};

	AnimationTagWrapper()
		: m_nameCRC(0)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = LoadSuccess;

		const char* tagName = xml->getAttr("name");
		if (!tagName)
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("AnimationTagWrapper", "name").c_str());
			result = LoadFailure;
		}

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
		m_tagName = tagName;
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

		m_nameCRC = CCrc32::ComputeLowercase(tagName);

		const LoadResult childResult = LoadChildFromXml(xml, context, strictMode);

		if (result == LoadSuccess && childResult == LoadSuccess)
		{
			return LoadSuccess;
		}
		else
		{
			return LoadFailure;
		}
	}

#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	virtual void GetCustomDebugText(const UpdateContext& context, stack_string& debugText) const override
	{
		debugText = m_tagName;
	}
#endif // DEBUG_MODULAR_BEHAVIOR_TREE

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		BaseClass::OnInitialize(context);

		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		SOBJECTSTATE& state = pPipeUser->GetState();
		state.mannequinRequest.CreateSetTagCommand(m_nameCRC);
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		BaseClass::OnTerminate(context);

		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		SOBJECTSTATE& state = pPipeUser->GetState();
		state.mannequinRequest.CreateClearTagCommand(m_nameCRC);
	}

private:
#ifdef DEBUG_MODULAR_BEHAVIOR_TREE
	string m_tagName;
#endif // DEBUG_MODULAR_BEHAVIOR_TREE
	unsigned int m_nameCRC;
};

//////////////////////////////////////////////////////////////////////////

// Sets the agent to shoot at the target from cover and adjusts his stance accordingly.
// Fails if the agent is not in cover, if there's no shoot posture or if the aim obstructed timeout elapses.
class ShootFromCover : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		CTimeValue                     endTime;
		CTimeValue                     nextPostureQueryTime;
		CTimeValue                     timeObstructed;
		PostureManager::PostureID      bestPostureID;
		PostureManager::PostureQueryID postureQueryID;

		RuntimeData()
			: endTime(0.0f)
			, nextPostureQueryTime(0.0f)
			, timeObstructed(0.0f)
			, bestPostureID(-1)
			, postureQueryID(0)
		{
		}
	};

	ShootFromCover()
		: m_duration(0.0f)
		, m_fireMode(FIREMODE_BURST)
		, m_aimObstructedTimeout(-1.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		IF_UNLIKELY (!xml->getAttr("duration", m_duration))
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageMissingOrEmptyAttribute("ShootFromCover", "duration").c_str());
			result = LoadFailure;
		}

		if (xml->haveAttr("fireMode"))
		{
			const AgentDictionary agentDictionary;

			IF_UNLIKELY (!agentDictionary.fireModeXml.Get(xml, "fireMode", m_fireMode))
			{
				ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("ShootFromCover", "fireMode", "", "Unknown fire mode").c_str());
				result = LoadFailure;
			}
		}

		xml->getAttr("aimObstructedTimeout", m_aimObstructedTimeout);

		if (m_duration < 0)
		{
			ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("ShootFromCover", "duration", ToString(m_duration), "Value must be greater or equal than 0").c_str());
		}

		if (m_aimObstructedTimeout < 0 && m_aimObstructedTimeout != -1)
		{
			ErrorReporter(*this, context).LogWarning("%s", ErrorReporter::ErrorMessageInvalidAttribute("ShootFromCover", "aimObstructedTimeout", ToString(m_aimObstructedTimeout), "Value must be greater or equal than 0").c_str());
		}

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("ShootFromCover");
		xml->setAttr("duration", m_duration);
		xml->setAttr("fireMode", Serialization::getEnumLabel<EFireMode>(m_fireMode));
		xml->setAttr("aimObstructedTimeout", m_aimObstructedTimeout);

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		const AgentDictionary agentDictionary;

		
		SerializationUtils::SerializeEnumList<EFireMode>(archive, "fireMode", "+Fire mode", agentDictionary.fireModesSerialization, agentDictionary.fireModeXml, m_fireMode);
		archive.doc("Fire mode used to shoot");

		archive(m_duration, "duration", "+Duration");
		archive.doc("Time in seconds before the node exists");

		archive(m_aimObstructedTimeout, "aimObstructedTimeout", "+Aim obstructed timeout");
		archive.doc("Maximum time in seconds that the character will be allowed to have its aim obstructed before exiting yielding a Failure");


		if (m_duration < 0)
		{
			archive.warning(m_duration, SerializationUtils::Messages::ErrorInvalidValueWithReason("Duration", ToString(m_duration), "Value must be greater or equal than 0"));
		}

		if (m_aimObstructedTimeout < 0 && m_aimObstructedTimeout != -1)
		{
			archive.warning(m_aimObstructedTimeout, SerializationUtils::Messages::ErrorInvalidValueWithReason("Aim Obstructed Timeout", ToString(m_aimObstructedTimeout), "Value must be greater or equal than 0"));
		}

		BaseClass::Serialize(archive);
	}
#endif
protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		BaseClass::OnInitialize(context);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.endTime = context.frameStartTime + CTimeValue(m_duration);
		runtimeData.nextPostureQueryTime = context.frameStartTime;

		if (CPipeUser* pipeUser = GetPipeUser(*this, context))
		{
			pipeUser->SetAdjustingAim(true);
			pipeUser->SetFireMode(m_fireMode);
		}

		runtimeData.timeObstructed = 0.0f;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		if (CPipeUser* pipeUser = GetPipeUser(*this, context))
		{
			pipeUser->SetFireMode(FIREMODE_OFF);
			pipeUser->SetAdjustingAim(false);

			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

			if (runtimeData.postureQueryID)
			{
				pipeUser->CastToCPuppet()->GetPostureManager().CancelPostureQuery(runtimeData.postureQueryID);
				runtimeData.postureQueryID = 0;
			}
		}

		BaseClass::OnTerminate(context);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (context.frameStartTime > runtimeData.endTime)
			return Success;

		if (!pPipeUser->IsInCover())
			return Failure;

		if (m_aimObstructedTimeout >= 0.0f)
		{
			const bool isAimObstructed = pPipeUser && (pPipeUser->GetState().aimObstructed || pPipeUser->GetAimState() == AI_AIM_OBSTRUCTED);
			if (isAimObstructed)
			{
				runtimeData.timeObstructed += context.frameDeltaTime;
				if (runtimeData.timeObstructed >= m_aimObstructedTimeout)
				{
					return Failure;
				}
			}
			else
			{
				runtimeData.timeObstructed = 0.0f;
			}
		}

		if (context.frameStartTime < runtimeData.nextPostureQueryTime)
			return Running;

		IF_UNLIKELY (!pPipeUser)
		{
			ErrorReporter(*this, context).LogError("Agent must be a pipe user but isn't.");
			return Failure;
		}

		CPuppet* puppet = pPipeUser->CastToCPuppet();
		IF_UNLIKELY (!puppet)
		{
			ErrorReporter(*this, context).LogError("Agent must be a puppet but isn't.");
			return Failure;
		}

		const IAIObject* target = pPipeUser->GetAttentionTarget();
		IF_UNLIKELY (!target)
			return Failure;

		const Vec3 targetPosition = target->GetPos();

		const CoverID& coverID = pPipeUser->GetCoverID();
		IF_UNLIKELY (coverID == 0)
			return Failure;

		Vec3 coverNormal(1, 0, 0);
		const Vec3 coverPosition = gAIEnv.pCoverSystem->GetCoverLocation(coverID, pPipeUser->GetParameters().distanceToCover, NULL, &coverNormal);

		if (runtimeData.postureQueryID == 0)
		{
			// Query posture
			PostureManager::PostureQuery query;
			query.actor = pPipeUser->CastToCAIActor();
			query.allowLean = true;
			query.allowProne = false;
			query.checks = PostureManager::DefaultChecks;
			query.coverID = coverID;
			query.distancePercent = 0.2f;
			query.hintPostureID = runtimeData.bestPostureID;
			query.position = coverPosition;
			query.target = targetPosition;
			query.type = PostureManager::AimPosture;

			PostureManager& postureManager = puppet->GetPostureManager();
			runtimeData.postureQueryID = postureManager.QueryPosture(query);
		}
		else
		{
			// Check status
			PostureManager& postureManager = puppet->GetPostureManager();
			PostureManager::PostureInfo* postureInfo;
			AsyncState status = postureManager.GetPostureQueryResult(runtimeData.postureQueryID, &runtimeData.bestPostureID, &postureInfo);
			if (status != AsyncInProgress)
			{
				runtimeData.postureQueryID = 0;

				const bool foundPosture = (status != AsyncFailed);

				if (foundPosture)
				{
					runtimeData.nextPostureQueryTime = context.frameStartTime + CTimeValue(2.0f);

					pPipeUser->m_State.bodystate = postureInfo->stance;
					pPipeUser->m_State.lean = postureInfo->lean;
					pPipeUser->m_State.peekOver = postureInfo->peekOver;

					const char* const coverActionName = postureInfo->agInput.c_str();
					pPipeUser->m_State.coverRequest.SetCoverAction(coverActionName, postureInfo->lean);
				}
				else
				{
					runtimeData.nextPostureQueryTime = context.frameStartTime + CTimeValue(1.0f);
				}
			}
		}

		const bool hasCoverPosture = (runtimeData.bestPostureID != -1);
		if (hasCoverPosture)
		{
			const Vec3 targetDirection = targetPosition - pPipeUser->GetPos();
			pPipeUser->m_State.coverRequest.SetCoverBodyDirectionWithThreshold(coverNormal, targetDirection, DEG2RAD(30.0f));
		}

		return Running;
	}

private:
	float     m_duration;
	EFireMode m_fireMode;
	float     m_aimObstructedTimeout;
};

//////////////////////////////////////////////////////////////////////////

struct ShootDictionary
{
	ShootDictionary();

	CXMLAttrReader<EStance>   shootStanceXml;
	Serialization::StringList shootStanceSerialization;
};

ShootDictionary::ShootDictionary()
{

	shootStanceXml.Reserve(4);
	shootStanceXml.Add("Stand", STANCE_STAND);
	shootStanceXml.Add("Crouch", STANCE_CROUCH);
	shootStanceXml.Add("Relaxed", STANCE_RELAXED);
	shootStanceXml.Add("Alerted", STANCE_ALERTED);

	shootStanceSerialization.reserve(3);
	shootStanceSerialization.push_back("Stand");
	shootStanceSerialization.push_back("Crouch");
	shootStanceSerialization.push_back("Relaxed");
	shootStanceSerialization.push_back("Alerted");
}

ShootDictionary g_shootDictionary;

class Shoot : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		CStrongRef<CAIObject> dummyTarget;
		CTimeValue            timeWhenShootingShouldEnd;
		CTimeValue            timeObstructed;

		RuntimeData()
			: timeWhenShootingShouldEnd(0.0f)
			, timeObstructed(0.0f)
		{
		}
	};

	Shoot()
		: m_duration(0.0f)
		, m_shootAt(ShootOp::ShootAt::AttentionTarget)
		, m_stance(STANCE_STAND)
		, m_fireMode(FIREMODE_OFF)
		, m_stanceToUseIfSlopeIsTooSteep(STANCE_STAND)
		, m_allowedSlopeNormalDeviationFromUpInRadians(0.0f)
		, m_aimObstructedTimeout(-1.0f)
		, m_position(ZERO)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		IF_UNLIKELY (!xml->getAttr("duration", m_duration) || m_duration < 0.0f)
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("Shoot", "duration", ToString(m_duration), "Missing or invalid value. Duration cannot be empty and should be greater or equal to 0").c_str());
			result = LoadFailure;
		}

		const ShootOpDictionary shootOpDictionary;

		IF_UNLIKELY (!shootOpDictionary.shootAtXml.Get(xml, "at", m_shootAt))
		{
			const string shootAtTarget = xml->getAttr("at");
			if (shootAtTarget == "Target")
			{
				m_shootAt = ShootOp::AttentionTarget;
			}
			else 
			{
				ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("Shoot", "at", "", "Missing or invalid value").c_str());
				result = LoadFailure;
			}
		}

		const AgentDictionary agentDictionary;

		IF_UNLIKELY (!agentDictionary.fireModeXml.Get(xml, "fireMode", m_fireMode))
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("Shoot", "fireMode", "", "Missing or invalid value").c_str());
			result = LoadFailure;
		}

		IF_UNLIKELY (!s_stanceDictionary.stancesXml.Get(xml, "stance", m_stance))
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("Shoot", "stance", "", "Missing or invalid value").c_str());
			result = LoadFailure;
		}

		if (m_shootAt == ShootOp::ShootAt::LocalSpacePosition)
		{
			IF_UNLIKELY (!xml->getAttr("position", m_position))
			{
				ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("Shoot", "position", "", "Missing or invalid value").c_str());
				result = LoadFailure;
			}
		}

		float degrees = 90.0f;
		xml->getAttr("allowedSlopeNormalDeviationFromUpInDegrees", degrees);
		m_allowedSlopeNormalDeviationFromUpInRadians = DEG2RAD(degrees);

		s_stanceDictionary.stancesXml.Get(xml, "stanceToUseIfSlopeIsTooSteep", m_stanceToUseIfSlopeIsTooSteep);
		xml->getAttr("aimObstructedTimeout", m_aimObstructedTimeout);

		return result;
	}

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("Shoot");
		xml->setAttr("duration", m_duration);
		xml->setAttr("at", Serialization::getEnumLabel<ShootOp::ShootAt>(m_shootAt));
		xml->setAttr("fireMode", Serialization::getEnumLabel<EFireMode>(m_fireMode));
		xml->setAttr("stance", Serialization::getEnumLabel<EStance>(m_stance));
		xml->setAttr("stanceToUseIfSlopeIsTooSteep", Serialization::getEnumLabel<EStance>(m_stanceToUseIfSlopeIsTooSteep));
		xml->setAttr("allowedSlopeNormalDeviationFromUpInDegrees", RAD2DEG(m_allowedSlopeNormalDeviationFromUpInRadians));
		xml->setAttr("aimObstructedTimeout", m_aimObstructedTimeout);

		if (m_shootAt == ShootOp::ShootAt::LocalSpacePosition)
		{
			xml->setAttr("position", m_position);
		}

		return xml;
	}
#endif

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	virtual void Serialize(Serialization::IArchive& archive) override
	{
		const ShootOpDictionary shootOpDictionary;
		const AgentDictionary agentDictionary;

		SerializationUtils::SerializeEnumList<ShootOp::ShootAt>(archive, "at", "+At", shootOpDictionary.shootAtSerialization, shootOpDictionary.shootAtXml, m_shootAt);
		archive.doc("Where to shoot at");
		
		SerializationUtils::SerializeEnumList<EFireMode>(archive, "fireMode", "+Fire mode", agentDictionary.fireModesSerialization, agentDictionary.fireModeXml, m_fireMode);
		archive.doc("Fire mode use to shoot");

		SerializationUtils::SerializeEnumList<EStance>(archive, "stance", "+Stance", g_shootDictionary.shootStanceSerialization, g_shootDictionary.shootStanceXml, m_stance);
		archive.doc("Stance to use by default");

		SerializationUtils::SerializeEnumList<EStance>(archive, "stanceToUseIfSlopeIsTooSteep", "+Stance to use if slope is too steep", g_shootDictionary.shootStanceSerialization, g_shootDictionary.shootStanceXml, m_stanceToUseIfSlopeIsTooSteep);
		archive.doc("Stance to use if terrain slope is too steep");

		float m_allowedSlopeNormalDeviationFromUpInDeg = RAD2DEG(m_allowedSlopeNormalDeviationFromUpInRadians);
		
		archive(m_allowedSlopeNormalDeviationFromUpInDeg, "allowedSlopeNormalDeviationFromUpInDegrees", "+Allowed slope normal deviation from up in degrees");
		archive.doc("If the real slope deviation is less than the allowed slope normal deviation, the character will use the default stance. Otherwise, it will use the stance set up for steep slopes");

		m_allowedSlopeNormalDeviationFromUpInRadians = DEG2RAD(m_allowedSlopeNormalDeviationFromUpInDeg);

		archive(m_duration, "duration", "+Duration");
		archive.doc("Time in seconds before the node exists yielding Success.");

		if (m_duration == 0.0f)
		{
			archive.warning(m_duration, SerializationUtils::Messages::ErrorInvalidValueWithReason("Duration", ToString(m_duration), "Shooting duration is set to 0"));
		}

		archive(m_aimObstructedTimeout, "aimObstructedTimeout", "+Aim obstructed timeout");
		archive.doc("Maximum time in seconds that the character will be allowed to have its aim obstructed before exiting yielding a Failure");

		if (m_shootAt == ShootOp::ShootAt::LocalSpacePosition)
		{
			archive(m_position, "position", "+Position");
			archive.doc("World position to shoot at");
		}

		BaseClass::Serialize(archive);
	}
#endif
protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		BaseClass::OnInitialize(context);

		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (m_shootAt == ShootOp::ShootAt::LocalSpacePosition)
		{
			SetupFireTargetForLocalPosition(*pPipeUser, runtimeData);
		}
		else if (m_shootAt == ShootOp::ShootAt::ReferencePoint)
		{
			SetupFireTargetForReferencePoint(*pPipeUser, runtimeData);
		}

		pPipeUser->SetFireMode(m_fireMode);

		if (m_stance != STANCE_NULL)
		{
			EStance slopeVerifiedStance = m_stance;

			if (IEntity* entity = gEnv->pEntitySystem->GetEntity(context.entityId))
			{
				if (IPhysicalEntity* physicalEntity = entity->GetPhysics())
				{
					const Vec3& up = Vec3Constants<float>::fVec3_OneZ;

					Vec3 groundNormal = up;

					if (physicalEntity && physicalEntity->GetType() == PE_LIVING)
					{
						pe_status_living status;
						if (physicalEntity->GetStatus(&status) > 0)
						{
							if (!status.groundSlope.IsZero() && status.groundSlope.IsValid())
							{
								groundNormal = status.groundSlope;
								groundNormal.NormalizeSafe(up);
							}
						}
					}

					if (acosf(clamp_tpl(groundNormal.Dot(up), -1.0f, +1.0f)) > m_allowedSlopeNormalDeviationFromUpInRadians)
					{
						slopeVerifiedStance = m_stanceToUseIfSlopeIsTooSteep;
					}
				}
			}

			pPipeUser->m_State.bodystate = slopeVerifiedStance;
		}

		MovementRequest movementRequest;
		movementRequest.entityID = pPipeUser->GetEntityID();
		movementRequest.type = MovementRequest::Stop;
		gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);

		runtimeData.timeWhenShootingShouldEnd = context.frameStartTime + CTimeValue(m_duration);
		runtimeData.timeObstructed = 0.0f;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.dummyTarget)
		{
			pPipeUser->SetFireTarget(NILREF);
			runtimeData.dummyTarget.Release();
		}

		pPipeUser->SetFireMode(FIREMODE_OFF);

		BaseClass::OnTerminate(context);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return Failure;
		}
		
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (context.frameStartTime >= runtimeData.timeWhenShootingShouldEnd)
			return Success;

		if (m_aimObstructedTimeout >= 0.0f)
		{
			const bool isAimObstructed = pPipeUser && (pPipeUser->GetState().aimObstructed || pPipeUser->GetAimState() == AI_AIM_OBSTRUCTED);
			if (isAimObstructed)
			{
				runtimeData.timeObstructed += context.frameDeltaTime;
				if (runtimeData.timeObstructed >= m_aimObstructedTimeout)
				{
					return Failure;
				}
			}
			else
			{
				runtimeData.timeObstructed = 0.0f;
			}
		}

		return Running;
	}

private:
	void SetupFireTargetForLocalPosition(CPipeUser& pipeUser, RuntimeData& runtimeData)
	{
		// The user of this op has specified that the character
		// should fire at a predefined position in local-space.
		// Transform that position into world-space and store it
		// in an AI object. The pipe user will shoot at this.

		gAIEnv.pAIObjectManager->CreateDummyObject(runtimeData.dummyTarget);

		const Vec3 fireTarget = pipeUser.GetEntity()->GetWorldTM().TransformPoint(m_position);

		runtimeData.dummyTarget->SetPos(fireTarget);

		pipeUser.SetFireTarget(runtimeData.dummyTarget);
	}

	void SetupFireTargetForReferencePoint(CPipeUser& pipeUser, RuntimeData& runtimeData)
	{
		gAIEnv.pAIObjectManager->CreateDummyObject(runtimeData.dummyTarget);

		if (IAIObject* referencePoint = pipeUser.GetRefPoint())
		{
			runtimeData.dummyTarget->SetPos(referencePoint->GetPos());
		}
		else
		{
			gEnv->pLog->LogWarning("Agent '%s' is trying to shoot at the reference point but it hasn't been set.", pipeUser.GetName());
			runtimeData.dummyTarget->SetPos(ZERO);
		}

		pipeUser.SetFireTarget(runtimeData.dummyTarget);
	}

	Vec3             m_position;
	float            m_duration;
	ShootOp::ShootAt m_shootAt;
	EFireMode        m_fireMode;
	EStance          m_stance;
	EStance          m_stanceToUseIfSlopeIsTooSteep;
	float            m_allowedSlopeNormalDeviationFromUpInRadians;
	float            m_aimObstructedTimeout;
};

//////////////////////////////////////////////////////////////////////////

// Attempts to throw a grenade.
// Success if a grenade is thrown.
// Failure if a grenade is not thrown.
class ThrowGrenade : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		CTimeValue timeWhenWeShouldGiveUpThrowing;
		bool       grenadeHasBeenThrown;

		RuntimeData()
			: timeWhenWeShouldGiveUpThrowing(0.0f)
			, grenadeHasBeenThrown(false)
		{
		}
	};

	ThrowGrenade()
		: m_fragGrenadeThrownEvent("WPN_SHOOT")
		, m_timeout(0.0f)
		, m_grenadeType(eRGT_EMP_GRENADE)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const struct LoadContext& context, const bool strictMode) override
	{
		LoadResult result = BaseClass::LoadFromXml(xml, context, strictMode);

		IF_UNLIKELY (!xml->getAttr("timeout", m_timeout) || m_timeout < 0.0f)
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("ThrowGrenade", "timeout", ToString(m_timeout), "Missing or invalid value. Timeout cannot be empty and should be greater or equal to 0").c_str());
			result = LoadFailure;
		}

		const stack_string grenadeTypeName = xml->getAttr("type");
		if (grenadeTypeName == "emp")
		{
			m_grenadeType = eRGT_EMP_GRENADE;
		}
		else if (grenadeTypeName == "frag")
		{
			m_grenadeType = eRGT_FRAG_GRENADE;
		}
		else if (grenadeTypeName == "smoke")
		{
			m_grenadeType = eRGT_SMOKE_GRENADE;
		}
		else
		{
			ErrorReporter(*this, context).LogError("%s", ErrorReporter::ErrorMessageInvalidAttribute("ThrowGrenade", "type", ToString(m_timeout), "Missing or invalid value").c_str());
			result = LoadFailure;
		}

		return result;
	}

	virtual void HandleEvent(const EventContext& context, const Event& event) override
	{
		if (m_fragGrenadeThrownEvent == event)
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			runtimeData.grenadeHasBeenThrown = true;
		}
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		BaseClass::OnInitialize(context);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.grenadeHasBeenThrown = false;
		runtimeData.timeWhenWeShouldGiveUpThrowing = context.frameStartTime + CTimeValue(m_timeout);

		if (CPuppet* puppet = GetPuppet(*this, context))
		{
			puppet->SetFireMode(FIREMODE_SECONDARY);
			puppet->RequestThrowGrenade(m_grenadeType, AI_REG_ATTENTIONTARGET);
		}
		else
		{
			ErrorReporter(*this, context).LogError("Agent wasn't a puppet.");
		}
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CPipeUser* pPipeUser = GetPipeUser(*this, context);
		IF_UNLIKELY(!pPipeUser)
		{
			return;
		}
		
		pPipeUser->SetFireMode(FIREMODE_OFF);

		BaseClass::OnTerminate(context);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.grenadeHasBeenThrown)
		{
			return Success;
		}
		else if (context.frameStartTime >= runtimeData.timeWhenWeShouldGiveUpThrowing)
		{
			return Failure;
		}

		return Running;
	}

private:
	const Event           m_fragGrenadeThrownEvent;
	float                 m_timeout;
	ERequestedGrenadeType m_grenadeType;
};

//////////////////////////////////////////////////////////////////////////

class PullDownThreatLevel : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
	};

#ifdef USING_BEHAVIOR_TREE_XML_DESCRIPTION_CREATION
	virtual XmlNodeRef CreateXmlDescription() override
	{
		XmlNodeRef xml = BaseClass::CreateXmlDescription();
		xml->setTag("PullDownThreatLevel");

		return xml;
	}
#endif

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		if (CPipeUser* pipeUser = GetPipeUser(*this, context))
		{
			gAIEnv.pTargetTrackManager->PullDownThreatLevel(pipeUser->GetAIObjectID(), AITHREAT_SUSPECT);
		}

		return Success;
	}
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SERIALIZATION_ENUM_BEGIN_NESTED(Move, DestinationType, "Destination Type")
SERIALIZATION_ENUM(Move::DestinationType::Target, "target", "Target")
SERIALIZATION_ENUM(Move::DestinationType::Cover, "cover", "Cover")
SERIALIZATION_ENUM(Move::DestinationType::ReferencePoint, "reference_point", "RefPoint")
SERIALIZATION_ENUM(Move::DestinationType::LastOp, "last_op", "LastOp")
SERIALIZATION_ENUM(Move::DestinationType::InitialPosition, "target", "InitialPosition")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(EAIRegister, "AI Register")
SERIALIZATION_ENUM(EAIRegister::AI_REG_REFPOINT, "ref_point", "RefPoint")
SERIALIZATION_ENUM(EAIRegister::AI_REG_COVER, "cover", "Cover")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Animate, PlayMode, "Play Mode")
SERIALIZATION_ENUM(Animate::PlayMode::PlayOnce, "play_once", "PlayOnce")
SERIALIZATION_ENUM(Animate::PlayMode::InfiniteLoop, "infinite_loop", "InfiniteLoop")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Look, At, "Look At")
SERIALIZATION_ENUM(Look::At::AttentionTarget, "attention_target", "AttentionTarget")
SERIALIZATION_ENUM(Look::At::ClosestGroupMember, "closest_group_member", "ClosestGroupMember")
SERIALIZATION_ENUM(Look::At::ReferencePoint, "reference_point", "ReferencePoint")
SERIALIZATION_ENUM_END()

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RegisterBehaviorTreeNodes_AI()
{
	assert(gAIEnv.pBehaviorTreeManager);

	IBehaviorTreeManager& manager = *gAIEnv.pBehaviorTreeManager;

	// Keep alphabetically sorted for better readability
	REGISTER_BEHAVIOR_TREE_NODE(manager, AimAroundWhileUsingAMachingGun);
	REGISTER_BEHAVIOR_TREE_NODE(manager, AnimationTagWrapper);
	REGISTER_BEHAVIOR_TREE_NODE(manager, CheckIfTargetCanBeReached);
	REGISTER_BEHAVIOR_TREE_NODE(manager, GoalPipe);
	REGISTER_BEHAVIOR_TREE_NODE(manager, LuaWrapper);
	REGISTER_BEHAVIOR_TREE_NODE(manager, SmartObjectStatesWrapper);
	REGISTER_BEHAVIOR_TREE_NODE(manager, ThrowGrenade);
	REGISTER_BEHAVIOR_TREE_NODE(manager, TurnBody);

#ifdef USING_BEHAVIOR_TREE_SERIALIZATION
	const char* COLOR_SDK = "ff00ff";
#else
	CRY_DISABLE_WARN_UNUSED_VARIABLES();
	const char* COLOR_SDK = nullptr;
	CRY_RESTORE_WARN_UNUSED_VARIABLES();
#endif

	// Keep alphabetically sorted for better readability
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, AdjustCoverStance, "GameSDK\\Adjust Cover Stance", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Aim, "GameSDK\\Aim", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Animate, "GameSDK\\Animate", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, AssertLua, "GameSDK\\Assert Lua", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Bubble, "GameSDK\\Bubble", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, ClearTargets, "GameSDK\\Clear Targets", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Communicate, "GameSDK\\Communicate", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, ExecuteLua, "GameSDK\\Execute Lua", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, GroupScope, "GameSDK\\Group Scope", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Look, "GameSDK\\Look", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, LuaBehavior, "GameSDK\\Lua Behavior (DEPRECATED)", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, LuaGate, "GameSDK\\Lua Gate", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Move, "GameSDK\\Move", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, PullDownThreatLevel, "GameSDK\\Pull Down Threat Level", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, QueryTPS, "GameSDK\\Query TPS", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, SendTransitionSignal, "GameSDK\\Send Transition Signal", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, SetAlertness, "GameSDK\\Set Alertness", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Shoot, "GameSDK\\Shoot", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, ShootFromCover, "GameSDK\\Shoot From Cover", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Signal, "GameSDK\\Send Signal", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, Stance, "GameSDK\\Stance", COLOR_SDK);
	REGISTER_BEHAVIOR_TREE_NODE_WITH_SERIALIZATION(manager, StopMovement, "GameSDK\\Stop Movement", COLOR_SDK);
}
}
