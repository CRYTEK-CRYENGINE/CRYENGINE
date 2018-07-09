// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id:$
$DateTime$
Description:  
-------------------------------------------------------------------------
History:
- 08:06:2007   : Created by Benito G.R.

*************************************************************************/
#include "StdAfx.h"
#include "C4Projectile.h"
#include "Player.h"
#include "GameRules.h"
#include "WeaponSystem.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "C4.h"
#include "PersistantStats.h"

#include <IVehicleSystem.h>
#include <IPerceptionManager.h>

CC4Projectile::CC4Projectile():
m_armed(false),
m_teamId(0),
m_disarmTimer(0.f),
m_pulseTimer(0.f),
m_pStatObj(NULL),
m_pArmedMaterial(NULL),
m_pDisarmedMaterial(NULL),
m_pLightSource(NULL),
m_OnSameTeam(false),
m_stickyProjectile(true),
m_isShowingUIIcon(false)
{
	Arm(false);
}

CC4Projectile::~CC4Projectile()
{
	if(m_pStatObj)
	{
		m_pStatObj->Release();
	}
	if(m_pArmedMaterial)
	{
		m_pArmedMaterial->Release();
	}

	if(m_pDisarmedMaterial)
	{
		m_pDisarmedMaterial->Release();
	}

	RemoveLight();

	CGameRules* pGameRules(NULL);
	if(gEnv->bMultiplayer && (pGameRules = g_pGame->GetGameRules()))
	{
		pGameRules->UnRegisterTeamChangedListener(this);
		pGameRules->UnRegisterClientConnectionListener(this);
	}
}

//------------------------------------------
bool CC4Projectile::Init(IGameObject *pGameObject)
{
	bool ok = CProjectile::Init(pGameObject);

	// C4 should always be saved (unlike other projectiles)
	IEntity* pEntity = GetEntity();
	pEntity->SetFlags(pEntity->GetFlags() & ~ENTITY_FLAG_NO_SAVE);

	if(!gEnv->bServer)
	{
		CreateLight();
	}

	if(gEnv->bMultiplayer)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		pGameRules->RegisterTeamChangedListener(this);
		pGameRules->RegisterClientConnectionListener(this);
	}

	return ok;
}

//------------------------------------------
void CC4Projectile::HandleEvent(const SGameObjectEvent &event)
{
	if (CheckAnyProjectileFlags(ePFlag_destroying))
		return;

	CProjectile::HandleEvent(event);

	if (event.event == eGFE_OnCollision)
	{
		EventPhysCollision *pCollision = (EventPhysCollision *)event.ptr;

		if(pCollision && pCollision->pEntity[0]->GetType()==PE_PARTICLE)
		{
			float bouncy, friction;
			uint32 pierceabilityMat;
			gEnv->pPhysicalWorld->GetSurfaceParameters(pCollision->idmat[1], bouncy, friction, pierceabilityMat);
			pierceabilityMat &= sf_pierceable_mask;
			uint32 pierceabilityProj = GetAmmoParams().pParticleParams ? GetAmmoParams().pParticleParams->iPierceability : 13;
			if (pierceabilityMat > pierceabilityProj)
				return;

			if(gEnv->bMultiplayer)
			{
				// Don't stick to weapons.
				if(pCollision->iForeignData[1]==PHYS_FOREIGN_ID_ENTITY)
				{
					if(IEntity* pEntity = (IEntity*)pCollision->pForeignData[1])
					{
						if(IItem* pItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pEntity->GetId()))
						{
							if(pItem->GetIWeapon())
							{
								return;
							}
						}
					}
				}
			}

			// Notify AI system about the C4.
			IPerceptionManager* perceptionManager = IPerceptionManager::GetInstance();
			if (perceptionManager)
			{
				// Associate event with vehicle if the shooter is in a vehicle (tank cannon shot, etc)
				EntityId ownerId = m_ownerId;
				IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(ownerId);
				if (pActor && pActor->GetLinkedVehicle() && pActor->GetLinkedVehicle()->GetEntityId())
					ownerId = pActor->GetLinkedVehicle()->GetEntityId();

				SAIStimulus stim(AISTIM_EXPLOSION, 0, ownerId, GetEntityId(),
					GetEntity()->GetWorldPos(), ZERO, 12.0f, AISTIMPROC_ONLY_IF_VISIBLE);
				perceptionManager->RegisterStimulus(stim);

				SAIStimulus soundStim(AISTIM_SOUND, AISOUND_COLLISION, ownerId, GetEntityId(),
					GetEntity()->GetWorldPos(), ZERO, 8.0f);
				perceptionManager->RegisterStimulus(soundStim);
			}
		}

		if (gEnv->bServer && !m_stickyProjectile.IsStuck())
			m_stickyProjectile.Stick(
				CStickyProjectile::SStickParams(this, pCollision, CStickyProjectile::eO_AlignToSurface));
	}
}

//--------------------------------------------
void CC4Projectile::ProcessEvent(const SEntityEvent& event)
{
	BaseClass::ProcessEvent(event);

	if(event.event == ENTITY_EVENT_TIMER && event.nParam[0] == ePTIMER_ACTIVATION)
	{
		if(m_disarmTimer <= 0.f)
		{
			Arm(true);
		}
	}
}

//--------------------------------------------
void CC4Projectile::Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale)
{
	CProjectile::Launch(pos, dir, velocity, speedScale);

	if(!gEnv->bMultiplayer)
	{
		//don't want the hud grenade indicator on c4 in multiplayer
		OnLaunch();
	}

	if(m_pAmmoParams->armTime > 0.f)
	{
		GetEntity()->SetTimer(ePTIMER_ACTIVATION, (int)(m_pAmmoParams->armTime*1000.f));
		Arm(false);
	}
	else
	{
		Arm(true);
	}

	//Set up armed/disarmed materials and set material based on initial armed state
	if(SC4ExplosiveParams* pExplosiveParams = m_pAmmoParams->pC4ExplosiveParams)
	{
		if(int count = GetEntity()->GetSlotCount())
		{
			for(int i = 0; i < count; i++)
			{
				SEntitySlotInfo info;
				GetEntity()->GetSlotInfo(i, info);
				if(info.pStatObj)
				{
					IMaterial* pMaterial = info.pStatObj->GetMaterial();
					if(pMaterial)
					{
						m_pStatObj = info.pStatObj;
						m_pStatObj->AddRef();

						IMaterialManager* pMatManager = gEnv->p3DEngine->GetMaterialManager();
						if( m_pArmedMaterial = pMatManager->LoadMaterial(pExplosiveParams->armedMaterial, false) )
						{
							m_pArmedMaterial->AddRef();
						}
						if( m_pDisarmedMaterial = pMatManager->LoadMaterial(pExplosiveParams->disarmedMaterial, false) )
						{
							m_pDisarmedMaterial->AddRef();
						}
						
						info.pStatObj->SetMaterial(m_armed ? m_pArmedMaterial : m_pDisarmedMaterial);

						break;
					}
				}
			}
		}
	}
}

void CC4Projectile::SetParams(const SProjectileDesc& projectileDesc)
{
	if(CGameRules* pGameRules = g_pGame->GetGameRules())
	{
		if(projectileDesc.ownerId) //Can be 0 for late joiners
		{
			m_teamId = pGameRules->GetTeam(projectileDesc.ownerId);

			// if this is a team game, record which team placed this claymore...
			if(gEnv->bServer)
			{
				pGameRules->SetTeam(m_teamId, GetEntityId());
			}

			if(!gEnv->IsDedicated())
			{
				const EntityId clientId = g_pGame->GetIGameFramework()->GetClientActorId();
				int localPlayerTeam = pGameRules->GetTeam( clientId );
				m_OnSameTeam = m_teamId ? m_teamId == localPlayerTeam : clientId == projectileDesc.ownerId;

				if(gEnv->bServer)
				{
					CreateLight();
				}

				if(gEnv->bMultiplayer)
				{
					SetupUIIcon();
				}
			}
		}
	}
	
	CProjectile::SetParams(projectileDesc);
}

bool CC4Projectile::CanDetonate()
{
	return m_armed;
}

bool CC4Projectile::Detonate()
{
	if(m_armed)
	{
		if(m_stickyProjectile.IsStuck())
		{
			IEntity* pEntity = GetEntity();

			Vec3 pos = pEntity->GetWorldPos();
			Matrix34 mtx = pEntity->GetWorldTM();
			Vec3 up = mtx.GetColumn2();

			CProjectile::SExplodeDesc explodeDesc(true);
			explodeDesc.impact = true;
			explodeDesc.pos = pos;
			explodeDesc.normal = up;
			explodeDesc.vel = -up;
			Explode(explodeDesc);
		}
		else
		{
			Explode(CProjectile::SExplodeDesc(true));
		}
	}
	
	return m_armed;
}

//------------------------------------------------------------------------
void CC4Projectile::FullSerialize(TSerialize ser)
{
	CProjectile::FullSerialize(ser);

	ser.Value("m_teamId", m_teamId);
	ser.Value("m_armed", m_armed);
	m_stickyProjectile.Serialize(ser);
}

//------------------------------------------------------------------------
bool CC4Projectile::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags)
{
	if(aspect == ASPECT_C4_STATUS)
	{
		bool isArmed = m_armed;
		ser.Value("m_armed", isArmed, 'bool');
		if(ser.IsReading())
		{
			Arm(isArmed);
		}
	}

	m_stickyProjectile.NetSerialize(this, ser, aspect);
	return BaseClass::NetSerialize(ser, aspect, profile, pflags);
}

NetworkAspectType CC4Projectile::GetNetSerializeAspects()
{
	return BaseClass::GetNetSerializeAspects() | m_stickyProjectile.GetNetSerializeAspects() | ASPECT_C4_STATUS;
}

void CC4Projectile::Update(SEntityUpdateContext &ctx, int updateSlot)
{
	if(gEnv->bMultiplayer)
	{
		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_ownerId);
		if(!pActor || pActor->IsDead())
		{
			if(gEnv->bServer)
			{
				Destroy();
			}
			else
			{
				GetEntity()->Hide(true);
			}

			RemoveLight();
			if(m_isShowingUIIcon)
			{
				SHUDEvent hudEvent(eHUDEvent_RemoveC4Icon);
				hudEvent.AddData((int)GetEntityId());
				CHUDEventDispatcher::CallEvent(hudEvent);
				m_isShowingUIIcon = false;
			}
		}
	}

	if(m_pLightSource)
	{
		UpdateLight(ctx.fFrameTime, false);
	}

	if(m_disarmTimer > 0.f)
	{
		m_disarmTimer -= ctx.fFrameTime;
		if(m_disarmTimer <= 0.f)
		{
			m_disarmTimer = 0.f;
			Arm(true);
		}
	}

	BaseClass::Update(ctx, updateSlot);
}

//------------------------------------------------------------------------
void CC4Projectile::Explode(const CProjectile::SExplodeDesc& explodeDesc)
{
	if(!CheckForDelayedDetonation(explodeDesc.pos))
	{
		BaseClass::Explode(explodeDesc);

		RemoveLight();
	}
}

void CC4Projectile::OnHit( const HitInfo& hitInfo)
{
	if(gEnv->bMultiplayer)
	{
		if(hitInfo.targetId == GetEntityId() && m_hitPoints > 0 && !CheckAnyProjectileFlags(ePFlag_destroying))
		{
			if(CGameRules* pGameRules = g_pGame->GetGameRules())
			{
				EntityId shooterTeamId = pGameRules->GetTeam(hitInfo.shooterId);
				if(m_teamId == 0 || shooterTeamId != m_teamId)
				{
					m_hitPoints -= (int)hitInfo.damage;
					if(m_hitPoints <= 0)
					{
						//We need to be armed or detonate won't work. As this is the last frame before the C4 vanishes, it won't have any knock-on.
						m_armed = true;

						//We need the damage to be credited to the person that shot the C4, which means the owner will be temporarily set to
						//	the shooter for the duration of the Detonate call.

						EntityId originalOwnerId = m_ownerId;

						m_ownerId = hitInfo.shooterId;
						
						Detonate();

						m_ownerId = originalOwnerId;
					}
				}
			}
		}

		return;
	}

	CProjectile::OnHit(hitInfo);
}

void CC4Projectile::OnServerExplosion( const ExplosionInfo& explosionInfo )
{
	if(gEnv->bMultiplayer)
	{
		//Check for explosions
		IPhysicalEntity *pPE = GetEntity()->GetPhysics();
		if(pPE)
		{
			float affected = gEnv->pSystem->GetIPhysicalWorld()->IsAffectedByExplosion(pPE);
			if(affected > 0.001f && m_hitPoints > 0 && !CheckAnyProjectileFlags(ePFlag_destroying))
			{
				//Only enemy explosions
				if(CGameRules* pGameRules = g_pGame->GetGameRules())
				{
					EntityId explosionTeamId = pGameRules->GetTeam(explosionInfo.shooterId);
					if(explosionTeamId != m_teamId)
					{
						m_armed = true;
						Detonate();
						return;
					}
				}		
			}
		}
	}

	CProjectile::OnServerExplosion(explosionInfo);
}

void CC4Projectile::Arm(bool arm)
{
	if(arm != m_armed)
	{
		m_armed = arm;

		if(gEnv->bServer)
		{
			CHANGED_NETWORK_STATE(this, ASPECT_C4_STATUS);

			if(CWeapon* pWeapon = GetWeapon())
			{
				CHANGED_NETWORK_STATE(pWeapon, CC4::ASPECT_DETONATE);
			}
		}

		if(m_pStatObj)
		{
			m_pStatObj->SetMaterial(arm ? m_pArmedMaterial : m_pDisarmedMaterial);
		}

		if(m_pLightSource)
		{
			UpdateLight(0.f, true);
		}
	}
}

void CC4Projectile::CreateLight()
{
	if(m_pAmmoParams->pC4ExplosiveParams)
	{
		m_pLightSource = gEnv->p3DEngine->CreateLightSource();
		if(m_pLightSource)
		{
			UpdateLight(0.f, false);
			SetLightParams();
			gEnv->p3DEngine->RegisterEntity(m_pLightSource);
		}
	}
}

void CC4Projectile::SetLightParams()
{
	SC4ExplosiveParams* pExplosiveParams = m_pAmmoParams->pC4ExplosiveParams;

	SRenderLight lightParams;
	//Members of the same team see armed status - enemies always see red
	lightParams.m_Flags |= DLF_POINT;
	lightParams.SetLightColor(m_armed && m_OnSameTeam ? pExplosiveParams->armedLightColour : pExplosiveParams->disarmedLightColour);
	lightParams.SetSpecularMult(pExplosiveParams->specularMultiplier);
	lightParams.SetRadius(pExplosiveParams->lightRadius);
	lightParams.m_sName = "C4 Projectile Light";

	m_pLightSource->SetLightProperties(lightParams);
}

void CC4Projectile::UpdateLight(float fFrameTime, bool forceColorChange)
{
	const Matrix34& mat = GetEntity()->GetWorldTM();
	m_pLightSource->SetMatrix(mat);

	SC4ExplosiveParams* pExplosiveParams = m_pAmmoParams->pC4ExplosiveParams;

	if(pExplosiveParams->pulseBeatsPerSecond > 0.f)
	{
		float fNewPulseTimer = m_pulseTimer + (fFrameTime * pExplosiveParams->pulseBeatsPerSecond);

		float timerScaledToWavelength = fNewPulseTimer * gf_PI;
		timerScaledToWavelength = (float)__fsel(timerScaledToWavelength - gf_PI2, timerScaledToWavelength - gf_PI2, timerScaledToWavelength);

		float finalMult = sinf(timerScaledToWavelength) * 0.5f + 0.5f;

		finalMult *= 1.0f - pExplosiveParams->pulseMinColorMultiplier;
		finalMult += pExplosiveParams->pulseMinColorMultiplier;
		m_pulseTimer = fNewPulseTimer;
		
		SRenderLight& light = m_pLightSource->GetLightProperties();
		light.SetLightColor(ColorF(m_armed && m_OnSameTeam ? pExplosiveParams->armedLightColour * finalMult : pExplosiveParams->disarmedLightColour * finalMult));
	}
	else if(forceColorChange)
	{
		SRenderLight& light = m_pLightSource->GetLightProperties();
		light.SetLightColor(ColorF(m_armed && m_OnSameTeam ? pExplosiveParams->armedLightColour : pExplosiveParams->disarmedLightColour));
	}
}

void CC4Projectile::OnExplosion( const ExplosionInfo& explosionInfo)
{
	if(explosionInfo.projectileId == GetEntityId())
	{
		RemoveLight();

		bool dangerous = !m_OnSameTeam || g_pGame->GetGameRules()->GetFriendlyFireRatio() > 0.f;
		if(gEnv->bMultiplayer && !dangerous && (!gEnv->bServer || !gEnv->IsDedicated()))
		{
			SHUDEvent hudEvent(eHUDEvent_RemoveC4Icon);
			hudEvent.AddData((int)GetEntityId());
			CHUDEventDispatcher::CallEvent(hudEvent);
		}
	}

	CProjectile::OnExplosion(explosionInfo);
}

void CC4Projectile::RemoveLight()
{
	if(m_pLightSource)
	{
		gEnv->p3DEngine->UnRegisterEntityDirect(m_pLightSource);
		gEnv->p3DEngine->DeleteLightSource(m_pLightSource);
		m_pLightSource = NULL;
	}
}

void CC4Projectile::SetupUIIcon()
{
	if(GetEntityId())
	{
		const bool dangerous = !m_OnSameTeam || g_pGame->GetGameRules()->GetFriendlyFireRatio() > 0.f;
		//If the C4 can't harm us we show a C4 icon (ThreatAwareness shows enemy C4 as well)
		const CPlayer* pPlayer = static_cast<CPlayer*>(gEnv->pGameFramework->GetClientActor());
		const bool shouldShow = !dangerous;
		if(shouldShow && !m_isShowingUIIcon)
		{
			SHUDEvent hudEvent(eHUDEvent_OnC4Spawned);
			hudEvent.AddData((int)GetEntityId());
			hudEvent.AddData((bool)dangerous);
			CHUDEventDispatcher::CallEvent(hudEvent);
			m_isShowingUIIcon = true;
		}
		else if(m_isShowingUIIcon && !shouldShow)
		{
			SHUDEvent hudEvent(eHUDEvent_RemoveC4Icon);
			hudEvent.AddData((int)GetEntityId());
			CHUDEventDispatcher::CallEvent(hudEvent);
			m_isShowingUIIcon = false;
		}
	}
}

void CC4Projectile::OnChangedTeam( EntityId entityId, int oldTeamId, int newTeamId )
{
	const EntityId clientId = gEnv->pGameFramework->GetClientActorId();
	if(entityId == clientId)
	{
		m_OnSameTeam = m_teamId ? m_teamId == newTeamId : GetOwnerId() == clientId;
		if(m_pLightSource)
		{
			UpdateLight(0.f, true); //Force colour update
		}
		if(gEnv->bMultiplayer)
		{
			SetupUIIcon();
		}
	}
}

void CC4Projectile::SerializeSpawnInfo( TSerialize ser )
{
	BaseClass::SerializeSpawnInfo(ser);

	int prevTeamId = m_teamId;
	ser.Value("teamId", m_teamId, 'team');
	if(ser.IsReading() && prevTeamId != m_teamId)
	{
		const EntityId clientId = g_pGame->GetIGameFramework()->GetClientActorId();
		int localPlayerTeam = g_pGame->GetGameRules()->GetTeam( clientId );
		m_OnSameTeam = m_teamId ? m_teamId == localPlayerTeam : GetOwnerId() == clientId;
	}
}

ISerializableInfoPtr CC4Projectile::GetSpawnInfo()
{
	SC4Info* pSpawnInfo = new SC4Info();

	FillOutProjectileSpawnInfo(pSpawnInfo);
	pSpawnInfo->team = m_teamId;

	return pSpawnInfo;
}

void CC4Projectile::OnOwnClientEnteredGame()
{
	SetupUIIcon();
}

void CC4Projectile::PostRemoteSpawn()
{
	BaseClass::PostRemoteSpawn();

	SetupUIIcon();
}

EntityId CC4Projectile::GetStuckToEntityId() const
{
	return m_stickyProjectile.GetStuckEntityId();
}
