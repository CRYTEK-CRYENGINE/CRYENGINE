// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IMovementSystem.h>
#include <CryAISystem/MovementRequest.h>
#include <CryAISystem/MovementStyle.h>
#include "PipeUser.h"                                // Big one, but needed for GetProxy and SetBehavior
#include "TacticalPointSystem/TacticalPointSystem.h" // Big one, but needed for InitQueryContextFromActor
#include "TargetSelection/TargetTrackManager.h"
#include "BehaviorTree/BehaviorTreeNodes_Helicopter.h"
#include <CryString/CryName.h>
#include <CryGame/IGameFramework.h>
#include "BehaviorTreeManager.h"

#include <../CryAction/ICryMannequin.h>

namespace
{
struct FireModeDictionary
{
	FireModeDictionary();
	CXMLAttrReader<EFireMode> fireMode;
};

FireModeDictionary::FireModeDictionary()
{
	fireMode.Add("Off", FIREMODE_OFF);
	fireMode.Add("Burst", FIREMODE_BURST);
	fireMode.Add("Continuous", FIREMODE_CONTINUOUS);
	fireMode.Add("Forced", FIREMODE_FORCED);
	fireMode.Add("Aim", FIREMODE_AIM);
	fireMode.Add("Secondary", FIREMODE_SECONDARY);
	fireMode.Add("SecondarySmoke", FIREMODE_SECONDARY_SMOKE);
	fireMode.Add("Melee", FIREMODE_MELEE);
	fireMode.Add("Kill", FIREMODE_KILL);
	fireMode.Add("BurstWhileMoving", FIREMODE_BURST_WHILE_MOVING);
	fireMode.Add("PanicSpread", FIREMODE_PANIC_SPREAD);
	fireMode.Add("BurstDrawFire", FIREMODE_BURST_DRAWFIRE);
	fireMode.Add("MeleeForced", FIREMODE_MELEE_FORCED);
	fireMode.Add("BurstSnipe", FIREMODE_BURST_SNIPE);
	fireMode.Add("AimSweep", FIREMODE_AIM_SWEEP);
	fireMode.Add("BurstOnce", FIREMODE_BURST_ONCE);
}

FireModeDictionary g_fireModeDictionary;

CPipeUser* GetPipeUser(const BehaviorTree::UpdateContext& context)
{
	assert(context.entity.GetAI());
	return context.entity.GetAI()->CastToCPipeUser();
}

CPuppet* GetPuppet(const BehaviorTree::UpdateContext& context)
{
	assert(context.entity.GetAI());
	return context.entity.GetAI()->CastToCPuppet();
}

CAIActor* GetAIActor(const BehaviorTree::UpdateContext& context)
{
	assert(context.entity.GetAI());
	return context.entity.GetAI()->CastToCAIActor();
}

const CTimeValue& GetFrameStartTime(const BehaviorTree::UpdateContext& context)
{
	return gEnv->pTimer->GetFrameStartTime();
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

	virtual LoadResult LoadFromXml(const XmlNodeRef& goalPipeXml, const LoadContext& context) override
	{
		LoadName(goalPipeXml, context);

		CGoalPipeXMLReader reader;
		reader.ParseGoalPipe(m_goalPipeName, goalPipeXml, CPipeManager::SilentlyReplaceDuplicate);

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText = m_goalPipeName;
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.goalPipeId = gEnv->pAISystem->AllocGoalPipeId();

		IPipeUser& pipeUser = *GetPipeUser(context);
		pipeUser.SelectPipe(AIGOALPIPE_RUN_ONCE, m_goalPipeName, NULL, runtimeData.goalPipeId);
		pipeUser.RegisterGoalPipeListener(&runtimeData, runtimeData.goalPipeId, "GoalPipeBehaviorTreeNode");

		runtimeData.entityToRemoveListenerFrom = context.entityId;

		runtimeData.goalPipeIsRunning = true;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.UnregisterGoalPipeListener();
		IPipeUser& pipeUser = *GetPipeUser(context);
		pipeUser.SelectPipe(AIGOALPIPE_LOOP, "_first_");
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

	virtual LoadResult LoadFromXml(const XmlNodeRef& node, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(node, context) == LoadFailure)
			return LoadFailure;

		m_behaviorName = node->getAttr("name");
		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText += m_behaviorName;
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CPipeUser& pipeUser = *GetPipeUser(context);
		IAIActorProxy* proxy = pipeUser.GetProxy();

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (proxy)
		{
			proxy->SetBehaviour(m_behaviorName);
			runtimeData.behaviorIsRunning = true;

			pipeUser.RegisterBehaviorListener(&runtimeData);
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

		IAIActorProxy* proxy = GetPipeUser(context)->GetProxy();
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
public:
	struct RuntimeData
	{
	};

	Bubble()
		: m_duration(2.0)
		, m_balloonFlags(0)
	{
	}

	virtual Status Update(const UpdateContext& context)
	{
		AIQueueBubbleMessage("Behavior Bubble", context.entityId, m_message, m_balloonFlags, m_duration, SAIBubbleRequest::eRT_PrototypeDialog);
		return Success;
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context)
	{
		m_message = xml->getAttr("message");

		m_duration = 2.0f;
		xml->getAttr("duration", m_duration);

		bool balloon = true;
		bool log = true;

		xml->getAttr("balloon", balloon);
		xml->getAttr("log", log);

		m_balloonFlags = eBNS_None;

		if (balloon) m_balloonFlags |= eBNS_Balloon;
		if (log) m_balloonFlags |= eBNS_Log;

		return LoadSuccess;
	}

private:
	string m_message;
	float  m_duration;
	uint32 m_balloonFlags;
};

//////////////////////////////////////////////////////////////////////////

class Move : public Action
{
	typedef Action BaseClass;

private:
	enum DestinationType
	{
		Target,
		Cover,
		ReferencePoint,
		LastOp,
		InitialPosition,
	};

	struct Dictionaries
	{
		CXMLAttrReader<DestinationType> to;

		Dictionaries()
		{
			to.Reserve(4);
			to.Add("Target", Target);
			to.Add("Cover", Cover);
			to.Add("RefPoint", ReferencePoint);
			to.Add("LastOp", LastOp);
			to.Add("InitialPosition", InitialPosition);
		}
	};

	static Dictionaries s_dictionaries;

public:
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
				gEnv->pAISystem->GetMovementSystem()->CancelRequest(this->movementRequestID);
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

	Move()
		: m_stopWithinDistance(0.0f)
		, m_stopDistanceVariation(0.0f)
		, m_destination(Target)
		, m_fireMode(FIREMODE_OFF)
		, m_dangersFlags(eMNMDangers_None)
		, m_considerActorsAsPathObstacles(false)
		, m_lengthToTrimFromThePathEnd(0.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		// Speed? Stance?
		m_movementStyle.ReadFromXml(xml);

		// Destination? Target/Cover/ReferencePoint
		s_dictionaries.to.Get(xml, "to", m_destination, true);
		m_movementStyle.SetMovingToCover(m_destination == Cover);

		xml->getAttr("stopWithinDistance", m_stopWithinDistance);
		if (m_stopWithinDistance > 0.0f)
			xml->getAttr("stopDistanceVariation", m_stopDistanceVariation);

		g_fireModeDictionary.fireMode.Get(xml, "fireMode", m_fireMode);

		bool avoidDangers = true;
		xml->getAttr("avoidDangers", avoidDangers);

		SetupDangersFlagsForDestination(avoidDangers);

		bool avoidGroupMates = true;
		xml->getAttr("avoidGroupMates", avoidGroupMates);
		if (avoidGroupMates)
		{
			m_dangersFlags |= eMNMDangers_GroupMates;
		}

		xml->getAttr("considerActorsAsPathObstacles", m_considerActorsAsPathObstacles);
		xml->getAttr("lengthToTrimFromThePathEnd", m_lengthToTrimFromThePathEnd);

		return LoadSuccess;
	}

	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		CPipeUser* pipeUser = GetPipeUser(context);
		IF_UNLIKELY (!pipeUser)
		{
			ErrorReporter(*this, context).LogError("Expected pipe user");
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
		if (m_fireMode != FIREMODE_OFF)
		{
			GetPipeUser(context)->SetFireMode(FIREMODE_OFF);
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.ReleaseCurrentMovementRequest();
	}

#if defined(USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT) && defined(COMPILE_WITH_MOVEMENT_SYSTEM_DEBUG)
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const override
	{
		const RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(updateContext);
		MovementRequestStatus status;
		gEnv->pAISystem->GetMovementSystem()->GetRequestStatus(runtimeData.movementRequestID, status);
		ConstructHumanReadableText(status, debugText);
	}
#endif

private:

	float           m_stopWithinDistance;
	float           m_stopDistanceVariation;
	MovementStyle   m_movementStyle;
	DestinationType m_destination;
	EFireMode       m_fireMode;
	MNMDangersFlags m_dangersFlags;
	bool            m_considerActorsAsPathObstacles;
	float           m_lengthToTrimFromThePathEnd;

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.pendingStatus != Running)
			return runtimeData.pendingStatus;

		CPipeUser& pipeUser = *GetPipeUser(context);

		if (m_destination == Target || m_destination == LastOp || m_destination == ReferencePoint)
		{
			ChaseTarget(pipeUser, context.entityId, runtimeData);
		}

		const bool stopMovementWhenWithinCertainDistance = runtimeData.effectiveStopDistanceSq > 0.0f;

		if (stopMovementWhenWithinCertainDistance)
		{
			if (GetSquaredDistanceToDestination(pipeUser, runtimeData) < runtimeData.effectiveStopDistanceSq)
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
		CXMLAttrReader<EAIRegister> reg;

		Dictionaries()
		{
			reg.Reserve(2);
			//reg.Add("LastOp",     AI_REG_LASTOP);
			reg.Add("RefPoint", AI_REG_REFPOINT);
			//reg.Add("AttTarget",  AI_REG_ATTENTIONTARGET);
			//reg.Add("Path",       AI_REG_PATH);
			reg.Add("Cover", AI_REG_COVER);
		}
	};

	static Dictionaries s_dictionaries;

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		const char* queryName = xml->getAttr("name");
		m_queryID = queryName ? gEnv->pAISystem->GetTacticalPointSystem()->GetQueryID(queryName) : TPSQueryID(0);
		if (m_queryID == 0)
		{
			gEnv->pLog->LogError("QueryTPS behavior tree node: Query '%s' does not exist (yet). Line %d.", queryName, xml->getLine());
			return LoadFailure;
		}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
		m_tpsQueryName = queryName;
#endif

		if (!s_dictionaries.reg.Get(xml, "register", m_register))
		{
			gEnv->pLog->LogError("QueryTPS behavior tree node: Missing 'register' attribute, line %d.", xml->getLine());
			return LoadFailure;
		}

		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText = m_tpsQueryName;
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CommitQuery(*GetPipeUser(context), context);
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

		CPipeUser& pipeUser = *GetPipeUser(context);

		STacticalPointResult point = runtimeData.queryInstance.GetBestResult();
		assert(point.IsValid());

		if (m_register == AI_REG_REFPOINT)
		{
			if (point.flags & eTPDF_AIObject)
			{
				if (CAIObject* pAIObject = gAIEnv.pObjectContainer->GetAIObject(point.aiObjectId))
				{
					pipeUser.SetRefPointPos(pAIObject->GetPos(), pAIObject->GetEntityDir());
					return Success;
				}
				else
				{
					return Failure;
				}
			}
			else if (point.flags & eTPDF_CoverID)
			{
				pipeUser.SetRefPointPos(point.vPos, Vec3Constants<float>::fVec3_OneY);
				return Success;
			}

			// we can expect a position. vObjDir may not be set if this is not a hidespot, but should be zero.
			pipeUser.SetRefPointPos(point.vPos, point.vObjDir);
			return Success;
		}
		else if (m_register == AI_REG_COVER)
		{
			assert(point.flags & eTPDF_CoverID);

			if (gAIEnv.pCoverSystem->IsCoverOccupied(point.coverID) && gAIEnv.pCoverSystem->GetCoverOccupant(point.coverID) != pipeUser.GetEntityID())
			{
				// Found cover was occupied by someone else in the meantime
				return Failure;
			}

			pipeUser.SetCoverRegister(point.coverID);
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

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	string m_tpsQueryName;
#endif
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

		RuntimeData() : gateIsOpen(false) {}
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		const stack_string code = xml->getAttr("code");
		if (code.empty())
		{
			gEnv->pLog->LogError("LuaGate expected the 'code' attribute at line %d.", xml->getLine());
			return LoadFailure;
		}

		m_scriptFunction = SmartScriptFunction(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(code.c_str(), code.length(), "LuaGate"));
		if (!m_scriptFunction)
		{
			ErrorReporter(*this, context).LogError("Failed to compile Lua code '%s'", code.c_str());
			return LoadFailure;
		}

		return LoadChildFromXml(xml, context);
	}

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
};

//////////////////////////////////////////////////////////////////////////

class AdjustCoverStance : public Action
{
public:
	struct RuntimeData
	{
		Timer timer;
	};

	AdjustCoverStance()
		: m_duration(0.0f)
		, m_variation(0.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context)
	{
		const stack_string str = xml->getAttr("duration");

		if (str == "continuous")
			m_duration = -1.0;
		else
		{
			xml->getAttr("duration", m_duration);
			xml->getAttr("variation", m_variation);
		}

		return LoadSuccess;
	}

	virtual void OnInitialize(const UpdateContext& context)
	{
		if (m_duration >= 0.0f)
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			runtimeData.timer.Reset(m_duration, m_variation);
		}

		CPipeUser* pipeUser = GetPipeUser(context);
		ClearCoverPosture(pipeUser);
	}

	virtual Status Update(const UpdateContext& context)
	{
		CPipeUser& pipeUser = *GetPipeUser(context);

		if (pipeUser.IsInCover())
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

			const CoverHeight coverHeight = pipeUser.CalculateEffectiveCoverHeight();
			pipeUser.m_State.bodystate = (coverHeight == HighCover) ? STANCE_HIGH_COVER : STANCE_LOW_COVER;
			return runtimeData.timer.Elapsed() ? Success : Running;
		}
		else
		{
			return Failure;
		}
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
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
#endif

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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		if (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		xml->getAttr("value", m_alertness);

		if (m_alertness >= 0 && m_alertness <= 2)
		{
			return LoadSuccess;
		}
		else
		{
			gEnv->pLog->LogError("Alertness '%d' on line %d is invalid. Can only be 0, 1 or 2.", m_alertness, xml->getLine());
			return LoadFailure;
		}
	}

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		IAIActorProxy* proxy = GetPipeUser(context)->GetProxy();

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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		ICommunicationManager* communicationManager = gEnv->pAISystem->GetCommunicationManager();

		const char* commName;
		if (xml->getAttr("name", &commName))
		{
			m_commID = communicationManager->GetCommunicationID(commName);
			if (!communicationManager->GetCommunicationName(m_commID))
			{
				ErrorReporter(*this, context).LogError("Unknown communication name '%s'", commName);
				m_commID = CommID(0);
				return LoadFailure;
			}
		}
		else
		{
			ErrorReporter(*this, context).LogError("Attribute 'name' is missing or empty.");
			return LoadFailure;
		}

		const char* channelName;
		if (xml->getAttr("channel", &channelName))
		{
			m_channelID = communicationManager->GetChannelID(channelName);
			if (!m_channelID)
			{
				ErrorReporter(*this, context).LogError("Unknown channel name '%s'", channelName);
				return LoadFailure;
			}
		}
		else
		{
			ErrorReporter(*this, context).LogError("Attribute 'name' is missing or empty.");
			return LoadFailure;
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

		return LoadSuccess;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CRY_PROFILE_FUNCTION(PROFILE_AI);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.startTime = GetAISystem()->GetFrameStartTime();
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

		IAIActorProxy* proxy = GetPipeUser(context)->GetProxy();
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
		float elapsedTime = (GetAISystem()->GetFrameStartTime() - runtimeData.startTime).GetSeconds();
		if (elapsedTime > m_timeout)
		{
			ErrorReporter(*this, context).LogWarning("Communication timed out.");
			return Success;
		}

		return Running;
	}

private:

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

	struct RuntimeData
	{
		bool signalWasSet;

		RuntimeData() : signalWasSet(false) {}
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

		LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context)
		{
			m_name = xml->getAttr("name");
			IF_UNLIKELY (m_name.empty())
			{
				gEnv->pLog->LogError("Empty or missing 'name' attribute for Animate node at line %d.", xml->getLine());
				return LoadFailure;
			}

			xml->getAttr("urgent", m_urgent);

			bool loop = false;
			xml->getAttr("loop", loop);
			m_playMode = loop ? InfiniteLoop : PlayOnce;

			xml->getAttr("setBodyDirectionTowardsAttentionTarget", m_setBodyDirectionTowardsAttentionTarget);

			return LoadSuccess;
		}
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		return m_configurationData.LoadFromXml(xml, context);
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const override
	{
		debugText = m_configurationData.m_name;
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		CPipeUser* pipeUser = GetPipeUser(context);
		IAIActorProxy* proxy = pipeUser->GetProxy();
		runtimeData.signalWasSet = proxy->SetAGInput(GetAGInputMode(), m_configurationData.m_name, m_configurationData.m_urgent);

		assert(runtimeData.signalWasSet);
		IF_UNLIKELY (!runtimeData.signalWasSet)
			gEnv->pLog->LogError("Animate behavior tree node failed call to SetAGInput(%i, %s).", GetAGInputMode(), m_configurationData.m_name.c_str());

		if (m_configurationData.m_setBodyDirectionTowardsAttentionTarget)
		{
			if (IAIObject* target = pipeUser->GetAttentionTarget())
			{
				pipeUser->SetBodyTargetDir((target->GetPos() - pipeUser->GetPos()).GetNormalizedSafe());
			}
		}
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		CPipeUser* pipeUser = GetPipeUser(context);

		if (m_configurationData.m_setBodyDirectionTowardsAttentionTarget)
		{
			pipeUser->ResetBodyTargetDir();
		}

		IAIActorProxy* proxy = pipeUser->GetProxy();
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
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		IF_UNLIKELY (!runtimeData.signalWasSet)
			return Success;

		if (m_configurationData.m_playMode == PlayOnce)
		{
			IAIActorProxy* proxy = GetPipeUser(context)->GetProxy();
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

//////////////////////////////////////////////////////////////////////////

class Signal : public Action
{
	typedef Action BaseClass;

private:
	struct Dictionaries
	{
		CXMLAttrReader<ESignalFilter> filters;

		Dictionaries()
		{
			filters.Reserve(2);
			filters.Add("Sender", SIGNALFILTER_SENDER);
			filters.Add("Group", SIGNALFILTER_GROUPONLY);
			filters.Add("GroupExcludingSender", SIGNALFILTER_GROUPONLY_EXCEPT);
		}
	};

	static Dictionaries s_signalDictionaries;

public:
	struct RuntimeData
	{
	};

	Signal()
		: m_filter(SIGNALFILTER_SENDER)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		m_signalName = xml->getAttr("name");
		s_signalDictionaries.filters.Get(xml, "filter", m_filter);
		return LoadSuccess;
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const override
	{
		debugText = m_signalName;
	}
#endif

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
		if (CPipeUser* pipeUser = GetPipeUser(context))
		{
			GetAISystem()->SendSignal(m_filter, 1, m_signalName.c_str(), pipeUser,
			                          NULL, CCrc32::Compute(m_signalName.c_str()));
		}
		else
		{
			ErrorReporter(*this, context).LogError("Expected agent to be pipe user but it wasn't.");
		}
	}

private:
	string        m_signalName;
	ESignalFilter m_filter;
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
	CXMLAttrReader<EStance> stances;

	Dictionaries()
	{
		stances.Reserve(4);
		stances.Add("Stand", STANCE_STAND);
		stances.Add("Crouch", STANCE_CROUCH);
		stances.Add("Relaxed", STANCE_RELAXED);
		stances.Add("Alerted", STANCE_ALERTED);
	}
};

Dictionaries s_stanceDictionary;

class Stance : public Action
{
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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		s_stanceDictionary.stances.Get(xml, "name", m_stance);

		float degrees = 90.0f;
		xml->getAttr("allowedSlopeNormalDeviationFromUpInDegrees", degrees);
		m_allowedSlopeNormalDeviationFromUpInRadians = DEG2RAD(degrees);

		s_stanceDictionary.stances.Get(xml, "stanceToUseIfSlopeIsTooSteep", m_stanceToUseIfSlopeIsTooSteep);

		return LoadSuccess;
	}

protected:
	virtual Status Update(const UpdateContext& context) override
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

		CPipeUser* pipeUser = GetPipeUser(context);
		pipeUser->m_State.bodystate = slopeVerifiedStance;
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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		stack_string enterCode = xml->getAttr("onEnter");
		stack_string exitCode = xml->getAttr("onExit");

		if (!enterCode.empty())
			m_enterScriptFunction.reset(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(enterCode.c_str(), enterCode.length(), "LuaWrapper - enter"));

		if (!exitCode.empty())
			m_exitScriptFunction.reset(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(exitCode.c_str(), exitCode.length(), "LuaWrapper - exit"));

		return LoadChildFromXml(xml, context);
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
public:
	struct RuntimeData
	{
	};

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (Action::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		const stack_string code = xml->getAttr("code");
		IF_UNLIKELY (code.empty())
		{
			gEnv->pLog->LogError("ExecuteLua node on line %d must have some Lua code in the 'code' attribute.", xml->getLine());
			return LoadFailure;
		}

		m_scriptFunction.reset(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(code.c_str(), code.length(), "ExecuteLua behavior tree node"));
		return m_scriptFunction ? LoadSuccess : LoadFailure;
	}

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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		const stack_string code = xml->getAttr("code");
		IF_UNLIKELY (code.empty())
		{
			ErrorReporter(*this, context).LogError("Missing or empty 'code' attribute.");
			return LoadFailure;
		}

		m_scriptFunction.reset(gEnv->pScriptSystem, gEnv->pScriptSystem->CompileBuffer(code.c_str(), code.length(), "AssertLua behavior tree node"));
		IF_UNLIKELY (!m_scriptFunction)
		{
			ErrorReporter(*this, context).LogError("Failed to compile Lua code '%s'.", code.c_str());
			return LoadFailure;
		}

		return LoadSuccess;
	}

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
	{}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		const stack_string scopeName = xml->getAttr("name");

		if (scopeName.empty())
			return LoadFailure;

		m_scopeID = CAIGroup::GetGroupScopeId(scopeName.c_str());

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
		m_scopeName = scopeName.c_str();
#endif

		xml->getAttr("allowedConcurrentUsers", m_allowedConcurrentUsers);

		return LoadChildFromXml(xml, context);
	}

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

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText.Format("%s", m_scopeName.c_str());
	}
#endif

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

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	string m_scopeName;
#endif
};

//////////////////////////////////////////////////////////////////////////

class Look : public Action
{
	typedef Action BaseClass;

public:
	struct RuntimeData
	{
		std::shared_ptr<Vec3> lookTarget;
	};

	Look() : m_targetType(AttentionTarget)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		const stack_string str = xml->getAttr("at");
		if (str == "ClosestGroupMember")
			m_targetType = ClosestGroupMember;
		else if (str == "RefPoint")
			m_targetType = ReferencePoint;
		else
			m_targetType = AttentionTarget;

		return LoadSuccess;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		GetRuntimeData<RuntimeData>(context).lookTarget = GetPipeUser(context)->CreateLookTarget();
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
		runtimeData.lookTarget.reset();
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		assert(runtimeData.lookTarget.get());

		CPipeUser& pipeUser = *GetPipeUser(context);

		if (m_targetType == AttentionTarget)
		{
			if (IAIObject* target = pipeUser.GetAttentionTarget())
			{
				*runtimeData.lookTarget = target->GetPos();
			}
		}
		else if (m_targetType == ClosestGroupMember)
		{
			const Group& group = gAIEnv.pGroupManager->GetGroup(pipeUser.GetGroupId());
			const Group::Members& members = group.GetMembers();
			Group::Members::const_iterator it = members.begin();
			Group::Members::const_iterator end = members.end();
			float closestDistSq = std::numeric_limits<float>::max();
			tAIObjectID closestID = INVALID_AIOBJECTID;
			Vec3 closestPos(ZERO);
			const Vec3& myPosition = pipeUser.GetPos();
			const tAIObjectID myAIObjectID = pipeUser.GetAIObjectID();
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
			if (IAIObject* rerefencePoint = pipeUser.GetRefPoint())
			{
				*runtimeData.lookTarget = rerefencePoint->GetPos();
			}
		}

		return Running;
	}

private:
	enum At
	{
		AttentionTarget,
		ClosestGroupMember,
		ReferencePoint,
	};

	At m_targetType;
};

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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

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

		return LoadSuccess;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		CPipeUser& pipeUser = *GetPipeUser(context);

		Vec3 fireTargetPosition(ZERO);

		if (m_aimAtReferencePoint)
		{
			CAIObject* referencePoint = pipeUser.GetRefPoint();
			IF_UNLIKELY (!referencePoint)
			{
				return;
			}

			fireTargetPosition = referencePoint->GetPos();
		}
		else if (IAIObject* target = pipeUser.GetAttentionTarget())
		{
			fireTargetPosition = target->GetPos();
		}
		else
		{
			return;
		}

		gAIEnv.pAIObjectManager->CreateDummyObject(runtimeData.fireTarget);
		runtimeData.fireTarget->SetPos(fireTargetPosition);
		pipeUser.SetFireTarget(runtimeData.fireTarget);
		pipeUser.SetFireMode(FIREMODE_AIM);
		runtimeData.startTime = gEnv->pTimer->GetFrameStartTime();
		runtimeData.timeSpentWithCorrectDirection = 0.0f;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.fireTarget)
			runtimeData.fireTarget.Release();

		CPipeUser& pipeUser = *GetPipeUser(context);

		pipeUser.SetFireTarget(NILREF);
		pipeUser.SetFireMode(FIREMODE_OFF);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		IF_UNLIKELY (!runtimeData.fireTarget)
			return Success;
		CAIObject* fireTargetAIObject = runtimeData.fireTarget.GetAIObject();
		IF_UNLIKELY (fireTargetAIObject == NULL)
		{
			ErrorReporter(*this, context).LogError("Expected fire target!");
			CRY_ASSERT_MESSAGE(false, "Behavior Tree node Aim suddenly lost its fire target!!");
			return Success;
		}

		const bool aimForever = m_durationOnceWithinThreshold < 0.0f;
		if (aimForever)
			return Running;

		CPipeUser& pipeUser = *GetPipeUser(context);
		const SAIBodyInfo& bodyInfo = pipeUser.QueryBodyInfo();

		const Vec3 desiredDirection = (fireTargetAIObject->GetPos() - bodyInfo.vFirePos).GetNormalized();
		const Vec3 currentDirection = bodyInfo.vFireDir;

		// Wait until we aim in the desired direction
		const float dotProduct = currentDirection.Dot(desiredDirection);
		if (dotProduct > m_angleThresholdCosine)
		{
			const float timeSpentWithCorrectDirection = runtimeData.timeSpentWithCorrectDirection + gEnv->pTimer->GetFrameTime();
			runtimeData.timeSpentWithCorrectDirection = timeSpentWithCorrectDirection;
			if (timeSpentWithCorrectDirection > m_durationOnceWithinThreshold)
				return Success;
		}

		const CTimeValue& now = gEnv->pTimer->GetFrameStartTime();
		if (now > runtimeData.startTime + CTimeValue(8.0f))
		{
			gEnv->pLog->LogWarning("Agent '%s' failed to aim towards %f %f %f. Timed out...", pipeUser.GetName(), fireTargetAIObject->GetPos().x, fireTargetAIObject->GetPos().y, fireTargetAIObject->GetPos().z);
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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

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
			ErrorReporter(*this, context).LogError("Attribute 'useReferencePointForInitialDirectionAndPivotPosition' is missing or different"
			                                       " than 1 (1 is currently the only accepted value)");
		}

		return LoadSuccess;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		CPipeUser* pipeUser = GetPipeUser(context);

		IF_UNLIKELY (!pipeUser)
		{
			ErrorReporter(*this, context).LogError("Agent must be a pipe user but isn't.");
			return;
		}

		CAIObject* pRefPoint = pipeUser->GetRefPoint();

		if (pRefPoint)
		{
			runtimeData.initialDirection = pRefPoint->GetEntityDir();
			runtimeData.mountedWeaponPivot = pRefPoint->GetPos();
		}
		else
		{
			runtimeData.initialDirection = pipeUser->GetEntityDir();
			runtimeData.mountedWeaponPivot = pipeUser->GetPos();
		}

		runtimeData.initialDirection.Normalize();

		gAIEnv.pAIObjectManager->CreateDummyObject(runtimeData.fireTarget);
		pipeUser->SetFireTarget(runtimeData.fireTarget);
		pipeUser->SetFireMode(FIREMODE_AIM);

		UpdateAimingPosition(runtimeData);
		runtimeData.lastUpdateTime = gEnv->pTimer->GetFrameStartTime();
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.fireTarget)
			runtimeData.fireTarget.Release();

		CPipeUser* pipeUser = GetPipeUser(context);

		if (pipeUser)
		{
			pipeUser->SetFireTarget(NILREF);
			pipeUser->SetFireMode(FIREMODE_OFF);
		}
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		IF_UNLIKELY (!runtimeData.fireTarget)
			return Success;

		const CTimeValue now = gEnv->pTimer->GetFrameStartTime();
		const float elapsedSecondsFromPreviousUpdate = now.GetDifferenceInSeconds(runtimeData.lastUpdateTime);
		if (elapsedSecondsFromPreviousUpdate > m_minSecondsBeweenUpdates)
		{
			UpdateAimingPosition(runtimeData);
			runtimeData.lastUpdateTime = now;
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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

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
				ErrorReporter(*this, context).LogError("stopWithinAngle should be in the range of [0.0 .. 180.0].");
				return LoadFailure;
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
				ErrorReporter(*this, context).LogError("Min. random angle should be in the range of [0.0 .. 180.0].");
				return LoadFailure;
			}
			m_randomMinAngle = DEG2RAD(angleDegrees);
		}
		angleDegrees = 0.0f;
		if (xml->getAttr("randomMaxAngle", angleDegrees))
		{
			if ((angleDegrees < 0.0f) || (angleDegrees > 180.0f))
			{
				ErrorReporter(*this, context).LogError("Max. random angle should be in the range of [0.0 .. 180.0].");
				return LoadFailure;
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
				ErrorReporter(*this, context).LogError("Min. and max. random angles are swapped.");
				return LoadFailure;
			}

			m_randomTurnRightChance = 0.5f;
			if (xml->getAttr("randomTurnRightChance", m_randomTurnRightChance))
			{
				if ((m_randomTurnRightChance < 0.0f) || (m_randomTurnRightChance > 1.0f))
				{
					ErrorReporter(*this, context).LogError("randomTurnRightChance should be in the range of [0.0 .. 1.0].");
					return LoadFailure;
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
			ErrorReporter(*this, context).LogError("Target is missing");
		}

		return LoadSuccess;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		CPipeUser* pipeUser = GetPipeUser(context);

		Vec3 desiredBodyDirection(ZERO);
		switch (m_turnTarget)
		{
		case TurnTarget_Invalid:
		default:
			// We should never get here!
			assert(false);
			break;

		case TurnTarget_Random:
			desiredBodyDirection = pipeUser->GetEntityDir();
			break;

		case TurnTarget_AttentionTarget:
			if (IAIObject* target = pipeUser->GetAttentionTarget())
			{
				if (m_alignWithTarget)
				{
					desiredBodyDirection = pipeUser->GetEntityDir();
				}
				else
				{
					desiredBodyDirection = target->GetPos() - pipeUser->GetPhysicsPos();
				}
				desiredBodyDirection.z = 0.0f;
				desiredBodyDirection.Normalize();
			}
			break;

		case TurnTarget_RefPoint:
			CAIObject* refPointObject = pipeUser->GetRefPoint();
			if (refPointObject != NULL)
			{
				if (m_alignWithTarget)
				{
					desiredBodyDirection = refPointObject->GetEntityDir();
				}
				else
				{
					desiredBodyDirection = refPointObject->GetPos() - pipeUser->GetPhysicsPos();
				}
				desiredBodyDirection.z = 0.0f;
				desiredBodyDirection.Normalize();
			}
			break;
		}

		desiredBodyDirection = ApplyRandomTurnOnNormalXYPlane(desiredBodyDirection);

		if (!desiredBodyDirection.IsZero())
		{
			pipeUser->SetBodyTargetDir(desiredBodyDirection);
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		runtimeData.desiredBodyDirection = desiredBodyDirection;
		runtimeData.timeSpentAligning = 0.0f;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		GetPipeUser(context)->ResetBodyTargetDir();
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.desiredBodyDirection.IsZero())
		{
			ErrorReporter(*this, context).LogWarning("Desired body direction is invalid.");
			return Success;
		}

		CPipeUser& pipeUser = *GetPipeUser(context);

		// If animation body direction is within the angle threshold,
		// wait for some time and then mark the agent as ready to move
		const Vec3 actualBodyDir = pipeUser.GetBodyInfo().vAnimBodyDir;
		const bool turnedTowardsDesiredDirection = (actualBodyDir.Dot(runtimeData.desiredBodyDirection) > m_stopWithinAngleCosined);
		if (turnedTowardsDesiredDirection)
			runtimeData.correctBodyDirectionTime += gEnv->pTimer->GetFrameTime();
		else
			runtimeData.correctBodyDirectionTime = 0.0f;

		const float timeSpentAligning = runtimeData.timeSpentAligning + gEnv->pTimer->GetFrameTime();
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
public:
	struct RuntimeData
	{
	};

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		CPipeUser& pipeUser = *GetPipeUser(context);
		gAIEnv.pTargetTrackManager->ResetAgent(pipeUser.GetAIObjectID());

		pipeUser.GetProxy()->ResendTargetSignalsNextFrame();

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
				gEnv->pAISystem->GetMovementSystem()->CancelRequest(this->movementRequestID);
				this->movementRequestID = 0;
			}
		}
	};

	StopMovement()
		: m_waitUntilStopped(false)
		, m_waitUntilIdleAnimation(false)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		m_waitUntilStopped = false;
		xml->getAttr("waitUntilStopped", m_waitUntilStopped);

		m_waitUntilIdleAnimation = false;
		xml->getAttr("waitUntilIdleAnimation", m_waitUntilIdleAnimation);

		return LoadSuccess;
	}

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
		CPipeUser& pipeUser = *GetPipeUser(context);
		IAIObject* referencePoint = pipeUser.GetRefPoint();
		IEntity* entity = pipeUser.GetEntity();

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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		m_enterStates = xml->getAttr("onEnter");
		m_exitStates = xml->getAttr("onExit");

		return LoadChildFromXml(xml, context);
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& updateContext, stack_string& debugText) const
	{
		debugText.Format("onEnter(%s) - onExit(%s)", m_enterStates.c_str(), m_exitStates.c_str());
	}
#endif

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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		const stack_string modeStr = xml->getAttr("mode");
		if (modeStr == "UseLiveTarget")
		{
			m_mode = UseLiveTarget;
		}
		else
		{
			m_mode = UseAttentionTarget;
		}

		return LoadSuccess;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		CPipeUser& pipeUser = *GetPipeUser(context);

		runtimeData.pendingStatus = Failure;

		Vec3 targetPosition(ZERO);
		if (GetTargetPosition(pipeUser, OUT targetPosition))
		{
			if (gEnv->pAISystem->GetNavigationSystem()->IsLocationValidInNavigationMesh(pipeUser.GetNavigationTypeID(), targetPosition, nullptr, 5.0f, 0.5f))
			{
				const MNMPathRequest request(pipeUser.GetEntity()->GetWorldPos(),
				                             targetPosition, ZERO, -1, 0.0f, 0.0f, true,
				                             functor(runtimeData, &RuntimeData::PathfinderCallback),
				                             pipeUser.GetNavigationTypeID(), eMNMDangers_None);

				runtimeData.queuedPathID = gAIEnv.pMNMPathfinder->RequestPathTo(pipeUser.GetEntityID(), request);

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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		const char* tagName = xml->getAttr("name");
		if (!tagName)
		{
			gEnv->pLog->LogError("AnimationTagWrapper behavior tree node: Missing 'name' attribute at line %d.", xml->getLine());
			return LoadFailure;
		}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
		m_tagName = tagName;
#endif

		m_nameCRC = CCrc32::ComputeLowercase(tagName);
		return LoadChildFromXml(xml, context);
	}

#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	virtual void GetCustomDebugText(const UpdateContext& context, stack_string& debugText) const override
	{
		debugText = m_tagName;
	}
#endif

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		BaseClass::OnInitialize(context);
		SOBJECTSTATE& state = GetPipeUser(context)->GetState();
		state.mannequinRequest.CreateSetTagCommand(m_nameCRC);
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		BaseClass::OnTerminate(context);
		SOBJECTSTATE& state = GetPipeUser(context)->GetState();
		state.mannequinRequest.CreateClearTagCommand(m_nameCRC);
	}

private:
#ifdef USING_BEHAVIOR_TREE_NODE_CUSTOM_DEBUG_TEXT
	string m_tagName;
#endif
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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		IF_UNLIKELY (!xml->getAttr("duration", m_duration))
		{
			ErrorReporter(*this, context).LogError("Missing 'duration' attribute.");
			return LoadFailure;
		}

		if (xml->haveAttr("fireMode"))
		{
			IF_UNLIKELY (!g_fireModeDictionary.fireMode.Get(xml, "fireMode", m_fireMode))
			{
				ErrorReporter(*this, context).LogError("Invalid 'fireMode' attribute.");
				return LoadFailure;
			}
		}

		xml->getAttr("aimObstructedTimeout", m_aimObstructedTimeout);

		return LoadSuccess;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		BaseClass::OnInitialize(context);

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		const CTimeValue& now = gEnv->pTimer->GetFrameStartTime();
		runtimeData.endTime = now + CTimeValue(m_duration);
		runtimeData.nextPostureQueryTime = now;

		if (CPipeUser* pipeUser = GetPipeUser(context))
		{
			pipeUser->SetAdjustingAim(true);
			pipeUser->SetFireMode(m_fireMode);
		}

		runtimeData.timeObstructed = 0.0f;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		if (CPipeUser* pipeUser = GetPipeUser(context))
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

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		const CTimeValue& now = gEnv->pTimer->GetFrameStartTime();
		if (now > runtimeData.endTime)
			return Success;

		CPipeUser* pipeUser = GetPipeUser(context);

		if (!pipeUser->IsInCover())
			return Failure;

		if (m_aimObstructedTimeout >= 0.0f)
		{
			const bool isAimObstructed = pipeUser && (pipeUser->GetState().aimObstructed || pipeUser->GetAimState() == AI_AIM_OBSTRUCTED);
			if (isAimObstructed)
			{
				runtimeData.timeObstructed += gEnv->pTimer->GetFrameTime();
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

		if (now < runtimeData.nextPostureQueryTime)
			return Running;

		IF_UNLIKELY (!pipeUser)
		{
			ErrorReporter(*this, context).LogError("Agent must be a pipe user but isn't.");
			return Failure;
		}

		CPuppet* puppet = pipeUser->CastToCPuppet();
		IF_UNLIKELY (!puppet)
		{
			ErrorReporter(*this, context).LogError("Agent must be a puppet but isn't.");
			return Failure;
		}

		const IAIObject* target = pipeUser->GetAttentionTarget();
		IF_UNLIKELY (!target)
			return Failure;

		const Vec3 targetPosition = target->GetPos();

		const CoverID& coverID = pipeUser->GetCoverID();
		IF_UNLIKELY (coverID == 0)
			return Failure;

		Vec3 coverNormal(1, 0, 0);
		const Vec3 coverPosition = gAIEnv.pCoverSystem->GetCoverLocation(coverID, pipeUser->GetParameters().distanceToCover, NULL, &coverNormal);

		if (runtimeData.postureQueryID == 0)
		{
			// Query posture
			PostureManager::PostureQuery query;
			query.actor = pipeUser->CastToCAIActor();
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
					runtimeData.nextPostureQueryTime = now + CTimeValue(2.0f);

					pipeUser->m_State.bodystate = postureInfo->stance;
					pipeUser->m_State.lean = postureInfo->lean;
					pipeUser->m_State.peekOver = postureInfo->peekOver;

					const char* const coverActionName = postureInfo->agInput.c_str();
					pipeUser->m_State.coverRequest.SetCoverAction(coverActionName, postureInfo->lean);
				}
				else
				{
					runtimeData.nextPostureQueryTime = now + CTimeValue(1.0f);
				}
			}
		}

		const bool hasCoverPosture = (runtimeData.bestPostureID != -1);
		if (hasCoverPosture)
		{
			const Vec3 targetDirection = targetPosition - pipeUser->GetPos();
			pipeUser->m_State.coverRequest.SetCoverBodyDirectionWithThreshold(coverNormal, targetDirection, DEG2RAD(30.0f));
		}

		return Running;
	}

private:
	float     m_duration;
	EFireMode m_fireMode;
	float     m_aimObstructedTimeout;
};

//////////////////////////////////////////////////////////////////////////

enum ShootAt
{
	AttentionTarget,
	ReferencePoint,
	LocalSpacePosition,
};

struct ShootDictionary
{
	ShootDictionary();
	CXMLAttrReader<ShootAt> shootAt;
};

ShootDictionary::ShootDictionary()
{
	shootAt.Reserve(3);
	shootAt.Add("Target", AttentionTarget);
	shootAt.Add("RefPoint", ReferencePoint);
	shootAt.Add("LocalSpacePosition", LocalSpacePosition);
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
		, m_shootAt(AttentionTarget)
		, m_stance(STANCE_STAND)
		, m_stanceToUseIfSlopeIsTooSteep(STANCE_STAND)
		, m_allowedSlopeNormalDeviationFromUpInRadians(0.0f)
		, m_aimObstructedTimeout(-1.0f)
	{
	}

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		IF_UNLIKELY (!xml->getAttr("duration", m_duration) || m_duration < 0.0f)
		{
			ErrorReporter(*this, context).LogError("Missing or invalid 'duration' attribute.");
			return LoadFailure;
		}

		IF_UNLIKELY (!g_shootDictionary.shootAt.Get(xml, "at", m_shootAt))
		{
			ErrorReporter(*this, context).LogError("Missing or invalid 'at' attribute.");
			return LoadFailure;
		}

		IF_UNLIKELY (!g_fireModeDictionary.fireMode.Get(xml, "fireMode", m_fireMode))
		{
			ErrorReporter(*this, context).LogError("Missing or invalid 'fireMode' attribute.");
			return LoadFailure;
		}

		IF_UNLIKELY (!s_stanceDictionary.stances.Get(xml, "stance", m_stance))
		{
			ErrorReporter(*this, context).LogError("Missing or invalid 'stance' attribute.");
			return LoadFailure;
		}

		if (m_shootAt == LocalSpacePosition)
		{
			IF_UNLIKELY (!xml->getAttr("position", m_position))
			{
				ErrorReporter(*this, context).LogError("Missing or invalid 'position' attribute.");
				return LoadFailure;
			}
		}

		float degrees = 90.0f;
		xml->getAttr("allowedSlopeNormalDeviationFromUpInDegrees", degrees);
		m_allowedSlopeNormalDeviationFromUpInRadians = DEG2RAD(degrees);

		s_stanceDictionary.stances.Get(xml, "stanceToUseIfSlopeIsTooSteep", m_stanceToUseIfSlopeIsTooSteep);
		xml->getAttr("aimObstructedTimeout", m_aimObstructedTimeout);

		return LoadSuccess;
	}

protected:
	virtual void OnInitialize(const UpdateContext& context) override
	{
		BaseClass::OnInitialize(context);

		CPipeUser* pipeUser = GetPipeUser(context);
		IF_UNLIKELY (!pipeUser)
		{
			ErrorReporter(*this, context).LogError("Agent wasn't a pipe user.");
			return;
		}

		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (m_shootAt == LocalSpacePosition)
			SetupFireTargetForLocalPosition(*pipeUser, runtimeData);
		else if (m_shootAt == ReferencePoint)
			SetupFireTargetForReferencePoint(*pipeUser, runtimeData);

		pipeUser->SetFireMode(m_fireMode);

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

			pipeUser->m_State.bodystate = slopeVerifiedStance;
		}

		MovementRequest movementRequest;
		movementRequest.entityID = pipeUser->GetEntityID();
		movementRequest.type = MovementRequest::Stop;
		gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);

		runtimeData.timeWhenShootingShouldEnd = gEnv->pTimer->GetFrameStartTime() + CTimeValue(m_duration);
		runtimeData.timeObstructed = 0.0f;
	}

	virtual void OnTerminate(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		CPipeUser* pipeUser = GetPipeUser(context);

		if (runtimeData.dummyTarget)
		{
			if (pipeUser)
			{
				pipeUser->SetFireTarget(NILREF);
			}

			runtimeData.dummyTarget.Release();
		}

		pipeUser->SetFireMode(FIREMODE_OFF);

		BaseClass::OnTerminate(context);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		const CTimeValue& now = gEnv->pTimer->GetFrameStartTime();

		if (now >= runtimeData.timeWhenShootingShouldEnd)
			return Success;

		if (m_aimObstructedTimeout >= 0.0f)
		{
			CPipeUser* pipeUser = GetPipeUser(context);
			const bool isAimObstructed = pipeUser && (pipeUser->GetState().aimObstructed || pipeUser->GetAimState() == AI_AIM_OBSTRUCTED);
			if (isAimObstructed)
			{
				runtimeData.timeObstructed += gEnv->pTimer->GetFrameTime();
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

	Vec3      m_position;
	float     m_duration;
	ShootAt   m_shootAt;
	EFireMode m_fireMode;
	EStance   m_stance;
	EStance   m_stanceToUseIfSlopeIsTooSteep;
	float     m_allowedSlopeNormalDeviationFromUpInRadians;
	float     m_aimObstructedTimeout;
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

	virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
	{
		IF_UNLIKELY (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			return LoadFailure;

		IF_UNLIKELY (!xml->getAttr("timeout", m_timeout) || m_timeout < 0.0f)
		{
			ErrorReporter(*this, context).LogError("Missing or invalid 'timeout' attribute.");
			return LoadFailure;
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
			ErrorReporter(*this, context).LogError("Missing or invalid 'type' attribute.");
			return LoadFailure;
		}

		return LoadSuccess;
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
		runtimeData.timeWhenWeShouldGiveUpThrowing = GetFrameStartTime(context) + CTimeValue(m_timeout);

		if (CPuppet* puppet = GetPuppet(context))
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
		GetPipeUser(context)->SetFireMode(FIREMODE_OFF);

		BaseClass::OnTerminate(context);
	}

	virtual Status Update(const UpdateContext& context) override
	{
		RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

		if (runtimeData.grenadeHasBeenThrown)
		{
			return Success;
		}
		else if (GetFrameStartTime(context) >= runtimeData.timeWhenWeShouldGiveUpThrowing)
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

protected:
	virtual Status Update(const UpdateContext& context) override
	{
		if (CPipeUser* pipeUser = GetPipeUser(context))
		{
			gAIEnv.pTargetTrackManager->PullDownThreatLevel(pipeUser->GetAIObjectID(), AITHREAT_SUSPECT);
		}

		return Success;
	}
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RegisterBehaviorTreeNodes_AI()
{
	assert(gAIEnv.pBehaviorTreeManager);

	IBehaviorTreeManager& manager = *gAIEnv.pBehaviorTreeManager;

	REGISTER_BEHAVIOR_TREE_NODE(manager, GoalPipe);
	REGISTER_BEHAVIOR_TREE_NODE(manager, LuaBehavior);
	REGISTER_BEHAVIOR_TREE_NODE(manager, Bubble);
	REGISTER_BEHAVIOR_TREE_NODE(manager, Move);
	REGISTER_BEHAVIOR_TREE_NODE(manager, QueryTPS);
	REGISTER_BEHAVIOR_TREE_NODE(manager, LuaGate);
	REGISTER_BEHAVIOR_TREE_NODE(manager, AdjustCoverStance);
	REGISTER_BEHAVIOR_TREE_NODE(manager, SetAlertness);
	REGISTER_BEHAVIOR_TREE_NODE(manager, Communicate);
	REGISTER_BEHAVIOR_TREE_NODE(manager, Animate);
	REGISTER_BEHAVIOR_TREE_NODE(manager, Signal);
	REGISTER_BEHAVIOR_TREE_NODE(manager, SendTransitionSignal);
	REGISTER_BEHAVIOR_TREE_NODE(manager, Stance);
	REGISTER_BEHAVIOR_TREE_NODE(manager, LuaWrapper);
	REGISTER_BEHAVIOR_TREE_NODE(manager, ExecuteLua);
	REGISTER_BEHAVIOR_TREE_NODE(manager, AssertLua);
	REGISTER_BEHAVIOR_TREE_NODE(manager, GroupScope);
	REGISTER_BEHAVIOR_TREE_NODE(manager, Look);
	REGISTER_BEHAVIOR_TREE_NODE(manager, Aim);
	REGISTER_BEHAVIOR_TREE_NODE(manager, AimAroundWhileUsingAMachingGun);
	REGISTER_BEHAVIOR_TREE_NODE(manager, TurnBody);
	REGISTER_BEHAVIOR_TREE_NODE(manager, ClearTargets);
	REGISTER_BEHAVIOR_TREE_NODE(manager, StopMovement);
	REGISTER_BEHAVIOR_TREE_NODE(manager, SmartObjectStatesWrapper);
	REGISTER_BEHAVIOR_TREE_NODE(manager, CheckIfTargetCanBeReached);
	REGISTER_BEHAVIOR_TREE_NODE(manager, AnimationTagWrapper);
	REGISTER_BEHAVIOR_TREE_NODE(manager, ShootFromCover);
	REGISTER_BEHAVIOR_TREE_NODE(manager, Shoot);
	REGISTER_BEHAVIOR_TREE_NODE(manager, ThrowGrenade);
	REGISTER_BEHAVIOR_TREE_NODE(manager, PullDownThreatLevel);

	REGISTER_BEHAVIOR_TREE_NODE(manager, TeleportStalker_Deprecated);
}
}
