// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "CollisionAvoidanceSystem.h"
#include "Walkability/WalkabilityCacheManager.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"

#include "DebugDrawContext.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

CollisionAvoidanceSystem::CollisionAvoidanceSystem()
{
}

CollisionAvoidanceSystem::AgentID CollisionAvoidanceSystem::CreateAgent(tAIObjectID objectID)
{
	uint32 id = m_agents.size();
	m_agents.resize(id + 1);

	m_agentAvoidanceVelocities.resize(m_agents.size());
	m_agentAvoidanceVelocities[id].zero();

	m_agentObjectIDs.resize(m_agents.size());
	m_agentObjectIDs[id] = objectID;

	m_agentNames.resize(m_agents.size());

	IAIObject* aiObject = gAIEnv.pAIObjectManager->GetAIObject(objectID);
	m_agentNames[id] = aiObject->GetName();

	return id;
}

CollisionAvoidanceSystem::ObstacleID CollisionAvoidanceSystem::CreateObstable()
{
	uint32 id = m_obstacles.size();
	m_obstacles.resize(id + 1);

	return id;
}

void CollisionAvoidanceSystem::RemoveAgent(AgentID agentID)
{
}

void CollisionAvoidanceSystem::RemoveObstacle(ObstacleID obstacleID)
{
}

void CollisionAvoidanceSystem::SetAgent(AgentID agentID, const Agent& params)
{
	m_agents[agentID] = params;
}

const CollisionAvoidanceSystem::Agent& CollisionAvoidanceSystem::GetAgent(AgentID agentID) const
{
	return m_agents[agentID];
}

void CollisionAvoidanceSystem::SetObstacle(ObstacleID obstacleID, const Obstacle& params)
{
	m_obstacles[obstacleID] = params;
}

const CollisionAvoidanceSystem::Obstacle& CollisionAvoidanceSystem::GetObstacle(ObstacleID obstacleID) const
{
	return m_obstacles[obstacleID];
}

const Vec2& CollisionAvoidanceSystem::GetAvoidanceVelocity(AgentID agentID)
{
	return m_agentAvoidanceVelocities[agentID];
}

void CollisionAvoidanceSystem::Reset(bool bUnload)
{
	if (bUnload)
	{
		stl::free_container(m_agents);
		stl::free_container(m_agentAvoidanceVelocities);
		stl::free_container(m_obstacles);

		stl::free_container(m_agentObjectIDs);
		stl::free_container(m_agentNames);

		stl::free_container(m_constraintLines);
		stl::free_container(m_nearbyAgents);
		stl::free_container(m_nearbyObstacles);
	}
	else
	{
		m_agents.clear();
		m_agentAvoidanceVelocities.clear();
		m_obstacles.clear();

		m_agentObjectIDs.clear();
		m_agentNames.clear();
	}
}

bool CollisionAvoidanceSystem::ClipPolygon(const Vec2* polygon, size_t vertexCount, const ConstraintLine& line, Vec2* output,
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

size_t CollisionAvoidanceSystem::ComputeFeasibleArea(const ConstraintLine* lines, size_t lineCount, float radius, Vec2* feasibleArea) const
{
	Vec2 buf0[FeasibleAreaMaxVertexCount];
	Vec2* original = buf0;

	Vec2 buf1[FeasibleAreaMaxVertexCount];
	Vec2* clipped = buf1;
	Vec2* output = clipped;

	assert(3 + lineCount <= FeasibleAreaMaxVertexCount);

	const float HalfSize = 1.0f + radius;

	original[0] = Vec2(-HalfSize, HalfSize);
	original[1] = Vec2(HalfSize, HalfSize);
	original[2] = Vec2(HalfSize, -HalfSize);
	original[3] = Vec2(-HalfSize, -HalfSize);

	size_t feasibleVertexCount = 4;
	size_t outputCount = feasibleVertexCount;

	for (size_t i = 0; i < lineCount; ++i)
	{
		const ConstraintLine& constraint = lines[i];

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

bool CollisionAvoidanceSystem::ClipVelocityByFeasibleArea(const Vec2& velocity, Vec2* feasibleArea, size_t vertexCount,
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
	if (lineSegmentLengthSq < 0.00001f)
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

	if (fabs_tpl(distToCenterSq - radiusSq) < 0.00001f)
	{
		output[0] = midpt;

		return 1;
	}

	float distToIntersection;
	if (fabs_tpl(distToCenterSq) < 0.00001f)
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

size_t CollisionAvoidanceSystem::ComputeOptimalAvoidanceVelocity(Vec2* feasibleArea, size_t vertexCount, const Agent& agent,
                                                                 const float minSpeed, const float maxSpeed, CandidateVelocity* output) const
{
	const Vec2& desiredVelocity = agent.desiredVelocity;
	const Vec2& currentVelocity = agent.currentVelocity;
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

bool CollisionAvoidanceSystem::FindFirstWalkableVelocity(AgentID agentID, CandidateVelocity* candidates,
                                                         size_t candidateCount, Vec2& output) const
{
	const Agent& agent = m_agents[agentID];
	const tAIObjectID aiObjectID = m_agentObjectIDs[agentID];

	CAIObject* aiObject = gAIEnv.pObjectContainer->GetAIObject(aiObjectID);
	const CAIActor* actor = aiObject->CastToCAIActor();
	if (!actor)
		return false;

	for (size_t i = 0; i < candidateCount; ++i)
	{
		CandidateVelocity& candidate = candidates[i];

		const Vec3 from = agent.currentLocation;
		const Vec3 to = agent.currentLocation + Vec3(candidate.velocity.x * 0.125f, candidate.velocity.y * 0.125f, 0.0f);

		output = ClampSpeedWithNavigationMesh(actor->GetNavigationTypeID(), agent.currentLocation, agent.currentVelocity, candidate.velocity);
		if (output.GetLength2() < 0.1)
			continue;
		return true;
	}

	return false;
}

void CollisionAvoidanceSystem::Update(float updateTime)
{
	Agents::iterator it = m_agents.begin();
	Agents::iterator end = m_agents.end();
	size_t index = 0;

	const bool debugDraw = gAIEnv.CVars.DebugDraw > 0;
	const float Epsilon = 0.00001f;
	const size_t MaxAgentsConsidered = 8;

	for (; it != end; ++it, ++index)
	{
		Agent& agent = *it;

		Vec2& newVelocity = m_agentAvoidanceVelocities[index];
		newVelocity = agent.desiredVelocity;

		float desiredSpeedSq = agent.desiredVelocity.GetLength2();
		if (desiredSpeedSq < Epsilon)
			continue;

		m_constraintLines.clear();
		m_nearbyAgents.clear();
		m_nearbyObstacles.clear();

		const float range = gAIEnv.CVars.CollisionAvoidanceRange;

		ComputeNearbyObstacles(agent, index, range, m_nearbyObstacles);
		ComputeNearbyAgents(agent, index, range, m_nearbyAgents);

		size_t obstacleConstraintCount = ComputeConstraintLinesForAgent(agent, index, 1.0f, m_nearbyAgents, MaxAgentsConsidered,
		                                                                m_nearbyObstacles, m_constraintLines);

		size_t agentConstraintCount = m_constraintLines.size() - obstacleConstraintCount;
		size_t constraintCount = m_constraintLines.size();
		size_t considerCount = agentConstraintCount;

		if (!constraintCount)
			continue;

		Vec2 candidate = agent.desiredVelocity;
		// TODO: as a temporary solution, avoid to reset the new Velocity.
		// In this case if no ORCA speed can be found, we use our desired one
		//newVelocity.zero();

		Vec2 feasibleArea[FeasibleAreaMaxVertexCount];
		size_t vertexCount = ComputeFeasibleArea(&m_constraintLines.front(), constraintCount, agent.maxSpeed,
		                                         feasibleArea);

		float minSpeed = gAIEnv.CVars.CollisionAvoidanceMinSpeed;

		CandidateVelocity candidates[FeasibleAreaMaxVertexCount + 1]; // +1 for clipped desired velocity
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

				const NearbyAgent& furthestNearbyAgent = m_nearbyAgents[considerCount - 1];
				const Agent& furthestAgent = m_agents[furthestNearbyAgent.agentID];

				if (furthestNearbyAgent.distanceSq <= sqr(agent.radius + agent.radius + furthestAgent.radius))
					break;

				--considerCount;
				--constraintCount;
			}
		}

		if (debugDraw)
		{
			if (IAIObject* object = gAIEnv.pAIObjectManager->GetAIObject(m_agentObjectIDs[index]))
			{
				if (CAIActor* actor = object->CastToCAIActor())
				{
					if (*gAIEnv.CVars.DebugDrawCollisionAvoidanceAgentName &&
					    !stricmp(actor->GetName(), gAIEnv.CVars.DebugDrawCollisionAvoidanceAgentName))
					{
						Vec3 agentLocation = actor->GetPhysicsPos();

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
							const ConstraintLine& line = *fit;

							ColorB color = lineColor[fit->objectID % 12];

							if (line.flags & ConstraintLine::ObstacleConstraint)
								color = Col_Grey;

							DebugDrawConstraintLine(agentLocation, line, color);
						}
					}
				}
			}
		}
	}
}

size_t CollisionAvoidanceSystem::ComputeNearbyAgents(const Agent& agent, size_t agentIndex, float range,
                                                     NearbyAgents& nearbyAgents) const
{
	const float Epsilon = 0.00001f;

	Agents::const_iterator ait = m_agents.begin();
	Agents::const_iterator aend = m_agents.end();
	size_t nearbyAgentIndex = 0;

	Vec3 agentLocation = agent.currentLocation;
	Vec2 agentLookDirection = agent.currentLookDirection;

	for (; ait != aend; ++ait, ++nearbyAgentIndex)
	{
		if (agentIndex != nearbyAgentIndex)
		{
			const Agent& otherAgent = *ait;

			const Vec2 relativePosition = Vec2(otherAgent.currentLocation) - Vec2(agentLocation);
			const float distanceSq = relativePosition.GetLength2();

			const bool nearby = distanceSq < sqr(range + otherAgent.radius);
			const bool sameFloor = fabs_tpl(otherAgent.currentLocation.z - agentLocation.z) < 2.0f;

			//Note: For some reason different agents end with the same location,
			//yet the source of the problem has to be found
			const bool ignore = (distanceSq < 0.0001f);

			if (nearby && sameFloor && !ignore)
			{
				Vec2 direction = relativePosition.GetNormalized();
				bool isVisible = agentLookDirection.Dot(Vec2(otherAgent.currentLocation) - (Vec2(agentLocation) - (direction * otherAgent.radius))) > 0.0f;

				//if (isVisible)
				{
					bool isMoving = otherAgent.desiredVelocity.GetLength2() >= Epsilon;
					bool canSeeMe = true;//otherAgent.currentLookDirection.Dot(agentLocation - (otherAgent.currentLocation + (direction * agent.radius))) > 0.0f;

					nearbyAgents.push_back(NearbyAgent(distanceSq, static_cast<uint16>(nearbyAgentIndex),
					                                   (canSeeMe ? NearbyAgent::CanSeeMe : 0)
					                                   | (isMoving ? NearbyAgent::IsMoving : 0)));
				}
			}
		}
	}

	std::sort(nearbyAgents.begin(), nearbyAgents.end());

	return nearbyAgents.size();
}

size_t CollisionAvoidanceSystem::ComputeNearbyObstacles(const Agent& agent, size_t agentIndex, float range,
                                                        NearbyObstacles& nearbyObstacles) const
{
	const float Epsilon = 0.00001f;

	Obstacles::const_iterator oit = m_obstacles.begin();
	Obstacles::const_iterator oend = m_obstacles.end();
	uint16 obstacleIndex = 0;

	Vec3 agentLocation = agent.currentLocation;
	Vec2 agentLookDirection = agent.currentLookDirection;

	for (; oit != oend; ++oit, ++obstacleIndex)
	{
		const Obstacle& obstacle = *oit;

		const Vec2 relativePosition = Vec2(obstacle.currentLocation) - Vec2(agentLocation);
		const float distanceSq = relativePosition.GetLength2();

		const bool nearby = distanceSq < sqr(range + obstacle.radius);
		const bool sameFloor = fabs_tpl(obstacle.currentLocation.z - agentLocation.z) < 2.0f;

		if (nearby && sameFloor)
		{
			Vec2 direction = relativePosition.GetNormalized();

			//if (agent.currentLookDirection.Dot(relativePosition - (direction * obstacle.radius)) > 0.0f)
			nearbyObstacles.push_back(NearbyObstacle(distanceSq, obstacleIndex));
		}
	}

	return nearbyObstacles.size();
}

size_t CollisionAvoidanceSystem::ComputeConstraintLinesForAgent(const Agent& agent, size_t agentIndex, float timeHorizonScale,
                                                                NearbyAgents& nearbyAgents, size_t maxAgentsConsidered, NearbyObstacles& nearbyObstacles, ConstraintLines& lines) const
{
	const float Epsilon = 0.00001f;

	NearbyObstacles::const_iterator oit = nearbyObstacles.begin();
	NearbyObstacles::const_iterator oend = nearbyObstacles.end();

	size_t obstacleCount = 0;

	for (; oit != oend; ++oit)
	{
		const NearbyObstacle& nearbyObstacle = *oit;
		const Obstacle& obstacle = m_obstacles[nearbyObstacle.obstacleID];

		ConstraintLine line;
		line.flags = ConstraintLine::ObstacleConstraint;
		line.objectID = nearbyObstacle.obstacleID;

		ComputeObstacleConstraintLine(agent, obstacle, timeHorizonScale, line);

		lines.push_back(line);
		++obstacleCount;
	}

	NearbyAgents::const_iterator ait = nearbyAgents.begin();
	NearbyAgents::const_iterator aend = nearbyAgents.begin() + std::min<size_t>(nearbyAgents.size(), maxAgentsConsidered);

	for (; ait != aend; ++ait)
	{
		const NearbyAgent& nearbyAgent = *ait;
		const Agent& otherAgent = m_agents[nearbyAgent.agentID];

		ConstraintLine line;
		line.objectID = nearbyAgent.agentID;
		line.flags = ConstraintLine::AgentConstraint;

		if (nearbyAgent.flags & NearbyAgent::IsMoving)
			ComputeAgentConstraintLine(agent, otherAgent, true, timeHorizonScale, line);
		else
		{
			Obstacle obstacle;
			obstacle.currentLocation = otherAgent.currentLocation;
			obstacle.radius = otherAgent.radius;

			ComputeObstacleConstraintLine(agent, obstacle, timeHorizonScale, line);
		}

		lines.push_back(line);
	}

	return obstacleCount;
}

void CollisionAvoidanceSystem::ComputeObstacleConstraintLine(const Agent& agent, const Obstacle& obstacle,
                                                             float timeHorizonScale, ConstraintLine& line) const
{
	const Vec2 relativePosition = Vec2(obstacle.currentLocation) - Vec2(agent.currentLocation);

	const float distanceSq = relativePosition.GetLength2();
	const float radii = agent.radius + obstacle.radius;
	const float radiiSq = sqr(radii);

	static const float heuristicWeightForDistance = 0.01f;
	static const float minimumTimeHorizonScale = 0.25f;
	const float adjustedTimeHorizonScale = max(min(timeHorizonScale, (heuristicWeightForDistance * distanceSq)), minimumTimeHorizonScale);
	const float TimeHorizon = gAIEnv.CVars.CollisionAvoidanceObstacleTimeHorizon * adjustedTimeHorizonScale;
	const float TimeStep = gAIEnv.CVars.CollisionAvoidanceTimeStep;

	const float invTimeHorizon = 1.0f / TimeHorizon;
	const float invTimeStep = 1.0f / TimeStep;

	Vec2 u;

	const Vec2 cutoffCenter = relativePosition * invTimeHorizon;

	if (distanceSq > radiiSq)
	{
		Vec2 w = agent.desiredVelocity - cutoffCenter;
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
	else if (distanceSq > 0.00001f)
	{
		const float distance = sqrt_tpl(distanceSq);
		const Vec2 w = relativePosition / distance;

		line.direction = Vec2(-w.y, w.x);

		const Vec2 point = ((radii - distance) * invTimeStep) * w;
		const float dot = (agent.currentVelocity - point).Dot(line.direction);
		line.point = cutoffCenter + obstacle.radius * invTimeHorizon * Vec2(-line.direction.y, line.direction.x);
	}
	else
	{
		const Vec2 w = agent.currentVelocity.GetNormalizedSafe(Vec2(0.0f, 1.0f));
		line.direction = Vec2(-w.y, w.x);

		const float dot = agent.currentVelocity.Dot(line.direction);
		line.point = dot * line.direction - agent.currentVelocity;
	}
}

Vec2 CollisionAvoidanceSystem::ClampSpeedWithNavigationMesh(const NavigationAgentTypeID agentTypeID, const Vec3 agentPosition,
                                                            const Vec2& currentVelocity, const Vec2& velocityToClamp) const
{
	Vec2 outputVelocity = velocityToClamp;
	if (gAIEnv.CVars.CollisionAvoidanceClampVelocitiesWithNavigationMesh == 1)
	{
		const float TimeHorizon = 0.25f * gAIEnv.CVars.CollisionAvoidanceAgentTimeHorizon;
		const float invTimeHorizon = 1.0f / gAIEnv.CVars.CollisionAvoidanceAgentTimeHorizon;
		const float TimeStep = gAIEnv.CVars.CollisionAvoidanceTimeStep;

		const Vec3 from = agentPosition;
		const Vec3 to = agentPosition + Vec3(velocityToClamp.x, velocityToClamp.y, 0.0f);

		if (NavigationMeshID meshID = gAIEnv.pNavigationSystem->GetEnclosingMeshID(agentTypeID, from))
		{
			const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshID);
			const MNM::CNavMesh& navMesh = mesh.navMesh;

			MNM::vector3_t startLoc = MNM::vector3_t(MNM::real_t(from.x), MNM::real_t(from.y), MNM::real_t(from.z));
			MNM::vector3_t endLoc = MNM::vector3_t(MNM::real_t(to.x), MNM::real_t(to.y), MNM::real_t(to.z));

			const MNM::real_t horizontalRange(5.0f);
			const MNM::real_t verticalRange(1.0f);

			MNM::TriangleID triStart = navMesh.GetTriangleAt(startLoc, verticalRange, verticalRange);

			MNM::TriangleID triEnd = navMesh.GetTriangleAt(endLoc, verticalRange, verticalRange);
			if (!triEnd)
			{
				MNM::vector3_t closestEndLocation;
				triEnd = navMesh.GetClosestTriangle(endLoc, verticalRange, horizontalRange, nullptr, &closestEndLocation);
				navMesh.PushPointInsideTriangle(triEnd, closestEndLocation, MNM::real_t(.05f));
				endLoc = closestEndLocation;
			}

			if (triStart && triEnd)
			{
				MNM::CNavMesh::RayCastRequest<512> raycastRequest;
				MNM::CNavMesh::ERayCastResult result = navMesh.RayCast(startLoc, triStart, endLoc, triEnd, raycastRequest);
				if (result == MNM::CNavMesh::eRayCastResult_Hit)
				{
					const float velocityMagnitude = min(TimeStep, raycastRequest.hit.distance.as_float());
					const Vec3 newEndLoc = agentPosition + ((endLoc.GetVec3() - agentPosition) * velocityMagnitude);
					const Vec3 newVelocity = newEndLoc - agentPosition;
					outputVelocity = Vec2(newVelocity.x, newVelocity.y) * invTimeHorizon;
				}
				else if (result == MNM::CNavMesh::eRayCastResult_NoHit)
				{
					const Vec3 newVelocity = endLoc.GetVec3() - agentPosition;
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

void CollisionAvoidanceSystem::ComputeAgentConstraintLine(const Agent& agent, const Agent& obstacleAgent,
                                                          bool reciprocal, float timeHorizonScale, ConstraintLine& line) const
{
	const Vec2 relativePosition = Vec2(obstacleAgent.currentLocation) - Vec2(agent.currentLocation);
	const Vec2 relativeVelocity = agent.currentVelocity - obstacleAgent.currentVelocity;

	const float distanceSq = relativePosition.GetLength2();
	const float radii = agent.radius + obstacleAgent.radius;
	const float radiiSq = sqr(radii);

	const float TimeHorizon = timeHorizonScale *
	                          (reciprocal ? gAIEnv.CVars.CollisionAvoidanceAgentTimeHorizon : gAIEnv.CVars.CollisionAvoidanceObstacleTimeHorizon);
	const float TimeStep = gAIEnv.CVars.CollisionAvoidanceTimeStep;

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
	line.point = agent.currentVelocity + effort * u;
}

bool CollisionAvoidanceSystem::FindLineCandidate(const ConstraintLine* lines, size_t lineCount, size_t lineNumber, float radius,
                                                 const Vec2& velocity, Vec2& candidate) const
{
	const ConstraintLine& line = lines[lineNumber];

	const float discriminant = sqr(radius) - sqr(line.direction.Cross(line.point));

	if (discriminant < 0.0f)
		return false;

	const float discriminantSqrt = sqrt_tpl(discriminant);
	const float dot = line.direction.Dot(line.point);

	float tLeft = -dot - discriminantSqrt;
	float tRight = -dot + discriminantSqrt;

	for (size_t i = 0; i < lineNumber; ++i)
	{
		const ConstraintLine& constraint = lines[i];

		const float determinant = line.direction.Cross(constraint.direction);
		const float distanceSigned = constraint.direction.Cross(line.point - constraint.point);

		// parallel constraints
		if (fabs_tpl(determinant) < 0.00001f)
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

bool CollisionAvoidanceSystem::FindCandidate(const ConstraintLine* lines, size_t lineCount, float radius, const Vec2& velocity,
                                             Vec2& candidate) const
{
	if (velocity.GetLength2() > sqr(radius))
		candidate = velocity.GetNormalized() * radius;
	else
		candidate = velocity;

	for (size_t i = 0; i < lineCount; ++i)
	{
		const ConstraintLine& constraint = lines[i];

		if (LeftOf(constraint.direction, candidate - constraint.point) < 0.0f)
		{
			if (!FindLineCandidate(lines, lineCount, i, radius, velocity, candidate))
				return false;
		}
	}

	return true;
}

void CollisionAvoidanceSystem::DebugDrawConstraintLine(const Vec3& agentLocation, const ConstraintLine& line,
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

void CollisionAvoidanceSystem::DebugDraw()
{
	CDebugDrawContext dc;

	dc->SetBackFaceCulling(false);
	dc->SetDepthWrite(false);
	dc->SetDepthTest(false);

	Agents::iterator it = m_agents.begin();
	Agents::iterator end = m_agents.end();
	uint32 index = 0;

	ColorB desiredColor = ColorB(Col_Black, 1.0f);
	ColorB newColor = ColorB(Col_DarkGreen, 0.5f);

	for (; it != end; ++it, ++index)
	{
		Agent& agent = *it;

		if (IAIObject* object = gAIEnv.pAIObjectManager->GetAIObject(m_agentObjectIDs[index]))
		{
			if (CAIActor* actor = object->CastToCAIActor())
			{
				Vec3 agentLocation = actor->GetPhysicsPos();
				Vec2 agentAvoidanceVelocity = m_agentAvoidanceVelocities[index];

				//if ((agent.desiredVelocity - agentAvoidanceVelocity).GetLength2() > 0.000001f)
				dc->DrawArrow(agentLocation, agent.desiredVelocity, 0.135f, desiredColor);

				dc->DrawArrow(agentLocation, agentAvoidanceVelocity, 0.2f, newColor);

				dc->DrawRangeCircle(agentLocation + Vec3(0, 0, 0.3f), agent.radius, 0.1f, ColorF(0.196078f, 0.8f, 0.6f, 0.5f), ColorF(0.196078f, 0.196078f, 0.8f), true);
			}
		}
	}
}
