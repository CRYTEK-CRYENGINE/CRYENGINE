// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
*************************************************************************/

#include "StdAfx.h"
#include "PickAndThrowWeapon.h"

#include "Game.h"
#include "GameActions.h"
#include "GameInputActionHandlers.h"
#include "Actor.h"
#include "Player.h"
#include "WeaponSharedParams.h"
#include "AI/Agent.h"
#include "GameRules.h"
#include "AutoAimManager.h"
#include <CryAISystem/ITargetTrackManager.h>
#include <IVehicleSystem.h>
#include <CryAISystem/ICommunicationManager.h>
#include "AI/GameAISystem.h"
#include "Network/Lobby/GameAchievements.h"
#include "PickAndThrowProxy.h"
#include "EnvironmentalWeapon.h"
#include "RecordingSystem.h"

#include "PlayerPlugin_Interaction.h"

#include "EntityUtility/EntityScriptCalls.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "PlayerVisTable.h"
#include "ScreenEffects.h"

#include "ItemAnimation.h"

#include "PersistantStats.h"
#include "StatsRecordingMgr.h"
#include "Utility/AttachmentUtils.h"
#include "Mannequin/Serialization.h"

#include <IPerceptionManager.h>

#ifdef PICKANDTHROWWEAPON_DEBUGINFO
#include "Utility/CryWatch.h"

	#define PTLogThrow(...)		{if(g_pGameCVars->pl_pickAndThrow.chargedThrowDebugOutputEnabled){CryLogAlways("[PICKTHROW] [THROW] " __VA_ARGS__);}}
	#define PTLogCombo(...)		{if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponComboDebugEnabled){CryLogAlways("[PICKTHROW] [COMBO] " __VA_ARGS__);}}
#else
	#define PTLogThrow(...)		{}
	#define PTLogCombo(...)		{}
#endif //PICKANDTHROWWEAPON_DEBUGINFO



//////////////////////////////////////////////////////////////////////////
/// Utilities
//////////////////////////////////////////////////////////////////////////

namespace PickAndThrow
{
	CEnvironmentalWeapon* GetEnviromentalWeaponForEntity( const EntityId entityId )
	{
		return static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(entityId, "EnvironmentalWeapon"));
	}

	void EnviromentalWeaponEnableIgnore( const CPlayer* pPlayer, const EntityId entityId )
	{
		if ((pPlayer != NULL) && pPlayer->IsClient())
		{
			CPlayerVisTable* pVisTable = g_pGame->GetPlayerVisTable();
			if(pVisTable)
			{
				pVisTable->AddGlobalIgnoreEntity( entityId, "EnviromentalWeaponUsed" ); 
			}
		}
	}

	void EnviromentalWeaponDisableIgnore( const CPlayer* pPlayer , EntityId entityId )
	{
		if ((pPlayer != NULL) && pPlayer->IsClient())
		{
			CPlayerVisTable* pVisTable = g_pGame->GetPlayerVisTable();
			if(pVisTable)
			{
				pVisTable->RemoveGlobalIgnoreEntity( entityId );  
			}
		}
	}

	void NotifyRecordingSystemOnUsed( const EntityId playerId, const EntityId pickAndThrowId, const EntityId enviromentEntityId, const bool bPickedUp )
	{
		CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			pRecordingSystem->OnPickAndThrowUsed(playerId, pickAndThrowId, enviromentEntityId, bPickedUp);
		}
	}
} // namespace PickAndThrow

//////////////////////////////////////////////////////////////////////////
/// SCHEDULER ACTIONS
//////////////////////////////////////////////////////////////////////////
struct CPickAndThrowWeapon::DoSimpleMelee
{
	DoSimpleMelee(CPickAndThrowWeapon& pickAndThrowWeapon)
		: m_pickAndThrowWeapon(pickAndThrowWeapon)
	{

	}

	void execute(CItem *_this)
	{
		m_pickAndThrowWeapon.OnDoSimpleMelee();
	}

private:
	CPickAndThrowWeapon& m_pickAndThrowWeapon;

};

struct CPickAndThrowWeapon::FinishSimpleMelee
{
	FinishSimpleMelee(CPickAndThrowWeapon& pickAndThrowWeapon)
		: m_pickAndThrowWeapon(pickAndThrowWeapon)
	{

	}

	void execute(CItem *_this)
	{
		m_pickAndThrowWeapon.OnSimpleMeleeFinished();
	}

private:
	CPickAndThrowWeapon& m_pickAndThrowWeapon;

};

struct CPickAndThrowWeapon::FinishComplexMelee
{
	FinishComplexMelee(CPickAndThrowWeapon& pickAndThrowWeapon)
		: m_pickAndThrowWeapon(pickAndThrowWeapon)
	{

	}

	void execute(CItem *_this)
	{
		m_pickAndThrowWeapon.OnComplexMeleeFinished();
	}

private:
	CPickAndThrowWeapon& m_pickAndThrowWeapon;

};

struct CPickAndThrowWeapon::DelayedMeleeImpulse
{
	DelayedMeleeImpulse(CPickAndThrowWeapon& pickAndThrow, EntityId collidedEntityId, const Vec3& impulsePos, const Vec3& impulse, int partId, int ipart, int hitTypeID)
		: m_pickAndThrow(pickAndThrow)
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
			m_pickAndThrow.m_collisionHelper.Impulse(pPhysicalEntity, m_impulsePos, m_impulse, m_partId, m_ipart, m_hitTypeID);
		}
	}

private:
	CPickAndThrowWeapon& m_pickAndThrow;
	EntityId m_collidedEntityId;
	Vec3 m_impulsePos;
	Vec3 m_impulse;
	int m_partId;
	int m_ipart;
	int m_hitTypeID;
};

struct CPickAndThrowWeapon::DelayedNPCRagdollThrow
{
	DelayedNPCRagdollThrow( const EntityId entityId, const Vec3& velocity )
		: m_entityId( entityId )
		, m_velocity( velocity )
	{

	}

	void execute(CItem *pItem)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_entityId);
		IPhysicalEntity* pPhysicalEntity = pEntity ? pEntity->GetPhysics() : NULL;

		if ((pPhysicalEntity != NULL) && (pPhysicalEntity->GetType() == PE_ARTICULATED))
		{
			pe_action_set_velocity actionVelocity;
			actionVelocity.v	= m_velocity;
			pPhysicalEntity->Action( &actionVelocity );

			// Set the special collision flag for the pick and throw proxy
			// This will improve the ragdoll-to-livingEntity collision response by making sure the 
			// ragdoll collides with the proxy, which has mass 0 and is "unpushable" and also have
			// a shape that avoids the ragdoll to slide under the living entity (therefore avoiding
			// the AI uglily stepping over the ragdoll when that happens)
			if (g_pGameCVars->pl_pickAndThrow.useProxies)
			{
				pe_params_part colltype;
				colltype.flagsColliderOR = CPickAndThrowProxy::geom_colltype_proxy;

				pPhysicalEntity->SetParams( &colltype );
			}
		}
	}

private:
	EntityId m_entityId;
	Vec3     m_velocity;
};

//////////////////////////////////////////////////////////////////////////
/// MANNEQUIN ACTIONS/PROCEDURAL CLIPS
//////////////////////////////////////////////////////////////////////////

#include "PlayerAnimation.h"

class CActionGrab : public TPlayerAction
{
public:

	DEFINE_ACTION("Grab");

	typedef TAction<SAnimationContext> BaseClass;

	CActionGrab(FragmentID fragID, const TagState &fragTags, CPlayer &player, IEntity &target, const bool useFixedRotation, const bool animationControlled)
		: TPlayerAction(PP_PlayerActionUrgent, fragID, fragTags),
		m_player(player),
		m_target(target),
		m_useFixedRotation(useFixedRotation),
		m_playerAnimationControlled(animationControlled)
	{
		m_targetActor = (CActor*)g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(target.GetId());
	}

	virtual void Enter() override
	{
		TPlayerAction::Enter();

		Vec3 originPosition; 
		Vec3 originForwardDirection;
		Vec3 targetPosition; 
		if(m_playerAnimationControlled)
		{
			originPosition = m_player.GetEntity()->GetWorldPos();
			originForwardDirection = m_player.GetEntity()->GetForwardDir();
			targetPosition = m_target.GetWorldPos();
			targetPosition.z = originPosition.z;
		}
		else
		{
			originPosition = m_target.GetWorldPos();
			originForwardDirection = m_target.GetForwardDir();
			targetPosition = m_player.GetEntity()->GetWorldPos();
			originPosition.z = targetPosition.z;
		}

		if(m_useFixedRotation)
		{
			m_targetViewDirection = m_target.GetWorldRotation() * Quat::CreateRotationZ(gf_PI);
		}
		else
		{
			Vec3 offset = targetPosition-originPosition;
			offset.NormalizeSafe(originForwardDirection);
			m_targetViewDirection = Quat::CreateRotationVDir(offset);
		}

		SetParam("TargetPos", QuatT(targetPosition,m_targetViewDirection));

		if(m_playerAnimationControlled)
		{
			IAnimatedCharacter* pAnimatedCharacterPlayer				= m_player.GetAnimatedCharacter();
			if( pAnimatedCharacterPlayer )
			{
				pAnimatedCharacterPlayer->SetMovementControlMethods(eMCM_Animation, eMCM_Animation);
				pAnimatedCharacterPlayer->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "CActionGrab::Enter()");
			}

			PlayerCameraAnimationSettings cameraAnimationSettings;
			cameraAnimationSettings.positionFactor = 1.0f;
			cameraAnimationSettings.rotationFactor = 1.0f;
			m_player.PartialAnimationControlled( true, cameraAnimationSettings );
			m_player.GetActorStats()->animationControlledID = m_target.GetId();
		}

		IAnimatedCharacter* pAnimatedCharacterTarget = m_targetActor ? m_targetActor->GetAnimatedCharacter() : NULL;
		if (pAnimatedCharacterTarget)
		{
			pAnimatedCharacterTarget->SetMovementControlMethods(eMCM_Animation, eMCM_Animation);
			pAnimatedCharacterTarget->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "CActionGrab::Enter()");
		}

	}

	virtual void Exit() override
	{
		TPlayerAction::Exit();

		if(m_playerAnimationControlled)
		{
			IAnimatedCharacter* pAnimatedCharacterPlayer = m_player.GetAnimatedCharacter();
			if( pAnimatedCharacterPlayer )
			{
				pAnimatedCharacterPlayer->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
				pAnimatedCharacterPlayer->ForceRefreshPhysicalColliderMode();
				pAnimatedCharacterPlayer->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "CActionGrab::Exit()");
			}

			m_player.PartialAnimationControlled( false, PlayerCameraAnimationSettings() );
			m_player.GetActorStats()->animationControlledID = 0;
		}
		
		IAnimatedCharacter* pAnimatedCharacterTarget = m_targetActor ? m_targetActor->GetAnimatedCharacter() : NULL;
		if (pAnimatedCharacterTarget)
		{
			pAnimatedCharacterTarget->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
			pAnimatedCharacterTarget->ForceRefreshPhysicalColliderMode();
			pAnimatedCharacterTarget->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "CActionGrab::Exit()");
		}

		IItem *pCurrentPlayerItem =  m_player.GetCurrentItem();
		if ((pCurrentPlayerItem != NULL) && !stricmp(pCurrentPlayerItem->GetType(), CPickAndThrowWeapon::GetWeaponComponentType()))
		{
			CPickAndThrowWeapon *pPickAndThrowWeapon = (CPickAndThrowWeapon *)pCurrentPlayerItem;
			pPickAndThrowWeapon->OnEndMannequinGrabAction();
		}
	}

	virtual EStatus Update(float timePassed) override
	{
		TPlayerAction::Update(timePassed);

		if (m_playerAnimationControlled)
		{
			//--- Update view direction
			const float TARGET_VIEW_SMOOTH_TIME_INV = (1.0f / 0.5f);
			const float blendFactor = min(m_activeTime * TARGET_VIEW_SMOOTH_TIME_INV, 1.0f);
			m_player.SetViewRotation( Quat::CreateSlerp( m_player.GetViewRotation(), m_targetViewDirection, blendFactor ) );
		}

		return m_eStatus;
	}

private:

	CPlayer	&m_player;
	IEntity &m_target;
	CActor  *m_targetActor;

	Quat     m_targetViewDirection;

	bool m_useFixedRotation;
	bool m_playerAnimationControlled;
};

class CActionPNTMelee : public CItemAction
{
public:

	DEFINE_ACTION("PNTMelee");

	CActionPNTMelee(CPickAndThrowWeapon &pntWeapon, const TagState& fragTags )
		: CItemAction(PP_PlayerAction, PlayerMannequin.fragmentIDs.melee, fragTags )
		, m_pntWeapon(pntWeapon)
	{
	}

	void Exit() override
	{
		CItemAction::Exit();

		m_pntWeapon.MeleeFinished();
	}

private:
	CPickAndThrowWeapon &m_pntWeapon;
};

struct SAttachPnTParams : public IProceduralParams
{
	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(Serialization::Decorators::AttachmentName(attachmentPt), "AttachmentPt", "Attachment Point");
	}
	
	TProcClipString attachmentPt;
};

class CProceduralClipAttachPnT : public TProceduralClip<SAttachPnTParams>
{
public:
	CProceduralClipAttachPnT()
		: m_pAttachment(NULL)
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SAttachPnTParams &params)
	{
		IEntity &rootEntity = m_scope->GetActionController().GetEntity();

		CActor* pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor( rootEntity.GetId() ));

		if (!pActor)
		{
			ICharacterInstance *pCharacter = rootEntity.GetCharacter(0);
			IAttachment *pAttachment = m_pAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName(params.attachmentPt.c_str());

			if (pAttachment)
			{
				CEntityAttachment *pEntityAttachment = new CEntityAttachment();
				pEntityAttachment->SetEntityId( m_scope->GetEntityId() );
				pAttachment->AddBinding( pEntityAttachment );
				pAttachment->HideAttachment(0);

				QuatT requiredRelativeLocation;

				m_attachmentOldRelativeLoc = pAttachment->GetAttRelativeDefault();

				requiredRelativeLocation.SetIdentity();
				pAttachment->SetAttRelativeDefault(requiredRelativeLocation);
			}
		}
		else
		{
			IItem *pItem =  pActor->GetCurrentItem();

			if (pItem && !stricmp(pItem->GetType(), CPickAndThrowWeapon::GetWeaponComponentType()))
			{
				CPickAndThrowWeapon *pPnTWeapon = (CPickAndThrowWeapon *)pItem;
				pPnTWeapon->QuickAttach();
			}
		}
	}
	virtual void OnExit(float blendTime)
	{
		if (m_pAttachment)
		{
			m_pAttachment->SetAttRelativeDefault(m_attachmentOldRelativeLoc);
			m_pAttachment->ClearBinding();
		}
	}

	virtual void Update(float timePassed)
	{
	}

private:
	QuatT m_attachmentOldRelativeLoc;
	IAttachment *m_pAttachment;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipAttachPnT, "AttachPnT");

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------
CPickAndThrowWeapon::CPickAndThrowWeapon()
: m_constraintId(0)
, m_constraintId2(0)
, m_timePassed(0)
, m_state( eST_UNKNOWN )
, m_framesToCheckConstraint( 0 )
, m_objectCollisionsLimited( false )
, m_objectGrabbed( false )
, m_objectId( 0 )
, m_netInitObjectId( 0 )
, m_objectScale( 1, 1, 1 )
, m_grabType( 0 )
, m_currentMeleeAutoTargetId(0)
, m_objectMass(0.0f)
, m_meleeRange(1.5f)
, m_animDuration(0.f)
, m_bSerializeObjectGrabbed(false)
, m_isNPC(false)
, m_hitsTakenByNPC(0)
, m_heldEntityInteractionType( eInteraction_None )
, m_RequestedThrowState(eST_UNKNOWN)
, m_throwCounter(0)
, m_chargedThrowState(eCTS_INACTIVE)
, m_postMeleeActionCooldown(0.0f)
, m_currentMeleeAnimationId(0)
, m_attackTimeSmoothRate(0.0f)
, m_attackTime(0.0f)
, m_logicFlags(ELogicFlag_None)
{
	m_infiniteViewEntities.reserve(5);
	CGameInputActionHandlers::TPickAndThrowWeaponActionHandler& pickAndThrowActionHandler = g_pGame->GetGameInputActionHandlers().GetCPickAndThrowWeaponActionHandler();
	
	if(pickAndThrowActionHandler.GetNumHandlers() == 0)
	{
		pickAndThrowActionHandler.AddHandler(g_pGame->Actions().use, &CPickAndThrowWeapon::OnActionUse);
		pickAndThrowActionHandler.AddHandler(g_pGame->Actions().drop, &CPickAndThrowWeapon::OnActionDrop);
		pickAndThrowActionHandler.AddHandler(g_pGame->Actions().toggle_weapon, &CPickAndThrowWeapon::OnActionDrop);
		pickAndThrowActionHandler.AddHandler(g_pGame->Actions().nextitem, &CPickAndThrowWeapon::OnActionDrop);
		pickAndThrowActionHandler.AddHandler(g_pGame->Actions().previtem, &CPickAndThrowWeapon::OnActionDrop);

		if(gEnv->bMultiplayer)
		{
			pickAndThrowActionHandler.AddHandler(g_pGame->Actions().attack1_xi, &CPickAndThrowWeapon::OnActionChargedThrow );	// throw object (if xml params allow)
			pickAndThrowActionHandler.AddHandler(g_pGame->Actions().special, &CPickAndThrowWeapon::OnActionMelee);				// primary attack (can be combo'd)
		}
		else
		{
			pickAndThrowActionHandler.AddHandler(g_pGame->Actions().attack1, &CPickAndThrowWeapon::OnActionAttack);	//throw
			pickAndThrowActionHandler.AddHandler(g_pGame->Actions().attack2_xi, &CPickAndThrowWeapon::OnActionAttack);	// throw
			pickAndThrowActionHandler.AddHandler(g_pGame->Actions().special, &CPickAndThrowWeapon::OnActionMelee); // attack 
		}
	}
	m_attachmentOldRelativeLoc.SetIdentity();
	m_collisionHelper.SetUser(this);	
}


//---------------------------------------------------------------------------
CPickAndThrowWeapon::~CPickAndThrowWeapon()
{
	//make sure we're not registered in AI visibility tracking (happens when thrown)
	ICVar* pEnableAI = gEnv->pConsole->GetCVar("sv_AISystem");
	if(!gEnv->bMultiplayer || (pEnableAI && pEnableAI->GetIVal()))
	{
		g_pGame->GetGameAISystem()->GetVisibleObjectsHelper().UnregisterObject( m_objectId );
	}

	ResetTrackingOfExternalEntities();

	if (gEnv->pEntitySystem->GetEntity( GetOwnerId() ))
	{
		ResetInternal();	
	}
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::Reset()
{
	inherited::Reset();
	ResetTrackingOfExternalEntities();
	ResetInternal();
}

//////////////////////////////////////////////////////////////////////////

void CPickAndThrowWeapon::ResetInternal()
{
	if (m_state != eST_UNKNOWN)
	{
		const bool isNPC = IsNPC();
		if (isNPC)
		{
			CActor *pActorNPC = GetNPCGrabbedActor();
			if (pActorNPC != NULL)
			{
				pActorNPC->SetGrabbedByPlayer(GetOwner(), false);
			}
			EnslaveTarget(false);
		}
	
		if (m_objectGrabbed)
		{
			if (isNPC)
			{
				PrepareNPCToBeDropped();
			}	
			DropObject();
		}
		
		const EntityId killObjectId = (gEnv->bMultiplayer && (m_state==eST_GOINGTOPICK || m_state==eST_PICKING)) ? m_objectId : 0;

		m_state = eST_UNKNOWN;

		CPlayer* pOwnerPlayer = GetOwnerPlayer();

		if(pOwnerPlayer)
		{
			pOwnerPlayer->ExitPickAndThrow();
		}
		/////////////

		if (m_objectCollisionsLimited)
			SetUpCollisionPropertiesAndMassInfo( false );

		AttachmentUtils::SetEntityRenderInNearestMode( GetEntityObject(), false );
		
		CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
		if(pEnvWeapon)
		{
			pEnvWeapon->SetOwner(0);

			PickAndThrow::EnviromentalWeaponDisableIgnore( pOwnerPlayer, m_objectId );
		}

		m_currentMeleeAutoTargetId = 0;
		m_objectMass = 0.0f;
		m_meleeRange = 1.5f;
		m_hitsTakenByNPC = 0;
		m_isNPC = false;
		m_objectHelperSlot = 0;
		if (m_objectId != 0)
		{
			GetGameObject()->SetNetworkParent(0);
			m_objectId = 0;
		}

		PowerThrowPrepareSound(false);

		if(killObjectId)
		{
			gEnv->pEntitySystem->RemoveEntity(killObjectId);
		}
	}

	m_obstructionChecker.Reset();
	m_attackIndex = -1; 
	m_chargedThrowState = eCTS_INACTIVE;
	m_logicFlags &= ~ELogicFlag_OrientationCorrectionInitialised; 
	m_RequestedThrowState = eST_UNKNOWN;
	m_currentMeleeAnimationId = 0;
	m_correctOrientationParams.Reset(); 
}

//---------------------------------------------------------------------------
bool CPickAndThrowWeapon::Init(IGameObject * pGameObject)
{
	if(!inherited::Init(pGameObject))
	{
		return false;
	}

	if(!m_weaponsharedparams->pPickAndThrowParams)
	{
		GameWarning("Uninitialised pick and throw params. Is the xml for item %s missing the 'PickAndThrowParams' node?", GetEntity()->GetName());

		return false;
	}
	
	return true;
}

//---------------------------------------------------------------------------
const char* CPickAndThrowWeapon::GetWeaponComponentType()
{
	return "CPickAndThrowWeapon";
}

//---------------------------------------------------------------------------
const char* CPickAndThrowWeapon::GetType() const
{
	return GetWeaponComponentType();
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::Select(bool select)
{
	CPlayer* pOwnerPlayer = GetOwnerPlayer();
	if (select)
	{
		if (m_flyingActorInfo.valid)
		{
			RestoreObjectPhysics( m_flyingActorInfo.entityId, true );
			m_flyingActorInfo.valid = false;
		}

		m_timePassed = 0;
		m_objectId = 0;

		if(pOwnerPlayer)
		{
			m_objectId = pOwnerPlayer->GetPickAndThrowEntity();

			if (m_objectId == 0)
			{
				m_objectId = m_netInitObjectId;
				m_netInitObjectId = 0;
			}
		}

		DecideGrabType();

		if(IsOwnerClient())
		{
			SHUDEventWrapper::ForceCrosshairType( this, (ECrosshairTypes)GetGrabTypeParams().crossHairType );
		}

		GetGameObject()->SetNetworkParent(m_objectId);
	}

	CWeapon::Select(select);
}

void CPickAndThrowWeapon::EnslaveTarget(bool enslave)
{
	CActor *pOwnerActor = GetOwnerActor();

	if (pOwnerActor)
	{
		IActionController *pActionController = pOwnerActor->GetAnimatedCharacter()->GetActionController();

		if (pActionController)
		{
			if (IsNPC())
			{
				IActor *pTargetActor		= g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_objectId);

				if (pTargetActor)
				{
					// Stop any hit reaction before enslaving
					if (enslave)
					{
						pTargetActor->HandleEvent( SGameObjectEvent( eCGE_ReactionEnd, eGOEF_ToExtensions ) );
					}

					IEntity *pTargetEntity= pTargetActor->GetEntity();
					IAnimatedCharacter *pAnimChar = pTargetActor->GetAnimatedCharacter();
					IActionController *pTargetActionController = pAnimChar->GetActionController();

					const IAnimationDatabase *pAnimDB = NULL;
					if (!GetGrabTypeParams().dbaFile.empty())
					{
						IMannequin &mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
						pAnimDB = mannequinSys.GetAnimationDatabaseManager().Load(GetGrabTypeParams().dbaFile.c_str());
					}

					if (pTargetActionController)
					{
						pActionController->SetSlaveController(*pTargetActionController, PlayerMannequin.contextIDs.SlaveChar, enslave, pAnimDB);
					}
					else
					{
						if (enslave)
						{
							if (pAnimDB)
							{
								pActionController->SetScopeContext(PlayerMannequin.contextIDs.SlaveChar, *pTargetEntity, pTargetEntity->GetCharacter(0), pAnimDB);
							}
						}
						else
						{
							pActionController->ClearScopeContext(PlayerMannequin.contextIDs.SlaveChar);
						}
					}
				}
			}
		}
	}
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::OnSelected(bool selected)
{
	CWeapon::OnSelected( selected );

	IActionController *pActionController = GetActionController();
	//////////////////////////////////////////////////////////////////////////
	// Look up for specific 'tag' overrides for this specific object
	IEntity *pTargetEntity = GetEntityObject();
	if(pTargetEntity != NULL)
	{
		const char* pGrabTagOverride = NULL; 
		if(EntityScripts::GetEntityProperty(pTargetEntity, "szGrabAnimTagOverride", pGrabTagOverride) && strlen(pGrabTagOverride) > 0)
		{
			if(pActionController)
			{
				CTagState &tagState = pActionController->GetContext().state;
				uint32 grabTypeCRC  = CCrc32::ComputeLowercase(pGrabTagOverride);
				tagState.SetByCRC(grabTypeCRC, selected);
			}
		}
	}

	const bool isNPC = IsNPC();
	bool fullBodyRootedAction = isNPC;
	const char* pRootedGrabTagOverride = NULL; 
	if( pTargetEntity != NULL )
	{
		if( IScriptTable* pObjectEntityScript = pTargetEntity->GetScriptTable() )
		{
			pObjectEntityScript->GetValue( "IsRooted", fullBodyRootedAction );
			SmartScriptTable properties;
			if (pObjectEntityScript->GetValue("Properties", properties))
			{
				properties->GetValue("szRootedGrabAnimTagOverride", pRootedGrabTagOverride );
			}
		}
	}

	if(pActionController && (fullBodyRootedAction || !selected) && pRootedGrabTagOverride && pRootedGrabTagOverride[0])
	{
		CTagState &tagState = pActionController->GetContext().state;
		uint32 grabTypeCRC  = CCrc32::ComputeLowercase(pRootedGrabTagOverride);
		tagState.SetByCRC(grabTypeCRC, selected);
	}

	if (selected)
	{
		const SPickAndThrowParams::SGrabTypeParams& grabParams					  = GetGrabTypeParams(); 
		const SPickAndThrowParams::SGrabTypeParams::TPropertyFlags& propertyFlags = grabParams.m_propertyFlags; 

		const bool validTargetEntity = (pTargetEntity != NULL);

		int16 actionType = eEVE_Pickup;

		if (IEntityRender *pIEntityRender = validTargetEntity ? pTargetEntity->GetRenderInterface() : NULL)
		{
			const Vec3 pos = pTargetEntity->GetWorldPos();
			AABB aabb;
			pTargetEntity->GetLocalBounds(aabb);
			const float radius = aabb.GetRadius();
			const Vec3 vRadius(radius,radius,radius);
			AABB worldBox(pos-vRadius, pos+vRadius);

			gEnv->p3DEngine->DeleteDecalsInRange(&worldBox, pIEntityRender->GetRenderNode());
		}

		//////////////////////////////////////////////////////////////////////////
		/// Trigger a 'ActionGrab' if the target is an AI or a 'Rooted' object
		if(fullBodyRootedAction && validTargetEntity)
		{
			actionType = eEVE_Ripup;

			CActor *ownerActor = GetOwnerActor();

			if (ownerActor)
			{
				EnslaveTarget(true);

				const CTagDefinition* pTagDefinition = NULL;
				const FragmentID fragmentID = GetFragmentID("grab", &pTagDefinition);

				TagState fragTags = TAG_STATE_EMPTY;
				if (pTagDefinition)
				{
					pTagDefinition->Set(fragTags, pTagDefinition->Find("rooted"), true);
				}

				const bool useFixedOrientation = (grabParams.m_propertyFlags & EPF_grab_orientation_override_localOffset_specified) != 0;
				const bool playerAnimationControlled = !isNPC;
				CActionGrab *pGrabAction = new CActionGrab(fragmentID, fragTags, *GetOwnerPlayer(), *pTargetEntity, useFixedOrientation, playerAnimationControlled);

				float speedOverride = -1.f;
				if(ownerActor->IsPlayer())
				{
					speedOverride = static_cast<CPlayer*>(ownerActor)->GetModifiableValues().GetValue(kPMV_SandboxInteractionSpeedMultiplier);
				}

				PlayFragment(pGrabAction, speedOverride);
				OnStartGrabAction( isNPC ); 
			}
		}
		else if(gEnv->bMultiplayer == false)
		{
			//////////////////////////////////////////////////////////////////////////
			// If not, just trigger a simple pick-up action 
			PlaySimpleGrabAction();
		}
	
		m_state = isNPC ? eST_GRABBINGNPC : eST_GOINGTOPICK;

		RequireUpdate( eIUS_General );

		m_objectGrabbed = m_objectCollisionsLimited = false;
		m_hitsTakenByNPC = 0;

		CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
		if (pEnvWeapon)
		{
			pEnvWeapon->SetVehicleDamageParams(grabParams.vehicleMeleeDamage, grabParams.vehicleThrowDamage, grabParams.vehicleThrowMinVelocity, grabParams.vehicleThrowMaxVelocity);

			PickAndThrow::EnviromentalWeaponEnableIgnore( GetOwnerPlayer(), m_objectId );

			CStatsRecordingMgr* pRecordingMgr = g_pGame->GetStatsRecorder();
			IStatsTracker* pTracker = pRecordingMgr ? pRecordingMgr->GetStatsTracker( GetOwnerActor() ) : NULL;
			if( pTracker )
			{
				int16 health = (int16)pEnvWeapon->GetCurrentHealth();
				pTracker->Event( eGSE_EnvironmentalWeaponEvent, new CEnvironmentalWeaponEvent( pEnvWeapon->GetEntityId(), actionType, health ) );
			}
		}
	}
	else 
	{
		ResetInternal();
	}
}

void CPickAndThrowWeapon::OnStartGrabAction( const bool isGrabbingNpc )
{
	// Environmental weapons want to know when a rip action starts / ends
	CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
	if(pEnvWeapon)
	{
		pEnvWeapon->OnGrabStart();
	}

	if(isGrabbingNpc)
	{
		CPlayer *pOwnerPlayer = GetOwnerPlayer();
		if(pOwnerPlayer != NULL)
		{
			if (CPersistantStats* pPersistantStats = g_pGame->GetPersistantStats())
			{
				pPersistantStats->IncrementClientStats( EIPS_GrabbedEnemies );
			}
		}
	}
}

void CPickAndThrowWeapon::OnEndGrabAction()
{
	// Environmental weapons want to know when a rip action starts / ends
	CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
	if(pEnvWeapon)
	{
		pEnvWeapon->OnGrabEnd();
	}
}


void CPickAndThrowWeapon::PlaySimpleGrabAction()
{
	// Cache these
	const SPickAndThrowParams::SGrabTypeParams& grabParams = GetGrabTypeParams(); 
	if( IEntity *pEntity = GetEntityObject() )
	{
		if( IScriptTable* pObjectEntityScript = pEntity->GetScriptTable() )
		{
			bool bIsRooted = false; 
			if( pObjectEntityScript->GetValue( "IsRooted", bIsRooted ) )
			{
				if(!bIsRooted)
				{
					// Calculate time scaling (if any) and play
					float requiredAnimTimeScaling = -1.0f; 

					float desiredDuration		  = PickAndThrow::SelectAnimDurationOverride(g_pGameCVars->pl_pickAndThrow.environmentalWeaponDesiredGrabAnimDuration, grabParams.anim_durationOverride_Grab);
					if(desiredDuration > 0.0f)
					{
						requiredAnimTimeScaling = CalculateAnimTimeScaling(grabParams.grab_action_part1_alternate, desiredDuration); 
					}

					PlayActionLocal( grabParams.grab_action_part1_alternate, false, eIPAF_Default, requiredAnimTimeScaling );
					return;
				}
			}
		}
	}

	// Standard primary grab action
	float requiredAnimTimeScaling = -1.0f; 

	// If CVAR override not set.. we fall back to XML
	float desiredDuration		  = PickAndThrow::SelectAnimDurationOverride(g_pGameCVars->pl_pickAndThrow.environmentalWeaponDesiredRootedGrabAnimDuration,grabParams.anim_durationOverride_RootedGrab);

	if(desiredDuration > 0.0f)
	{
		requiredAnimTimeScaling = CalculateAnimTimeScaling(grabParams.grab_action_part1, desiredDuration); 
	}
	PlayActionLocal( grabParams.grab_action_part1, false, eIPAF_Default, requiredAnimTimeScaling );
}


//---------------------------------------------------------------------------
void CPickAndThrowWeapon::CalcObjectHelperInverseMat( Matrix34& objectHelperInvMat )
{
	IEntity* pEntity = GetEntityObject();
	
	objectHelperInvMat.SetIdentity();
	if (!pEntity || IsNPC())  // the new system uses always the origin, when it is an NPC
		return;

	SEntitySlotInfo info;
	if (pEntity->GetSlotInfo( m_objectHelperSlot, info ))
	{
		if (info.pStatObj)
		{
			IStatObj::SSubObject* pSObjHelper = PickAndThrow::FindHelperObject( GetGrabHelperName(), m_objectId, m_objectHelperSlot );
			
			if (pSObjHelper)
			{
				objectHelperInvMat = pSObjHelper->tm;

				if(!gEnv->bMultiplayer)
				{
					// this is a way of adjusting scaled objects so they dont block players view if scaled up, or they dont disappear if scaled down. 
					// in any case is not perfect, and designers are encouraged to not use scaled objects for pickandthrow, or at least check them before if they do so.
					AABB box;
					pEntity->GetLocalBounds( box );
					float height = box.GetSize().z;
					float displacementNeeded = ( height * m_objectScale.z ) - height;
			
					objectHelperInvMat.m23 += displacementNeeded;
				}
			}
				
			objectHelperInvMat.InvertFast();
			objectHelperInvMat.ScaleColumn( m_objectScale );
		}
	}
}

CPickAndThrowWeapon::HelperInfo CPickAndThrowWeapon::GlobalFindHelperObject( const EntityId objectId ) const
{
	const int numTypes = m_weaponsharedparams->pPickAndThrowParams->grabTypesParams.size();
	const int activeSlot = PickAndThrow::FindActiveSlot( objectId );

	// 1) Do a full search, but only with the "basic" methods
	int slot = activeSlot;
	for (int i = 0; i < numTypes; ++i)
	{
		HelperInfo helperInfo;
		const SPickAndThrowParams::SGrabTypeParams& grabParams = m_weaponsharedparams->pPickAndThrowParams->grabTypesParams[i];
		if ( !(grabParams.m_propertyFlags & EPF_isNPCType) && ((helperInfo.m_pSubObject = PickAndThrow::FindHelperObject_Basic( grabParams.helper.c_str(), objectId, slot )) != NULL) )
		{
			helperInfo.m_grabTypeIdx = i;
			helperInfo.m_slot = slot;

			return helperInfo;
		}
	}

	if (slot != 0)
	{
		HelperInfo helperInfo;
		slot = 0;
		for (int i = 0; i < numTypes; ++i)
		{
			const SPickAndThrowParams::SGrabTypeParams& grabParams = m_weaponsharedparams->pPickAndThrowParams->grabTypesParams[i];
			if ( !(grabParams.m_propertyFlags & EPF_isNPCType) && ((helperInfo.m_pSubObject = PickAndThrow::FindHelperObject_Basic( grabParams.helper.c_str(), m_objectId, m_objectHelperSlot )) != NULL) )
			{
				helperInfo.m_grabTypeIdx = i;
				helperInfo.m_slot = slot;
				return helperInfo;
			}
		}
	}


	// 2) If not found, repeats everything with "extended" method. TODO: this part should not be needed.
	slot = activeSlot;
	for (int i = 0; i < numTypes; ++i)
	{
		HelperInfo helperInfo;
		const SPickAndThrowParams::SGrabTypeParams& grabParams = m_weaponsharedparams->pPickAndThrowParams->grabTypesParams[i];
		if ( !(grabParams.m_propertyFlags & EPF_isNPCType) && ((helperInfo.m_pSubObject = PickAndThrow::FindHelperObject_Extended( grabParams.helper.c_str(), objectId, slot )) != NULL) )
		{
			helperInfo.m_grabTypeIdx = i;
			helperInfo.m_slot = slot;
			return helperInfo;
		}
	}

	if (slot != 0)
	{
		HelperInfo helperInfo;
		slot = 0;
		for (int i = 0; i < numTypes; ++i)
		{
			const SPickAndThrowParams::SGrabTypeParams& grabParams = m_weaponsharedparams->pPickAndThrowParams->grabTypesParams[i];
			if ( !(grabParams.m_propertyFlags & EPF_isNPCType) && ((helperInfo.m_pSubObject = PickAndThrow::FindHelperObject_Extended( grabParams.helper.c_str(), m_objectId, m_objectHelperSlot )) != NULL) )
			{
				helperInfo.m_grabTypeIdx = i;
				helperInfo.m_slot = slot;
				return helperInfo;
			}
		}
	}


	return HelperInfo();
}


//---------------------------------------------------------------------------
bool CPickAndThrowWeapon::UpdateAimAnims( SParams_WeaponFPAiming &aimAnimParams)
{
	aimAnimParams.shoulderLookParams = &m_sharedparams->params.aimLookParams;
	 
	return true;
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::UpdateTags(const class IActionController *pActionController, class CTagState &tagState, bool selected) const
{
	CWeapon::UpdateTags(pActionController, tagState, selected);

	uint32 tagCrC = CCrc32::ComputeLowercase(GetGrabTypeParams().tag.c_str());
	tagState.SetByCRC(tagCrC, selected);
}

//---------------------------------------------------------------------------
bool CPickAndThrowWeapon::GetAnimGraphInputs( CItem::TempAGInputName &pose, CItem::TempAGInputName &suffix, CItem::TempAGInputName &itemClass )
{
	pose = GetGrabTypeParams().animGraphPose.c_str();
	suffix = "";
	itemClass = "";

	return true;
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::OnAction(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	CGameInputActionHandlers::TPickAndThrowWeaponActionHandler& pickAndThrowActionHandler = g_pGame->GetGameInputActionHandlers().GetCPickAndThrowWeaponActionHandler();

	if(!pickAndThrowActionHandler.Dispatch(this,actorId,actionId,activationMode,value))
	{
		CWeapon::OnAction(actorId, actionId, activationMode, value);
	}
}


//---------------------------------------------------------------------------
bool CPickAndThrowWeapon::OnActionAttack(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (m_state != eST_IDLE)
		return false;

	const SPickAndThrowParams::SThrowParams& throwParams = GetThrowParams();

	if (activationMode == eAAM_OnPress && m_state == eST_IDLE)
	{
		PowerThrowPrepareSound(true);
	}
	else if (activationMode == eAAM_OnRelease)
	{	
		PowerThrowPrepareSound(false);
		RequestStartThrow(eST_PUSHINGAWAY);
	}
	
	return true;
}


//---------------------------------------------------------------------------
bool CPickAndThrowWeapon::OnActionUse(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{	
	CPlayer* pOwnerPlayer = GetOwnerPlayer();
	if (pOwnerPlayer && !pOwnerPlayer->IsStillWaitingOnServerUseResponse())
	{
		bool performDrop = ( activationMode == eAAM_OnHold ) && ( m_state == eST_IDLE);

		if( performDrop )
		{
			if (!PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId ))
			{
				if( IsNPC() )
					RequestStartThrow( eST_PUSHINGAWAY_WEAK );		
				else
					Start_Drop();
			}
			else
			{
				RequestStartThrow( eST_PUSHINGAWAY_WEAK );		
			}
		}
	}

	return true;
}

//---------------------------------------------------------------------------
bool CPickAndThrowWeapon::OnActionDrop(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	bool performDrop = (activationMode == eAAM_OnPress) && (m_state == eST_IDLE);

	if (performDrop)
	{
		if (! PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId ) )
		{
			if (IsNPC())
				RequestStartThrow( eST_PUSHINGAWAY_WEAK );		
			else
				Start_Drop();
		}
		else
		{
			RequestStartThrow(eST_PUSHINGAWAY_WEAK);
		}
	}

	return true;
}

void CPickAndThrowWeapon::PerformMelee(int activationMode,const bool bForcePerformMelee /*= false*/)
{
	// Cache these
	const SPickAndThrowParams::SGrabTypeParams& grabParams = GetGrabTypeParams(); 

	if (IsNPC())
	{
		if (m_state == eST_IDLE)
		{
			m_state = eST_KILLING_GRABBEDNPC;
			m_timePassed = 0;
			const SPickAndThrowParams::SKillGrabbedNPCParams& killParams = grabParams.killGrabbedNPCParams;
			PlayActionLocal( killParams.action.c_str(), false, eIPAF_Default ); 
		}
	}
	else
	{
		const bool performMelee = (activationMode == eAAM_OnPress) && (m_state == eST_IDLE) && !m_collisionHelper.IsBlocked();
		if (performMelee || bForcePerformMelee)
		{
			CActor *pActor = GetOwnerActor();
			if ((pActor != NULL) && pActor->IsClient())
			{
				if (IMovementController * pMC = pActor->GetMovementController())
				{
					SMovementState info;
					pMC->GetMovementState(info);

					if(!(grabParams.m_propertyFlags & EPF_use_physical_melee_system))
					{
						m_currentMeleeAutoTargetId = m_collisionHelper.GetBestAutoAimTargetForUser(GetOwnerId(), info.eyePosition, info.eyeDirection, g_pGameCVars->pl_melee.melee_snap_target_select_range, cos_tpl(DEG2RAD(g_pGameCVars->pl_melee.melee_snap_angle_limit)));
					}
					else 
					{
						m_currentMeleeAutoTargetId = m_collisionHelper.GetBestAutoAimTargetForUser(GetOwnerId(), info.eyePosition, info.eyeDirection,  grabParams.complexMelee_snap_target_select_range, cos_tpl(DEG2RAD(g_pGameCVars->pl_pickAndThrow.complexMelee_snap_angle_limit)));
					}
				}

				SetBusy(true);
			}

			TagState fragTags;
			if(m_attackIndex==1)
			{
				const SMannequinPlayerParams::Fragments::Smelee& meleeEnviro = PlayerMannequin.fragments.melee;
				if(const CTagDefinition* pFragTagDef = meleeEnviro.pTagDefinition)
				{
					pFragTagDef->Set(fragTags, meleeEnviro.fragmentTagIDs.secondSwing, true);
				}
			}
			PlayFragment(new CActionPNTMelee(*this, fragTags));
			m_animDuration = GetCurrentAnimationTime(eIGS_Owner) * 0.001f;  // divide by 1000

			// If using the more expensive 'physical' melee system for larger weapons (e.g. lamposts etc) we instead transition to the ComplexMelee state
			if(grabParams.m_propertyFlags & EPF_use_physical_melee_system)
			{
				m_state = eST_COMPLEX_MELEE; 
				OnDoComplexMelee(true); 
			}
			else
			{
				m_state = eST_MELEE;
				GetScheduler()->TimerAction((uint32)(grabParams.melee_delay * 1000.0f), CSchedulerAction<DoSimpleMelee>::Create(DoSimpleMelee(*this)), false);
				GetScheduler()->TimerAction((uint32)(grabParams.meleeAction_duration * 1000.0f), CSchedulerAction<FinishSimpleMelee>::Create(FinishSimpleMelee(*this)), false);
			}

			// User can request a new combo
			m_logicFlags &= ~ELogicFlag_RequestedCombo; 
		}
	}
}

//--------------------------------------------------------------------------------------------------------------
float CPickAndThrowWeapon::CalculateAnimTimeScaling(const ItemString& anim, const float desiredAnimDuration)
{
	// TODO : use mannequin to calculate the time scaling
	return -1.0f; 
}

//---------------------------------------------------------------------------
bool CPickAndThrowWeapon::OnActionMelee(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if( CEnvironmentalWeapon* pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId ) )
	{
		if(m_postMeleeActionCooldown <= 0.0f && activationMode == eAAM_OnPress)
		{
			// Cache these
			const SPickAndThrowParams::SGrabTypeParams& grabParams = GetGrabTypeParams(); 

			const bool bAalreadyPerformingMelee = m_state == eST_COMPLEX_MELEE;

			// If already in a melee attack, the user would like a combo attack to be chained (if possible)
			const int size = grabParams.m_meleeComboList.size(); 
			if( ( bAalreadyPerformingMelee || (m_state == eST_IDLE && m_attackIndex == -1)) && 
					(m_logicFlags & ELogicFlag_RequestedCombo) == 0  && 
					!grabParams.m_meleeComboList.empty() && 
					m_attackIndex < size)
			{
				if(bAalreadyPerformingMelee)
				{
					// Lets check the player has tried to attack within the valid 'window' part of the previous anim (e.g. 0.8 -> 1.0 T of prev anim)
					const float windowDuration = max((m_animDuration * g_pGameCVars->pl_pickAndThrow.comboInputWindowSize),g_pGameCVars->pl_pickAndThrow.minComboInputWindowDurationInSecs);
					if((m_animDuration - m_timePassed) >= windowDuration) // combo input window size [0.0f,1.0f]
					{
						return true; 
					}
				}

				// Good to go!
				++m_attackIndex;
				
				PTLogCombo("CLIENT INPUT - Melee Attack reg, incremented m_attackIndex to [%d]", m_attackIndex);

				// First attack doesn't trigger a combo 
				if(m_attackIndex > 0)
				{
					// Move onto next combo in chain
					m_logicFlags |= ELogicFlag_RequestedCombo; 
					PTLogCombo("CLIENT INPUT - Melee Attack registered, m_bRequestedCombo is now TRUE");
				}
			}

			// If not already in a melee.. lets attack
			if(m_state != eST_COMPLEX_MELEE)
			{
				PTLogCombo("CLIENT INPUT - Was not already in a eST_COMPLEX_MELEE so calling request start melee attack");
				RequestStartMeleeAttack(true, false, m_attackIndex); // This triggers a netserialise ASPECT_STREAM for 3rd party clients to mimic melee
				if(g_pGame->GetIGameFramework()->GetClientActorId() == actorId)
				{
					PerformMelee(eAAM_OnPress);
				}

				//track melee combo
				IActor* pOwnerActor = gEnv->pGameFramework->GetIActorSystem()->GetActor( actorId );
				CStatsRecordingMgr* pRecordingMgr = g_pGame->GetStatsRecorder();

				if( IStatsTracker* pTracker = pRecordingMgr ? pRecordingMgr->GetStatsTracker( pOwnerActor ) : NULL )
				{
					pTracker->Event( eGSE_EnvironmentalWeaponEvent, new CEnvironmentalWeaponEvent( pEnvWeapon->GetEntityId(), eEVE_MeleeAttack, m_attackIndex ) );
				}
			}
		}
	}
	else
	{
		PerformMelee(activationMode);
	}

	return true;
}

bool CPickAndThrowWeapon::OnActionChargedThrow(EntityId actorId, const ActionId& actionId, int activationMode, float value)
{
	if (m_state != eST_IDLE && m_state != eST_CHARGED_THROW_PRE_RELEASE)
		return false;

	const SPickAndThrowParams::SThrowParams& throwParams = GetThrowParams();

	if( throwParams.chargedThrowEnabled )
	{
		if (activationMode == eAAM_OnPress && m_state == eST_IDLE)
		{
			// Play some sound effect to indicate charged throw starting
			PowerThrowPrepareSound(true);

			// Start a charged throw!
			RequestChargedThrowPreRelease();

			// Fire the trigger immeadiately
			m_logicFlags |= ELogicFlag_ReleaseChargedThrowTriggered;
		}
	}

	return true;
}

//---------------------------------------------------------------------------
bool CPickAndThrowWeapon::CanModify() const
{
	return false;
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::SetUpCollisionPropertiesAndMassInfo( const bool pickedUp )
{
	IEntity *pObjectEntity = GetEntityObject();
	IF_UNLIKELY (pObjectEntity == NULL)
		return;

	if (pObjectEntity->IsHidden())
	{
		m_constraintId = 0;
		m_constraintId2 = 0;
		m_objectCollisionsLimited = 0;
		m_objectMass = 0.0f;
		m_meleeRange = 1.5f;
		return;
	}

	IPhysicalEntity *pObjectPhysics = pObjectEntity->GetPhysics();
	IF_UNLIKELY (pObjectPhysics == NULL)
		return;

	if (pickedUp)
	{
		CActor *pOwnerActor = GetOwnerActor();
		if (pOwnerActor)
		{
			pe_action_add_constraint ic;
			ic.flags = constraint_inactive|constraint_ignore_buddy;
			ic.pBuddy = pOwnerActor->GetEntity()->GetPhysics();
			ic.pt[0].Set(0,0,0);
			m_constraintId = pObjectPhysics->Action(&ic);

			if(ICharacterInstance* pCharInst = pOwnerActor->GetEntity()->GetCharacter(0))
			{
				if(ISkeletonPose* pPose = pCharInst->GetISkeletonPose())
				{
					if(IPhysicalEntity* pCharPhys = pPose->GetCharacterPhysics())
					{
						ic.pBuddy = pCharPhys;
						m_constraintId2 = pObjectPhysics->Action(&ic);
					}
				}
			}

			m_framesToCheckConstraint = 3;  // need to wait a few frames before start checking for the constraint. apparently, some issues with sys_physics_cpu 1 
			m_objectCollisionsLimited = true;
			
			// deactivate bullet and explosion collisions on the NPC
			if (IsNPC())
			{
				ICharacterInstance* pNpcCharacterInstance = pObjectEntity->GetCharacter(0);
				IPhysicalEntity *pNpcSkeletonPhysics = (pNpcCharacterInstance != NULL) ? pNpcCharacterInstance->GetISkeletonPose()->GetCharacterPhysics() : NULL;
				if (pNpcSkeletonPhysics)
				{
					pe_params_part ppp;
					ppp.flagsAND = ~(geom_colltype_ray|geom_colltype_foliage_proxy);  // No idea why 'geom_colltype_foliage_proxy' affects the hits, but it does. RnD guy said its expected.
					pNpcSkeletonPhysics->SetParams(&ppp);
				}
			}
		}

		// Set some properties on the object to allow collisions to still be detected by listeners, but not processed and acted on by the physics system
		pe_params_part colltype;
		colltype.flagsOR = geom_no_coll_response;
		pObjectPhysics->SetParams(&colltype);

		pe_status_dynamics dynamics;
		if (pObjectPhysics->GetStatus(&dynamics))
		{
			// Sometimes mass is set really high, causes objects to fly off
			// to infinite (and beyond!).
			// Clamping it to 80kg gives a nice satifying impulse to enemies when you hit them.
			const float maxMass = 80.0f;

			m_objectMass = (float)__fsel( maxMass-dynamics.mass, dynamics.mass, maxMass );
		}

		float range = 1.5f;
		AABB objectBBox;
		pObjectEntity->GetWorldBounds(objectBBox);

		m_meleeRange = (!objectBBox.IsEmpty()) ? max(objectBBox.GetRadius() + range, range) : range;
	}
	else
	{
		RestoreObjectPhysics( m_objectId, IsNPC());
		m_objectCollisionsLimited = false;
		m_objectMass = 0.0f;
		m_meleeRange = 1.5f;

		// restore collision response	
		pe_params_part colltype;
		colltype.flagsAND = ~geom_no_coll_response;
		pObjectPhysics->SetParams(&colltype);
	}
}


//---------------------------------------------------------------------------
void CPickAndThrowWeapon::RestoreObjectPhysics( EntityId entityId, bool isNPC )
{
	IEntity *pObjectEntity = m_pEntitySystem->GetEntity( entityId );
	IF_UNLIKELY (pObjectEntity == NULL)
		return;

	IPhysicalEntity *pObjectPhysics = pObjectEntity->GetPhysics();
	IF_UNLIKELY (pObjectPhysics == NULL)
		return;

	pe_action_update_constraint up;
	up.bRemove = true;
	up.idConstraint = m_constraintId;
	m_constraintId = 0;
	pObjectPhysics->Action(&up);

	up.idConstraint = m_constraintId2;
	m_constraintId2 = 0;
	pObjectPhysics->Action(&up);

	// reactivate explosion collisions on the NPC ( not like it matters as it will always be dead but oh well)
	if (IsNPC())
	{
		CActor* pTargetNpcActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor( entityId ));
		if ((pTargetNpcActor != NULL) && CPlayer::GetActorClassType() == pTargetNpcActor->GetActorClass() )
		{
			CPlayer* pTargetNpcPlayer = static_cast<CPlayer*>(pTargetNpcActor);

			IPhysicalEntity* pPhysicalEntity = pTargetNpcPlayer->GetEntity()->GetPhysics();
				
			// It is not always a ragdoll at this point. for example, when grabandthrow is aborted because a loadinggame or ESC on editormode.
			if ((pPhysicalEntity != NULL) && (pPhysicalEntity->GetType() == PE_ARTICULATED))
			{
				pTargetNpcPlayer->SetRagdollPhysicsParams(pPhysicalEntity, false);
			}
			else
			{
				// Restore the flags we cleared if not ragdoll
				ICharacterInstance* pNpcCharacterInstance = pTargetNpcPlayer->GetEntity()->GetCharacter(0);
				IPhysicalEntity *pNpcSkeletonPhysics = (pNpcCharacterInstance != NULL) ? pNpcCharacterInstance->GetISkeletonPose()->GetCharacterPhysics() : NULL;
				if (pNpcSkeletonPhysics)
				{
					pe_params_part ppp;
					ppp.flagsOR = (geom_colltype_ray|geom_colltype_foliage_proxy);  
					pNpcSkeletonPhysics->SetParams(&ppp);
				}
			}
			}
	}
	
} 

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::Update(SEntityUpdateContext& ctx, int val )
{
	CWeapon::Update( ctx, val );

	if (val!=eIUS_General)
		return;   // to avoid multiple updates in the same frame

	// Cache these
	const SPickAndThrowParams::SGrabTypeParams& grabParams = GetGrabTypeParams(); 

	if(m_postMeleeActionCooldown > 0.0f)
	{
		m_postMeleeActionCooldown -= ctx.fFrameTime; 
	}
	
	UpdateInfiniteViewEntities( ctx.fFrameTime );
	
	if (m_flyingActorInfo.valid)
	{
		m_flyingActorInfo.timeToEndTracking -= ctx.fFrameTime;
		if (m_flyingActorInfo.timeToEndTracking<=0)
		{
			RestoreObjectPhysics( m_flyingActorInfo.entityId, true );
			m_flyingActorInfo.valid = false;
		}
	}
		
	m_timePassed += ctx.fFrameTime;
	
	switch (m_state)
	{
		case eST_UNKNOWN:
			break;
		case eST_GOINGTOPICK:
		{
			RequireUpdate( eIUS_General );

			if (!GetEntityObject() || !IsGrabStillValid())
			{
				CPlayer *pOwner = GetOwnerPlayer();
				if (pOwner)
				{
					pOwner->ExitPickAndThrow();
				}
			}
			else
			{
				// Determine time limit in eST_GOINGTOPICK state
				const float timeLimit = DetermineGoingToPickStateDuration();

				// Check for state transition
				if (m_timePassed >= timeLimit)
				{
					m_state = eST_PICKING;
					CPlayer *pOwner = GetOwnerPlayer();
					if (pOwner)
					{
						pOwner->NotifyObjectGrabbed( true, m_objectId, IsNPC() );
					}

					AttachObject();
				}
			}

			break;
		}
	
		case eST_PICKING:
		{
			RequireUpdate( eIUS_General );

			const float timeLimit = DeterminePickingStateDuration(); 
			if (m_timePassed >= timeLimit)
			{
				m_state = eST_IDLE;

				OnEndGrabAction(); 
			}
			break;
		}

		case eST_GRABBINGNPC:
			{
				RequireUpdate( eIUS_General );
			}
			break;
		
		case eST_CHARGED_THROW_PRE_RELEASE:
		{
			UpdateChargedThrowPreRelease(ctx.fFrameTime); 
			break; 
		}

		case eST_PUSHINGAWAY:
		case eST_PUSHINGAWAY_POWER:
		case eST_PUSHINGAWAY_WEAK:
		case eST_CHARGED_THROW_POST_RELEASE:
		{
			RequireUpdate( eIUS_General );
			
			CPlayer *pOwner = GetOwnerPlayer();

			m_obstructionChecker.DoCheck( pOwner, m_objectId );

			if (m_timePassed >= GetThrowParams().timeToFreeAtThrowing || m_timePassed>=m_animDuration)
			{
				if (pOwner)
				{
					pOwner->NotifyObjectGrabbed( false, m_objectId, IsNPC(), false, GetThrowParams().throwSpeed );
				}
				if (IsNPC())
				{
					PrepareNPCToBeDropped(m_state != eST_PUSHINGAWAY_WEAK);
				}

				ThrowObject();

				if (!IsNPC())
				{
					SetUpCollisionPropertiesAndMassInfo( false );
				}

				if(m_state == eST_CHARGED_THROW_POST_RELEASE)
				{
					OnDoChargedThrow(); 
				}

				m_state = eST_THROWING; 
			}
			break;
		}
		
		case eST_THROWING:
		{
			RequireUpdate( eIUS_General );
			float timeMax = m_animDuration;
			if (IsNPC())
				timeMax = max( timeMax, (GetThrowParams().timeToFreeAtThrowing + GetThrowParams().timeToRestorePhysicsAfterFree) );
			
			if (m_timePassed >= timeMax)
			{
				if (IsNPC())
				{
					SetUpCollisionPropertiesAndMassInfo( false );
					EnslaveTarget(false);
				}

				CPlayer *pOwner = GetOwnerPlayer();
				if (pOwner)
				{
					pOwner->ExitPickAndThrow();
				}
				if (m_state != eST_UNKNOWN)
				{
					ResetInternal();
				}
			}
			break;
		}
		
		case eST_STARTDROPPING:
		{
			RequireUpdate( eIUS_General );
			if (m_timePassed >= GetGrabTypeParams().timeToFreeAtDropping)  
			{
				m_state = eST_DROPPING;
				SetUpCollisionPropertiesAndMassInfo( false );
			}
			break;
		}

		case eST_DROPPING:
		{
			RequireUpdate( eIUS_General );
			if (m_timePassed >= GetGrabTypeParams().timeDropping)
			{
				CPlayer *pOwner = GetOwnerPlayer();
				if (pOwner)
				{
					pOwner->ExitPickAndThrow();
				}
				m_state = eST_UNKNOWN;
			}
			break;
		}
		
		case eST_MELEE:
		{
			if(m_currentMeleeAutoTargetId)
			{
				g_pGame->GetAutoAimManager().SetCloseCombatSnapTarget(m_currentMeleeAutoTargetId, 
					g_pGameCVars->pl_melee.melee_snap_end_position_range, 
					g_pGameCVars->pl_melee.melee_snap_move_speed_multiplier);
				
				RequireUpdate( eIUS_General );
			}
			break;
		}

		case eST_COMPLEX_MELEE:
		{
			RequireUpdate( eIUS_General );
			UpdateComplexMelee(ctx.fFrameTime);	
			break;
		}
		
		case eST_KILLING_GRABBEDNPC:
		{
			RequireUpdate( eIUS_General );
			if (m_timePassed >= m_animDuration)
			{
				m_state = eST_KILLED_GRABBEDNPC;
				m_timePassed = 0;
			}
			break;
		}
		
		case eST_KILLED_GRABBEDNPC:
		{
			RequireUpdate( eIUS_General );
			{
				Start_Drop( false );
			}
			break;
		}
		
		
		case eST_IDLE:
		{
			if (!IsGrabStillValid())
			{
				Start_Drop();
			}

			CPlayer *pOwner = GetOwnerPlayer();

			// If finished a melee, and a combo was requested.. trigger next. 
			if(m_logicFlags & ELogicFlag_RequestedCombo)
			{
				PTLogCombo("m_bRequestedCombo was set from idle so triggering RequestStartMeleeAttack with attackindex [%d]",m_attackIndex);
				const int8 index = m_attackIndex-1;
				if(! (index < 0 || grabParams.m_meleeComboList.empty() || (index >= (int8)grabParams.m_meleeComboList.size())) )
				{
					if( CEnvironmentalWeapon* pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId ) )
					{
						//track melee combo
						CStatsRecordingMgr* pRecordingMgr = g_pGame->GetStatsRecorder();
						if( IStatsTracker* pTracker = pRecordingMgr ? pRecordingMgr->GetStatsTracker( pOwner ) : NULL )
						{
							pTracker->Event( eGSE_EnvironmentalWeaponEvent, new CEnvironmentalWeaponEvent( pEnvWeapon->GetEntityId(), eEVE_MeleeAttack, m_attackIndex ) );
						}
					}

					RequestStartMeleeAttack(true, false, m_attackIndex);
					if(g_pGame->GetIGameFramework()->GetClientActorId() == GetOwnerId())
					{
						PerformMelee(eAAM_OnPress);			
					}
				}

				m_logicFlags &= ~ELogicFlag_RequestedCombo;
			}

			if((m_logicFlags & (ELogicFlag_MeleeActionFinished)) == (ELogicFlag_MeleeActionFinished))
			{
				if( CEnvironmentalWeapon* pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId ) )
				{
					CStatsRecordingMgr* pRecordingMgr = g_pGame->GetStatsRecorder();
					if( IStatsTracker* pTracker = pRecordingMgr ? pRecordingMgr->GetStatsTracker( pOwner ) : NULL )
					{
						pTracker->Event( eGSE_EnvironmentalWeaponEvent, new CEnvironmentalWeaponEvent( pEnvWeapon->GetEntityId(), eEVE_MeleeAttack, m_attackIndex ) );
					}
				}
			}

			if(m_logicFlags & ELogicFlag_ProcessEndComplexMeleeState)
			{
				// We may have some view correction to perform after complex melee has ended to correct the player's view. 
				m_correctOrientationParams.m_logicFlags |= (SCorrectOrientationParams::ELogicFlag_AllowVerticalPitchCorrection | SCorrectOrientationParams::ELogicFlag_SetOrientation); 
				m_correctOrientationParams.m_logicFlags &= ~SCorrectOrientationParams::ELogicFlag_SetTranslation;

				if((m_logicFlags & ELogicFlag_OrientationCorrectionInitialised) && UpdateCorrectOrientation(ctx.fFrameTime))
				{
					// Done with correct orientation, disable for remainder
					m_logicFlags &= ~ELogicFlag_OrientationCorrectionInitialised; 
				}
			}
		  break;
		}
	}

#ifdef PICKANDTHROWWEAPON_DEBUGINFO
	if (g_pGameCVars->pl_pickAndThrow.chargedThrowDebugOutputEnabled==1)
	{
		//////////////////
		// DEBUG OUTPUT // 
		//////////////////

		string stateName;
		switch(m_chargedThrowState)
		{
		case eCTS_INACTIVE:
			{
				stateName = "eCTS_INACTIVE"; 
				break;
			}
		case eCTS_PRIMING:
			{
				stateName = "eCTS_PRIMING"; 
				break;
			}
		case eCTS_CHARGING:
			{
				stateName = "eCTS_CHARGING"; 
				break;
			}
		default:
			{
				stateName = "UNKNOWN"; 
				break;
			}
		}
		CryWatch(" ChargedThrowState = %s", stateName.c_str());
	}
#endif // #ifdef PICKANDTHROWWEAPON_DEBUGINFO
}
//---------------------------------------------------------------------------
void CPickAndThrowWeapon::UpdateComplexMelee(const float dt)
{
	//-----------------------------------------
	// GENERAL COMPLEX MELEE UPDATE LOGIC 
	CPlayer* pOwner = GetOwnerPlayer(); 
	if(pOwner && pOwner->IsClient())
	{
		UpdateComplexMeleeLerp(pOwner); 
	}
	// ~GENERAL COMPLEX MELEE UPDATE LOGIC 
	//-----------------------------------------
}

//----------------------------------------------------------------------------
void CPickAndThrowWeapon::UpdateComplexMeleeLerp(const CPlayer* pOwner)
{
	if(m_currentMeleeAutoTargetId)
	{
		const IActor* pTarget = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_currentMeleeAutoTargetId);
		if(pTarget && !pTarget->IsDead())
		{
			Vec3 targetPos = pTarget->GetAnimatedCharacter()->GetAnimLocation().t;
			Vec3 playerPos = pOwner->GetAnimatedCharacter()->GetAnimLocation().t;

			const SPickAndThrowParams::SGrabTypeParams& grabParams = GetGrabTypeParams(); 
			const float	lerpTargetSelectRange	= grabParams.complexMelee_snap_target_select_range;

			if(lerpTargetSelectRange > 0.0f) // If we are too far from target with this attack.. we don't want lerp.. abort here. 
			{
				const float lerpEnd	= grabParams.complexMelee_snap_end_position_range; 
				Vec3 displacement   = (targetPos - playerPos);
				if(displacement.GetLengthSquared() > (lerpEnd*lerpEnd))
				{
					// Request the lerp
					g_pGame->GetAutoAimManager().SetCloseCombatSnapTarget(m_currentMeleeAutoTargetId, lerpEnd, g_pGameCVars->pl_pickAndThrow.complexMelee_lerp_target_speed);

					// Also do some view lerp
					UpdateTargetViewLock(displacement, grabParams);

					// break here so we dont set m_currentMeleeAutoTargetId to 0, so we  repeat next frame. 
					return;
				}
			}
		}
	
		// If reach here, we either have no target, or are within range. Done with lerp till next attack
		m_currentMeleeAutoTargetId = 0; 
	}
}

//---------------------------------------------------------------------------
float CPickAndThrowWeapon::DetermineGoingToPickStateDuration() const
{
	const SPickAndThrowParams::SGrabTypeParams& grabParams = GetGrabTypeParams(); 

	float duration;
	if (IsNPC())
	{
		duration = m_animDuration;
	}
	else
	{
		duration = grabParams.timeToStickAtPicking;
	}

	CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
	if(pEnvWeapon)
	{
		bool bIsRooted = false; 
		if( const IEntity *pObjectEntity = GetEntityObject() )
		{
			if( IScriptTable* pObjectEntityScript = pObjectEntity->GetScriptTable() )
			{
				pObjectEntityScript->GetValue( "IsRooted", bIsRooted );
			}
		}

		// If no longer rooted and being picked up.. and is an environmental weapon.. then we *always* attach at the end of the grab anim
		if(!bIsRooted)
		{
			duration = m_animDuration;
			duration *= g_pGameCVars->pl_pickAndThrow.environmentalWeaponUnrootedPickupTimeMult;
		}

		// CVar override for duration wins out. 
		float desiredDuration = g_pGameCVars->pl_pickAndThrow.environmentalWeaponDesiredGrabAnimDuration;
		if(bIsRooted)
		{
			desiredDuration = g_pGameCVars->pl_pickAndThrow.environmentalWeaponDesiredRootedGrabAnimDuration; // < 0.0f == no scaling desired
		}

		// If CVAR override not set.. we fall back to XML
		if(desiredDuration <= 0.0f)
		{
			desiredDuration = grabParams.anim_durationOverride_Grab;
			if(bIsRooted)
			{
				desiredDuration = grabParams.anim_durationOverride_RootedGrab;
			}
		}

		if(desiredDuration > 0.0f)
		{
			ScaleTimeValue(duration, m_animDuration, desiredDuration);
		}
	}

	// Done
	return duration;
}

//---------------------------------------------------------------------------
float CPickAndThrowWeapon::DeterminePickingStateDuration() const
{
	float duration = GetGrabTypeParams().timePicking;
	if (IsNPC() || duration < 0.0f) // We can mark -1.0f duration in xml to specify we should use anim duration
	{
		duration = m_animDuration;
	}

	CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
	if(pEnvWeapon)
	{
		bool bIsRooted = false; 
		if( const IEntity *pObjectEntity = GetEntityObject() )
		{
			if( IScriptTable* pObjectEntityScript = pObjectEntity->GetScriptTable() )
			{
				pObjectEntityScript->GetValue( "IsRooted", bIsRooted );
			}
		}

		// CVar override for duration wins out. 
		float desiredDuration = g_pGameCVars->pl_pickAndThrow.environmentalWeaponDesiredGrabAnimDuration;
		if(bIsRooted)
		{
			desiredDuration = g_pGameCVars->pl_pickAndThrow.environmentalWeaponDesiredRootedGrabAnimDuration; // < 0.0f == no scaling desired
		}

		// If CVAR override not set.. we fall back to XML
		if(desiredDuration <= 0.0f)
		{
			const SPickAndThrowParams::SGrabTypeParams& grabParams = GetGrabTypeParams();

			desiredDuration = grabParams.anim_durationOverride_Grab;
			if(bIsRooted)
			{
				desiredDuration = grabParams.anim_durationOverride_RootedGrab;
			}
		}

		if(desiredDuration > 0.0f)
		{
			ScaleTimeValue(duration, m_animDuration, desiredDuration);
		}
	}

	return duration; 
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::ScaleTimeValue(float& timeValueToScale, const float totalDuration, const float desiredDuration) const
{
	CRY_ASSERT( desiredDuration > 0.0f ); 
	timeValueToScale = (timeValueToScale / totalDuration) * desiredDuration; 
}


//---------------------------------------------------------------------------
void CPickAndThrowWeapon::PerformCameraShake(const SPickAndThrowParams::SCameraShakeParams& camShake, const CPlayer* pPlayer )
{
	CRY_ASSERT_MESSAGE(pPlayer, "CPickAndThrowWeapon::PerformCameraShake(...) < error pPlayer is NULL");
	if(camShake.enabled && camShake.time > 0.0f)
	{
		CScreenEffects* pScreenFX = g_pGame->GetScreenEffects();
		CRY_ASSERT_MESSAGE(pScreenFX, "CPickAndThrowWeapon::PerformCameraShake(...) < error pScreenFX is NULL");

		float attenuation = 1.0f;
		if (camShake.viewAngleAttenuation)
		{
			const Vec3 up(0, 0, 1);
			Vec3 upVector = pPlayer->GetViewQuat() * up;
			float viewDot = upVector * up;
			attenuation = viewDot * viewDot;
		}
		float time = camShake.time * attenuation;
		float frequency = camShake.frequency * attenuation;
		Vec3 shift = camShake.shift * attenuation;
		Vec3 rotate = camShake.rotate * attenuation;

		bool changeCamDir = camShake.randomDirection && cry_random(0, 1);
		if (changeCamDir)
		{
			shift.x = -shift.x;
			rotate.y = -rotate.y;
		}

		pScreenFX->CamShake(rotate, shift, frequency, time,	0.0f, CScreenEffects::eCS_GID_Player);
	}
};

//---------------------------------------------------------------------------
// Contains all the logic for the various sub stages of a charged throw
void CPickAndThrowWeapon::UpdateChargedThrowPreRelease( const float frameTime )
{
	RequireUpdate( eIUS_General );

	// Need to make sure we switch to a looped hold anim when prime is done
	if( m_chargedThrowState == eCTS_PRIMING && 
		  m_timePassed >= (m_animDuration*0.99f)) // NOTE: pre-empt the end of this anim slightly?
	{
		m_chargedThrowState = eCTS_CHARGING; 
		m_timePassed				= 0.0f; 
	}
	else if(m_chargedThrowState == eCTS_CHARGING)
	{
		// If the user released the charge key (or we've set instant charged throw and are owned by local player) .. its time to throw!
		if( m_logicFlags & ELogicFlag_ReleaseChargedThrowTriggered )
		{
			// Start throw
			RequestStartThrow(eST_CHARGED_THROW_POST_RELEASE); 
			
			// done
			m_logicFlags &= ~ELogicFlag_ReleaseChargedThrowTriggered; 
		}
	}

	// DEBUG RENDERING
#ifdef PICKANDTHROWWEAPON_DEBUGINFO

	// Only called AT point of throw normally.. force call every frame (w visuals)
	if (g_pGameCVars->pl_pickAndThrow.chargedThrowDebugOutputEnabled)
	{
		const CCamera& cam = gEnv->p3DEngine->GetRenderingCamera(); 
		const IEntity* pEntity = CalculateBestChargedThrowAutoAimTarget(cam.GetMatrix()); 

		// Draw cone
		float length = g_pGameCVars->pl_pickAndThrow.chargedThrowAutoAimDistance; 
		float radius = length * tan(DEG2RAD(g_pGameCVars->pl_pickAndThrow.chargedThrowAutoAimConeSize * 0.5f));

		SAuxGeomRenderFlags originalFlags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
		SAuxGeomRenderFlags newFlags = originalFlags; 
		newFlags.SetCullMode(e_CullModeNone); 
		newFlags.SetFillMode(e_FillModeWireframe);
		newFlags.SetAlphaBlendMode(e_AlphaBlended);

		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(cam.GetMatrix().GetColumn3() + (cam.GetMatrix().GetColumn1()*length),-cam.GetMatrix().GetColumn1(), radius , length ,ColorB(0,0,255,120), true );
		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(originalFlags);

	}
#endif //#ifdef PICKANDTHROWWEAPON_DEBUGINFO

	//////////////////////////////////
	// [JB] - Future improvements:  //
	//////////////////////////////////

	// Add logic in here to force drop/release if enter water etc /
	// Add logic to handle crouching specific stuff
}

//---------------------------------------------------------------------------
bool CPickAndThrowWeapon::UpdateCorrectOrientation( const float dt )
{
	if(m_logicFlags & ELogicFlag_OrientationCorrectionInitialised)
	{
		// Lets orient the player...
		m_orientationCorrector.UpdateBlendOverTime(dt);
		const QuatT& desiredQuatT = m_orientationCorrector.GetCurrent(); 

		CPlayer* pPlayer = GetOwnerPlayer();
		if (pPlayer)
		{
			if(m_correctOrientationParams.m_logicFlags & SCorrectOrientationParams::ELogicFlag_SetTranslation)
			{
				// Set translation
				pPlayer->GetEntity()->SetPos(desiredQuatT.t);
			}
	
			if(m_correctOrientationParams.m_logicFlags & SCorrectOrientationParams::ELogicFlag_SetOrientation)
			{
				// Set orientation
				Vec3 forceLookVector = desiredQuatT.GetColumn1();

				if((m_correctOrientationParams.m_logicFlags & SCorrectOrientationParams::ELogicFlag_AllowVerticalPitchCorrection) == 0)
				{
					forceLookVector.z		 = 0.0f;
					forceLookVector.Normalize();
				}

				pPlayer->SetForceLookAt(forceLookVector, false);
			}
		}

		if( m_orientationCorrector.BlendComplete() )
		{
			return true; 
		}

		return false; 
	}
	return true; 
}

//---------------------------------------------------------------------------
// entities thrown away are kept at infinite view distance until they stop

void CPickAndThrowWeapon::UpdateInfiniteViewEntities( float frameTime )
{
	if (m_infiniteViewEntities.size()>0)
	{
		RequireUpdate( eIUS_General );
		uint32 numValidEntities = 0;
		float currTime = gEnv->pTimer->GetCurrTime();
		
		std::vector<SInfiniteViewDistEntity>::iterator iter = m_infiniteViewEntities.begin();
		while (iter!=m_infiniteViewEntities.end())
		{
			SInfiniteViewDistEntity& info =  *iter;
			
			if (info.valid)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity( info.entityId );
				if (pEntity)
				{
					++numValidEntities;
					if (currTime>info.timeLimit) // to workaround any possible bad situation of entity falling forever or whatever
					{
						RestoreEntityViewDist( info );
						info.valid = false;
					}
					else
					{
						IPhysicalEntity *pPE = pEntity->GetPhysics();
						if (pPE)
						{
							pe_status_dynamics dynStat;
							int dynStatType = dynStat.type;
							memset(&dynStat, 0, sizeof(pe_status_dynamics));
							dynStat.type = dynStatType;
							pPE->GetStatus( &dynStat );
							const float MIN_SPEED = 0.1f;  // is stopped for us when is moving at less than this
							if (dynStat.v.GetLengthSquared()<(MIN_SPEED*MIN_SPEED))
							{
								info.timeStopped += frameTime;
								const float TIME_TO_CONSIDER_IT_REALLY_STOPPED = 3.f;
								if (info.timeStopped>=TIME_TO_CONSIDER_IT_REALLY_STOPPED)
								{
									RestoreEntityViewDist( info );
									info.valid = false;
								}
							}
						}
					}
				}
				else
					info.valid = false;
			}
		
			++iter;
		}
	
		if (numValidEntities==0)
			m_infiniteViewEntities.clear();
	}
}


//////////////////////////////////////////////////////////////////////////

void CPickAndThrowWeapon::RestoreEntityViewDist( const SInfiniteViewDistEntity& info ) const
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity( info.entityId );
	if (pEntity)
	{
		{
			pEntity->SetViewDistRatio( info.oldViewDistRatio );
		}
	}
}


//////////////////////////////////////////////////////////////////////////

void CPickAndThrowWeapon::ResetTrackingOfExternalEntities()
{
	// flying actor
	if (m_flyingActorInfo.valid)
	{
		RestoreObjectPhysics( m_flyingActorInfo.entityId, true );
		m_flyingActorInfo.valid = false;
	}
	

	// flying objects
	for (uint32 i=0; i<m_infiniteViewEntities.size(); ++i)
	{
		const SInfiniteViewDistEntity& info = m_infiniteViewEntities[i];
		RestoreEntityViewDist( info );
	}
	m_infiniteViewEntities.clear();
}



//---------------------------------------------------------------------------
void CPickAndThrowWeapon::UpdateFPView( float frameTime )
{
	CWeapon::UpdateFPView( frameTime );

	switch (m_state)
	{
		case eST_PICKING:
		case eST_IDLE:
		case eST_PUSHINGAWAY:
		case eST_PUSHINGAWAY_POWER:
		case eST_PUSHINGAWAY_WEAK:
		case eST_CHARGED_THROW_PRE_RELEASE:
		case eST_CHARGED_THROW_POST_RELEASE:
		case eST_MELEE:
		case eST_COMPLEX_MELEE:
		case eST_KILLING_GRABBEDNPC:
		case eST_KILLED_GRABBEDNPC:
		{
			if (!IsNPC())
			{
				CheckObjectHiddenByLayerSwitch();


				if (!PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId ) && !CheckObjectIsStillThere())
				{
					SetUpCollisionPropertiesAndMassInfo( false );
					DropObject();
					CPlayer *pOwner = GetOwnerPlayer();
					if (pOwner)
					{
						pOwner->ExitPickAndThrow();
					}
				}
			}
			break;
		}
	}
}


//---------------------------------------------------------------------------
bool CPickAndThrowWeapon::CheckObjectIsStillThere()
{
	// check to allow physics some time to create the constraint
	if (m_framesToCheckConstraint>0)
	{
		m_framesToCheckConstraint--;
		return true;
	}
	
	IEntity *pEntity = GetEntityObject();
	if (pEntity)
	{
		IPhysicalEntity* pPE = pEntity->GetPhysics();
		if(pPE) 
		{
			pe_status_constraint state;
			state.id = m_constraintId;

			bool isLivingBoid = (strcmp(pEntity->GetClass()->GetName(),"Boid") == 0) && (pPE->GetType() == PE_PARTICLE);
			return isLivingBoid || (pPE->GetStatus( &state ) != 0);// when this returns 0, is because the object was destroyed
		} 
	}
	
	return false;
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::CheckObjectHiddenByLayerSwitch()
{
	//If not using 'entity' attachment skip this check
	//if(PickAndThrow::UseEntityAttachment() == false)
		//return;

	//Probably object was hidden by a layer change, restore it
	IEntity *pEntity = GetEntityObject();
	if (pEntity && pEntity->IsHidden())
	{
		pEntity->Hide(false);
		SetUpCollisionPropertiesAndMassInfo(true);
	}
}

float CPickAndThrowWeapon::CalculateThrowSpeed( const CPlayer& ownerPlayer, const SPickAndThrowParams::SThrowParams& throwParams ) const
{
	if (m_state == eST_PUSHINGAWAY)
	{
			return throwParams.throwSpeed; 
	}
	else if(m_state == eST_PUSHINGAWAY_WEAK) 
	{
		return throwParams.autoThrowSpeed; 
	}
	else if(m_state == eST_CHARGED_THROW_POST_RELEASE) 
	{
		return throwParams.maxChargedThrowSpeed;
	}

	return 1.0f;
}

void CPickAndThrowWeapon::ThrowObject()
{
	DetachObject();

	IEntity *pEntity = GetEntityObject();
	IPhysicalEntity *pPhysicalEntity = (pEntity != NULL) ? pEntity->GetPhysics() : NULL;
	if (pPhysicalEntity == NULL)
		return;

	CPlayer *pOwnerPlayer = GetOwnerPlayer();
	if (pOwnerPlayer != NULL)
	{
		const SPickAndThrowParams::SThrowParams& throwParams = GetThrowParams();
		Vec3 throwDir = pOwnerPlayer->GetViewQuat().GetColumn1();
		
		float speed = CalculateThrowSpeed( *pOwnerPlayer, throwParams );
			
		if (m_obstructionChecker.IsObstructed())
		{
			speed = 1.0f; 
			CEnvironmentalWeapon* pEnvWeapon = NULL;
			if(pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId ))
			{
				QuatT safePos;
				const bool bRequiresAdjust = pEnvWeapon->GetIntersectionAssistUnit().GetSafeLocationForSubjectEntity(safePos);
				if(bRequiresAdjust)
				{
					Matrix34 adjustedwMat(safePos);
					pEntity->SetWorldTM(adjustedwMat);
				}
			}
			else
			{
				Matrix34 newEntityLocation = pEntity->GetWorldTM();
				newEntityLocation.SetTranslation(newEntityLocation.GetTranslation() - (throwDir * 0.25f));
				pEntity->SetWorldTM(newEntityLocation);
				throwDir = -throwDir;
			}
		}

		const static float kSpeedReductionVertical = 0.4f;

		speed = speed - (fabs_tpl(throwDir.z) * kSpeedReductionVertical * speed); // reduce throw speed slightly at sharp angles (both up + down)

		if(m_state == eST_CHARGED_THROW_POST_RELEASE)
		{
			// Find the object's centre to work out the throw direction from
			{
				AABB aabb;
				pEntity->GetPhysicsWorldBounds(aabb);
				//find a target and the path to it (if any) , or straight ahead throw
				throwDir = CalculateChargedThrowDir( gEnv->p3DEngine->GetRenderingCamera().GetMatrix(), aabb.GetCenter() ); 
			}	
		}

		//////////////////////////////////////////////////////////////////////////
		// Finally prepare the velocity to be set...

		if (!IsNPC())  
		{							
			pe_action_set_velocity asv;
			asv.v = (throwDir * speed);

			// this adds a variable rotation to the object around its center of mass
			if(m_state != eST_CHARGED_THROW_POST_RELEASE)
			{
				if(!gEnv->bMultiplayer)
				{
					AABB box;
					pEntity->GetWorldBounds(box);
					Vec3 finalW = -gEnv->pSystem->GetViewCamera().GetMatrix().GetColumn0()*(8.0f/max(0.1f,box.GetRadius()));

					finalW = finalW.CompMul(cry_random_componentwise(Vec3(0.5f), Vec3(1.3f)));

					asv.w = finalW;
				}
			}
			else
			{
				if( g_pGameCVars->pl_pickAndThrow.enviromentalWeaponUseThrowInitialFacingOveride )
				{
					//Start the throw with the entity at the desired orientation
					Quat initialRotation = Quat::CreateRotationXYZ(Ang3(throwParams.chargedThrowInitialFacingDir));
					Quat currentView(gEnv->pSystem->GetViewCamera().GetMatrix());
					initialRotation = currentView * initialRotation;
					initialRotation.Normalize();
					pEntity->SetRotation(initialRotation);
				}		
			}

			if (!gEnv->bMultiplayer && g_pGameCVars->pl_pickAndThrow.useProxies)
			{
				pe_params_part colltype;
				colltype.flagsColliderOR = CPickAndThrowProxy::geom_colltype_proxy;

				pPhysicalEntity->SetParams( &colltype );
			}

			pPhysicalEntity->Action(&asv);


			// This is for smaller objects to collide and bounce off living characters. It's automatically turned off when the object lost kinetic energy. Not needed for larger objects
			if(PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId ) != NULL)
			{
				pe_params_flags pf; 
				pf.flagsOR = ref_small_and_fast;
				pPhysicalEntity->SetParams(&pf);
			}
				
			if (m_state == eST_PUSHINGAWAY_POWER)
			{
				IEntityRender *pIEntityRender = pEntity->GetRenderInterface();
				IRenderNode *pRenderNode = (pIEntityRender != NULL) ? pIEntityRender->GetRenderNode() : NULL;
				if (pRenderNode)
				{
					SInfiniteViewDistEntity info;
					info.entityId = pEntity->GetId();
					info.oldViewDistRatio = pRenderNode->GetViewDistRatio();
					const float MAX_TIME_FOR_INFINITE_VIEW = 10;
					info.timeLimit = gEnv->pTimer->GetCurrTime() + MAX_TIME_FOR_INFINITE_VIEW;
					info.timeStopped = 0;
					info.valid = true;
					m_infiniteViewEntities.push_back( info );
						
					pRenderNode->SetViewDistUnlimited();
				}
			}

		}
		else
		{
			GetScheduler()->TimerAction(50, CSchedulerAction<DelayedNPCRagdollThrow>::Create(DelayedNPCRagdollThrow(pEntity->GetId(), (throwDir * speed))), true);
		}

		// Make the object visible to AI while it's in motion
		IEntity *pPlayerEntity = pOwnerPlayer->GetEntity();
		IAIObject *pPlayerAI = pPlayerEntity ? pPlayerEntity->GetAI() : NULL;
		if (pPlayerAI)
		{
			ObservableParams observableParams;
			observableParams.faction = pPlayerAI->GetFactionID();
			observableParams.typeMask = General;
			observableParams.observablePositionsCount = 1;
			observableParams.observablePositions[0] = pEntity->GetWorldPos();
			
			int skipListSize = 0;
			if (pPhysicalEntity)
				observableParams.skipList[skipListSize++] = pPhysicalEntity;
			if (IPhysicalEntity *pPhysics = pPlayerEntity->GetPhysics())
				observableParams.skipList[skipListSize++] = pPhysics;
			observableParams.skipListSize = skipListSize;

			g_pGame->GetGameAISystem()->GetVisibleObjectsHelper().RegisterObject(pEntity->GetId(), observableParams, 
					eVOR_OnlyWhenMoving|eVOR_FlagDropOnceInvisible, CPickAndThrowWeapon::OnThrownObjectSeen, this);

			// Tell the AI system that the player threw an object
			SAIEVENT e;
			e.targetId = pEntity->GetId();
			pPlayerAI->Event(AIEVENT_PLAYER_STUNT_THROW, &e);
		}

		if (!IsNPC())
		{
			EntityScripts::CallScriptFunction(pEntity, pEntity->GetScriptTable(), "OnThrown");
		}
	}

	if (g_pGame->GetGameAchievements())
		g_pGame->GetGameAchievements()->PlayerThrewObject(pEntity->GetId());
}


//---------------------------------------------------------------------------
Vec3 CPickAndThrowWeapon::CalculateChargedThrowDir(const Matrix34& attackerTransform, const Vec3& throwPos) const
{
	Vec3 target;
	bool foundTarget = false;

	const CPlayer* pPlayer = GetOwnerPlayer();
	CRY_ASSERT(pPlayer);

	Vec3 targetHeightOffset( 0, 0, g_pGameCVars->pl_pickAndThrow.chargedThrowAimHeightOffset );
	
	// If we have a target we can autoAim at, lets do so..
	if(pPlayer && pPlayer->IsClient() && g_pGameCVars->pl_pickAndThrow.chargedthrowAutoAimEnabled)
	{
		IEntity* pTargetPlayer = CalculateBestChargedThrowAutoAimTarget(attackerTransform);
		if(pTargetPlayer)
		{
			const SAutoaimTarget* pAutoAimInfo = g_pGame->GetAutoAimManager().GetTargetInfo(pTargetPlayer->GetId());
			if(pAutoAimInfo)
			{
				target = pAutoAimInfo->primaryAimPosition + targetHeightOffset;
				foundTarget = true;
			}
			else
			{
				// Backup.. if we cant get hold of the primaryaimPos from auto aim info...
				target = pTargetPlayer->GetWorldPos() + targetHeightOffset;
				foundTarget = true;
			}
		}
	}

	if( !foundTarget )
	{
		Vec3 viewDir;
		Vec3 viewPos;

		if( !pPlayer || pPlayer->IsClient() )
		{
			const CCamera& cam = gEnv->p3DEngine->GetRenderingCamera();
			viewDir = cam.GetViewdir();
			viewPos = cam.GetPosition();
		}
		else
		{
			if( IMovementController* pMC = pPlayer->GetMovementController() )
			{
				SMovementState info;
				pMC->GetMovementState(info);
				viewDir = info.eyeDirection;
				viewPos = info.eyePosition; 
			}
			else
			{
				CryLog( "PickAndThrow:: Panic no eyes!" );
				viewPos = pPlayer->GetEntity()->GetWorldPos();
				viewDir = Vec3( 0, 0, 1 );
			}
		}

		// if no auto aim, or no valid target we set the thrown entity to intersect camera view dir at max autoaim range
		const CCamera& cam = gEnv->p3DEngine->GetRenderingCamera();
		target = viewDir * g_pGameCVars->pl_pickAndThrow.chargedThrowAutoAimDistance + viewPos;
	}

#ifdef PICKANDTHROWWEAPON_DEBUGINFO
	CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
	if(pEnvWeapon)
	{
		pEnvWeapon->DebugSetThrowPath( throwPos, target );
	}
#endif

	return ( target - throwPos ).GetNormalized();

}

//---------------------------------------------------------------------------
IEntity* CPickAndThrowWeapon::CalculateBestChargedThrowAutoAimTarget(const Matrix34& attackerTransform) const
{

#ifdef PICKANDTHROWWEAPON_DEBUGINFO
	const ColorB red(127,0,0);
	const ColorB green(0,127,0);
	const ColorB brightGreen(0,255,0);
	const static float s_sphereDebugRad = 0.15f; 
#endif // #ifdef PICKANDTHROWWEAPON_DEBUGINFO

	IEntity* pBestTarget = NULL; 
	float    fBestScore  = 0.0f; 

	const TAutoaimTargets& players = g_pGame->GetAutoAimManager().GetAutoAimTargets();
	const int targetCount = players.size();

	// Cache commonly required constants for scoring
	const float autoAimDistSqrd   = g_pGameCVars->pl_pickAndThrow.chargedThrowAutoAimDistance * g_pGameCVars->pl_pickAndThrow.chargedThrowAutoAimDistance; 
	const Vec3&  attackerPos	    = attackerTransform.GetColumn3(); 
	const Vec3&  attackerFacing   = attackerTransform.GetColumn1(); 
	const float  coneSizeRads     = DEG2RAD(g_pGameCVars->pl_pickAndThrow.chargedThrowAutoAimConeSize); 
	IEntitySystem* pEntitySystem  = gEnv->pEntitySystem; 
	CPlayerVisTable* pVisTable    = g_pGame->GetPlayerVisTable(); 

	// For each target within the box, we do a dist + cone check
	TAutoaimTargets::const_iterator endIter = players.end(); 
	for(TAutoaimTargets::const_iterator iter = players.begin(); iter != endIter; ++iter)
	{
		// If entity exists and we are allowed to target them
		EntityId targetEntityId = iter->entityId; 
		IEntity* pEntity = pEntitySystem->GetEntity(targetEntityId); 
		if(pEntity && PickAndThrow::AllowedToTargetPlayer(GetOwnerId(),targetEntityId))
		{
			// If further than allowed dist, discard
			Vec3 targetTestPos; 

			// We test against target aabb centre to reduce error
			{
				AABB aabb;
				pEntity->GetPhysicsWorldBounds(aabb);
				targetTestPos = aabb.GetCenter(); 
			}

			Vec3 toTarget = targetTestPos - attackerPos; 
			if(toTarget.GetLengthSquared() < autoAimDistSqrd)
			{

				// If not within cone.. discard
				float theta = 0.0f; 
				if(PickAndThrow::TargetEntityWithinFrontalCone(attackerPos,targetTestPos, attackerFacing,coneSizeRads, theta))
				{
					// If cant see them .. next
					if(!pVisTable->CanLocalPlayerSee(targetEntityId, 5))
					{
						continue; 
					}

					// For each candidate, generate their Auto Aim score. 
					
					// 1) [0.0f,1.0f] score comprised of DISTANCE based score (closer is better)
					float targetDistScore    =  1.0f - ( toTarget.GetLength() * __fres(max(g_pGameCVars->pl_pickAndThrow.chargedThrowAutoAimDistance, FLT_EPSILON)) ); 

					// Lets try squaring this to make candidates with only small gaps between them reflect distance scoring better and reduce the importance of distance at super long ranges
					targetDistScore *= targetDistScore; 

					// 2) +  [0.0f,1.0f]  cone based score (central is better)
					const float targetConeScore		 =  1.0f - ( theta * __fres(max(coneSizeRads*0.5f, FLT_EPSILON)) );

					// Factor in designer controlled multipliers
					const float finalScore = (targetDistScore * g_pGameCVars->pl_pickAndThrow.chargedThrowAutoAimDistanceHeuristicWeighting) + 
																	 (targetConeScore * g_pGameCVars->pl_pickAndThrow.chargedThrowAutoAimAngleHeuristicWeighting); 

					if(finalScore > fBestScore)
					{
						fBestScore  = finalScore; 
						pBestTarget = pEntity; 
					}

					// Debug rendering!
#ifdef PICKANDTHROWWEAPON_DEBUGINFO
					if(g_pGameCVars->pl_pickAndThrow.chargedThrowDebugOutputEnabled)
					{
						CryWatch("Entity [%d] DistScore [%.2f] , ConeScore[%.2f], totalScore [%.3f]", pEntity->GetId(), targetDistScore, targetConeScore, finalScore); 

						// Draw a green sphere to indicate valid
						gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(targetTestPos, s_sphereDebugRad,green);
					}
#endif //#ifdef PICKANDTHROWWEAPON_DEBUGINFO

				}
#ifdef PICKANDTHROWWEAPON_DEBUGINFO
				else if(g_pGameCVars->pl_pickAndThrow.chargedThrowDebugOutputEnabled)
				{
					// Draw a red sphere to indicate not valid
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(targetTestPos, s_sphereDebugRad, red); 
				}
#endif //#ifdef PICKANDTHROWWEAPON_DEBUGINFO
			}
		}
	}

	// Draw a Really bright sphere on BEST target
#ifdef PICKANDTHROWWEAPON_DEBUGINFO
	if(pBestTarget && g_pGameCVars->pl_pickAndThrow.chargedThrowDebugOutputEnabled)
	{
		// If further than allowed dist, discard
		Vec3 targetTestPos = pBestTarget->GetPos(); 

		// We use aabb centre to reduce error
		{
			AABB aabb;
			pBestTarget->GetPhysicsWorldBounds(aabb);
			targetTestPos = aabb.GetCenter(); 
		}

		// Draw a red sphere to indicate not valid
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(targetTestPos, s_sphereDebugRad*1.05f, brightGreen); 
	}
#endif //#ifdef PICKANDTHROWWEAPON_DEBUGINFO

	return pBestTarget; 
}



//static---------------------------------------------------------------------------
void CPickAndThrowWeapon::OnThrownObjectSeen(const Agent& agent, EntityId objectId, void *pVPickAndThrowWeapon)
{
	CPickAndThrowWeapon *pPickAndThrowWeapon = static_cast<CPickAndThrowWeapon*>(pVPickAndThrowWeapon);
	assert(pPickAndThrowWeapon);

	CPlayer *pPlayer = pPickAndThrowWeapon ? pPickAndThrowWeapon->GetOwnerPlayer() : NULL;
	IEntity *pPlayerEntity = pPlayer ? pPlayer->GetEntity() : NULL;
	IAIObject *pPlayerAI = pPlayerEntity ? pPlayerEntity->GetAI() : NULL;

	IAISystem *pAISystem = gEnv->pAISystem;
	ITargetTrackManager *pManager = pAISystem ? pAISystem->GetTargetTrackManager() : NULL;
	if (pManager && agent.IsValid() && pPlayerAI)
	{
		const tAIObjectID aiPlayerId = pPlayerAI->GetAIObjectID();
		const tAIObjectID aiTargetId = agent.GetAIObjectID();

		bool bAlert = false;
		if (IAIActorProxy* pTargetAIProxy = agent.GetAIActor()->GetProxy())
		{
			const int alertness = pTargetAIProxy->GetAlertnessState();
			bAlert = (alertness >= 2);
		}

		IEntity *pObject = gEnv->pEntitySystem->GetEntity(objectId);

		TargetTrackHelpers::SStimulusEvent eventInfo;
		eventInfo.vPos = (!bAlert && pObject ? pObject->GetWorldPos() : pPlayerAI->GetPos());
		eventInfo.eStimulusType = TargetTrackHelpers::eEST_Generic;
		eventInfo.eTargetThreat = AITHREAT_THREATENING;
		pManager->HandleStimulusEventForAgent(aiTargetId, aiPlayerId, "SeeThrownObject", eventInfo);

		// Send signal to target AI
		IAISignalExtraData *pData = pAISystem->CreateSignalExtraData();
		pData->nID = objectId;
		pAISystem->SendSignal(SIGNALFILTER_SENDER, AISIGNAL_DEFAULT, "OnThrownObjectSeen", agent.GetAIObject(), pData);
	}
}


//---------------------------------------------------------------------------
void CPickAndThrowWeapon::DropObject()
{
	DetachObject();

	CPlayer* pOwnerPlayer = GetOwnerPlayer();

	IEntity *pEntity = GetEntityObject();
	IPhysicalEntity *pPhysicalEntity = (pEntity != NULL) ? pEntity->GetPhysics() : NULL;
	if (!pPhysicalEntity)
		return;
		
	// AI should ignore collisions from this item for a while
	// to not 'scare' himself and the friends around him
	IPerceptionManager* pPerceptionManager = IPerceptionManager::GetInstance();
	if (pPerceptionManager)
	{
		pPerceptionManager->IgnoreStimulusFrom(pEntity->GetId(), AISTIM_COLLISION, 2.0f);
		pPerceptionManager->IgnoreStimulusFrom(pEntity->GetId(), AISTIM_SOUND, 2.0f);
	}

	pe_params_pos ppos;
	ppos.pos = pEntity->GetWorldPos();
	pPhysicalEntity->SetParams(&ppos);

	pe_action_set_velocity asv;
	asv.v.Set(0 , 0, -0.1f);
	
	const bool isNPC = IsNPC();
	if (GetGrabTypeParams().dropSpeedForward > 0) // extra forward impulse
	{
		Vec3 playerDir2D = pOwnerPlayer->GetViewQuat().GetColumn1();
		
		Vec3 impulse = pOwnerPlayer->GetActorPhysics().velocity + playerDir2D * GetGrabTypeParams().dropSpeedForward;
		asv.v.x += impulse.x;
		asv.v.y += impulse.y;
		
		if (isNPC)
		{
			GetScheduler()->TimerAction(50, CSchedulerAction<DelayedNPCRagdollThrow>::Create(DelayedNPCRagdollThrow(pEntity->GetId(), asv.v)), true);
		}
	}

	pPhysicalEntity->Action(&asv);
	
	if (pOwnerPlayer)
	{
		pOwnerPlayer->NotifyObjectGrabbed( false, m_objectId, isNPC );
	}
}

//---------------------------------------------------------------------------
IAttachment* CPickAndThrowWeapon::GetOwnerAttachment(const int slotIndex) const
{
	IAttachment* pAttachment = NULL;
	
	IEntity* pEntity = GetOwner();
	SEntitySlotInfo info;
	if (pEntity && pEntity->GetSlotInfo(slotIndex, info))
	{
		if (info.pCharacter)
		{
			pAttachment = info.pCharacter->GetIAttachmentManager()->GetInterfaceByName( GetGrabTypeParams().attachment.c_str() );
		}
	}

	return pAttachment;
}			

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::SetViewMode(int mode)
{
	if(m_objectId)
	{
		const bool bToggleFirstThirdPerson = ((m_stats.viewmode == eIVM_FirstPerson ||
																					m_stats.viewmode == eIVM_ThirdPerson) &&
																					(mode == eIVM_FirstPerson ||
																					mode == eIVM_ThirdPerson) &&
																					mode != m_stats.viewmode) ? true : false;

		if(bToggleFirstThirdPerson)
		{
			DetachObject();
		}

		CWeapon::SetViewMode(mode);

		if(bToggleFirstThirdPerson)
		{
			AttachObject();
		}
	}
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::AttachObject(const bool bIsRefresh /*= false*/)
{
	const int mainCharacterSlot = 0;
	const int shadowCharacterSlot = 5;

	IEntity* pObjectEntity = GetEntityObject();
	IAttachment* pAttachment = GetOwnerAttachment(mainCharacterSlot);

	IF_UNLIKELY (pObjectEntity == NULL)
		return;

	IF_UNLIKELY (pAttachment == NULL)
		return;

	AttachmentUtils::TAttachmentFlags flags = AttachmentUtils::eAF_SyncRenderNearest;
	if(gEnv->bMultiplayer)
	{
		flags |= AttachmentUtils::eAF_AllowShadowCharAtt;
	}

	//////////////////////////////////////////////////////////////////////////
	/// Notify 'Environmental weapon' if the object we just picked up is one of them

	CEnvironmentalWeapon* pEnvironmentalWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
	if (pEnvironmentalWeapon)
	{
		pEnvironmentalWeapon->OnAttached();
		flags |= AttachmentUtils::eAF_SyncCloak;
	}

	m_objectGrabbed = true;
	m_objectScale = pObjectEntity->GetScale();

	//////////////////////////////////////////////////////////////////////////
	/// Setup physics constrains and correct rendering mode on the object

	const bool isOwnerFP = IsOwnerFP();
	const bool isObjectNPC = IsNPC();

	SetUpCollisionPropertiesAndMassInfo( true );

	//////////////////////////////////////////////////////////////////////////
	/// Create actual attachment and bind the entity to it

	AttachmentUtils::AttachObject( isOwnerFP, GetOwner(), pObjectEntity, GetGrabTypeParams().attachment.c_str(), flags, mainCharacterSlot, shadowCharacterSlot );

	//////////////////////////////////////////////////////////////////////////
	/// Set Attachment Offset.

	QuatT requiredRelativeLocation;

	if (isObjectNPC)
	{
		m_attachmentOldRelativeLoc = pAttachment->GetAttRelativeDefault();

		requiredRelativeLocation.SetIdentity();
		pAttachment->SetAttRelativeDefault(requiredRelativeLocation);
	}
	else
	{
		m_attachmentOldRelativeLoc = pAttachment->GetAttRelativeDefault();

		Matrix34 objectHelperInvMat;
		CalcObjectHelperInverseMat( objectHelperInvMat );
		requiredRelativeLocation = m_attachmentOldRelativeLoc * QuatT(objectHelperInvMat.GetTranslation(), Quat(objectHelperInvMat).GetNormalized());
		requiredRelativeLocation.q.Normalize();

		//////////////////////////////////////////////////////////////////////////
		// Set Shadow Offsets
		IAttachment* pShadowAttachment = isOwnerFP ? GetOwnerAttachment(shadowCharacterSlot) : NULL;
		if(pShadowAttachment)
		{
			pShadowAttachment->SetAttRelativeDefault(requiredRelativeLocation);
		}
	}
	pAttachment->SetAttRelativeDefault(requiredRelativeLocation);

	//////////////////////////////////////////////////////////////////////////
	// Ensure environmental weapon respects player's cloak status
	if (pEnvironmentalWeapon && !bIsRefresh)
	{
		PickAndThrow::NotifyRecordingSystemOnUsed(GetOwnerId(), GetEntityId(), pEnvironmentalWeapon->GetEntityId(), true);
	}

	// By default the user can simply letgo/throw the equipped item. This can be overriden
	if(GetGrabTypeParams().m_propertyFlags & EPF_hideInteractionPrompts)
	{
		m_heldEntityInteractionType = eInteraction_None; 
	}
	else
	{
		m_heldEntityInteractionType = eInteraction_LetGo; 
	}
}

void CPickAndThrowWeapon::DetachObject( const bool bIsRefresh /*= false*/ )
{
	const int mainCharacterSlot = 0;
	const int shadowCharacterSlot = 5;
	AttachmentUtils::TAttachmentFlags flags = AttachmentUtils::eAF_SyncRenderNearest;

	//////////////////////////////////////////////////////////////////////////
	/// Remove cloak if necessary on the entity
	CEnvironmentalWeapon* pEnvironmentalWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
	if(pEnvironmentalWeapon)
	{
		flags |= AttachmentUtils::eAF_SyncCloak;

		if(!bIsRefresh)
		{
			PickAndThrow::NotifyRecordingSystemOnUsed(GetOwnerId(), GetEntityId(), pEnvironmentalWeapon->GetEntityId(), false);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	/// If we have another controller ensalved, free it, but not for an AI that will happen a bit later
	if(!IsNPC())
	{
		EnslaveTarget(false);
	}

	//////////////////////////////////////////////////////////////////////////
	/// Restore offsets.
	if (IAttachment* pAttachment = GetOwnerAttachment(mainCharacterSlot))
	{
		pAttachment->SetAttRelativeDefault(m_attachmentOldRelativeLoc);
	}

	if(IAttachment* pShadowAttachment = GetOwnerAttachment(shadowCharacterSlot))
	{
		pShadowAttachment->SetAttRelativeDefault(m_attachmentOldRelativeLoc);
	}

	//////////////////////////////////////////////////////////////////////////
	// DetachObject and sync flags.
	AttachmentUtils::DetachObject( GetOwner(), GetEntityObject(), GetGrabTypeParams().attachment.c_str(), flags, mainCharacterSlot, shadowCharacterSlot );

	//////////////////////////////////////////////////////////////////////////
	// No interaction type now we are no longer holding an entity. 
	m_heldEntityInteractionType = eInteraction_None; 
	m_objectGrabbed = false;

	//////////////////////////////////////////////////////////////////////////
	/// Notify environmental weapon once more
	if(pEnvironmentalWeapon && !bIsRefresh)
	{
		const bool chargedThrow = m_state == eST_CHARGED_THROW_POST_RELEASE;
		pEnvironmentalWeapon->OnDetachedFrom( GetOwnerActor(), chargedThrow );
	}
}
//---------------------------------------------------------------------------
IEntity* CPickAndThrowWeapon::GetEntityObject() 
{
	IEntity *pEntity = m_pEntitySystem->GetEntity( m_objectId );
	return pEntity;
}

//---------------------------------------------------------------------------
const IEntity* CPickAndThrowWeapon::GetEntityObject() const
{
	IEntity *pEntity = m_pEntitySystem->GetEntity( m_objectId );
	return pEntity;
}

//---------------------------------------------------------------------------
const SPickAndThrowParams::SGrabTypeParams& CPickAndThrowWeapon::GetGrabTypeParams() const
{
	assert( (int)m_weaponsharedparams->pPickAndThrowParams->grabTypesParams.size()>m_grabType );
	const SPickAndThrowParams::SGrabTypeParams& params = m_weaponsharedparams->pPickAndThrowParams->grabTypesParams[m_grabType];
	return params;
}


//---------------------------------------------------------------------------
const SPickAndThrowParams::SThrowParams& CPickAndThrowWeapon::GetThrowParams() const
{
	return GetGrabTypeParams().throwParams;
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::DecideGrabType()
{
	int numTypes = m_weaponsharedparams->pPickAndThrowParams->grabTypesParams.size();
	m_grabType = 0; //Default one
	
	if (CActor* pActor = GetNPCGrabbedActor())
	{
		m_isNPC = true;
		CPlayer *pOwner = GetOwnerPlayer();
		Vec3 playerDir2D = pOwner ? pOwner->GetBaseQuat().GetColumn1() : FORWARD_DIRECTION;
		Vec3 NPCDir2D = GetNPCGrabbedPlayer()->GetBaseQuat().GetColumn1();

		SmartScriptTable props;
		IScriptTable* pScriptTable = pActor->GetEntity()->GetScriptTable();
		stack_string grabType;
		if (pScriptTable && pScriptTable->GetValue("Properties", props))
		{
			SmartScriptTable propsPlayerInteractions;
			if (props->GetValue("PlayerInteractions", propsPlayerInteractions))
			{
				ScriptAnyValue grabTypeScript;
				propsPlayerInteractions->GetValueAny("esGrabType", grabTypeScript);
				if (grabTypeScript.GetVarType()==svtString)
					grabType = grabTypeScript.GetString();
			}
		}

		for (int i = 0; i < numTypes; ++i)
		{
			const SPickAndThrowParams::SGrabTypeParams& grabParams = m_weaponsharedparams->pPickAndThrowParams->grabTypesParams[i];
			
			if ( (grabParams.m_propertyFlags & EPF_isNPCType) && (grabType == grabParams.NPCtype.c_str()) )
			{
				m_grabType = i;
				return;
			}
		}
		assert( false );
	}
	else
	{
		m_isNPC = false;

		HelperInfo helperInfo = GlobalFindHelperObject( m_objectId );
		if (helperInfo.Valid())
		{
			m_objectHelperSlot = helperInfo.m_slot;
			m_grabType = helperInfo.m_grabTypeIdx;
		}
	}
}

//---------------------------------------------------------------------------
bool CPickAndThrowWeapon::HasObjectGrabHelper(EntityId entityId) const
{
	return GlobalFindHelperObject( entityId ).Valid();
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::OnSimpleMeleeFinished()
{
	if (!IsNPC())
		m_state = eST_IDLE;
	m_currentMeleeAutoTargetId = 0;
	
	SetBusy(false);
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::OnDoSimpleMelee()
{
	if (CActor *pActor = GetOwnerActor())
	{
		if (IMovementController * pMC = pActor->GetMovementController())
		{
			SMovementState info;
			pMC->GetMovementState(info);

			m_collisionHelper.DoCollisionTest(SCollisionTestParams(info.eyePosition, info.eyeDirection, m_meleeRange, pActor->GetEntityId(), m_objectId, false));
		}
	}
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::OnSuccesfulHit( const ray_hit& hitResult )
{
	IEntity *pTarget = gEnv->pEntitySystem->GetEntityFromPhysics(hitResult.pCollider);
	EntityId targetId = pTarget ? pTarget->GetId() : 0;

	int hitTypeID = DoSimpleMeleeHit(hitResult, targetId);

	DoSimpleMeleeImpulse(hitResult, targetId, hitTypeID);
	
	PlayActionLocal( GetGrabTypeParams().melee_hit_action );

	m_currentMeleeAutoTargetId = 0;
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::OnFailedHit()
{
	const SCollisionTestParams& collisionParams = m_collisionHelper.GetCollisionTestParams();

	if (m_currentMeleeAutoTargetId != 0)
	{
		m_collisionHelper.PerformMeleeOnAutoTarget(m_currentMeleeAutoTargetId);
	}

	m_currentMeleeAutoTargetId = 0;
}

//---------------------------------------------------------------------------
int CPickAndThrowWeapon::DoSimpleMeleeHit( const ray_hit& hitResult, EntityId collidedEntityId )
{
	// generate the damage
	CActor *pOwnerActor = GetOwnerActor();
	int hitTypeId = 0;

	if (pOwnerActor)
	{
		const SCollisionTestParams& collisionParams = m_collisionHelper.GetCollisionTestParams();

		IEntity *pTarget = gEnv->pEntitySystem->GetEntity(collidedEntityId);
		IEntity* pOwnerEntity = pOwnerActor->GetEntity();

		bool isFriendlyHit = false;

		if (IAIObject* pOwnerAI = pOwnerEntity->GetAI())
		{
			SAIEVENT AIevent;
			AIevent.targetId = pTarget ? pTarget->GetId() : 0;
			pOwnerAI->Event(AIEVENT_PLAYER_STUNT_PUNCH, &AIevent);

			IAIObject *pTargetAI = pTarget ? pTarget->GetAI() : NULL;
			isFriendlyHit = (pTargetAI) ? !pOwnerAI->IsHostile(pTargetAI, false) : false;

			// Send target stimuli
			IAISystem *pAISystem = gEnv->pAISystem;
			ITargetTrackManager *pTargetTrackManager = pAISystem ? pAISystem->GetTargetTrackManager() : NULL;
			if (pTargetTrackManager && pTargetAI)
			{
				const tAIObjectID aiOwnerId = pOwnerAI->GetAIObjectID();
				const tAIObjectID aiTargetId = pTargetAI->GetAIObjectID();

				TargetTrackHelpers::SStimulusEvent eventInfo;
				eventInfo.vPos = hitResult.pt;
				eventInfo.eStimulusType = TargetTrackHelpers::eEST_Generic;
				eventInfo.eTargetThreat = AITHREAT_AGGRESSIVE;
				pTargetTrackManager->HandleStimulusEventForAgent(aiTargetId, aiOwnerId, "MeleeHit", eventInfo);
				pTargetTrackManager->HandleStimulusEventInRange(aiOwnerId, "MeleeHitNear", eventInfo, 5.0f);
			}
		}

		if(!isFriendlyHit)
		{
			const SPickAndThrowParams::SGrabTypeParams& grabTypeParams = GetGrabTypeParams(); 
			float  damage = m_objectMass * grabTypeParams.melee_damage_scale;
			
			//Generate Hit
			if(pTarget)
			{
				CGameRules *pGameRules = g_pGame->GetGameRules();

				hitTypeId = CGameRules::EHitType::Melee;
				HitInfo info(GetOwnerId(), pTarget->GetId(), GetEntityId(),
					damage, 0.0f, hitResult.surface_idx, hitResult.partid,
					hitTypeId, hitResult.pt, collisionParams.m_dir, hitResult.n);

				info.knocksDown = (cry_random(20.0f, 50.0f) < m_objectMass);
				info.remote = collisionParams.m_remote;

				pGameRules->ClientHit(info);
			}

			//Play Material FX
			IMaterialEffects* pMaterialEffects = gEnv->pGameFramework->GetIMaterialEffects();

			TMFXEffectId effectId = pMaterialEffects->GetEffectId(GetGrabTypeParams().melee_mfxLibrary.c_str(), hitResult.surface_idx);
			if (effectId != InvalidEffectId)
			{
				SMFXRunTimeEffectParams params;
				params.pos = hitResult.pt;
				params.playflags = eMFXPF_All | eMFXPF_Disable_Delay;
				//params.soundSemantic = eSoundSemantic_Player_Foley;
				pMaterialEffects->ExecuteEffect(effectId, params);
			}
		}
	}

	return hitTypeId;
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::DoSimpleMeleeImpulse( const ray_hit& hitResult, EntityId collidedEntityId, int hitTypeID )
{
	CActor* pWeaponOwner = GetOwnerActor();
	
	float impulseScale = 1.0f;
	bool delayImpulse = false;

	IEntity* pCollidedEntity = gEnv->pEntitySystem->GetEntity(collidedEntityId);
	if (pCollidedEntity)
	{
		IGameFramework* pGameFrameWork = g_pGame->GetIGameFramework();
			
		CActor* pColliderActor = static_cast<CActor*>(pGameFrameWork->GetIActorSystem()->GetActor(collidedEntityId));
		if (pColliderActor)
		{
			if (pColliderActor->IsDead())
			{
				delayImpulse = true;
				impulseScale = pColliderActor->GetActorParams().meeleHitRagdollImpulseScale; 
			}
		}
	}

	if (hitResult.pCollider)
	{
		const SCollisionTestParams& collisionParams = m_collisionHelper.GetCollisionTestParams();
		Vec3 impulse = collisionParams.m_dir * m_objectMass * GetGrabTypeParams().melee_impulse_scale * impulseScale; //Scale up a bit, maybe expose this in xml as well.

		if (!delayImpulse)
		{
			m_collisionHelper.Impulse(hitResult.pCollider, hitResult.pt, impulse, hitResult.partid, hitResult.ipart, hitTypeID);
		}
		else
		{
			GetScheduler()->TimerAction(75, CSchedulerAction<DelayedMeleeImpulse>::Create(DelayedMeleeImpulse(*this, collidedEntityId, hitResult.pt, impulse, hitResult.partid, hitResult.ipart, hitTypeID)), true);
		}

		const Vec3 speed = collisionParams.m_dir * sqrt_tpl(2000.0f/(80.0f * 0.5f)); // 80.0f is the mass of the player
		m_collisionHelper.GenerateArtificialCollision(GetOwner(), hitResult.pCollider, hitResult.pt, hitResult.n, speed, hitResult.partid, hitResult.ipart, hitResult.surface_idx, hitResult.iPrim);
	}
}

//---------------------------------------------------------------------------
// called right before the soldier is attached to the player - that is. when visually the soldier goes "in" player's hand
void CPickAndThrowWeapon::OnNpcAttached()
{
	CActor *pOwnerActor = GetOwnerActor();
	CActor *pGrabbedActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor( m_objectId ));

	if((pGrabbedActor != NULL) && (pOwnerActor != NULL))
	{
		pGrabbedActor->SetGrabbedByPlayer(pOwnerActor->GetEntity(), true);

		//The NPC drops his weapon
		CItem *pCurrentItem = static_cast<CItem*>(pGrabbedActor->GetCurrentItem());

		if (pCurrentItem)
		{
			if (pCurrentItem->IsMounted())
			{
				if (pCurrentItem->IsUsed())
					pCurrentItem->StopUse( pGrabbedActor->GetEntityId() );
			}
			else
			{
				pCurrentItem->Drop(1.0f, false);
			}
		}

		PlayCommunication( "choke_grab", "Choking" );
	}

	m_timePassed = 0.0f;
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::PrepareNPCToBeDropped(bool hasNPCBeenThrown /* = false */)
{
	CActor *pActorNPC = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor( m_objectId ));

	if(!pActorNPC)
		return;
		
	SendMortalHitTo( pActorNPC );

	pActorNPC->SetGrabbedByPlayer(GetOwner(), false);
	
	if(hasNPCBeenThrown)
		PlayCommunication( "comm_falling_death", "Falling" );
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::SendMortalHitTo( CActor* pActor )
{
	// TODO: find a more proper way of killing the NPC

	uint16 pickAndThrowClassId = ~uint16(0);
	g_pGame->GetIGameFramework()->GetNetworkSafeClassId(pickAndThrowClassId, "PickAndThrowWeapon");

	assert( pActor );
	HitInfo hitInfo;
	hitInfo.shooterId = GetOwnerId();
	hitInfo.targetId = pActor->GetEntityId();
	hitInfo.weaponId = GetEntityId();
	hitInfo.weaponClassId = pickAndThrowClassId;
	hitInfo.damage = 99999.0f; // CERTAIN_DEATH
	hitInfo.dir = pActor->GetEntity()->GetForwardDir();
	hitInfo.normal = -hitInfo.dir; // this has to be in an opposite direction from the hitInfo.dir or the hit is ignored as a 'backface' hit
	hitInfo.type = CGameRules::EHitType::SilentMelee;
	g_pGame->GetGameRules()->ClientHit(hitInfo);
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::PlayCommunication( const char* pCommName, const char* pChannelName )
{
	IEntity* pEntity = GetEntityObject();
	if (!pEntity)
		return;

	IAIObject* pAI = pEntity->GetAI();
	if (!pAI)
		return;

	IAIActorProxy* pAIProxy = pAI->GetProxy();
	if (!pAIProxy)
		return;

	if (IAISystem* pAISystem = gEnv->pAISystem)
	{
		if (ICommunicationManager* pCommManager = pAISystem->GetCommunicationManager())
		{
			SCommunicationRequest request;

			request.actorID = pEntity->GetId();
			request.channelID = pCommManager->GetChannelID(pChannelName);
			request.commID = pCommManager->GetCommunicationID(pCommName);
			request.configID = pCommManager->GetConfigID(pAIProxy->GetCommunicationConfigName());
			request.contextExpiry = 0;
			request.ordering = SCommunicationRequest::Ordered;
			pCommManager->PlayCommunication(request);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
bool CPickAndThrowWeapon::IsGrabStillValid() const
{
	if (IsNPC())
	{
		const CActor *pActorNPC = GetNPCGrabbedActor();
		if (!pActorNPC || pActorNPC->IsDead())
			return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
const CActor* CPickAndThrowWeapon::GetNPCGrabbedActor() const
{
	const CActor *pActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor( m_objectId ));
	return pActor;
}


//////////////////////////////////////////////////////////////////////////
CPlayer* CPickAndThrowWeapon::GetNPCGrabbedPlayer()
{
	CActor* pActor = GetNPCGrabbedActor();
	assert( CPlayer::GetActorClassType() == pActor->GetActorClass() );
	CPlayer *pPlayer = static_cast<CPlayer*>(pActor);
	return pPlayer;
}



//---------------------------------------------------------------------------
CPlayer* CPickAndThrowWeapon::GetOwnerPlayer()
{
	assert( !GetOwnerActor() || GetOwnerActor()->IsPlayer() );  
	return static_cast<CPlayer*>(GetOwnerActor());
}

const CPlayer* CPickAndThrowWeapon::GetOwnerPlayer() const
{
	assert( !GetOwnerActor() || GetOwnerActor()->IsPlayer() );  
	return static_cast<CPlayer*>(GetOwnerActor());
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::FullSerialize( TSerialize ser )
{
	CWeapon::FullSerialize(ser);

	//assumes that re-picking the object will reset everything..
	ser.Value("m_objectId", m_objectId);
	ser.Value("m_objectGrabbed", m_objectGrabbed);
	m_bSerializeObjectGrabbed = m_objectGrabbed;
	
	if (ser.IsReading())
	{
		ResetTrackingOfExternalEntities();
	}
}


//---------------------------------------------------------------------------
void CPickAndThrowWeapon::PostSerialize()
{
	CWeapon::PostSerialize();
	CActor *pOwner = GetOwnerActor();
	if(m_objectId && pOwner && pOwner->IsPlayer() && m_bSerializeObjectGrabbed)
	{		
		CPlayer *pPlayer = static_cast<CPlayer*>(pOwner);
		pPlayer->EnterPickAndThrow(m_objectId, true, true );
	}
}

void CPickAndThrowWeapon::RequestStartThrow( EState throwState )
{
	if (!PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId ))
	{
		Start_Throw(throwState);
	}
	else
	{
		if(gEnv->bServer)
		{
			PTLogThrow("SERVER - RequestStartThrow() - Start_Throw() being called on server with subsequent Netserialise");
			m_throwCounter = (m_throwCounter + 1) % kThrowCounterMax;
			m_RequestedThrowState = throwState;
			CHANGED_NETWORK_STATE(this, ASPECT_STREAM);
			Start_Throw(m_RequestedThrowState);
			RequireUpdate( eIUS_General );
		}
		else
		{
			PTLogThrow("CLIENT - SvRequestThrow() RMI being called by client, for throw type %d", throwState);
			if (GetOwnerPlayer()->IsClient())
			{
				m_RequestedThrowState = throwState;
				Start_Throw(m_RequestedThrowState);
				RequireUpdate( eIUS_General );
			}

			if(throwState != eST_CHARGED_THROW_POST_RELEASE)
			{
				GetGameObject()->InvokeRMI(SvRequestThrow(), SvRequestThrowParams(throwState), eRMI_ToServer);
			}
			else
			{
				GetGameObject()->InvokeRMI(SvRequestChargedThrowPostRelease(), NoParams(), eRMI_ToServer);
			}
		}
	}
}

// Kick off the 'pre-release hold-to-charge' part of a charged throw
void CPickAndThrowWeapon::RequestChargedThrowPreRelease( )
{
	if (!PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId ))
	{
		Start_Charged_Throw_PreRelease(); 
	}
	else
	{
		if(gEnv->bServer)
		{
			PTLogThrow("SERVER - RequestChargedThrowPreRelease() - Start_Charged_Throw_PreRelease() being called on server with subsequent Netserialise");
			m_throwCounter = (m_throwCounter + 1) % kThrowCounterMax;
			m_RequestedThrowState = eST_CHARGED_THROW_PRE_RELEASE; 
			Start_Charged_Throw_PreRelease(); 
			CHANGED_NETWORK_STATE(this, ASPECT_STREAM);
			RequireUpdate( eIUS_General );
		}
		else
		{
			PTLogThrow("CLIENT - RequestChargedThrowPreRelease() RMI being called by client");
			if (GetOwnerPlayer()->IsClient())
			{
				m_RequestedThrowState = eST_CHARGED_THROW_PRE_RELEASE;
				Start_Charged_Throw_PreRelease(); 
				RequireUpdate( eIUS_General );
			}

			GetGameObject()->InvokeRMI(SvRequestChargedThrowPreRelease(),NoParams(), eRMI_ToServer);
		}
	}
}



void CPickAndThrowWeapon::Start_Charged_Throw_PreRelease()
{
	PTLogThrow("Start_Charged_Throw_PreRelease()  being called");
	PlayActionLocal( "hold" );
	
	PowerThrowPrepareSound(true);

	m_obstructionChecker.DoCheck( GetOwnerActor(), m_objectId );

	m_state = eST_CHARGED_THROW_PRE_RELEASE; 
	m_chargedThrowState = eCTS_PRIMING; 
}
//////////////////////////////////////////////////////////////////////////
// starts the chain of states that produce the throw of the object/npc

void CPickAndThrowWeapon::Start_Throw( EState throwState )
{
	// Cache this and re-use. 
	const SPickAndThrowParams::SThrowParams& throwParams = GetThrowParams();

	assert( throwState==eST_PUSHINGAWAY || throwState==eST_PUSHINGAWAY_POWER || throwState==eST_PUSHINGAWAY_WEAK || 
			    throwState == eST_CHARGED_THROW_POST_RELEASE);
	
	m_state = throwState;
	
	if(throwState == eST_CHARGED_THROW_POST_RELEASE)
	{
		PTLogThrow("Start_Throw()  being called for eST_CHARGED_THROW_POST_RELEASE");
		PowerThrowPrepareSound(false);
	}

	m_timePassed = 0;

	if( throwState == eST_PUSHINGAWAY_WEAK )
	{
		PlayActionLocal( GetGrabTypeParams().drop_action );
	}
	else
	{
		// Get correct throw action 
		const ItemString& thowAction = (throwState == eST_CHARGED_THROW_POST_RELEASE) ? throwParams.chargedObjectThrowAction : throwParams.throw_action;
		PlayActionLocal( thowAction, false, eIPAF_Default, g_pGameCVars->pl_pickAndThrow.environmentalWeaponThrowAnimationSpeed );
	}

	m_obstructionChecker.DoCheck(GetOwnerActor(), m_objectId);
}



//////////////////////////////////////////////////////////////////////////
// starts the chain of states that drops the object/npc

void CPickAndThrowWeapon::Start_Drop( bool withAnimation )
{
	m_timePassed = 0;
	if (withAnimation)
		PlayActionLocal( "Drop" ); 
	m_state = eST_STARTDROPPING;

	if (IsNPC())
	{
		PrepareNPCToBeDropped(withAnimation);
		m_flyingActorInfo.entityId = m_objectId;
		m_flyingActorInfo.timeToEndTracking = GetGrabTypeParams().timeToFreeAtDropping;
		m_flyingActorInfo.valid = true;
		if (GetOwnerPlayer())
			GetOwnerPlayer()->ExitPickAndThrow();
		m_objectCollisionsLimited = false;
	}
	DropObject();

	m_obstructionChecker.DoCheck(GetOwnerActor(), m_objectId);
}

void CPickAndThrowWeapon::OnBeginCutScene()
{
	if (m_objectGrabbed)
	{
		if (IsNPC())
			PrepareNPCToBeDropped();
		DropObject();
	
		if (m_objectCollisionsLimited)
		{
			SetUpCollisionPropertiesAndMassInfo( false );
		}

		AttachmentUtils::SetEntityRenderInNearestMode( GetEntityObject(), false );
	}

	m_state = eST_UNKNOWN;
	m_currentMeleeAutoTargetId = 0;
	m_objectMass = 0.0f;
	m_meleeRange = 1.5f;

	CPlayer* pOwnerPlayer = GetOwnerPlayer();
	if (pOwnerPlayer)
	{
		pOwnerPlayer->ExitPickAndThrow();
	}
}

void CPickAndThrowWeapon::OnEndCutScene()
{

}

float CPickAndThrowWeapon::GetMeleeRange() const
{
	return m_meleeRange;
}


//////////////////////////////////////////////////////////////////////////
// returns true if is holding something that should protect the player even when is the player who is physically receiving the hit

bool CPickAndThrowWeapon::ShouldFilterOutHitOnOwner( const HitInfo& hit )
{
	if (m_state==eST_IDLE && IsNPC() && GetOwnerPlayer() && GetNPCGrabbedActor())
	{
		if (hit.dir.dot( GetOwnerPlayer()->GetViewRotation().GetColumn1() ) < 0 )
		{
			m_hitsTakenByNPC++;
			if (m_hitsTakenByNPC>=GetGrabTypeParams().shieldingHits)
			{
				RequestStartThrow( eST_PUSHINGAWAY_WEAK );
			}
			else
			{
				CAudioSignalPlayer::JustPlay( GetGrabTypeParams().audioSignalHitOnNPC, m_objectId );
			}
			return m_hitsTakenByNPC<=GetGrabTypeParams().shieldingHits;  // should be false only when the shieldingHits value is 0 in the .xml.
		}
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////
// returns true if is holding something that should protect the player even when is the player who is physically receiving the hit

bool CPickAndThrowWeapon::ShouldFilterOutExplosionOnOwner( const HitInfo& hitInfo )
{
	// protect in MP and if holding an environmental weapon. for NOW (simple) dont protect during melee anims etc. 
	IEntity* pEntity								  = GetEntityObject();
	CPlayer* pPlayer									= GetOwnerPlayer();
	if((m_state == eST_IDLE) && !IsNPC() && (pPlayer != NULL) && (pEntity != NULL))
	{
		// Currently only environmental weapons can protect from explosion damage
		CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
		if(pEnvWeapon)
		{
			IScriptTable* pObjectEntityScript = pEntity->GetScriptTable();
			if (pObjectEntityScript)
			{
				SmartScriptTable propertiesTable;
				if (pObjectEntityScript->GetValue("Properties", propertiesTable))
				{
					float shieldsFromExplosionConeAngle = false; 
					if(propertiesTable->GetValue("shieldsFromExplosionConeAngle", shieldsFromExplosionConeAngle) && shieldsFromExplosionConeAngle > 0.0f)
					{
						// convert
						shieldsFromExplosionConeAngle = DEG2RAD(shieldsFromExplosionConeAngle); 

						// 180 degree cone of protection whilst testing
						float theta = acos(-(hitInfo.dir).dot( pPlayer->GetViewRotation().GetColumn1() ));
						if ( theta < (0.5f * shieldsFromExplosionConeAngle) )
						{
							return true; 
						}
					}
				}
			}
		}
	}
	return false; 

}

//////////////////////////////////////////////////////////////////////////
void CPickAndThrowWeapon::PlayActionLocal( const ItemString& actionName, bool loop, uint32 flags, const float speedOverride)
{
	FragmentID fragment = GetFragmentID(actionName.c_str());
	PlayAction( fragment, 0, loop, flags, speedOverride );
	m_animDuration = GetCurrentAnimationTime(eIGS_Owner) * 0.001f;  // divide by 1000
}


void CPickAndThrowWeapon::PowerThrowPrepareSound(bool enable)
{
	if (CPlayer* pOwnerPlayer = GetOwnerPlayer())
	{
		pOwnerPlayer->PlaySound(CPlayer::ESound_ChargeMelee, enable);
	}
}

bool CPickAndThrowWeapon::ShouldDoPostSerializeReset() const
{
	return false;
}

EInteractionType CPickAndThrowWeapon::GetHeldEntityInteractionType()
{
	return m_heldEntityInteractionType;
}

bool CPickAndThrowWeapon::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if(aspect == ASPECT_STREAM)
	{
		NET_PROFILE_SCOPE("P&T WeaponStream", ser.IsReading());

		int previousThrowCount = m_throwCounter;
		ser.Value("throwCount", m_throwCounter, 'ui2');

		EState prevThrowState = m_RequestedThrowState;

		int tempcurrStateHolder = m_RequestedThrowState; 
		ser.Value("requestedThrowState", tempcurrStateHolder, 'ui4');
		m_RequestedThrowState = (EState)tempcurrStateHolder; 

		bool throwStateChanged = prevThrowState != m_RequestedThrowState;

		CActor *pOwner = GetOwnerActor();
		if(ser.IsReading() && pOwner && !pOwner->IsClient())	// Client is now authoritive on throws so we only need to deal with remote players here
		{
			if(m_throwCounter != previousThrowCount)
			{
				if(m_RequestedThrowState == eST_CHARGED_THROW_PRE_RELEASE)
				{
					Start_Charged_Throw_PreRelease(); 
					RequireUpdate( eIUS_General );
				}
				else if(throwStateChanged && m_RequestedThrowState != eST_UNKNOWN)
				{
					Start_Throw(m_RequestedThrowState);
					RequireUpdate( eIUS_General );
				}
			}
		}
	}
	else if(aspect == ASPECT_MELEE)
	{
		NET_PROFILE_SCOPE("P&T WeaponMelee", ser.IsReading());

		// Keep track of the attack combo index
		int8 prevAttackIndex = m_attackIndex; 
		ser.Value("attackIndex", m_attackIndex,'cmbo');
		
		int previousMeleeCount = m_meleeCounter;
		ser.Value("meleeCount", m_meleeCounter, 'ui2');
		
		if(ser.IsReading())
		{
			// If this weapon is controlled by the local client, then their attackIndex is priority (they are clicking after all)
			const bool bIsClient = g_pGame->GetIGameFramework()->GetClientActorId() == GetOwnerId(); 
			if( bIsClient && m_attackIndex < prevAttackIndex)
			{
				PTLogCombo("NetSerialise - client player Atk.Idx.[%d] has priority over incoming Atk.Idx.= [%d]",prevAttackIndex , m_attackIndex);
				m_attackIndex = prevAttackIndex; 
			}
			PTLogCombo("NetSerialise [%s]  AFTER SYNC m_attack index = [%d]", ser.IsReading() ? "READING": "WRITING",m_attackIndex);

			if(m_meleeCounter != previousMeleeCount)
			{
				// Attacker performs attack locally, server performs before netserialise. Only 3rd party clients left to perform. 
				if(!bIsClient && !gEnv->bServer)
				{
					// If this is a non local player.. we want to keep things in sync as much as possible. Force them to begin the next
					// melee anim even if they are slightly behind and havent quite finished the last one
					PTLogCombo("Performing Melee [Frcd:TRUE] From Net Serialise Atk.Idx. [%d]", m_attackIndex);
					PerformMelee(eAAM_OnPress, true);
				}
			}
		}
	}

	return true;
}

NetworkAspectType CPickAndThrowWeapon::GetNetSerializeAspects()
{
	return ASPECT_STREAM | ASPECT_MELEE;
}

void CPickAndThrowWeapon::NetStartMeleeAttack(bool boostedAttack, int8 attackIndex /*= -1*/)
{
	if(!boostedAttack)
	{
		PTLogCombo("Performing Melee From [NetStartMeleeAttack] Is this ok? Atk.Idx. [%d]", m_attackIndex);
		m_attackIndex = attackIndex; 
		PerformMelee(eAAM_OnPress,true);
	}
}

void CPickAndThrowWeapon::QuickAttach()
{
	const bool isNpc = IsNPC();

	CPlayer *pOwner = GetOwnerPlayer();
	if (pOwner)
	{
		pOwner->NotifyObjectGrabbed( true, m_objectId, isNpc );
	}

	AttachObject();

	if (isNpc)
	{
		OnNpcAttached();
	}
	else
	{
		m_state = eST_IDLE;
	}
	
	RequireUpdate( eIUS_General );
}

void CPickAndThrowWeapon::OnEndMannequinGrabAction()
{
	if(IsNPC())
	{
		m_state = eST_IDLE;
	}
}

void CPickAndThrowWeapon::SetObjectId( EntityId objectId )
{
	m_objectId = objectId;
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::OnDoComplexMelee(const bool bScheduleFinishComplexMelee)
{
	// Need to tell the environmental weapon that A melee action has been started
	CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
	if(pEnvWeapon)
	{
		const SPickAndThrowParams::SGrabTypeParams& grabParams = GetGrabTypeParams(); 

		// Start melee attack. Primary attacks support an override hit direction
		Vec3 hitDir(grabParams.melee_hit_dir);

		// to make more natural, we flip any z impulse component based on whether the player is looking up / down (e.g. shield smash when looking downward should slam victim into the floor not up into the air, as when looking up). 
		if(g_pGameCVars->pl_pickAndThrow.environmentalWeaponFlippedImpulseOverrideMult > 0.0f)
		{
			const CCamera& cam = gEnv->p3DEngine->GetRenderingCamera();
			float lookZ		   = cam.GetViewdir().GetNormalized().z; 
			if(lookZ < g_pGameCVars->pl_pickAndThrow.environmentalWeaponFlipImpulseThreshold)
			{
				hitDir.z *= lookZ * g_pGameCVars->pl_pickAndThrow.environmentalWeaponFlippedImpulseOverrideMult;
			}
		}

		pEnvWeapon->OnStartMelee( hitDir, grabParams.melee_hit_dir_min_ratio, grabParams.melee_hit_dir_max_ratio, (grabParams.m_propertyFlags & EPF_useSweepTestMelee) > 0); 
	}

	// Schedule calling of OnComplexMeleeFinished at end of anim if requested
	if(bScheduleFinishComplexMelee)
	{
		GetScheduler()->TimerAction((uint32)(m_animDuration * 1000.0f), CSchedulerAction<FinishComplexMelee>::Create(FinishComplexMelee(*this)), false);
	}

	CPlayer *pOwner = GetOwnerPlayer();
	if (pOwner)
	{
		// Also we may want to lerp to our nearest melee target
		if(pOwner->IsClient())
		{
			m_attackTime = 0.f;
			UpdateComplexMeleeLerp(pOwner); 
		}
	}
	
	// Not processed any impacts
	m_timePassed = 0.0f; 
	
	m_logicFlags &= ~(ELogicFlag_MeleeActionFinished | ELogicFlag_ProcessEndComplexMeleeState);
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::OnDoChargedThrow()
{
	if(m_objectId)
	{
		CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
		if(pEnvWeapon)
		{
			pEnvWeapon->OnStartChargedThrow(); 
		}
	}
}

//---------------------------------------------------------------------------
void CPickAndThrowWeapon::OnComplexMeleeFinished()
{
	// Cache these
	const SPickAndThrowParams::SGrabTypeParams& grabParams = GetGrabTypeParams(); 

	// Need to tell the environmental weapon that a melee action is now finished
	CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
	if(pEnvWeapon)
	{
		pEnvWeapon->OnFinishMelee(); 
	}

	// TEST:Switch back to regular rendering mode
	AttachmentUtils::SyncRenderInNearestModeWithEntity( GetOwner(), GetEntityObject() );

	// Done transition back to idle state
	if (!IsNPC())
	{
		m_state = eST_IDLE;
	}
	m_currentMeleeAutoTargetId = 0;

	// Combo counter resets if finished attack and no other attack queued
	if((m_logicFlags & ELogicFlag_RequestedCombo) == 0)
	{
		// Time for a cooldown
		if(m_attackIndex > 0)
		{
			m_postMeleeActionCooldown = grabParams.post_melee_action_cooldown_combo;
		}
		else
		{
			m_postMeleeActionCooldown = grabParams.post_melee_action_cooldown_primary;
		}

		m_attackIndex = -1; 
	}

	SetBusy(false);
}


//---------------------------------------------------------------------------
void CPickAndThrowWeapon::OnObjectPhysicsPropertiesChanged()
{
	//---------------------------------------------------------------------------
	// If an object changes geometry whilst changed (e.g. to a different damage model)
	// We need to reset its physics properties (and potentially tell the environmental weapon
	// if it is mid attack)
	
	// Lets refresh if we're in a state where we're attached to the player
	if(m_objectId)
	{
		// Perform a full refresh (and setup new attachment bindings)
		// Pass true through as we don't want to perform kill cam changes + intersection assistance logic (as we arent really dropping it). 
		DetachObject(true); 
		AttachObject(true);

		// Also tell any env weapon about this 
		CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
		if(pEnvWeapon)
		{
			return pEnvWeapon->OnObjectPhysicsPropertiesChanged(); 
		}
	}
}


bool CPickAndThrowWeapon::OnAnimationEvent( const AnimEventInstance &event )
{
	// env weapon may want to know about these
	CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
	if(pEnvWeapon)
	{
		pEnvWeapon->OnAnimationEvent(event); 
	}
	// Not consumed
	return false; 
}

void CPickAndThrowWeapon::GetAngleLimits( EStance stance, float& minAngle, float& maxAngle )
{
	if(m_objectId !=0)
	{
		minAngle = -35.0f;  //Limit angle to prevent clipping, throw objects at feet, etc...
		CEnvironmentalWeapon *pEnvWeapon = PickAndThrow::GetEnviromentalWeaponForEntity( m_objectId );
		if(pEnvWeapon)
		{
			// Seperate control over environmental weapons etc
			minAngle = g_pGameCVars->pl_pickAndThrow.environmentalWeaponMinViewClamp; 
		}
	}
}

const char* CPickAndThrowWeapon::GetGrabHelperName() const
{
	const SPickAndThrowParams::SGrabTypeParams& grabParams = GetGrabTypeParams(); 

	if( const CPlayer* pPlayer = GetOwnerPlayer() )
	{
		if( pPlayer->IsThirdPerson() )
		{
			if(grabParams.helper_3p.length())
			{
				return grabParams.helper_3p.c_str();
			}
		}
	}

	return grabParams.helper.c_str();
}

void CPickAndThrowWeapon::SerializeSpawnInfo( TSerialize ser )
{
	ser.Value("netInitObjectId", m_netInitObjectId, 'eid');
}

struct PickAndThrowWeapon_SpawnInfoParams : public ISerializableInfo
{
	virtual void SerializeWith(TSerialize ser)
	{
		ser.Value("netInitObjectId", m_netInitObjectId, 'eid');
	}

	EntityId m_netInitObjectId;
};

ISerializableInfoPtr CPickAndThrowWeapon::GetSpawnInfo()
{
	PickAndThrowWeapon_SpawnInfoParams *pParams = new PickAndThrowWeapon_SpawnInfoParams();
	pParams->m_netInitObjectId = m_objectId;

	return pParams;
}

bool CPickAndThrowWeapon::ShouldPlaySelectAction() const
{
	if( m_state == eST_THROWING || m_state == eST_DROPPING )
	{
		//don't play deselect animation, we've already got rid of object
		//(ShouldPlaySelectAction also determines if we should play deselect)
		return false;
	}
	else if (IsNPC())
	{
		return false;
	}
	else
	{
		//If we are selecting the item and it is rooted then we don't want to play the select action
		bool isRooted = false;
		if( const IEntity *pObjectEntity = GetEntityObject() )
		{
			if( IScriptTable* pObjectEntityScript = pObjectEntity->GetScriptTable() )
			{
				pObjectEntityScript->GetValue( "IsRooted", isRooted );
				return !isRooted;
			}
		}
	}

	return CWeapon::ShouldPlaySelectAction();
}

void CPickAndThrowWeapon::UpdateTargetViewLock( const Vec3& toTargetVec, const SPickAndThrowParams::SGrabTypeParams &grabParams )
{
	if(CActor* pOwner  = GetOwnerActor())
	{
		Vec3 newLookDir     = toTargetVec; 
		const Quat& viewRot = pOwner->GetViewRotation(); 
		newLookDir.z       += g_pGameCVars->pl_pickAndThrow.environmentalWeaponViewLerpZOffset; 

		Vec3 nDirToTarget = newLookDir.GetNormalized();
		SmoothCD(m_attackTime, m_attackTimeSmoothRate, gEnv->pTimer->GetFrameTime(), 1.f, g_pGameCVars->pl_pickAndThrow.environmentalWeaponViewLerpSmoothTime);
		Quat newViewRotation;
		newViewRotation.SetSlerp(viewRot, Quat::CreateRotationVDir(nDirToTarget), m_attackTime);
		pOwner->SetViewRotation( newViewRotation );
	}
}

float CPickAndThrowWeapon::GetMovementModifier() const
{
	float modifier = CWeapon::GetMovementModifier();

	if(const CPlayer* pOwnerPlayer = GetOwnerPlayer())
	{
		modifier *= pOwnerPlayer->GetModifiableValues().GetValue(kPMV_HeavyWeaponMovementMultiplier);
		modifier *= GetGrabTypeParams().movementSpeedMultiplier;
	}

	return modifier;
}

void CPickAndThrowWeapon::OnHostMigration()
{
	CRY_ASSERT(GetOwnerPlayer() && GetOwnerPlayer()->IsClient());
	SHUDEventWrapper::ForceCrosshairType( this, (ECrosshairTypes)GetGrabTypeParams().crossHairType );
}


//////////////////////////////////////////////////////////////////////////
/// NETWORK
//////////////////////////////////////////////////////////////////////////

IMPLEMENT_RMI(CPickAndThrowWeapon, SvRequestThrow)
{
	PTLogThrow("SERVER - SvRequestThrow() has been called by client, for throw type %d", params.m_RequestedThrowState);
	RequestStartThrow((EState)params.m_RequestedThrowState);
	return true;
}

IMPLEMENT_RMI(CPickAndThrowWeapon, SvRequestChargedThrowPreRelease)
{
	RequestChargedThrowPreRelease();
	return true;
}

IMPLEMENT_RMI(CPickAndThrowWeapon, SvRequestChargedThrowPostRelease)
{
	PTLogThrow("SERVER - SvRequestChargedThrowPostRelease() has been called by client");
	// If the player is dead, dont let them throw. They must not have received their kill message yet. 
	const CPlayer* pPlayer = GetOwnerPlayer();
	if(pPlayer && !pPlayer->IsDead())
	{
		// Begin the throw process
		RequestStartThrow(eST_CHARGED_THROW_POST_RELEASE);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
/// DEBUG
//////////////////////////////////////////////////////////////////////////

#ifdef PICKANDTHROWWEAPON_DEBUGINFO

void CPickAndThrowWeapon::DebugDraw()
{
	if (!GetOwnerPlayer())
		return;

	float white[] = {1.0f,1.0f,1.0f,1.0f};
	const int HIST_SIZE = 15;
	float yHIST = 20.f;

	// simple history debug code
	{
		static struct SStateNames
		{
			EState state;
			const char* pName;
		} stateNames[]=
		{
			{ eST_UNKNOWN,						"eST_UNKNOWN" },
			{ eST_GOINGTOPICK,				"eST_GOINGTOPICK" },	
			{ eST_PICKING,						"eST_PICKING" },
			{ eST_IDLE,								"eST_IDLE" },
			{ eST_PUSHINGAWAY,				"eST_PUSHINGAWAY" },
			{ eST_PUSHINGAWAY_POWER,	"eST_PUSHINGAWAY_POWER" },
			{ eST_PUSHINGAWAY_WEAK,	  "eST_PUSHINGAWAY_WEAK" },
			{ eST_CHARGED_THROW_PRE_RELEASE,	"eST_CHARGED_THROW_PRE_RELEASE" },
			{ eST_CHARGED_THROW_POST_RELEASE, "eST_CHARGED_THROW_POST_RELEASE" },
			{ eST_THROWING,						"eST_THROWING" },
			{ eST_STARTDROPPING,			"eST_STARTDROPPING" },
			{ eST_DROPPING,						"eST_DROPPING" },
			{ eST_MELEE,							"eST_MELEE" },
			{ eST_COMPLEX_MELEE,			"eST_COMPLEX_MELEE" },
			{ eST_KILLING_GRABBEDNPC,	"eST_KILLING_GRABBEDNPC" },
			{ eST_KILLED_GRABBEDNPC,	"eST_KILLED_GRABBEDNPC" },
		};

		const char* pStateName = "---";
		if (m_state<eST_LAST)
		{
			assert( m_state==stateNames[m_state].state );
			assert(m_state > 0 && m_state < eST_LAST);
			PREFAST_SUPPRESS_WARNING(6385);
			pStateName = stateNames[m_state].pName;
		}

		static EState s_lastState = eST_UNKNOWN;
		static int s_stateListUsed = 0;
		struct SHistory
		{
			const char* pStateName;
			float time;
		};
		static SHistory s_statesHist[HIST_SIZE];
		if (s_stateListUsed>0)
			s_statesHist[s_stateListUsed-1].time += gEnv->pTimer->GetFrameTime();

		if (s_lastState!=m_state)
		{
			if (s_lastState==eST_UNKNOWN)  // this check will not work properly 100%. but is good enough
			{
				s_stateListUsed = 0;
			}
			if (s_stateListUsed<HIST_SIZE)
			{
				s_statesHist[s_stateListUsed].pStateName = pStateName;
				s_statesHist[s_stateListUsed].time = 0;
				s_stateListUsed++;
			}
		}

		for (int i=0; i<s_stateListUsed; ++i)
		{
			IRenderAuxText::Draw2dLabel(20, yHIST+20.f*i, 2.0f, white, false, "%s %3.2f", s_statesHist[i].pStateName, s_statesHist[i].time );
		}	
		s_lastState = m_state;
	}			



	if (IsNPC() && GetNPCGrabbedActor())
	{
		ICharacterInstance* pPlayerCharacter = GetOwner()->GetCharacter(0);
		ICharacterInstance* pNPCCharacter = GetNPCGrabbedActor()->GetEntity()->GetCharacter(0);

		if (pPlayerCharacter && pNPCCharacter)
		{
			const CAnimation &playerAnim = pPlayerCharacter->GetISkeletonAnim()->GetAnimFromFIFO(ITEM_OWNER_ACTION_LAYER, 0);
			const CAnimation &NPCAnim = pNPCCharacter->GetISkeletonAnim()->GetAnimFromFIFO(0, 0);

			float timepopo = pPlayerCharacter->GetISkeletonAnim()->GetLayerNormalizedTime( ITEM_OWNER_ACTION_LAYER );
			const char* pPlayerAnimName = pPlayerCharacter->GetIAnimationSet()->GetNameByAnimID( playerAnim.GetAnimationId() );
			const char* pNPCAnimName = pNPCCharacter->GetIAnimationSet()->GetNameByAnimID( NPCAnim.GetAnimationId() );

			const float ANIMFPS = 30.f;
			int playerFrame = pos_round( (playerAnim.GetCurrentSegmentNormalizedTime() * playerAnim.GetCurrentSegmentExpectedDurationSeconds()) / (1/ANIMFPS) );
			int NPCFrame = pos_round( (NPCAnim.GetCurrentSegmentNormalizedTime() * NPCAnim.GetCurrentSegmentExpectedDurationSeconds()) / (1/ANIMFPS) );

			float y = yHIST + 20*HIST_SIZE + 20;

			IRenderAuxText::Draw2dLabel(20, y, 2.0f, white, false, "Ply dur: %.2f %d  %.2f  '%s' ", playerAnim.GetCurrentSegmentNormalizedTime(), playerFrame, playerAnim.GetCurrentSegmentExpectedDurationSeconds(), pPlayerAnimName );
			IRenderAuxText::Draw2dLabel(20, y+20, 2.0f, white, false, "npc dur: %.2f %d  %.2f  '%s'", NPCAnim.GetCurrentSegmentNormalizedTime(), NPCFrame, NPCAnim.GetCurrentSegmentExpectedDurationSeconds(), pNPCAnimName );
			IRenderAuxText::Draw2dLabel(20, y+40, 2.0f, white, false, "dif %.2f  framedif: %d DurDif: %.3f", 
				playerAnim.GetCurrentSegmentNormalizedTime() - NPCAnim.GetCurrentSegmentNormalizedTime(), playerFrame - NPCFrame, playerAnim.GetCurrentSegmentExpectedDurationSeconds() - NPCAnim.GetCurrentSegmentExpectedDurationSeconds());
		}
	}


	if (m_flyingActorInfo.valid)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_flyingActorInfo.entityId );
		IRenderAuxText::Draw2dLabel(600, 40.f, 2.0f, white, false, "falling actor: %d %s   time: %f", m_flyingActorInfo.entityId, pEntity?pEntity->GetName() : "<NULL>", m_flyingActorInfo.timeToEndTracking);
	}



	if (!m_infiniteViewEntities.empty())
	{
		float X = 600;
		float green[] = {0.0f,1.0f,0.0f,1.0f};
		IRenderAuxText::Draw2dLabel(X, 20, 2.0f, white, false, "infiniteViewEntities");
		for (uint32 i=0; i<m_infiniteViewEntities.size(); i++)
		{
			const SInfiniteViewDistEntity& info = m_infiniteViewEntities[i];
			IRenderAuxText::Draw2dLabel(X, 60.f+i*20.f, 2.0f, info.valid?green:white, false, "entityId: %d  timeLimit: %f  timeStopped: %f  oldViewDistRatio: %d", info.entityId, info.timeLimit - gEnv->pTimer->GetCurrTime(), info.timeStopped, info.oldViewDistRatio );
		}
	}


	EntityId objectId = GetOwnerPlayer()->GetCurrentInteractionInfo().interactiveEntityId;
	IEntity* pObject = m_pEntitySystem->GetEntity( objectId );
	if (pObject)
	{
		const float boxSize = 0.1f;
		const float axisSize = 1.f;
		ColorB colorBox( 255, 0, 0 );
		const ColorB red(255,0,0);
		const ColorB blue(0,0,255);
		const ColorB green(0,255,0);
		Matrix34 mat = pObject->GetWorldTM();
		const char* pHelperName = "No Helper";
		float textColor[] = {1.0f,0.0f,0.0f,1.0f};

		// hits to drop npc		
		if (IsNPC())
		{
			int hitsToDropNPC = GetGrabTypeParams().shieldingHits - m_hitsTakenByNPC;
			float colWhite[] = {1.0f,1.0f,1.0f,1.0f};
			IRenderAuxText::Draw2dLabel(20, 100, 2.0f, colWhite, false, "Hits to dropNPC: %d", hitsToDropNPC);
		}


		// 3d draw of helper
		HelperInfo helperInfo = GlobalFindHelperObject( objectId );
		if (helperInfo.Valid())
		{
			mat = pObject->GetWorldTM() * helperInfo.m_pSubObject->tm;
			colorBox = ColorB(0,255,255);
			pHelperName = helperInfo.m_pSubObject->name.c_str();
			textColor[0] = 0; textColor[1] = 1.f; textColor[2] = 1.f;
		}

		Vec3 wposCenter = mat.GetColumn3();
		Vec3 wposX = mat * Vec3(axisSize,0,0);
		Vec3 wposY = mat * Vec3(0,axisSize,0);
		Vec3 wposZ = mat * Vec3(0,0,axisSize);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( wposCenter, red, wposX, red );
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( wposCenter, green, wposY, green );
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( wposCenter, blue, wposZ, blue );

		AABB box( Vec3(-boxSize, -boxSize, -boxSize), Vec3( boxSize, boxSize, boxSize ) );
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawAABB( box, mat, false, colorBox, eBBD_Faceted );

		IRenderAuxText::DrawLabelEx( wposCenter, 2, textColor, true, true, pHelperName );
	}
}

#endif //PICKANDTHROWWEAPON_DEBUGINFO
