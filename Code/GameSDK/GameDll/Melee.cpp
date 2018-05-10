// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 23:3:2006   13:05 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Melee.h"
#include "Game.h"
#include "Item.h"
#include "Weapon.h"
#include "GameRules.h"
#include "Player.h"
#include "IVehicleSystem.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <CryAction/IMaterialEffects.h>
#include "GameCVars.h"
#include "WeaponSharedParams.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "WeaponMelee.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>	
#include "ScreenEffects.h"
#include "AutoAimManager.h"
#include <CryAISystem/ITargetTrackManager.h>
#include <CryAISystem/IAIActor.h>

#include "PlayerStateEvents.h"
#include "WeaponSystem.h"
#include "UI/HUD/HUDEventDispatcher.h"

#include "ItemAnimation.h"
#include "RecordingSystem.h"
#include "ICryMannequin.h"

#include "ActorImpulseHandler.h"

#if 0 && (defined(USER_claire) || defined(USER_tombe) || defined(USER_johnmichael)|| defined(USER_evgeny)) 
#define MeleeDebugLog(...)				CryLogAlways("[MELEE DEBUG] " __VA_ARGS__)
#else
#define MeleeDebugLog(...)				(void)(0)
#endif

EntityId	CMelee::s_meleeSnapTargetId = 0;
float			CMelee::s_fNextAttack = 0.0f;
bool			CMelee::s_bMeleeSnapTargetCrouched = false;


CRY_IMPLEMENT_GTI_BASE(CMelee);


//------------------------------------------------------------------------
CMelee::CMelee()
: m_pWeapon(NULL)
, m_pMeleeParams(NULL)
, m_attacking(false)
, m_attacked(false)
, m_netAttacking(false)
, m_slideKick(false)
, m_delayTimer(0.0f)
, m_useMeleeWeaponDelay(-1.0f)
, m_shortRangeAttack(false)
, m_hitTypeID(0)
, m_hitStatus(EHitStatus_Invalid)
, m_pMeleeAction(NULL)
, m_attackTurnAmount(0.f)
, m_attackTurnAmountSmoothRate(0.f)
, m_attackTime(0.f)
, m_piActionController(NULL)
{
	m_collisionHelper.SetUser(this);

	m_lastRayHit.pCollider = NULL;
}

void CMelee::Release()
{
	if (g_pGame)
	{
		g_pGame->GetWeaponSystem()->DestroyMeleeMode(this);
	}
}

//------------------------------------------------------------------------
CMelee::~CMelee()
{
	m_pMeleeParams = NULL;
	SAFE_RELEASE(m_pMeleeAction);
	SAFE_RELEASE(m_lastRayHit.pCollider);
}

void CMelee::InitMeleeMode(CWeapon* pWeapon, const SMeleeModeParams* pParams)
{
	CRY_ASSERT_MESSAGE(pParams, "Melee Mode Params NULL! Have you set up the weapon xml correctly?");

	m_pWeapon = pWeapon;
	m_pMeleeParams = pParams;
}

void CMelee::InitFragmentData()
{
	if( m_pWeapon && m_pWeapon->GetOwnerActor() && m_pWeapon->GetOwnerActor()->GetAnimatedCharacter() )
	{
		m_piActionController = m_pWeapon->GetOwnerActor()->GetAnimatedCharacter()->GetActionController();

		assert( m_pWeapon->GetOwnerActor()->IsPlayer() ); // If this is not the case, we cannot use PlayerMannequin!

		const CTagDefinition* pTagDefinition = m_piActionController->GetTagDefinition( PlayerMannequin.fragmentIDs.melee_weapon );
		if ( pTagDefinition )
		{
			m_pMeleeParams->meleetags.Resolve( pTagDefinition, PlayerMannequin.fragmentIDs.melee_weapon, m_piActionController );
		}
		else
		{
			CryWarning( VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CMelee: tagdefs for melee_weapon fragment missing for the player." );
		}
	}
}

//------------------------------------------------------------------------
void CMelee::Update(float frameTime, uint32 frameId)
{
	CRY_PROFILE_FUNCTION( PROFILE_GAME );

	bool remote = false;
	bool doMelee = false;
	bool requireUpdate = false;

	Vec3 dirToTarget(ZERO);
	if(m_pMeleeAction && s_meleeSnapTargetId && g_pGameCVars->pl_melee.mp_melee_system_camera_lock_and_turn)
	{
		m_attackTime += frameTime;

		CActor* pOwner = m_pWeapon->GetOwnerActor();
		IEntity* pTarget = gEnv->pEntitySystem->GetEntity(s_meleeSnapTargetId);
		if(pOwner && pTarget)
		{
			Vec3 targetPos = pTarget->GetWorldPos();

			if(s_bMeleeSnapTargetCrouched)
			{
				targetPos.z -= g_pGameCVars->pl_melee.mp_melee_system_camera_lock_crouch_height_offset;
			}

			dirToTarget = targetPos - pOwner->GetEntity()->GetWorldPos();
			dirToTarget.Normalize();

			static const float smooth_time = 0.1f;
			SmoothCD(m_attackTurnAmount, m_attackTurnAmountSmoothRate, frameTime, 1.f, smooth_time);
			Quat newViewRotation;
			newViewRotation.SetSlerp(pOwner->GetViewRotation(), Quat::CreateRotationVDir(dirToTarget), m_attackTurnAmount);
			Ang3 newAngles(newViewRotation);
			newAngles.y = 0.f; //No head tilting
			newViewRotation = Quat(newAngles);
			pOwner->SetViewRotation( newViewRotation );
		}

		if(m_attackTime >= g_pGameCVars->pl_melee.mp_melee_system_camera_lock_time && pOwner && pOwner->IsClient())
		{
			pOwner->GetActorParams().viewLimits.ClearViewLimit(SViewLimitParams::eVLS_Item);
		}
	}

	if (m_attacking)
	{
		MeleeDebugLog ("CMelee<%p> Update while attacking: m_attacked=%d, delay=%f", this, m_attacked, m_delayTimer);

		requireUpdate = true;
		if (m_delayTimer>0.0f)
		{
			RequestAlignmentToNearestTarget();
			m_delayTimer-=frameTime;
			if (m_delayTimer<=0.0f)
			{
				m_delayTimer=0.0f;
			}
		}
		else if (m_netAttacking)
		{
			remote = true;
			doMelee = true;
			m_attacking = false;
			m_slideKick = false;
			m_netAttacking = false;

			m_pWeapon->SetBusy(false);
		}	
		else if (!m_attacked)
		{
			doMelee = true;
			m_attacked = true;
		}

		if ( !m_collisionHelper.IsBlocked() && doMelee)
		{
			if (CActor *pActor = m_pWeapon->GetOwnerActor())
			{
				if (IMovementController * pMC = pActor->GetMovementController())
				{
					SMovementState info;
					pMC->GetMovementState(info);

					if(!dirToTarget.IsZeroFast())
					{
						PerformMelee(info.weaponPosition, dirToTarget, remote); //We know where we will be facing at the point of impact - using our current fire direction is not accurate enough
					}
					else
					{
						PerformMelee(info.weaponPosition, info.fireDirection, remote);
					}
				}
			}
		}

		if( (m_useMeleeWeaponDelay <= 0.0f && m_useMeleeWeaponDelay > -1.0f) )
		{
			m_useMeleeWeaponDelay = -1.0f;

			// Switch to the MELEE WEAPON.
			SwitchToMeleeWeaponAndAttack();

			m_attacking = false;
		}
		m_useMeleeWeaponDelay -= frameTime;
	}

	if (requireUpdate)
		m_pWeapon->RequireUpdate(eIUS_FireMode);
}

//------------------------------------------------------------------------
void CMelee::Activate(bool activate)
{
	MeleeDebugLog ("CMelee<%p> Activate(%s)", this, activate ? "true" : "false");

	if( !IsMeleeWeapon() )
	{
		m_attacking = false;
		m_slideKick = false;
		m_shortRangeAttack = false;
		m_delayTimer = 0.0f;
	}

	if(!activate && m_pMeleeAction)
	{
		m_pMeleeAction->ForceFinish();
	}
}

//------------------------------------------------------------------------
bool CMelee::CanAttack() const
{
	if(!m_attacking)
	{
		if (gEnv->bMultiplayer)
		{
			if(CPlayer * pPlayer = static_cast<CPlayer*>(m_pWeapon->GetOwnerActor()))
			{
				if(pPlayer->IsClient() && gEnv->pTimer->GetFrameStartTime().GetSeconds() > CMelee::s_fNextAttack && !pPlayer->GetStealthKill().IsBusy())
				{
					return true;
				}
			}
			
			return false; 
		}
		else
		{
			return true;
		}
	}
	else
	{
		return false;
	}	
}

//------------------------------------------------------------------------
struct CMelee::StopAttackingAction
{
	CMelee *_this;
	StopAttackingAction(CMelee *melee): _this(melee) {};
	void execute(CItem *pItem)
	{
		_this->m_attacking = false;
		_this->m_slideKick = false;
		_this->m_netAttacking = false;

		_this->m_delayTimer = 0.0f;
		pItem->SetBusy(false);
		pItem->ForcePendingActions();
		MeleeDebugLog ("CMelee<%p> StopAttackingAction is being executed!", _this);

		CActor* pActor(NULL);
		if(!gEnv->bMultiplayer)
		{
			if (IEntity* owner = pItem->GetOwner())
				if (IAIObject* aiObject = owner->GetAI())
					if (IAIActor* aiActor = aiObject->CastToIAIActor())
						aiActor->SetSignal(0, "OnMeleePerformed");
		}
		else if( g_pGameCVars->pl_melee.mp_melee_system_camera_lock_and_turn && s_meleeSnapTargetId && (pActor = pItem->GetOwnerActor()) && pActor->IsClient() )
		{
			CActor* pOwnerActor = pItem->GetOwnerActor();
			pOwnerActor->GetActorParams().viewLimits.ClearViewLimit(SViewLimitParams::eVLS_Item);

			s_meleeSnapTargetId = 0;
			CHANGED_NETWORK_STATE(pOwnerActor, CPlayer::ASPECT_SNAP_TARGET);
		}

		if(_this->m_pMeleeAction)
		{
			SAFE_RELEASE(_this->m_pMeleeAction);
		}
	}
};

SMeleeTags::SMeleeFragData CMelee::GenerateFragmentData( const SMeleeTags::TTagParamsContainer& tagContainer ) const
{
	const size_t numTags  = tagContainer.size();
	if( numTags > 0 )
	{
		const size_t tagIndex = cry_random((size_t)0, numTags - 1);
		const SMeleeTags::STagParams& meleeTags = tagContainer[tagIndex];

		SMeleeTags::SMeleeFragData fragData( tagIndex, meleeTags );

		return fragData;
	}

	return SMeleeTags::SMeleeFragData();
}

void CMelee::GenerateAndQueueMeleeAction() const
{
	const SMeleeTags& tags = m_pMeleeParams->meleetags;
	const CWeaponMelee::EMeleeStatus meleeStatus = static_cast<CWeaponMelee*> (m_pWeapon)->GetMeleeAttackAction();

	const char* pActionName = NULL;

	switch( meleeStatus )
	{
	case CWeaponMelee::EMeleeStatus_Left:
		GenerateAndQueueMeleeActionForStatus( tags.tag_params_combo_left );
		pActionName = "MeleeCombo";
		break;
	case CWeaponMelee::EMeleeStatus_Right:
		GenerateAndQueueMeleeActionForStatus( tags.tag_params_combo_right );
		pActionName = "MeleeCombo";
		break;
	case CWeaponMelee::EMeleeStatus_KillingBlow:
		GenerateAndQueueMeleeActionForStatus( tags.tag_params_combo_killingblow );
		pActionName = "MeleeKillingBlow";
		break;
	default:
		CryLog( "[Melee] Attempted to run a melee action when the status was unknown" );
		break;
	}
}

void CMelee::GenerateAndQueueMeleeActionForStatus( const SMeleeTags::TTagParamsContainer& tagContainer ) const
{
	SMeleeTags::SMeleeFragData fragData = GenerateFragmentData( tagContainer );
	if( fragData.m_pMeleeTags )
	{
		m_hitTypeID = fragData.m_pMeleeTags->hitType;

		CItemAction *pAction = new CItemAction(PP_PlayerAction, PlayerMannequin.fragmentIDs.melee_weapon, fragData.m_pMeleeTags->tagState);
		m_pWeapon->PlayFragment( pAction );
	}
}

const ItemString &CMelee::SelectMeleeAction() const
{
	CRY_ASSERT_MESSAGE( !IsMeleeWeapon(), "SelectMeleeAction is deprecated for the melee weapon!" );

	const SMeleeActions& actions = m_pMeleeParams->meleeactions;

	CActor* pOwner = m_pWeapon->GetOwnerActor();
	if (pOwner)
	{
		if( !IsMeleeWeapon() )
		{
			m_hitTypeID = CGameRules::EHitType::Melee;

			if (m_shortRangeAttack && !actions.attack_closeRange.empty())
			{
				return actions.attack_closeRange;
			}
		}
	}

	return actions.attack;
}

void CMelee::StartAttack()
{
	bool canAttack = CanAttack();

	MeleeDebugLog ("CMelee<%p> StartAttack canAttack=%s", this, canAttack ? "true" : "false");

	if (!canAttack)
		return;

	CActor* pOwner = m_pWeapon->GetOwnerActor();
	CPlayer *ownerPlayer = m_pWeapon->GetOwnerPlayer();

	m_attacking = true;
	m_slideKick = false;
	m_attacked = false;
	m_pWeapon->RequireUpdate(eIUS_FireMode);
	m_pWeapon->ExitZoom();

	bool isClient = pOwner ? pOwner->IsClient() : false;
	bool doingMultiMeleeAttack = true;

	if (!DoSlideMeleeAttack(pOwner))
	{
		const float use_melee_weapon_delay = m_pMeleeParams->meleeparams.use_melee_weapon_delay;
		if( use_melee_weapon_delay >= 0.0f )
		{
			if( !ownerPlayer || ownerPlayer->IsSliding() || ownerPlayer->IsExitingSlide() )
			{
				m_attacking = false;
				return;
			}

			if( use_melee_weapon_delay == 0.0f )
			{
				// instant switch: don't use the weapon's melee, send the event to the WeaponMelee weapon instead!
				if( SwitchToMeleeWeaponAndAttack() )
				{
					m_useMeleeWeaponDelay = -1.0f;
					return;
				}
			}
		}

		if( IsMeleeWeapon() )
		{
			doingMultiMeleeAttack = false;

			GenerateAndQueueMeleeAction();
		}
		else if(!gEnv->bMultiplayer || !g_pGameCVars->pl_melee.mp_melee_system || !ownerPlayer || !StartMultiAnimMeleeAttack(ownerPlayer))
		{
			const ItemString &meleeAction = SelectMeleeAction();

			FragmentID fragmentId = m_pWeapon->GetFragmentID(meleeAction.c_str());
			m_pWeapon->PlayAction(fragmentId, 0, false, CItem::eIPAF_Default);

			doingMultiMeleeAttack = false;
		}

		m_delayTimer = GetDelay();
		m_attackTime = 0.f;

		if (ownerPlayer)
		{
			ownerPlayer->StateMachineHandleEventMovement( PLAYER_EVENT_FORCEEXITSLIDE );
		}
	}
	else
	{
		CRY_ASSERT(pOwner && (pOwner->GetActorClass() == CPlayer::GetActorClassType()));

		doingMultiMeleeAttack = false;
		
		m_hitTypeID = CGameRules::EHitType::Melee;

		// Dispatch the event and the delay timer is magically set!
		ownerPlayer->StateMachineHandleEventMovement( SStateEventSlideKick(this) );

		m_slideKick = true;
	}

	m_pWeapon->SetBusy(true);

	if(!doingMultiMeleeAttack)
	{
		// Set up a timer to disable the melee attack at the end of the first-person animation...
		uint32 durationInMilliseconds = static_cast<uint32>(GetDuration() * 1000.f);
		MeleeDebugLog ("CMelee<%p> Setting a timer to trigger StopAttackingAction (duration=%u)", this, durationInMilliseconds);

		// How much longer we need the animation to be than the damage delay, in seconds...
		const float k_requireAdditionalTime = 0.1f;

		if (durationInMilliseconds < (m_delayTimer + k_requireAdditionalTime) * 1000.0f)
		{
			if (!gEnv->IsDedicated())
			{
				CRY_ASSERT_MESSAGE(false, string().Format("CMelee<%p> Warning! Melee attack timeout (%f seconds) needs to be substantially longer than the damage delay (%f seconds)! Increasing...", this, durationInMilliseconds / 1000.f, m_delayTimer));
			}
			durationInMilliseconds = (uint32) ((m_delayTimer + k_requireAdditionalTime) * 1000.0f + 0.5f);	// Add 0.5f to round up when turning into a uint32
		}
		m_pWeapon->GetScheduler()->TimerAction(durationInMilliseconds, CSchedulerAction<StopAttackingAction>::Create(this), true);
	}
	
	m_pWeapon->OnMelee(m_pWeapon->GetOwnerId());
	m_pWeapon->RequestStartMeleeAttack((m_pWeapon->GetMelee() == this),	false);

	if (isClient)
	{
		s_fNextAttack				= GetDuration() + gEnv->pTimer->GetFrameStartTime().GetSeconds();
		if(s_meleeSnapTargetId = GetNearestTarget())
		{
			IActor* pTargetActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(s_meleeSnapTargetId);
			s_bMeleeSnapTargetCrouched = pTargetActor && static_cast<CPlayer*>(pTargetActor)->GetStance() == STANCE_CROUCH;
		}
		CHANGED_NETWORK_STATE(pOwner, CPlayer::ASPECT_SNAP_TARGET);

		if (ownerPlayer)
		{
			if (ownerPlayer->IsThirdPerson())
			{
				CAudioSignalPlayer::JustPlay(m_pMeleeParams->meleeparams.m_3PSignalId, ownerPlayer->GetEntityId());
			}
			else
			{
				CAudioSignalPlayer::JustPlay(m_pMeleeParams->meleeparams.m_FPSignalId, ownerPlayer->GetEntityId());
			}
		}

		CCCPOINT(Melee_LocalActorMelee);
	}
	else
	{
		CCCPOINT(Melee_NonLocalActorMelee);
	}
}

//------------------------------------------------------------------------
void CMelee::NetAttack()
{
	MeleeDebugLog ("CMelee<%p> NetAttack", this);

	const ItemString &meleeAction = SelectMeleeAction();

	m_pWeapon->OnMelee(m_pWeapon->GetOwnerId());

	CActor* pOwner = m_pWeapon->GetOwnerActor();

	if (!DoSlideMeleeAttack(pOwner))
	{
		if(!gEnv->bMultiplayer || !g_pGameCVars->pl_melee.mp_melee_system || !StartMultiAnimMeleeAttack(pOwner))
		{
			FragmentID fragmentId = m_pWeapon->GetFragmentID(meleeAction.c_str());
			m_pWeapon->PlayAction(fragmentId, 0, false, CItem::eIPAF_Default);
		}
	}
	else
	{
		CRY_ASSERT(pOwner && (pOwner->GetActorClass() == CPlayer::GetActorClassType()));

		if( pOwner->IsPlayer() )
		{
			static_cast<CPlayer*> (pOwner)->StateMachineHandleEventMovement( SStateEventSlideKick(this) );
		}
	}

	m_attacking = true;
	m_slideKick = false;
	m_netAttacking = true;
	m_delayTimer = GetDelay();

	m_pWeapon->SetBusy(true);
	m_pWeapon->RequireUpdate(eIUS_FireMode);

	CPlayer *pOwnerPlayer = m_pWeapon->GetOwnerPlayer();
	if (pOwnerPlayer)
	{
		if (pOwnerPlayer->IsThirdPerson())
		{
			CAudioSignalPlayer::JustPlay(m_pMeleeParams->meleeparams.m_3PSignalId, pOwnerPlayer->GetEntityId());
		}
		else
		{
			CAudioSignalPlayer::JustPlay(m_pMeleeParams->meleeparams.m_FPSignalId, pOwnerPlayer->GetEntityId());
		}
	}
}

//------------------------------------------------------------------------
int CMelee::GetDamage() const
{
	return m_pMeleeParams->meleeparams.damage;
}

//-----------------------------------------------------------------------
void CMelee::PerformMelee(const Vec3 &pos, const Vec3 &dir, bool remote)
{
	CCCPOINT_IF(! remote, Melee_PerformLocal);
	CCCPOINT_IF(remote, Melee_PerformRemote);

	MeleeDebugLog ("CMelee<%p> PerformMelee(remote=%s)", this, remote ? "true" : "false");

#if !defined(_RELEASE)
	if(g_pGameCVars->pl_melee.debug_gfx)
	{
		IPersistantDebug  *pDebug = g_pGame->GetIGameFramework()->GetIPersistantDebug();
		pDebug->Begin("CMelee::PerformMelee", false);
		pDebug->AddLine(pos, (pos + dir), ColorF(1.f,0.f,0.f,1.f), 15.f);
	}
#endif

	m_collisionHelper.DoCollisionTest(SCollisionTestParams(pos, dir, GetRange(), m_pWeapon->GetOwnerId(), 0, remote));
}

//------------------------------------------------------------------------
bool CMelee::PerformCylinderTest(const Vec3 &pos, const Vec3 &dir, bool remote)
{
	MeleeDebugLog ("CMelee<%p> PerformCylinderTest(remote=%s)", this, remote ? "true" : "false");

	IEntity *pOwner = m_pWeapon->GetOwner();
	IPhysicalEntity *pIgnore = pOwner?pOwner->GetPhysics():0;

	primitives::cylinder cyl;
	cyl.r = 0.25f;
	cyl.axis = dir;
	cyl.hh = GetRange() *0.5f;
	cyl.center = pos + dir.normalized()*cyl.hh;

	geom_contact *contacts;
	intersection_params params;
	params.bStopAtFirstTri = false;
	params.bNoBorder = true;
	params.bNoAreaContacts = true;
	float n = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(primitives::cylinder::type, &cyl, Vec3(ZERO),
		ent_rigid|ent_sleeping_rigid|ent_independent|ent_static|ent_terrain|ent_water, &contacts, 0,
		geom_colltype_foliage|geom_colltype_player, &params, 0, 0, &pIgnore, pIgnore?1:0);

	int ret = (int)n;

	float closestdSq = 9999.0f;
	geom_contact *closestc = 0;
	geom_contact *currentc = contacts;

	for (int i=0; i<ret; i++)
	{
		geom_contact *contact = currentc;
		if (contact)
		{
			IPhysicalEntity *pCollider = gEnv->pPhysicalWorld->GetPhysicalEntityById(contact->iPrim[0]);
			if (pCollider)
			{
				IEntity *pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pCollider);
				if (pEntity)
				{
					if (pEntity == pOwner)
					{
						++currentc;
						continue;
					}
				}

				const float distSq = (pos-currentc->pt).len2();
				if (distSq < closestdSq)
				{
					closestdSq = distSq;
					closestc = contact;
				}
			}
		}
		++currentc;
	}

	if (closestc)
	{
		IPhysicalEntity *pCollider = gEnv->pPhysicalWorld->GetPhysicalEntityById(closestc->iPrim[0]);
		IEntity* pCollidedEntity = pCollider ? gEnv->pEntitySystem->GetEntityFromPhysics(pCollider) : NULL;
		EntityId collidedEntityId = pCollidedEntity ? pCollidedEntity->GetId() : 0;

		ApplyMeleeDamage(closestc->pt, dir, -dir, pCollider, collidedEntityId, closestc->iPrim[1], 0, closestc->id[1], remote, closestc->iPrim[1]);
	}

	return closestc!=0;
}

//------------------------------------------------------------------------
int CMelee::Hit(const Vec3 &pt, const Vec3 &dir, const Vec3 &normal, IPhysicalEntity *pCollider, EntityId collidedEntityId, int partId, int ipart, int surfaceIdx, bool remote)
{
	MeleeDebugLog ("CMelee<%p> HitPointDirNormal(remote=%s)", this, remote ? "true" : "false");
	int hitTypeID = 0;

	CActor *pOwnerActor = m_pWeapon->GetOwnerActor();

	if (pOwnerActor)
	{	
		IActor* pTargetActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(collidedEntityId);
		IEntity* pTarget = pTargetActor ? pTargetActor->GetEntity() : gEnv->pEntitySystem->GetEntity(collidedEntityId);
		IEntity* pOwnerEntity = pOwnerActor->GetEntity();
		IAIObject* pOwnerAI = pOwnerEntity->GetAI();
		
		float damageScale = 1.0f;
		bool silentHit = false;

		if(pTargetActor)
		{
			IAnimatedCharacter* pTargetAC = pTargetActor->GetAnimatedCharacter();
			IAnimatedCharacter* pOwnerAC = pOwnerActor->GetAnimatedCharacter();

			if(pTargetAC && pOwnerAC)
			{
				Vec3 targetFacing(pTargetAC->GetAnimLocation().GetColumn1());
				Vec3 ownerFacing(pOwnerAC->GetAnimLocation().GetColumn1());
				float ownerFacingDot = ownerFacing.Dot(targetFacing);
				float fromBehindDot = cos_tpl(DEG2RAD(g_pGameCVars->pl_melee.angle_limit_from_behind));

				if(ownerFacingDot > fromBehindDot)
				{
#ifndef _RELEASE
					if (g_pGameCVars->g_LogDamage)
					{
						CryLog ("[DAMAGE] %s '%s' is%s meleeing %s '%s' from behind (because %f > %f)",
							pOwnerActor->GetEntity()->GetClass()->GetName(), pOwnerActor->GetEntity()->GetName(), silentHit ? " silently" : "",
							pTargetActor->GetEntity()->GetClass()->GetName(), pTargetActor->GetEntity()->GetName(),
							ownerFacingDot, fromBehindDot);
					}
#endif

					damageScale *= g_pGameCVars->pl_melee.damage_multiplier_from_behind;
				}
			}
		}


		// Send target stimuli
		if (!gEnv->bMultiplayer)
		{
			IAISystem *pAISystem = gEnv->pAISystem;
			ITargetTrackManager *pTargetTrackManager = pAISystem ? pAISystem->GetTargetTrackManager() : NULL;
			if (pTargetTrackManager && pOwnerAI)
			{
				IAIObject *pTargetAI = pTarget ? pTarget->GetAI() : NULL;
				if (pTargetAI)
				{
					const tAIObjectID aiOwnerId = pOwnerAI->GetAIObjectID();
					const tAIObjectID aiTargetId = pTargetAI->GetAIObjectID();

					TargetTrackHelpers::SStimulusEvent eventInfo;
					eventInfo.vPos = pt;
					eventInfo.eStimulusType = TargetTrackHelpers::eEST_Generic;
					eventInfo.eTargetThreat = AITHREAT_AGGRESSIVE;
					pTargetTrackManager->HandleStimulusEventForAgent(aiTargetId, aiOwnerId, "MeleeHit",eventInfo);
					pTargetTrackManager->HandleStimulusEventInRange(aiOwnerId, "MeleeHitNear", eventInfo, 5.0f);
				}
			}
		}

		//Check if is a friendly hit, in that case FX and Hit will be skipped
		bool isFriendlyHit = (pOwnerEntity && pTarget) ? IsFriendlyHit(pOwnerEntity, pTarget) : false;

		if(!isFriendlyHit)
		{
			CPlayer * pAttackerPlayer = pOwnerActor->IsPlayer() ? static_cast<CPlayer*>(pOwnerActor) : NULL;
			float damage = m_pMeleeParams->meleeparams.damage_ai;

			if(pOwnerActor->IsPlayer())
			{
				damage = m_slideKick ? m_pMeleeParams->meleeparams.slide_damage : GetMeleeDamage();
			}

#ifndef _RELEASE
			if (pTargetActor && g_pGameCVars->g_LogDamage)
			{
				CryLog ("[DAMAGE] %s '%s' is%s meleeing %s '%s' applying damage = %.3f x %.3f = %.3f",
					pOwnerActor->GetEntity()->GetClass()->GetName(), pOwnerActor->GetEntity()->GetName(), silentHit ? " silently" : "",
					pTargetActor->GetEntity()->GetClass()->GetName(), pTargetActor->GetEntity()->GetName(),
					damage, damageScale, damage * damageScale);
			}
#endif

			//Generate Hit
			if(pTarget)
			{
				CGameRules *pGameRules = g_pGame->GetGameRules();
				CRY_ASSERT_MESSAGE(pGameRules, "No game rules! Melee can not apply hit damage");

				if (pGameRules)
				{
					hitTypeID = silentHit ? CGameRules::EHitType::SilentMelee : m_hitTypeID;
					HitInfo info(m_pWeapon->GetOwnerId(), pTarget->GetId(), m_pWeapon->GetEntityId(),
						damage * damageScale, 0.0f, surfaceIdx, partId, hitTypeID, pt, dir, normal);

					if (m_pMeleeParams->meleeparams.knockdown_chance>0 && cry_random(0, 99) < m_pMeleeParams->meleeparams.knockdown_chance)
						info.knocksDown = true;

					info.remote = remote;

					pGameRules->ClientHit(info);
				}

				if (pAttackerPlayer && pAttackerPlayer->IsClient())
				{
					const Vec3 posOffset = (pt - pTarget->GetWorldPos());
					SMeleeHitParams params;

					params.m_boostedMelee = false;
					params.m_hitNormal = normal;
					params.m_hitOffset = posOffset;
					params.m_surfaceIdx = surfaceIdx;
					params.m_targetId = pTarget->GetId();

					pAttackerPlayer->OnMeleeHit(params);
				}
			}
			else
			{
				//Play Material FX
				PlayHitMaterialEffect(pt, normal, false, surfaceIdx);
			}
		}

		if (pTarget)
		{
			CActor *pCTargetActor = static_cast<CActor*>(pTargetActor);
			CPlayer* pTargetPlayer = (pTargetActor && pTargetActor->IsPlayer()) ? static_cast<CPlayer*>(pTargetActor) : NULL;

			if(pTargetPlayer && pTargetPlayer->IsClient())
			{
				if(m_pMeleeParams->meleeparams.trigger_client_reaction)
				{
					pTargetPlayer->TriggerMeleeReaction();
				}
			}
		}
	}

	return hitTypeID;
}

//------------------------------------------------------------------------
/* static */ void CMelee::PlayHitMaterialEffect(const Vec3 &position, const Vec3 &normal, bool bBoostedMelee, int surfaceIdx)
{
	//Play Material FX
	const char* meleeFXType = bBoostedMelee ? "melee_combat" : "melee";  //Benito: Check with fx guys to update names

	IMaterialEffects* pMaterialEffects = gEnv->pGameFramework->GetIMaterialEffects();

	TMFXEffectId effectId = pMaterialEffects->GetEffectId(meleeFXType, surfaceIdx);
	if (effectId != InvalidEffectId)
	{
		SMFXRunTimeEffectParams params;
		params.pos = position;
		params.normal = normal;
		params.playflags = eMFXPF_All | eMFXPF_Disable_Delay;
		//params.soundSemantic = eSoundSemantic_Player_Foley;
		pMaterialEffects->ExecuteEffect(effectId, params);
	}
}

//------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
struct CMelee::DelayedImpulse
{
	DelayedImpulse(CMelee& melee, EntityId collidedEntityId, const Vec3& impulsePos, const Vec3& impulse, int partId, int ipart, int hitTypeID)
		: m_melee(melee)
		, m_collidedEntityId(collidedEntityId)
		, m_impulsePos(impulsePos)
		, m_impulse(impulse)
		, m_partId(partId)
		, m_ipart(ipart)
		, m_hitTypeID(hitTypeID)
	{

	};

	void execute(CItem *pItem)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_collidedEntityId);
		IPhysicalEntity* pPhysicalEntity = pEntity ? pEntity->GetPhysics() : NULL;

		if (pPhysicalEntity)
		{
			m_melee.m_collisionHelper.Impulse(pPhysicalEntity, m_impulsePos, m_impulse, m_partId, m_ipart, m_hitTypeID);
		}
	}

private:
	CMelee& m_melee;
	EntityId m_collidedEntityId;
	Vec3 m_impulsePos;
	Vec3 m_impulse;
	int m_partId;
	int m_ipart;
	int m_hitTypeID;
};
//////////////////////////////////////////////////////////////////////////

void CMelee::Impulse(const Vec3 &pt, const Vec3 &dir, const Vec3 &normal, IPhysicalEntity *pCollider, EntityId collidedEntityId, int partId, int ipart, int surfaceIdx, int hitTypeID, int iPrim)
{
	if (pCollider && m_pMeleeParams->meleeparams.impulse>0.001f)
	{
		CActor* pOwnerActor = m_pWeapon->GetOwnerActor();
		const SPlayerMelee& meleeCVars = g_pGameCVars->pl_melee;
		const SMeleeParams& meleeParams = m_pMeleeParams->meleeparams;
		float impulse = meleeParams.impulse;
		bool aiShooter = pOwnerActor ? !pOwnerActor->IsPlayer() : true;
		bool delayImpulse = false;

		float impulseScale = 1.0f;
		
		//[kirill] add impulse to phys proxy - to make sure it's applied to cylinder as well (not only skeleton) - so that entity gets pushed
		// if no pEntity - do it old way
		IEntity * pEntity = gEnv->pEntitySystem->GetEntity(collidedEntityId);
		IGameFramework* pGameFramework = g_pGame->GetIGameFramework();
		CActor* pTargetActor = static_cast<CActor*>(pGameFramework->GetIActorSystem()->GetActor(collidedEntityId));
		if (pEntity && pTargetActor)
		{
			//If it's an entity, use the specific impulses if needed, and apply to physics proxy
			if ( meleeCVars.impulses_enable != SPlayerMelee::ei_Disabled )
			{
				impulse = meleeParams.impulse_actor;

				bool aiTarget = !pTargetActor->IsPlayer();

				if (aiShooter && !aiTarget)
				{
					float impulse_ai_to_player = GetImpulseAiToPlayer();
					if (impulse_ai_to_player != -1.f)
					{
						impulse = impulse_ai_to_player;
					}
				}

				//Delay a bit on death actors, when switching from alive to death, impulses don't apply
				//I schedule an impulse here, to get rid off the ugly .lua code which was calculating impulses on its own
				if (pTargetActor->IsDead())
				{
					if (meleeCVars.impulses_enable != SPlayerMelee::ei_OnlyToAlive)
					{
						delayImpulse = true;
						const float actorCustomScale = 1.0f;

						impulseScale *= actorCustomScale;
					}
					else
					{
						impulse = 0.0f;
					}
				}
				else if (meleeCVars.impulses_enable == SPlayerMelee::ei_OnlyToDead)
				{
					// Always allow impulses for melee from AI to local player
					// [*DavidR | 27/Oct/2010] Not sure about this
					if (!(aiShooter && !aiTarget))
						impulse = 0.0f;
				}
			}
			else if (pGameFramework->GetIVehicleSystem()->GetVehicle(collidedEntityId))
			{
				impulse = m_pMeleeParams->meleeparams.impulse_vehicle;
				impulseScale = 1.0f;
			}
		}

		const float fScaledImpulse = impulse * impulseScale;
		if (fScaledImpulse > 0.0f)
		{
			if (!delayImpulse)
			{
				m_collisionHelper.Impulse(pCollider, pt, dir * fScaledImpulse, partId, ipart, hitTypeID);
			}
			else
			{
				//Force up impulse, to make the enemy fly a bit
				Vec3 newDir = (dir.z < 0.0f) ? Vec3(dir.x, dir.y, 0.1f) : dir;
				newDir.Normalize();
				newDir.x *= fScaledImpulse;
				newDir.y *= fScaledImpulse;
				newDir.z *= impulse;

				if( pTargetActor )
				{
					pe_action_impulse imp;
					imp.iApplyTime = 0;
					imp.impulse = newDir;
					//imp.ipart = ipart;
					imp.partid = partId;
					imp.point = pt;
					pTargetActor->GetImpulseHander()->SetOnRagdollPhysicalizedImpulse( imp );
				}
				else
				{
					m_pWeapon->GetScheduler()->TimerAction(100, CSchedulerAction<DelayedImpulse>::Create(DelayedImpulse(*this, collidedEntityId, pt, newDir, partId, ipart, hitTypeID)), true);
				}
			}
		}

		// scar bullet
		// m = 0.0125
		// v = 800
		// energy: 4000
		// in this case the mass of the active collider is a player part
		// so we must solve for v given the same energy as a scar bullet
		float speed = sqrt_tpl(4000.0f/(80.0f*0.5f)); // 80.0f is the mass of the player

		if( IRenderNode *pBrush = (IRenderNode*)pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC) )
		{
			speed = 0.003f;
		}

		m_collisionHelper.GenerateArtificialCollision(m_pWeapon->GetOwner(), pCollider, pt, normal, dir * speed, partId, ipart, surfaceIdx, iPrim);
	}
}

//-----------------------------------------------------------
bool CMelee::IsFriendlyHit(IEntity* pShooter, IEntity* pTarget)
{
	if(gEnv->bMultiplayer)
	{
		// Only count entity as friendly if friendly fire damage is off
		// and the player is on the same team. This will prevent blood effects.
		if (g_pGame->GetGameRules()->GetFriendlyFireRatio() <= 0.f)
		{
			CActor* pTargetActor = (CActor*)g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pTarget->GetId());
			if (pTargetActor)
			{
				return pTargetActor->IsFriendlyEntity(pShooter->GetId()) != 0;
			}
		}
	}
	else
	{
		IActor* pAITarget = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pTarget->GetId());
		if(pAITarget && pTarget->GetAI() && !pTarget->GetAI()->IsHostile(pShooter->GetAI(),false))
		{
			return true;
		}

		return IsMeleeFilteredOnEntity(*pTarget);
	}

	return false;
}

//-----------------------------------------------------------------------
void CMelee::ApplyMeleeEffects(bool hit)
{
}

void CMelee::ApplyMeleeDamage(const Vec3& point, const Vec3& dir, const Vec3& normal, IPhysicalEntity* physicalEntity,
															EntityId entityID, int partId, int ipart, int surfaceIdx, bool remote, int iPrim)
{
	const float blend = m_slideKick ? 0.4f : SATURATE(m_pMeleeParams->meleeparams.impulse_up_percentage);
	Vec3 impulseDir = Vec3(0, 0, 1) * blend + dir * (1.0f - blend);
	impulseDir.normalize();

	int hitTypeID = Hit(point, dir, normal, physicalEntity, entityID, partId, ipart, surfaceIdx, remote);

	IActor* pActor = gEnv->bMultiplayer && entityID ? g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(entityID) : NULL;
	if(!gEnv->bMultiplayer || !pActor) //MP handles melee impulses to actors in GameRulesClientServer.cpp. See: ClApplyActorMeleeImpulse)
	{
		Impulse(point, impulseDir, normal, physicalEntity, entityID, partId, ipart, surfaceIdx, hitTypeID, iPrim);
	}
}

//---------------------------------------------------------------------
void CMelee::RequestAlignmentToNearestTarget()
{
	CActor* pOwner = m_pWeapon->GetOwnerActor();
	if(pOwner && pOwner->IsClient())
	{
		if (!s_meleeSnapTargetId)
		{
			// If we don't already have an auto-aim target, try and find one.
			if (s_meleeSnapTargetId = GetNearestTarget())
			{
				IActor* pTargetActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(s_meleeSnapTargetId);
				s_bMeleeSnapTargetCrouched = pTargetActor && static_cast<CPlayer*>(pTargetActor)->GetStance() == STANCE_CROUCH;

				CHANGED_NETWORK_STATE(pOwner, CPlayer::ASPECT_SNAP_TARGET);
			}
		}

		if(!s_meleeSnapTargetId || m_netAttacking)
			return;

		if(pOwner && pOwner->IsClient() && m_pMeleeAction && g_pGameCVars->pl_melee.mp_melee_system_camera_lock_and_turn)
		{
			pOwner->GetActorParams().viewLimits.SetViewLimit(pOwner->GetViewRotation().GetColumn1(), 0.01f, 0.01f, 0.01f, 0.01f, SViewLimitParams::eVLS_Item);
		}

		g_pGame->GetAutoAimManager().SetCloseCombatSnapTarget(s_meleeSnapTargetId, 
			g_pGameCVars->pl_melee.melee_snap_end_position_range, 
			g_pGameCVars->pl_melee.melee_snap_move_speed_multiplier);
	}
}

//--------------------------------------------------------
EntityId CMelee::GetNearestTarget()
{
	CActor* pOwnerActor = m_pWeapon->GetOwnerActor();
	if(pOwnerActor == NULL || (gEnv->bMultiplayer && m_slideKick))
		return 0;

	CRY_ASSERT(pOwnerActor->IsClient());

	IMovementController* pMovementController = pOwnerActor->GetMovementController();
	if (!pMovementController)
		return 0;

	SMovementState moveState;
	pMovementController->GetMovementState(moveState);

	const Vec3 playerDir = moveState.aimDirection;
	const Vec3 playerPos = moveState.eyePosition;
	const float range = g_pGameCVars->pl_melee.melee_snap_target_select_range;
	const float angleLimit = cos_tpl(DEG2RAD(g_pGameCVars->pl_melee.melee_snap_angle_limit));

	return m_collisionHelper.GetBestAutoAimTargetForUser(pOwnerActor->GetEntityId(), playerPos, playerDir, range, angleLimit);
}

//-----------------------------------------------------------------------
void CMelee::OnMeleeHitAnimationEvent()
{
	if( IsMeleeWeapon() && m_hitStatus == EHitStatus_HaveHitResult )
	{
		ApplyMeleeDamageHit( m_lastCollisionTest, m_lastRayHit );

		m_hitStatus = EHitStatus_Invalid;

		m_lastRayHit.pCollider->Release();
		m_lastRayHit.pCollider = NULL;
	}
	else
	{
		m_hitStatus= EHitStatus_ReceivedAnimEvent;
	}
}

//-----------------------------------------------------------------------
void CMelee::GetMemoryUsage( ICrySizer * s ) const
{
	s->AddObject(this, sizeof(*this));
}

void CMelee::ApplyMeleeDamageHit( const SCollisionTestParams& collisionParams, const ray_hit& hitResult )
{
	IEntity* pCollidedEntity = gEnv->pEntitySystem->GetEntityFromPhysics(hitResult.pCollider);
	EntityId collidedEntityId = pCollidedEntity ? pCollidedEntity->GetId() : 0;

	ApplyMeleeDamage(hitResult.pt, collisionParams.m_dir, hitResult.n, hitResult.pCollider, collidedEntityId,
		hitResult.partid, hitResult.ipart, hitResult.surface_idx, collisionParams.m_remote, hitResult.iPrim);
}

void CMelee::OnSuccesfulHit( const ray_hit& hitResult )
{
	CActor* pOwner = m_pWeapon->GetOwnerActor();
	if( IsMeleeWeapon() && m_hitStatus != EHitStatus_ReceivedAnimEvent && hitResult.pCollider )
	{
		// we defer the MeleeDamage until the MeleeHitEvent is received!
		m_lastCollisionTest = m_collisionHelper.GetCollisionTestParams();
		m_lastRayHit = hitResult;

		m_lastRayHit.pCollider->AddRef();
		m_hitStatus = EHitStatus_HaveHitResult;
	}
	else
	{
		const SCollisionTestParams& collisionParams = m_collisionHelper.GetCollisionTestParams();
		
		ApplyMeleeDamageHit( collisionParams, hitResult );

		m_hitStatus = EHitStatus_Invalid;

		if(m_pMeleeAction)
		{
			if(pOwner)
			{
				m_pMeleeAction->OnHitResult(pOwner, true);
			}
			else
			{
				//Owner has dropped weapon (Likely from being killed) so we can stop and release the action
				m_pMeleeAction->ForceFinish();
				SAFE_RELEASE(m_pMeleeAction);
			}
		}
	}

	if(pOwner && pOwner->IsClient())
	{
		s_meleeSnapTargetId = 0;
		CHANGED_NETWORK_STATE(pOwner, CPlayer::ASPECT_SNAP_TARGET);

		if(g_pGameCVars->pl_melee.mp_melee_system_camera_lock_and_turn)
		{
			pOwner->GetActorParams().viewLimits.ClearViewLimit(SViewLimitParams::eVLS_Item);
		}
	}
}

void CMelee::OnFailedHit()
{
	const SCollisionTestParams& collisionParams = m_collisionHelper.GetCollisionTestParams();

	bool collided = PerformCylinderTest(collisionParams.m_pos, collisionParams.m_dir, collisionParams.m_remote);

	CActor* pOwner = m_pWeapon->GetOwnerActor();

	if(pOwner && pOwner->IsClient())
	{
		if(!collided && s_meleeSnapTargetId)
		{
			Vec3 ownerpos = pOwner->GetEntity()->GetWorldPos();
			IEntity* pTarget = gEnv->pEntitySystem->GetEntity(s_meleeSnapTargetId);

			if(pTarget && ownerpos.GetSquaredDistance(pTarget->GetWorldPos()) < sqr(GetRange() * m_pMeleeParams->meleeparams.target_range_mult))
			{
				collided = m_collisionHelper.PerformMeleeOnAutoTarget(s_meleeSnapTargetId);
			}
		}

		s_meleeSnapTargetId = 0;
		CHANGED_NETWORK_STATE(pOwner, CPlayer::ASPECT_SNAP_TARGET);

		if(g_pGameCVars->pl_melee.mp_melee_system_camera_lock_and_turn)
		{
			pOwner->GetActorParams().viewLimits.ClearViewLimit(SViewLimitParams::eVLS_Item);
		}
	}

	if(m_pMeleeAction)
	{
		if(pOwner)
		{
			m_pMeleeAction->OnHitResult(pOwner, collided);
		}
		else
		{
			//Owner has dropped weapon (Likely from being killed) so we can stop and release the action
			m_pMeleeAction->ForceFinish();
			SAFE_RELEASE(m_pMeleeAction);
		}
	}
	
	ApplyMeleeEffects(collided);
}

float CMelee::GetRange() const
{
	if (m_slideKick)
		return 2.75f;

	const SMeleeParams& meleeParams = m_pMeleeParams->meleeparams;
	return m_shortRangeAttack ? meleeParams.closeAttack.range : meleeParams.range;
}

bool CMelee::DoSlideMeleeAttack( CActor* pOwnerActor )
{
	if (pOwnerActor && pOwnerActor->GetActorClass() == CPlayer::GetActorClassType())
	{
		CPlayer* pOwnerPlayer = static_cast<CPlayer*>(pOwnerActor);
		const bool canDoKick = pOwnerPlayer->CanDoSlideKick();
		return canDoKick;
	}
	return false;
}

void CMelee::CloseRangeAttack(bool closeRangeAttack)
{
	m_shortRangeAttack = closeRangeAttack;
}

float CMelee::GetDuration() const
{
	if( IsMeleeWeapon() )
	{
		return m_pWeapon->GetCurrentAnimationTime( eIGS_Owner )*0.001f;
	}
	if (!m_shortRangeAttack)
	{
		return m_pMeleeParams->meleeparams.duration;
	}
	else
	{
		return m_pMeleeParams->meleeparams.closeAttack.duration;
	}
}

float CMelee::GetDelay() const
{
	if (!m_shortRangeAttack)
	{
		CActor* pOwner = m_pWeapon->GetOwnerActor();
		if (pOwner && pOwner->IsPlayer())
			return m_pMeleeParams->meleeparams.delay;
		else
			return m_pMeleeParams->meleeparams.aiDelay;
	}
	else
	{
		return m_pMeleeParams->meleeparams.closeAttack.delay;
	}
}

float CMelee::GetImpulseAiToPlayer() const
{
	return m_shortRangeAttack ? m_pMeleeParams->meleeparams.closeAttack.impulse_ai_to_player : m_pMeleeParams->meleeparams.impulse_ai_to_player;
}

bool CMelee::IsMeleeFilteredOnEntity( const IEntity& targetEntity ) const
{
	static IEntityClass* s_pAnimObjectClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("AnimObject");

	if (targetEntity.GetClass() == s_pAnimObjectClass)
	{
		if (IScriptTable* pEntityScript = targetEntity.GetScriptTable())
		{
			SmartScriptTable properties;
			if (pEntityScript->GetValue("Properties", properties))
			{
				SmartScriptTable physicProperties;
				if (properties->GetValue("Physics", physicProperties))
				{
					bool bulletCollisionEnabled = true;
					return (physicProperties->GetValue("bBulletCollisionEnabled", bulletCollisionEnabled) && !bulletCollisionEnabled);
				}
			}
		}
	}

	return false;
}

bool CMelee::SwitchToMeleeWeaponAndAttack()
{
	// Checking for the equipped MeleeWeapon for each melee is hateful, but there's nowhere to check the inventory when a weapon is equipped.
	CActor* pOwner = m_pWeapon->GetOwnerActor();
	if( pOwner && (pOwner->GetInventory()->GetItemByClass( CItem::sWeaponMeleeClass ) != 0) )
	{
		m_pWeapon->SetPlaySelectAction( false );
		pOwner->SelectItemByName( "WeaponMelee", true );

		// trigger the attack - should be good without ptr checks, we validated that the weapon exists in the inventory.
		pOwner->GetCurrentItem()->GetIWeapon()->MeleeAttack();

		return true;
	}

	return false;
}

float CMelee::GetMeleeDamage() const
{
	return m_pMeleeParams->meleeparams.damage;
}

bool CMelee::StartMultiAnimMeleeAttack(CActor* pActor)
{
	if(pActor)
	{
		m_hitTypeID = CGameRules::EHitType::Melee;

		const IActionController* pActionController = pActor->GetAnimatedCharacter()->GetActionController();
		const CTagDefinition& fragmentIDs = pActionController->GetContext().controllerDef.m_fragmentIDs;
		const SMeleeActions& actions = m_pMeleeParams->meleeactions;

		if(m_pMeleeAction)
		{
			m_pMeleeAction->NetEarlyExit(); //Cancel the old one before we start the new one
		}

		int introID = fragmentIDs.Find(actions.attack_multipart.c_str());
		if(introID != FRAGMENT_ID_INVALID)
		{
			m_pMeleeAction = new CMeleeAction(PP_PlayerActionUrgent, m_pWeapon, introID);
			m_pMeleeAction->AddRef();
			pActor->GetAnimatedCharacter()->GetActionController()->Queue(*m_pMeleeAction);
			m_attackTurnAmount = 0.f;
			return true;
		}
	}

	return false; 
}

float CMelee::GetImpulseStrength()
{
	return m_pMeleeParams->meleeparams.impulse_actor;
}

//////////////////////// CMeleeAction ////////////////////////

CMelee::CMeleeAction::CMeleeAction( int priority, CWeapon* pWeapon, FragmentID fragmentID)
	: BaseClass(priority, fragmentID, TAG_STATE_EMPTY, IAction::NoAutoBlendOut), m_weaponId(pWeapon->GetEntityId()), m_FinalStage(false), m_bCancelled(false)
{
	const SMannequinPlayerParams::Fragments::Smelee_multipart& meleeFragment = PlayerMannequin.fragments.melee_multipart;

	const CTagDefinition* pFragTagDef = meleeFragment.pTagDefinition;
	if(pFragTagDef)
	{
		CTagState fragTagState(*pFragTagDef, m_fragTags);

		pWeapon->SetFragmentTags(fragTagState);

		m_fragTags = fragTagState.GetMask();

		pFragTagDef->Set(m_fragTags, meleeFragment.fragmentTagIDs.into, true);
	}
}

void CMelee::CMeleeAction::Exit()
{
	if(!m_bCancelled)
	{
		StopAttackAction();
	}

	BaseClass::Exit();
}

void CMelee::CMeleeAction::OnHitResult( CActor* pOwnerActor, bool hit )
{
	CRY_ASSERT(pOwnerActor);

	const SMannequinPlayerParams::Fragments::Smelee_multipart& meleeFragment = PlayerMannequin.fragments.melee_multipart;

	const CTagDefinition* pFragTagDef = meleeFragment.pTagDefinition;
	if(pFragTagDef)
	{
		pFragTagDef->Set(m_fragTags, hit ? meleeFragment.fragmentTagIDs.hit : meleeFragment.fragmentTagIDs.miss, true);

		// In TP, force record the tag change so that the FP KillCam replay looks correct.
		if(pOwnerActor->IsThirdPerson() && GetStatus()==Installed)
		{
			if(CRecordingSystem* pRecSys = g_pGame->GetRecordingSystem())
			{
				uint32 optionIdx = GetOptionIdx();
				if (optionIdx == OPTION_IDX_RANDOM)
				{
					SetOptionIdx(GetContext().randGenerator.GenerateUint32());
				}
				pRecSys->OnMannequinRecordHistoryItem( SMannHistoryItem(GetForcedScopeMask(), PlayerMannequin.fragmentIDs.melee_multipart, m_fragTags, GetOptionIdx(), false), GetRootScope().GetActionController(), GetRootScope().GetEntityId() );
			}
		}
	}

	SetFragment(PlayerMannequin.fragmentIDs.melee_multipart, m_fragTags, m_optionIdx, m_userToken, false);

	m_FinalStage = true;
	m_flags &= ~IAction::NoAutoBlendOut;
}

void CMelee::CMeleeAction::NetEarlyExit()
{
	m_eStatus = IAction::Finished;
	m_flags &= ~(IAction::Interruptable | IAction::NoAutoBlendOut);

	StopAttackAction();
	m_bCancelled = true;
}

void CMelee::CMeleeAction::StopAttackAction()
{
	if( CItem* pItem = static_cast<CItem*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(m_weaponId)) )
	{
		CWeapon* pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon());
		CMelee* pMelee = pWeapon->GetMelee();
		if(pMelee)
		{
			StopAttackingAction action(pMelee);
			action.execute(pItem);
		}
	}
}
