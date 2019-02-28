// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "GameCVars.h"
#include "Projectile.h"
#include "Bullet.h"
#include "WeaponSystem.h"
#include <CryNetwork/ISerialize.h>
#include "IGameObject.h"
#include "Actor.h"
#include "Player.h"

#include <CryEntitySystem/IEntitySystem.h>
#include <IItemSystem.h>
#include <CryAISystem/IAgent.h>
#include <IVehicleSystem.h>
#include "GameRules.h"
#include "ScreenEffects.h"
#include "Utility/CryWatch.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "Single.h"
#include "ItemResourceCache.h"
#include "ActorManager.h"
#include "PersistantStats.h"
#include "RecordingSystem.h"

#include "AI/HazardModule/HazardModule.h"
#include "AI/GameAIEnv.h"
#include <IPerceptionManager.h>
#include <CryAISystem/IAIObjectManager.h>

#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "Weapon.h"
#include "VTOLVehicleManager/VTOLVehicleManager.h"

#include "Environment/WaterPuddle.h"

namespace Proj
{
void RegisterEvents(IGameObjectExtension& goExt, IGameObject& gameObject)
{
	gameObject.RegisterExtForEvents(&goExt, nullptr, 0);
}
}

using namespace HazardSystem;

CRY_IMPLEMENT_GTI_BASE(CProjectile);

CProjectile::SMaterialLookUp CProjectile::s_materialLookup;

void CProjectile::SMaterialLookUp::Init()
{
	if (m_initialized)
		return;

	m_initialized = true;

	const char* materialNames[eType_Count] =
	{
		"mat_water",
	};

	ISurfaceTypeManager* pSurfaceManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();

	CRY_ASSERT(pSurfaceManager != nullptr);

	for (int i = 0; i < eType_Count; ++i)
	{
		ISurfaceType* pSurfaceType = pSurfaceManager->GetSurfaceTypeByName(materialNames[i]);
		if (pSurfaceType != nullptr)
		{
			m_lookUp[i] = pSurfaceType->GetId();
		}
		else
		{
			m_lookUp[i] = -2;
			GameWarning("'%s' surface type not found!!", materialNames[i]);
		}
	}

}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------
CProjectile::SProjectileDesc::SProjectileDesc(EntityId _ownerId, EntityId _hostId, EntityId _weaponId, int _damage, float _damageFallOffStart, float _damageFallOffAmount, float _damageFallOffMin, int _hitTypeId, int8 _bulletPierceabilityModifier, bool _aimedShot)
	: ownerId(_ownerId)
	, hostId(_hostId)
	, weaponId(_weaponId)
	, damage(_damage)
	, damageFallOffStart(_damageFallOffStart)
	, damageFallOffAmount(_damageFallOffAmount)
	, damageFallOffMin(_damageFallOffMin)
	, pointBlankAmount(1.0f)
	, pointBlankDistance(0.0f)
	, pointBlankFalloffDistance(0.0f)
	, hitTypeId(_hitTypeId)
	, bulletPierceabilityModifier(_bulletPierceabilityModifier)
	, aimedShot(_aimedShot)
{
}

CProjectile::SExplodeDesc::SExplodeDesc(bool _destroy)
	: destroy(_destroy)
	, impact(false)
	, pos(ZERO)
	, normal(Vec3(0, 0, 1))
	, vel(ZERO)
	, targetId(0)
	, overrideEffectClassName(0)
{
}

CProjectile::SElectricHitTarget::SElectricHitTarget(IPhysicalEntity* pProjectilePhysics, EventPhysCollision* pCollision)
{
	int source = 0;
	int target = 1;
	IPhysicalEntity* pTarget = pCollision->pEntity[target];
	if (pTarget == pProjectilePhysics)
	{
		source = 1;
		target = 0;
		pTarget = pCollision->pEntity[target];
	}
	IEntity* pEntity = static_cast<IEntity*>(pTarget->GetForeignData(PHYS_FOREIGN_ID_ENTITY));

	entity = pEntity ? pEntity->GetId() : 0;
	partId = pCollision->partid[target];
	matId = pCollision->idmat[target];

	hitPosition = pCollision->pt;
	hitDirection = Vec3(ZERO);
	if (pCollision->vloc[source].GetLengthSquared() > 1e-6f)
		hitDirection = pCollision->vloc[0].GetNormalized();
	hitNormal = pCollision->n;
}

//------------------------------------------------------------------------
CProjectile::CProjectile()
	: m_whizTriggerID(CryAudio::InvalidControlId),
	m_ricochetTriggerID(CryAudio::InvalidControlId),
	//m_trailSoundId(INVALID_SOUNDID),
	m_trailEffectId(0),
	m_pPhysicalEntity(0),
	m_projectileFlags(ePFlag_none),
	m_pAmmoParams(0),
	m_totalLifetime(0.0f),
	m_scaledEffectval(0.0f),
	m_mpDestructionDelay(0.0f),
	m_obstructObject(0),
	m_hitTypeId(0),
	m_hitPoints(-1),
	m_initial_pos(ZERO),
	m_initial_dir(ZERO),
	m_initial_vel(ZERO),
	m_minDamageForKnockDown(0),
	m_minDamageForKnockDownLeg(0),
	m_bullet_pierceability_modifier(0),
	m_currentPhysProfile(ePT_None),
	m_trailSoundEnable(true),
	m_boundToTracerIdx(-1),
	m_threatTrailTracerIdx(-1),
	m_bShouldHaveExploded(false),
	m_last(ZERO),
	m_damage(0),
	m_chanceToKnockDownLeg(0),
	m_ownerId(0),
	m_hostId(0),
	m_weaponId(0),
	m_ammoCost(1),
	m_exploded(false)
{
}

//------------------------------------------------------------------------
CProjectile::~CProjectile()
{
	GetGameObject()->ReleaseProfileManager(this);

	Destroy();

	if (CheckAnyProjectileFlags(ePFlag_hitListener))
		if (CGameRules* pGameRules = g_pGame->GetGameRules())
			pGameRules->RemoveHitListener(this);

	if (g_pGame)
		g_pGame->GetWeaponSystem()->RemoveProjectile(this);

	UnregisterHazardArea(); // (Safety code).
}

//------------------------------------------------------------------------
void CProjectile::DestroyObstructObject()
{
	if (m_obstructObject)
	{
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_obstructObject);
		m_obstructObject = nullptr;
	}
}

//------------------------------------------------------------------------
bool CProjectile::SetAspectProfile(EEntityAspects aspect, uint8 profile)
{
	//if (m_pAmmoParams->physicalizationType == ePT_None)
	//return true;

	if (aspect == eEA_Physics)
	{
		if (m_currentPhysProfile == profile && !gEnv->pSystem->IsSerializingFile())
		{
			CryLog("CProjectile::SetAspectProfile trying to set physics aspect profile to %d, but we've already done so", profile);
			return true;
		}

		Vec3 spin(m_pAmmoParams->spin);
		Vec3 spinRandom = cry_random_componentwise(-m_pAmmoParams->spinRandom, m_pAmmoParams->spinRandom);
		spin += spinRandom;
		spin = DEG2RAD(spin);

		switch (profile)
		{
		case ePT_Particle:
			{
				if (m_pAmmoParams->pParticleParams)
				{
					if (m_pAmmoParams->surfaceTypeId != -1)
						m_pAmmoParams->pParticleParams->surface_idx = m_pAmmoParams->surfaceTypeId;

					m_pAmmoParams->pParticleParams->wspin = spin;
					if (!m_initial_dir.IsZero() && !gEnv->bServer)
						m_pAmmoParams->pParticleParams->heading = m_initial_dir;
				}

				SEntityPhysicalizeParams params;
				params.type = PE_PARTICLE;
				params.mass = m_pAmmoParams->mass;
				if (m_pAmmoParams->pParticleParams)
					params.pParticle = m_pAmmoParams->pParticleParams;

				GetEntity()->Physicalize(params);

				IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
				if (pPhysics)
				{
					pe_params_part ignoreCollisions;
					ignoreCollisions.flagsAND = ~(geom_colltype_explosion | geom_colltype_ray);
					ignoreCollisions.flagsColliderOR = geom_no_particle_impulse;
					pPhysics->SetParams(&ignoreCollisions);
				}
			}
			break;
		case ePT_Rigid:
			{
				SEntityPhysicalizeParams params;
				params.type = PE_RIGID;
				params.mass = m_pAmmoParams->mass;
				params.nSlot = 0;

				GetEntity()->Physicalize(params);

				pe_action_set_velocity velocity;
				m_pPhysicalEntity = GetEntity()->GetPhysics();
				velocity.w = spin;
				m_pPhysicalEntity->Action(&velocity);

				if (m_pAmmoParams->surfaceTypeId != -1)
				{
					pe_params_part part;
					part.ipart = 0;

					GetEntity()->GetPhysics()->GetParams(&part);
					for (int i = 0; i < part.nMats; i++)
						part.pMatMapping[i] = m_pAmmoParams->surfaceTypeId;
				}
			}
			break;

		case ePT_Static:
			{
				SEntityPhysicalizeParams params;
				params.type = PE_STATIC;
				params.nSlot = 0;

				GetEntity()->Physicalize(params);

				if (m_pAmmoParams->surfaceTypeId != -1)
				{
					pe_params_part part;
					part.ipart = 0;

					IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();
					CRY_ASSERT(pPhysicalEntity);

					if (pPhysicalEntity && pPhysicalEntity->GetParams(&part))
						if (!is_unused(part.pMatMapping))
							for (int i = 0; i < part.nMats; i++)
								part.pMatMapping[i] = m_pAmmoParams->surfaceTypeId;
				}
			}
			break;
		case ePT_None:
			{
				SEntityPhysicalizeParams params;
				params.type = PE_NONE;
				params.nSlot = 0;

				GetEntity()->Physicalize(params);
			}
			break;
		}

		m_pPhysicalEntity = GetEntity()->GetPhysics();

		if (m_pPhysicalEntity)
		{
			pe_simulation_params simulation;
			simulation.maxLoggedCollisions = m_pAmmoParams->maxLoggedCollisions;

			pe_params_flags flags;
			flags.flagsOR = pef_log_collisions | (m_pAmmoParams->traceable ? pef_traceable : 0);
			flags.flagsAND = m_pAmmoParams->traceable ? ~(0) : ~(pef_traceable);

			pe_params_part colltype;
			colltype.flagsAND = ~geom_colltype_explosion;

			m_pPhysicalEntity->SetParams(&simulation);
			m_pPhysicalEntity->SetParams(&flags);
			m_pPhysicalEntity->SetParams(&colltype);
		}

		m_currentPhysProfile = profile;
	}

	return true;
}

//------------------------------------------------------------------------
bool CProjectile::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags)
{
	if (aspect == eEA_Physics && !CheckAnyProjectileFlags(ePFlag_dontNetSerialisePhysics))
	{
		NET_PROFILE_SCOPE("ProjectilePhysics", ser.IsReading());

		pe_type type = PE_NONE;
		switch (profile)
		{
		case ePT_Rigid:
			type = PE_RIGID;
			break;
		case ePT_Particle:
			type = PE_PARTICLE;
			break;
		case ePT_None:
			return true;
		case ePT_Static:
			{
				Vec3 pos = GetEntity()->GetWorldPos();
				Quat ori = GetEntity()->GetWorldRotation();
				ser.Value("pos", pos, 'wrld');
				ser.Value("ori", ori, 'ori1');
				if (ser.IsReading())
					GetEntity()->SetWorldTM(Matrix34::Create(Vec3(1, 1, 1), ori, pos));
			}
			return true;
		default:
			return false;
		}

		if (ser.IsWriting())
		{
			if (!GetEntity()->GetPhysicalEntity() || GetEntity()->GetPhysicalEntity()->GetType() != type)
			{
				gEnv->pPhysicalWorld->SerializeGarbageTypedSnapshot(ser, type, 0);
				return true;
			}
		}

		GetEntity()->PhysicsNetSerializeTyped(ser, type, pflags);
	}

	if (aspect == ASPECT_DETONATION)
	{
		CProjectile* pProjectile = static_cast<CProjectile*>(this);
		ser.Value("delayedDet", pProjectile, &CProjectile::HasDetonationBeenDelayed, &CProjectile::SetDetonationHasBeenDelayed, 'bool');
		ser.Value("failedDet", pProjectile, &CProjectile::HasFailedDetonation, &CProjectile::SetFailedDetonation, 'bool');
	}

	return true;
}

NetworkAspectType CProjectile::GetNetSerializeAspects()
{
	return eEA_Physics | ASPECT_DETONATION;
}

//------------------------------------------------------------------------
bool CProjectile::HasDetonationBeenDelayed() const
{
	return CheckAnyProjectileFlags(ePFlag_delayedDetonation);
}

//------------------------------------------------------------------------
void CProjectile::SetDetonationHasBeenDelayed(bool delayed)
{
	if (delayed && !CheckAnyProjectileFlags(ePFlag_delayedDetonation))
	{
		SetProjectileFlags(ePFlag_delayedDetonation);
		if (m_ownerId == g_pGame->GetIGameFramework()->GetClientActorId())
		{
			TAudioSignalID signalId = g_pGame->GetGameAudio()->GetSignalID("DelayedDetonation_ReceivedDelay", true);
			CAudioSignalPlayer::JustPlay(signalId);
		}
	}
}

//------------------------------------------------------------------------
bool CProjectile::CheckForDelayedDetonation(Vec3 pos)
{
	return false;
}

//------------------------------------------------------------------------
bool CProjectile::Init(IGameObject* pGameObject)
{
	SetGameObject(pGameObject);

	s_materialLookup.Init();

	IEntity* pEntity = GetEntity();

	const uint32 flags = pEntity->GetFlags();

	m_projectileEffects.InitWithEntity(pEntity);

	g_pGame->GetWeaponSystem()->AddProjectile(pEntity, this);

	if (!GetGameObject()->CaptureProfileManager(this))
		return false;

	m_pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(pEntity->GetClass());

	CRY_ASSERT(m_pAmmoParams);

	if (0 == (flags & (ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SERVER_ONLY)))
		if (!m_pAmmoParams->predictSpawn)
			if (!GetGameObject()->BindToNetwork())
				return false;

	LoadGeometry();
	Physicalize();

	IEntityRender* pProxy = pEntity->GetRenderInterface();
	if (pProxy && pProxy->GetRenderNode())
	{
		pProxy->GetRenderNode()->SetViewDistRatio(255);
		pProxy->GetRenderNode()->SetLodRatio(255);
	}

	float lifetime = m_pAmmoParams->lifetime;
	if (lifetime > 0.0f)
		pEntity->SetTimer(ePTIMER_LIFETIME, (int)(lifetime * 1000.0f));

	float showtime = m_pAmmoParams->showtime;
	if (showtime > 0.0f)
	{
		pEntity->SetSlotFlags(0, pEntity->GetSlotFlags(0) & (~ENTITY_SLOT_RENDER));
		pEntity->SetTimer(ePTIMER_SHOWTIME, (int)(showtime * 1000.0f));
	}
	else
		pEntity->SetSlotFlags(0, pEntity->GetSlotFlags(0) | ENTITY_SLOT_RENDER);

	m_spawnTime = gEnv->pTimer->GetFrameStartTime();

	pEntity->SetFlags(flags | ENTITY_FLAG_NO_SAVE);

	if (m_pAmmoParams->pRicochet)
	{
		const string& ricochetTriggerName = m_pAmmoParams->pRicochet->audioTriggerName;
		if (!ricochetTriggerName.empty())
		{
			m_ricochetTriggerID = CryAudio::StringToId(ricochetTriggerName.c_str());
		}
	}

	if (m_pAmmoParams->pWhiz)
	{
		const string& whizTriggerName = m_pAmmoParams->pWhiz->audioTriggerName;
		if (!whizTriggerName.empty())
		{
			m_whizTriggerID = CryAudio::StringToId(whizTriggerName.c_str());
		}
	}

	return true;
}

//---------------------------------------------------------------------
////If the projectile is in a pool, this function will be called when this projectile is about to be "re-spawn"
void CProjectile::ReInitFromPool()
{
	CRY_ASSERT(m_pAmmoParams);

	float lifetime = m_pAmmoParams->lifetime;
	if (lifetime > 0.0f)
		GetEntity()->SetTimer(ePTIMER_LIFETIME, (int)(lifetime * 1000.0f));

	float showtime = m_pAmmoParams->showtime;
	if (showtime > 0.0f)
	{
		GetEntity()->SetSlotFlags(0, GetEntity()->GetSlotFlags(0) & (~ENTITY_SLOT_RENDER));
		GetEntity()->SetTimer(ePTIMER_SHOWTIME, (int)(showtime * 1000.0f));
	}
	else
		GetEntity()->SetSlotFlags(0, GetEntity()->GetSlotFlags(0) | ENTITY_SLOT_RENDER);

	//Reset some members
	m_projectileFlags = ePFlag_none;
	m_totalLifetime = 0.0f;
	m_scaledEffectval = 0.0f;
	m_mpDestructionDelay = 0.0f;
	m_obstructObject = 0;
	m_hitPoints = -1;
	m_boundToTracerIdx = -1;
	m_threatTrailTracerIdx = -1;
	m_spawnTime = gEnv->pTimer->GetFrameStartTime();
	m_bShouldHaveExploded = false;
	m_exploded = false;
	m_trailEffectId = 0;

	m_projectileEffects.FreeAllEffects();

	if (m_pPhysicalEntity)
	{
		pe_params_flags flags;
		flags.flagsOR = pef_log_collisions | (m_pAmmoParams->traceable ? pef_traceable : 0);
		flags.flagsAND = m_pAmmoParams->traceable ? ~(0) : ~(pef_traceable);
		m_pPhysicalEntity->SetParams(&flags);

		if (m_pAmmoParams->physicalizationType == ePT_Particle)
		{
			pe_params_particle particleParams;
			particleParams.dontPlayHitEffect = 0;
			particleParams.wspin = m_pAmmoParams->pParticleParams->wspin;
			m_pPhysicalEntity->SetParams(&particleParams);
		}
	}
}

//-----------------------------------------------------------
void CProjectile::SetLifeTime(float lifeTime)
{
	if (lifeTime > 0.0f)
		GetEntity()->SetTimer(ePTIMER_LIFETIME, (int)(lifeTime * 1000.0f));
}

//------------------------------------------------------------------------
void CProjectile::PostInit(IGameObject* pGameObject)
{
	Proj::RegisterEvents(*this, *pGameObject);

	GetGameObject()->EnableUpdateSlot(this, 0);
}

//------------------------------------------------------------------------
bool CProjectile::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	ResetGameObject();
	Proj::RegisterEvents(*this, *pGameObject);
	CRY_ASSERT_MESSAGE(false, "CProjectile::ReloadExtension not implemented");

	return false;
}

//------------------------------------------------------------------------
bool CProjectile::GetEntityPoolSignature(TSerialize signature)
{
	CRY_ASSERT_MESSAGE(false, "CProjectile::GetEntityPoolSignature not implemented");

	return true;
}

//------------------------------------------------------------------------
void CProjectile::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CProjectile::FullSerialize(TSerialize ser)
{
	CRY_ASSERT(ser.GetSerializationTarget() != eST_Network);

	bool remote = CheckAnyProjectileFlags(ePFlag_remote);
	bool destroying = CheckAnyProjectileFlags(ePFlag_destroying);
	bool hitListener = CheckAnyProjectileFlags(ePFlag_hitListener);

	ser.Value("Remote", remote);
	// m_tracerpath should be serialized but the template-template stuff doesn't work under VS2005
	ser.Value("Owner", m_ownerId, 'eid');
	ser.Value("Weapon", m_weaponId, 'eid');
	ser.Value("TrailEffect", m_trailEffectId);
	//ser.Value("TrailSound", m_trailSoundId);
	ser.Value("WhizSound", m_whizTriggerID);
	ser.Value("RicochetTrigger", m_ricochetTriggerID);
	ser.Value("Damage", m_damage);
	ser.Value("Destroying", destroying);
	ser.Value("LastPos", m_last);
	ser.Value("InitialPos", m_initial_pos);
	ser.Value("HitListener", hitListener);
	ser.Value("HitPoints", m_hitPoints);

	SetProjectileFlags(ePFlag_destroying, destroying);
	SetProjectileFlags(ePFlag_hitListener, hitListener);
	SetProjectileFlags(ePFlag_remote, remote);

	bool wasVisible = false;
	if (ser.IsWriting())
		wasVisible = (GetEntity()->GetSlotFlags(0) & (ENTITY_SLOT_RENDER)) ? true : false;
	ser.Value("Visibility", wasVisible);
	if (ser.IsReading())
	{
		if (wasVisible)
			GetEntity()->SetSlotFlags(0, GetEntity()->GetSlotFlags(0) | ENTITY_SLOT_RENDER);
		else
			GetEntity()->SetSlotFlags(0, GetEntity()->GetSlotFlags(0) & (~ENTITY_SLOT_RENDER));

		InitWithAI();
	}
}

//------------------------------------------------------------------------
void CProjectile::Update(SEntityUpdateContext& ctx, int updateSlot)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	CRY_ASSERT_MESSAGE(!RequiresDelayedDestruct() || gEnv->bMultiplayer, "The mpProjectileDestructDelay ammo params should only ever be greater than zero in Multiplayer");

	// we need to destroy this projectile
	if (CheckAnyProjectileFlags(ePFlag_needDestruction) && !GetEntity()->IsKeptAlive())
	{
		bool doDestroy = true;

		if (gEnv->bMultiplayer)
		{
			if (RequiresDelayedDestruct())
			{
				if (m_mpDestructionDelay > ctx.fFrameTime)
				{
					m_mpDestructionDelay -= ctx.fFrameTime;
					doDestroy = false;
				}
				else
				{
					m_mpDestructionDelay = 0.f;
				}
			}
		}

		if (doDestroy)
		{
			DestroyImmediate();
			return;
		}
	}

	if (m_HazardID.IsDefined())
	{
		// Estimate the direction in which the projectile was traveling
		// (note: upon impact the orientation of the projectile might
		// alter immediately due to deflection, this will cause unexpected behavior
		// when we generate hazard areas at the moment of impact if the projectile
		// exploded). Also, some projectiles tumble in mid-air, so the forward
		// direction of the entity is unsuited for our needs.
		const Vec3 estimatedTravelDirection = (GetEntity()->GetWorldPos() - m_last).GetNormalizedSafe(ZERO);
		if (!estimatedTravelDirection.IsZero())
		{
			if (!CheckAnyProjectileFlags(ePFlag_destroying | ePFlag_needDestruction))
			{
				SyncHazardArea(estimatedTravelDirection);
			}
		}
	}

	if (!CheckAnyProjectileFlags(ePFlag_failedDetonation))
	{
#ifndef _RELEASE
		if (g_pGameCVars->i_debug_projectiles > 0)
		{
			CryWatch("SLOT #%d: Projectile: '%s' %p (physics type %d, bullet type %d)", updateSlot, GetEntity()->GetClass()->GetName(), this, m_pAmmoParams->physicalizationType, m_pAmmoParams->bulletType);
		}
#endif

		if (updateSlot != 0)
			return;

		Vec3 pos = GetEntity()->GetWorldPos();

		UpdateWhiz(pos, false);

		/*if (m_trailSoundEnable && m_trailSoundId==INVALID_SOUNDID)
		   TrailSound(true);*/

		m_totalLifetime += ctx.fFrameTime;
		m_last = pos;
	}
}

//------------------------------------------------------------------------
bool CProjectile::IsGrenade() const
{
	return m_pAmmoParams->bulletType == -1;
}

//------------------------------------------------------------------------
void CProjectile::HandleEvent(const SGameObjectEvent& event)
{
	if (CheckAnyProjectileFlags(ePFlag_destroying | ePFlag_needDestruction))
	{
		return;
	}

	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if (event.event == eGFE_OnPostStep && (event.flags & eGOEF_LoggedPhysicsEvent) == 0)
	{
		//==============================================================================
		// Warning this is called directly from the physics thread! must be thread safe!
		//==============================================================================
		EventPhysPostStep* pEvent = (EventPhysPostStep*)event.ptr;
		g_pGame->GetWeaponSystem()->OnProjectilePhysicsPostStep(this, pEvent, 1);
	}

	if (event.event == eGFE_OnCollision)
	{
		EventPhysCollision* pCollision = (EventPhysCollision*)event.ptr;
		m_last = pCollision->pt;

		IEntity* pTarget = pCollision->iForeignData[1] == PHYS_FOREIGN_ID_ENTITY ? (IEntity*)pCollision->pForeignData[1] : 0;
		if (ProcessCollisionEvent(pTarget))
		{
			DoCollisionDamage(pCollision, pTarget);

			const SCollisionEffectParams* pCollisionFxParams = m_pAmmoParams->pCollision ? m_pAmmoParams->pCollision->pEffectParams : nullptr;
			if (pCollisionFxParams)
			{
				if (!pCollisionFxParams->effect.empty())
				{
					CItemParticleEffectCache& particleCache = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().GetParticleEffectCache();
					IParticleEffect* pEffect = particleCache.GetCachedParticle(pCollisionFxParams->effect);

					if (pEffect)
					{
						pEffect->Spawn(true, IParticleEffect::ParticleLoc(pCollision->pt, pCollision->n, pCollisionFxParams->scale));
					}
				}

				REINST("needs verification!");
				/*if (!pCollisionFxParams->sound.empty())
				   {
				   _smart_ptr<ISound> const pSound = gEnv->pSoundSystem->CreateSound(pCollisionFxParams->sound.c_str(), FLAG_SOUND_DEFAULT_3D);
				   pSound->SetSemantic(eSoundSemantic_Projectile);
				   pSound->SetPosition(pCollision->pt);
				   pSound->Play();
				   }*/
			}

			Ricochet(pCollision);
		}

		if (GetAmmoParams().pElectricProjectileParams)
		{
			SElectricHitTarget target(GetEntity()->GetPhysics(), pCollision);
			ProcessElectricHit(target);
		}

#ifdef SHOT_DEBUG
		if (CWeapon* pWeapon = this->GetWeapon())
		{
			if (CShotDebug* pShotDebug = pWeapon->GetShotDebug())
			{
				pShotDebug->OnProjectileHit(*this, *pCollision);
			}
		}
#endif //SHOT_DEBUG

		SetProjectileFlags(ePFlag_collided);
	}
}

//------------------------------------------------------------------------
bool CProjectile::ProcessCollisionEvent(IEntity* pTarget) const
{
	bool bResult = true;

	const bool bIsClient = (m_ownerId == g_pGame->GetIGameFramework()->GetClientActorId());
	const bool bEnableFriendlyHit = (bIsClient ? g_pGameCVars->g_enableFriendlyPlayerHits : g_pGameCVars->g_enableFriendlyAIHits) != 0;

	if ((!bEnableFriendlyHit) && (!gEnv->bMultiplayer))
	{
		IEntity* pOwner = gEnv->pEntitySystem->GetEntity(m_ownerId);
		IAIObject* pOwnerAI = pOwner ? pOwner->GetAI() : nullptr;
		IAIObject* pTargetAI = pTarget ? pTarget->GetAI() : nullptr;

		if (pOwnerAI && pTargetAI && !pOwnerAI->IsHostile(pTargetAI))
			bResult = false;
	}

	//In MP we don't want entities to be able to shoot their parent entities
	if (gEnv->bMultiplayer && pTarget)
	{
		IEntity* pOwner = gEnv->pEntitySystem->GetEntity(m_ownerId);
		IEntity* pParent = pOwner ? pOwner->GetParent() : nullptr;

		if (pOwner && pParent == pTarget)
		{
			bResult = false;
		}
	}

	return bResult;
}

//------------------------------------------------------------------------
void CProjectile::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_TIMER:
		{
			switch (event.nParam[0])
			{
			case ePTIMER_SHOWTIME:
				{
					IEntity* pEntity = GetEntity();
					const uint32 uNewFlags = pEntity->GetSlotFlags(0) | ENTITY_SLOT_RENDER;
					pEntity->SetSlotFlags(0, uNewFlags);
					CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
					if (pRecordingSystem)
					{
						pRecordingSystem->OnEntityDrawSlot(pEntity, 0, uNewFlags);
					}
					break;
				}
			case ePTIMER_LIFETIME:
				if (m_pAmmoParams->quietRemoval || HasFailedDetonation())  // claymores don't explode when they timeout
				{
					Destroy();
				}
				else
				{
					CProjectile::SExplodeDesc explodeDesc(true);
					Explode(explodeDesc);
				}
				break;
			}
		}
		break;
	}
}

uint64 CProjectile::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_TIMER);
}

//------------------------------------------------------------------------
void CProjectile::LoadGeometry()
{
	if (m_pAmmoParams && !m_pAmmoParams->fpGeometryName.empty())
	{
		GetEntity()->LoadGeometry(0, m_pAmmoParams->fpGeometryName.c_str());
		GetEntity()->SetSlotLocalTM(0, m_pAmmoParams->fpLocalTM);
	}
}

//------------------------------------------------------------------------
void CProjectile::Physicalize()
{
	if (!m_pAmmoParams || m_pAmmoParams->physicalizationType == ePT_None)
		return;

	GetGameObject()->SetAspectProfile(eEA_Physics, m_pAmmoParams->physicalizationType);
}

//------------------------------------------------------------------------
void CProjectile::SetVelocity(const Vec3& pos, const Vec3& dir, const Vec3& velocity, float speedScale, Vec3* appliedVelocityOut, int bThreadSafe)
{
	if (!m_pPhysicalEntity)
		return;

	Vec3 totalVelocity = (dir * m_pAmmoParams->speed * speedScale) + velocity;

	if (appliedVelocityOut != nullptr)
		*appliedVelocityOut = totalVelocity;

	if (m_pPhysicalEntity->GetType() == PE_PARTICLE)
	{
		pe_params_particle particle;
		particle.heading = totalVelocity.GetNormalized();
		particle.velocity = totalVelocity.GetLength();

		m_pPhysicalEntity->SetParams(&particle, bThreadSafe);
	}
	else if (m_pPhysicalEntity->GetType() == PE_RIGID)
	{
		pe_action_set_velocity vel;
		vel.v = totalVelocity;

		m_pPhysicalEntity->Action(&vel, bThreadSafe);
	}
}

//------------------------------------------------------------------------
void CProjectile::SetParams(const SProjectileDesc& projectileDesc)
{
	m_ownerId = projectileDesc.ownerId;
	m_weaponId = projectileDesc.weaponId;
	m_hostId = projectileDesc.hostId;
	m_damage = projectileDesc.damage;
	m_hitTypeId = projectileDesc.hitTypeId;
	m_bullet_pierceability_modifier = projectileDesc.bulletPierceabilityModifier;

	IEntity* pOwnerEntity = gEnv->pEntitySystem->GetEntity(m_hostId ? m_hostId : m_ownerId);

	ClearProjectileFlags(ePFlag_ownerIsPlayer);
	if (m_ownerId)
	{
		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId);
		if (pActor && pActor->IsPlayer())
		{
			SetProjectileFlags(ePFlag_ownerIsPlayer);
		}
	}

	if (projectileDesc.aimedShot)
		SetProjectileFlags(ePFlag_aimedShot);
	else
		ClearProjectileFlags(ePFlag_aimedShot);

	bool isParticle = (m_pPhysicalEntity != nullptr) && (m_pPhysicalEntity->GetType() == PE_PARTICLE);
	if (isParticle)
	{
		SetUpParticleParams(pOwnerEntity, projectileDesc.bulletPierceabilityModifier);

		pe_params_flags pf;
		pf.flagsOR = particle_no_impulse;
		if (m_pPhysicalEntity)
			m_pPhysicalEntity->SetParams(&pf);
	}
}

void CProjectile::SetKnocksTargetInfo(const SFireModeParams* pParams)
{
	const SFireParams& fireParams = pParams->fireparams;
	SetProjectileFlags(ePFlag_knocksTarget, fireParams.knocks_target);
	m_minDamageForKnockDown = fireParams.min_damage_for_knockDown;
	m_minDamageForKnockDownLeg = fireParams.min_damage_for_knockDown_leg;
	m_chanceToKnockDownLeg = fireParams.knockdown_chance_leg;
}

//------------------------------------------------------------------------
void CProjectile::Launch(const Vec3& pos, const Vec3& dir, const Vec3& velocity, float speedScale /*=1.0f*/)
{
	SetProjectileFlags(ePFlag_launched);
	ClearProjectileFlags(ePFlag_destroying | ePFlag_hitListener | ePFlag_needDestruction | ePFlag_hitListener_mp_OnExplosion_only);

	GetGameObject()->EnablePhysicsEvent(true, eEPE_OnCollisionLogged);

	// Enable Post Physics Callbacks
	if (IPhysicalEntity* pent = GetEntity()->GetPhysics())
	{
		pe_params_flags flags;
		flags.flagsOR = pef_monitor_poststep;
		pent->SetParams(&flags);
		GetGameObject()->EnablePhysicsEvent(true, eEPE_OnPostStepImmediate);
	}

	// Only for bullets
	m_hitPoints = m_pAmmoParams->hitPoints;

	if (m_hitPoints > 0)
	{
		//Only projectiles with hit points are hit listeners
		g_pGame->GetGameRules()->AddHitListener(this);
		SetProjectileFlags(ePFlag_hitListener);
		SetProjectileFlags(ePFlag_noBulletHits, m_pAmmoParams->noBulletHits);
	}
	else
	{
		if (gEnv->bMultiplayer)
		{
			if (RequiresDelayedDestruct())
			{
				g_pGame->GetGameRules()->AddHitListener(this);
				SetProjectileFlags(ePFlag_hitListener | ePFlag_hitListener_mp_OnExplosion_only);
			}
		}
	}

	Matrix34 worldTM = Matrix34(Matrix33::CreateRotationVDir(dir.GetNormalizedSafe()));
	worldTM.SetTranslation(pos);
	GetEntity()->SetWorldTM(worldTM);

	//Must set velocity after position, if not velocity could be reseted for PE_RIGID
	Vec3 appliedVelocity;
	SetVelocity(pos, dir, velocity, speedScale, &appliedVelocity);

	m_initial_pos = pos;
	m_initial_dir = dir;
	m_initial_vel = velocity;

	m_last = pos;

	// Attach effect when fired (not first update)
	if (m_trailEffectId == 0)
	{
		TrailEffect(true);
	}

	InitWithAI();

	IEntity* pOwnerEntity = gEnv->pEntitySystem->GetEntity(m_hostId ? m_hostId : m_ownerId);
	if (pOwnerEntity)
	{
		IEntity* pSelfEntity = GetEntity();

		if (pSelfEntity)
		{
			//need to set AI species to the shooter - not to be scared of it's own rockets
			IAIObject* projectileAI = pSelfEntity->GetAI();
			IAIObject* shooterAI = pOwnerEntity->GetAI();

			if (projectileAI && shooterAI)
				projectileAI->SetFactionID(shooterAI->GetFactionID());
		}
	}

	if (IsGrenade())
	{
		CCCPOINT(Projectile_GrenadeLaunched);
	}
	else
	{
		CCCPOINT(Projectile_BulletLaunched);
	}

	g_pGame->GetWeaponSystem()->OnLaunch(this, pos, appliedVelocity);

	if (ShouldGenerateHazardArea())
	{
		RegisterHazardArea();
	}
}

//------------------------------------------------------------------------
void CProjectile::OnLaunch() const
{
	if (gEnv->IsClient() && m_pAmmoParams->trackOnHud)
	{
		const CActor* pClientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
		if (pClientActor)
		{
			const EntityId clientActorId = pClientActor->GetEntityId();
			const bool isLocalActorsExplosive = (clientActorId == m_ownerId);

			const EntityId projectileEntId = GetEntityId();

			const float range = (m_pAmmoParams->pExplosion) ? m_pAmmoParams->pExplosion->maxRadius : 0.f;
			const int maxDamage = m_damage;
			const bool isFriendlyExplosive = (0 != pClientActor->IsFriendlyEntity(m_ownerId));

			SHUDEvent eventGrenade(eHUDEvent_OnExplosiveSpawned);
			eventGrenade.AddData(SHUDEventData((int)projectileEntId));
			eventGrenade.AddData(SHUDEventData(range));
			eventGrenade.AddData(SHUDEventData(maxDamage));
			eventGrenade.AddData(SHUDEventData(isFriendlyExplosive));
			eventGrenade.AddData(SHUDEventData(isLocalActorsExplosive));
			CHUDEventDispatcher::CallEvent(eventGrenade);
		}
	}
}

//------------------------------------------------------------------------
void CProjectile::Destroy()
{
	TrailSound(false);

#ifndef _RELEASE
	if (m_initial_pos == m_last && !m_last.IsZero() && !m_initial_pos.IsZero())
	{
		IEntity* weaponEntity = gEnv->pEntitySystem->GetEntity(m_weaponId);
		IEntity* shooterEntity = gEnv->pEntitySystem->GetEntity(m_ownerId);
		GameWarning("Projectile %p fired from '%s' owned by %s '%s' hasn't moved since launch (%.2f %.2f %.2f)", this, weaponEntity ? weaponEntity->GetClass()->GetName() : "??", shooterEntity ? shooterEntity->GetClass()->GetName() : "??", shooterEntity ? shooterEntity->GetName() : "??", m_last.x, m_last.y, m_last.z);
	}
#endif

	if (CheckAllProjectileFlags(ePFlag_linked))
	{
		g_pGame->GetWeaponSystem()->RemoveLinkedProjectile(GetEntityId());

		ClearProjectileFlags(ePFlag_linked);
	}

	if (!CheckAnyProjectileFlags(ePFlag_needDestruction))  // ie. first-time through this func, before any possible delay or keep-alive
	{
		if (IsGrenade())
		{
			CCCPOINT(Projectile_GrenadeDestroyed);
		}
		else
		{
			CCCPOINT(Projectile_BulletDestroyed);
		}
	}

	const bool delayedDestruct = (gEnv->bMultiplayer && RequiresDelayedDestruct() && !CheckAnyProjectileFlags(ePFlag_failedDetonation));

	// don't destroy entities which are kept alive (used for deferred collision events) OR which are set to Delayed Destruction in MP (to give the ClKill RMI to arrive with a valid entityId for the projectile so the Kill Cam can do its playback correctly)
	// these are eventually destroyed in the Update function
	if (GetEntity()->IsKeptAlive() || delayedDestruct)
	{
		if (delayedDestruct)
		{
			m_mpDestructionDelay = m_pAmmoParams->mpProjectileDestructDelay;
		}
		SetProjectileFlags(ePFlag_needDestruction);
		gEnv->pAISystem->GetAIObjectManager()->RemoveObjectByEntityId(GetEntityId()); // unregister from AI. Will be removed from active list when hidden otherwise (see EvaluateUpdateActivation)
		GetEntity()->SetFlags(GetEntity()->GetFlags() | ENTITY_FLAG_UPDATE_HIDDEN);   // Bugfix for grenades persisting on client after exploding.
		GetEntity()->Hide(true);
		return;
	}

	// not used for deferred collision events (or MP excpetions), destroy right away
	DestroyImmediate();
}

//------------------------------------------------------------------------
void CProjectile::DestroyImmediate()
{
	if (CheckAnyProjectileFlags(ePFlag_destroying))
		return;

	if (CheckAnyProjectileFlags(ePFlag_failedDetonation))
	{
		IEntity* pEntity = GetEntity();

		if (m_pAmmoParams->pExplosion)
		{
			CItemParticleEffectCache& particleCache = g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().GetParticleEffectCache();
			IParticleEffect* pParticleEffect = particleCache.GetCachedParticle(m_pAmmoParams->pExplosion->failedEffectName);

			if (pParticleEffect)
			{
				pParticleEffect->Spawn(true, IParticleEffect::ParticleLoc(pEntity->GetWorldPos(), m_initial_dir, 1.0f));
			}
		}
	}

	UpdateWhiz(GetEntity()->GetWorldPos(), true); //updated on destroy as typically this is the only time when bullet entities have actually moved

	SetProjectileFlags(ePFlag_destroying);

	GetGameObject()->EnablePhysicsEvent(false, eEPE_OnCollisionLogged | eEPE_OnPostStepImmediate);

	DestroyObstructObject();

	CWeaponSystem* pWeaponSystem = g_pGame->GetWeaponSystem();
	EntityId projectileId = GetEntity()->GetId();

	if (m_boundToTracerIdx != -1)
	{
		pWeaponSystem->GetTracerManager().OnBoundProjectileDestroyed(m_boundToTracerIdx, projectileId, GetEntity()->GetWorldPos());
	}

	if (m_threatTrailTracerIdx != -1)
	{
		pWeaponSystem->GetTracerManager().OnBoundProjectileDestroyed(m_threatTrailTracerIdx, projectileId, GetEntity()->GetWorldPos());
	}

	bool returnToPoolOK = true;
	if (m_pAmmoParams->reusable)
	{
		returnToPoolOK = pWeaponSystem->ReturnToPool(this);
	}

	if (!m_pAmmoParams->reusable || !returnToPoolOK)
	{
		if ((GetEntity()->GetFlags() & ENTITY_FLAG_CLIENT_ONLY) || gEnv->bServer)
		{
			gEnv->pEntitySystem->RemoveEntity(projectileId);
		}
	}
}

//------------------------------------------------------------------------
void CProjectile::CreateBulletTrail(const Vec3& hitPos)
{
	if (IsGrenade() == false)
	{
		CCCPOINT(Projectile_BulletTrail);
	}
}

//------------------------------------------------------------------------
bool CProjectile::IsRemote() const
{
	return CheckAnyProjectileFlags(ePFlag_remote);
}

//------------------------------------------------------------------------
void CProjectile::SetRemote(bool remote)
{
	SetProjectileFlags(ePFlag_remote, remote);
}

//------------------------------------------------------------------------
void CProjectile::SetFiredViaProxy(bool proxy)
{
	SetProjectileFlags(ePFlag_firedViaProxy, proxy);
}

//------------------------------------------------------------------------
void CProjectile::Explode(const SExplodeDesc& explodeDesc)
{
	UnregisterHazardArea();

	if (m_exploded)
		return;
	m_exploded = true;

	if (!m_pAmmoParams->serverSpawn || gEnv->bServer)
	{
		const SExplosionParams* pExplosionParams = m_pAmmoParams->pExplosion;
		if (pExplosionParams)
		{
			Vec3 dir(0, 0, 1);
			if (explodeDesc.impact && explodeDesc.vel.len2() > 0)
				dir = explodeDesc.vel.normalized();
			else if (explodeDesc.normal.len2() > 0)
				dir = -explodeDesc.normal;

			m_hitPoints = 0;

			// marcok: using collision pos sometimes causes explosions to have no effect. Anton advised to use entity pos
			Vec3 epos = explodeDesc.pos.len2() > 0 ? (explodeDesc.pos - dir * 0.2f) : GetEntity()->GetWorldPos();

			CGameRules* pGameRules = g_pGame->GetGameRules();

			if (gEnv->bServer)
			{
				uint16 projectileNetClassId = 0;

				if (g_pGame->GetIGameFramework()->GetNetworkSafeClassId(projectileNetClassId, GetEntity()->GetClass()->GetName()))
				{
					SProjectileExplosionParams pep(
					  m_ownerId,
					  m_weaponId,
					  GetEntityId(),
					  explodeDesc.impact ? explodeDesc.targetId : 0,
					  epos,
					  dir,
					  explodeDesc.impact ? explodeDesc.normal : FORWARD_DIRECTION,
					  explodeDesc.impact ? explodeDesc.vel : ZERO,
					  static_cast<float>(m_damage),
					  projectileNetClassId,
					  explodeDesc.impact,
					  CheckAnyProjectileFlags(ePFlag_firedViaProxy));

					if (explodeDesc.overrideEffectClassName)
						pep.m_overrideEffectClassName = string(explodeDesc.overrideEffectClassName);

					pGameRules->ProjectileExplosion(pep);
				}
#if !defined(_RELEASE)
				else
				{
					char msg[1024] = { 0 };
					cry_sprintf(msg, "missing network safe class id for entity class %s", GetEntity()->GetClass()->GetName());
					CRY_ASSERT_MESSAGE(false, msg);
				}
#endif
			}
		}

		if (!gEnv->bMultiplayer)
		{
			//Single player (AI related code)is processed here, CGameRules::ClientExplosion process the effect
			if (m_pAmmoParams->pFlashbang)
				FlashbangEffect(m_pAmmoParams->pFlashbang);

			// Used for showing hud radar enemy grenade explosion sound waves
			if (IsGrenade() && m_ownerId != g_pGame->GetIGameFramework()->GetClientActorId())
			{
				SHUDEvent eventGrenade(eHUDEvent_OnGrenadeExploded);
				eventGrenade.AddData(SHUDEventData((int)GetEntityId()));
				CHUDEventDispatcher::CallEvent(eventGrenade);
			}
		}

		if (explodeDesc.destroy)
		{
			Destroy();
		}
	}
	else
	{
		// This projectile would have exploded if we were a server, set a flag so that in the event of
		// a host migration, we can clean up any projectiles left behind
		m_bShouldHaveExploded = true;
	}
}

//------------------------------------------------------------------------
void CProjectile::TrailSound(bool enable, const Vec3& dir)
{
	/*if (enable)
	   {
	   if (!m_pAmmoParams->pTrail || m_pAmmoParams->pTrail->sound.empty())
	    return;

	   m_trailSoundId = GetAudioProxy()->PlaySound(m_pAmmoParams->pTrail->sound.c_str(), Vec3(0,0,0), FORWARD_DIRECTION, FLAG_SOUND_DEFAULT_3D, 0, eSoundSemantic_Projectile, 0, 0);
	   if (m_trailSoundId != INVALID_SOUNDID)
	   {
	    ISound *pSound=GetAudioProxy()->GetSound(m_trailSoundId);
	    if (pSound)
	      pSound->GetInterfaceDeprecated()->SetLoopMode(true);
	   }
	   }
	   else if (m_trailSoundId!=INVALID_SOUNDID)
	   {
	   GetAudioProxy()->StopSound(m_trailSoundId);
	   m_trailSoundId=INVALID_SOUNDID;
	   }*/

	m_trailSoundEnable = enable;
}

//------------------------------------------------------------------------
void CProjectile::UpdateWhiz(const Vec3& pos, bool destroy)
{
	if (m_pAmmoParams->pWhiz && m_whizTriggerID != CryAudio::InvalidControlId && !IsEquivalent(m_last, pos))
	{
		IActor* pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
		if (pClientActor && (m_ownerId != pClientActor->GetEntityId()))
		{
			Vec3 soundPos(0.0f, 0.0f, 0.0f);
			Vec3 referencePosition = pClientActor->GetEntity()->GetWorldPos();
			if (IMovementController* pMC = pClientActor->GetMovementController())
			{
				SMovementState movementState;
				pMC->GetMovementState(movementState);
				referencePosition = movementState.eyePosition;
			}

			//distance to the (infinitely long) line
			const float whizDistanceLimitSq = m_pAmmoParams->pWhiz->distanceSq;
			const float distanceSq = Distance::Point_LineSq(referencePosition, m_last, pos, soundPos);
			if (distanceSq < whizDistanceLimitSq)
			{
				//check distance is approximately correct
				const Vec3 soundDir = (soundPos - m_last);
				const Vec3 dir = (pos - m_last);
				const float bulletDist = dir.GetLengthSquared(); //|dir| * |dir|
				const float dot = dir.Dot(soundDir);             //cos(th) * |soundDir| * |dir|
				//cos (th) approaching 1 when we want to play sound
				if (dot > 0.0f && dot < bulletDist)  //therefore if |soundDir| * |dir| < |dir| * |dir| we should play the sound (and greater than zero, otherwise we are behind the firing position)
				{
					AABB playerAABB;
					pClientActor->GetEntity()->GetWorldBounds(playerAABB);
					playerAABB.Expand(Vec3(-0.1f, -0.1f, -0.05f));
					Lineseg lineSegment(m_last, soundDir * whizDistanceLimitSq);

					//If it overlaps it means probably the bullet is going to hit the player, so try to skip the whiz sound in that case
					Vec3 intersectPoint;
					if (!playerAABB.IsContainPoint(soundPos) && !Intersect::Lineseg_AABB(lineSegment, playerAABB, intersectPoint))
					{
						WhizSound(soundPos);
#if !defined(_RELEASE)
						if (g_pGameCVars->i_debug_projectiles > 2)
						{
							IPersistantDebug* pDebug = gEnv->pGameFramework->GetIPersistantDebug();
							pDebug->Begin("CProjectile::UpdateWhizSound", true);
							pDebug->AddCone(soundPos, dir, 0.1f, 1.0f, ColorF(0.0f, 0.0f, 1.0f, 1.0f), 5.0f);
							pDebug->AddLine(m_last, pos, ColorF(0.0f, 0.0f, 1.0f, 1.0f), 5.0f);
						}
#endif
					}
				}
			}
		}
	}

#ifndef _RELEASE
	if (g_pGameCVars->g_ProjectilePathDebugGfx)
	{
		if (IsGrenade())
		{
			IPersistantDebug* dbg = g_pGame->GetIGameFramework()->GetIPersistantDebug();
			dbg->Begin("grenades", false);
			dbg->AddLine(m_last, GetEntity()->GetWorldPos(), ColorF(1.f, 1.f, 0.5f, 1.f), 6);
		}
		else if (destroy)
		{
			IPersistantDebug* dbg = g_pGame->GetIGameFramework()->GetIPersistantDebug();
			dbg->Begin("bullets", false);
			dbg->AddLine(m_initial_pos, GetEntity()->GetWorldPos(), ColorF(1.f, 0.25f, 0.25f, 1.f), 6);
		}
		else
		{
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(m_initial_pos, ColorB(128, 255, 128, 255), GetEntity()->GetWorldPos(), ColorB(128, 255, 128, 255), 3);
		}
	}
#endif
}

//------------------------------------------------------------------------
void CProjectile::WhizSound(const Vec3& pos)
{
	CryAudio::SExecuteTriggerData const data(m_whizTriggerID, "WhizBy", CryAudio::EOcclusionType::Ignore, pos, INVALID_ENTITYID, true);
	gEnv->pAudioSystem->ExecuteTriggerEx(data);
}

//------------------------------------------------------------------------
void CProjectile::RicochetSound(const Vec3& pos)
{
	CryAudio::SExecuteTriggerData const data(m_ricochetTriggerID, "Ricochet", CryAudio::EOcclusionType::Ignore, pos, INVALID_ENTITYID, true);
	gEnv->pAudioSystem->ExecuteTriggerEx(data);
}

//------------------------------------------------------------------------
void CProjectile::TrailEffect(bool enable)
{
	if (enable)
	{
		const STrailParams* pTrail = m_pAmmoParams->pTrail;
		if (!pTrail)
			return;

		CWeapon* pOwnerWeapon = GetWeapon();
		const bool useFPTrail = (pOwnerWeapon && pOwnerWeapon->GetStats().fp && !pTrail->effect_fp.empty());

		const char* effectName = useFPTrail ? pTrail->effect_fp.c_str() : pTrail->effect.c_str();
		EntityEffects::SEffectAttachParams attachParams(ZERO, FORWARD_DIRECTION, pTrail->scale, pTrail->prime, 1);

		m_trailEffectId = m_projectileEffects.AttachParticleEffect(GetCachedEffect(effectName), attachParams);
	}
	else
	{
		m_projectileEffects.DetachEffect(m_trailEffectId);
		m_trailEffectId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
IParticleEffect* CProjectile::GetCachedEffect(const char* effectName) const
{
	return g_pGame->GetGameSharedParametersStorage()->GetItemResourceCache().GetParticleEffectCache().GetCachedParticle(effectName);
}

//------------------------------------------------------------------------
IEntityAudioComponent* CProjectile::GetAudioProxy()
{
	IEntityAudioComponent* pIEntityAudioComponent = GetEntity()->GetOrCreateComponent<IEntityAudioComponent>();

	CRY_ASSERT(pIEntityAudioComponent != nullptr);

	return pIEntityAudioComponent;
}

void CProjectile::FlashbangEffect(const SFlashbangParams* flashbang)
{
	if (!flashbang)
		return;

	IPerceptionManager* pPerceptionManager = IPerceptionManager::GetInstance();
	if (!pPerceptionManager)
		return;

	const float radius = flashbang->maxRadius;

	// Associate event with vehicle if the shooter is in a vehicle (tank cannon shot, etc)
	EntityId ownerId = m_ownerId;
	IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(ownerId);
	if (pActor && pActor->GetLinkedVehicle() && pActor->GetLinkedVehicle()->GetEntityId())
		ownerId = pActor->GetLinkedVehicle()->GetEntityId();

	SAIStimulus stim(AISTIM_GRENADE, AIGRENADE_FLASH_BANG, ownerId, GetEntityId(),
	                 GetEntity()->GetWorldPos(), ZERO, radius);
	pPerceptionManager->RegisterStimulus(stim);

	SAIStimulus stimSound(AISTIM_SOUND, AISOUND_WEAPON, ownerId, 0,
	                      GetEntity()->GetWorldPos(), ZERO, radius * 3.0f);
	pPerceptionManager->RegisterStimulus(stimSound);
}

//------------------------------------------------------------------------
bool CProjectile::IsAlive() const
{
	return true;
}

// ===========================================================================
//	Try to detonate the projectile.
//
//	Returns:	True if the projectile actually detonated; otherwise false.
//
bool CProjectile::Detonate()
{
	UnregisterHazardArea();

	return true;
}

//------------------------------------------------------------------------
void CProjectile::Ricochet(EventPhysCollision* pCollision)
{
	if (!m_pAmmoParams->pRicochet)
		return;

	IActor* pActor = g_pGame->GetIGameFramework()->GetClientActor();
	if (!pActor)
		return;

	Vec3 dir = pCollision->vloc[0];
	dir.NormalizeSafe();

	float dot = pCollision->n.Dot(dir);

	if (dot >= 0.0f) // backface
		return;

	float b = 0, f = 0;
	uint32 matPierceability = 0;
	if (!gEnv->pPhysicalWorld->GetSurfaceParameters(pCollision->idmat[1], b, f, matPierceability))
		return;

	matPierceability &= sf_pierceable_mask;
	float probability = 0.25f + 0.25f * (max(0.0f, 7.0f - (float)matPierceability) / 7.0f);
	if (matPierceability >= 8 || cry_random(0.0f, 1.0f) > probability)
		return;

	f32 cosine = dir.Dot(-pCollision->n);
	float angle = RAD2DEG(fabs_tpl(acos_tpl(cosine)));
	if (angle < 10.0f)
		return;

	Vec3 ricochetDir = -2.0f * dot * pCollision->n + dir;
	ricochetDir.NormalizeSafe();

	Ang3 angles = Ang3::GetAnglesXYZ(Matrix33::CreateRotationVDir(ricochetDir));

	float rx = cry_random(-0.5f, 0.5f);
	float rz = cry_random(-0.5f, 0.5f);

	angles.x += rx * DEG2RAD(10.0f);
	angles.z += rz * DEG2RAD(10.0f);

	ricochetDir = Matrix33::CreateRotationXYZ(angles).GetColumn(1).normalized();

	Lineseg line(pCollision->pt, pCollision->pt + ricochetDir * 20.0f);
	Vec3 player = pActor->GetEntity()->GetWorldPos();

	float t;
	float distanceSq = Distance::Point_LinesegSq(player, line, t);

	if (distanceSq < 7.5 * 7.5 && (t >= 0.0f && t <= 1.0f))
	{
		if (distanceSq >= 0.25 * 0.25)
		{
			Sphere s;
			s.center = player;
			s.radius = 6.0f;

			Vec3 entry, exit;
			int intersect = Intersect::Lineseg_Sphere(line, s, entry, exit);
			if (intersect) // one entry or one entry and one exit
			{
				if (intersect == 0x2)
					entry = pCollision->pt;
				RicochetSound(entry);

				//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(entry, ColorB(255, 255, 255, 255), entry+ricochetDir, ColorB(255, 255, 255, 255), 2);
			}
		}
	}
}

CWeapon* CProjectile::GetWeapon() const
{
	if (m_weaponId)
	{
		IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_weaponId);
		if (pItem)
			return static_cast<CWeapon*>(pItem->GetIWeapon());
	}
	return 0;
}

//==================================================================
void CProjectile::OnHit(const HitInfo& hit)
{
	if (gEnv->bMultiplayer)
	{
		if (CheckAnyProjectileFlags(ePFlag_hitListener_mp_OnExplosion_only))
		{
			CRY_ASSERT(RequiresDelayedDestruct());
			return;
		}
	}

	//C4, special case
	if (CheckAnyProjectileFlags(ePFlag_noBulletHits))
		return;

	//Reduce hit points if hit, and explode (only for C4)
	if (hit.targetId == GetEntityId() && m_hitPoints > 0 && !CheckAnyProjectileFlags(ePFlag_destroying))
	{
		m_hitPoints -= (int)hit.damage;

		if (m_hitPoints <= 0)
			Explode(true);
	}
}
//==================================================================
void CProjectile::OnExplosion(const ExplosionInfo& explosion)
{
	if (gEnv->bMultiplayer)
	{
		if (RequiresDelayedDestruct())
		{
			if ((explosion.projectileId != 0) && (explosion.projectileId == GetEntityId()))
			{
				Destroy();
			}
		}
	}
}
//==================================================================
void CProjectile::OnServerExplosion(const ExplosionInfo& explosion)
{
	if (gEnv->bMultiplayer)
	{
		if (CheckAnyProjectileFlags(ePFlag_hitListener_mp_OnExplosion_only))
		{
			CRY_ASSERT(RequiresDelayedDestruct());
			return;
		}
	}

	//In case this was the same projectile that created the explosion, hitPoints should be already 0
	if (m_hitPoints <= 0 || CheckAnyProjectileFlags(ePFlag_destroying))
		return;

	//One check more, just in case...
	//if(CWeapon* pWep = GetWeapon())
	//if(pWep->GetEntityId()==explosion.weaponId)
	//return;

	//Stolen from SinglePlayer.lua ;p
	IPhysicalEntity* pPE = GetEntity()->GetPhysics();
	if (pPE)
	{
		float obstruction = 1.0f - gEnv->pSystem->GetIPhysicalWorld()->IsAffectedByExplosion(pPE);

		float distance = (GetEntity()->GetWorldPos() - explosion.pos).len();
		distance = max(0.0f, min(distance, explosion.radius));

		float effect = (explosion.radius - distance) / explosion.radius;
		effect = max(min(1.0f, effect * effect), 0.0f);
		effect = effect * (1.0f - obstruction * 0.7f);

		m_hitPoints -= (int)(effect * explosion.damage);

		if (m_hitPoints <= 0)
			Explode(true);
	}

}

//---------------------------------------------------------------------------------
void CProjectile::SetDefaultParticleParams(pe_params_particle* pParams)
{
	//Use ammo params if they exist
	if (m_pAmmoParams && m_pAmmoParams->pParticleParams)
	{
		pParams->mass = m_pAmmoParams->pParticleParams->mass;
		pParams->size = m_pAmmoParams->pParticleParams->size;
		pParams->thickness = m_pAmmoParams->pParticleParams->thickness;
		pParams->heading.Set(0.0f, 0.0f, 0.0f);
		pParams->velocity = 0.0f;
		pParams->wspin = m_pAmmoParams->pParticleParams->wspin;
		pParams->gravity = m_pAmmoParams->pParticleParams->gravity;
		pParams->normal.Set(0.0f, 0.0f, 0.0f);
		pParams->kAirResistance = m_pAmmoParams->pParticleParams->kAirResistance;
		pParams->accThrust = m_pAmmoParams->pParticleParams->accThrust;
		pParams->accLift = m_pAmmoParams->pParticleParams->accLift;
		pParams->q0.SetIdentity();
		pParams->surface_idx = m_pAmmoParams->pParticleParams->surface_idx;
		pParams->flags = m_pAmmoParams->pParticleParams->flags;
		pParams->pColliderToIgnore = nullptr;
		pParams->iPierceability = m_pAmmoParams->pParticleParams->iPierceability;
		pParams->rollAxis = m_pAmmoParams->pParticleParams->rollAxis;
	}
	else
	{
		int type = pParams->type;
		memset(pParams, 0, sizeof(pe_params_particle));
		pParams->type = type;
		pParams->velocity = 0.0f;
		pParams->iPierceability = 7;
	}
}

void CProjectile::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_pAmmoParams);
}

void CProjectile::PostRemoteSpawn()
{
	Launch(m_initial_pos, m_initial_dir, m_initial_vel);
}

//------------------------------------------------------------------------
void CProjectile::SerializeSpawnInfo(TSerialize ser)
{
	ser.Value("hostId", m_hostId, 'eid');
	ser.Value("ownerId", m_ownerId, 'eid');
	ser.Value("weaponId", m_weaponId, 'eid');
	ser.Value("pos", m_initial_pos, 'wrld');
	ser.Value("dir", m_initial_dir, 'dir0');
	ser.Value("vel", m_initial_vel, 'vel0');
	ser.Value("bulletPierceMod", m_bullet_pierceability_modifier, 'i8');

	if (ser.IsReading())
	{
		SetParams(SProjectileDesc(m_ownerId, m_hostId, m_weaponId, m_damage, 0.f, 0.f, 0.f, m_hitTypeId, m_bullet_pierceability_modifier, false));
	}
}

void CProjectile::SInfo::SerializeWith(TSerialize ser)
{
	ser.Value("hostId", hostId, 'eid');
	ser.Value("ownerId", ownerId, 'eid');
	ser.Value("weaponId", weaponId, 'eid');
	ser.Value("pos", pos, 'wrld');
	ser.Value("dir", dir, 'dir0');
	ser.Value("vel", vel, 'vel0');
	ser.Value("bulletPierceMod", bulletPierceMod, 'i8');
}

void CProjectile::FillOutProjectileSpawnInfo(SInfo* pSpawnInfo) const
{
	pSpawnInfo->hostId = m_hostId;
	pSpawnInfo->ownerId = m_ownerId;
	pSpawnInfo->weaponId = m_weaponId;
	pSpawnInfo->pos = m_initial_pos;
	pSpawnInfo->dir = m_initial_dir;
	pSpawnInfo->vel = m_initial_vel;
	pSpawnInfo->bulletPierceMod = m_bullet_pierceability_modifier;
}

//------------------------------------------------------------------------
ISerializableInfoPtr CProjectile::GetSpawnInfo()
{

	SInfo* p = new SInfo();
	FillOutProjectileSpawnInfo(p);

	return p;
}

uint8 CProjectile::GetDefaultProfile(EEntityAspects aspect)
{
	if (aspect == eEA_Physics)
		return m_pAmmoParams->physicalizationType;
	else
		return 0;
}

//------------------------------------------------------------------------
void CProjectile::PostSerialize()
{
	//	InitWithAI();
}

//------------------------------------------------------------------------
void CProjectile::InitWithAI()
{
	// register with ai if needed
	//FIXME
	//make AI ignore grenades thrown by AI; needs proper/readable grenade reaction
	if (m_pAmmoParams->aiType != AIOBJECT_NONE)
	{
		if (IEntity* pOwnerEntity = gEnv->pEntitySystem->GetEntity(m_ownerId))
		{
			if (IAIObject* pOwnerAI = pOwnerEntity->GetAI())
			{
				unsigned short int nOwnerType = pOwnerAI->GetAIType();
				if (nOwnerType != AIOBJECT_ACTOR)
				{
					gEnv->pAISystem->GetAIObjectManager()->CreateAIObject(AIObjectParams(m_pAmmoParams->aiType, 0, GetEntityId()));
				}
			}
		}
	}

	GetGameObject()->SetAIActivation(eGOAIAM_Always);
}

//----------------------------------------------------------------
void CProjectile::ResolveTarget(EventPhysCollision* pCollision, int& targetId, int& sourceId, IEntity*& pTargetEntity) const
{
	sourceId = 0;
	targetId = 1;
	IPhysicalEntity* pTarget = pCollision->pEntity[1];
	pTargetEntity = pTarget ? gEnv->pEntitySystem->GetEntityFromPhysics(pTarget) : 0;

	if (pTargetEntity == GetEntity())
	{
		pTarget = pCollision->pEntity[0];
		pTargetEntity = pTarget ? gEnv->pEntitySystem->GetEntityFromPhysics(pTarget) : 0;

		sourceId = 1;
		targetId = 0;
	}
};

//-----------------------------------------------------------------------
void CProjectile::SetUpParticleParams(IEntity* pOwnerEntity, uint8 pierceabilityModifier)
{
	CRY_ASSERT(m_pPhysicalEntity);

	pe_params_particle pparams;
	pparams.pColliderToIgnore = pOwnerEntity ? pOwnerEntity->GetPhysics() : nullptr;
	if (m_pAmmoParams)
	{
		pparams.iPierceability = max(0, min(m_pAmmoParams->pParticleParams->iPierceability + pierceabilityModifier, (int)sf_max_pierceable));

		bool resetParametersNotSimulatedByAI = false;
		if (!gEnv->bMultiplayer)
		{
			// If single player and AI then we need to reset some parameters that are not simulated
			// by the shoot prediction to match our actual throw
			const CActor* pOwnerActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId));
			resetParametersNotSimulatedByAI = pOwnerActor && !pOwnerActor->IsPlayer();
		}
		if (resetParametersNotSimulatedByAI)
		{
			pparams.accThrust = 0.0f;
			pparams.kAirResistance = 0.0f;
		}
	}
	m_pPhysicalEntity->SetParams(&pparams);
}

//-----------------------------------------------------------------------
bool CProjectile::ShouldCollisionsDamageTarget() const
{
	return false;
}

//-----------------------------------------------------------------------
bool CProjectile::CanCollisionsDamageTarget(IEntity* pTarget) const
{
	if (pTarget)
	{
		bool isOwner = pTarget->GetId() == m_ownerId || pTarget->GetId() == GetEntityId();
		bool isActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(pTarget->GetId()) != 0;
		const SCollisionParams* pCollisionParams = m_pAmmoParams->pCollision;
		return !isOwner && isActor &&
		       pCollisionParams && ((pCollisionParams->damageScale * pCollisionParams->damageLimit) != 0.0f); // Damage limit or damage scale of 0.0f won't cause any damage, so skip it
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------
void CProjectile::DoCollisionDamage(EventPhysCollision* pCollision, IEntity* pTarget)
{
	if (!gEnv->bServer || !CanCollisionsDamageTarget(pTarget) || !ShouldCollisionsDamageTarget())
		return;

#ifndef _RELEASE
	if (g_pGameCVars->g_DisableCollisionDamage)
	{
		return;
	}
#endif

	const SCollisionParams* pCollisionParams = m_pAmmoParams->pCollision;

	float speed = 0.0f;
	Vec3 dir(0, 0, 0);
	if (pCollision->vloc[0].GetLengthSquared() > 1e-6f)
	{
		Vec3 velocity = pCollision->vloc[0];
		speed = velocity.len();
		dir = velocity / speed;
	}

	float damage = speed * m_pAmmoParams->pParticleParams->mass * pCollisionParams->damageScale;
	damage = static_cast<float>(__fsel(pCollisionParams->damageLimit, min(pCollisionParams->damageLimit, damage), damage));

	if (damage != 0.0f)
	{
		// [*DavidR | 25/Nov/2010] Note: for now let's use collision hit type, but it could be worth either specifying the
		// hit type on the collision params or use the hitType in the SProjectileDesc in the future
		CGameRules* pGameRules = g_pGame->GetGameRules();
		HitInfo hitInfo(m_ownerId, pTarget ? pTarget->GetId() : 0, m_weaponId,
		                damage, 0.0f, pCollision->idmat[1], pCollision->partid[1],
		                CGameRules::EHitType::Collision, pCollision->pt, dir, pCollision->n);

		hitInfo.remote = false;
		hitInfo.projectileId = GetEntityId();
		hitInfo.bulletType = m_pAmmoParams->bulletType;

		pGameRules->ClientHit(hitInfo);
	}
}

//-----------------------------------------------------------------------
// Note: This will return actors in the area that are currently in vehicles.
void CProjectile::GetActorsInArea(TActorIds& actorIds, float range, const Vec3& center)
{
	//Instead of offsetting each actor up by 1.0f, we'll offset the check _down_ by 1.0f to get closer to the actor's entity origin
	Vec3 offsetCenter = center;
	offsetCenter.z -= 1.0f;
	AABB area(offsetCenter, range);

	CActorManager* pActorManager = CActorManager::GetActorManager();

	pActorManager->PrepareForIteration();

	const int kNumActors = pActorManager->GetNumActorsIncludingLocalPlayer();
	for (int i = 0; i < kNumActors; i++)
	{
		SActorData actorData;
		pActorManager->GetNthActorData(i, actorData);

		if (actorData.spectatorMode == CActor::eASM_None && actorData.health > 0.0f)
		{
			if (area.IsOverlapSphereBounds(actorData.position, 1.0f))
			{
				actorIds.push_back(actorData.entityId);
			}
		}
	}
}

//-----------------------------------------------------------------------
void CProjectile::ReportHit(EntityId targetId)
{
	if (!CheckAllProjectileFlags(ePFlag_hitRecorded))
	{
		SetProjectileFlags(ePFlag_hitRecorded, CPersistantStats::GetInstance()->IncrementWeaponHits(*this, targetId));
	}
}

//-----------------------------------------------------------------------
void CProjectile::RegisterLinkedProjectile(uint8 shotIndex)
{
	SetProjectileFlags(ePFlag_linked);
	g_pGame->GetWeaponSystem()->AddLinkedProjectile(GetEntityId(), m_weaponId, shotIndex);
}

//------------------------------------------------------------------------
void CProjectile::ProcessFailedDetonation()
{
	CHANGED_NETWORK_STATE(this, ASPECT_DETONATION);

	SetProjectileFlags(ePFlag_failedDetonation);

	TrailEffect(false);
	TrailSound(false);

	if (gEnv->bServer)
	{
		SetLifeTime(g_pGameCVars->i_failedDetonation_lifetime);

		pe_status_dynamics status;
		if (!m_pPhysicalEntity->GetStatus(&status))
			return;

		pe_params_particle params;
		params.accThrust = 0.f;
		params.accLift = 0.f;
		params.gravity = m_pAmmoParams->pParticleParams->gravity.IsZero() ? Vec3(0.f, 0.f, -9.8f) : m_pAmmoParams->pParticleParams->gravity;

		m_pPhysicalEntity->SetParams(&params);

		pe_action_set_velocity action;
		action.v = status.v * g_pGameCVars->i_failedDetonation_speedMultiplier;
		m_pPhysicalEntity->Action(&action);
	}
}

//------------------------------------------------------------------------
bool CProjectile::HasFailedDetonation() const
{
	return CheckAnyProjectileFlags(ePFlag_failedDetonation);
}

//------------------------------------------------------------------------
void CProjectile::SetFailedDetonation(bool failed)
{
	if (failed && !CheckAnyProjectileFlags(ePFlag_failedDetonation))
	{
		ProcessFailedDetonation();
	}
}

//------------------------------------------------------------------------
bool CProjectile::ProximityDetector(float proxyRadius)
{
	if (gEnv->bMultiplayer)
	{
		return ProximityDetector_MP(proxyRadius);
	}
	else
	{
		return ProximityDetector_SP(proxyRadius);
	}
}

bool CProjectile::ProximityDetector_MP(float proxyRadius)
{
	IActorSystem* pActorSystem = g_pGame->GetIGameFramework()->GetIActorSystem();

	CActor* pOwnerActor = static_cast<CActor*>(pActorSystem->GetActor(m_ownerId));
	if (!pOwnerActor || pOwnerActor->IsDead())
		return false;

	CGameRules* pGameRules = g_pGame->GetGameRules();

	const static IEntityClass* sVTOLClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(CVTOLVehicleManager::s_VTOLClassName);

	IVehicleSystem* pVehicleSystem = g_pGame->GetIGameFramework()->GetIVehicleSystem();

	CAutoAimManager& rAutoAimManager = g_pGame->GetAutoAimManager();

	const TAutoaimTargets& rAutoAimTargets = rAutoAimManager.GetAutoAimTargets();

	const Vec3& proxyCenter = GetEntity()->GetWorldPos();

	const AABB proximityAABB(proxyCenter, proxyRadius);

	Sphere proximitySphere(GetEntity()->GetWorldPos(), proxyRadius);

	for (int i = 0, size = rAutoAimTargets.size(); i < size; i++)
	{
		const SAutoaimTarget& rTarget = rAutoAimTargets[i];

		//Create a very generous bounds from the outer radius of the autoaim
		//Use the secondary aim as the center as it is generally the center of mass
		Sphere targetSphere(rTarget.secondaryAimPosition, rTarget.outerRadius * 2.0f);

		if (Overlap::Sphere_Sphere(proximitySphere, targetSphere))
		{
			if (rTarget.entityId == pOwnerActor->GetEntityId())
				continue;
			if (g_pGameCVars->g_friendlyfireratio == 0.f && pOwnerActor->IsFriendlyEntity(rTarget.entityId))
				continue;

			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(rTarget.entityId);
			const IEntityClass* pEntityClass = nullptr;

			if (!pEntity)
				continue;

			pEntityClass = pEntity->GetClass();

			if (pEntityClass != sVTOLClass && !pActorSystem->GetActor(rTarget.entityId) && !pVehicleSystem->GetVehicle(rTarget.entityId))
				continue;

			//check that we're actually sufficiently near the entity
			AABB entityBounds;
			pEntity->GetWorldBounds(entityBounds);
			if (!entityBounds.IsIntersectBox(proximityAABB))
				continue;

			return true;
		}
	}

	return false;
}

bool CProjectile::ProximityDetector_SP(float proxyRadius)
{
	IEntity* pOwner = gEnv->pEntitySystem->GetEntity(m_ownerId);
	if (!pOwner)
		return false;

	const Vec3& proxyCenter = GetEntity()->GetWorldPos();

	const AABB proximityAABB(proxyCenter, proxyRadius);

	// broadphase with box intersection
	SEntityProximityQuery query;
	query.box.min = proxyCenter - Vec3(proxyRadius, proxyRadius, proxyRadius);
	query.box.max = proxyCenter + Vec3(proxyRadius, proxyRadius, proxyRadius);
	gEnv->pEntitySystem->QueryProximity(query);

	CActor* pOwnerActor = static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(m_ownerId));
	if (pOwnerActor && pOwnerActor->IsDead())
		return false;

	for (int j = 0; j < query.nCount; ++j)
	{
		IEntity* pEntity = query.pEntities[j];
		IAIObject* pAIObject = pEntity->GetAI();
		if (!pOwnerActor || pEntity == pOwnerActor->GetEntity())
			continue;
		if (!pAIObject)
			continue;
		if (pOwnerActor && pOwnerActor->IsFriendlyEntity(pEntity->GetId()))
			continue;

		AABB entityBounds;
		pEntity->GetWorldBounds(entityBounds);
		if (!entityBounds.IsIntersectBox(proximityAABB))
			continue;

		return true;
	}

	return false;
}

void CProjectile::ProcessElectricHit(const SElectricHitTarget& target)
{
	if (CheckAllProjectileFlags(ePFlag_electricHit) != 0)
		return;
	SetProjectileFlags(ePFlag_electricHit, true);

	const SElectricProjectileParams* pElectricProjectileParams = GetAmmoParams().pElectricProjectileParams;

	// Spawn effect at hit position
	const EntityEffects::SEffectSpawnParams spawnParams(target.hitPosition);
	EntityEffects::SpawnParticleFX(GetCachedEffect(pElectricProjectileParams->hitEffect.c_str()), spawnParams);

	IParticleEffect* pSparkEffect = GetCachedEffect(pElectricProjectileParams->hitEffect.c_str());
	CWaterPuddle* pPuddle = g_pGame->GetWaterPuddleManager()->FindWaterPuddle(target.hitPosition);
	CGameRules* pGameRules = g_pGame->GetGameRules();
	const int ownerTeam = pGameRules->GetTeam(m_ownerId);

	if (target.entity)
	{
		HitInfo hitInfo(m_ownerId, target.entity, m_weaponId,
		                float(m_damage), 0.0f, target.matId, target.partId,
		                m_hitTypeId, target.hitPosition, target.hitDirection,
		                target.hitNormal);

		hitInfo.remote = IsRemote();
		hitInfo.projectileId = GetEntityId();
		hitInfo.hitViaProxy = CheckAnyProjectileFlags(ePFlag_firedViaProxy);
		hitInfo.aimed = CheckAnyProjectileFlags(ePFlag_aimedShot);

		g_pGame->GetGameRules()->ClientHit(hitInfo);

		ReportHit(target.entity);
	}
	else if (pPuddle)
	{
		pPuddle->ZapEnemiesOnPuddle(
		  ownerTeam, m_ownerId, m_weaponId, float(m_damage),
		  m_hitTypeId, pSparkEffect);
	}
	else if (s_materialLookup.IsMaterial(target.matId, CProjectile::SMaterialLookUp::eType_Water))
	{
		CActorManager* pActorManager = CActorManager::GetActorManager();
		pActorManager->PrepareForIteration();

		const float distanceThresholdSqr = pElectricProjectileParams->inWaterRange * pElectricProjectileParams->inWaterRange;

		const int numberOfActors = pActorManager->GetNumActors();
		const Vec3 referencePosition = target.hitPosition;

		// Hit any enemies in range
		for (int i = 0; i < numberOfActors; i++)
		{
			SActorData actorData;
			pActorManager->GetNthActorData(i, actorData);

			if (actorData.health <= 0.0f)
				continue;

			if (actorData.teamId == ownerTeam)
				continue;

			if ((actorData.position - referencePosition).len2() > distanceThresholdSqr)
				continue;

			const float waterLevel = gEnv->p3DEngine->GetWaterLevel(&actorData.position);

			const bool applyHit = (waterLevel > actorData.position.z) && (fabs_tpl(waterLevel - referencePosition.z) < 0.5f);

			if (applyHit)
			{
				const Vec3 hitPosition = actorData.position + Vec3Constants<float>::fVec3_OneZ;
				const Vec3 hitDirection = (hitPosition - referencePosition).GetNormalized();
				HitInfo hitInfo(m_ownerId, actorData.entityId, m_weaponId,
				                float(m_damage), 0.0f, -1, -1,
				                m_hitTypeId, hitPosition, hitDirection,
				                -hitDirection);

				hitInfo.remote = IsRemote();
				hitInfo.projectileId = GetEntityId();
				hitInfo.hitViaProxy = CheckAnyProjectileFlags(ePFlag_firedViaProxy);
				hitInfo.aimed = false;

				pGameRules->ClientHit(hitInfo);

				ReportHit(target.entity);

				const EntityEffects::SEffectSpawnParams effectSpawnParams(Vec3(actorData.position.x, actorData.position.y, waterLevel));
				EntityEffects::SpawnParticleFX(pSparkEffect, effectSpawnParams);
			}
		}
	}
}

// ===========================================================================
//	Get the hazard parameters.
//
//	Returns:	The hazard parameters (or nullptr if none could be obtained).
//
const SHazardAmmoParams* CProjectile::GetHazardParams() const
{
	if (m_pAmmoParams == nullptr)
	{
		return nullptr;
	}
	return m_pAmmoParams->pHazardParams;
}

// ===========================================================================
//	Query if we should generate a hazard area in front of the projectile.h
//
//	Returns:	True if a hazard area should be generated; otherwise false.
//
bool CProjectile::ShouldGenerateHazardArea() const
{
	return (GetHazardParams() != nullptr && !gEnv->bMultiplayer);
}

// ===========================================================================
//	Retrieve the pose of the hazard area in front of the projectile.
//
//	In:		The estimated forward direction normal vector (a 0 vector
//			will be ignored).
//	Out:	The start position of the area (in world-space) (nullptr is
//			invalid!)
//	Out:	The aiming direction normal vector (in world-space) (nullptr
//			is invalid!)
//
void CProjectile::RetrieveHazardAreaPoseInFrontOfProjectile(
  const Vec3& estimatedForwardNormal,
  Vec3* hazardStartPos, Vec3* hazardForwardNormal) const
{
	// Sanity checks.
	CRY_ASSERT(hazardStartPos != nullptr);
	CRY_ASSERT(hazardForwardNormal != nullptr);
	const SHazardAmmoParams* hazardParams = GetHazardParams();
	CRY_ASSERT(hazardParams != nullptr);

	const IEntity* entity = GetEntity();

	Vec3 forwardNormal;
	if (estimatedForwardNormal.IsZero())
	{
		forwardNormal = entity->GetForwardDir();
	}
	else
	{
		forwardNormal = estimatedForwardNormal;
	}
	*hazardForwardNormal = forwardNormal;
	*hazardStartPos = entity->GetWorldPos() + (forwardNormal * hazardParams->startPosNudgeOffset);
}

// ===========================================================================
//	Register the hazard area that will be placed in front of the rocket.
//
void CProjectile::RegisterHazardArea()
{
	UnregisterHazardArea(); // (Just in case).

	HazardSystem::HazardSetup setup;
	setup.m_OriginEntityID = GetEntityId();
	setup.m_ExpireDelay = -1.0f;            // (Never expire automatically).
	setup.m_WarnOriginEntityFlag = false;

	const CWeapon* weapon = GetWeapon();

	const SHazardAmmoParams* hazardParams = GetHazardParams();
	CRY_ASSERT(hazardParams != nullptr);

	HazardSystem::HazardProjectile context;
	RetrieveHazardAreaPoseInFrontOfProjectile(
	  Vec3(ZERO),
	  &context.m_Pos,
	  &context.m_MoveNormal);
	context.m_Radius = hazardParams->hazardRadius;
	context.m_MaxScanDistance = hazardParams->maxHazardDistance;
	context.m_MaxPosDeviationDistance = hazardParams->maxHazardApproxPosDeviation;
	context.m_MaxAngleDeviationRad = DEG2RAD(hazardParams->maxHazardApproxAngleDeviationDeg);
	if (weapon != nullptr)
	{
		context.m_IgnoredWeaponEntityID = weapon->GetEntityId();
	}
	m_HazardID = gGameAIEnv.hazardModule->ReportHazard(setup, context);
	CRY_ASSERT(m_HazardID.IsDefined());
}

// ===========================================================================
//	Synchronize the hazardous area in front of the projectile (if needed).
//
//	In:		The estimated forward direction of the missile in world-space.
//
void CProjectile::SyncHazardArea(const Vec3& estimatedForwardNormal)
{
	Vec3 startPos;
	Vec3 forwardNormal;
	RetrieveHazardAreaPoseInFrontOfProjectile(estimatedForwardNormal, &startPos, &forwardNormal);
	gGameAIEnv.hazardModule->ModifyHazard(m_HazardID,
	                                      startPos, forwardNormal);
}

// ===========================================================================
//	Unregister the hazardous area (if needed).
//
void CProjectile::UnregisterHazardArea()
{
	if (!m_HazardID.IsDefined())
	{
		return;
	}

	gGameAIEnv.hazardModule->ExpireHazard(m_HazardID);
	m_HazardID.Undefine();
}

float Projectile::GetPointBlankMultiplierAtRange(float fRange, float fPointBlankDistance, float fPointBlankFalloffDistance, float fPointBlankMultiplier)
{
	float pointBlankDist =
	  (fPointBlankFalloffDistance != fPointBlankDistance) ?
	  (fRange - fPointBlankDistance) / (fPointBlankFalloffDistance - fPointBlankDistance) :
	  0.0f;
	float pointBlankMultiplier = LERP(fPointBlankMultiplier, 1.0f, SATURATE(pointBlankDist));
	return pointBlankMultiplier;
}
