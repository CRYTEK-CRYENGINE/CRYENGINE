// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeflectorShield.h"
#include "Projectile.h"
#include "WeaponSystem.h"
#include <CryAISystem/IAIObjectManager.h>


void CDeflectorShield::GetMemoryUsage(ICrySizer *pSizer) const {}
void CDeflectorShield::InitClient(int channelId) {}
void CDeflectorShield::PostInitClient(int channelId) {}
void CDeflectorShield::PostReloadExtension(IGameObject * pGameObject, const SEntitySpawnParams &params) {}
bool CDeflectorShield::GetEntityPoolSignature(TSerialize signature) {return true;}
void CDeflectorShield::Release() {}
void CDeflectorShield::FullSerialize(TSerialize ser) {}
bool CDeflectorShield::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags) {return true;}
void CDeflectorShield::PostSerialize() {}
void CDeflectorShield::SerializeSpawnInfo(TSerialize ser) {}
ISerializableInfoPtr CDeflectorShield::GetSpawnInfo() {return ISerializableInfoPtr();}
void CDeflectorShield::ProcessEvent(const SEntityEvent& event) {}
void CDeflectorShield::SetChannelId(uint16 id) {}
const void * CDeflectorShield::GetRMIBase() const {return 0;}
void CDeflectorShield::PostUpdate(float frameTime) {}
void CDeflectorShield::PostRemoteSpawn() {}

namespace DS
{
	void RegisterEvents( IGameObjectExtension& goExt, IGameObject& gameObject )
	{
		const int eventID = eGFE_OnCollision;
		gameObject.UnRegisterExtForEvents( &goExt, NULL, 0 );
		gameObject.RegisterExtForEvents( &goExt, &eventID, 1 );
	}
}

CDeflectorShield::CDeflectorShield()
	:	m_minDamage(50)
	,	m_maxDamage(500)
	,	m_dropMinDistance(10.0f)
	,	m_dropPerMeter(10.0f)
	,	m_spread(0.0f)
	,	m_hitTypeId(0)
	,	m_pAmmoClass(0)
	,	m_pDeflectedEffect(0)
	,	m_energyRadius(0.4f)
	,	m_invEnergyDelay(0.75f)
	,	m_NonDeflectedOwnersGroupID(0)
{
}



bool CDeflectorShield::Init(IGameObject * pGameObject)
{
	SetGameObject(pGameObject);
	GetGameObject()->EnablePhysicsEvent(true, eEPE_OnCollisionLogged);
	LoadScriptProperties();

	return true;
}

void CDeflectorShield::PostInit(IGameObject * pGameObject)
{
	DS::RegisterEvents( *this, *pGameObject );
}

bool CDeflectorShield::ReloadExtension(IGameObject * pGameObject, const SEntitySpawnParams &params)
{
	ResetGameObject();
	DS::RegisterEvents( *this, *pGameObject );
	return true;
}



void CDeflectorShield::Update(SEntityUpdateContext& ctx, int updateSlot)
{
	if (updateSlot != 0)
		return;
	UpdateDeflectedEnergies(ctx.fFrameTime);
	DebugDraw();
}



void CDeflectorShield::HandleEvent(const SGameObjectEvent& event)
{
	if (event.event == eGFE_OnCollision)
	{
		EventPhysCollision* pCollision = (EventPhysCollision*)event.ptr;
		ProcessCollision(*pCollision);
	}
}


// ===========================================================================
//	Set the group ID for which its owners can fire projectiles that
//	are not deflected by the shield.
//
//	In:		All the projectiles that have an owner that is a member of this
//			group's ID will not be deflected by the shield (and can thus
//			pass through it unhindered) (0 will disable).
//
void CDeflectorShield::SetNonDeflectedOwnerGroup(int groupID)
{
	m_NonDeflectedOwnersGroupID = groupID;
}


// ===========================================================================
//	Enable/disable physics collision response.
//
//	In:		True if we should have normal physics responses; false if the 
//			shield should have no actual physical presence (so, projectiles
//			will still get deflected, but anything else can pass through it).
//
void CDeflectorShield::SetPhysicsCollisionResponse(bool normalCollResonseFlag)
{
	pe_params_part colltype;

	if (normalCollResonseFlag)
	{
		colltype.flagsAND = ~geom_no_coll_response;
	}
	else
	{
		colltype.flagsOR = geom_no_coll_response;
	}
	
	GetEntity()->GetPhysics()->SetParams(&colltype);
}


// ============================================================================
//	Query if the shield is depleted.
//
//  Note: The shield can only be depleted if it is not configured to be
//  'invulnerable'.
//
//	Returns:	True if depleted; otherwise false.
//
bool CDeflectorShield::IsDepleted() const
{
	// 'Dead' and 'depleted' are same for the deflector shield.
	return IsDead();
}


// ============================================================================
//	Make the shield fully recharged (and visible).
//
//  Note: We should always be allowed to call this function, even though the
//	shield might be set to 'invulnerable', because the owner of the shield
//	might decide to raise or lower the shield voluntarily.
//
void CDeflectorShield::Recharged()
{	
	IEntity* entity = GetEntity();
	assert(entity != NULL);
	EntityScripts::CallScriptFunction(entity, entity->GetScriptTable(), "Recharged");
}


// ============================================================================
//	Make the shield fully depleted (and hidden).
//
//  Note: We should always be allowed to call this function, even though the
//	shield might be set to 'invulnerable', because the owner of the shield
//	might decide to raise or lower the shield voluntarily.
//
void CDeflectorShield::Depleted()
{	
	PurgeDeflectedEnergiesBuffer();

	// Hide the shield and 'kill' it basically.
	IEntity* entity = GetEntity();
	assert(entity != NULL);
	EntityScripts::CallScriptFunction(entity, entity->GetScriptTable(), "Depleted");
}


// ============================================================================
//	Enable/disable invulnerability.
//
//	In:		True if the shield should be invulnerable (takes no damage); false
//			if not.
//
void CDeflectorShield::SetInvulnerability(const bool invulnerableFlag)
{
	IEntity* entity = GetEntity();
	assert(entity != NULL);

	EntityScripts::CallScriptFunction(entity, entity->GetScriptTable(), 
		"SetInvulnerability", invulnerableFlag);
}


void CDeflectorShield::LoadScriptProperties()
{
	SmartScriptTable pScriptTable = GetEntity()->GetScriptTable();
	if (!pScriptTable)
		return;

	SmartScriptTable pProperties;
	if (!pScriptTable->GetValue("Properties", pProperties))
		return;

	const char* hitType = 0;
	const char* ammoType = 0;
	const char* particleFx = 0;
	float energyDelay;

	pProperties->GetValue("MinDamage", m_minDamage);
	pProperties->GetValue("MaxDamage", m_maxDamage);
	pProperties->GetValue("DropMinDistance", m_dropMinDistance);
	pProperties->GetValue("DropPerMeter", m_dropPerMeter);
	pProperties->GetValue("Spread", m_spread);
	pProperties->GetValue("EnergyRadius", m_energyRadius);
	pProperties->GetValue("EnergyDelay", energyDelay);
	pProperties->GetValue("HitType", hitType);
	pProperties->GetValue("AmmoType", ammoType);
	pProperties->GetValue("ParticleEffect", particleFx);

	if (hitType)
		m_hitTypeId = g_pGame->GetGameRules()->GetHitTypeId(hitType);
	if (ammoType)
		m_pAmmoClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(ammoType);
	if (particleFx)
		m_pDeflectedEffect = gEnv->p3DEngine->GetParticleManager()->FindEffect(particleFx);

	m_invEnergyDelay = 1.0f / (energyDelay + FLT_EPSILON);

	if (m_pAmmoClass)
		PreCacheAmmoResources();
	else
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "No ammo type specified for Deflector Shield '%s'", GetEntity()->GetName());
}



void CDeflectorShield::PreCacheAmmoResources()
{
	const SAmmoParams* pParams = g_pGame->GetWeaponSystem()->GetAmmoParams(m_pAmmoClass);
	if (pParams)
	 pParams->CacheResources();
	else
	 CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Deflector Shield '%s' specifies invalid ammo type", GetEntity()->GetName());
}



void CDeflectorShield::ProcessCollision(const EventPhysCollision& pCollision)
{
	int id = 0;
	IPhysicalEntity* pTarget = pCollision.pEntity[id];
	if (pTarget == GetEntity()->GetPhysics())
	{
		id = 1;
		pTarget = pCollision.pEntity[id];
	}

	IEntity* pTargetEntity = (IEntity*)pTarget->GetForeignData(PHYS_FOREIGN_ID_ENTITY);
	if (pTargetEntity == 0)
		return;

	CProjectile* pProjectile = g_pGame->GetWeaponSystem()->GetProjectile(pTargetEntity->GetId());

	if (pProjectile)
		ProcessProjectile(pProjectile, pCollision.pt, pCollision.n, pCollision.vloc[id].GetNormalized());
}



void CDeflectorShield::ProcessProjectile(CProjectile* pProjectile, Vec3 hitPosition, Vec3 hitNormal, Vec3 hitDirection)
{
	if (CanProjectilePassThroughShield(pProjectile))
	{
		return;
	}

	const Matrix34& intoWorldTransform = GetEntity()->GetWorldTM();
	Matrix34 intoLocalTransform = intoWorldTransform.GetInvertedFast();

	SDeflectedEnergy deflectedEnergy;
	deflectedEnergy.m_delay = 0.0f;
	deflectedEnergy.m_localPosition = intoLocalTransform.TransformPoint(hitPosition);
	deflectedEnergy.m_localDirection = intoLocalTransform.TransformVector(-hitDirection);
	deflectedEnergy.m_damage = CLAMP(pProjectile->GetDamage(), m_minDamage, m_maxDamage);

	if (deflectedEnergy.m_localDirection.NormalizeSafe() > 0.0f)
	{
		m_deflectedEnergies.push_back(deflectedEnergy);
		GetGameObject()->EnableUpdateSlot(this, 0);
	}

	if (m_pDeflectedEffect)
	{
		m_pDeflectedEffect->Spawn(IParticleEffect::ParticleLoc(hitPosition, hitNormal));
	}

	pProjectile->Destroy();
}


void CDeflectorShield::UpdateDeflectedEnergies(float deltaTime)
{
	TDeflectedEnergies::iterator it;

	for (it = m_deflectedEnergies.begin(); it != m_deflectedEnergies.end(); ++it)
		it->m_delay += deltaTime * m_invEnergyDelay;

	while (!m_deflectedEnergies.empty() && m_deflectedEnergies.front().m_delay > 1.0f)
	{
		ShootDeflectedEnergy(*m_deflectedEnergies.begin());
		m_deflectedEnergies.pop_front();
	}

	ValidateUpdateSlot();
}


// ===========================================================================
//	Validate the update slot.
//
//	Make sure we are not requesting updates when there are no deflected 
//  energies pending in the buffer.
//	
void CDeflectorShield::ValidateUpdateSlot()
{
	if (m_deflectedEnergies.empty())
	{
		GetGameObject()->DisableUpdateSlot(this, 0);
	}
}


void CDeflectorShield::ShootDeflectedEnergy(const CDeflectorShield::SDeflectedEnergy& energy)
{
	if (!m_pAmmoClass)
		return;
	CProjectile* pEnergyBlast = g_pGame->GetWeaponSystem()->SpawnAmmo(m_pAmmoClass, false);
	if (!pEnergyBlast)
		return;

	const Matrix34& worldTransform = GetEntity()->GetWorldTM();

	const float positionBias = 0.05f;

	const Vec3 worldReflectPos = worldTransform.TransformPoint(energy.m_localPosition);
	const Vec3 worldReflectDir = worldTransform.TransformVector(energy.m_localDirection);

	const Vec3 worldSpreadU = worldReflectDir.GetOrthogonal();
	const Vec3 worldSpreadV = worldReflectDir.Cross(worldSpreadU);

	const float spreadOffset = cry_random(0.0f, m_spread);
	
	const Vec3 position = worldReflectPos + worldReflectDir * positionBias;
	Vec3 direction = 
		worldReflectDir + 
		(worldSpreadU * cry_random(0.0f, m_spread)) + 
		(worldSpreadV * cry_random(0.0f, m_spread));
	direction.Normalize();

	CProjectile::SProjectileDesc projectileDesc(
		0, 0, 0, energy.m_damage, m_dropMinDistance, m_dropPerMeter, float(m_minDamage), m_hitTypeId,
		0, false);
	pEnergyBlast->SetParams(projectileDesc);
	pEnergyBlast->Launch(position, direction, Vec3(ZERO));
}


// ===========================================================================
//	Purge the deflected energies buffer.
//
void CDeflectorShield::PurgeDeflectedEnergiesBuffer()
{
	m_deflectedEnergies.resize(0);
	ValidateUpdateSlot();
}


// ===========================================================================
//	Query if a projectile can pass through the shield.
//
//	In:		The projectile (NULL will abort!)
//
//	Returns:	True if the projectile can pass through the shield unhindered;
//				false it should be deflected.
//
bool CDeflectorShield::CanProjectilePassThroughShield(const CProjectile* pProjectile) const
{
	if (m_NonDeflectedOwnersGroupID == 0)
	{
		return false;
	}

	if (pProjectile == NULL)
	{
		return false;
	}

	// If the 'owner' of the projectile is not the same group as the owner of the shield
	// then the projectile is not allowed to pass through the shield.
	const CWeapon* pWeapon = pProjectile->GetWeapon();
	if (pWeapon == NULL)
	{
		return false;
	}
	const IEntity* pOwnerEntity = pWeapon->GetOwner();
	if (pOwnerEntity == NULL)
	{
		return false;
	}	
	const IAIObject* pOwnerAIObject = gEnv->pAISystem->GetAIObjectManager()->GetAIObject(
		pOwnerEntity->GetAIObjectID());
	if (pOwnerAIObject == NULL)
	{
		return false;
	}
	if (pOwnerAIObject->GetGroupId() != m_NonDeflectedOwnersGroupID)
	{
		return false;
	}

	return true;
}


// ============================================================================
//	Query if the shield entity is dead.
//
//	Returns:	True if dead; otherwise false.
//
bool CDeflectorShield::IsDead() const
{
	// In the future we could make this a call-back or something?
	assert(GetEntity() != NULL);
	IScriptTable* scriptTable = GetEntity()->GetScriptTable();
	if (scriptTable == NULL)
	{
		assert(false);
		return true;
	}

	HSCRIPTFUNCTION scriptFunc(NULL);	
	if (!(scriptTable->GetValue("IsDead", scriptFunc)))
	{
		assert(false);
		return true;
	}

	bool deadFlag = false;
	Script::CallReturn(gEnv->pScriptSystem, scriptFunc, scriptTable, deadFlag);
	gEnv->pScriptSystem->ReleaseFunc(scriptFunc);
	return deadFlag;
}


void CDeflectorShield::DebugDraw()
{/*
	TDeflectedEnergies::iterator it;

	for (it = m_deflectedEnergies.begin(); it != m_deflectedEnergies.end(); ++it)
	{
		float x = it->m_delay;
		float radius = (-x*x*4.0f + x*4.0f) * m_energyRadius;
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(it->m_localPosition, radius, ColorB(0, 255, 0));
	}*/
}
