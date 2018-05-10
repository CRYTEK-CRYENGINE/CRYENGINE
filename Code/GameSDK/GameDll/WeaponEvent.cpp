// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 9:12:2005   10:50 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "Weapon.h"
#include "Player.h"
#include "GameRules.h"
#include "GameCVars.h"
#include <IActorSystem.h>
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAgent.h>
#include "Audio/GameAudio.h"

#include "ItemSharedParams.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "WeaponSharedParams.h"
#include "Battlechatter.h"
#include "AmmoParams.h"
#include "WeaponSystem.h"
#include "FireMode.h"
#include "PersistantStats.h"
#include "Single.h"
#include "EntityUtility/EntityEffects.h"


#define BROADCAST_WEAPON_EVENT(event, params)	\
	for (TWeaponEventListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next()) \
		notifier->event params;

//----------------------------------------------
struct CWeapon::AnimationEventFireAutoStop
{
	AnimationEventFireAutoStop(CWeapon& ownerWeapon) : m_ownerWeapon(ownerWeapon) {}

	void execute(CItem *_this)
	{
		m_ownerWeapon.OnAnimationEventStopFire();
	}

	void destroy()
	{
		execute(&m_ownerWeapon);
	}

	CWeapon& m_ownerWeapon;
};

//----------------------------------------------
struct CWeapon::RefillBeltAction
{
	RefillBeltAction(CWeapon *_weapon): weapon(_weapon){};
	CWeapon *weapon;

	void execute(CItem *_this)
	{
		weapon->RefillBelt();
	}
};

//------------------------------------------------------------------------
CWeapon::CAnimationFiringLocator::CAnimationFiringLocator() : m_pOwnerWeapon(NULL), m_pPreviousFiringLocator(NULL)
{
}

//------------------------------------------------------------------------
bool CWeapon::CAnimationFiringLocator::Init(CWeapon* pOwnerWeapon)
{
	CRY_ASSERT(pOwnerWeapon);

	m_pOwnerWeapon = pOwnerWeapon;

	return (m_pOwnerWeapon != NULL);
}

//------------------------------------------------------------------------
void CWeapon::CAnimationFiringLocator::Set()
{
	CRY_ASSERT(!IsSet());
	CRY_ASSERT(m_pOwnerWeapon);

	IWeaponFiringLocator* pPreviousFiringLocator = m_pOwnerWeapon->GetFiringLocator();
	if (pPreviousFiringLocator != this)
	{
		m_pPreviousFiringLocator = pPreviousFiringLocator;
		m_pOwnerWeapon->SetFiringLocator(this);
	}
}

//------------------------------------------------------------------------
void CWeapon::CAnimationFiringLocator::SetOtherFiringLocator(IWeaponFiringLocator* pFiringLocator)
{
	CRY_ASSERT(IsSet());
	CRY_ASSERT(pFiringLocator != this);

	if (m_pPreviousFiringLocator && (pFiringLocator != this))
	{
		m_pPreviousFiringLocator->WeaponReleased();
		m_pPreviousFiringLocator = pFiringLocator;
	}
}

//------------------------------------------------------------------------
void CWeapon::CAnimationFiringLocator::Release()
{
	CRY_ASSERT(IsSet());
	CRY_ASSERT(m_pOwnerWeapon);

	if (IsSet())
	{
		// Otherwise SetFiringLocator will call SetOtherFiringLocator
		WeaponReleased();
		m_pOwnerWeapon->m_pFiringLocator = NULL;

		// Reestablish the previous firing locator
		m_pOwnerWeapon->SetFiringLocator(m_pPreviousFiringLocator);
		m_pPreviousFiringLocator = NULL;
	}
}

//------------------------------------------------------------------------
bool CWeapon::CAnimationFiringLocator::GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	bool helper = pFireMode->HasFireHelper();
	if (helper)
	{
		dir = pFireMode->GetFireHelperDir();
	}
	else
	{
		IEntity* pWeaponEntity = gEnv->pEntitySystem->GetEntity(weaponId);
		if (pWeaponEntity)
		{
			dir = pWeaponEntity->GetSlotWorldTM(eIGS_ThirdPerson).GetColumn1();
		}
		else
			return false;
	}

	return true;
}

//------------------------------------------------------------------------
bool CWeapon::CAnimationFiringLocator::GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit)
{
	CRY_ASSERT(m_pOwnerWeapon);

	// [*DavidR | 14/Oct/2010] Note: Basically a Copy & Paste of CSingle::GetProbableHit but without getting info
	// from the movement controller
	static Vec3 pos(ZERO), dir(FORWARD_DIRECTION);

	CActor *pActor = m_pOwnerWeapon->GetOwnerActor();
	IEntity *pWeaponEntity = m_pOwnerWeapon->GetEntity();

	static PhysSkipList skipList;
	skipList.clear();
	CSingle::GetSkipEntities(m_pOwnerWeapon, skipList);

	float rayLength = 250.0f;
	IEntityClass* pAmmoClass = pFireMode->GetAmmoType();
	const SAmmoParams *pAmmoParams = pAmmoClass ? g_pGame->GetWeaponSystem()->GetAmmoParams(pAmmoClass) : NULL;
	if (pAmmoParams)
	{
		//Benito - It could happen that the lifetime/speed are zero, so ensure at least some minimum distance check
		rayLength = clamp_tpl(min(pAmmoParams->speed * pAmmoParams->lifetime, rayLength), 5.0f, rayLength);
	}

	// Change this so it calls this firing locator's GetFiringPos if it gets implemented (no need atm)
	pos = pFireMode->GetFiringPos(Vec3Constants<float>::fVec3_Zero);
	IF_UNLIKELY(!GetFiringDir(weaponId, pFireMode, dir, Vec3Constants<float>::fVec3_Zero, Vec3Constants<float>::fVec3_Zero))
		dir = pFireMode->GetFiringDir(Vec3Constants<float>::fVec3_Zero, pos);
	dir *= rayLength;

	static ray_hit rayHit;	

	// use the ammo's pierceability
	uint32 flags=(geom_colltype_ray|geom_colltype13)<<rwi_colltype_bit|rwi_colltype_any|rwi_force_pierceable_noncoll|rwi_ignore_solid_back_faces;
	uint8 pierceability = 8;
	if (pAmmoParams && pAmmoParams->pParticleParams && !is_unused(pAmmoParams->pParticleParams->iPierceability))
	{
		pierceability=pAmmoParams->pParticleParams->iPierceability;
	}
	flags |= pierceability;

	if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, dir, ent_all, flags, &rayHit, 1, !skipList.empty() ? &skipList[0] : NULL, skipList.size()))
	{
		hit = rayHit.pt;
	}
	else
	{
		hit = pos + dir;
	}

	return true;
}

//------------------------------------------------------------------------
void CWeapon::OnShoot(EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3&vel)
{
	BROADCAST_WEAPON_EVENT(OnShoot, (this, shooterId, ammoId, pAmmoType, pos, dir, vel));

	//FIXME:quick temporary solution
	IActor *pClientActor = m_pGameFramework->GetClientActor();

	CActor *pActor = static_cast<CActor*> (g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(shooterId));

	if (pActor)
	{
		pActor->HandleEvent(SGameObjectEvent(eCGE_OnShoot,eGOEF_ToExtensions));

		if(!gEnv->bMultiplayer && (pActor == pClientActor))
		{
			if (IAIObject *pAIObject = pActor->GetEntity()->GetAI())
			{
				gEnv->pAISystem->SendSignal(SIGNALFILTER_LEADER, 1, "OnEnableFire",	pAIObject, 0);
			}

			gEnv->pAISystem->DisableGlobalPerceptionScaling();
		}
	}

	const bool sendHUDEvent = (shooterId != 0) && ShouldSendOnShootHUDEvent();
	if (sendHUDEvent)
	{
		SHUDEvent event(eHUDEvent_OnShoot);
		event.AddData(SHUDEventData((int)shooterId));
		CHUDEventDispatcher::CallEvent(event);
	}
}

void CWeapon::EndBurst()
{
	BROADCAST_WEAPON_EVENT(OnEndBurst, (this, GetOwnerId()));
}

//------------------------------------------------------------------------
void CWeapon::OnStartFire(EntityId shooterId)
{
	BROADCAST_WEAPON_EVENT(OnStartFire, (this, shooterId));
}

//------------------------------------------------------------------------
void CWeapon::OnStopFire(EntityId shooterId)
{
	BROADCAST_WEAPON_EVENT(OnStopFire, (this, shooterId));
}


//------------------------------------------------------------------------
void CWeapon::OnFireModeChanged(int currentFireMode)
{
	// Update weapon stats since depends on firemode
	if (m_pWeaponStats)
	{
		bool hasFireModeStats = false;
		if (m_fm)
		{
			CFireMode* pFireMode = (CFireMode*)m_fm;
			if (pFireMode != NULL)
			{
				const SFireModeParams* pFireModeParams = pFireMode->GetShared();
				if (pFireModeParams != NULL)
				{
					hasFireModeStats = true;
					m_pWeaponStats->SetStatsFromFiremode(&pFireModeParams->weaponStatChangesParams);
				}
			}
		}

		if (!hasFireModeStats)
		{
			m_pWeaponStats->SetStatsFromFiremode(NULL);
		}
	}

	BROADCAST_WEAPON_EVENT(OnFireModeChanged, (this, currentFireMode));
}


//------------------------------------------------------------------------
void CWeapon::OnStartReload(EntityId shooterId, IEntityClass* pAmmoType)
{
	BROADCAST_WEAPON_EVENT(OnStartReload, (this, shooterId, pAmmoType));

	if (CActor *pActor = GetOwnerActor())
	{
		if (IAIObject *pAIObject=pActor->GetEntity()->GetAI())
			if (gEnv->pAISystem)
				gEnv->pAISystem->SendSignal( SIGNALFILTER_SENDER, 1, "OnReload", pAIObject);

		if(pActor->IsClient())
		{
			if (gEnv->bMultiplayer && pActor->IsCloaked())
			{
				CPersistantStats::GetInstance()->IncrementClientStats(EIPS_CloakedReloads);
			}

			if(m_weaponsharedparams->bulletBeltParams.numBullets > 0)
			{
				const uint32 refillTime = (uint32)(GetCurrentAnimationTime(eIGS_Owner) * m_weaponsharedparams->bulletBeltParams.beltRefillReloadFraction);

				GetScheduler()->TimerAction(refillTime, CSchedulerAction<RefillBeltAction>::Create(RefillBeltAction(this)), false);
			}
		}
	}

	IFireMode *pFireMode = GetFireMode(GetCurrentFireMode());
	if (pFireMode)
	{
		if(GetInventoryAmmoCount(pAmmoType) < pFireMode->GetClipSize())
		{
			BATTLECHATTER(BC_LowAmmo, shooterId);
		}
		else
		{
			BATTLECHATTER(BC_Reloading, shooterId);
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::OnEndReload(EntityId shooterId, IEntityClass* pAmmoType)
{
	BROADCAST_WEAPON_EVENT(OnEndReload, (this, shooterId, pAmmoType));

	if (CActor *pActor = GetOwnerActor())
	{
		if (IAIObject *pAIObject=pActor->GetEntity()->GetAI())
			if (gEnv->pAISystem)
				gEnv->pAISystem->SendSignal( SIGNALFILTER_SENDER, 1, "OnReloadDone", pAIObject);
	}

	if (m_doingMagazineSwap)
	{
		const char* magazineAttachment = m_weaponsharedparams->reloadMagazineParams.magazineAttachment.c_str();
		HideCharacterAttachment(eIGS_FirstPerson, magazineAttachment, false);
		HideCharacterAttachment(eIGS_ThirdPerson, magazineAttachment, false);
		m_doingMagazineSwap = false;
	}
}

void CWeapon::OnSetAmmoCount(EntityId shooterId)
{
	BROADCAST_WEAPON_EVENT(OnSetAmmoCount, (this, shooterId));
}

//------------------------------------------------------------------------
void CWeapon::OnOutOfAmmo(IEntityClass* pAmmoType)
{
	BROADCAST_WEAPON_EVENT(OnOutOfAmmo, (this, pAmmoType));

/*	- no need to send signal here - puppet will check ammo when fires
	if (CActor *pActor = GetOwnerActor())
	{
		if (IAIObject *pAIObject=Actor->GetEntity()->GetAI())
			gEnv->pAISystem->SendSignal( SIGNALFILTER_SENDER, 1, "OnOutOfAmmo", pAIObject);
	}
*/
}

//------------------------------------------------------------------------
void CWeapon::OnReadyToFire()
{
	BROADCAST_WEAPON_EVENT(OnReadyToFire, (this));
}

//------------------------------------------------------------------------
void CWeapon::OnPickedUp(EntityId actorId, bool destroyed)
{
	BROADCAST_WEAPON_EVENT(OnPickedUp, (this, actorId, destroyed));

	BaseClass::OnPickedUp(actorId, destroyed);

	GetEntity()->SetFlags(GetEntity()->GetFlags() | ENTITY_FLAG_NO_PROXIMITY);	

	// bonus ammo is always put in the actor's inv
	if (!m_bonusammo.empty())
	{
		for (TAmmoVector::iterator it = m_bonusammo.begin(); it != m_bonusammo.end(); ++it)
		{
			const SWeaponAmmo& currentBonusAmmo = *it;

			SetInventoryAmmoCount(currentBonusAmmo.pAmmoClass, GetInventoryAmmoCount(currentBonusAmmo.pAmmoClass)+currentBonusAmmo.count);
		}

		m_bonusammo.clear();
	}

	if(GetISystem()->IsSerializingFile() == 1)
		return;

	CActor *pActor = GetActor(actorId);
	if (!pActor)
		return;
	
	// current ammo is only added to actor's inv, if we already have this weapon
	if (destroyed && m_sharedparams->params.unique)
	{
		for (TAmmoVector::iterator it = m_ammo.begin(); it!=m_ammo.end(); ++it)
		{
			//Only add ammo to inventory, if not accessory ammo (accessories give ammo already)
			const SWeaponAmmo& currentAmmo = *it;
			const SWeaponAmmo* pAccessoryAmmo = SWeaponAmmoUtils::FindAmmo(m_weaponsharedparams->ammoParams.accessoryAmmo, currentAmmo.pAmmoClass);
			if(pAccessoryAmmo != NULL)
			{
				SetInventoryAmmoCount(currentAmmo.pAmmoClass, GetInventoryAmmoCount(currentAmmo.pAmmoClass)+currentAmmo.count);
			}
		}
	}

	TestClipAmmoCountIsValid();

	if (!gEnv->bServer && pActor->IsPlayer())
	{
		IEntityClass* pCurrentAmmoClass = m_fm ? m_fm->GetAmmoType() : NULL;
		if (pCurrentAmmoClass)
		{
			//server has serialised the inventory count already

			if(IInventory* pInventory = GetActorInventory(GetOwnerActor()))
			{
				if(m_lastRecvInventoryAmmo > pInventory->GetAmmoCapacity(pCurrentAmmoClass))
				{
					pInventory->SetAmmoCapacity(pCurrentAmmoClass, m_lastRecvInventoryAmmo);
				}

				SetInventoryAmmoCountInternal(pInventory, pCurrentAmmoClass, m_lastRecvInventoryAmmo);
			}
		}
	}

	if(gEnv->bMultiplayer)
	{
		HighlightWeapon(false);
	}

	m_expended_ammo = 0;
}

void CWeapon::TestClipAmmoCountIsValid()
{
	TFireModeVector::iterator itEnd = m_firemodes.end();
	for (TFireModeVector::iterator it = m_firemodes.begin(); it != itEnd; ++it)
	{
		IFireMode* pFM = *it;
		if (pFM && pFM->IsEnabled())
		{
			IEntityClass* pAmmoType = pFM->GetAmmoType();
			if(pAmmoType)
			{
				int clipSize = pFM->GetClipSize();
				const SFireModeParams* pFireModeParams = ((CFireMode*)pFM)->GetShared();
				if (pFireModeParams)
				{
					clipSize += pFireModeParams->fireparams.bullet_chamber;
				}
				const int ammoCount = GetAmmoCount(pAmmoType);
				if (ammoCount > clipSize)
				{
					SetAmmoCount(pAmmoType, clipSize);
					const int excessAmmo = ammoCount - clipSize;
					SetInventoryAmmoCount(pAmmoType, GetInventoryAmmoCount(pAmmoType) + excessAmmo);
				}
			}
		}
	}
}

static bool IsEntityOfClass(IEntity* pEntity, const char* name)
{
	if (pEntity)
	{
		IEntityClass* pClass = pEntity->GetClass();
		if (pClass != NULL)
		{
			const char* className = pEntity->GetClass()->GetName();
			if (className != NULL)
			{
				if (strcmp(className, name) == 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}

//------------------------------------------------------------------------
void CWeapon::OnDropped(EntityId actorId, bool ownerWasAI)
{
	BROADCAST_WEAPON_EVENT(OnDropped, (this, actorId));

	BaseClass::OnDropped(actorId, ownerWasAI);

	IEntity * pEntity = GetEntity();

	if(gEnv->bServer)
	{
		bool removeWeapon = true;

		if(gEnv->bMultiplayer && GetParams().check_clip_size_after_drop)
		{
			TFireModeVector::const_iterator firemodesEndIt = m_firemodes.end();
			for (TFireModeVector::const_iterator firemodeCit = m_firemodes.begin(); firemodeCit != firemodesEndIt && removeWeapon; ++firemodeCit)
			{
				const CFireMode* pFiremode = *firemodeCit;
				if (pFiremode)
				{
					IEntityClass* pFiremodeAmmo = pFiremode->GetAmmoType();
					if (pFiremodeAmmo)
					{
						//only check the main ammo type given with the weapon
						if(SWeaponAmmoUtils::FindAmmoConst(m_weaponsharedparams->ammoParams.ammo, pFiremodeAmmo))
						{
							int currentClipAmount = GetAmmoCount(pFiremodeAmmo);
							int clipSize = pFiremode->GetClipSize();

							if(currentClipAmount > 0 && currentClipAmount >= clipSize)
							{
								removeWeapon = false;
							}
						}
					}
				}
			}
		}
		else
		{
			const bool outOfAmmo = OutOfAmmo(true) && !ownerWasAI;
			const bool removeOnDrop = GetSharedItemParams()->params.remove_on_drop;
			removeWeapon = !gEnv->pSystem->IsSerializingFile() && (outOfAmmo && removeOnDrop);
		}

		if(removeWeapon && GetParams().check_bonus_ammo_after_drop)
		{
			for(unsigned int i = 0; i < m_bonusammo.size(); ++i)
			{
				if(m_bonusammo[i].count > 0)
				{
					removeWeapon = false;
					break;
				}
			}
		}

		if(removeWeapon)
		{
			if(gEnv->IsEditor())
			{
				pEntity->Hide(true);
			}
			else
			{
				gEnv->pEntitySystem->RemoveEntity(pEntity->GetId());
			}		
		}
	}

	uint32 flags = pEntity->GetFlags();
	
	flags &= ~ENTITY_FLAG_NO_PROXIMITY;

	pEntity->SetFlags(flags);

	m_expended_ammo = 0;

	if(gEnv->bMultiplayer && (g_pGameCVars->i_highlight_dropped_weapons == 2) || (IsHeavyWeapon() && g_pGameCVars->i_highlight_dropped_weapons == 1))
	{
		HighlightWeapon(true, true);
	}
}

//------------------------------------------------------------------------
void CWeapon::OnDroppedByAI(IInventory* pAIInventory)
{
	CRY_ASSERT(pAIInventory);

	const TAmmoVector& minDroppedAmmoMap = m_weaponsharedparams->ammoParams.minDroppedAmmo;
	const bool checkForMinAmmoDrops = m_minDropAmmoAvailable && !minDroppedAmmoMap.empty();

	TFireModeVector::const_iterator firemodesEndIt = m_firemodes.end();
	for (TFireModeVector::const_iterator firemodeCit = m_firemodes.begin(); firemodeCit != firemodesEndIt ; ++firemodeCit)
	{
		const CFireMode* pFiremode = *firemodeCit;
		if (pFiremode)
		{
			IEntityClass* pFiremodeAmmo = pFiremode->GetAmmoType();
			if (pFiremodeAmmo)
			{
				// Additional ammo which always goes to the magazine, when ai drops a weapon
				int minAmmoDrop = 0;
				if (checkForMinAmmoDrops)
				{
					const SWeaponAmmo* pMinDroppedAmmo = SWeaponAmmoUtils::FindAmmoConst(minDroppedAmmoMap, pFiremodeAmmo);
					if (pMinDroppedAmmo != NULL)
					{
						minAmmoDrop = pMinDroppedAmmo->count;
						SWeaponAmmo* pCurrentAmmo = SWeaponAmmoUtils::FindAmmo(m_ammo, pFiremodeAmmo);
						if (pCurrentAmmo)
						{
							pCurrentAmmo->count = min(minAmmoDrop + pCurrentAmmo->count, pFiremode->GetClipSize());
						}
						else
						{
							m_ammo.push_back(SWeaponAmmo(pFiremodeAmmo, min(minAmmoDrop, pFiremode->GetClipSize())));
						}
					}
				}

				// Exchange also ammo pool from inventory to bonus ammo map, for next user who picks it up
				{
					const int ammoCount = pAIInventory->GetAmmoCount(pFiremodeAmmo);

					if (ammoCount > 0)
					{
						SetInventoryAmmoCount(pFiremodeAmmo, 0);
						SWeaponAmmoUtils::SetAmmo(m_bonusammo, pFiremodeAmmo, ammoCount);
					}
					else
					{
						//Put always some extra rounds when dropped by AI
						const SWeaponAmmo* pDefaultBonusAmmo = SWeaponAmmoUtils::FindAmmoConst(m_weaponsharedparams->ammoParams.bonusAmmo, pFiremodeAmmo);
						if (pDefaultBonusAmmo)
						{
							SetInventoryAmmoCount(pFiremodeAmmo, 0);
							SWeaponAmmoUtils::SetAmmo(m_bonusammo, pFiremodeAmmo, std::max(minAmmoDrop, cry_random(0, pDefaultBonusAmmo->count - 1)));
						}
					}
				}
			}
		}
	}

	m_minDropAmmoAvailable = false;
}

//------------------------------------------------------------------------
void CWeapon::OnDroppedByPlayer(IInventory* pPlayerInventory)
{
	CRY_ASSERT(pPlayerInventory);

	TFireModeVector::const_iterator firemodesEndIt = m_firemodes.end();
	for (TFireModeVector::const_iterator firemodeCit = m_firemodes.begin(); firemodeCit != firemodesEndIt ; ++firemodeCit)
	{
		const CFireMode* pFiremode = *firemodeCit;
		if (pFiremode)
		{
			IEntityClass* pFiremodeAmmo = pFiremode->GetAmmoType();
			if (pFiremodeAmmo)
			{
				// Exchange also ammo pool from inventory to bonus ammo map, for next user who picks it up
				int ammoCount = pPlayerInventory->GetAmmoCount(pFiremodeAmmo);
				if (ammoCount > 0)
				{
					if(gEnv->bMultiplayer)
					{
						int currentClipAmount = GetAmmoCount(pFiremodeAmmo);
						int clipSize = pFiremode->GetClipSize();

						int numForClip = clamp_tpl(clipSize - currentClipAmount, 0, ammoCount);
						SetAmmoCount(pFiremodeAmmo, currentClipAmount + numForClip);

						ammoCount -= numForClip;
					}
					
					SetInventoryAmmoCount(pFiremodeAmmo, 0);
					SWeaponAmmoUtils::SetAmmo(m_bonusammo, pFiremodeAmmo, ammoCount);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CWeapon::OnMelee(EntityId shooterId)
{
	BROADCAST_WEAPON_EVENT(OnMelee, (this, shooterId));
}

//------------------------------------------------------------------------
void CWeapon::OnStartTargetting(IWeapon *pWeapon)
{
	BROADCAST_WEAPON_EVENT(OnStartTargetting,(this));
}

//------------------------------------------------------------------------
void CWeapon::OnStopTargetting(IWeapon *pWeapon)
{
	BROADCAST_WEAPON_EVENT(OnStopTargetting,(this));
}

//------------------------------------------------------------------------
void CWeapon::OnSelected(bool selected)
{
	BaseClass::OnSelected(selected);

	BROADCAST_WEAPON_EVENT(OnSelected,(this, selected));
}

//------------------------------------------------------------------------
void CWeapon::OnEnterFirstPerson()
{
	BaseClass::OnEnterFirstPerson();

	if (m_fm)
	{
		static_cast<CFireMode*>(m_fm)->OnEnterFirstPerson();
	}
}

//---------------------------------------------------------------------------
void CWeapon::AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event)
{
	const SReloadMagazineParams& magazineParams = m_weaponsharedparams->reloadMagazineParams;

	if (magazineParams.magazineEventCRC32 == event.m_EventNameLowercaseCRC32)
	{
		int slot = (GetEntity()->GetCharacter(eIGS_FirstPerson) == pCharacter) ? eIGS_FirstPerson : eIGS_ThirdPerson;

		//Spawn effect only once (in FP the event might come twice, one for FP, one for shadow character)
		if (!m_doingMagazineSwap)
		{
			const char* helperBone = (event.m_BonePathName && event.m_BonePathName[0]) ? event.m_BonePathName : "";
			if (event.m_CustomParameter && event.m_CustomParameter[0])
			{
				EntityEffects::SpawnParticleWithEntity(GetEntity(), slot, event.m_CustomParameter, helperBone, EntityEffects::SEffectSpawnParams(ZERO));
			}
			m_doingMagazineSwap = true;
		}

		HideCharacterAttachment(slot, magazineParams.magazineAttachment.c_str(), true);
	}
}

//--------------------------------------------------------
void CWeapon::OnAnimationEventStartFire(const char* szCustomParameter)
{
	if (!m_animationFiringLocator.IsSet())
	{
		const float fAutoStopTimer = szCustomParameter ? static_cast<float>(atof(szCustomParameter)) : 0.0f;
		m_animationFiringLocator.Set();

		StartFire();

		// If the parameter is a negative value the fire is not automatically stopped. This is dangerous, but could be useful
		// in some cases when we know the anim can't be interrupted before the following OnAnimationEventStopFire
		if (fAutoStopTimer >= 0.0f)
			GetScheduler()->TimerAction(fAutoStopTimer, CSchedulerAction<AnimationEventFireAutoStop>::Create(AnimationEventFireAutoStop(*this)), false);
	}
}

//--------------------------------------------------------
void CWeapon::OnAnimationEventStopFire()
{
	if (m_animationFiringLocator.IsSet())
	{
		m_animationFiringLocator.Release();

		StopFire();
	}
}


//--------------------------------------------------------
void CWeapon::OnZoomChanged(bool zoomed, int idx)
{
	BROADCAST_WEAPON_EVENT(OnZoomChanged,(this, zoomed, idx));
}


