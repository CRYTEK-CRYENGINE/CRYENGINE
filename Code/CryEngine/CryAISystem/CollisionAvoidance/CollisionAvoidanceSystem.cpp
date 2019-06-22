// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CollisionAvoidanceSystem.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "Navigation/MNM/NavMeshQueryManager.h"

#include "DebugDrawContext.h"

namespace Cry
{
namespace AI
{
namespace CollisionAvoidance
{

static const float kEpsilon = 0.00001f;

CCollisionAvoidanceSystem::CCollisionAvoidanceSystem()
	: m_isUpdating(false)
{
}

bool CCollisionAvoidanceSystem::RegisterAgent(IAgent* pAgent)
{
	if (!pAgent)
	{
		CRY_ASSERT_MESSAGE(pAgent, "Parameter 'pAgent' must be non-null.");
		return false;
	}

	if (m_isUpdating)
	{
		CRY_ASSERT_MESSAGE(!m_isUpdating, "Collision Avoidance System should not be updating while an agent is being registered.");
		return false;
	}

	// The magic number '3' was already a constraint. See ComputeFeasibleArea function.
	if (m_registeredAgentsPtrs.size() + 3 >= FeasibleAreaMaxVertexCount)
	{
		// With this early return we avoid registering more agents that the maximum allowed to prevent memory corruptions in 'ComputeFeasibleArea' function.
		CRY_ASSERT_MESSAGE(m_registeredAgentsPtrs.size() + 3 < FeasibleAreaMaxVertexCount, "Maximum number of agents is 61 (value of enum 'FeasibleAreaMaxVertexCount' minus 3).");
		return false;
	}

	auto actorIt = std::find(m_registeredAgentsPtrs.begin(), m_registeredAgentsPtrs.end(), pAgent);
	if (actorIt == m_registeredAgentsPtrs.end())
	{
		m_registeredAgentsPtrs.push_back(pAgent);
	}
	return true;
}

bool CCollisionAvoidanceSystem::UnregisterAgent(IAgent* pAgent)
{
	if (pAgent)
	{
		if (!m_isUpdating)
		{
			const std::vector<IAgent*>::const_iterator it = std::remove(m_registeredAgentsPtrs.begin(), m_registeredAgentsPtrs.end(), pAgent);
			if (it != m_registeredAgentsPtrs.end())
			{
				m_registeredAgentsPtrs.erase(it, m_registeredAgentsPtrs.end());
				return true;
			}
			CRY_ASSERT_MESSAGE(it != m_registeredAgentsPtrs.end(), "Parameter 'pAgent' was not registered.");
		}
		CRY_ASSERT_MESSAGE(!m_isUpdating, "Collision Avoidance System should not be updating while an agent is being unregistered.");
	}
	CRY_ASSERT_MESSAGE(pAgent, "Parameter 'pAgent' must be non-null.");
	return false;
}

CCollisionAvoidanceSystem::AgentID CCollisionAvoidanceSystem::CreateAgent(NavigationAgentTypeID navigationTypeID, const INavMeshQueryFilter* pQueryFilter)
{
	const size_t id = m_agents.size();
	const size_t newSize = id + 1;
	m_agents.resize(newSize);

	m_agentAvoidanceVelocities.resize(newSize);
	m_agentAvoidanceVelocities[id].zero();

	m_agentsNavigationProperties.resize(newSize);
	m_agentsNavigationProperties[id].agentTypeId = navigationTypeID;
	m_agentsNavigationProperties[id].pQueryFilter = pQueryFilter;

	return id;
}

CCollisionAvoidanceSystem::ObstacleID CCollisionAvoidanceSystem::CreateObstable()
{
	uint32 id = m_obstacles.size();
	m_obstacles.resize(id + 1);

	return id;
}

void CCollisionAvoidanceSystem::SetAgent(AgentID agentID, const SAgentParams& params)
{
	m_agents[agentID] = params;
}

const SAgentParams& CCollisionAvoidanceSystem::GetAgent(AgentID agentID) const
{
	return m_agents[agentID];
}

void CCollisionAvoidanceSystem::SetObstacle(ObstacleID obstacleID, const SObstacleParams& params)
{
	m_obstacles[obstacleID] = params;
}

const SObstacleParams& CCollisionAvoidanceSystem::GetObstacle(ObstacleID obstacleID) const
{
	return m_obstacles[obstacleID];
}

const Vec2& CCollisionAvoidanceSystem::GetAvoidanceVelocity(AgentID agentID)
{
	return m_agentAvoidanceVelocities[agentID];
}

void CCollisionAvoidanceSystem::Reset()
{
	m_avoidingAgentsPtrs.clear();
	m_agents.clear();
	m_agentAvoidanceVelocities.clear();
	m_obstacles.clear();

	m_agentsNavigationProperties.clear();
}

void CCollisionAvoidanceSystem::Clear()
{
	stl::free_container(m_agents);
	stl::free_container(m_agentAvoidanceVelocities);
	stl::free_container(m_obstacles);

	stl::free_container(m_agentsNavigationProperties);

	stl::free_container(m_constraintLines);
	stl::free_container(m_nearbyAgents);
	stl::free_container(m_nearbyObstacles);
}

bool CCollisionAvoidanceSystem::ClipPolygon(const Vec2* polygon, size_t vertexCount, const SConstraintLine& line, Vec2* output,
	size_t* outputVertexCount) const
{
	bool shapeChanged = false;
	const Vec2* outputStart = output;

	Vec2 v0 = polygon[0];
	bool v0Side = line.direction.Cross(v0 - line.point) >= 0.0f;

	for (size_t i = 1; i <= vertexCount; ++i)
	{
		const Vec2 v1 = polygon[i % vertexCount];
		const bool v1Side = line.direction.Cross(v1 - line.point) >= 0.0f;

		if (v0Side && (v0Side == v1Side))
		{
			*(output++) = v1;
		}
		else if (v0Side != v1Side)
		{
			float det = line.direction.Cross(v1 - v0);

			if (fabs_tpl(det) >= 0.000001f)
			{
				shapeChanged = true;

				float detA = (v0 - line.point).Cross(v1 - v0);
				float t = detA / det;

				*(output++) = line.point + line.direction * t;
			}

			if (v1Side)
				*(output++) = v1;
		}
		else
			shapeChanged = true;

		v0 = v1;
		v0Side = v1Side;
	}

	*outputVertexCount = static_cast<size_t>(output - outputStart);

	return shapeChanged;
}

size_t CCollisionAvoidanceSystem::ComputeFeasibleArea(const SConstraintLine* lines, size_t lineCount, float radius, Vec2* feasibleArea) const
{
	Vec2 buf0[FeasibleAreaMaxVertexCount];
	Vec2* original = buf0;

	Vec2 buf1[FeasibleAreaMaxVertexCount];
	Vec2* clipped = buf1;
	Vec2* output = clipped;

	CRY_ASSERT_MESSAGE(3 + lineCount <= FeasibleAreaMaxVertexCount, "More agents/obstacles than the maximum supported are being handled. This will cause memory buffers to go beyond their limit, possibly corrupting memory. This shouldn't happen since the system should not allow to register more agents than the maximum which is equivalent to 'FeasibleAreaMaxVertexCount' - 3. See RegisterAgent function.");

	const float HalfSize = 1.0f + radius;

	original[0] = Vec2(-HalfSize, HalfSize);
	original[1] = Vec2(HalfSize, HalfSize);
	original[2] = Vec2(HalfSize, -HalfSize);
	original[3] = Vec2(-HalfSize, -HalfSize);

	size_t feasibleVertexCount = 4;
	size_t outputCount = feasibleVertexCount;

	for (size_t i = 0; i < lineCount; ++i)
	{
		const SConstraintLine& constraint = lines[i];

		if (ClipPolygon(original, outputCount, constraint, clipped, &outputCount))
		{
			if (outputCount)
			{
				output = clipped;
				std::swap(original, clipped);
			}
			else
				return 0;
		}
	}

	PREFAST_SUPPRESS_WARNING(6385)
		memcpy(feasibleArea, output, outputCount * sizeof(Vec2));

	return outputCount;
}

bool CCollisionAvoidanceSystem::ClipVelocityByFeasibleArea(const Vec2& velocity, Vec2* feasibleArea, size_t vertexCount,
	Vec2& output) const
{
	if (Overlap::Point_Polygon2D(velocity, feasibleArea, vertexCount))
	{
		output = velocity;

		return false;
	}

	for (size_t i = 0; i < vertexCount; ++i)
	{
		const Vec2 v0 = feasibleArea[i];
		const Vec2 v1 = feasibleArea[(i + 1) % vertexCount];

		float a, b;
		if (Intersect::Lineseg_Lineseg2D(Lineseg(v0, v1), Lineseg(ZERO, velocity), a, b))
		{
			output = velocity * b;

			return true;
		}
	}

	output.zero();

	return true;
}

size_t IntersectLineSegCircle(const Vec2& center, float radius, const Vec2& a, const Vec2& b, Vec2* output)
{
	const Vec2 v1 = b - a;
	const float lineSegmentLengthSq = v1.GetLength2();
	if (lineSegmentLengthSq < kEpsilon)
		return 0;

	const Vec2 v2 = center - a;
	const float radiusSq = sqr(radius);

	const float dot = v1.Dot(v2);
	const float dotProdOverLength = (dot / lineSegmentLengthSq);
	const Vec2 proj1 = Vec2((dotProdOverLength * v1.x), (dotProdOverLength * v1.y));
	const Vec2 midpt = Vec2(a.x + proj1.x, a.y + proj1.y);

	float distToCenterSq = sqr(midpt.x - center.x) + sqr(midpt.y - center.y);
	if (distToCenterSq > radiusSq)
		return 0;

	if (fabs_tpl(distToCenterSq - radiusSq) < kEpsilon)
	{
		output[0] = midpt;

		return 1;
	}

	float distToIntersection;
	if (fabs_tpl(distToCenterSq) < kEpsilon)
		distToIntersection = radius;
	else
	{
		distToIntersection = sqrt_tpl(radiusSq - distToCenterSq);
	}

	const Vec2 vIntersect = v1.GetNormalized() * distToIntersection;

	size_t resultCount = 0;
	const Vec2 solution1 = midpt + vIntersect;
	if ((solution1.x - a.x) * vIntersect.x + (solution1.y - a.y) * vIntersect.y > 0.0f &&
		(solution1.x - b.x) * vIntersect.x + (solution1.y - b.y) * vIntersect.y < 0.0f)
	{
		output[resultCount++] = solution1;
	}

	const Vec2 solution2 = midpt - vIntersect;
	if ((solution2.x - a.x) * vIntersect.x + (solution2.y - a.y) * vIntersect.y > 0.0f &&
		(solution2.x - b.x) * vIntersect.x + (solution2.y - b.y) * vIntersect.y < 0.0f)
	{
		output[resultCount++] = solution2;
	}

	return resultCount;
}

size_t CCollisionAvoidanceSystem::ComputeOptimalAvoidanceVelocity(Vec2* feasibleArea, size_t vertexCount, const SAgentParams& agent,
	const float minSpeed, const float maxSpeed, SCandidateVelocity* output) const
{
	const Vec2& desiredVelocity = agent.desiredVelocity;
	if (vertexCount > 2)
	{
		Vec2 velocity;

		if (!ClipVelocityByFeasibleArea(desiredVelocity, feasibleArea, vertexCount, velocity))
		{
			output[0].velocity = desiredVelocity;
			output[0].distanceSq = 0.0f;

			return 1;
		}
		else
		{
			const float MinSpeedSq = sqr(minSpeed);
			const float MaxSpeedSq = sqr(maxSpeed);

			size_t candidateCount = 0;

			float clippedSpeedSq = velocity.GetLength2();

			if (clippedSpeedSq > MinSpeedSq)
			{
				output[candidateCount].velocity = velocity;
				output[candidateCount++].distanceSq = (velocity - desiredVelocity).GetLength2();
			}

			Vec2 intersections[2];
			size_t intersectionCount = 0;
			const Vec2 center(ZERO);

			for (size_t i = 0; i < vertexCount; ++i)
			{
				const Vec2& a = feasibleArea[i];
				const Vec2& b = feasibleArea[(i + 1) % vertexCount];

				const float aLenSq = a.GetLength2();
				if ((aLenSq <= MaxSpeedSq) && (aLenSq > .0f))
				{
					output[candidateCount].velocity = a;
					output[candidateCount++].distanceSq = (a - desiredVelocity).GetLength2();
				}

				intersectionCount = IntersectLineSegCircle(center, maxSpeed, a, b, intersections);

				for (size_t ii = 0; ii < intersectionCount; ++ii)
				{
					const Vec2 candidateVelocity = intersections[ii];

					if ((candidateVelocity.GetLength2() > MinSpeedSq) /*&& (candidateVelocity.Dot(desiredVelocity) > 0.0f)*/)
					{
						output[candidateCount].velocity = candidateVelocity;
						output[candidateCount++].distanceSq = (candidateVelocity - desiredVelocity).GetLength2();
					}
				}
			}

			if (candidateCount)
			{
				std::sort(&output[0], &output[0] + candidateCount);

				return candidateCount;
			}
		}
	}

	return 0;
}

bool CCollisionAvoidanceSystem::FindFirstWalkableVelocity(AgentID agentID, SCandidateVelocity* candidates,
	size_t candidateCount, Vec2& output) const
{
	const SAgentParams& agent = m_agents[agentID];

	for (size_t i = 0; i < candidateCount; ++i)
	{
		SCandidateVelocity& candidate = candidates[i];

		output = ClampSpeedWithNavigationMesh(m_agentsNavigationProperties[agentID], agent.currentLocation, agent.currentVelocity, candidate.velocity);
		if (output.GetLength2() < 0.1)
			continue;
		return true;
	}

	return false;
}

void CCollisionAvoidanceSystem::PopulateState()
{
	Reset();

	static_assert(int(ETreatType::Count) == 3, "Unexpected enum count!");
	for (IAgent* pAgent : m_registeredAgentsPtrs)
	{
		SAgentParams agentParams;
		SObstacleParams obstacleParams;

		switch (pAgent->GetTreatmentDuringUpdateTick(agentParams, obstacleParams))
		{
		case ETreatType::Agent:
		{
			if (m_avoidingAgentsPtrs.size() < m_avoidingAgentsPtrs.max_size())
			{
				CCollisionAvoidanceSystem::AgentID agentID = CreateAgent(pAgent->GetNavigationTypeId(), pAgent->GetNavigationQueryFilter());
				SetAgent(agentID, agentParams);

				m_avoidingAgentsPtrs.push_back(pAgent);
			}
			break;
		}
		case ETreatType::Obstacle:
		{
			CCollisionAvoidanceSystem::ObstacleID obstacleID = CreateObstable();
			SetObstacle(obstacleID, obstacleParams);
			break;
		}
		}
	}
}

void CCollisionAvoidanceSystem::ApplyResults(float updateTime)
{
	if ((gAIEnv.CVars.collisionAvoidance.CollisionAvoidanceUpdateVelocities > 0) || (gAIEnv.CVars.collisionAvoidance.CollisionAvoidanceEnableRadiusIncrement > 0))
	{
		for (size_t i = 0, count = m_avoidingAgentsPtrs.size(); i < count; ++i)
		{
			const Vec2 avoidanceVelocity = GetAvoidanceVelocity(i);
			m_avoidingAgentsPtrs[i]->ApplyComputedVelocity(avoidanceVelocity, updateTime);
		}
	}
}

void CCollisionAvoidanceSystem::Update(float updateTime)
{
	m_isUpdating = true;

	PopulateState();

	const bool debugDraw = gAIEnv.CVars.DebugDraw > 0;
	const float Epsilon = kEpsilon;
	const size_t MaxAgentsConsidered = 8;

	for (size_t index = 0, size = m_agents.size(); index < size; ++index)
	{
		const SAgentParams& agent = m_agents[index];

		Vec2& newVelocity = m_agentAvoidanceVelocities[index];
		newVelocity = Vec2(agent.desiredVelocity);

		float desiredSpeedSq = agent.desiredVelocity.GetLengthSquared2D();
		if (desiredSpeedSq < Epsilon)
			continue;

		m_constraintLines.clear();
		m_nearbyAgents.clear();
		m_nearbyObstacles.clear();

		const float range = gAIEnv.CVars.collisionAvoidance.CollisionAvoidanceRange;

		ComputeNearbyObstacles(agent, index, range, m_nearbyObstacles);
		ComputeNearbyAgents(agent, index, range, m_nearbyAgents);

		size_t obstacleConstraintCount = ComputeConstraintLinesForAgent(agent, index, 1.0f, m_nearbyAgents, MaxAgentsConsidered,
			m_nearbyObstacles, m_constraintLines);

		size_t agentConstraintCount = m_constraintLines.size() - obstacleConstraintCount;
		size_t constraintCount = m_constraintLines.size();
		size_t considerCount = agentConstraintCount;

		if (!constraintCount)
			continue;

		// TODO: as a temporary solution, avoid to reset the new Velocity.
		// In this case if no ORCA speed can be found, we use our desired one
		//newVelocity.zero();

		Vec2 feasibleArea[FeasibleAreaMaxVertexCount];
		size_t vertexCount = ComputeFeasibleArea(&m_constraintLines.front(), constraintCount, agent.maxSpeed,
			feasibleArea);

		float minSpeed = gAIEnv.CVars.collisionAvoidance.CollisionAvoidanceMinSpeed;

		SCandidateVelocity candidates[FeasibleAreaMaxVertexCount + 1]; // +1 for clipped desired velocity
		size_t candidateCount = ComputeOptimalAvoidanceVelocity(feasibleArea, vertexCount, agent, minSpeed, agent.maxSpeed, &candidates[0]);

		if (!candidateCount || !FindFirstWalkableVelocity(index, candidates, candidateCount, newVelocity))
		{
			m_constraintLines.clear();

			obstacleConstraintCount = ComputeConstraintLinesForAgent(agent, index, 0.25f, m_nearbyAgents, considerCount,
				m_nearbyObstacles, m_constraintLines);

			agentConstraintCount = m_constraintLines.size() - obstacleConstraintCount;
			constraintCount = m_constraintLines.size();

			while (considerCount > 0)
			{
				vertexCount = ComputeFeasibleArea(&m_constraintLines.front(), m_constraintLines.size(), agent.maxSpeed,
					feasibleArea);

				candidateCount = ComputeOptimalAvoidanceVelocity(feasibleArea, vertexCount, agent, minSpeed, agent.maxSpeed, &candidates[0]);

				if (candidateCount && !FindFirstWalkableVelocity(index, candidates, candidateCount, newVelocity))
					break;

				if (m_nearbyAgents.empty())
					break;

				const SNearbyAgent& furthestNearbyAgent = m_nearbyAgents[considerCount - 1];
				const SAgentParams& furthestAgent = m_agents[furthestNearbyAgent.agentID];

				if (furthestNearbyAgent.distanceSq <= sqr(agent.radius + agent.radius + furthestAgent.radius))
					break;

				--considerCount;
				--constraintCount;
			}
		}

		if (debugDraw)
		{
			const char* szAgentName = m_avoidingAgentsPtrs[index]->GetDebugName();
			if(*gAIEnv.CVars.collisionAvoidance.DebugDrawCollisionAvoidanceAgentName && szAgentName
				&& !stricmp(szAgentName, gAIEnv.CVars.collisionAvoidance.DebugDrawCollisionAvoidanceAgentName))
			{
				const Vec3 agentLocation = agent.currentLocation;

				CDebugDrawContext dc;

				dc->DrawCircleOutline(agentLocation, agent.maxSpeed, Col_Blue);

				dc->SetBackFaceCulling(false);
				dc->SetAlphaBlended(true);

				Vec3 polygon3D[128];

				for (size_t i = 0; i < vertexCount; ++i)
					polygon3D[i] = Vec3(agentLocation.x + feasibleArea[i].x, agentLocation.y + feasibleArea[i].y,
						agentLocation.z + 0.005f);

				ColorB polyColor(255, 255, 255, 128);
				polyColor.a = 96;

				for (size_t i = 2; i < vertexCount; ++i)
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(polygon3D[0], polyColor, polygon3D[i - 1], polyColor,
						polygon3D[i], polyColor);

				ConstraintLines::iterator fit = m_constraintLines.begin();
				ConstraintLines::iterator fend = m_constraintLines.begin() + constraintCount;

				ColorB lineColor[12] = {
					ColorB(Col_Orange,        0.5f),
					ColorB(Col_Tan,           0.5f),
					ColorB(Col_NavyBlue,      0.5f),
					ColorB(Col_Green,         0.5f),
					ColorB(Col_BlueViolet,    0.5f),
					ColorB(Col_IndianRed,     0.5f),
					ColorB(Col_ForestGreen,   0.5f),
					ColorB(Col_DarkSlateGrey, 0.5f),
					ColorB(Col_Turquoise,     0.5f),
					ColorB(Col_Gold,          0.5f),
					ColorB(Col_Khaki,         0.5f),
					ColorB(Col_CadetBlue,     0.5f),
				};

				for (; fit != fend; ++fit)
				{
					const SConstraintLine& line = *fit;

					ColorB color = lineColor[fit->objectID % 12];

					if (line.flags & SConstraintLine::ObstacleConstraint)
						color = Col_Grey;

					DebugDrawConstraintLine(agentLocation, line, color);
				}
			}
		}
	}

	ApplyResults(updateTime);

	m_isUpdating = false;
}

size_t CCollisionAvoidanceSystem::ComputeNearbyAgents(const SAgentParams& agent, size_t agentIndex, float range, NearbyAgents& nearbyAgents) const
{
	ComputeNearbyAgents(agent, agentIndex, 0, agentIndex, range, nearbyAgents);
	ComputeNearbyAgents(agent, agentIndex, agentIndex + 1, m_agents.size(), range, nearbyAgents);

	std::sort(nearbyAgents.begin(), nearbyAgents.end());

	return nearbyAgents.size();
}

void CCollisionAvoidanceSystem::ComputeNearbyAgents(const SAgentParams& agent, size_t agentIndex, size_t fromIndex, size_t toIndex, float range, NearbyAgents& nearbyAgents) const
{
	CRY_ASSERT(toIndex <= m_agents.size());

	const Vec3 agentLocation = agent.currentLocation;

	for (size_t idx = fromIndex; idx < toIndex; ++idx)
	{
		const SAgentParams& otherAgent = m_agents[idx];

		const Vec2 relativePosition = Vec2(otherAgent.currentLocation) - Vec2(agentLocation);
		const float distanceSq = relativePosition.GetLength2();

		const float bottomMax = max(agentLocation.z, otherAgent.currentLocation.z);
		const float topMin = min(agentLocation.z + agent.height, otherAgent.currentLocation.z + otherAgent.height);
		const bool sameFloor = bottomMax <= topMin;
		const bool nearby = distanceSq < sqr(range + otherAgent.radius);

		//Note: For some reason different agents end with the same location,
		//yet the source of the problem has to be found
		const bool ignore = (distanceSq < 0.0001f);

		if (nearby && sameFloor && !ignore)
		{
			const uint16 flags = otherAgent.desiredVelocity.GetLengthSquared2D() >= kEpsilon ? SNearbyAgent::IsMoving : 0;
			nearbyAgents.emplace_back(distanceSq, idx, flags);
		}
	}
}

size_t CCollisionAvoidanceSystem::ComputeNearbyObstacles(const SAgentParams& agent, size_t agentIndex, float range,
	NearbyObstacles& nearbyObstacles) const
{
	const Vec2 agentLocation2D = agent.currentLocation;
	const float agentHeightPos = agent.currentLocation.z;

	for (uint16 obstacleIndex = 0; obstacleIndex < m_obstacles.size(); ++obstacleIndex)
	{
		const SObstacleParams& obstacle = m_obstacles[obstacleIndex];
		
		const Vec2 relativePosition = Vec2(obstacle.currentLocation) - agentLocation2D;
		const float distanceSq = relativePosition.GetLength2();

		const float bottomMax = max(agentHeightPos, obstacle.currentLocation.z);
		const float topMin = min(agentHeightPos + agent.height, obstacle.currentLocation.z + obstacle.height);
		const bool sameFloor = bottomMax <= topMin;
		const bool nearby = distanceSq < sqr(range + obstacle.radius);

		if (nearby && sameFloor)
		{
			nearbyObstacles.emplace_back(distanceSq, obstacleIndex);
		}
	}

	return nearbyObstacles.size();
}

size_t CCollisionAvoidanceSystem::ComputeConstraintLinesForAgent(const SAgentParams& agent, size_t agentIndex, float timeHorizonScale,
	NearbyAgents& nearbyAgents, size_t maxAgentsConsidered, NearbyObstacles& nearbyObstacles, ConstraintLines& lines) const
{
	size_t obstacleCount = 0;

	for (const SNearbyObstacle& nearbyObstacle : nearbyObstacles)
	{
		const SObstacleParams& obstacle = m_obstacles[nearbyObstacle.obstacleID];

		SConstraintLine line;
		line.flags = SConstraintLine::ObstacleConstraint;
		line.objectID = nearbyObstacle.obstacleID;

		ComputeObstacleConstraintLine(agent, obstacle, timeHorizonScale, line);

		lines.push_back(line);
		++obstacleCount;
	}

	NearbyAgents::const_iterator ait = nearbyAgents.begin();
	NearbyAgents::const_iterator aend = nearbyAgents.begin() + std::min<size_t>(nearbyAgents.size(), maxAgentsConsidered);

	for (; ait != aend; ++ait)
	{
		const SNearbyAgent& nearbyAgent = *ait;
		const SAgentParams& otherAgent = m_agents[nearbyAgent.agentID];

		SConstraintLine line;
		line.objectID = nearbyAgent.agentID;
		line.flags = SConstraintLine::AgentConstraint;

		if (nearbyAgent.flags & SNearbyAgent::IsMoving)
		{
			ComputeAgentConstraintLine(agent, otherAgent, true, timeHorizonScale, line);
		}
		else
		{
			SObstacleParams obstacle;
			obstacle.currentLocation = otherAgent.currentLocation;
			obstacle.radius = otherAgent.radius;

			ComputeObstacleConstraintLine(agent, obstacle, timeHorizonScale, line);
		}

		lines.push_back(line);
	}

	return obstacleCount;
}

void CCollisionAvoidanceSystem::ComputeObstacleConstraintLine(const SAgentParams& agent, const SObstacleParams& obstacle,
	float timeHorizonScale, SConstraintLine& line) const
{
	const Vec2 relativePosition = Vec2(obstacle.currentLocation) - Vec2(agent.currentLocation);

	const float distanceSq = relativePosition.GetLength2();
	const float radii = agent.radius + obstacle.radius;
	const float radiiSq = sqr(radii);

	static const float heuristicWeightForDistance = 0.01f;
	static const float minimumTimeHorizonScale = 0.25f;
	const float adjustedTimeHorizonScale = max(min(timeHorizonScale, (heuristicWeightForDistance * distanceSq)), minimumTimeHorizonScale);
	const float TimeHorizon = gAIEnv.CVars.collisionAvoidance.CollisionAvoidanceObstacleTimeHorizon * adjustedTimeHorizonScale;
	const float invTimeHorizon = 1.0f / TimeHorizon;

	Vec2 u;

	const Vec2 cutoffCenter = relativePosition * invTimeHorizon;

	if (distanceSq > radiiSq)
	{
		Vec2 w = Vec2(agent.desiredVelocity) - cutoffCenter;
		const float wLenSq = w.GetLength2();

		const float dot = w.Dot(relativePosition);

		// compute closest point from relativeVelocity to the velocity object boundary
		if ((dot < 0.0f) && (sqr(dot) > radiiSq * wLenSq))
		{
			// w is pointing backwards from cone apex direction
			// closest point lies on the cutoff arc
			const float wLen = sqrt_tpl(wLenSq);
			w /= wLen;

			line.direction = Vec2(w.y, -w.x);
			line.point = cutoffCenter + (radii * invTimeHorizon) * w;
		}
		else
		{
			// w is pointing into the cone
			// closest point is on an edge
			const float edge = sqrt_tpl(distanceSq - radiiSq);

			if (LeftOf(relativePosition, w) > 0.0f)
			{
				// left edge
				line.direction = Vec2(relativePosition.x * edge - relativePosition.y * radii,
					relativePosition.x * radii + relativePosition.y * edge) / distanceSq;
			}
			else
			{
				// right edge
				line.direction = -Vec2(relativePosition.x * edge + relativePosition.y * radii,
					-relativePosition.x * radii + relativePosition.y * edge) / distanceSq;
			}

			line.point = cutoffCenter + radii * invTimeHorizon * Vec2(-line.direction.y, line.direction.x);
		}
	}
	else if (distanceSq > kEpsilon)
	{
		const float distance = sqrt_tpl(distanceSq);
		const Vec2 w = relativePosition / distance;

		line.direction = Vec2(-w.y, w.x);
		line.point = cutoffCenter + obstacle.radius * invTimeHorizon * Vec2(-line.direction.y, line.direction.x);
	}
	else
	{
		const Vec2 currentVelocity2D = Vec2(agent.currentVelocity);
		
		const Vec2 w = currentVelocity2D.GetNormalizedSafe(Vec2(0.0f, 1.0f));
		line.direction = Vec2(-w.y, w.x);

		const float dot = currentVelocity2D.Dot(line.direction);
		line.point = dot * line.direction - currentVelocity2D;
	}
}

Vec2 CCollisionAvoidanceSystem::ClampSpeedWithNavigationMesh(const SNavigationProperties& agentNavProperties, const Vec3& agentPosition,
	const Vec3& currentVelocity, const Vec2& velocityToClamp) const
{
	Vec2 outputVelocity = velocityToClamp;
	if (gAIEnv.CVars.collisionAvoidance.CollisionAvoidanceClampVelocitiesWithNavigationMesh == 1)
	{
		const float invTimeHorizon = 1.0f / gAIEnv.CVars.collisionAvoidance.CollisionAvoidanceAgentTimeHorizon;
		const float TimeStep = gAIEnv.CVars.collisionAvoidance.CollisionAvoidanceTimeStep;

		const Vec3 from = agentPosition;
		const Vec3 to = agentPosition + Vec3(velocityToClamp.x, velocityToClamp.y, 0.0f);

		if (NavigationMeshID meshID = gAIEnv.pNavigationSystem->GetEnclosingMeshID(agentNavProperties.agentTypeId, from))
		{
			const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshID);
			const MNM::CNavMesh& navMesh = mesh.navMesh;

			const MNM::vector3_t startMeshLoc = navMesh.ToMeshSpace(from);
			MNM::vector3_t endMeshLoc = navMesh.ToMeshSpace(to);

			const MNM::real_t horizontalRange(5.0f);
			const MNM::real_t verticalRange(1.0f);

			const INavMeshQueryFilter* pFilter = agentNavProperties.pQueryFilter;

			const MNM::TriangleID triStart = navMesh.QueryTriangleAt(
				startMeshLoc,
				verticalRange,
				verticalRange,
				MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial,
				pFilter
			);

			MNM::TriangleID triEnd = navMesh.QueryTriangleAt(
				endMeshLoc,
				verticalRange,
				verticalRange,
				MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial,
				pFilter
			);

			if (!triEnd)
			{
				MNM::vector3_t closestEndLocation;
				const MNM::aabb_t localAabb(MNM::vector3_t(-horizontalRange, -horizontalRange, -verticalRange), MNM::vector3_t(horizontalRange, horizontalRange, verticalRange));
				const MNM::SClosestTriangle closestTriangle = navMesh.QueryClosestTriangle(endMeshLoc, localAabb, MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial, MNM::real_t::max(), pFilter);
				closestEndLocation = closestTriangle.position;
				triEnd = closestTriangle.id;
				navMesh.PushPointInsideTriangle(triEnd, closestEndLocation, MNM::real_t(.05f));
				endMeshLoc = closestEndLocation;
			}

			if (triStart && triEnd)
			{
				MNM::CNavMesh::RayCastRequest<512> raycastRequest;
				MNM::ERayCastResult result = navMesh.RayCast(startMeshLoc, triStart, endMeshLoc, triEnd, raycastRequest, pFilter);
				if (result == MNM::ERayCastResult::Hit)
				{
					const float velocityMagnitude = min(TimeStep, raycastRequest.hit.distance.as_float());
					const Vec3 newEndLoc = agentPosition + (navMesh.ToWorldSpace(endMeshLoc).GetVec3() - agentPosition) * velocityMagnitude;
					const Vec3 newVelocity = newEndLoc - agentPosition;
					outputVelocity = Vec2(newVelocity.x, newVelocity.y) * invTimeHorizon;
				}
				else if (result == MNM::ERayCastResult::NoHit)
				{
					const Vec3 newVelocity = navMesh.ToWorldSpace(endMeshLoc).GetVec3() - agentPosition;
					outputVelocity = Vec2(newVelocity.x, newVelocity.y);
				}
				else
				{
					assert(0);
				}
			}
		}
	}
	return outputVelocity;
}

void CCollisionAvoidanceSystem::ComputeAgentConstraintLine(const SAgentParams& agent, const SAgentParams& obstacleAgent,
	bool reciprocal, float timeHorizonScale, SConstraintLine& line) const
{
	const Vec2 agentCurrentVelocity(agent.currentVelocity);
	
	const Vec2 relativePosition = Vec2(obstacleAgent.currentLocation) - Vec2(agent.currentLocation);
	const Vec2 relativeVelocity = agentCurrentVelocity - Vec2(obstacleAgent.currentVelocity);

	const float distanceSq = relativePosition.GetLength2();
	const float radii = agent.radius + obstacleAgent.radius;
	const float radiiSq = sqr(radii);

	const float TimeHorizon = timeHorizonScale *
		(reciprocal ? gAIEnv.CVars.collisionAvoidance.CollisionAvoidanceAgentTimeHorizon : gAIEnv.CVars.collisionAvoidance.CollisionAvoidanceObstacleTimeHorizon);
	const float TimeStep = gAIEnv.CVars.collisionAvoidance.CollisionAvoidanceTimeStep;

	const float invTimeHorizon = 1.0f / TimeHorizon;
	const float invTimeStep = 1.0f / TimeStep;

	Vec2 u;

	if (distanceSq > radiiSq)
	{
		const Vec2 cutoffCenter = relativePosition * invTimeHorizon;

		Vec2 w = relativeVelocity - cutoffCenter;
		const float wLenSq = w.GetLength2();

		const float dot = w.Dot(relativePosition);

		// compute closest point from relativeVelocity to the velocity object boundary
		if ((dot < 0.0f) && (sqr(dot) > radiiSq * wLenSq))
		{
			// w is pointing backwards from cone apex direction
			// closest point lies on the cutoff arc
			const float wLen = sqrt_tpl(wLenSq);
			w /= wLen;

			line.direction = Vec2(w.y, -w.x);
			u = (radii * invTimeHorizon - wLen) * w;
		}
		else
		{
			// w is pointing into the cone
			// closest point is on an edge
			const float edge = sqrt_tpl(distanceSq - radiiSq);

			if (LeftOf(relativePosition, w) > 0.0f)
			{
				// left edge
				line.direction = Vec2(relativePosition.x * edge - relativePosition.y * radii,
					relativePosition.x * radii + relativePosition.y * edge) / distanceSq;
			}
			else
			{
				// right edge
				line.direction = -Vec2(relativePosition.x * edge + relativePosition.y * radii,
					-relativePosition.x * radii + relativePosition.y * edge) / distanceSq;
			}

			const float proj = relativeVelocity.Dot(line.direction);

			u = proj * line.direction - relativeVelocity;
		}
	}
	else
	{
		const float distance = sqrt_tpl(distanceSq);
		const Vec2 w = relativePosition / distance;

		line.direction = Vec2(-w.y, w.x);

		const Vec2 point = ((distance - radii) * invTimeStep) * w;
		const float dot = (relativeVelocity - point).Dot(line.direction);

		u = point + dot * line.direction - relativeVelocity;
	}

	const float effort = reciprocal ? 0.5f : 1.0f;
	line.point = agentCurrentVelocity + effort * u;
}

bool CCollisionAvoidanceSystem::FindLineCandidate(const SConstraintLine* lines, size_t lineCount, size_t lineNumber, float radius,
	const Vec2& velocity, Vec2& candidate) const
{
	const SConstraintLine& line = lines[lineNumber];

	const float discriminant = sqr(radius) - sqr(line.direction.Cross(line.point));

	if (discriminant < 0.0f)
		return false;

	const float discriminantSqrt = sqrt_tpl(discriminant);
	const float dot = line.direction.Dot(line.point);

	float tLeft = -dot - discriminantSqrt;
	float tRight = -dot + discriminantSqrt;

	for (size_t i = 0; i < lineNumber; ++i)
	{
		const SConstraintLine& constraint = lines[i];

		const float determinant = line.direction.Cross(constraint.direction);
		const float distanceSigned = constraint.direction.Cross(line.point - constraint.point);

		// parallel constraints
		if (fabs_tpl(determinant) < kEpsilon)
		{
			if (distanceSigned < 0.0f)
				return false;
			else
				continue; // no constraint
		}

		const float t = distanceSigned / determinant;

		if (determinant > 0.0f)
			tRight = min(t, tRight);
		else
			tLeft = max(t, tLeft);

		if (tLeft > tRight)
			return false;
	}

	const float t = line.direction.Dot(velocity - line.point);

	if (t < tLeft)
		candidate = line.point + tLeft * line.direction;
	else if (t > tRight)
		candidate = line.point + tRight * line.direction;
	else
		candidate = line.point + t * line.direction;

	return true;
}

bool CCollisionAvoidanceSystem::FindCandidate(const SConstraintLine* lines, size_t lineCount, float radius, const Vec2& velocity,
	Vec2& candidate) const
{
	if (velocity.GetLength2() > sqr(radius))
		candidate = velocity.GetNormalized() * radius;
	else
		candidate = velocity;

	for (size_t i = 0; i < lineCount; ++i)
	{
		const SConstraintLine& constraint = lines[i];

		if (LeftOf(constraint.direction, candidate - constraint.point) < 0.0f)
		{
			if (!FindLineCandidate(lines, lineCount, i, radius, velocity, candidate))
				return false;
		}
	}

	return true;
}

void CCollisionAvoidanceSystem::DebugDrawConstraintLine(const Vec3& agentLocation, const SConstraintLine& line,
	const ColorB& color)
{
	CDebugDrawContext dc;

	dc->SetBackFaceCulling(false);
	dc->SetDepthWrite(false);

	Vec3 v1 = agentLocation + line.point;
	Vec3 v0 = v1 - line.direction * 10.0f;
	Vec3 v2 = v1 + line.direction * 10.0f;

	dc->DrawLine(v0, color, v2, color, 5.0f);
	dc->DrawArrow(v1, Vec2(-line.direction.y, line.direction.x) * 0.35f, 0.095f, color);
}

void CCollisionAvoidanceSystem::DebugDraw()
{
	CDebugDrawContext dc;

	dc->SetBackFaceCulling(false);
	dc->SetDepthWrite(false);
	dc->SetDepthTest(false);

	for (const SObstacleParams& obstacle : m_obstacles)
	{
		dc->DrawRangeCircle(obstacle.currentLocation + Vec3(0, 0, 0.3f), obstacle.radius, 0.1f, ColorF(0.8f, 0.196078f, 0.6f, 0.5f), ColorF(0.8f, 0.196078f, 0.196078f), true);
	}

	ColorB desiredColor = ColorB(Col_Black, 1.0f);
	ColorB newColor = ColorB(Col_DarkGreen, 0.5f);

	for (size_t index = 0; index < m_agents.size(); ++index)
	{
		const SAgentParams& agent = m_agents[index];

		const Vec3 agentLocation = agent.currentLocation;
		const Vec2 agentAvoidanceVelocity = m_agentAvoidanceVelocities[index];

		dc->DrawArrow(agentLocation, agent.desiredVelocity, 0.135f, desiredColor);
		dc->DrawArrow(agentLocation, agentAvoidanceVelocity, 0.2f, newColor);

		dc->DrawRangeCircle(agentLocation + Vec3(0, 0, 0.3f), agent.radius, 0.1f, ColorF(0.196078f, 0.8f, 0.6f, 0.5f), ColorF(0.196078f, 0.196078f, 0.8f), true);
	}
}

} // namespace CollisionAvoidance
} // namespace AI
} // namespace Cry
