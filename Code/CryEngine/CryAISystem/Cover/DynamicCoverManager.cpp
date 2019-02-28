// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DynamicCoverManager.h"
#include "CoverSystem.h"

const bool DynamicCoverDeferred = false;

void DynamicCoverManager::OnEntityEvent(IEntity* entity, const SEntityEvent& event)
{
	assert(m_entityCover.find(entity->GetId()) != m_entityCover.end());
	assert((event.event == ENTITY_EVENT_XFORM) || (event.event == ENTITY_EVENT_DONE));

	EntityId entityID = entity->GetId();

	if (event.event == ENTITY_EVENT_XFORM)
		MoveEvent(entityID, entity->GetWorldTM());
	else
		RemoveEntity(entityID);
}

DynamicCoverManager::DynamicCoverManager()
	: m_segmentsGrid(20.0f, 20.0f, 20.0f, segment_position(m_segments))
{
}

void DynamicCoverManager::AddValidationSegment(const ValidationSegment& segment)
{
	size_t index;

	if (m_freeSegments.empty())
	{
		index = m_segments.size();
		m_segments.push_back(segment);
	}
	else
	{
		index = m_freeSegments.back();
		m_freeSegments.pop_back();

		ValidationSegment& slot = m_segments[index];
		slot = segment;
	}
	assert(m_segmentsGrid.find(segment.center, index) == m_segmentsGrid.end());
	m_segmentsGrid.insert(segment.center, index);
	assert(m_segments.capacity() < (512 << 10));
}

void DynamicCoverManager::RemoveSurfaceValidationSegments(const CoverSurfaceID& surfaceID)
{
	Segments::iterator it = m_segments.begin();
	Segments::iterator end = m_segments.end();

	size_t pushbackCount = 0;

	for (; it != end; ++it)
	{
		ValidationSegment& segment = *it;

		if (segment.surfaceID == surfaceID)
		{
			segment.surfaceID = CoverSurfaceID(0);

			size_t index = (size_t)std::distance(m_segments.begin(), it);
			m_freeSegments.push_back(static_cast<uint16>(index));

			SegmentsGrid::iterator gridIt = m_segmentsGrid.find(segment.center, index);
			assert(gridIt != m_segmentsGrid.end());
			if (gridIt != m_segmentsGrid.end())
				m_segmentsGrid.erase(gridIt);
			++pushbackCount;
		}
	}

	ValidationQueue::iterator qit = m_validationQueue.begin();
	ValidationQueue::iterator qend = m_validationQueue.end();

	if (pushbackCount > 0)
	{
		for (; qit != qend; )
		{
			QueuedValidation& queued = *qit;

			if (std::find(m_freeSegments.end() - pushbackCount, m_freeSegments.end(), queued.validationSegmentIdx) != m_freeSegments.end())
			{
				std::swap(queued, m_validationQueue.back());
				m_validationQueue.pop_back();
				qend = m_validationQueue.end();
			}
			else
				++qit;
		}
	}
}

void DynamicCoverManager::AddEntity(EntityId entityID)
{
	gEnv->pEntitySystem->AddEntityEventListener(entityID, ENTITY_EVENT_XFORM, this);
	gEnv->pEntitySystem->AddEntityEventListener(entityID, ENTITY_EVENT_DONE, this);

	m_entityCover.insert(EntityCover::value_type(entityID, EntityCoverState(gEnv->pTimer->GetFrameStartTime())));
}

void DynamicCoverManager::RemoveEntity(EntityId entityID)
{
	EntityCover::iterator it = m_entityCover.find(entityID);

	if (it != m_entityCover.end())
	{
		EntityCoverState& state = it->second;

		RemoveEntity(it->first, state);

		m_entityCover.erase(it);
	}
}

void DynamicCoverManager::RemoveEntity(EntityId entityID, EntityCoverState& state)
{
	gEnv->pEntitySystem->RemoveEntityEventListener(entityID, ENTITY_EVENT_XFORM, this);
	gEnv->pEntitySystem->RemoveEntityEventListener(entityID, ENTITY_EVENT_DONE, this);

	m_entityCoverSampler.Cancel(entityID);

	RemoveEntityCoverSurfaces(state);
}

void DynamicCoverManager::Reset()
{
	m_validationQueue.clear();
}

void DynamicCoverManager::Clear()
{
	ClearValidationSegments();

	EntityCover::iterator it = m_entityCover.begin();
	EntityCover::iterator end = m_entityCover.end();

	for (; it != end; ++it)
	{
		EntityCoverState& state = it->second;

		RemoveEntity(it->first, state);
	}

	m_entityCoverSampler.Clear();
	m_entityCover.clear();
}

void DynamicCoverManager::ClearValidationSegments()
{
	m_segments.clear();
	m_segmentsGrid.clear();
	m_freeSegments.clear();
	m_validationQueue.clear();
}

void DynamicCoverManager::Update(float updateTime)
{
	PREFAST_SUPPRESS_WARNING(6239);
	if (!DynamicCoverDeferred && !m_validationQueue.empty())
		ValidateOne();

	EntityCover::iterator it = m_entityCover.begin();
	EntityCover::iterator end = m_entityCover.end();

	CTimeValue now = gEnv->pTimer->GetFrameStartTime();

	for (; it != end; ++it)
	{
		EntityCoverState& state = it->second;

		if (state.state == EntityCoverState::Moving)
		{
			if ((now - state.lastMovement).GetMilliSecondsAsInt64() >= 150)
			{
				state.state = EntityCoverState::Sampling;

				m_entityCoverSampler.Queue(it->first, functor(*this, &DynamicCoverManager::EntityCoverSampled));
			}
		}
	}

	m_entityCoverSampler.Update();
}

void DynamicCoverManager::EntityCoverSampled(EntityId entityID, EntityCoverSampler::ESide side,
                                             const ICoverSystem::SurfaceInfo& surfaceInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	EntityCover::iterator it = m_entityCover.find(entityID);
	assert(it != m_entityCover.end());

	if (it != m_entityCover.end())
	{
		EntityCoverState& state = it->second;

		if (side == EntityCoverSampler::LastSide)
			state.state = EntityCoverState::Sampled;

		if (surfaceInfo.sampleCount > 0)
		{
			if (state.surfaceID[side])
				gAIEnv.pCoverSystem->UpdateSurface(state.surfaceID[side], surfaceInfo);
			else
				state.surfaceID[side] = gAIEnv.pCoverSystem->AddSurface(surfaceInfo);
		}
		else if (state.surfaceID[side])
		{
			gAIEnv.pCoverSystem->RemoveSurface(state.surfaceID[side]);
			state.surfaceID[side] = CoverSurfaceID(0);
		}
	}
}

void DynamicCoverManager::RemoveEntityCoverSurfaces(EntityCoverState& state)
{
	for (size_t i = 0; i < 4; ++i)
	{
		if (state.surfaceID[i])
		{
			gAIEnv.pCoverSystem->RemoveSurface(state.surfaceID[i]);
			state.surfaceID[i] = CoverSurfaceID(0);
		}
	}
}

void DynamicCoverManager::BreakEvent(const Vec3& position, float radius)
{
	m_dirtyBuf.clear();

	if (size_t count = m_segmentsGrid.query_sphere(position, radius, m_dirtyBuf))
	{
		for (size_t i = 0; i < count; ++i)
			QueueValidation(m_dirtyBuf[i]);
	}
}

void DynamicCoverManager::MoveEvent(EntityId entityID, const Matrix34& worldTM)
{
	EntityCover::iterator it = m_entityCover.find(entityID);

	if (it != m_entityCover.end())
	{
		EntityCoverState& state = it->second;

		if (!state.lastWorldTM.GetTranslation().IsEquivalent(worldTM.GetTranslation(), 0.075f) || !Matrix33::IsEquivalent(Matrix33(state.lastWorldTM), Matrix33(worldTM), 0.0125f))
		{
			m_entityCoverSampler.Cancel(entityID);

			state.state = EntityCoverState::Moving;
			state.lastMovement = gEnv->pTimer->GetFrameStartTime();
			state.lastWorldTM = worldTM;

			RemoveEntityCoverSurfaces(state);
		}
	}
}

void DynamicCoverManager::QueueValidation(int index)
{
	ValidationSegment& validationSegment = m_segments[index];

	if ((validationSegment.flags & ValidationSegment::Disabled) || (validationSegment.flags & ValidationSegment::Validating))
		return;

	validationSegment.flags |= ValidationSegment::Validating;

	m_validationQueue.push_back(QueuedValidation(index));

	if (DynamicCoverDeferred)
	{
		if (m_validationQueue.size() == 1)
			ValidateOne();
	}
}

void DynamicCoverManager::ValidateOne()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	QueuedValidation& queuedValidation = m_validationQueue.front();
	assert(queuedValidation.waitingCount == 0);

	ValidationSegment& validationSegment = m_segments[queuedValidation.validationSegmentIdx];

	const float ValidationGrounOffset = 0.35f;
	const float ValidationVerticalSpacing = 0.075f;
	const float ValidationTraceLength = 0.5f;

	const float radius = min(0.15f, validationSegment.length * 0.25f);
	Vec3 dir = -validationSegment.normal * ValidationTraceLength;

	primitives::sphere sphere;
	sphere.center = validationSegment.center + validationSegment.normal * (radius + 0.025f);
	sphere.r = radius;

	float centerZ = validationSegment.center.z + ValidationGrounOffset + radius;
	float maxZ = validationSegment.center.z + validationSegment.height;

	if (DynamicCoverDeferred)
	{
		while (centerZ < maxZ)
		{
			sphere.center.z = centerZ;

			++queuedValidation.waitingCount;
			++queuedValidation.negativeCount;

			gAIEnv.pIntersectionTester->Queue(IntersectionTestRequest::HighestPriority,
			                                  IntersectionTestRequest(sphere.type, sphere, dir, ent_static | ent_terrain),
			                                  functor(*this, &DynamicCoverManager::IntersectionTestComplete));

			//GetAISystem()->AddDebugLine(sphere.center, sphere.center + dir, 255, 255, 128, 5.0f);

			centerZ += radius + ValidationVerticalSpacing;
		}
	}
	else
	{
		validationSegment.flags &= ~ValidationSegment::Validating;

		sphere.center.z = centerZ;
		Vec3 end = sphere.center + dir;

		Vec3 boxMin(min(sphere.center.x, end.x), min(sphere.center.y, end.y), min(sphere.center.z, end.z));
		boxMin -= Vec3(radius, radius, radius);

		Vec3 boxMax(max(sphere.center.x, end.x), max(sphere.center.y, end.y), boxMin.z + validationSegment.height);
		boxMax += Vec3(radius, radius, radius);

		uint32 collisionEntities = CoverCollisionEntities;
		if (collisionEntities & ent_static)
			collisionEntities |= ent_rigid | ent_sleeping_rigid; // support 0-mass rigid entities too

		IPhysicalEntity** entityList = 0;
		size_t entityCount = (size_t)GetPhysicalEntitiesInBox(boxMin, boxMax, entityList, collisionEntities);

		if (entityCount)
		{
			if (CoverCollisionEntities & ent_static)
			{
				for (size_t i = 0; i < entityCount; ++i)
				{
					IPhysicalEntity* entity = entityList[i];

					if (entity->GetType() == PE_RIGID)
					{
						pe_status_dynamics status;

						if (entity->GetStatus(&status))
						{
							if ((status.mass > 0.00001f) && (status.mass < 99999.9999f)) // 99 tons won't move
							{
								entityList[i] = entityList[--entityCount];
								--i;
							}
						}
					}
				}
			}

			if (entityCount)
			{
				while (centerZ < maxZ)
				{
					sphere.center.z = centerZ;

					float closest = FLT_MAX;
					IPhysicalEntity* closestEntity = 0;

					ray_hit hit;
					for (size_t i = 0; i < entityCount; ++i)
					{
						if (gEnv->pPhysicalWorld->CollideEntityWithPrimitive(entityList[i], primitives::sphere::type, &sphere, dir, &hit))
						{
							if (hit.dist < closest)
							{
								closest = hit.dist + radius;
								closestEntity = entityList[i];
							}
						}
					}

					if (!closestEntity)
					{
						validationSegment.flags |= ValidationSegment::Disabled;

						CoverSurface& surface = gAIEnv.pCoverSystem->GetCoverSurface(validationSegment.surfaceID);

						if (surface.IsValid())
						{
							CoverSurface::Segment& segment = surface.GetSegment(validationSegment.segmentIdx);

							segment.flags |= CoverSurface::Segment::Disabled;
						}
						break;
					}

					centerZ += radius + ValidationVerticalSpacing;
				}
			}
		}

		m_validationQueue.pop_front();
	}
}

void DynamicCoverManager::IntersectionTestComplete(const QueuedIntersectionID& intID, const IntersectionTestResult& result)
{
	if (!m_validationQueue.empty())
	{
		QueuedValidation& queuedValidation = m_validationQueue.front();
		assert(queuedValidation.waitingCount > 0);

		--queuedValidation.waitingCount;
		queuedValidation.negativeCount -= result ? 1 : 0;

		if (!queuedValidation.waitingCount)
		{
			ValidationSegment& validationSegment = m_segments[queuedValidation.validationSegmentIdx];
			validationSegment.flags &= ~ValidationSegment::Validating;

			if (queuedValidation.negativeCount)
			{
				validationSegment.flags |= ValidationSegment::Disabled;

				CoverSurface& surface = gAIEnv.pCoverSystem->GetCoverSurface(validationSegment.surfaceID);

				if (surface.IsValid())
				{
					CoverSurface::Segment& segment = surface.GetSegment(validationSegment.segmentIdx);

					segment.flags |= CoverSurface::Segment::Disabled;
				}
			}

			m_validationQueue.pop_front();

			if (DynamicCoverDeferred)
			{
				if (!m_validationQueue.empty())
					ValidateOne();
			}
		}
	}
}
