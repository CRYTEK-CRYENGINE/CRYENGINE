// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Damage Effect Controller

-------------------------------------------------------------------------
History:
- 02:09:2009   15:00 : Created by Claire Allan

***********************************************************************/

#include "StdAfx.h"
#include "ActorDamageEffectController.h"
#include "Game.h"
#include "GameRules.h"
#include <CryParticleSystem/ParticleParams.h>
#include "Actor.h"
#include "Player.h"
#include "Weapon.h"
#include "Audio/AudioSignalPlayer.h"
#include "UI/HUD/HUDEventDispatcher.h"

uint32 CKVoltEffect::s_hashId = 0;
uint32 CTinnitusEffect::s_hashId = 0;
uint32 CEntityTimerEffect::s_hashId = 0;


CDamageEffectController::CDamageEffectController()
: m_ownerActor(NULL)
, m_activeEffectsBitfield(0)
, m_effectsResetSwitchBitfield(0)
, m_effectsKillBitfield(0)
, m_allowSerialise(false)
{
	for(int i = 0; i < MAX_NUM_DAMAGE_EFFECTS; i++)
	{
		m_effectList[i] = NULL;
		m_associatedHitType[i] = -1;
		m_minDamage[i] = -1.0f;
	}
}

CDamageEffectController::~CDamageEffectController()
{
	for(int i = 0; i < MAX_NUM_DAMAGE_EFFECTS; i++)
	{
		delete m_effectList[i];
	}
}

uint32 CDamageEffectController::CreateHash(const char* string)
{
	return CCrc32::ComputeLowercase(string);
}

void CDamageEffectController::Init(CActor* actor)
{
	if (CKVoltEffect::s_hashId == 0) //once initialised it can't be 0
	{
		CKVoltEffect::s_hashId = CreateHash("KVoltFX");
		CTinnitusEffect::s_hashId = CreateHash("TinnitusFx");
		CEntityTimerEffect::s_hashId = CreateHash("TimerFX");
	}

	IGameRules* pGameRules = g_pGame->GetGameRules();
	m_ownerActor = actor;

	const IItemParamsNode* actorParams = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorParams(actor->GetEntityClassName());
	const IItemParamsNode* damageEffectParams = actorParams ? actorParams->GetChild("DamageEffectParams") : 0;

	if (damageEffectParams)
	{
		int numChildren = damageEffectParams->GetChildCount();
		int allowSerialise = 1;

		damageEffectParams->GetAttribute("allowSerialise", allowSerialise);
		m_allowSerialise = allowSerialise ? true : false;

		CRY_ASSERT_MESSAGE(numChildren <= MAX_NUM_DAMAGE_EFFECTS, "Too many damage effects found. Increase the MAX_NUM_DAMAGE_EFFECTS and size of activeEffects and effectsResetSwitch");

		for (int i = 0; i < numChildren; i++)
		{
			const IItemParamsNode* child = damageEffectParams->GetChild(i);
			const IItemParamsNode* effect = child->GetChild(0);

			const char* hittype = child->GetAttribute("hittype");
			const char* name = effect->GetName();

			m_associatedHitType[i] = pGameRules->GetHitTypeId(hittype);

			child->GetAttribute("minDamage", m_minDamage[i]);	
			
			uint64 hashcode = CreateHash(name);

			if (hashcode == CKVoltEffect::s_hashId)
			{
				m_effectList[i] = new CKVoltEffect();
			}
			else if (hashcode == CTinnitusEffect::s_hashId)
			{
				m_effectList[i] = new CTinnitusEffect();
			}
			else if (hashcode == CEntityTimerEffect::s_hashId)
			{
				m_effectList[i] = new CEntityTimerEffect();
			}
			else
			{
				GameWarning("INVALID DAMAGE EFFECT PARSED");
				m_associatedHitType[i] = -1;
			}
			
			if(m_effectList[i])
			{
				m_effectList[i]->Init(actor, effect);
			}
		}
	}
}

void CDamageEffectController::OnHit(const HitInfo *hitInfo) 
{
	uint8 bitCheck = 1;

	bool netSync = false;

	for(int i = 0; i < MAX_NUM_DAMAGE_EFFECTS; i++)
	{
		if (m_associatedHitType[i] == hitInfo->type && m_minDamage[i] < hitInfo->damage)
		{
			netSync = true;
			CRY_ASSERT_MESSAGE(m_effectList[i], "The effect should not be null if a hit type is associated with it");
			if (m_activeEffectsBitfield & bitCheck)
			{
				m_effectList[i]->Reset();
				m_effectsResetSwitchBitfield = m_effectsResetSwitchBitfield & bitCheck 
											? m_effectsResetSwitchBitfield & ~bitCheck 
											: m_effectsResetSwitchBitfield | bitCheck;
			}
			else
			{
				m_effectList[i]->Enter();

				m_effectsResetSwitchBitfield &= ~bitCheck;
				m_activeEffectsBitfield |= bitCheck;
			}			
		}
		bitCheck = bitCheck << 1;
	}

	if (netSync)
	{
		CHANGED_NETWORK_STATE(m_ownerActor, eEA_GameServerDynamic);
	}
}

void CDamageEffectController::OnKill(const HitInfo* hitInfo) 
{
	uint8 bitCheck = 1;

	bool netSync = false;

	for(int i = 0; i < MAX_NUM_DAMAGE_EFFECTS; i++)
	{
		if (m_associatedHitType[i] == hitInfo->type)
		{
			netSync = true;
			CRY_ASSERT_MESSAGE(m_effectList[i], "The effect should not be null if a hit type is associated with it");
			if (!(m_activeEffectsBitfield & bitCheck))
			{
				m_effectList[i]->Enter();
				m_effectsResetSwitchBitfield &= ~bitCheck;
				m_activeEffectsBitfield |= bitCheck;
			}
			
			m_effectList[i]->OnKill();
			m_effectsKillBitfield |= bitCheck;		
		}
		bitCheck = bitCheck << 1;
	}

	if (netSync)
	{
		CHANGED_NETWORK_STATE(m_ownerActor, eEA_GameServerDynamic);
	}
}

void CDamageEffectController::OnRevive() 
{
	uint8 bitCheck = 1;

	bool netSync = false;

	for(int i = 0; i < MAX_NUM_DAMAGE_EFFECTS; i++)
	{
		if (m_activeEffectsBitfield & bitCheck)
		{
			netSync = true;
			CRY_ASSERT_MESSAGE(m_effectList[i], "The effect should not be null if it is active");
			
			m_effectList[i]->Leave();
		}
		bitCheck = bitCheck << 1;
	}

	m_activeEffectsBitfield = 0;
	m_effectsKillBitfield = 0;

	if (netSync)
	{
		CHANGED_NETWORK_STATE(m_ownerActor, eEA_GameServerDynamic);
	}
}

void CDamageEffectController::UpdateEffects(float frameTime) 
{
	uint8 bitCheck = 1;

	bool netSync = false;

	for(int i = 0; i < MAX_NUM_DAMAGE_EFFECTS; i++)
	{
		if (m_activeEffectsBitfield & bitCheck)
		{
			CRY_ASSERT_MESSAGE(m_effectList[i], "The effect should not be null if it is active");

			if (!m_effectList[i]->Update(frameTime) && gEnv->bServer)
			{
				netSync = true;
				m_effectList[i]->Leave();
				m_activeEffectsBitfield &= ~bitCheck;
			}
		}
		bitCheck = bitCheck << 1;
	}

	if (netSync)
	{
		CHANGED_NETWORK_STATE(m_ownerActor, eEA_GameServerDynamic);
	}
}

void CDamageEffectController::NetSerialiseEffects(TSerialize ser, EEntityAspects aspect)
{
	if(m_allowSerialise && aspect == eEA_GameServerDynamic)
	{
		NET_PROFILE_SCOPE("DamageEffects", ser.IsReading());

		ser.Value("activeEffects", m_ownerActor, &CActor::GetActiveDamageEffects, &CActor::SetActiveDamageEffects, 'ui8');
		ser.Value("effectReset", m_ownerActor, &CActor::GetDamageEffectsResetSwitch, &CActor::SetDamageEffectsResetSwitch, 'ui8');
		ser.Value("effectKilled", m_ownerActor, &CActor::GetDamageEffectsKilled, &CActor::SetDamageEffectsKilled, 'ui8');
	}
}

void CDamageEffectController::SetActiveEffects(uint8 active)
{
	uint8 bitCheck = 1;

	for(int i = 0; i < MAX_NUM_DAMAGE_EFFECTS; i++)
	{
		bool serverActive = !((active & bitCheck) == 0);
		bool locallyActive = !((m_activeEffectsBitfield & bitCheck) == 0);
		if ( serverActive != locallyActive )
		{
			CRY_ASSERT_MESSAGE(m_effectList[i], "The effect should not be null");
			if (serverActive)
			{
				m_effectList[i]->Enter();

				m_effectsResetSwitchBitfield &= ~bitCheck;
				m_activeEffectsBitfield |= bitCheck;
			}
			else
			{
				m_effectList[i]->Leave();
				m_activeEffectsBitfield &= ~bitCheck;
			}
		}
		bitCheck = bitCheck << 1;
	}
}

void CDamageEffectController::SetEffectResetSwitch(uint8 reset)
{
	uint8 bitCheck = 1;

	for(int i = 0; i < MAX_NUM_DAMAGE_EFFECTS; i++)
	{
		if ((reset & bitCheck) != (m_effectsResetSwitchBitfield & bitCheck))
		{
			CRY_ASSERT_MESSAGE((m_activeEffectsBitfield & bitCheck), "Might not leave the effect and shouldn't happen");
			CRY_ASSERT_MESSAGE(m_effectList[i], "The effect should not be null");
			m_effectList[i]->Reset();
		}
		bitCheck = bitCheck << 1;
	}

	m_effectsResetSwitchBitfield = reset;
}

void CDamageEffectController::SetEffectsKilled(uint8 killed)
{
	unsigned int bitCheck = 1;

	for(int i = 0; i < MAX_NUM_DAMAGE_EFFECTS; i++)
	{
		if ((killed & bitCheck) && !(m_effectsKillBitfield & bitCheck))
		{
			CRY_ASSERT_MESSAGE(m_effectList[i], "The effect should not be null");
			m_effectList[i]->OnKill();
		}
		bitCheck = bitCheck << 1;
	}

	m_effectsKillBitfield = killed;
}

void CKVoltEffect::Init(CActor* actor, const IItemParamsNode* params)
{
	inherited::Init(actor, params);

	params->GetAttribute("timer", m_effectTime);
	params->GetAttribute("disabledCrosshairTime", m_disabledCrosshairTime);
	const char* effect = params->GetAttribute("effect");
	const char* screenEffect = params->GetAttribute("screenEffect");

	m_particleEffect = gEnv->pParticleManager->FindEffect(effect);
	m_screenEffect = gEnv->pParticleManager->FindEffect(screenEffect);

	if(m_particleEffect)
	{
		m_particleEffect->AddRef();
	}
	else
	{
		GameWarning("Invalid Particle Effect for KVolt Effect");
	}

	if(m_screenEffect)
	{
		m_screenEffect->AddRef();
	}
	else
	{
		GameWarning("Invalid Screen Effect for KVolt Effect");
	}

	m_particleEmitter = NULL;
}

void CKVoltEffect::Enter()
{
	m_timer = m_effectTime;

	SHUDEvent eventTempAddToRadar(eHUDEvent_TemporarilyTrackEntity);
	eventTempAddToRadar.AddData(SHUDEventData((int)m_ownerActor->GetEntityId()));
	eventTempAddToRadar.AddData(SHUDEventData(m_effectTime));
	eventTempAddToRadar.AddData(true);
	eventTempAddToRadar.AddData(false);
	CHUDEventDispatcher::CallEvent(eventTempAddToRadar);
	
	if(m_particleEffect)
	{
		IEntity* entity = m_ownerActor->GetEntity();

		const ParticleParams& particleParams = m_particleEffect->GetParticleParams();

		SpawnParams	spawnParams;
		spawnParams.eAttachType = particleParams.eAttachType;
		spawnParams.eAttachForm = particleParams.eAttachForm;

		m_particleEmitter = m_particleEffect->Spawn(entity->GetWorldTM());
		if(m_particleEmitter)
		{
			m_particleEmitter->AddRef();

			// Set spawn params so that the emitter spawns particles all over the character's physics mesh
			GeomRef geomRef;
			geomRef.Set(entity, 0);
			if(geomRef)
			{
				m_particleEmitter->SetSpawnParams(spawnParams, geomRef);
			}
		}
	}
	ResetScreenEffect();
}

void CKVoltEffect::ResetScreenEffect()
{
	if(m_screenEffect && m_ownerActor->IsClient())
	{
		IAttachment* screenAttachment = GetScreenAttachment();
		
		if(screenAttachment)
		{
			CEffectAttachment* pEffectAttachment = new CEffectAttachment(m_screenEffect->GetName(), Vec3(0,0,0), Vec3(0,1,0), 1.f);
			if (pEffectAttachment->GetEmitter())
			{
				screenAttachment->AddBinding(pEffectAttachment);
				pEffectAttachment->ProcessAttachment(screenAttachment);
			}
			else
			{
				delete pEffectAttachment;
			}
		}
		else
		{
			GameWarning("Failed to find #camera attachment");
		}

		//FadeCrosshair();
		DisableScopeReticule();
	}
}

#if 0
void CKVoltEffect::FadeCrosshair()
{
	//Note: this does not currently work. If we want it to work, the HUDEvent_FadeCrosshair handling needs
	//			to be changed. Speak to Rich S
	const float fadeTime = 0.1f;

	SHUDEvent hudEvent(eHUDEvent_FadeCrosshair);
	hudEvent.AddData(0.0f);
	hudEvent.AddData(fadeTime);
	hudEvent.AddData(0.0f);
	CHUDEventDispatcher::CallEvent(hudEvent);

	hudEvent.ClearData();
	hudEvent.AddData(1.0f);
	hudEvent.AddData(fadeTime);
	hudEvent.AddData(m_disabledCrosshairTime);
	CHUDEventDispatcher::CallEvent(hudEvent);
}
#endif

void CKVoltEffect::DisableScopeReticule()
{
	CWeapon* pWeapon = m_ownerActor->GetWeapon(m_ownerActor->GetCurrentItemId());
	if (pWeapon)
		pWeapon->GetScopeReticule().Disable(m_disabledCrosshairTime);
}

IAttachment* CKVoltEffect::GetScreenAttachment()
{
	IEntity* pEntity = m_ownerActor ? m_ownerActor->GetEntity() : NULL;
	ICharacterInstance* pCharacter = pEntity ? pEntity->GetCharacter(0) : NULL;
	IAttachmentManager* pAttachmentMan = pCharacter ? pCharacter->GetIAttachmentManager() : NULL;

	if(pAttachmentMan)
	{
		return pAttachmentMan->GetInterfaceByName("#camera");
	}

	return NULL;
}

void CKVoltEffect::Reset()
{ 
	m_timer = m_effectTime; 
	ResetScreenEffect();
}

void CKVoltEffect::Leave()
{
	if(m_particleEmitter)
	{
		gEnv->pParticleManager->DeleteEmitter(m_particleEmitter);
		m_particleEmitter->Release();
		m_particleEmitter = NULL;
	}

	IAttachment* screenAttachment = GetScreenAttachment();
	if(screenAttachment)
	{
		screenAttachment->ClearBinding();
	}
}

bool CKVoltEffect::Update(float frameTime)
{
	if(m_timer > frameTime)
	{
		m_timer -= frameTime;
		return true;
	}

	m_timer = 0.f;
	return false;
}

void CTinnitusEffect::Init(CActor* actor, const IItemParamsNode* params)
{
	inherited::Init(actor, params);

	params->GetAttribute("timer", m_tinnitusTime);
	m_timer = 0;
}

void CTinnitusEffect::Enter()
{
	m_timer = m_tinnitusTime;

	if(m_ownerActor == g_pGame->GetIGameFramework()->GetClientActor())
	{
		static_cast<CPlayer*>(m_ownerActor)->StartTinnitus();	
	}
}

void CTinnitusEffect::Reset()
{ 
	Enter();
}

void CTinnitusEffect::Leave()
{
	if(m_ownerActor == g_pGame->GetIGameFramework()->GetClientActor())
	{
		static_cast<CPlayer*>(m_ownerActor)->StopTinnitus();
	}
}

bool CTinnitusEffect::Update(float frameTime)
{
	if(m_timer > frameTime)
	{
		m_timer -= frameTime;
		
	if(m_ownerActor == g_pGame->GetIGameFramework()->GetClientActor())
	{
		static_cast<CPlayer*>(m_ownerActor)->UpdateTinnitus(m_timer/m_tinnitusTime);
	}

		return true;
	}

	m_timer = 0.f;
	return false;
}

void CEntityTimerEffect::Init(CActor* actor, const IItemParamsNode* params)
{
	inherited::Init(actor, params);

	params->GetAttribute("timerID", m_entityTimerID);
	params->GetAttribute("time", m_initialTime);
	m_timer = 0.f;
}

void CEntityTimerEffect::Enter()
{
	m_timer = m_initialTime;

	if(gEnv->bServer)
	{
		m_ownerActor->GetEntity()->SetTimer(m_entityTimerID, (int)(m_initialTime*1000.f));
	}
}

bool CEntityTimerEffect::Update(float frameTime)
{
	if(m_timer > frameTime)
	{
		m_timer -= frameTime;

		return true;
	}

	m_timer = 0.f;
	return false;
}
