// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Cry
{
namespace AI
{
namespace CollisionAvoidance
{

enum class ETreatType
{
	None,
	Agent,
	Obstacle,
	Count,
};

struct SAgentParams
{
	float radius = 0.4f;
	float height = 2.0f;
	float maxSpeed = 5.0f;
	float maxAcceleration = 0.1f; // currently not used

	Vec3  currentLocation = ZERO;
	Vec3  currentVelocity = ZERO;
	Vec3  desiredVelocity = ZERO;
};

struct SObstacleParams
{
	float radius = 1.0f;
	float height = 2.0f;
	Vec3  currentLocation = ZERO;
	Vec3  currentVelocity = ZERO; // currently not used
};

struct IAgent
{
	virtual ~IAgent() {}

	//! Sets parameters and returns how the collision avoidance agent should be used in the each update tick. This function is called for every registered agent in Collision Avoidance system prior its updating.
	//! \param (out) outAgentParams Parameters of the agent that should actively avoid other agents or obstacles. Must be set when returning ETreatType::Agent.
	//! \param (out) outObstacleParams Parameters of the agent that creates obstacle for other agents but does not try to actively avoid obstacles. Must be set when returning ETreatType::Obstacle.
	//! \return Way how the agent should be treated in the current frame.
	virtual ETreatType            GetTreatmentDuringUpdateTick(SAgentParams& outAgentParams, SObstacleParams& outObstacleParams) const = 0;
	
	//! Function called after Collision Avoidance finished its updating adjusted velocity is available.
	//! \param avoidanceVelocity Resulting velocity of the agent after collision avoidance update.
	//! \param updateTime Last update tick duration.
	virtual void                  ApplyComputedVelocity(const Vec2& avoidanceVelocity, float updateTime) = 0;

	//! Returns pointer to the navigation query filter.
	//! \return Pointer to the navigation query filter, which needs to remain valid while the collision avoidance is being computed.
	virtual const INavMeshQueryFilter* GetNavigationQueryFilter() const = 0;
	virtual NavigationAgentTypeID      GetNavigationTypeId() const = 0;

	virtual const char*                GetDebugName() const = 0;
};

struct ISystem
{
	virtual ~ISystem() {}

	//! Tries to register the Agent to the Collision Avoidance System.
	//! \param pAgent Agent to be registered.
	//! \return True if the agent was successfully registered or was already registered. False otherwise.
	virtual bool RegisterAgent(IAgent* pAgent) = 0;

	//! Tries to unregister the Agent from the Collision Avoidance System if it was previously registered.
	//! \param pAgent Agent to be unregistered.
	//! \return True if the agent was successfully unregistered. False if the agent was not registered.
	virtual bool UnregisterAgent(IAgent* pAgent) = 0;
};

} // namespace CollisionAvoidance
} // namespace AI
} // namespace Cry