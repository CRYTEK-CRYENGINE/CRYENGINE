// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryAISystem/ICollisionAvoidance.h"

struct INavMeshQueryFilter;

namespace Cry
{
namespace AI
{
namespace CollisionAvoidance
{
class CCollisionAvoidanceSystem : public ISystem
{
public:
    CCollisionAvoidanceSystem();
    virtual ~CCollisionAvoidanceSystem() override {}

    void Reset();
	void Clear();
    void Update(float updateTime);

    virtual bool RegisterAgent(IAgent* pAgent) override;
    virtual bool UnregisterAgent(IAgent* pAgent) override;

    void DebugDraw();
private:
    static const size_t kMaxAvoidingAgents = 512;

    typedef size_t AgentID;
    typedef size_t ObstacleID;

    void                  PopulateState();
    void                  ApplyResults(float updateTime);

    AgentID               CreateAgent(NavigationAgentTypeID navigationTypeID, const INavMeshQueryFilter* pFilter);
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
            AgentConstraint = 0,
            ObstacleConstraint = 1,
        };

        Vec2   direction;
        Vec2   point;

        uint16 flags;
        uint16 objectID; // debug only
    };

    struct SNearbyAgent
    {
        SNearbyAgent() {}

        SNearbyAgent(float _distanceSq, uint16 _agentID, uint16 _flags = 0)
            : distanceSq(_distanceSq)
            , agentID(_agentID)
            , flags(_flags)
        {}

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
        {}

        SNearbyObstacle(float _distanceSq, uint16 _agentID, uint16 _flags = 0)
            : distanceSq(_distanceSq)
            , obstacleID(_agentID)
            , flags(_flags)
        {}

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
    void   ComputeNearbyAgents(const SAgentParams& agent, size_t agentIndex, size_t fromIndex, size_t toIndex, float range, NearbyAgents& nearbyAgents) const;

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

    Vec2 ClampSpeedWithNavigationMesh(const SNavigationProperties& agentNavProperties, const Vec3& agentPosition, const Vec3& currentVelocity, const Vec2& velocityToClamp) const;
    bool FindFirstWalkableVelocity(AgentID agentID, SCandidateVelocity* candidates, size_t candidateCount,
        Vec2& output) const;

    bool FindLineCandidate(const SConstraintLine* lines, size_t lineCount, size_t lineNumber, float radius, const Vec2& velocity,
        Vec2& candidate) const;
    bool FindCandidate(const SConstraintLine* lines, size_t lineCount, float radius, const Vec2& velocity, Vec2& candidate) const;

    void DebugDrawConstraintLine(const Vec3& agentLocation, const SConstraintLine& line, const ColorB& color);

    std::vector<IAgent*> m_registeredAgentsPtrs;
    CryFixedArray<IAgent*, kMaxAvoidingAgents> m_avoidingAgentsPtrs;
    std::vector<SAgentParams> m_agents;
    std::vector<Vec2> m_agentAvoidanceVelocities;
    std::vector<SObstacleParams> m_obstacles;

    NearbyAgents    m_nearbyAgents;
    NearbyObstacles m_nearbyObstacles;
    ConstraintLines m_constraintLines;

    std::vector<SNavigationProperties> m_agentsNavigationProperties;

    bool m_isUpdating;
};
}
}
} // Cry

