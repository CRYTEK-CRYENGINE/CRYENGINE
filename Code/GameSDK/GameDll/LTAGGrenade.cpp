// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Rocket

-------------------------------------------------------------------------
History:
- 25:02:2009   11:15 : Created by Filipe Amim
- 16:09:2009   Re-factor to work together with LTagSingle (Benito G.R.)

*************************************************************************/
#include "StdAfx.h"
#include "LTAGGrenade.h"
#include "Game.h"
#include "Bullet.h"
#include "GameRules.h"
#include "Player.h"
#include "GameCVars.h"
#include "Utility/CryWatch.h"
#include <CryAISystem/IAIActor.h>
#include "IVehicleSystem.h"

namespace
{

	static Vec3 diffToLine(const Vec3& pos, const Vec3& p0, const Vec3& p1)
	{
		const Vec3 line = p1 - p0;
		const Vec3 diff = pos - p0;
		float lenSq = line.GetLengthSquared();
		if (lenSq>1e-5f)
		{
			float t = clamp_tpl((diff * line) / lenSq, 0.f, 1.f);
			return diff - line * t;
		}
		return diff;
	}



	bool ShouldRicochetDetonateOnImpact(const SLTagGrenadeParams::SCommonParams& grenadeParams, IActor* pHitActor, IEntity* pTargetEntity)
	{
		if (pTargetEntity && !gEnv->bMultiplayer)
		{
			const IEntityClass* pHitEntityClass = pTargetEntity->GetClass();
			static IEntityClass* pHelicopterClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("blackopshelicoptercrysis2");
			
			if (pHitEntityClass == pHelicopterClass)
			{
				return true;
			}
		}

		bool bDetonate = grenadeParams.m_detonateOnImpact;

		if( !bDetonate && grenadeParams.m_detonateOnActorImpact )
		{
			if(pHitActor)
			{
				bDetonate = true;
			}
			else if ( gEnv->bMultiplayer && pTargetEntity && g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pTargetEntity->GetId()))
			{
				//If detonateOnActorImpact is set, we also want to detonate on collision with vehicles (MP design decision, so gEnv->bMultiplayer required)
				bDetonate = true;
			}
		}

		return bDetonate;
	}


}

CRY_IMPLEMENT_GTI(CLTAGGrenade, CProjectile);

//------------------------------------------------------------------------
CLTAGGrenade::CLTAGGrenade()
:	m_launchLoc(0,0,0)
,	m_grenadeType(ELTAGGrenadeType_RICOCHET)
,	m_state(EState_NONE)
,	m_activeTime(0)
,	m_extraArmTime(0)
//,	m_armedSoundId(INVALID_SOUNDID)
, m_characterAttachment(NULL)
,	m_enemyKilledByCollision(false)
,	m_normal(ZERO)
, m_stickyProjectile(true)
,	m_netState(k_netStateInvalid)
{
	m_postStepLock=0;
}

//------------------------------------------------------------------------
CLTAGGrenade::~CLTAGGrenade()
{
	WriteLock lock(m_postStepLock);
	GetGameObject()->ReleaseProfileManager(this);
}

void CLTAGGrenade::UpdateStoredTracjectory(const Vec3& pos)
{
	int i;
	for (i=0; i<k_maxNetPoints-1; i++)
	{
		m_lastPos[i] = m_lastPos[i+1];
	}
	m_lastPos[i] = pos;
}

//------------------------------------------------------------------------
void CLTAGGrenade::HandleEvent(const SGameObjectEvent &event)
{
	if (CheckAnyProjectileFlags(ePFlag_destroying))		
		return;
	
	if (gEnv->bMultiplayer && event.event == eGFE_OnPostStep && (event.flags&eGOEF_LoggedPhysicsEvent)==0)
	{
		//==============================================================================
		// Warning this is called directly from the physics thread! Must be thread safe!
		//==============================================================================
		WriteLock lock(m_postStepLock);
		IPhysicalEntity* pent=GetEntity()->GetPhysics();
		if (m_netState==k_netStateReading)
		{
			if (m_lastPosSet!=m_netDesiredPos)
			{
				// Keep a record of the trajectory
				pe_status_dynamics psd;
				pent->GetStatus(&psd);
				if ((m_netDesiredPos-m_lastPos[k_maxNetPoints-1]).GetLengthSquared() > sqr(0.2f))
				{
					UpdateStoredTracjectory(m_netDesiredPos);
				}
				if (m_wasLaunchedByLocalPlayer)
				{

					// Find where serialisaed pos comes closest to the stored trajectory
					float minErrorSq = 1e6f;
					Vec3 minError(0.f,0.f,0.f);
					for (int i=0; i<k_maxNetPoints-1; i++)
					{
						Vec3 p0 = m_lastPos[i];
						Vec3 p1 = m_lastPos[i+1];
						Vec3 error = diffToLine(m_netDesiredPos, p0, p1);
						float errorSq = error.GetLengthSquared();
						if (errorSq<minErrorSq)
						{
							minErrorSq = errorSq;
							minError = error;
						}
					}

					pe_params_pos ppp;
					if (minError.GetLengthSquared() < sqr(0.3f))
					{
						// Approximately update the position with the error
						ppp.pos = 0.8f*(psd.centerOfMass + minError) + 0.2f*m_netDesiredPos;
					}
					else
					{
						ppp.pos = m_netDesiredPos;
					}
					pent->SetParams(&ppp);
				}
				else
				{
					// Simply Set
					pe_action_set_velocity asv;
					pe_params_pos ppp;
					ppp.pos = m_netDesiredPos;
					asv.v = m_netDesiredVel;
					pent->Action(&asv, 1);
					pent->SetParams(&ppp, 1);
				}
				m_lastPosSet = m_netDesiredPos;
			}
		}
		if (m_netState==k_netStateWriting)
		{
			// Simply keep updating the desired
			// position for writing later
			pe_status_dynamics psd;
			pent->GetStatus(&psd);
			m_netDesiredVel = psd.v;
			m_netDesiredPos = psd.centerOfMass;
		}
	}

	if (!gEnv->bServer || IsDemoPlayback())
		return;

	switch (event.event)
	{
	case eGFE_OnCollision:
		OnCollision(event);
		break;
	}

	CProjectile::HandleEvent(event);

	CActor* pStuckEntity =
		static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(
			m_stickyProjectile.GetStuckEntityId()));
	if (pStuckEntity && pStuckEntity->IsDead())
		m_enemyKilledByCollision = true;
}

//------------------------------------------------------------------------
void CLTAGGrenade::Launch(const Vec3 &pos, const Vec3 &dir, const Vec3 &velocity, float speedScale)
{
	CProjectile::Launch(pos, dir, velocity, speedScale);
	//	m_ownerId = GetWeapon()->GetOwner()->GetId();

	OnLaunch();
	
	// Bespoke net lerping
	if (gEnv->bMultiplayer)
	{
		if (IPhysicalEntity* pent=GetEntity()->GetPhysics())
		{
			// Enable Post Physics Callbacks
			pe_params_flags flags;
			flags.flagsOR = pef_monitor_poststep;
			pent->SetParams(&flags);
			GetGameObject()->EnablePhysicsEvent(true, eEPE_OnPostStepImmediate);

			// Did we launch this?
			m_wasLaunchedByLocalPlayer = (m_ownerId == g_pGame->GetIGameFramework()->GetClientActorId()) ? 1 : 0;

			// Get initial network conditions
			pe_status_dynamics psd;
			pent->GetStatus(&psd);
			m_netDesiredVel = psd.v;
			m_netDesiredPos = psd.centerOfMass;
			m_lastPosSet = psd.centerOfMass;
			m_netState = k_netStateInvalid;
			for (int i=0; i<k_maxNetPoints; i++)
			{
				m_lastPos[i] = psd.centerOfMass;
			}
		}
	}

	const SLTagGrenadeParams::SCommonParams& grenadeParams = GetModeParams();

	m_launchLoc = pos;

	m_extraArmTime = grenadeParams.m_armTime * 1.15f;
	m_activeTime = 0;

	if (m_grenadeType == ELTAGGrenadeType_RICOCHET)
		ChangeState(ERicochetState_FLYING);

	ChangeTexture(grenadeParams.m_fireMaterial);
	ChangeTrail(grenadeParams.m_fireTrail);
}


//------------------------------------------------------------------------
void CLTAGGrenade::Update(SEntityUpdateContext &ctx, int updateSlot)
{
	inherited::Update(ctx, updateSlot);

	if(!HasFailedDetonation())
	{
		const SLTagGrenadeParams::SCommonParams& grenadeParams = GetModeParams();

		StateUpdate(ctx, grenadeParams);

		if(grenadeParams.m_detonateWithProximity)
		{
			ProximityGrenadeUpdate(ctx, grenadeParams);
		}
	
		UpdateLTagTimeOut(grenadeParams);
	}
}


//------------------------------------------------------------------------
void CLTAGGrenade::UpdateLTagTimeOut(const SLTagGrenadeParams::SCommonParams& grenadeParams)
{
	if (m_totalLifetime < grenadeParams.m_ltagLifetime)
		return;

	if(m_pAmmoParams->quietRemoval)
		Destroy();
	else
		ExplodeGrenade();
}


//------------------------------------------------------------------------
void CLTAGGrenade::ProximityGrenadeUpdate(SEntityUpdateContext &ctx, const SLTagGrenadeParams::SCommonParams& grenadeParams)
{
	switch (m_grenadeType)
	{
		case ELTAGGrenadeType_STICKY:
		{
			if(m_state == EStickyState_UNSAFE && m_activeTime >= m_extraArmTime)
			{
				if (m_enemyKilledByCollision || ProximityDetector(grenadeParams.m_boxDimension))
					ExplodeGrenade();
			}
			break;
		}

		case ELTAGGrenadeType_RICOCHET:
		{
			if (m_state == ERicochetState_FLYING)
			{
				if (ProximityDetector(grenadeParams.m_innerBoxDimension))
					ExplodeGrenade();
			}
			else if (m_state == ERicochetState_PROXY)
			{
				if (ProximityDetector(grenadeParams.m_boxDimension))
					ChangeState(ERicochetState_ARMED);
			}
			break;
		}
	}
}

//------------------------------------------------------------------------
void CLTAGGrenade::StateUpdate(SEntityUpdateContext &ctx, const SLTagGrenadeParams::SCommonParams& grenadeParams)
{
	m_activeTime += ctx.fFrameTime;

	switch (m_state)
	{
	case EStickyState_SAFE:
		if (!IsOwnerToClose())
		{
			ChangeState(EStickyState_ARMING);
			m_activeTime = 0;
		}
		break;

	case EStickyState_UNSAFE:
		if (!g_pGame->GetGameRules()->IsMultiplayerDeathmatch())
		{
			if (IsOwnerToClose())
			{
				ChangeState(EStickyState_SAFE);
			}
		}
		break;

	case EStickyState_ARMING:
		if (IsOwnerToClose())
		{
			ChangeState(EStickyState_SAFE);
		}
		else if (m_activeTime >= grenadeParams.m_armTime)
		{
			ChangeState(EStickyState_UNSAFE);
		}
		break;


	case ERicochetState_FLYING:
		m_normal = GetEntity()->GetWorldTM().GetColumn0();
		m_activeTime = 0;
		break;

	case ERicochetState_PROXY:
		m_normal = GetEntity()->GetWorldTM().GetColumn0();
		m_activeTime = 0;
		break;

	case ERicochetState_ARMED:
		m_normal = GetEntity()->GetWorldTM().GetColumn0();

		if (m_activeTime > grenadeParams.m_armTime)
		{
			ExplodeGrenade();
		}
		break;
	}
}


//------------------------------------------------------------------------
void CLTAGGrenade::StateEnter(EState state)
{
	const SLTagGrenadeParams::SCommonParams& grenadeParams = GetModeParams();

	switch (state)
	{
	case EStickyState_SAFE:
		PlaySound(grenadeParams.m_disarmSound);
		ChangeTexture(grenadeParams.m_disarmMaterial);
		ChangeTrail(grenadeParams.m_disarmTrail);
		break;

	case EStickyState_UNSAFE:
		{
			//m_armedSoundId = PlaySound(grenadeParams.m_armedSound);
			ChangeTexture(grenadeParams.m_armedMaterial);
			ChangeTrail(grenadeParams.m_armedTrail);
			/*ISound* pSound = GetAudioProxy()->GetSound(m_armedSoundId);
			if (pSound)
				pSound->GetInterfaceDeprecated()->SetLoopMode(true);*/

			if (g_pGame->GetGameRules()->IsMultiplayerDeathmatch())
			{
#if 0
//			IEntity* pOwner = gEnv->pEntitySystem->GetEntity(m_ownerId);
				EntityId clientActorId = gEnv->pGameFramework->GetClientActorId();
				int clientActorTeam = g_pGame->GetGameRules()->GetTeam(clientActorId);
				int ownerTeam = g_pGame->GetGameRules()->GetTeam(m_ownerId);
#endif

				CActor* clientActor = static_cast<CActor*>(gEnv->pGameFramework->GetClientActor());
				
				if (clientActor && clientActor->IsFriendlyEntity(m_ownerId))
				{
					CryLog("LTAGGrenade::StateEnter() entering EStickyState_UNSAFE - grenade owner is friendlyEntity to current player clientActor - setting safe armed material");
					ChangeTexture(grenadeParams.m_armedSafeMaterial);	// TODO replace with new material when its available - blue
					ChangeTrail(grenadeParams.m_armedSafeTrail);		// crucially controls the glow colour
				}
				else
				{
					CryLog("LTAGGrenade::StateEnter() entering EStickyState_UNSAFE - grenade owner is NOT friendlyEntity to current player clientActor - setting unsafe armed material");
					ChangeTexture(grenadeParams.m_armedMaterial);
					ChangeTrail(grenadeParams.m_armedTrail);
				}
			}
			else
			{
				ChangeTexture(grenadeParams.m_armedMaterial);
				ChangeTrail(grenadeParams.m_armedTrail);
			}
		}
		break;

	case EStickyState_ARMING:
		PlaySound(grenadeParams.m_armSound);
		ChangeTexture(grenadeParams.m_armMaterial);
		ChangeTrail(grenadeParams.m_armTrail);
		break;


	case ERicochetState_PROXY:
		ChangeTexture(grenadeParams.m_bounceMaterial);
		ChangeTrail(grenadeParams.m_bounceTrail);
		break;

	case ERicochetState_ARMED:
		m_activeTime = 0.0f;
		ChangeTexture(grenadeParams.m_armMaterial);
		ChangeTrail(grenadeParams.m_armTrail);
		break;
	}
}


//------------------------------------------------------------------------
void CLTAGGrenade::StateExit(EState state)
{
	switch (state)
	{
	case EStickyState_UNSAFE:
		/*StopSound(m_armedSoundId);
		m_armedSoundId = INVALID_SOUNDID;*/
		break;
	}
}


//------------------------------------------------------------------------
void CLTAGGrenade::ChangeState(EState state)
{
	//CryLog("CLTAGGrenade::ChangeState() from %d to %d\n", m_state, state);
	StateExit(m_state);
	m_state = state;
	StateEnter(m_state);

	if(gEnv->bServer)
	{
		CHANGED_NETWORK_STATE(this, eEA_GameServerStatic);
	}
}

void CLTAGGrenade::OnCollision(const SGameObjectEvent &event)
{
	EventPhysCollision *pCollision = (EventPhysCollision *)event.ptr;

	if (gEnv->bMultiplayer && m_wasLaunchedByLocalPlayer)
	{
		WriteLock lock(m_postStepLock);
		UpdateStoredTracjectory(pCollision->pt);
	}

	float bouncy, friction;
	uint32 pierceabilityMat;
	gEnv->pPhysicalWorld->GetSurfaceParameters(pCollision->idmat[1], bouncy, friction, pierceabilityMat);
	pierceabilityMat &= sf_pierceable_mask;
	uint32 pierceabilityProj = GetAmmoParams().pParticleParams ? GetAmmoParams().pParticleParams->iPierceability : 13;
	if (pierceabilityMat > pierceabilityProj)
		return;

	int trgId = 1;
	int srcId = 0;
	IPhysicalEntity *pTarget = pCollision->pEntity[trgId];
	IEntity* pTargetEntity = pTarget ? gEnv->pEntitySystem->GetEntityFromPhysics(pTarget) : 0;
	IActor* pHitActor =
		pTargetEntity ?
		(gEnv->pGameFramework->GetIActorSystem()->GetActor(pTargetEntity->GetId())) :
	0;

	m_normal = pCollision->n;

	const SLTagGrenadeParams::SCommonParams& grenadeParams = GetModeParams();

	switch (m_grenadeType)
	{
	case ELTAGGrenadeType_STICKY:
		PlaySound(grenadeParams.m_bounceSound);
		m_stickyProjectile.Stick(
			CStickyProjectile::SStickParams(this, pCollision, CStickyProjectile::eO_AlignToSurface));
		ChangeState(EStickyState_SAFE);
		break;

	case ELTAGGrenadeType_RICOCHET:
		{
			const bool bDetonate = ShouldRicochetDetonateOnImpact(grenadeParams, pHitActor, pTargetEntity);

			if(bDetonate)
			{
				ExplodeGrenade(pHitActor);
			}
			else if(m_state == ERicochetState_FLYING)
			{
				if(grenadeParams.m_detonateWithProximity)
				{
					ChangeState(ERicochetState_PROXY);
				}
				else
				{
					ChangeState(ERicochetState_ARMED);
				}
			}
		}
		break;
	}
}

//------------------------------------------------------------------------
void CLTAGGrenade::ExplodeGrenade(IActor* pHitActor /*= 0*/)
{
	if (gEnv->bServer && !HasFailedDetonation())
	{
		const bool ownerToClose = IsOwnerToClose();

		if (ownerToClose && pHitActor && static_cast<CPlayer*> (pHitActor)->CanFall())
			pHitActor->Fall();

		const char* pClassName = 0;
		if(!gEnv->bMultiplayer)
		{
			switch (m_grenadeType)
			{
				case ELTAGGrenadeType_RICOCHET: pClassName = "LTAG_ricochet"; break;
				case ELTAGGrenadeType_STICKY: pClassName = "LTAG_sticky"; break;
			}
		}		

		if (!ownerToClose)
		{
			IEntity* pEntity = GetEntity();
			Vec3 entityPosition = pEntity->GetWorldPos();

			CProjectile::SExplodeDesc explodeDesc(true);
			explodeDesc.overrideEffectClassName = pClassName;
			
			if(pHitActor)
			{
				explodeDesc.impact = true;
				explodeDesc.targetId = pHitActor->GetEntityId();
			}
			
			if (m_stickyProjectile.IsStuck())
			{
				explodeDesc.impact = true;
				explodeDesc.targetId = m_stickyProjectile.GetStuckEntityId();
				explodeDesc.normal = m_normal;
			}

			explodeDesc.pos = entityPosition;

			Explode(explodeDesc);
		}
		else if (!m_pAmmoParams->pExplosion->failedEffectName.empty())
		{
			ProcessFailedDetonation();
		}
	}
}


//------------------------------------------------------------------------
bool CLTAGGrenade::IsOwnerToClose()
{
	const SLTagGrenadeParams::SCommonParams& grenadeParams = GetModeParams();

	if (grenadeParams.m_safeExplosion <= 0.0f)
		return false;

	IEntity* pOwner = gEnv->pEntitySystem->GetEntity(m_ownerId);
	if (!pOwner)
		return false;

	const Vec3& grenadePosition = GetEntity()->GetWorldPos();
	const Vec3& ownerPosition = pOwner->GetWorldPos();
	const float dp2 = (ownerPosition - grenadePosition).len2();
	return (dp2 <= grenadeParams.m_safeExplosion*grenadeParams.m_safeExplosion);
}



int CLTAGGrenade::PlaySound(const string& soundName)
{
	int id = 0; //GetAudioProxy()->PlaySound(
	//	soundName, /*GetEntity()->GetWorldPos()*/ Vec3(0, 0, 0),
	//	FORWARD_DIRECTION, FLAG_SOUND_DEFAULT_3D, 0,
	//	eSoundSemantic_Projectile,
	//	0, 0);
	return id;
}


//------------------------------------------------------------------------
void CLTAGGrenade::StopSound(int id)
{
	/*if (id == INVALID_SOUNDID)
		return;
	GetAudioProxy()->StopSound(id);*/
}



void CLTAGGrenade::ChangeTexture(const string& textureName)
{
	if (textureName.empty())
		return;

	IEntity* pEntity = GetEntity();
	IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(textureName.c_str());
	if (pMaterial)
		pEntity->SetSlotMaterial(0, pMaterial);
}



void CLTAGGrenade::ChangeTrail(const string& trailString)
{
	m_projectileEffects.DetachEffect(m_trailEffectId);

	EntityEffects::SEffectAttachParams attachParams(ZERO, FORWARD_DIRECTION, 1.0f, true, 1);
	m_trailEffectId = m_projectileEffects.AttachParticleEffect(GetCachedEffect(trailString.c_str()), attachParams);
}

//------------------------------------------------------------------------
bool CLTAGGrenade::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags)
{
	//In multiplayer, the LTAG grenades are not sticky.
	//m_stickyProjectile.NetSerialize(this, ser, aspect);

	if(aspect == eEA_GameServerStatic)
	{
		ser.EnumValue("state", static_cast<CLTAGGrenade*>(this), &CLTAGGrenade::NetGetState, &CLTAGGrenade::NetSetState, EState_NONE, ERicochetState_LAST);

		m_netState = 0;
	}

	if (aspect==eEA_Physics && profile==ePT_Particle)
	{
		// Do the serialisation and net lerping manually
		// Need to writelock since m_netDesiredPos is also
		// accessed from the physics tick
		WriteLock lock(m_postStepLock);
		ser.Value("pos", m_netDesiredPos, 'wrld');
		ser.Value("vel", m_netDesiredVel, 'pLVt');

		// Dont let the generic projectile code do any physics serialisation
		profile = ePT_None;
		m_netState = (ser.IsReading()) ? k_netStateReading : k_netStateWriting;
		SetProjectileFlags(ePFlag_dontNetSerialisePhysics, true);
	}
	else
	{
		m_netState = k_netStateInvalid;
		SetProjectileFlags(ePFlag_dontNetSerialisePhysics, false);
	}
	return inherited::NetSerialize(ser, aspect, profile, pflags);
}

NetworkAspectType CLTAGGrenade::GetNetSerializeAspects()
{
	return inherited::GetNetSerializeAspects() | eEA_GameServerStatic | eEA_Physics;
}

//------------------------------------------------------------------------
void CLTAGGrenade::NetSetState(EState inState)
{
	ChangeState(static_cast<EState>(inState));		// TODO remove if not needed. Presume we need to cast away the enum to use this
}


void CLTAGGrenade::ReInitFromPool()
{
	CProjectile::ReInitFromPool();

	m_grenadeType = ELTAGGrenadeType_RICOCHET;
	m_normal.zero();
	m_state = EState_NONE;
	//m_armedSoundId = INVALID_SOUNDID;
	m_stickyProjectile.Reset();
}

const SLTagGrenadeParams::SCommonParams& CLTAGGrenade::GetModeParams() const
{
	assert(m_pAmmoParams->pLTagParams);

	switch(m_grenadeType)
	{
	case ELTAGGrenadeType_STICKY:
		return m_pAmmoParams->pLTagParams->m_sticky;

	case ELTAGGrenadeType_RICOCHET:
		return m_pAmmoParams->pLTagParams->m_ricochet;
	}

	assert(0);

	return m_pAmmoParams->pLTagParams->m_ricochet;
}

bool CLTAGGrenade::ShouldCollisionsDamageTarget() const
{
	return true;
}

