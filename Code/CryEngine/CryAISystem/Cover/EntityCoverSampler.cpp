// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EntityCoverSampler.h"
#include "CoverSystem.h"

#include "DebugDrawContext.h"

EntityCoverSampler::EntityCoverSampler()
	: m_sampler(0)
	, m_currentSide(Left)
	, m_lastSort(0ll)
{
}

EntityCoverSampler::~EntityCoverSampler()
{
	if (m_sampler)
		m_sampler->Release();
	m_sampler = 0;
}

void EntityCoverSampler::Clear()
{
	m_queue.clear();

	m_lastSort.SetValue(0ll);
}

void EntityCoverSampler::Queue(EntityId entityID, const Callback& callback)
{
	m_queue.push_back(QueuedEntity(entityID, callback));
}

void EntityCoverSampler::Cancel(EntityId entityID)
{
	if (!m_queue.empty())
	{
		QueuedEntities::iterator it = m_queue.begin();
		QueuedEntities::iterator end = m_queue.end();

		if (entityID == m_queue.front().entityID)
			m_queue.pop_front();
		else
		{
			for (; it != end; ++it)
			{
				QueuedEntity& queued = *it;

				if (queued.entityID == entityID)
				{
					m_queue.erase(it);
					break;
				}
			}
		}
	}
}

void EntityCoverSampler::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CTimeValue now = gEnv->pTimer->GetFrameStartTime();

	while (!m_queue.empty())
	{
		if ((now - m_lastSort).GetMilliSecondsAsInt64() > 1250)
		{
			if (CAIObject* player = GetAISystem()->GetPlayer())
			{
				Vec3 center = player->GetPos();

				QueuedEntities::iterator it = m_queue.begin();
				QueuedEntities::iterator end = m_queue.end();

				for (; it != end; ++it)
				{
					QueuedEntity& queued = *it;

					if (IEntity* entity = gEnv->pEntitySystem->GetEntity(queued.entityID))
						queued.distanceSq = (center - entity->GetWorldPos()).GetLengthSquared2D();
					else
						queued.distanceSq = FLT_MAX;
				}

				if (m_queue.front().state != QueuedEntity::Queued)
					std::sort(m_queue.begin() + 1, m_queue.end(), QueuedEntitySorter());
				else
					std::sort(m_queue.begin(), m_queue.end(), QueuedEntitySorter());

				m_lastSort = now;
			}
		}

		QueuedEntity& queued = m_queue.front();

		if (!m_sampler)
			m_sampler = gAIEnv.pCoverSystem->CreateCoverSampler("Default");

		assert(m_sampler);

		IEntity* entity = gEnv->pEntitySystem->GetEntity(queued.entityID);
		assert(entity);

		if (queued.state == QueuedEntity::Queued)
		{
			if (IPhysicalEntity* physicalEntity = entity->GetPhysics())
			{
				pe_status_nparts nparts;

				if (int partCount = physicalEntity->GetStatus(&nparts))
				{
					AABB localBB(AABB::RESET);

					pe_status_pos pp;
					primitives::box box;

					for (int p = 0; p < partCount; ++p)
					{
						pp.ipart = p;
						pp.flags = status_local;

						if (physicalEntity->GetStatus(&pp))
						{
							if (IGeometry* geometry = pp.pGeomProxy ? pp.pGeomProxy : pp.pGeom)
							{
								geometry->GetBBox(&box);

								Vec3 center = box.center * pp.scale;
								Vec3 size = box.size * pp.scale;

								center = pp.pos + pp.q * center;
								Matrix33 orientationTM = Matrix33(pp.q) * box.Basis.GetTransposed();

								localBB.Add(center + orientationTM * Vec3(size.x, size.y, size.z));
								localBB.Add(center + orientationTM * Vec3(size.x, size.y, -size.z));
								localBB.Add(center + orientationTM * Vec3(size.x, -size.y, size.z));
								localBB.Add(center + orientationTM * Vec3(size.x, -size.y, -size.z));
								localBB.Add(center + orientationTM * Vec3(-size.x, size.y, size.z));
								localBB.Add(center + orientationTM * Vec3(-size.x, size.y, -size.z));
								localBB.Add(center + orientationTM * Vec3(-size.x, -size.y, size.z));
								localBB.Add(center + orientationTM * Vec3(-size.x, -size.y, -size.z));
							}
						}
					}

					Matrix34 worldTM = entity->GetWorldTM();

					m_obb = OBB::CreateOBBfromAABB(Matrix33(worldTM), localBB);

					float upSign = fsgnf(worldTM.GetColumn2().dot(CoverUp));

					m_params.position = worldTM.GetTranslation() + m_obb.m33.TransformVector(m_obb.c) +
					                    (m_obb.m33.GetColumn0() * -m_obb.h.x * upSign) + m_obb.m33.GetColumn2() * -m_obb.h.z * upSign;
					Vec3 dir = m_obb.m33.GetColumn0() * upSign;
					dir.z = 0.0f;
					dir.normalize();
					m_params.referenceEntity = entity;
					m_params.heightSamplerInterval = 0.25f;
					m_params.widthSamplerInterval = 0.25f;
					m_params.heightAccuracy = 0.125f;
					m_params.limitDepth = 0.5f;
					m_params.maxStartHeight = m_params.minHeight;
					m_params.maxCurvatureAngleCos = 0.087f; // 85
					m_params.limitHeight = m_obb.h.z * 2.125f;
					m_params.limitLeft = m_obb.h.y * 1.075f;
					m_params.limitRight = m_obb.h.y * 1.075f;

					m_params.direction = dir;

					m_currentSide = Left;
					m_sampler->StartSampling(m_params);

					queued.state = QueuedEntity::SamplingLeft;
					break;
				}
			}

			// nothing to sample
			if (queued.callback)
				queued.callback(queued.entityID, LastSide, ICoverSystem::SurfaceInfo());

			m_queue.pop_front();

			continue;
		}

		ICoverSampler::ESamplerState state = m_sampler->Update(0.00025f);

		if (gAIEnv.CVars.DebugDrawDynamicCoverSampler)
			m_sampler->DebugDraw();

		if (state != ICoverSampler::InProgress)
		{
			ICoverSystem::SurfaceInfo surfaceInfo;

			if (state == ICoverSampler::Finished)
			{
				surfaceInfo.flags = m_sampler->GetSurfaceFlags();
				surfaceInfo.samples = m_sampler->GetSamples();
				surfaceInfo.sampleCount = m_sampler->GetSampleCount();
			}

			if (queued.callback)
				queued.callback(queued.entityID, (ESide)m_currentSide, surfaceInfo);

			if (++m_currentSide > LastSide)
				m_queue.pop_front();
			else
			{
				Matrix34 worldTM = entity->GetWorldTM();

				float upSign = fsgnf(worldTM.GetColumn2().dot(CoverUp));

				switch (m_currentSide)
				{
				case Right:
					{
						queued.state = QueuedEntity::SamplingRight;

						m_params.position = worldTM.GetTranslation() + m_obb.m33.TransformVector(m_obb.c) +
						                    (m_obb.m33.GetColumn0() * m_obb.h.x * upSign) + m_obb.m33.GetColumn2() * -m_obb.h.z * upSign;

						m_params.direction = -m_params.direction;

						m_sampler->StartSampling(m_params);
					}
					break;
				case Front:
					{
						queued.state = QueuedEntity::SamplingFront;

						m_params.position = worldTM.GetTranslation() + m_obb.m33.TransformVector(m_obb.c) +
						                    (m_obb.m33.GetColumn1() * m_obb.h.y * upSign) + m_obb.m33.GetColumn2() * -m_obb.h.z * upSign;
						/*
						   FrancescoR: Switching -upSign with (-1 * upSign) to break a Visual Studio 2010
						   assembly optimization that causes a crash in Profile|64 (It looks to be a compiler bug)
						 */
						Vec3 dir = -1 * upSign * m_obb.m33.GetColumn1();
						dir.z = 0.0f;
						dir.normalize();
						m_params.limitHeight = m_obb.h.z * 2.125f;
						m_params.limitLeft = m_obb.h.x * 1.075f;
						m_params.limitRight = m_obb.h.x * 1.075f;

						m_params.direction = dir;

						m_sampler->StartSampling(m_params);
					}
					break;
				case Back:
					{
						queued.state = QueuedEntity::SamplingBack;

						m_params.position = worldTM.GetTranslation() + m_obb.m33.TransformVector(m_obb.c) +
						                    (m_obb.m33.GetColumn1() * -m_obb.h.y * upSign) + m_obb.m33.GetColumn2() * -m_obb.h.z * upSign;

						m_params.direction = -m_params.direction;

						m_sampler->StartSampling(m_params);
					}
					break;
				}
			}
		}
		break;
	}

	if (gAIEnv.CVars.DebugDrawDynamicCoverSampler)
		DebugDraw();
}

void EntityCoverSampler::DebugDraw()
{
	CDebugDrawContext dc;

	const float fontSize = 1.25f;
	const float lineHeight = 11.0f;

	float y = 30.0f;
	dc->Draw2dLabel(10.0f, y, 1.45f, m_queue.empty() ? Col_DarkGray : Col_DarkSlateBlue, false,
	                "Entity Cover Sampler Queue (%" PRISIZE_T ")", m_queue.size());
	y += lineHeight * 1.45f;

	QueuedEntities::const_iterator it = m_queue.begin();
	QueuedEntities::const_iterator end = m_queue.end();

	for (; it != end; ++it)
	{
		const QueuedEntity& queued = *it;

		const char* side = 0;
		const char* name = "<invalid>";

		if (IEntity* entity = gEnv->pEntitySystem->GetEntity(queued.entityID))
			name = entity->GetName();

		if (queued.state == QueuedEntity::Queued)
			dc->Draw2dLabel(10.0f, y, 1.25f, Col_Gray, false, "%s", name);
		else
		{
			switch (queued.state)
			{
			case QueuedEntity::SamplingLeft:
				side = "left";
				break;
			case QueuedEntity::SamplingRight:
				side = "right";
				break;
			case QueuedEntity::SamplingFront:
				side = "front";
				break;
			case QueuedEntity::SamplingBack:
				side = "back";
				break;
			default:
				assert(0);
				break;
			}

			dc->Draw2dLabel(10.0f, y, 1.25f, Col_BlueViolet, false, "%s: %s", name, side);
		}

		y += lineHeight * fontSize;
	}
}
