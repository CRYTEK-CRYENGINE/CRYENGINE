// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "MovementActorAdapter.h"
#include "CollisionAvoidanceAgent.h"

#include <CryAISystem/MovementRequest.h>
#include <CryAISystem/NavigationSystem/INavigationQuery.h>

#include <CrySerialization/Forward.h>
#include <CryEntitySystem/IEntityComponent.h>

namespace Schematyc
{
// Forward declare interfaces.
struct IEnvRegistrar;
// Forward declare structures.
struct SUpdateContext;
}

class CEntityAINavigationComponent final : public IEntityComponent
{
	const uint64 kDefaultEntityEventMask =
		ENTITY_EVENT_BIT(ENTITY_EVENT_LEVEL_LOADED)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_RESET)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED);

public:
	static const CryGUID& IID()
	{
		static CryGUID id = "1b988fa3-2cc8-4dfa-9107-a63aded77e91"_cry_guid;
		return id;
	}

	typedef std::function<void (const Vec3&)> StateUpdatedCallback;

	struct SMovementProperties
	{
		static void ReflectType(Schematyc::CTypeDesc<SMovementProperties>& desc);

		bool        operator==(const SMovementProperties& other) const
		{
			return normalSpeed == other.normalSpeed
			       && minSpeed == other.minSpeed
			       && maxSpeed == other.maxSpeed
			       && maxAcceleration == other.maxAcceleration
			       && maxDeceleration == other.maxDeceleration
			       && lookAheadDistance == other.lookAheadDistance
			       && bStopAtEnd == other.bStopAtEnd;
		}

		float normalSpeed = 4.0f;
		float minSpeed = 0.0f;
		float maxSpeed = 6.0f;
		float maxAcceleration = 4.0f;
		float maxDeceleration = 6.0f;
		float lookAheadDistance = 0.5f;
		bool  bStopAtEnd = true;
	};

	struct SCollisionAvoidanceProperties
	{
		enum class EType
		{
			None,
			Passive,
			Active,
		};

		static void ReflectType(Schematyc::CTypeDesc<SCollisionAvoidanceProperties>& desc);

		bool        operator==(const SCollisionAvoidanceProperties& other) const
		{
			return radius == other.radius && type == other.type;
		}

		float radius = 0.3;
		EType type = EType::Active;
	};

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

	bool        TestRaycastHit(const Vec3& toPositon, Vec3& hitPos, Vec3& hitNorm) const;
	bool        IsRayObstructed(const Vec3& toPosition) const;
	bool        IsDestinationReachable(const Vec3& destination) const;
	void        NavigateTo(const Vec3& destination);
	void        StopMovement();

	Vec3        GetRequestedVelocity() const;
	void        SetRequestedVelocity(const Vec3& velocity);
	void        SetStateUpdatedCallback(StateUpdatedCallback callback) { m_stateUpdatedCallback = callback; }

	NavigationAgentTypeID      GetNavigationTypeId() const { return m_movementAdapter.GetNavigationAgentTypeID(); }
	const INavMeshQueryFilter* GetNavigationQueryFilter() const { return &m_navigationQueryFilter; }

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

	const SMovementProperties&           GetMovementProperties() const           { return m_movementProperties; }
	const SCollisionAvoidanceProperties& GetCollisionAvoidanceProperties() const { return m_collisionAvoidanceProperties; }

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

	SNavMeshQueryFilterDefault    m_navigationQueryFilter;
};

void ReflectType(Schematyc::CTypeDesc<CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType>& desc);
