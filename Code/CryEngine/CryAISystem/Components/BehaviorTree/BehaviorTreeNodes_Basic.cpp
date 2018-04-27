// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BehaviorTreeNodes_Basic.h"
#include "../../Cover/CoverSystem.h"
#include <CryAISystem/BehaviorTree/Action.h>
#include <CryAISystem/MovementRequest.h>
#include <CryAISystem/IMovementSystem.h>


// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace BehaviorTree
{

	//////////////////////////////////////////////////////////////////////////

	class Basic_MoveToPosition : public Action
	{

		typedef Action BaseClass;

	public:

		struct RuntimeData
		{
			MovementRequestID movementRequestID;
			Status pendingStatus;

			RuntimeData()
				: movementRequestID(0)
				, pendingStatus(Running)
			{}

			~RuntimeData()
			{
				if (this->movementRequestID)
				{
					gAIEnv.pMovementSystem->CancelRequest(this->movementRequestID);
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

	public:

		virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
		{
			if (BaseClass::LoadFromXml(xml, context) == LoadFailure)
			{
				return LoadFailure;
			}

			// "bbDestination" (name under which the destination position is stored in the blackboard)
			const char* szBBDestination;
			if (!xml->getAttr("bbDestination", &szBBDestination))
			{
				ErrorReporter(*this, context).LogError("Missing 'bbDestination' parameter");
				return LoadFailure;
			}
			m_destinationBBVariableId = BlackboardVariableId(szBBDestination);
			m_destinationBBVariableName = szBBDestination;

			// movementStyle
			m_movementStyle.ReadFromXml(xml);

			return LoadSuccess;
		}

		virtual void OnInitialize(const UpdateContext& context) override
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

			Vec3 destination;
			if (context.blackboard.GetVariable(m_destinationBBVariableId, destination))
			{
				RequestMovementTo(destination, context.entityId, runtimeData);
			}
			else
			{
				ErrorReporter(*this, context).LogError("Missing entry in the blackboard or data type mismatch: '%s' (expected to be a Vec3)", m_destinationBBVariableName.c_str());
				runtimeData.pendingStatus = Failure;
			}
		}

	protected:

		virtual Status Update(const UpdateContext& context) override
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			return runtimeData.pendingStatus;
		}

	private:

		void RequestMovementTo(const Vec3& destination, const EntityId entityID, RuntimeData& runtimeData)
		{
			assert(!runtimeData.movementRequestID);

			MovementRequest movementRequest;
			movementRequest.entityID = entityID;
			movementRequest.destination = destination;
			movementRequest.callback = functor(runtimeData, &RuntimeData::MovementRequestCallback);
			movementRequest.style = m_movementStyle;

			runtimeData.movementRequestID = gAIEnv.pMovementSystem->QueueRequest(movementRequest);

			if (runtimeData.movementRequestID.IsInvalid())
			{
				runtimeData.pendingStatus = Failure;
			}
		}

	private:

		BlackboardVariableId m_destinationBBVariableId;
		string m_destinationBBVariableName;  // for debugging purpose
		MovementStyle m_movementStyle;
	};

	//////////////////////////////////////////////////////////////////////////

	class Basic_MoveToCover : public Action
	{
		typedef Action BaseClass;

	public:
		struct RuntimeData
		{
			Status            pendingStatus;
			MovementRequestID movementRequestID;

			RuntimeData()
				: movementRequestID(0)
				, pendingStatus(Running)
			{
			}

			~RuntimeData()
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

		Basic_MoveToCover()
		{
		}

		virtual LoadResult LoadFromXml(const XmlNodeRef& xml, const LoadContext& context) override
		{
			if(BaseClass::LoadFromXml(xml, context) == LoadFailure)
				return LoadFailure;

			const char* szBBCoverId;
			if (!xml->getAttr("bbCoverId", &szBBCoverId))
			{
				ErrorReporter(*this, context).LogError("Missing 'bbCoverId' parameter");
				return LoadFailure;
			}
			
			m_coverIdBBVariableId = BlackboardVariableId(szBBCoverId);
			m_coverIdBBVariableName = szBBCoverId;

			m_movementStyle.ReadFromXml(xml);
			m_movementStyle.SetMovingToCover(true);

			return LoadSuccess;
		}

		virtual void OnInitialize(const UpdateContext& context) override
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);

			CoverID coverId;
			if (!context.blackboard.GetVariable<CoverID>(m_coverIdBBVariableId, coverId))
			{
				ErrorReporter(*this, context).LogError("Missing entry in the blackboard or data type mismatch: '%s' (expected to be a CoverId)", m_coverIdBBVariableName.c_str());
				runtimeData.pendingStatus = Failure;
				return;
			}

			if (!coverId)
			{
				ErrorReporter(*this, context).LogError("Invalid input Cover id (= %u).", static_cast<uint32>(coverId));
				runtimeData.pendingStatus = Failure;
				return;
			}

			ICoverUser* pCoverUser = gEnv->pAISystem->GetCoverSystem()->GetRegisteredCoverUser(context.entityId);
			if (!pCoverUser)
			{
				ErrorReporter(*this, context).LogWarning("Entity '%s' isn't registered in cover system.", context.entity.GetName());
				runtimeData.pendingStatus = Failure;
				return;
			}

			runtimeData.pendingStatus = Running;

			pCoverUser->SetNextCoverID(coverId);
			const Vec3 destPosition = pCoverUser->GetCoverLocation(coverId);

			CRY_ASSERT(!runtimeData.movementRequestID);

			MovementRequest movementRequest;
			movementRequest.entityID = context.entityId;
			movementRequest.destination = destPosition;
			movementRequest.callback = functor(runtimeData, &RuntimeData::MovementRequestCallback);
			movementRequest.style = m_movementStyle;

			runtimeData.movementRequestID = gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);
		}

	private:
		virtual Status Update(const UpdateContext& context) override
		{
			RuntimeData& runtimeData = GetRuntimeData<RuntimeData>(context);
			return runtimeData.pendingStatus;
		}

	private:
		BlackboardVariableId m_coverIdBBVariableId;
		string          m_coverIdBBVariableName;
		MovementStyle   m_movementStyle;
	};

	//////////////////////////////////////////////////////////////////////////

	void RegisterBehaviorTreeNodes_Basic()
	{
		IBehaviorTreeManager& pManager = *gEnv->pAISystem->GetIBehaviorTreeManager();

		REGISTER_BEHAVIOR_TREE_NODE_WITH_NAME(pManager, Basic_MoveToPosition, "Basic_MoveToPosition");
		REGISTER_BEHAVIOR_TREE_NODE_WITH_NAME(pManager, Basic_MoveToCover, "Basic_MoveToCover");
	}

}
