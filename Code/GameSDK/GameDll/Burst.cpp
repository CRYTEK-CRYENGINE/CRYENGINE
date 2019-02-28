// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 11:9:2005   15:00 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Burst.h"
#include "Actor.h"

#include "WeaponSharedParams.h"
#include "ICryMannequin.h"
#include "ItemAnimation.h"
#include "PlayerAnimation.h"


CRY_IMPLEMENT_GTI(CBurst, CSingle);


//------------------------------------------------------------------------
CBurst::CBurst()
	:	m_fireTriggerDown(false)
{
	m_burstFiringLocator.SetBurst(this);
}

//------------------------------------------------------------------------
CBurst::~CBurst()
{
	m_pWeapon->SetFiringLocator(0);
}

//------------------------------------------------------------------------
void CBurst::Update(float frameTime, uint32 frameId)
{
	BaseClass::Update(frameTime, frameId);

	if (m_bursting)
	{
		if (m_next_shot <= 0.0f)
		{
			float saved_next_burst=m_next_burst;
			m_next_burst=0.0f;

			m_burst_shot = m_burst_shot+1;
			Shoot(true, m_fireParams->fireparams.autoReload);

			if (!m_firing || (m_burst_shot >= m_fireParams->burstparams.nshots-1))
			{
				EndBurst();
				m_firing = false;
				m_bursting = false;
				m_burst_shot = 0;
				SmokeEffect();
				m_pWeapon->PlayAction(GetFragmentIds().spin_down_tail);
				m_pWeapon->SetItemFlag(CItem::eIF_BlockActions, false);
			}

			m_next_burst=saved_next_burst;
		}
	}

	if (m_fireTriggerDown || m_bursting)
		m_pWeapon->RequireUpdate(eIUS_FireMode);

	m_next_burst -= frameTime;
	if (m_next_burst <= 0.0f)
		m_next_burst = 0.0f;
}

//------------------------------------------------------------------------
void CBurst::Activate(bool activate)
{
	BaseClass::Activate(activate);

	m_next_burst = 0.0f;
	m_next_burst_dt = 60.0f/(float)m_fireParams->burstparams.rate;
	m_bursting = false;
	m_burst_shot = 0;
	m_fireTriggerDown = false;

	if (m_fireParams->burstparams.multiPipe)
		m_pWeapon->SetFiringLocator(&m_burstFiringLocator);

	if(activate == false)
	{
		m_pWeapon->SetItemFlag(CItem::eIF_BlockActions, false);
	}
}

//------------------------------------------------------------------------
bool CBurst::CanFire(bool considerAmmo) const
{
	return CSingle::CanFire(considerAmmo) /*&& m_next_burst<=0.0f*/;
}

bool CBurst::IsFiring() const
{
	return CSingle::IsFiring() || m_bursting || m_fireTriggerDown;
}

//------------------------------------------------------------------------
void CBurst::StartFire()
{
	if (!m_bursting && !m_fireTriggerDown)
	{
		m_fireTriggerDown = true;
		if (m_next_burst <= 0.0f)
		{
			CSingle::StartFire();

			if(m_fired)	//Only set if first shot was successful
			{
				IEntityClass* ammo = GetShared()->fireparams.ammo_type_class;
				
				// We need to add 1, because Single decreased one the ammo count already
				int ammoCount = (m_pWeapon->GetAmmoCount(ammo) + 1); 

				const CTagDefinition* pTagDefinition = NULL;
					
				int fragmentID = m_pWeapon->GetFragmentID("burst_fire", &pTagDefinition);
				if (fragmentID != FRAGMENT_ID_INVALID)
				{
					TagState actionTags = TAG_STATE_EMPTY;

					if(pTagDefinition)
					{
						CTagState fragTags(*pTagDefinition);
						m_pWeapon->SetAmmoCountFragmentTags(fragTags, ammoCount);
						actionTags = fragTags.GetMask();
					}
						
					CBurstFireAction* pAction = new CBurstFireAction(PP_PlayerAction, fragmentID, this, actionTags);

					if(m_fireParams->burstparams.useBurstSoundParam)
					{
						int burstLength = m_fireParams->burstparams.nshots;

						float param = (float)min(burstLength, ammoCount);
						pAction->SetParam(CItem::sActionParamCRCs.burstFire, param);
					}

					m_pWeapon->PlayFragment(pAction, -1.f, -1.f, GetFireAnimationWeight(), GetFireFFeedbackWeight(), false);
				}

				m_next_burst = m_next_burst_dt;
				m_bursting = true;
				m_pWeapon->SetItemFlag(CItem::eIF_BlockActions, true);
			}
		}
	}
}

void CBurst::NetStartFire()
{
	IEntityClass* ammo = GetShared()->fireparams.ammo_type_class;

 	int ammoCount = m_pWeapon->GetAmmoCount(ammo); 

	m_pWeapon->PlayAction(
		GetFragmentIds().burst_fire,
		0, false, CItem::eIPAF_Default, -1.0f, 
		GetFireAnimationWeight(), GetFireFFeedbackWeight());
}

Vec3 CBurst::GetFiringDir(const Vec3 &probableHit, const Vec3& firingPos) const
{
	Vec3 dir = BaseClass::GetFiringDir(probableHit, firingPos);
	CActor *pActor = m_pWeapon->GetOwnerActor();
	const bool playerIsShooter = pActor ? pActor->IsPlayer() : false;

	const short int nShots = m_fireParams->burstparams.nshots;
	const short int spreadAngle = m_fireParams->burstparams.spreadAngle;

	if(!playerIsShooter && nShots > 0 && spreadAngle > 0 )
	{
		const float startingAngle = - spreadAngle / 2.0f;
		const float rotationStep = (float) spreadAngle / nShots;
		dir = dir.GetRotated(Vec3Constants<float>::fVec3_OneZ, DEG2RAD(startingAngle+(rotationStep*m_burst_shot)));
	}
	return dir;
}

//------------------------------------------------------------------------
void CBurst::StopFire()
{
	m_fireTriggerDown = false;
}

//------------------------------------------------------------------------
void CBurst::EndBurst()
{
	m_pWeapon->EndBurst();

	//Generally handled by Single::Shoot for most weapons - but bursts dont go through there for clients
	if(!gEnv->bServer && OutOfAmmo())
	{
		OnOutOfAmmo(m_pWeapon->GetOwnerActor(), m_fireParams->fireparams.autoReload);
	}
}


CBurst::CBurstFiringLocator::CBurstFiringLocator()
	:	m_pBurst(0)
{
}


void CBurst::CBurstFiringLocator::SetBurst(CBurst* pBurst)
{
	m_pBurst = pBurst;
}


int CBurst::CBurstFiringLocator::GetCurrentWeaponSlot() const
{
	int slot = eIGS_ThirdPerson;
	if (m_pBurst->m_pWeapon->GetStats().fp)
		slot = eIGS_FirstPerson;
	return slot;
}


bool CBurst::CBurstFiringLocator::HasValidFiringLocator() const
{
	int slot = eIGS_ThirdPerson;
	CActor* pOwnerActor = m_pBurst->m_pWeapon->GetOwnerActor();
	if (pOwnerActor && !pOwnerActor->IsThirdPerson())
		slot = eIGS_FirstPerson;
	bool isThirdPerson = pOwnerActor && pOwnerActor->IsThirdPerson();

	ICharacterInstance* pCharacter = m_pBurst->m_pWeapon->GetEntity()->GetCharacter(slot);
	if (pCharacter)
	{
		if (isThirdPerson && !pCharacter->IsCharacterVisible())
			return false;
		return true;
	}
	else
	{
		return false;
	}
}


Matrix34 CBurst::CBurstFiringLocator::GetFiringLocatorTM()
{
	Matrix34 fireHelperLocation = m_pBurst->m_pWeapon->GetCharacterAttachmentWorldTM(GetCurrentWeaponSlot(), m_pBurst->m_fireParams->burstparams.multiPipeHelper.c_str());
	return fireHelperLocation;
}



bool CBurst::CBurstFiringLocator::GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit)
{
	return false;
}


bool CBurst::CBurstFiringLocator::GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos)
{
	if (!HasValidFiringLocator())
		return false;

	Matrix34 fireHelperLocation = GetFiringLocatorTM();
	Vec3 right = fireHelperLocation.GetColumn0();
	pos = fireHelperLocation.TransformPoint(Vec3(ZERO)) + (right*(float)m_pBurst->m_burst_shot*0.1f - right*0.35f);

	return true;
}


bool CBurst::CBurstFiringLocator::GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	if (!HasValidFiringLocator())
		return false;

	Matrix34 fireHelperLocation = GetFiringLocatorTM();
	dir = fireHelperLocation.GetColumn1();

	return true;
}


bool CBurst::CBurstFiringLocator::GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	return false;
}


bool CBurst::CBurstFiringLocator::GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir)
{
	return false;
}


void CBurst::CBurstFiringLocator::WeaponReleased()
{
}


IAction::EStatus CBurstFireAction::Update(float timePassed)
{
	if(m_pFireMode)
	{
		SetParam(CItem::sActionParamCRCs.fired, m_pFireMode->Fired());
		SetParam(CItem::sActionParamCRCs.firstFire, m_pFireMode->FirstFire());
	}
	return BaseClass::Update(timePassed);
} 
