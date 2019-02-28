// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   MissLocationSensor.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - 29 Mar 2010			 : Alex McCarthy: extracted from AIPlayer.h/cpp

 *********************************************************************/
#include "StdAfx.h"
#include "MissLocationSensor.h"
#include "DebugDrawContext.h"

CMissLocationSensor::CMissLocationSensor(const CAIActor* owner)
	: m_state(Starting)
	, m_owner(owner)
	, m_lastCollectionLocation(ZERO)
{
	AddDestroyableClass("DestroyableObject");
	AddDestroyableClass("BreakableObject");
	AddDestroyableClass("PressurizedObject");
}

CMissLocationSensor::~CMissLocationSensor()
{
	ClearEntities();
}

void CMissLocationSensor::Reset()
{
	ClearEntities();

	MissLocations().swap(m_working);
	MissLocations().swap(m_locations);
	MissLocations().swap(m_goodies);

	m_lastCollectionLocation.zero();
}

void CMissLocationSensor::Update(float timeLimit)
{
	while (true)
	{
		switch (m_state)
		{
		case Starting:
			{
				m_updateCount = 0;

				if ((m_lastCollectionLocation - m_owner->GetPhysicsPos()).len2() > sqr(0.5f))
				{
					m_state = Collecting;
					break;
				}
			}
			return;
		case Collecting:
			{
				Collect(ent_static | ent_rigid | ent_sleeping_rigid | ent_independent);
				m_state = Filtering;
			}
			return;
		case Filtering:
			{
				if (Filter(timeLimit))
				{
					m_state = Finishing;
					break;
				}
			}
			return;
		case Finishing:
			{
				m_working.swap(m_locations);
				m_working.resize(0);
				m_state = Starting;
			}
			return;
		default:
			{
				assert(0);
				return;
			}
		}
	}

	++m_updateCount;
}

void CMissLocationSensor::Collect(int types)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	const float boxHalfSize = gAIEnv.CVars.CoolMissesBoxSize * 0.5f;
	const float boxHeight = gAIEnv.CVars.CoolMissesBoxHeight;

	const Vec3& feet = m_owner->GetPhysicsPos();
	const Vec3& dir = m_owner->GetViewDir();

	Vec3 pos = feet + Vec3(0.0f, 0.0f, -0.25f) + dir * boxHalfSize * 0.5f;
	Vec3 min(pos.x - boxHalfSize, pos.y - boxHalfSize, pos.z);
	Vec3 max(pos.x + boxHalfSize, pos.y + boxHalfSize, pos.z + boxHeight);

	types |= ent_addref_results;

	ClearEntities();
	m_entities.resize(MaxCollectedCount);

	IPhysicalEntity** entities = &m_entities.front();
	size_t entityCount = gEnv->pPhysicalWorld->GetEntitiesInBox(min, max, entities, types, MaxCollectedCount);

	if (entityCount > MaxCollectedCount)
	{
		assert(entities != &m_entities.front());

		for (size_t i = 0; i < entityCount; ++i)
		{
			if (entities[i])
				entities[i]->Release();
		}

		m_entities.resize(0);
	}
	else
		m_entities.resize(entityCount);

	m_lastCollectionLocation = feet;
}

bool CMissLocationSensor::Filter(float timeLimit)
{
	if (m_entities.empty())
		return true;

	CTimeValue now = gEnv->pTimer->GetAsyncTime();
	CTimeValue start = now;
	CTimeValue endTime = now + CTimeValue(timeLimit);

	float MaxMass = gAIEnv.CVars.CoolMissesMaxLightweightEntityMass;

	pe_params_part pp;
	pe_status_dynamics dyn;
	pe_params_rope prope;
	pe_status_pos spos;
	pe_params_structural_joint sjp;
	pe_status_rope srope;

	do
	{
		IPhysicalEntity* entity = m_entities.front();
		std::swap(m_entities.front(), m_entities.back());
		m_entities.pop_back();

		bool destroyable = false;
		bool lightweight = false;

		if (IEntity* ientity = gEnv->pEntitySystem->GetEntityFromPhysics(entity))
		{
			IEntityClass* entityClass = ientity->GetClass();
			for (uint c = 0; c < m_destroyableEntityClasses.size(); ++c)
			{
				if (entityClass == m_destroyableEntityClasses[c])
				{
					destroyable = true;
					break;
				}
			}

			MARK_UNUSED dyn.partid, dyn.ipart;

			if (entity->GetStatus(&dyn) && (dyn.mass > 0.0001f) && (dyn.mass <= MaxMass))
				lightweight = true;
		}

		// Check for idmatBreakable or geom_manually_breakable
		// If the entity was previous known to be destroyable add all it's physical parts
		if (entity)
		{
			Matrix34 worldTM;
			Matrix34 partTM;

			spos.ipart = -1;
			spos.partid = -1;
			spos.pMtx3x4 = &worldTM;

			if (entity->GetStatus(&spos))
			{
				{
					pp.pMtx3x4 = &partTM;
					pp.ipart = 0;
					MARK_UNUSED pp.partid;

					uint32 partCount = 0;
					while (entity->GetParams(&pp))
					{
						MARK_UNUSED pp.partid;
						pp.ipart = ++partCount;

						if (!pp.pPhysGeom || !pp.pPhysGeom->pGeom)
							continue;

						primitives::box box;
						pp.pPhysGeom->pGeom->GetBBox(&box);

						Vec3 pos = (worldTM * partTM).TransformPoint(box.center);

						if (!destroyable && !lightweight)
						{
							if (pp.flagsOR & geom_manually_breakable)
							{
								m_working.push_back(
								  MissLocation(pos, MissLocation::ManuallyBreakable));
							}

							if (pp.idmatBreakable >= 0)
							{
								m_working.push_back(
								  MissLocation(pos, MissLocation::MatBreakable));
							}

							if (pp.idSkeleton >= 0)
							{
								m_working.push_back(
								  MissLocation(pos, MissLocation::Deformable));
							}
						}
						else
						{
							m_working.push_back(
							  MissLocation(pos, lightweight ? MissLocation::Lightweight : MissLocation::Destroyable));
						}
					}
				}

				// Check if entity contains structural joints
				// Track which parts are connected by joints
				// Add those parts as good miss locations
				{
					uint64 jointConnectedParts = 0;
					uint32 jointCount = 0;
					sjp.idx = 0;

					while (entity->GetParams(&sjp))
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

					if (jointConnectedParts)
					{
						for (uint32 p = 0; p < 64; ++p)
						{
							if (jointConnectedParts & (1ll << p))
							{
								spos.ipart = -1;
								spos.partid = p;
								spos.pMtx3x4 = &partTM;

								if (!entity->GetStatus(&spos))
									continue;

								if (!spos.pGeom)
									continue;

								primitives::box box;
								spos.pGeom->GetBBox(&box);

								m_working.push_back(
								  MissLocation((worldTM * partTM).TransformPoint(box.center), MissLocation::JointStructure));
							}
						}
					}
				}

				// Add rope vertices
				if (entity->GetStatus(&srope) && entity->GetParams(&prope))
				{
					if (prope.pEntTiedTo[0] && prope.pEntTiedTo[1])
					{
						uint32 pointCount = std::max(srope.nVtx, (srope.nSegments + 1)); // use the version with the most detail
						if ((pointCount > 0) && (pointCount < 128))
						{
							Vec3 vertices[128];
							uint32 step = 1;

							if (pointCount > MaxRopeVertexCount)
								step = pointCount / MaxRopeVertexCount;

							if (srope.nVtx >= (srope.nSegments + 1))
								srope.pVtx = &vertices[0];
							else
								srope.pPoints = &vertices[0];

							entity->GetStatus(&srope);

							for (uint i = 0; i < pointCount; i += step)
								m_working.push_back(MissLocation(vertices[i], MissLocation::Rope));
						}
					}
				}
			}

			entity->Release();
		}

		now = gEnv->pTimer->GetAsyncTime();

		if (m_entities.empty())
			return true;

	}
	while (now < endTime);

	return false;
}

bool CMissLocationSensor::GetLocation(CAIObject* target, const Vec3& shootPos, const Vec3& shootDir, float maxAngle, Vec3& pos)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (m_locations.empty())
		return false;

	m_goodies.resize(0);

	float maxAngleCos = cos_tpl(maxAngle);
	float angleIntervalInv = 1.0f / (1.0f - maxAngleCos);

	const Vec3 targetViewDir = target->GetViewDir();
	const Vec3 targetLocation = target->GetPos();

	uint32 maxConsidered = std::min<uint32>(MaxConsiderCount, m_locations.size());

	MissLocations::iterator it = m_locations.begin();
	MissLocations::iterator end = m_locations.end();

	for (; maxConsidered && (it != end); ++it)
	{
		MissLocation& location = *it;

		Vec3 dir(location.position - shootPos);
		dir.Normalize();

		float angleCos = dir.dot(shootDir);
		if (angleCos <= maxAngleCos)
			continue;

		float extraScore = min(1.0f + (location.position - targetLocation).dot(targetViewDir), 1.0f);
		//if ((location.position - targetLocation).dot(targetViewDir) <= 0.0f)
		//continue;

		// Ignore anything that would hit the player
		if (IPhysicalEntity* ownerPhysics = m_owner->GetPhysics())
		{
			pe_status_pos ppos;
			if (ownerPhysics->GetStatus(&ppos))
			{
				AABB player(ppos.BBox[0] - ppos.pos, ppos.BBox[1] + ppos.pos);
				player.Expand(Vec3(0.35f));

				Lineseg lineOfFire;
				lineOfFire.start = shootPos;

				uint32 locationCount = m_goodies.size();
				for (uint32 i = 0; (i < locationCount) && (i < MaxRandomPool); ++i)
				{
					lineOfFire.end = m_goodies[i].position;

					if (Overlap::Lineseg_AABB(lineOfFire, player))
						continue;
				}
			}
		}

		float typeScore = 0.0f;
		switch (location.type)
		{
		case MissLocation::Lightweight:
			typeScore = 1.0f;
			break;
		case MissLocation::Destroyable:
			typeScore = 0.95f;
			break;
		case MissLocation::Rope:
			typeScore = 0.90f;
			break;
		case MissLocation::ManuallyBreakable:
			typeScore = 0.85f;
			break;
		case MissLocation::JointStructure:
			typeScore = 0.75f;
			break;
		case MissLocation::Deformable:
			typeScore = 0.65f;
			break;
		case MissLocation::MatBreakable:
			typeScore = 0.5f;
			break;
		case MissLocation::Unbreakable:
		default:
			break;
		}

		float angleScore = (angleCos - maxAngleCos) * angleIntervalInv;

		m_goodies.push_back(location);
		m_goodies.back().score = (angleScore * 0.4f) + (typeScore * 0.6f);
		m_goodies.back().score *= extraScore;
		--maxConsidered;
	}

	size_t goodiesCount = std::min<size_t>(MaxRandomPool, m_goodies.size());
	std::partial_sort(m_goodies.begin(), m_goodies.begin() + goodiesCount, m_goodies.end());

	if (m_goodies.empty())
		return false;

	const MissLocation& location = m_goodies[cry_random((size_t)0, goodiesCount - 1)];

#ifdef CRYAISYSTEM_DEBUG
	if (gAIEnv.CVars.DebugDrawCoolMisses)
	{
		GetAISystem()->AddDebugCone(location.position + Vec3(0.0f, 0.0f, 0.75f), Vec3(0.0f, 0.0f, -1.0f), 0.225f, 0.35f,
		                            Col_Green, 0.5f);
	}
#endif

	pos = location.position;

	return true;
}

void CMissLocationSensor::DebugDraw()
{
#ifdef CRYAISYSTEM_DEBUG
	if (gAIEnv.CVars.DebugDrawCoolMisses)
	{
		CDebugDrawContext dc;

		for (size_t i = 0; i < m_locations.size(); ++i)
		{
			const MissLocation& location = m_locations[i];

			dc->DrawSphere(location.position + Vec3(0.0f, 0.0f, 0.3f), 0.2f, Col_Sienna);
		}
	}
#endif
}

void CMissLocationSensor::AddDestroyableClass(const char* className)
{
	if (IEntityClass* entityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(className))
		stl::push_back_unique(m_destroyableEntityClasses, entityClass);
}

void CMissLocationSensor::ResetDestroyableClasses()
{
	m_destroyableEntityClasses.clear();
}

void CMissLocationSensor::ClearEntities()
{
	MissEntities::iterator it = m_entities.begin();
	MissEntities::iterator end = m_entities.end();

	for (; it != end; ++it)
	{
		if ((*it))
		{
			(*it)->Release();
		}
	}

	m_entities.clear();
}
