// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GroundEffect.h"
#include <CryAction/IMaterialEffects.h>
#include "CryAction.h"

const float InvalidRayWorldIntersectDistance = -1.0f;

CGroundEffect::CGroundEffect(IEntity* pEntity) :
	m_pEntity(pEntity),
	m_pParticleEffect(NULL),
	m_flags(eGEF_AlignToGround | eGEF_AlignToOcean), m_slot(-1), m_surfaceIdx(0), m_rayWorldIntersectSurfaceIdx(0),
	m_raycastID(0),
	m_active(false),
	m_stopped(false),
	m_validRayWorldIntersect(false),
	m_height(10.0f), m_rayWorldIntersectHeight(0.0f), m_ratio(1.0f),
	m_sizeScale(1.0f), m_sizeGoal(1.0f),
	m_countScale(1.0f),
	m_speedScale(1.0f), m_speedGoal(1.0f),
	m_interpSpeed(5.0f),
	m_maxHeightCountScale(1.0f), m_maxHeightSizeScale(1.0f)
{
}

CGroundEffect::~CGroundEffect()
{
	Reset();

	if (m_raycastID != 0)
	{
		CCryAction::GetCryAction()->GetPhysicQueues().GetRayCaster().Cancel(m_raycastID);
		m_raycastID = 0;
	}
}

void CGroundEffect::SetHeight(float height)
{
	m_height = height;

	Reset();
}

void CGroundEffect::SetHeightScale(float countScale, float sizeScale)
{
	m_maxHeightCountScale = countScale;
	m_maxHeightSizeScale = sizeScale;
}

void CGroundEffect::SetBaseScale(float sizeScale, float countScale, float speedScale)
{
	m_sizeGoal = sizeScale;
	m_countScale = countScale;
	m_speedGoal = speedScale;
}

void CGroundEffect::SetInterpolation(float speed)
{
	CRY_ASSERT(speed >= 0.f);

	m_interpSpeed = speed;
}

void CGroundEffect::SetFlags(int flags)
{
	m_flags = flags;
	m_active = false;
}

int CGroundEffect::GetFlags() const
{
	return m_flags;
}

bool CGroundEffect::SetParticleEffect(const char* pName)
{
	if (pName)
	{
		m_pParticleEffect = gEnv->pParticleManager->FindEffect(pName);
	}
	else
	{
		m_pParticleEffect = NULL;
	}

	if (m_active)
	{
		Reset();
	}

	m_stopped = false;

	if (!m_pParticleEffect || !m_pEntity)
	{
		return false;
	}

	if (DebugOutput())
	{
		CryLog("[GroundEffect] <%s> set particle effect to (%s)", m_pEntity->GetName(), m_pParticleEffect->GetName());
	}

	return true;
}

void CGroundEffect::SetInteraction(const char* pName)
{
	m_interaction = pName;
}

void CGroundEffect::Update()
{
	if (!m_stopped)
	{
		bool prevActive = m_active;

		int objTypes = ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid;

		int flags = rwi_colltype_any | rwi_ignore_noncolliding | rwi_stop_at_pierceable;

		Vec3 entityPos(m_pEntity->GetWorldPos());

		float refHeight = (m_flags & eGEF_AlignToGround) ? gEnv->p3DEngine->GetTerrainElevation(entityPos.x, entityPos.y) : 0.0f;

		if (m_flags & eGEF_AlignToOcean)
		{
			objTypes |= ent_water;

			refHeight = max(refHeight, gEnv->p3DEngine->GetWaterLevel(&entityPos));
		}

		if (m_interpSpeed > 0.0f)
		{
			float dt = gEnv->pTimer->GetFrameTime();

			Interpolate(m_sizeScale, m_sizeGoal, m_interpSpeed, dt);

			Interpolate(m_speedScale, m_speedGoal, m_interpSpeed, dt);
		}
		else
		{
			m_sizeScale = m_sizeGoal;
			m_speedScale = m_speedGoal;
		}

		Vec3 rayPos(entityPos.x, entityPos.y, entityPos.z + max(0.f, refHeight - entityPos.z + 0.01f));

		if (DeferredRayCasts())
		{
			if (m_raycastID == 0)
			{
				PhysSkipList skipList;

				if (IPhysicalEntity* pPhysics = m_pEntity->GetPhysics())
				{
					skipList.push_back(pPhysics);
				}

				m_raycastID = CCryAction::GetCryAction()->GetPhysicQueues().GetRayCaster().Queue(RayCastRequest::HighPriority, RayCastRequest(rayPos, Vec3(0.0f, 0.0f, -m_height), objTypes, flags, &skipList[0], skipList.size()), functor(*this, &CGroundEffect::OnRayCastDataReceived));
			}
		}
		else
		{
			ray_hit rayhit;

			if (gEnv->pPhysicalWorld->RayWorldIntersection(rayPos, Vec3(0, 0, -m_height), objTypes, flags, &rayhit, 1, m_pEntity->GetPhysics()))
			{
				m_validRayWorldIntersect = true;
				m_rayWorldIntersectHeight = rayhit.pt.z;
				m_rayWorldIntersectSurfaceIdx = rayhit.surface_idx;
			}
			else
			{
				m_validRayWorldIntersect = false;
			}
		}

		m_active = m_validRayWorldIntersect;
		m_ratio = (rayPos.z - m_rayWorldIntersectHeight) / m_height;

		// Has surface changed?

		bool newEffect = false;

		if ((m_rayWorldIntersectSurfaceIdx != m_surfaceIdx) && !m_interaction.empty())
		{
			const char* pEffectName = NULL;

			IMaterialEffects* pMaterialEffects = CCryAction::GetCryAction()->GetIMaterialEffects();

			TMFXEffectId effectId = pMaterialEffects->GetEffectId(m_interaction.c_str(), m_rayWorldIntersectSurfaceIdx);

			if (effectId != InvalidEffectId)
			{
				SMFXResourceListPtr pList = pMaterialEffects->GetResources(effectId);

				if (pList && pList->m_particleList)
				{
					pEffectName = pList->m_particleList->m_particleParams.name;
				}
			}

			if (DebugOutput())
			{
				const char* pMaterialName = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(m_rayWorldIntersectSurfaceIdx)->GetName();

				CryLog("[GroundEffect] <%s> GetEffectString for matId %i (%s) returned <%s>", m_pEntity->GetName(), m_rayWorldIntersectSurfaceIdx, pMaterialName, pEffectName ? pEffectName : "null");
			}

			newEffect = SetParticleEffect(pEffectName);
			m_surfaceIdx = m_rayWorldIntersectSurfaceIdx;
		}

		// Has status changed?

		if ((m_active != prevActive) || newEffect)
		{
			if (m_active && m_pParticleEffect)
			{
				bool prime = (m_flags & eGEF_PrimeEffect) != 0;

				m_slot = m_pEntity->LoadParticleEmitter(m_slot, m_pParticleEffect, 0, prime);

				if (DebugOutput())
				{
					CryLog("[GroundEffect] <%s> effect %s loaded to slot %i", m_pEntity->GetName(), m_pParticleEffect->GetName(), m_slot);
				}
			}
			else
			{
				if (DebugOutput())
				{
					CryLog("[GroundEffect] <%s> slot %i freed", m_pEntity->GetName(), m_slot);
				}

				Reset();
			}
		}

		if (m_active && (m_flags & eGEF_StickOnGround))
		{
			Matrix34 effectTM(Matrix33(IDENTITY), Vec3(entityPos.x, entityPos.y, m_rayWorldIntersectHeight));

			m_pEntity->SetSlotLocalTM(m_slot, m_pEntity->GetWorldTM().GetInverted() * effectTM);
		}

		if (m_active && m_pParticleEffect)
		{
			SpawnParams spawnParams;

			spawnParams.fSizeScale = m_sizeScale;
			spawnParams.fCountScale = m_countScale;
			spawnParams.fSpeedScale = m_speedScale;

			spawnParams.fSizeScale *= (1.0f - m_maxHeightSizeScale) * (1.0f - m_ratio) + m_maxHeightSizeScale;
			spawnParams.fCountScale *= (1.0f - m_maxHeightCountScale) * (1.0f - m_ratio) + m_maxHeightCountScale;

			SetSpawnParams(spawnParams);
		}

		if (DebugOutput())
		{
			Vec3 pos = (m_slot != -1) ? m_pEntity->GetSlotWorldTM(m_slot).GetTranslation() : entityPos;

			const char* pEffectName = m_pParticleEffect ? m_pParticleEffect->GetName() : "";

			IRenderAuxText::DrawLabelF(pos + Vec3(0.0f, 0.0f, 0.5f), 1.25f, "GO [interaction '%s'], effect '%s', active: %i", m_interaction.c_str(), pEffectName, m_active);

			IRenderAuxText::DrawLabelF(pos + Vec3(0.0f, 0.0f, 0.0f), 1.25f, "height %.1f/%.1f, base size/count scale %.2f/%.2f", rayPos.z - m_rayWorldIntersectHeight, m_height, m_sizeScale, m_countScale);
		}
	}
}

void CGroundEffect::Stop(bool stop)
{
	if (stop)
	{
		Reset();
	}

	m_stopped = stop;
}

void CGroundEffect::OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result)
{
	CRY_ASSERT_MESSAGE(rayID == m_raycastID, "CGroundEffect: Received raycast data with mismatching id");

	m_validRayWorldIntersect = result.hitCount > 0;

	if (m_validRayWorldIntersect)
	{
		m_rayWorldIntersectHeight = result->pt.z;
		m_rayWorldIntersectSurfaceIdx = result->surface_idx;
	}

	m_raycastID = 0;
}

void CGroundEffect::SetSpawnParams(const SpawnParams& params)
{
	if (m_slot < 0)
	{
		return;
	}

	SEntitySlotInfo info;

	if (m_pEntity->GetSlotInfo(m_slot, info) && info.pParticleEmitter)
	{
		info.pParticleEmitter->SetSpawnParams(params);
	}
}

void CGroundEffect::Reset()
{
	if (m_slot != -1)
	{
		if (m_pEntity)
		{
			SEntitySlotInfo info;

			if (m_pEntity->GetSlotInfo(m_slot, info) && info.pParticleEmitter)
			{
				info.pParticleEmitter->Activate(false);
			}

			m_pEntity->FreeSlot(m_slot);
		}

		m_slot = -1;
	}

	m_active = false;
}
