// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CoverSampler.h"
#include "CoverSystem.h"

#include "DebugDrawContext.h"

CoverSampler::CoverSampler()
	: m_externalState(None)
	, m_aabb(AABB(AABB::RESET))
	, m_flags(0)
{
}

CoverSampler::~CoverSampler()
{
}

void CoverSampler::Release()
{
	delete this;
}

ICoverSampler::ESamplerState CoverSampler::StartSampling(const ICoverSampler::Params& params)
{
	m_state = SamplingState();
	m_state.internalState = SamplerStarting;

	m_params = params;
	m_externalState = InProgress;

	m_aabb = AABB(AABB::RESET);
	m_flags = 0;

	return m_externalState;
}

ICoverSampler::ESamplerState CoverSampler::Update(float timeLimitPerFrame, float timeLimitTotal)
{
	CTimeValue now = gEnv->pTimer->GetAsyncTime();
	CTimeValue start = now;
	CTimeValue endTime = now + CTimeValue(timeLimitPerFrame);
	CTimeValue maxTime = CTimeValue(timeLimitTotal);

	do
	{
		switch (m_state.internalState)
		{
		case SamplerStarting:
			{
				m_tempSamples.clear();
				m_samples.clear();

				m_state.initialDirection = m_params.direction;
				m_state.initialDirection.z = 0.0f;
				m_state.initialDirection += CoverSamplerBiasUp;
				m_state.initialDirection.NormalizeSafe(Vec3Constants<float>::fVec3_OneY);
				m_state.direction = m_state.initialDirection;

				Vec3 floorNormal;

				if (SampleFloor(m_params.position, m_params.floorSearchHeight * 2.0f, m_params.floorSearchRadius,
				                &m_state.initialFloor, &floorNormal))
				{
					m_state.initialFloor += CoverUp * CoverOffsetUp;
					m_state.floor = m_state.initialFloor;
					m_state.initialRight = m_state.direction.Cross(floorNormal);
					m_state.initialRight.Normalize();
					m_state.right = m_state.initialRight;

					SurfaceType surfaceType;
					float depth = -1.0f;
					float heightSamplerInterval = max(m_params.heightSamplerInterval, 0.05f);

					float height = SampleHeight(m_state.floor, heightSamplerInterval, m_params.limitHeight - CoverOffsetUp,
					                            &depth, &m_state.surfaceNormal, &surfaceType);

					if ((height > 0.0f) && (depth > 0.0f) && (height + CoverOffsetUp >= m_params.minHeight))
					{
						height += CoverOffsetUp;

						Vec3 origin = m_state.floor - (CoverUp * CoverOffsetUp);
						Vec3 surfaceOrigin = origin + m_state.direction * depth;

						m_tempSamples.push_back(Sample(surfaceOrigin, height,
						                               surfaceType != SolidSurface ? ICoverSampler::Sample::Dynamic : 0));

						m_state.origin = origin + m_state.direction * (depth - (m_params.floorSearchRadius + CoverExtraClearance));

						m_state.internalState = SamplerLeft;
						m_state.limitLength = m_params.limitLeft;
						m_state.length = 0.0f;
						m_state.edgeHeight = 0.0f;
						m_state.depth = depth;

						m_state.solidDirection = m_state.direction;
						m_state.solidRight = m_state.initialRight;
						m_state.solidFloor = m_state.initialFloor;

						break;
					}
				}

				m_state.internalState = SamplerError;
			}
			break;
		case SamplerLeft:
			{
				if (!SampleWidth(Left, m_params.widthSamplerInterval, m_params.limitHeight))
				{
					m_state.direction = m_state.initialDirection;
					m_state.right = m_state.initialRight;
					m_state.floor = m_state.initialFloor;

					m_state.internalState = SamplerRight;
					m_state.limitLength = m_params.limitRight;
					m_state.length = 0.0f;
				}
			}
			break;
		case SamplerRight:
			{
				if (!SampleWidth(Right, m_params.widthSamplerInterval, m_params.limitHeight))
				{
					m_state.internalState = SamplerSimplifySurface;
					m_state.limitLength = 0.0f;
					m_state.length = 0.0f;
				}
			}
			break;
		case SamplerSimplifySurface:
			{
				if (m_tempSamples.size() < 2)
					m_state.internalState = SamplerError;
				else
				{
					if (m_tempSamples.size() > 3)
					{
						float distanceSq = (m_tempSamples.front().position - m_tempSamples.back().position).GetLengthSquared2D();
						if (distanceSq <= sqr(m_params.widthSamplerInterval + CoverSamplerLoopExtraOffset))
						{
							m_state.looped = true;
							m_tempSamples.push_back(m_tempSamples.front());
						}
					}

					m_state.originalSurfaceSamples = m_tempSamples.size();

					Simplify();

					TempSamples().swap(m_tempSamples);
					m_state.internalState = SamplerFinished;
				}
			}
			break;
		case SamplerFinished:
			{
				m_state.internalState = SamplerReady;
				m_externalState = Finished;

				m_flags = 0;
				if (m_state.looped)
					m_flags |= ICoverSystem::SurfaceInfo::Looped;
			}
			break;
		case SamplerError:
			break;
		default:
			return m_externalState;
		}

		if (m_state.internalState == SamplerError)
		{
			m_state.internalState = SamplerReady;
			m_externalState = Error;

			break;
		}

		now = gEnv->pTimer->GetAsyncTime();

		if (m_externalState != InProgress)
			break;

	}
	while (now < endTime);

	++m_state.updateCount;

	CTimeValue totalTime = m_state.totalTime + now - start;
	m_state.totalTime = totalTime;

	if (totalTime >= maxTime)
	{
		TempSamples().swap(m_tempSamples);
		Samples().swap(m_samples);

		m_externalState = Error;
		m_state.internalState = SamplerError;
	}

	return m_externalState;
}

ICoverSampler::ESamplerState CoverSampler::GetState() const
{
	return m_externalState;
}

uint32 CoverSampler::GetSampleCount() const
{
	return m_samples.size();
}

const ICoverSampler::Sample* CoverSampler::GetSamples() const
{
	if (!m_samples.empty())
		return &m_samples[0];

	return 0;
}

const AABB& CoverSampler::GetAABB() const
{
	return m_aabb;
}

uint32 CoverSampler::GetSurfaceFlags() const
{
	return m_flags;
}

void CoverSampler::DebugDraw() const
{
	CDebugDrawContext dc;

	dc->Draw3dLabel(m_state.origin + CoverUp * 0.5f, 1.35f,
	                "Time: ~%.2f/%.2fms\nUpdate Count: %d\nPWI Count: %d\nSimplify: %" PRISIZE_T "/%d",
	                m_state.totalTime.GetMilliSeconds() / (float)m_state.updateCount, m_state.totalTime.GetMilliSeconds(),
	                m_state.updateCount, m_state.pwiCount, m_samples.size(), m_state.originalSurfaceSamples);
}

bool CoverSampler::OverlapCylinder(const Vec3& bottomCenter, const Vec3& dir, float length, float radius,
                                   uint32 collisionFlags) const
{
	++m_state.pwiCount;

	float hh = length * 0.5f;

	primitives::cylinder cylinder;
	cylinder.center = bottomCenter + dir * hh;
	cylinder.axis = dir;
	cylinder.hh = hh;
	cylinder.r = radius;

	if (IntersectionTestResult intResult = gAIEnv.pIntersectionTester->Cast(
	      IntersectionTestRequest(cylinder.type, cylinder, Vec3Constants<float>::fVec3_Zero, CoverCollisionEntities)))
	{
		return true;
	}

	return false;
}

bool CoverSampler::IntersectSweptSphere(const Vec3& center, const Vec3& dir, float radius, IPhysicalEntity** entityList,
                                        size_t entityCount, IntersectionResult* result) const
{
	++m_state.pwiCount;

	primitives::sphere sphere;
	sphere.center = center;
	sphere.r = radius;

	float closest = FLT_MAX;
	Vec3 closestLocation;
	Vec3 closestNormal;
	IPhysicalEntity* closestEntity = 0;
	int closestPart = 0;

	ray_hit hit;
	for (size_t i = 0; i < entityCount; ++i)
	{
		if (gEnv->pPhysicalWorld->CollideEntityWithPrimitive(entityList[i], primitives::sphere::type, &sphere, dir, &hit))
		{
			if (hit.dist < closest)
			{
				closest = hit.dist + radius;
				closestLocation = hit.pt;
				closestNormal = hit.n;
				closestEntity = entityList[i];
				closestPart = hit.ipart;
			}
		}
	}

	if (closestEntity)
	{
		result->distance = closest;
		result->position = closestLocation;
		result->normal = closestNormal;
		result->surfaceType = GetSurfaceType(closestEntity, closestPart);

		return true;
	}

	return false;
}

bool CoverSampler::IntersectRay(const Vec3& center, const Vec3& dir, uint32 collisionFlags, IntersectionResult* result) const
{
	++m_state.rwiCount;

	if (RayCastResult rayResult = gAIEnv.pRayCaster->Cast(RayCastRequest(center, dir, collisionFlags,
	                                                                     rwi_stop_at_pierceable | (geom_colltype_player << rwi_colltype_bit))))
	{
		if (rayResult->dist >= 0.0f)
		{
			if (collisionFlags & ent_static)
			{
				if (rayResult->pCollider->GetType() == PE_RIGID)
				{
					pe_status_dynamics status;
					if (rayResult->pCollider->GetStatus(&status))
					{
						if ((status.mass > 0.00001f))
							return false;
					}
				}
			}
			if (result)
			{
				result->distance = rayResult->dist;
				result->position = rayResult->pt;
				result->normal = rayResult->n;
				result->surfaceType = GetSurfaceType(rayResult->pCollider, rayResult->ipart);
			}
			return true;
		}
	}

	return false;
}

CoverSampler::SurfaceType CoverSampler::GetSurfaceType(IPhysicalEntity* physicalEntity, int ipart) const
{
	pe_params_part pp;
	pp.ipart = ipart;

	if (physicalEntity->GetParams(&pp))
	{
		if (!pp.pPhysGeom || !pp.pPhysGeom->pGeom)
			return SolidSurface;

		if ((pp.flagsOR & geom_manually_breakable) ||
		    (pp.idmatBreakable >= 0) ||
		    (pp.idSkeleton >= 0))
		{
			return DynamicSurface;
		}
	}

	if (IEntity* entity = gEnv->pEntitySystem->GetEntityFromPhysics(physicalEntity))
	{
		if (gAIEnv.pCoverSystem->IsDynamicSurfaceEntity(entity))
			return DynamicSurface;
	}

	uint64 jointConnectedParts = 0;
	pe_params_structural_joint sjp;
	sjp.idx = 0;

	uint32 jointCount = 0;
	while (physicalEntity->GetParams(&sjp))
	{
		++jointCount;
		if (sjp.bBreakable && !sjp.bBroken)
		{
			if ((sjp.partid[0] > -1) && (sjp.partid[0] < 64))
				jointConnectedParts |= 1ll << sjp.partid[0];

			if ((sjp.partid[1] > -1) && (sjp.partid[1] < 64))
				jointConnectedParts |= 1ll << sjp.partid[1];
		}
		sjp.idx = jointCount;
		MARK_UNUSED sjp.id;
	}

	return jointConnectedParts ? DynamicSurface : SolidSurface;
}

float CoverSampler::SampleHeightInterval(const Vec3& position, const Vec3& dir, float interval,
                                         IPhysicalEntity** entityList, size_t entityCount, float* depth, Vec3* normal,
                                         SurfaceType* surfaceType)
{
	Vec3 top = position + CoverUp * interval;

	if (gAIEnv.CVars.DebugDrawCoverSampler & 2)
	{
		GetAISystem()->AddDebugLine(top, top + dir, 92, 192, 0, 3.0f);
	}

	IntersectionResult result;

	if (IntersectSweptSphere(top, dir, 0.00125f, entityList, entityCount, &result)
	    && (fabs_tpl(result.normal.z) < 0.5f))
	{
		if (depth)
			*depth = result.distance;

		if (normal)
			*normal = result.normal;

		if (surfaceType)
			*surfaceType = result.surfaceType;

		if (gAIEnv.CVars.DebugDrawCoverSampler & 4)
		{
			GetAISystem()->AddDebugLine(result.position, result.position + result.normal, 0, 128, 192, 3.0f);
		}

		return interval;
	}
	else if (interval >= m_params.heightAccuracy * 2.0f)
	{
		float intervalMid = interval * 0.5f;

		float res = SampleHeightInterval(position, dir, intervalMid, entityList, entityCount, depth, normal, surfaceType);
		if (res > 0.0001f)
			return res;

		res = SampleHeightInterval(position + CoverUp * intervalMid, dir, intervalMid, entityList, entityCount,
		                           depth, normal, surfaceType);

		if (res > 0.0001f)
			return res;
	}

	return 0.0f;
}

float CoverSampler::SampleHeight(const Vec3& position, float heightInterval, float maxHeight, float* depth,
                                 Vec3* averageNormal, SurfaceType* surfaceType)
{
	assert(heightInterval > std::numeric_limits<float>::epsilon());
	assert(maxHeight >= 0.0f);
	uint32 sampleCount = (uint32)(0.5f + maxHeight / heightInterval);

	if (sampleCount <= 0)
		return 0.0f;

	float deltaHeight = maxHeight / (float)sampleCount;

	Vec3 dir = m_state.direction * (m_params.limitDepth + m_params.floorSearchRadius + CoverExtraClearance);
	Vec3 end = position + dir;

	Vec3 boxMin(min(position.x, end.x), min(position.y, end.y), min(position.z, end.z));
	boxMin -= Vec3(0.05f, 0.05f, 0.05f);

	Vec3 boxMax(max(position.x, end.x), max(position.y, end.y), boxMin.z + maxHeight);
	boxMax += Vec3(0.05f, 0.05f, 0.05f);

	uint32 collisionEntities = CoverCollisionEntities;
	if (collisionEntities & ent_static)
		collisionEntities |= ent_rigid | ent_sleeping_rigid; // support 0-mass rigid entities too

	IPhysicalEntity* physicalRefEntity = 0;
	IPhysicalEntity** entityList = 0;
	size_t entityCount = 0;

	if (m_params.referenceEntity)
	{
		physicalRefEntity = m_params.referenceEntity->GetPhysics();
		entityList = &physicalRefEntity;
		entityCount = physicalRefEntity ? 1 : 0;
	}
	else
	{
		entityCount = (size_t)GetPhysicalEntitiesInBox(boxMin, boxMax, entityList, collisionEntities);

		if (entityCount == 0)
			return 0.0f;

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
						if ((status.mass > 0.00001f))
						{
							entityList[i] = entityList[--entityCount];
							--i;
						}
					}
				}
			}
		}
	}

	if (entityCount == 0)
		return 0.0f;

	float currHeight = 0.0f;
	float currDepth = FLT_MAX;

	uint32 resultCount = 0;

	if (averageNormal)
		averageNormal->zero();

	if (surfaceType)
		*surfaceType = SolidSurface;

	for (uint32 i = 0; i <= sampleCount; ++i)
	{
		float sampleStart = deltaHeight * i;
		Vec3 center = position + CoverUp * sampleStart;

		Vec3 normal;
		float smpDepth;
		SurfaceType intervalSurfaceType;
		float height = SampleHeightInterval(center, dir, heightInterval, entityList, entityCount, &smpDepth, &normal,
		                                    &intervalSurfaceType);

		if (height > 0.001f)
		{
			if ((intervalSurfaceType != SolidSurface) && surfaceType)
				*surfaceType = intervalSurfaceType;

			currHeight = sampleStart + height;
			if (currDepth > smpDepth)
				currDepth = smpDepth;

			++resultCount;

			if (averageNormal)
				*averageNormal += normal;
		}
		else
		{
			float sampleHeight = sampleStart + deltaHeight;
			if (sampleHeight >= m_params.maxStartHeight)
			{
				if ((sampleHeight - currHeight) > heightInterval)
					break;
			}
		}
	}

	if (depth)
		*depth = currDepth;

	if (averageNormal && (resultCount > 1))
		*averageNormal /= (float)resultCount;

	return currHeight;
}

bool CoverSampler::SampleFloor(const Vec3& position, float searchHeight, float searchRadius,
                               Vec3* floor, Vec3* normal)
{
	Vec3 center = position + CoverUp * (searchHeight * 0.5f);
	Vec3 dir = CoverUp * -searchHeight;

	if (gAIEnv.CVars.DebugDrawCoverSampler & 1)
	{
		GetAISystem()->AddDebugLine(center, center + dir, 214, 164, 96, 3.0f);
		GetAISystem()->AddDebugSphere(center, 0.1f, 210, 183, 135, 3.0f);
		GetAISystem()->AddDebugSphere(center + dir, 0.1f, 139, 69, 19, 3.0f);
	}

	IntersectionResult result;

	int collisionEntities = CoverCollisionEntities;
	if (collisionEntities & ent_static)
		collisionEntities |= ent_rigid | ent_sleeping_rigid; // support 0-mass rigid entities too

	if (IntersectRay(center, dir, collisionEntities, &result))
	{
		if (CoverUp.dot(result.normal) <= 0.0001f)
			return false;

		if (floor)
			*floor = result.position;

		if (normal)
			*normal = result.normal;

		return true;
	}

	return false;
}

float CoverSampler::SampleWidthInterval(EDirection direction, float interval, float maxHeight, Vec3& floor,
                                        Vec3& floorNormal, Vec3& averageNormal)
{
	float delta = 0.025f;
	float offset = interval;

	while (offset > (delta + 0.0025f))
	{
		Vec3 offsetDir = m_state.right * ((direction == Left) ? -offset : offset);
		bool ok = SampleFloor(m_state.floor + offsetDir, m_params.floorSearchHeight, m_params.floorSearchRadius, &floor, &floorNormal);
		floor += CoverUp * CoverOffsetUp;

		ok = ok && CheckClearance(m_state.floor, floorNormal, offsetDir, maxHeight);
		ok = ok && CanSampleHeight(floor, m_state.direction, direction);

		if (ok)
		{
			float depth = -1.0f;
			float heightSamplerInterval = max(m_params.heightSamplerInterval, 0.05f);

			SurfaceType surfaceType;
			float height = SampleHeight(floor, heightSamplerInterval, maxHeight - CoverOffsetUp, &depth, &averageNormal,
			                            &surfaceType);

			if ((height > 0.0f) && (depth > 0.0f) && (height + CoverOffsetUp >= m_params.minHeight))
			{
				Sample sample(floor + (m_state.direction * depth), height);
				sample.flags |= (surfaceType != SolidSurface) ? ICoverSampler::Sample::Dynamic : 0;

				Vec3 backoffDir = -m_state.direction;
				backoffDir.z *= -1.0f; // direction.z is always 0 + small offset

				floor = sample.position + (backoffDir * (m_params.floorSearchRadius + CoverExtraClearance));
				floor += CoverUp * -CoverOffsetUp;
				height += CoverOffsetUp;

				CLAMP(height, 0.0f, maxHeight);

				float deltaSq;

				if (ValidateSample(floor, m_state.direction, sample, direction, &deltaSq))
				{
					TempSamples::iterator insertLocation = (direction == Left) ? m_tempSamples.begin() : m_tempSamples.end();
					m_tempSamples.insert(insertLocation, sample);

					m_state.length += sqrt_tpl(deltaSq);
					m_state.depth = depth;
					m_state.edgeHeight = height;
				}
				else if (gAIEnv.CVars.DebugDrawCoverSampler & 8)
				{
					GetAISystem()->AddDebugSphere(sample.position, 0.035f, 255, 0, 0, 3.0f);
					GetAISystem()->AddDebugLine(sample.position + CoverUp * CoverOffsetUp, floor + CoverUp * CoverOffsetUp, 255, 0, 0, 3.0f);
				}

				return height;
			}
		}

		offset -= delta;
	}

	return 0.0f;
}

bool CoverSampler::SampleWidth(EDirection direction, float widthInterval, float maxHeight)
{
	Vec3 floor(ZERO);
	Vec3 floorNormal(ZERO);
	Vec3 averageNormal(ZERO);

	if (SampleWidthInterval(direction, widthInterval, maxHeight, floor, floorNormal, averageNormal) < m_params.minHeight)
		return false;

	if (!Adjust(floor, floorNormal, m_state.surfaceNormal, 0.25f))
		return false;

	m_state.solidDirection = m_state.direction;
	m_state.solidRight = m_state.right;
	m_state.solidFloor = m_state.floor;

	averageNormal.z = 0.0f;
	averageNormal.normalize();
	m_state.surfaceNormal = -averageNormal;

	if (m_state.length >= m_state.limitLength)
		return false;

	// prevent overlapping if looping

	if (m_tempSamples.size() > 3)
	{
		float distanceSq = (m_tempSamples.front().position - m_tempSamples.back().position).GetLengthSquared2D();
		if (distanceSq <= sqr(m_params.widthSamplerInterval + CoverSamplerLoopExtraOffset))
			return false;
	}

	return true;
}

bool CoverSampler::CheckClearance(const Vec3& floor, const Vec3& floorNormal, const Vec3& dir, float maxHeight)
{
	/*

	   #ifdef CRYAISYSTEM_DEBUG
	   if (gAIEnv.CVars.DebugDrawCoverSampler & 16)
	   {
	    Vec3 center = floor + CoverUp * (slopeOffset + (maxHeight - slopeOffset) * 0.5f);

	    GetAISystem()->AddDebugCylinder(center, CoverUp, m_params.floorSearchRadius, maxHeight - slopeOffset,
	      ColorB(Col_DarkWood, 0.25f), 3.0f);
	   }
	   #endif //CRYAISYSTEM_DEBUG

	   if (OverlapCylinder(floor + CoverUp * slopeOffset, CoverUp, maxHeight - slopeOffset, m_params.floorSearchRadius,
	    CoverCollisionEntities))
	   {
	   #ifdef CRYAISYSTEM_DEBUG
	    Vec3 center = floor + CoverUp * (slopeOffset + (maxHeight - slopeOffset) * 0.5f);

	    GetAISystem()->AddDebugCylinder(center, CoverUp, m_params.floorSearchRadius, maxHeight - slopeOffset,
	      ColorB(Col_Red, 0.75f), 3.0f);
	   #endif //CRYAISYSTEM_DEBUG

	    return false;
	   }*/

	//if (!m_params.referenceEntity)
	{
		const float slopeOffset = tan_tpl(DEG2RAD(25.0f)) * m_params.floorSearchRadius;

		const float Radius = 0.25f;
		IntersectionResult result;

		//	Vec3 offset = dir.normalized() * -Radius;
		Vec3 offset(ZERO);
		Vec3 center = offset + floor + CoverUp * (slopeOffset + Radius);
		//if (IntersectSweptSphere(center, dir, Radius, CoverCollisionEntities, &result))
		if (IntersectRay(center, dir, CoverCollisionEntities, &result))
			return false;

		center = offset + floor + CoverUp * (slopeOffset + (maxHeight - slopeOffset - Radius));
		//if (IntersectSweptSphere(center, dir, Radius, CoverCollisionEntities, &result))
		if (IntersectRay(center, dir, CoverCollisionEntities, &result))
			return false;
	}

	return true;
}

bool CoverSampler::Adjust(const Vec3& floor, const Vec3& floorNormal, const Vec3& wantedDirection, float weight)
{
	Vec3 adjustedDir = (wantedDirection * weight + m_state.direction * (1.0f - weight));
	adjustedDir.z = 0.0f;
	adjustedDir += CoverSamplerBiasUp;
	adjustedDir.Normalize();

	if (m_params.maxCurvatureAngleCos >= -1.0f)
	{
		if (m_state.initialDirection.dot(adjustedDir) <= m_params.maxCurvatureAngleCos)
			return false;
	}

	m_state.direction = adjustedDir;
	m_state.right = m_state.direction.Cross(floorNormal);
	m_state.right.Normalize();
	m_state.floor = floor;

	return true;
}

bool CoverSampler::CanSampleHeight(const Vec3& floor, const Vec3& direction, EDirection samplingDirection) const
{
	if (m_tempSamples.size() <= 1)
		return true;

	const Vec3& end =
	  (samplingDirection == Left) ? m_tempSamples.begin()->position : m_tempSamples.rbegin()->position;
	const Vec3& start =
	  (samplingDirection == Left) ? (m_tempSamples.begin() + 1)->position : (m_tempSamples.rbegin() + 1)->position;

	float a, b;
	Lineseg segment(start, end);
	Lineseg ray(floor, floor + direction * (m_params.limitDepth + m_params.floorSearchRadius + CoverExtraClearance));

	if (Intersect::Lineseg_Lineseg2D(segment, ray, a, b))
		return false;

	return true;
}

bool CoverSampler::ValidateSample(const Vec3& floor, const Vec3& direction, const Sample& sample,
                                  EDirection samplingDirection, float* deltaSq) const
{
	assert(deltaSq);

	const Sample& previous = (samplingDirection == Left) ? m_tempSamples.front() : m_tempSamples.back();

	Vec3 delta = previous.position - sample.position;
	*deltaSq = delta.GetLengthSquared2D();

	const float SmallLocationThresholdSq = sqr(0.0125f);

	if (*deltaSq < SmallLocationThresholdSq)
		return false;

	if (m_tempSamples.size() <= 1)
		return true;

	// check if the angle is not too spiky
	const Vec3& mid =
	  (samplingDirection == Left) ? m_tempSamples.begin()->position : m_tempSamples.rbegin()->position;
	const Vec3& right =
	  (samplingDirection == Left) ? (m_tempSamples.begin() + 1)->position : (m_tempSamples.rbegin() + 1)->position;
	const Vec3& left = sample.position;

	Vec3 toLeft = left - mid;
	toLeft.z = 0.0f;
	toLeft.normalize();

	Vec3 toRight = right - mid;
	toRight.z = 0.0f;
	toRight.normalize();

	const float MinAngleCos = cos_tpl(DEG2RAD(90.5f));
	if (toLeft.Dot(toRight) >= MinAngleCos)
		return false;

	return true;
}

void CoverSampler::Simplify()
{
	uint32 coverSampleCount = m_tempSamples.size();

	if (coverSampleCount < 3)
		return;

	m_aabb = AABB(AABB::RESET);
	m_samples.resize(0);

	uint32 leftIdx = 0;
	uint32 midIdx = 1;
	uint32 rightIdx = 2;

	if (!m_state.looped)
	{
		const Sample& front = m_tempSamples.front();
		m_samples.push_back(front);
		m_aabb.Add(front.position);
		m_aabb.Add(front.position + Vec3(0.0f, 0.0f, front.GetHeight()));
	}

	const float MinThreshold = 0.0025f;
	const float MinThresholdSq = sqr(MinThreshold);
	const float Threshold = max(m_params.simplifyThreshold, MinThreshold);
	const float ThresholdSq = sqr(Threshold);
	const float MaxDynamicSampleDistanceSq = sqr(0.4f);

	uint32 tempSampleCount = m_tempSamples.size();
	uint32 iterationCount = m_state.looped ? tempSampleCount : (tempSampleCount - 3);

	for (uint i = 0; i < iterationCount; ++i)
	{
		const Sample& left = m_tempSamples[leftIdx % tempSampleCount];
		const Sample& mid = m_tempSamples[midIdx % tempSampleCount];
		const Sample& right = m_tempSamples[rightIdx % tempSampleCount];

		float t;
		float distanceSq = Distance::Point_LinesegSq(mid.position, Lineseg(left.position, right.position), t);

		if (distanceSq < ThresholdSq)
		{
			float toLeftLen = (left.position - mid.position).NormalizeSafe();
			float toRightLen = (right.position - mid.position).NormalizeSafe();

			float leftTop = left.position.z + left.GetHeight();
			float rightTop = right.position.z + right.GetHeight();
			float midTop = mid.position.z + mid.GetHeight();

			float midFaction = toLeftLen / (toLeftLen + toRightLen);
			float goodMid = leftTop + midFaction * (rightTop - leftTop);

			float topDiff = fabs_tpl(goodMid - midTop);

			if (topDiff < Threshold)
			{
				if (mid.flags & ICoverSampler::Sample::Dynamic)
				{
					if (m_samples.empty() || ((m_samples.back().flags & ICoverSampler::Sample::Dynamic) == 0)
					    || ((m_samples.back().position - mid.position).len2() >= MaxDynamicSampleDistanceSq))
					{
						m_aabb.Add(mid.position);
						m_aabb.Add(mid.position + Vec3(0.0f, 0.0f, mid.GetHeight()));

						m_samples.push_back(mid);
						++leftIdx;
						++midIdx;
						++rightIdx;

						continue;
					}
				}

				if ((topDiff < MinThreshold) && (distanceSq < MinThresholdSq)) // just lying on the line
				{

					++leftIdx;
					++midIdx;
					++rightIdx;
					continue;
				}
				else
				{
					m_aabb.Add(right.position);
					m_aabb.Add(right.position + Vec3(0.0f, 0.0f, right.GetHeight()));

					m_samples.push_back(right);
					leftIdx += 2;
					midIdx += 2;
					rightIdx += 2;
					iterationCount -= 1;
					continue;
				}
			}
		}

		m_aabb.Add(mid.position);
		m_aabb.Add(mid.position + Vec3(0.0f, 0.0f, mid.GetHeight()));

		m_samples.push_back(mid);
		++leftIdx;
		++midIdx;
		++rightIdx;
	}

	if (!m_state.looped)
	{
		const Sample& back = m_tempSamples.back();
		m_samples.push_back(back);
		m_aabb.Add(back.position);
		m_aabb.Add(back.position + Vec3(0.0f, 0.0f, back.GetHeight()));
	}

	// workaround for when we end up with no more than 1 sample
	// (this can happen if all samples lie on a line and the last sample is looping back to the first one)
	if (m_samples.size() < 2)
	{
		// reject all samples
		m_aabb = AABB(AABB::RESET);
		m_samples.resize(0);
	}
}
