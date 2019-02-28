#pragma once

#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/IObject.h>
#include <CrySchematyc/Utils/SharedString.h>
#include <CrySchematyc/Reflection/TypeDesc.h>

#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IMovementSystem.h>

#include <CryAISystem/MovementRequest.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CPathfindingComponent
			: public IEntityComponent
			, private IMovementActorAdapter
			, private IAIPathAgent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// Dummy implementation so we can use IAISystem::CreateAndReturnNewDefaultPathFollower
			class CPathObstacles final
				: public IPathObstacles
			{
				// IPathObstacles
				virtual bool IsPathIntersectingObstacles(const NavigationMeshID meshID, const Vec3& start, const Vec3& end, float radius) const final { return false; }
				virtual bool IsPointInsideObstacles(const Vec3& position) const final { return false; }
				virtual bool IsLineSegmentIntersectingObstaclesOrCloseToThem(const Lineseg& linesegToTest, float maxDistanceToConsiderClose) const final { return false; }
				// ~IPathObstacles
			};

			// IEntityComponent
			virtual void Initialize() final;

			virtual void ProcessEvent(const SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final;

			virtual void ShutDown() final { Reset(); }
			// ~IEntityComponent

		public:
			virtual ~CPathfindingComponent() = default;

			static void ReflectType(Schematyc::CTypeDesc<CPathfindingComponent>& desc)
			{
				desc.SetGUID("{470C0CD6-198F-412E-A99A-15456066162B}"_cry_guid);
				desc.SetEditorCategory("AI");
				desc.SetLabel("Pathfinder");
				desc.SetDescription("Exposes the ability to get path finding callbacks");
				//desc.SetIcon("icons:ObjectTypes/object.ico");
				desc.SetComponentFlags({ IEntityComponent::EFlags::HiddenFromUser, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

				desc.AddMember(&CPathfindingComponent::m_maxAcceleration, 'maxa', "MaxAcceleration", "Maximum Acceleration", "Maximum possible physical acceleration", 10.f);
			}

			virtual void RequestMoveTo(const Vec3 &position)
			{
				CRY_ASSERT_MESSAGE(m_movementRequestId.id == 0, "RequestMoveTo can not be called while another request is being handled!");

				MovementRequest movementRequest;
				movementRequest.entityID = GetEntityId();
				movementRequest.destination = position;
				movementRequest.callback = functor(*this, &CPathfindingComponent::MovementRequestCallback);
				movementRequest.style.SetSpeed(MovementStyle::Walk);
				movementRequest.type = MovementRequest::Type::MoveTo;

				m_movementRequestId = gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);
			}

			virtual void CancelCurrentRequest()
			{
				CRY_ASSERT(m_movementRequestId.id != 0);

				gEnv->pAISystem->GetMovementSystem()->CancelRequest(m_movementRequestId);
				m_movementRequestId = 0;

				if (m_pathFinderRequestId != 0)
				{
					gEnv->pAISystem->GetMNMPathfinder()->CancelPathRequest(m_pathFinderRequestId);

					m_pathFinderRequestId = 0;
					m_pathfindingState = Movement::Canceled;
				}
			}

			bool IsProcessingRequest() const { return m_movementRequestId != 0; }

			virtual void SetMaxAcceleration(float maxAcceleration);
			float GetMaxAcceleration() const { return m_maxAcceleration; }

			virtual void SetMovementRecommendationCallback(std::function<void(const Vec3& recommendedVelocity)> callback) { m_movementRecommendationCallback = callback; }

		protected:
			// IMovementActorAdapter
			virtual void                  OnMovementPlanProduced() final {}

			virtual NavigationAgentTypeID GetNavigationAgentTypeID() const final { return m_navigationAgentTypeId; }
			virtual Vec3                  GetPhysicsPosition() const final { return GetEntity()->GetWorldPos(); }
			virtual Vec3                  GetVelocity() const final
			{
				if (IPhysicalEntity* pPhysicalEntity = m_pEntity->GetPhysicalEntity())
				{
					pe_status_dynamics dynStatus;
					if (pPhysicalEntity->GetStatus(&dynStatus))
					{
						return dynStatus.v;
					}
				}

				return ZERO;
			}

			virtual Vec3                  GetMoveDirection() const final { return GetVelocity().GetNormalized(); }
			virtual Vec3                  GetAnimationBodyDirection() const final { return m_requestedTargetBodyDirection; }
			virtual EActorTargetPhase     GetActorPhase() const final { return eATP_None; }
			virtual void                  SetMovementOutputValue(const PathFollowResult& result) final;
			virtual void                  SetBodyTargetDirection(const Vec3& direction) final { m_requestedTargetBodyDirection = direction; }
			virtual void                  ResetMovementContext() final {}
			virtual void                  ClearMovementState() final;
			virtual void                  ResetBodyTarget() final {}
			virtual void                  ResetActorTargetRequest() final {}
			virtual bool                  IsMoving() const final { return !GetVelocity().IsZero(0.01f); }

			virtual void                  RequestExactPosition(const SAIActorTargetRequest* request, const bool lowerPrecision) final {}

			virtual bool                  IsClosestToUseTheSmartObject(const OffMeshLink_SmartObject& smartObjectLink) const final { return false; }
			virtual bool                  PrepareNavigateSmartObject(CSmartObject* pSmartObject, OffMeshLink_SmartObject* pSmartObjectLink) final { return false; }
			virtual void                  InvalidateSmartObjectLink(CSmartObject* pSmartObject, OffMeshLink_SmartObject* pSmartObjectLink) final {}

			virtual bool                  GetDesignedPath(SShape& pathShape) const final { return false; }
			virtual void                  CancelRequestedPath() final {}
			virtual void                  ConfigurePathfollower(const MovementStyle& style) final {}

			virtual void                  SetActorPath(const MovementStyle& style, const INavPath& navPath) final {}
			virtual void                  SetActorStyle(const MovementStyle& style, const INavPath& navPath) final {}
			virtual void                  SetStance(const MovementStyle::Stance stance) final {}

			virtual std::shared_ptr<Vec3> CreateLookTarget() final { return nullptr; }
			virtual void                  SetLookTimeOffset(float lookTimeOffset) final {}
			virtual void UpdateLooking(float updateTime, std::shared_ptr<Vec3> lookTarget, const bool targetReachable, const float pathDistanceToEnd, const Vec3& followTargetPosition, const MovementStyle& style) final {}
			// ~IMovementActorAdapter

			// IAIPathAgent
			virtual IEntity *GetPathAgentEntity() const final { return GetEntity(); }
			virtual const char *GetPathAgentName() const final { return GetEntity()->GetClass()->GetName(); }

			virtual unsigned short GetPathAgentType() const final { return AIOBJECT_ACTOR; }

			virtual float GetPathAgentPassRadius() const final { return 1.f; }
			virtual Vec3 GetPathAgentPos() const final { return GetPathAgentEntity()->GetWorldPos(); }
			virtual Vec3 GetPathAgentVelocity() const final { return GetVelocity(); }

			virtual const AgentMovementAbility& GetPathAgentMovementAbility() const final { return m_movementAbility; }

			virtual void SetPathToFollow(const char* pathName) final {}
			virtual void         SetPathAttributeToFollow(bool bSpline) final {}

			virtual bool GetValidPositionNearby(const Vec3& proposedPosition, Vec3& adjustedPosition) const final { return false; }
			virtual bool GetTeleportPosition(Vec3& teleportPos) const final { return false; }

			virtual IPathFollower *GetPathFollower() const final { return m_pPathFollower.get(); }

			virtual bool IsPointValidForAgent(const Vec3& pos, uint32 flags) const final { return true; }
			// ~IAIPathAgent

			void Reset()
			{
				gEnv->pAISystem->GetMovementSystem()->UnregisterEntity(GetEntityId());
				m_pPathFollower.reset();
				SAFE_RELEASE(m_pFoundPath);
			}

			void MovementRequestCallback(const MovementRequestResult &result) {}

			INavPath *GetINavPath() { return m_pFoundPath; };
			Movement::PathfinderState GetPathfinderState()
			{ 
				// This function should always return the state of the last path finding request.
				// m_pathfindingState shouldn't be changed anywhere but when RequestPathTo callback is called or path finding request is completed or canceled 
				return m_pathfindingState;
			}
			void RequestPathTo(MNMPathRequest& request)
			{
				m_pathfindingState = Movement::StillFinding;

				request.resultCallback = functor(*this, &CPathfindingComponent::OnMNMPathResult);
				request.agentTypeID = m_navigationAgentTypeId;

				m_pathFinderRequestId = gEnv->pAISystem->GetMNMPathfinder()->RequestPathTo(GetEntityId(), request);
			}

			void OnMNMPathResult(const MNM::QueuedPathID& requestId, MNMPathRequestResult& result)
			{
				m_pathFinderRequestId = 0;

				if (result.HasPathBeenFound())
				{
					m_pathfindingState = Movement::FoundPath;

					SAFE_DELETE(m_pFoundPath);
					m_pFoundPath = result.pPath->Clone();

					// Bump version
					m_pFoundPath->SetVersion(m_pFoundPath->GetVersion() + 1);

					m_pPathFollower->Reset();
					m_pPathFollower->AttachToPath(m_pFoundPath);
				}
				else
				{
					m_pathfindingState = Movement::CouldNotFindPath;
				}
			}

			NavigationAgentTypeID m_navigationAgentTypeId;
			MovementActorCallbacks m_callbacks;

			MovementRequestID m_movementRequestId = 0;
			uint32 m_pathFinderRequestId;

			std::shared_ptr<IPathFollower> m_pPathFollower;
			CPathObstacles m_pathObstacles;

			Movement::PathfinderState m_pathfindingState = Movement::CouldNotFindPath;
			INavPath *m_pFoundPath = nullptr;

			AgentMovementAbility m_movementAbility;

			Vec3 m_requestedTargetBodyDirection;

			Schematyc::Range<0, 10000> m_maxAcceleration = 6.0f;
			std::function<void(const Vec3& recommendedVelocity)> m_movementRecommendationCallback;
		};
	}
}