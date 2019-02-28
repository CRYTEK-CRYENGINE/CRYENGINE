// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "MovementActorAdapter.h"
#include "CollisionAvoidanceAgent.h"

#include <CryAISystem/MovementRequest.h>
#include <CryAISystem/NavigationSystem/INavigationQuery.h>
#include <CryAISystem/Components/IEntityNavigationComponent.h>

#include <CrySerialization/Forward.h>

namespace Schematyc
{
// Forward declare interfaces.
struct IEnvRegistrar;
// Forward declare structures.
struct SUpdateContext;
}

class CEntityAINavigationComponent final : public IEntityNavigationComponent
{
	const uint64 kDefaultEntityEventMask =
		ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_RESET)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

public:
	struct SStateUpdatedSignal
	{
		static void ReflectType(Schematyc::CTypeDesc<SStateUpdatedSignal>& typeInfo);
		Vec3        m_requestedVelocity;
	};

	struct SNavigationCompletedSignal
	{
		static void ReflectType(Schematyc::CTypeDesc<SNavigationCompletedSignal>& typeInfo);
	};

	struct SNavigationFailedSignal
	{
		static void ReflectType(Schematyc::CTypeDesc<SNavigationFailedSignal>& typeInfo);
	};

public:

	CEntityAINavigationComponent();
	virtual ~CEntityAINavigationComponent();

	// IEntityComponent
	virtual void   Initialize() override;
	virtual void   OnShutDown() override;

	virtual void   ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const override;
	// ~IEntityComponent

	// IEntityNavigationComponent
	virtual void   SetNavigationAgentType(const char* szTypeName) override;
	virtual void   SetMovementProperties(const SMovementProperties& properties) override;
	virtual void   SetCollisionAvoidanceProperties(const SCollisionAvoidanceProperties& properties) override;
	virtual void   SetNavigationQueryFilter(const SNavMeshQueryFilterDefault& filter) override { m_navigationQueryFilter = filter; }
	virtual bool   TestRaycastHit(const Vec3& toPositon, Vec3& hitPos, Vec3& hitNorm) const override;
	virtual bool   IsRayObstructed(const Vec3& toPosition) const override;
	virtual bool   IsDestinationReachable(const Vec3& destination) const override;
	virtual void   NavigateTo(const Vec3& destination) override;
	virtual void   StopMovement() override;

	virtual const SMovementProperties& GetMovementProperties() const override;
	virtual const SCollisionAvoidanceProperties& GetCollisionAvoidanceProperties() const override;

	virtual Vec3   GetRequestedVelocity() const override;
	virtual void   SetRequestedVelocity(const Vec3& velocity) override;
	virtual void   SetStateUpdatedCallback(const IEntityNavigationComponent::StateUpdatedCallback& callback) override { m_stateUpdatedCallback = callback; }
	virtual void   SetNavigationCompletedCallback(const NavigationCompletedCallback& callback) override { m_navigationCompletedCallback = callback; }

	virtual NavigationAgentTypeID      GetNavigationTypeId() const  override { return m_movementAdapter.GetNavigationAgentTypeID(); }
	virtual const INavMeshQueryFilter* GetNavigationQueryFilter() const  override { return &m_navigationQueryFilter; }
	// ~IEntityNavigationComponent

	static void ReflectType(Schematyc::CTypeDesc<CEntityAINavigationComponent>& desc);
	static void Register(Schematyc::IEnvRegistrar& registrar);

private:
	friend class CMovementActorAdapter;
	friend class CCollisionAvoidanceAgent;

	// Local implementation of the IPathObstacles
	// For now we don't handle dynamic obstacles but this object is required to use the default path follower offered by the AISystem
	struct PathObstacles : public IPathObstacles
	{
		virtual ~PathObstacles() {}
		virtual bool IsPathIntersectingObstacles(const NavigationMeshID meshID, const Vec3& start, const Vec3& end, float radius) const override    { return false; };
		virtual bool IsPointInsideObstacles(const Vec3& position) const override                                                                    { return false; }
		virtual bool IsLineSegmentIntersectingObstaclesOrCloseToThem(const Lineseg& linesegToTest, float maxDistanceToConsiderClose) const override { return false; }
	};

	bool                                 IsGameOrSimulation() const;
	void                                 Start();
	void                                 Stop();
	void                                 Reset();

	void                                 UpdateTransformation(float deltaTime);
	void                                 UpdateVelocity(float deltaTime);

	bool                                 QueueMovementRequest(const MovementRequest& request);
	void                                 CancelCurrentMovementRequest();
	void                                 CancelRequestedPath();

	Vec3                                 GetVelocity() const;
	Vec3                                 GetPosition() const;

	void                                 FillPathFollowerParams(PathFollowerParams& params) const;

	// Callbacks
	void                      QueueRequestPathFindingRequest(MNMPathRequest& request);
	Movement::PathfinderState CheckOnPathfinderState();
	IPathFollower*            GetPathfollower() { return m_pPathfollower.get(); }
	INavPath*                 GetNavPath()      { return m_pNavigationPath.get(); }

	void                      MovementRequestCompleted(const MovementRequestResult& result);
	void                      OnMNMPathfinderResult(const MNM::QueuedPathID& requestId, MNMPathRequestResult& result);
	void                      NewStateComputed(const PathFollowResult& result, void* pSender);

	Vec3                          m_currentPosition;
	Vec3                          m_currentVelocity;
	Vec3                          m_requestedVelocity;

	NavigationAgentTypeID         m_agentTypeId;
	SMovementProperties           m_movementProperties;
	SCollisionAvoidanceProperties m_collisionAvoidanceProperties;
	bool                          m_bUpdateTransformation = false;

	CMovementActorAdapter         m_movementAdapter;
	CCollisionAvoidanceAgent      m_collisionAgent;

	PathObstacles                 m_pathObstacles;
	IPathFollowerPtr              m_pPathfollower;

	MNM::QueuedPathID             m_pathRequestId;
	INavPathPtr                   m_pNavigationPath;

	MovementRequestID             m_movementRequestId;
	bool                          m_lastPathRequestFailed;

	StateUpdatedCallback          m_stateUpdatedCallback;
	NavigationCompletedCallback   m_navigationCompletedCallback;

	SNavMeshQueryFilterDefault    m_navigationQueryFilter;
};

void ReflectType(Schematyc::CTypeDesc<CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType>& desc);
