// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CollisionAvoidanceSystem_h__
#define __CollisionAvoidanceSystem_h__

#pragma once

struct ICollisionAvoidanceAgent;
struct INavMeshQueryFilter;

class CCollisionAvoidanceSystem
{
public:
	CCollisionAvoidanceSystem();

	struct SAgentParams
	{
		SAgentParams()
			: radius(0.4f)
			, maxSpeed(4.5f)
			, maxAcceleration(0.1f)
			, currentLocation(ZERO)
			, currentLookDirection(ZERO)
			, currentVelocity(ZERO)
			, desiredVelocity(ZERO)
		{}

		SAgentParams(float _radius, float _maxSpeed, float _maxAcceleration, const Vec2& _currentLocation, const Vec2& _currentVelocity,
		            const Vec2& _currentLookDirection, const Vec2& _desiredVelocity)
			: radius(_radius)
			, maxSpeed(_maxSpeed)
			, maxAcceleration(_maxAcceleration)
			, currentLocation(_currentLocation)
			, currentVelocity(_currentVelocity)
			, currentLookDirection(_currentLookDirection)
			, desiredVelocity(_desiredVelocity)
		{}

		float radius;
		float maxSpeed;
		float maxAcceleration;

		Vec3  currentLocation;
		Vec2  currentVelocity;
		Vec2  currentLookDirection;
		Vec2  desiredVelocity;
	};

	struct SObstacleParams
	{
		SObstacleParams()
			: radius(1.0f)
			, currentLocation(ZERO)
		{}

		float radius;

		Vec3  currentLocation;
		Vec2  currentVelocity;
	};

	void Reset(bool bUnload = false);
	void Update(float updateTime);

	void RegisterAgent(ICollisionAvoidanceAgent* agent);
	void UnregisterAgent(ICollisionAvoidanceAgent* agent);

	void DebugDraw();
private:
	static const size_t kMaxAvoidingAgents = 512;

	typedef size_t AgentID;
	typedef size_t ObstacleID;

	void                  PopulateState();
	void                  ApplyResults(float updateTime);

	AgentID               CreateAgent(NavigationAgentTypeID navigationTypeID, const INavMeshQueryFilter* pFilter, const char* szName);
	ObstacleID            CreateObstable();

	void                  SetAgent(AgentID agentID, const SAgentParams& params);
	const SAgentParams&    GetAgent(AgentID agentID) const;

	void                  SetObstacle(ObstacleID obstacleID, const SObstacleParams& params);
	const SObstacleParams& GetObstacle(ObstacleID obstacleID) const;

	const Vec2&           GetAvoidanceVelocity(AgentID agentID);

	ILINE float           LeftOf(const Vec2& line, const Vec2& point) const
	{
		return line.Cross(point);
	}

	struct SConstraintLine
	{
		enum EOrigin
		{
			AgentConstraint    = 0,
			ObstacleConstraint = 1,
		};

		Vec2   direction;
		Vec2   point;

		uint16 flags;
		uint16 objectID; // debug only
	};

	struct SNearbyAgent
	{
		SNearbyAgent()
		{
		};

		SNearbyAgent(float _distanceSq, uint16 _agentID, uint16 _flags = 0)
			: distanceSq(_distanceSq)
			, agentID(_agentID)
			, flags(_flags)
		{
		};

		enum EFlags
		{
			CanSeeMe = 1 << 0,
			IsMoving = 1 << 1,
		};

		bool operator<(const SNearbyAgent& other) const
		{
			return distanceSq < other.distanceSq;
		};

		float  distanceSq;
		uint16 agentID;
		uint16 flags;
	};

	struct SNearbyObstacle
	{
		SNearbyObstacle()
			: distanceSq(0)
			, obstacleID(0)
			, flags(0)
		{
		};

		SNearbyObstacle(float _distanceSq, uint16 _agentID, uint16 _flags = 0)
			: distanceSq(_distanceSq)
			, obstacleID(_agentID)
			, flags(_flags)
		{
		};

		enum EFlags
		{
			CanSeeMe = 1 << 0,
			IsMoving = 1 << 1,
		};

		ILINE bool operator<(const SNearbyObstacle& other) const
		{
			return distanceSq < other.distanceSq;
		};

		float  distanceSq;
		uint16 obstacleID;
		uint16 flags;
	};

	struct SCandidateVelocity
	{
		float distanceSq;
		Vec2  velocity;

		ILINE bool operator<(const SCandidateVelocity& other) const
		{
			return distanceSq < other.distanceSq;
		}
	};

	struct SNavigationProperties
	{
		NavigationAgentTypeID agentTypeId;
		const INavMeshQueryFilter* pQueryFilter;
	};

	enum { FeasibleAreaMaxVertexCount = 64, };

	typedef std::vector<SNearbyAgent>    NearbyAgents;
	typedef std::vector<SNearbyObstacle> NearbyObstacles;
	typedef std::vector<SConstraintLine> ConstraintLines;

	size_t ComputeNearbyAgents(const SAgentParams& agent, size_t agentIndex, float range, NearbyAgents& nearbyAgents) const;
	size_t ComputeNearbyObstacles(const SAgentParams& agent, size_t agentIndex, float range, NearbyObstacles& nearbyObstacles) const;

	size_t ComputeConstraintLinesForAgent(const SAgentParams& agent, size_t agentIndex, float timeHorizonScale,
	                                      NearbyAgents& nearbyAgents, size_t maxAgentsConsidered, NearbyObstacles& nearbyObstacles, ConstraintLines& lines) const;
	void   ComputeAgentConstraintLine(const SAgentParams& agent, const SAgentParams& obstacleAgent, bool reciprocal, float timeHorizonScale,
	                                  SConstraintLine& line) const;
	void   ComputeObstacleConstraintLine(const SAgentParams& agent, const SObstacleParams& obstacle, float timeHorizonScale,
	                                     SConstraintLine& line) const;

	bool   ClipPolygon(const Vec2* polygon, size_t vertexCount, const SConstraintLine& line, Vec2* output,
	                   size_t* outputVertexCount) const;
	size_t ComputeFeasibleArea(const SConstraintLine* lines, size_t lineCount, float radius, Vec2* feasibleArea) const;
	bool   ClipVelocityByFeasibleArea(const Vec2& velocity, Vec2* feasibleArea, size_t vertexCount, Vec2& output) const;

	size_t ComputeOptimalAvoidanceVelocity(Vec2* feasibleArea, size_t vertexCount, const SAgentParams& agent,
	                                       const float minSpeed, const float maxSpeed, SCandidateVelocity* output) const;

	Vec2 ClampSpeedWithNavigationMesh(const SNavigationProperties& agentNavProperties, const Vec3 agentPosition, const Vec2& currentVelocity, const Vec2& velocityToClamp) const;
	bool FindFirstWalkableVelocity(AgentID agentID, SCandidateVelocity* candidates, size_t candidateCount,
	                               Vec2& output) const;

	bool FindLineCandidate(const SConstraintLine* lines, size_t lineCount, size_t lineNumber, float radius, const Vec2& velocity,
	                       Vec2& candidate) const;
	bool FindCandidate(const SConstraintLine* lines, size_t lineCount, float radius, const Vec2& velocity, Vec2& candidate) const;

	void DebugDrawConstraintLine(const Vec3& agentLocation, const SConstraintLine& line, const ColorB& color);

	typedef std::vector<ICollisionAvoidanceAgent*> RegisteredAgents;
	RegisteredAgents m_registeredAgents;

	typedef CryFixedArray<ICollisionAvoidanceAgent*, kMaxAvoidingAgents> CurrentAvoidingAgents;
	CurrentAvoidingAgents m_avoidingAgents;

	typedef std::vector<SAgentParams> Agents;
	Agents m_agents;

	typedef std::vector<Vec2> AgentAvoidanceVelocities;
	AgentAvoidanceVelocities m_agentAvoidanceVelocities;

	typedef std::vector<SObstacleParams> Obstacles;
	Obstacles       m_obstacles;

	NearbyAgents    m_nearbyAgents;
	NearbyObstacles m_nearbyObstacles;
	ConstraintLines m_constraintLines;

	std::vector<SNavigationProperties> m_agentsNavigationProperties;

	typedef std::vector<string> AgentNames;
	AgentNames m_agentNames;

	bool m_bUpdating;
};

struct ICollisionAvoidanceAgent
{
	enum class TreatType
	{
		None,
		Agent,
		Obstacle,
	};

	virtual ~ICollisionAvoidanceAgent() {}

	virtual NavigationAgentTypeID GetNavigationTypeId() const = 0;
	virtual const INavMeshQueryFilter* GetNavigationQueryFilter() const = 0;
	virtual const char*           GetName() const = 0;

	virtual TreatType             GetTreatmentType() const = 0;

	virtual void                  InitializeCollisionAgent(CCollisionAvoidanceSystem::SAgentParams& agent) const = 0;
	virtual void                  InitializeCollisionObstacle(CCollisionAvoidanceSystem::SObstacleParams& obstacle) const = 0;

	virtual void                  ApplyComputedVelocity(const Vec2& avoidanceVelocity, float updateTime) = 0;
};

#endif //__CollisionAvoidanceSystem_h__
