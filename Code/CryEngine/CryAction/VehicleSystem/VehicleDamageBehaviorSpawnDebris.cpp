// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleDamageBehaviorSpawnDebris.h"

#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehiclePartAnimated.h"

//------------------------------------------------------------------------
CVehicleDamageBehaviorSpawnDebris::CVehicleDamageBehaviorSpawnDebris()
	: m_pVehicle(nullptr)
	, m_pickableDebris(false)
{
}

//------------------------------------------------------------------------
CVehicleDamageBehaviorSpawnDebris::~CVehicleDamageBehaviorSpawnDebris()
{
	Reset();
}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorSpawnDebris::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;

	if (CVehicleParams debrisParams = table.findChild("SpawnDebris"))
	{
		debrisParams.getAttr("pickable", m_pickableDebris);
		m_particleEffect = debrisParams.getAttr("effect");
	}

	int partCount = m_pVehicle->GetPartCount();
	for (int k = 0; k < partCount; k++)
	{
		IVehiclePart* pPart = m_pVehicle->GetPart(k);
		CRY_ASSERT(pPart);
		PREFAST_ASSUME(pPart);

		if (CVehiclePartAnimated* pAnimPart = CAST_VEHICLEOBJECT(CVehiclePartAnimated, pPart))
		{
			ICharacterInstance* pCharInstance = pAnimPart->GetEntity()->GetCharacter(pAnimPart->GetSlot());
			if (pCharInstance)
			{
				IDefaultSkeleton& rIDefaultSkeleton = pCharInstance->GetIDefaultSkeleton();
				int jointCount = rIDefaultSkeleton.GetJointCount();
				for (int jointId = 0; jointId < jointCount; jointId++)
				{
					int i = 1;
					IStatObj* pStatObj = NULL;
					const char* pJointName = rIDefaultSkeleton.GetJointNameByID(jointId);

					while (i < 25)
					{
						pStatObj = pAnimPart->GetDestroyedGeometry(pJointName, i);
						if (pStatObj)
						{
							m_debris.resize(m_debris.size() + 1);
							SDebrisInfo& debrisInfo = m_debris.back();

							debrisInfo.pAnimatedPart = pAnimPart;
							debrisInfo.jointId = jointId;
							debrisInfo.slot = -1;
							debrisInfo.index = i;
							debrisInfo.entityId = 0;
							debrisInfo.time = 0.0f;

#if ENABLE_VEHICLE_DEBUG
							if (VehicleCVars().v_debugdraw == eVDB_Parts)
							{
								CryLog("VehicleDamageBehaviorSpawnDebris[%s]: adding debris part %s", m_pVehicle->GetEntity()->GetName(), pStatObj->GetGeoName());
							}
#endif
						}
						else
							break;

						i++;
					}
				}
			}
		}
	}

	return (m_debris.empty() == false);
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorSpawnDebris::Reset()
{
	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);

	TDebrisInfoList::iterator debrisIte = m_debris.begin();
	TDebrisInfoList::iterator debrisEnd = m_debris.end();

	for (; debrisIte != debrisEnd; ++debrisIte)
	{
		SDebrisInfo& debrisInfo = *debrisIte;
		debrisInfo.time = 0.0f;

		if (debrisInfo.entityId)
		{
			if (GetISystem()->IsSerializingFile() != 1)
				gEnv->pEntitySystem->RemoveEntity(debrisInfo.entityId);
			debrisInfo.entityId = 0;
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorSpawnDebris::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& params)
{
	if (event != eVDBE_Hit && params.hitValue < 1.0f)
		return;

	bool isUpdateNeeded = false;

	TDebrisInfoList::iterator debrisIte = m_debris.begin();
	TDebrisInfoList::iterator debrisEnd = m_debris.end();

	for (; debrisIte != debrisEnd; ++debrisIte)
	{
		SDebrisInfo& debrisInfo = *debrisIte;

		if (!debrisInfo.entityId)
		{
			debrisInfo.time = cry_random(0.0f, 1.0f) * (4.0f - min(3.0f, params.hitValue));
			debrisInfo.force = params.componentDamageRatio;
			isUpdateNeeded = true;

			IEntity* pEntity = debrisInfo.pAnimatedPart->GetEntity();
			CRY_ASSERT(pEntity);

			ICharacterInstance* pCharInstance = debrisInfo.pAnimatedPart->GetEntity()->GetCharacter(
			  debrisInfo.pAnimatedPart->GetSlot());

			if (!pCharInstance)
				return;

			ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
			IDefaultSkeleton& rIDefaultSkeleton = pCharInstance->GetIDefaultSkeleton();
			const char* pJointName = rIDefaultSkeleton.GetJointNameByID(debrisInfo.jointId);

			IStatObj* pDebrisObj = debrisInfo.pAnimatedPart->GetDestroyedGeometry(pJointName, debrisInfo.index);
			if (pDebrisObj)
			{
				Matrix34 vehicleTM = debrisInfo.pAnimatedPart->GetDestroyedGeometryTM(pJointName, debrisInfo.index);
				Matrix34 intactTM = Matrix34((pSkeletonPose->GetAbsJointByID(debrisInfo.jointId)));

				if (IEntity* pDebrisEntity = SpawnDebris(pDebrisObj, vehicleTM * Matrix33(intactTM), debrisInfo.force))
				{
					debrisInfo.entityId = pDebrisEntity->GetId();
					float force = cry_random(0.0f, 2.0f);
					GiveImpulse(pDebrisEntity, force);
					AttachParticleEffect(pEntity);
				}
				else
					debrisInfo.entityId = 0;
			}
		}
	}

	if (isUpdateNeeded)
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorSpawnDebris::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	if (event == eVE_Collision)
	{
		TDebrisInfoList::iterator debrisIte = m_debris.begin();
		TDebrisInfoList::iterator debrisEnd = m_debris.end();

		for (; debrisIte != debrisEnd; ++debrisIte)
		{
			SDebrisInfo& debrisInfo = *debrisIte;
			if (debrisInfo.time > 0.0f)
			{
				if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(debrisInfo.entityId))
				{
					pEntity->DetachThis();
					AttachParticleEffect(pEntity);
					GiveImpulse(pEntity, 1.0f);
				}

				debrisInfo.time = 0.0f;
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorSpawnDebris::Update(const float deltaTime)
{
	const Matrix34& worldTM = m_pVehicle->GetEntity()->GetWorldTM();

	TDebrisInfoList::iterator debrisIte = m_debris.begin();
	TDebrisInfoList::iterator debrisEnd = m_debris.end();

	for (; debrisIte != debrisEnd; ++debrisIte)
	{
		SDebrisInfo& debrisInfo = *debrisIte;

		if (debrisInfo.time > 0.0f)
		{
			debrisInfo.time -= deltaTime;

			if (debrisInfo.time <= 0.0f)
			{
				if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(debrisInfo.entityId))
				{
					pEntity->DetachThis();
					AttachParticleEffect(pEntity);
					GiveImpulse(pEntity, 2.0f);
				}
				debrisInfo.time = 0.0f;
			}
		}
	}
}

//------------------------------------------------------------------------
IEntity* CVehicleDamageBehaviorSpawnDebris::SpawnDebris(IStatObj* pStatObj, Matrix34 vehicleTM, float force)
{
	IEntity* pVehicleEntity = m_pVehicle->GetEntity();
	CRY_ASSERT(pVehicleEntity);

	// spawn the detached entity

	char buffer[128];
	cry_sprintf(buffer, "%s_DetachedPart_%s", m_pVehicle->GetEntity()->GetName(), pStatObj->GetGeoName());

	SEntitySpawnParams spawnParams;
	spawnParams.sName = buffer;
	spawnParams.nFlags = ENTITY_FLAG_CLIENT_ONLY;
	if (!m_pickableDebris)
		spawnParams.nFlags |= ENTITY_FLAG_NO_PROXIMITY;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("VehiclePartDetached");
	if (!spawnParams.pClass)
		return NULL;

	IEntity* pSpawnedDebris = gEnv->pEntitySystem->SpawnEntity(spawnParams, true);

	if (!pSpawnedDebris)
		return NULL;

	// place the geometry on the new entity
	int slot = pSpawnedDebris->SetStatObj(pStatObj, -1, true, 200.0f);

	pSpawnedDebris->SetWorldTM(m_pVehicle->GetEntity()->GetWorldTM() * vehicleTM);

#if ENABLE_VEHICLE_DEBUG
	if (VehicleCVars().v_debugdraw == eVDB_Parts)
	{
		CryLog("VehicleDamageBehaviorSpawnDebris[%s]: spawned debris part %s (offfset %i %i %i)", m_pVehicle->GetEntity()->GetName(), pStatObj->GetGeoName(), (int)vehicleTM.GetTranslation().x, (int)vehicleTM.GetTranslation().y, (int)vehicleTM.GetTranslation().z);
	}
#endif

	SEntityPhysicalizeParams physicsParams;

	if (!pStatObj->GetPhysicalProperties(physicsParams.mass, physicsParams.density))
		physicsParams.mass = 200.0f;

	physicsParams.type = PE_RIGID;
	physicsParams.nFlagsOR &= pef_log_collisions;
	physicsParams.nSlot = 0;
	pSpawnedDebris->Physicalize(physicsParams);

	if (IPhysicalEntity* pPhysEntity = pSpawnedDebris->GetPhysics())
	{
		pe_params_buoyancy buoyancy;
		buoyancy.waterDensity = 0.15f;
		buoyancy.waterResistance = 30.0f;
		pPhysEntity->SetParams(&buoyancy);

		m_spawnedDebris.push_back(pSpawnedDebris->GetId());
		return pSpawnedDebris;
	}
	else
	{
		return NULL;
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorSpawnDebris::AttachParticleEffect(IEntity* pDetachedEntity)
{
	if (m_particleEffect.empty())
		return;

	if (IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(m_particleEffect.c_str(), "VehicleDamageBehaviorEffect"))
	{
		int slot = pDetachedEntity->LoadParticleEmitter(-1, pEffect, NULL, false, true);

		if (IParticleEmitter* pParticleEmitter = pDetachedEntity->GetParticleEmitter(slot))
		{
			SpawnParams spawnParams;
			spawnParams.fSizeScale = cry_random(0.5f, 1.0f);

			pParticleEmitter->SetSpawnParams(spawnParams);
		}
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorSpawnDebris::GiveImpulse(IEntity* pDetachedEntity, float force)
{
	IPhysicalEntity* pDebrisPhysEntity = pDetachedEntity->GetPhysics();
	IPhysicalEntity* pVehiclePhysEntity = m_pVehicle->GetEntity()->GetPhysics();
	if (!pDebrisPhysEntity || !pVehiclePhysEntity)
		return;

	pe_action_impulse imp;
	Vec3 randomVel;

	pe_status_dynamics debrisDyn;
	pe_status_dynamics vehicleDyn;
	pDebrisPhysEntity->GetStatus(&debrisDyn);
	pVehiclePhysEntity->GetStatus(&vehicleDyn);

	randomVel.x = cry_random(0.0f, 1.0f);
	randomVel.y = cry_random(0.0f, 1.0f);
	randomVel.z = cry_random(0.0f, 1.0f);

	imp.impulse = Vec3(vehicleDyn.v.x * randomVel.x, vehicleDyn.v.y * randomVel.y, vehicleDyn.v.z * randomVel.z);
	imp.impulse *= debrisDyn.mass * force;

	randomVel.x = cry_random(0.0f, 1.0f);
	randomVel.y = cry_random(0.0f, 1.0f);
	randomVel.z = cry_random(0.0f, 1.0f);

	imp.angImpulse = Vec3(vehicleDyn.w.x * randomVel.x, vehicleDyn.w.y * randomVel.y, vehicleDyn.w.z * randomVel.z);
	imp.angImpulse *= debrisDyn.mass * force;

	pDebrisPhysEntity->Action(&imp);
}

void CVehicleDamageBehaviorSpawnDebris::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
	s->AddObject(m_spawnedDebris);
	s->AddObject(m_debris);
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorSpawnDebris);
