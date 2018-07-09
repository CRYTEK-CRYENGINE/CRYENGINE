// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - 7:10:2004   14:48 : Created by Márcio Martins
												taken over by Filippo De Luca

*************************************************************************/
#include "StdAfx.h"
#include <CryString/StringUtils.h>
#include "Game.h"
#include "GameCVars.h"
#include "GamePhysicsSettings.h"
#include "Actor.h"
#include "ScriptBind_Actor.h"
#include <CryNetwork/ISerialize.h>
#include <CryGame/GameUtils.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryGame/IGameTokens.h>
#include <IItemSystem.h>
#include <IInteractor.h>
#include "Item.h"
#include "Weapon.h"
#include "WeaponSharedParams.h"
#include "WeaponSystem.h"
#include "Player.h"
#include "GameRules.h"
#include "Battlechatter.h"
#include <CryAction/IMaterialEffects.h>
#include "IVehicleSystem.h"
#include <CryAISystem/IAgent.h>
#include "IPlayerInput.h"
#include "Utility/StringUtils.h"
#include "Utility/DesignerWarning.h"
#include "UI/UIManager.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDEventWrapper.h"
#include <CryAnimation/IFacialAnimation.h>
#include "ScreenEffects.h"
#include "LagOMeter.h"
#include "TacticalManager.h"
#include "EquipmentLoadout.h"
#include "ActorImpulseHandler.h"
#include <CrySystem/ISystem.h>
#include <CrySystem/Profilers/IStatoscope.h>

#include <CryNetwork/INetwork.h>
#include "AI/GameAISystem.h"
#include "AI/GameAIEnv.h"
#include "Utility/CryWatch.h"

#include "SkillKill.h"
#include "RecordingSystem.h"
#include "ActorManager.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include "GameCache.h"

#include <CryGame/IGameStatistics.h>
#include <CryAISystem/IAIActor.h>

#include "BodyManagerCVars.h"

#include "BodyManager.h"
#include "EntityUtility/EntityEffectsCloak.h"
#include "FireMode.h"

#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "Network/Lobby/GameLobby.h"

#include "EntityUtility/EntityScriptCalls.h"

#include "GameRulesModules/IGameRulesPlayerSetupModule.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"

#include "ProceduralContextRagdoll.h"
#include "AnimActionBlendFromRagdoll.h"

IItemSystem *CActor::m_pItemSystem=0;
IGameFramework	*CActor::m_pGameFramework=0;
IGameplayRecorder	*CActor::m_pGameplayRecorder=0;

#if !defined(_RELEASE)
	#define DEBUG_ACTOR_STATE
#endif

#if !defined(_RELEASE)
	#define STANCE_DEBUG
#endif


SStanceInfo CActor::m_defaultStance;

#define PHYSICS_COUNTER_MAX		4

#define kNumFramesUntilDisplayNullStanceWarning   200

AUTOENUM_BUILDNAMEARRAY(s_BONE_ID_NAME, ActorBoneList);

static void DoNotDeleteThisPointer(void* pPointer) {}

const char CActor::DEFAULT_ENTITY_CLASS_NAME[] = "Default";

//------------------------------------------------------------------------
// "W" stands for "world"
void SIKLimb::SetWPos(IEntity *pOwner,const Vec3 &pos,const Vec3 &normal,float blend,float recover,int requestID)
{
  assert(!_isnan(pos.len2()));
  assert(!_isnan(normal.len2()));
  assert(pos.len()<25000.f);

	// NOTE Dez 13, 2006: <pvl> request ID's work like priorities - if
	// the new request has an ID lower than the one currently being performed,
	// nothing happens. 
	if (requestID<blendID)
		return;

	goalWPos = pos;
	goalNormal = normal;

	if (requestID!=blendID)
	{
		blendTime = blend;
		invBlendTimeMax = 1.0f / blend;
		blendID = requestID;
	}
	else if (blendTime<0.001f)
	{
		blendTime = 0.0011f;
	}

	recoverTime = recoverTimeMax = recover;
}

void SIKLimb::SetLimb(int slot,const char *limbName,int rootID,int midID,int endID,int iFlags)
{
	rootBoneID = rootID;
	endBoneID = endID;
	middleBoneID = midID;

	cry_strcpy(name, limbName);

	blendID = -1;

	flags = iFlags;

	characterSlot = slot;
}

void SIKLimb::Update(IEntity *pOwner,float frameTime)
{
	assert(pOwner);

	ICharacterInstance *pCharacter = pOwner->GetCharacter(characterSlot);
	if (!pCharacter)
		return;

	// pvl: the correction for translation is to be removed once character offsets are redone
//	lAnimPos = pCharacter->GetISkeleton()->GetAbsJointByID(endBoneID).t - pOwner->GetSlotLocalTM (characterSlot, false).GetTranslation ();

	Vec3 vRootBone = pCharacter->GetISkeletonPose()->GetAbsJointByID(0).t; // - pOwner->GetSlotLocalTM (characterSlot, false).GetTranslation ();
	vRootBone.z=0;
	lAnimPos = pCharacter->GetISkeletonPose()->GetAbsJointByID(endBoneID).t-vRootBone;// - pOwner->GetSlotLocalTM (characterSlot, false).GetTranslation ();
  
  assert(!_isnan(lAnimPos.len2()));

	bool setLimbPos(true);
	Vec3 finalPos=Vec3(ZERO);

	if (blendTime>0.001f)
	{
		Vec3 limbWPos = currentWPos;
		finalPos = goalWPos;

		blendTime = (float)__fsel(-fabsf(blendTime - 0.0011f), 0.0f, blendTime);

		finalPos -= (finalPos - limbWPos) * min(blendTime * invBlendTimeMax,1.0f);
		currentWPos = finalPos;

		blendTime -= frameTime;
	}
	else if (recoverTime>0.001f)
	{
		Vec3 limbWPos = currentWPos;
		finalPos = pOwner->GetSlotWorldTM(characterSlot) * lAnimPos;

		finalPos -= (finalPos - limbWPos) * min(recoverTime / recoverTimeMax,1.0f);
		currentWPos = finalPos;
		goalNormal.zero();

		recoverTime -= frameTime;
		
		if (recoverTime<0.001f)
			blendID = -1;
	}
	else
	{
		currentWPos = pOwner->GetSlotWorldTM(characterSlot) * lAnimPos;
		setLimbPos = false;
	}

  assert(!_isnan(finalPos.len2()));
  assert(!_isnan(goalNormal.len2()));

	if (setLimbPos)
	{
		if (flags & IKLIMB_RIGHTHAND)
			pCharacter->GetISkeletonPose()->SetHumanLimbIK(finalPos,"RgtArm01"); //SetRArmIK(finalPos);
		else if (flags & IKLIMB_LEFTHAND)
			pCharacter->GetISkeletonPose()->SetHumanLimbIK(finalPos,"LftArm01");  //SetLArmIK(finalPos);
	}
}

//--------------------
IVehicle *SLinkStats::GetLinkedVehicle()
{
	if (!linkID)
		return NULL;
	else
	{
		IVehicle *pVehicle = NULL;
		if(g_pGame && g_pGame->GetIGameFramework() && g_pGame->GetIGameFramework()->GetIVehicleSystem())
			pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(linkID);
		//if the vehicle doesnt exist and this is supposed to be a vehicle linking forget about it.
		if (!pVehicle && flags & LINKED_VEHICLE)
			UnLink();

		return pVehicle;
	}
}

void SLinkStats::Serialize(TSerialize ser)
{
	assert(ser.GetSerializationTarget() != eST_Network);

	ser.BeginGroup("PlayerLinkStats");

	//when reading, reset the structure first.
	if (ser.IsReading())
		*this = SLinkStats();

	ser.Value("linkID", linkID);
	ser.Value("flags", flags);

	ser.EndGroup();
}
//--------------------

SActorAnimationEvents CActor::s_animationEventsTable;

void SActorAnimationEvents::Init()
{
	if (!m_initialized)
	{
		m_soundId = CCrc32::ComputeLowercase("sound");
		m_plugginTriggerId = CCrc32::ComputeLowercase("PluginTrigger");
		m_footstepSignalId = CCrc32::ComputeLowercase("footstep");
		m_foleySignalId = CCrc32::ComputeLowercase("foley");
		m_groundEffectId = CCrc32::ComputeLowercase("groundEffect");

		m_swimmingStrokeId = CCrc32::ComputeLowercase("swimmingStroke");
		m_footStepImpulseId = CCrc32::ComputeLowercase("footstep_impulse");
		m_forceFeedbackId = CCrc32::ComputeLowercase("ForceFeedback");
		m_grabObjectId = CCrc32::ComputeLowercase("FlagGrab");
		m_stowId = CCrc32::ComputeLowercase("Stow");
		m_weaponLeftHandId = CCrc32::ComputeLowercase("leftHand");
		m_weaponRightHandId = CCrc32::ComputeLowercase("rightHand");

		m_deathReactionEndId = CCrc32::ComputeLowercase("DeathReactionEnd");
		m_reactionOnCollision = CCrc32::ComputeLowercase("ReactionOnCollision");
		m_forbidReactionsId = CCrc32::ComputeLowercase("ForbidReactions");
		m_ragdollStartId = CCrc32::ComputeLowercase( "RagdollStart");

		m_deathBlow = CCrc32::ComputeLowercase("DeathBlow");
		m_killId = CCrc32::ComputeLowercase("Kill");

		m_startFire = CCrc32::ComputeLowercase("StartFire");
		m_stopFire = CCrc32::ComputeLowercase("StopFire");

		m_shootGrenade = CCrc32::ComputeLowercase("ShootGrenade");

		m_meleeHitId = CCrc32::ComputeLowercase("MeleeHit");
		m_meleeStartDamagePhase = CCrc32::ComputeLowercase("MeleeStartDamagePhase");
		m_meleeEndDamagePhase = CCrc32::ComputeLowercase("MeleeEndDamagePhase");
		m_detachEnvironmentalWeapon = CCrc32::ComputeLowercase("DetachEnvironmentalWeapon");
		m_stealthMeleeDeath = CCrc32::ComputeLowercase("StealthMeleeDeath");

		m_endReboundAnim = CCrc32::ComputeLowercase("EndReboundAnim");
	}

	m_initialized = true;
}

//------------------------------------------------------------------------
CActor::CActor()
: m_pAnimatedCharacter(0)
, m_isClient(false)
, m_isPlayer(false)
, m_isMigrating(false)
, m_pMovementController(0)
, m_stance(STANCE_NULL)
, m_desiredStance(STANCE_NULL)
,	m_teamId(0)
, m_pInventory(0)
, m_cloakLayerActive(false)
, m_registeredInAutoAimMng(false)
, m_registeredAnimationDBAs(false)
, m_lastUnCloakTime(0.0f)
, m_IsImmuneToForbiddenZone(false)
, m_bAllowHitImpulses(true)
, m_spectateSwitchTime(0.f)
, m_fAwaitingServerUseResponse(0.f)
, m_DefaultBodyDamageProfileId(INVALID_BODYDAMAGEPROFILEID)
, m_OverrideBodyDamageProfileId(INVALID_BODYDAMAGEPROFILEID)
, m_bAwaitingServerUseResponse(false)
, m_shouldPlayHitReactions(true)
, m_pendingDropEntityId(0)
, m_lastNetItemId(0)
{
	m_currentPhysProfile=CActor::GetDefaultProfile(eEA_Physics);

	m_pImpulseHandler.reset(new CActorImpulseHandler(*this));
	CRY_ASSERT(m_pImpulseHandler != NULL);

	m_timeImpulseRecover = 0.0f;
	m_airResistance = 0.0f;
	m_airControl = 1.0f;
	m_inertia = 10.0f;
	m_inertiaAccel = 11.0f;
	m_netLastSelectablePickedUp = 0;
	m_enableSwitchingItems = true;
	m_enableIronSights = true;
	m_enablePickupItems = true;
	m_pLegsCollider[0]=m_pLegsCollider[1]=m_pLegsFrame = 0;
	m_iboneLeg[0]=m_iboneLeg[1] = 0;
	m_bLegActive[0]=m_bLegActive[1] = 0;

	//[AlexMcC:22.03.10] CActors aren't owned by shared_ptrs, so we use boost's "weak without shared" technique:
	// http://www.boost.org/doc/libs/1_42_0/libs/smart_ptr/sp_techniques.html#weak_without_shared
	m_pThis.reset(this, DoNotDeleteThisPointer);

	m_netPhysCounter = 0;
	memset(m_boneTrans, 0, sizeof(m_boneTrans));

#ifndef _RELEASE
	m_tryToChangeStanceCounter = 0;
#endif
}

//------------------------------------------------------------------------
CActor::~CActor()
{
	CRY_ASSERT(g_pGame);
	PREFAST_ASSUME(g_pGame);

	GetGameObject()->SetMovementController(NULL);
	SAFE_RELEASE(m_pMovementController);

	if (m_pInventory)
	{
		if (IItem* item = GetCurrentItem())
		{
			if (item->IsUsed())
				item->StopUse(GetEntityId());
		}

		// Notify all items that our action controller is being destroyed with the animated character extension below
		for (int i = 0, n = m_pInventory->GetCount(); i < n; i++)
		{
			if (IItem* pItem = m_pItemSystem->GetItem(m_pInventory->GetItem(i)))
			{
				pItem->SetCurrentActionController(nullptr);
			}
		}

		if (gEnv->bServer)
			m_pInventory->Destroy();

		GetGameObject()->ReleaseExtension("Inventory");
	}
	
	if (!m_sLipSyncExtensionType.empty())
	{
		GetGameObject()->ReleaseExtension(m_sLipSyncExtensionType.c_str());
	}

	if (m_pAnimatedCharacter)
	{
		GetGameObject()->ReleaseExtension("AnimatedCharacter");
		GetGameObject()->DeactivateExtension("AnimatedCharacter");
	}
	GetGameObject()->ReleaseView( this );
	GetGameObject()->ReleaseProfileManager( this );

	if(g_pGame && g_pGame->GetIGameFramework() && g_pGame->GetIGameFramework()->GetIActorSystem())
		g_pGame->GetIGameFramework()->GetIActorSystem()->RemoveActor( GetEntityId() );

	ICVar* pEnableAI = gEnv->pConsole->GetCVar("sv_AISystem");
	if(!gEnv->bMultiplayer || (pEnableAI && pEnableAI->GetIVal()))
	{
		g_pGame->GetGameAISystem()->LeaveAllModules(GetEntityId());
	}

	UnRegisterInAutoAimManager();
	UnRegisterDBAGroups();
	ReleaseLegsColliders();
	CActorManager::GetActorManager()->ActorRemoved(this);

	if (IAIObject *pAI = GetEntity()->GetAI())
	{
		if (pAI->GetProxy())
		{
			pAI->GetProxy()->OnActorRemoved();
		}
	}
}

//------------------------------------------------------------------------
void CActor::PhysicalizeBodyDamage()
{
	CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
	CRY_ASSERT(pBodyDamageManager);

	const TBodyDamageProfileId bodyDamageProfileId = GetCurrentBodyDamageProfileId();
	if ((bodyDamageProfileId != INVALID_BODYDAMAGEPROFILEID) && GetEntity()->GetCharacter(0))
	{
		pBodyDamageManager->PhysicalizePlayer(bodyDamageProfileId, *GetEntity());
	}
}

//------------------------------------------------------------------------
bool CActor::Init( IGameObject * pGameObject )
{
	SetGameObject(pGameObject);

	if (!GetGameObject()->CaptureProfileManager(this))
		return false;

	g_pGame->GetIGameFramework()->GetIActorSystem()->AddActor(GetEntityId(), this);

	IEntity *pEntity = GetEntity();
	IEntityClass *pEntityClass = pEntity->GetClass();

	m_isPlayer = (pEntityClass == gEnv->pEntitySystem->GetClassRegistry()->FindClass("Player"));

	g_pGame->GetGameCache().CacheActorClass(pEntityClass, pEntity->GetScriptTable());

	m_pMovementController = CreateMovementController();
	GetGameObject()->SetMovementController(m_pMovementController);

	g_pGame->GetActorScriptBind()->AttachTo(this);
	m_pAnimatedCharacter = static_cast<IAnimatedCharacter*>(pGameObject->AcquireExtension("AnimatedCharacter"));
	if (m_pAnimatedCharacter)
	{
		ResetAnimationState();
	}

	m_pInventory = static_cast<IInventory*>(pGameObject->AcquireExtension("Inventory"));

	if (!m_pGameFramework)
	{
		m_pGameFramework = g_pGame->GetIGameFramework();
		m_pGameplayRecorder = g_pGame->GetIGameFramework()->GetIGameplayRecorder();
		m_pItemSystem = m_pGameFramework->GetIItemSystem();
	}

	if (!GetGameObject()->BindToNetwork())
		return false;

	uint32 uNewFlags = (ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO);
	if(GetChannelId() == 0 && pEntityClass != gEnv->pEntitySystem->GetClassRegistry()->FindClass("DummyPlayer"))
	{
		uNewFlags |= ENTITY_FLAG_TRIGGER_AREAS;
	}

	pEntity->SetFlags(pEntity->GetFlags() | uNewFlags);

	m_damageEffectController.Init(this);

	g_pGame->GetTacticalManager()->AddEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Unit);

	s_animationEventsTable.Init();
	m_telemetry.SetOwner(this);

	// Need to setup multiplayer lua script before reviving the player
	// Note: This is done before we cache any data, so the Lua cache will contain the properties we override here
	if (gEnv->bMultiplayer)
	{
		IScriptTable *pEntityScript = pEntity->GetScriptTable();
		if (pEntityScript)
		{
			HSCRIPTFUNCTION setIsMultiplayerFunc(NULL);
			if (pEntityScript->GetValue("SetIsMultiplayer", setIsMultiplayerFunc))
			{
				Script::Call(gEnv->pScriptSystem, setIsMultiplayerFunc, pEntityScript);
				gEnv->pScriptSystem->ReleaseFunc(setIsMultiplayerFunc);
			}
		}
	}

	g_pGame->GetGameCache().CacheActorInstance(pEntity->GetId(), pEntity->GetScriptTable());

	PrepareLuaCache();

	GenerateBlendRagdollTags();

	return true;
}

void CActor::Release()
{
	delete this;
}

//----------------------------------------------------------------------
void CActor::PostInit( IGameObject * pGameObject )
{
	GetGameObject()->EnablePrePhysicsUpdate( gEnv->bMultiplayer ? ePPU_Always : ePPU_WhenAIActivated );

	pGameObject->EnableUpdateSlot(this, 0);
	pGameObject->EnablePostUpdates( this );

	if (m_teamId)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		pGameRules->ClDoSetTeam(m_teamId, GetEntityId());
	}
}

//------------------------------------------------------------------------
bool CActor::ReloadExtension( IGameObject *pGameObject, const SEntitySpawnParams &params )
{
	CRY_ASSERT(GetGameObject() == pGameObject);

	ResetGameObject();

	if (!GetGameObject()->CaptureView(this))
		return false;
	if (!GetGameObject()->CaptureProfileManager(this))
		return false;

	if (!GetGameObject()->BindToNetwork())
		return false;

	//--- If we are in a vehicle then we should exit it as we've been torn down
	IVehicle *pVehicle = GetLinkedVehicle();
	if (pVehicle)
	{
		IVehicleSeat *pVehicleSeat = pVehicle->GetSeatForPassenger(params.prevId);
		if (pVehicleSeat)
		{
			pVehicleSeat->Exit(false, true);
		}
	}
	CRY_ASSERT(GetLinkedVehicle() == NULL);

	g_pGame->GetIGameFramework()->GetIActorSystem()->RemoveActor(params.prevId);
	g_pGame->GetIGameFramework()->GetIActorSystem()->AddActor(GetEntityId(), this);

	g_pGame->GetTacticalManager()->RemoveEntity(params.prevId, CTacticalManager::eTacticalEntity_Unit);
	g_pGame->GetTacticalManager()->AddEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Unit);

	ICVar* pEnableAI = gEnv->pConsole->GetCVar("sv_AISystem");
	if(!gEnv->bMultiplayer || (pEnableAI && pEnableAI->GetIVal()))
	{
		g_pGame->GetGameAISystem()->LeaveAllModules(params.prevId);
	}

	InvalidateCurrentModelName();
	SetAspectProfile(eEA_Physics, eAP_NotPhysicalized);

	PrepareLuaCache();

	return true;
}

//----------------------------------------------------------------------
void CActor::PostReloadExtension( IGameObject *pGameObject, const SEntitySpawnParams &params )
{
	CRY_ASSERT(GetGameObject() == pGameObject);

	pGameObject->SetMovementController(m_pMovementController);
	m_pMovementController->Reset();

	g_pGame->GetActorScriptBind()->AttachTo(this);

	ResetAnimationState();

	GetGameObject()->EnablePrePhysicsUpdate( gEnv->bMultiplayer ? ePPU_Always : ePPU_WhenAIActivated );

	GetEntity()->SetFlags(GetEntity()->GetFlags() |
		(ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO | ENTITY_FLAG_TRIGGER_AREAS));

	if (m_registeredInAutoAimMng)
	{
		g_pGame->GetAutoAimManager().UnregisterAutoaimTarget(params.prevId);
		m_registeredInAutoAimMng = false;
	}
	if (m_registeredAnimationDBAs)
	{
		g_pGame->GetGameCache().RemoveDBAUser(params.prevId);
		m_registeredAnimationDBAs = false;
	}

	RegisterInAutoAimManager();
	RegisterDBAGroups();
}

//----------------------------------------------------------------------
void CActor::PrepareLuaCache()
{
	const CGameCache &gameCache = g_pGame->GetGameCache();
	IEntityClass *pClass = GetEntity()->GetClass();

	m_LuaCache_PhysicsParams.reset(gameCache.GetActorPhysicsParams(pClass));
	m_LuaCache_GameParams.reset(gameCache.GetActorGameParams(pClass));
	m_LuaCache_Properties.reset(gameCache.GetActorProperties(GetEntityId()));
}

//----------------------------------------------------------------------
void CActor::HideAllAttachments(bool isHiding)
{

	if (IItem *pCurrentItem = GetCurrentItem())
	{
		CItem *pItem = static_cast<CItem*>(pCurrentItem);
		const bool doHide = !pItem->IsMounted();
		if(doHide)
			pCurrentItem->GetEntity()->Hide(isHiding);
	}

	//This is only for AI, in MP players don't have back attachments
	if (!IsPlayer())
	{
		int totalItems = m_pInventory->GetCount();
		for (int i=0; i<totalItems; i++)
		{
			EntityId entId = m_pInventory->GetItem(i);
			CItem *item = (CItem *)m_pItemSystem->GetItem(entId);
			if (item && item->IsAttachedToBack())
			{
				item->Hide(isHiding);
			}
		}
	}
}

//------------------------------------------------------------------------
void CActor::InitClient(int channelId)
{
	if (m_health.IsDead() && !GetSpectatorMode())
		GetGameObject()->InvokeRMI(ClSimpleKill(), NoParams(), eRMI_ToClientChannel, channelId);
}

//------------------------------------------------------------------------
void CActor::Revive( EReasonForRevive reasonForRevive )
{
	if (reasonForRevive == kRFR_FromInit)
		g_pGame->GetGameRules()->OnRevive(this);

	if(gEnv->bServer)
		m_damageEffectController.OnRevive();

	for (uint32 i=0; i<BONE_ID_NUM; i++)
	{
		m_boneTrans[i].SetIdentity();
		m_boneIDs[i] = -1;
	}

	CancelScheduledSwitch();

	if(IsClient())
	{
		// Stop force feedback
		IForceFeedbackSystem* pForceFeedbackSystem = gEnv->pGameFramework->GetIForceFeedbackSystem();
		if(pForceFeedbackSystem)
		{
			pForceFeedbackSystem->StopAllEffects();
		}
	}
	else
	{
		g_pGame->GetBodyDamageManager()->ResetInstance(*GetEntity(), m_bodyDestructionInstance);
	}

	//Reset animated character before setting the model (could break shadow char setup)
	if (m_pAnimatedCharacter)
		m_pAnimatedCharacter->ResetState();

	bool hasChangedModel = SetActorModel(); // set the model before physicalizing

	if(!gEnv->bMultiplayer || hasChangedModel)
	{
		ReadDataFromXML();
	}

	m_stance = STANCE_NULL;
	m_desiredStance = STANCE_NULL;

	if(reasonForRevive != kRFR_StartSpectating)
	{
		if(gEnv->bMultiplayer)
		{
			Physicalize();
		}

		if (gEnv->bServer)
		{
			GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
		}
	}

	// Set the actor game parameters after model is loaded and physicalized
	InitGameParams();

  if (IPhysicalEntity* pPhysics = GetEntity()->GetPhysics())
  {
    pe_action_move actionMove;    
    actionMove.dir.zero();
    actionMove.iJump = 1;

		pe_action_set_velocity actionVel;
		actionVel.v.zero();
		actionVel.w.zero();
    
    pPhysics->Action(&actionMove);
		pPhysics->Action(&actionVel);
  }

	if (m_pMovementController)
		m_pMovementController->Reset();

	m_linkStats = SLinkStats();

	if (ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0))
		pCharacter->EnableProceduralFacialAnimation(GetMaxHealth() > 0);

	if(!IsPlayer())
	{
		const char* const szTeamName = g_pGameCVars->sv_aiTeamName->GetString();
		if(szTeamName && (*szTeamName) != 0)
		{
			IGameRulesSystem *pIGameRulesSystem = g_pGame->GetIGameFramework()->GetIGameRulesSystem();
			CRY_ASSERT(pIGameRulesSystem);

			if (CGameRules *pGameRules=static_cast<CGameRules*>(pIGameRulesSystem->GetCurrentGameRules()))
			{
				int teamId = pGameRules->GetTeamId(szTeamName);
				if(!teamId)
				{
					teamId = pGameRules->CreateTeam(szTeamName);
					CRY_ASSERT(teamId);
				}

				// Team assignment is propagated from server to client,
				// so we should only set a unit's team on the server
				if(gEnv->bServer)
					pGameRules->SetTeam(teamId, GetEntityId());
			}
		}
	}

	//Only from scripts, when AI has already set its properties
	const bool registerForAutoAimDuringRevival = (reasonForRevive == CActor::kRFR_ScriptBind) || IsPlayer();
	if (registerForAutoAimDuringRevival)
	{
		RegisterInAutoAimManager();
		RegisterDBAGroups();
	}

	if(reasonForRevive == kRFR_StartSpectating)
	{
		if (ICharacterInstance *pCharacter=GetEntity()->GetCharacter(0))
				pCharacter->GetISkeletonPose()->DestroyCharacterPhysics(1);
		m_pAnimatedCharacter->ForceRefreshPhysicalColliderMode();
		m_pAnimatedCharacter->RequestPhysicalColliderMode( eColliderMode_Spectator, eColliderModeLayer_Game, "Actor::Revive");
	}

	if (reasonForRevive == kRFR_FromInit)
	{
		CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
		CRY_ASSERT(pBodyDamageManager);
		m_DefaultBodyDamageProfileId = pBodyDamageManager->GetBodyDamage(*GetEntity());

		pBodyDamageManager->GetBodyDestructibility(*GetEntity(), m_bodyDestructionInstance);
	}

	UpdateAutoDisablePhys(false);

	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	IFacialInstance* pFacialInstance = (pCharacter) ? pCharacter->GetFacialInstance() : 0;
	if (pFacialInstance)
	{
		pFacialInstance->StopAllSequencesAndChannels();
	}

	if(reasonForRevive == kRFR_Spawn)
	{
		// ensure the AI part gets enabled in case this is a re-spawn
		// (this is to make sure that Players on a dedicated server expose themselves to the AISystem again once they respawn)
		if(IAIObject *pAI = GetEntity()->GetAI())
		{
			pAI->Event(AIEVENT_ENABLE, NULL);
		}
	}
}

void CActor::UpdateAutoDisablePhys(bool bRagdoll)
{
	EAutoDisablePhysicsMode adpm = eADPM_Never;
	if (bRagdoll)
		adpm = eADPM_Never;
	else if (gEnv->bMultiplayer)
		adpm = eADPM_Never;
	else if (IsClient())
		adpm = eADPM_Never;
	else
		adpm = eADPM_WhenAIDeactivated;

	GetGameObject()->SetAutoDisablePhysicsMode(adpm);
}

//------------------------------------------------------------------------
bool CActor::LoadPhysicsParams(SmartScriptTable pEntityTable, const char* szEntityClassName, SEntityPhysicalizeParams &outPhysicsParams, 
							   pe_player_dimensions &outPlayerDim, pe_player_dynamics &outPlayerDyn)
{
	assert((bool)pEntityTable);

	bool bResult = false;

	if (pEntityTable)
	{
		SmartScriptTable physicsParams;
		if (pEntityTable->GetValue("physicsParams", physicsParams))
		{
			CScriptSetGetChain physicsTableChain(physicsParams);

			outPhysicsParams.nSlot = 0;
			outPhysicsParams.type = PE_LIVING;

			//Separate player's mass from AI, for testing
			string massParamName = string("mass") + "_" + string(szEntityClassName);
			if (!physicsTableChain.GetValue(massParamName.c_str(), outPhysicsParams.mass))
				physicsTableChain.GetValue("mass", outPhysicsParams.mass);
			physicsTableChain.GetValue("density", outPhysicsParams.density);
			physicsTableChain.GetValue("flags", outPhysicsParams.nFlagsOR);
			physicsTableChain.GetValue("partid", outPhysicsParams.nAttachToPart);
			physicsTableChain.GetValue("stiffness_scale", outPhysicsParams.fStiffnessScale);

			SmartScriptTable livingTab;
			if (physicsTableChain.GetValue("Living", livingTab))
			{
				CScriptSetGetChain livingTableChain(livingTab);

				// Player Dimensions
				livingTableChain.GetValue("height", outPlayerDim.heightCollider);
				livingTableChain.GetValue("size", outPlayerDim.sizeCollider);
				//livingTableChain.GetValue("height_eye", outPlayerDim.heightEye);
				livingTableChain.GetValue("height_pivot", outPlayerDim.heightPivot);
				livingTableChain.GetValue("use_capsule", outPlayerDim.bUseCapsule);

				// As part of a previous fix, these should be kept at 0
				//	See changelist #144179
				//	"!B 34147 -fixed sometimes players clipping through geometry while unfreezing"
				outPlayerDim.headRadius = 0.0f;
				outPlayerDim.heightEye = 0.0f;

				// Player Dynamics.
				livingTableChain.GetValue("inertia", outPlayerDyn.kInertia);
				livingTableChain.GetValue("k_air_control", outPlayerDyn.kAirControl);
				livingTableChain.GetValue("inertiaAccel", outPlayerDyn.kInertiaAccel);
				livingTableChain.GetValue("air_resistance", outPlayerDyn.kAirResistance);
				livingTableChain.GetValue("gravity", outPlayerDyn.gravity.z);

				//Separate player's mass from AI, for testing
				if (!physicsTableChain.GetValue(massParamName.c_str(), outPlayerDyn.mass))
					physicsTableChain.GetValue("mass",outPlayerDyn.mass);

				livingTableChain.GetValue("min_slide_angle", outPlayerDyn.minSlideAngle);
				livingTableChain.GetValue("max_climb_angle", outPlayerDyn.maxClimbAngle);
				livingTableChain.GetValue("max_jump_angle", outPlayerDyn.maxJumpAngle);
				livingTableChain.GetValue("min_fall_angle", outPlayerDyn.minFallAngle);
				livingTableChain.GetValue("max_vel_ground", outPlayerDyn.maxVelGround);
				livingTableChain.GetValue("timeImpulseRecover", outPlayerDyn.timeImpulseRecover);

				const char *colliderMat = 0;
				if (livingTableChain.GetValue("colliderMat", colliderMat) && colliderMat && colliderMat[0])
				{
					if (ISurfaceType *pSurfaceType=gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeByName(colliderMat))
						outPlayerDyn.surface_idx = pSurfaceType->GetId();
				}
			}

			bResult = true;
		}
	}

	return bResult;
}

//------------------------------------------------------------------------
void CActor::Physicalize(EStance stance)
{
	IEntity *pEntity = GetEntity();

	bool bHidden = pEntity->IsHidden();
	if (bHidden)
		pEntity->Hide(false);

	bool bHasPhysicsParams = true;
	SEntityPhysicalizeParams pp;

	// Physics params structs contain floats with invalid data (marked as "unused" see MARK_UNUSED macro on physinterface.h) 
	// by design that will generate a FPE when used in floating-point operations (like assignments).
	// Default copy-constructing the physic params here generates a bitwise copy of the struct, which avoid any FP-operation 
	// and FPE. 
	pe_player_dimensions playerDim;
	pe_player_dynamics playerDyn; 
	if (m_LuaCache_PhysicsParams)
	{
		memcpy(&playerDim, &m_LuaCache_PhysicsParams->playerDim, sizeof(playerDim));
		memcpy(&playerDyn, &m_LuaCache_PhysicsParams->playerDyn, sizeof(playerDyn));
		pp = m_LuaCache_PhysicsParams->params;
	}
	else
	{
		IEntityClass *pClass = pEntity->GetClass();

		// Run-time loading
		if (CGameCache::IsLuaCacheEnabled())
		{
			GameWarning("[Game Cache] Warning: Loading physics params for entity class \'%s\' at run-time!", pClass->GetName());
		}

		bHasPhysicsParams = LoadPhysicsParams(pEntity->GetScriptTable(), pClass->GetName(), pp, playerDim, playerDyn);
	}

	if (bHasPhysicsParams)
	{
		pp.pPlayerDimensions = &playerDim;
		pp.pPlayerDynamics = &playerDyn;

		// Enable the post step callback in physics.
		if( m_pAnimatedCharacter )
		{
			pp.nFlagsOR = pef_log_poststep;
		}

		// Multiply mass
		if (m_LuaCache_Properties)
		{
			pp.mass *= m_LuaCache_Properties->fPhysicMassMult;
		}

		// Player Dimensions
		if (STANCE_NULL != stance)
		{
			const SStanceInfo *sInfo = GetStanceInfo(stance);
			playerDim.heightCollider = sInfo->heightCollider;
			playerDim.sizeCollider = sInfo->size;
			playerDim.heightPivot = sInfo->heightPivot;
			playerDim.maxUnproj = max(0.0f,sInfo->heightPivot);
			playerDim.bUseCapsule = sInfo->useCapsule;
		}

		if(!is_unused(playerDyn.timeImpulseRecover))
			m_timeImpulseRecover = playerDyn.timeImpulseRecover;
		else
			m_timeImpulseRecover = 0.0f;

		if(!is_unused(playerDyn.kAirResistance))
			m_airResistance = playerDyn.kAirResistance;
		else
			m_airResistance = 0.0f;

		if(!is_unused(playerDyn.kAirControl))
			m_airControl = playerDyn.kAirControl;
		else
			m_airControl = 1.0f;

		if(!is_unused(playerDyn.kInertia))
			m_inertia = playerDyn.kInertia;
		else
			m_inertia = 10.0f; //Same value as scripts default

		if(!is_unused(playerDyn.kInertiaAccel))
			m_inertiaAccel = playerDyn.kInertiaAccel;
		else
			m_inertiaAccel = 11.0f; //Same value as scripts default

		if (pEntity->GetPhysics())
		{
			Ang3 rot(pEntity->GetWorldAngles());
			pEntity->SetRotation(Quat::CreateRotationZ(rot.z));

			SEntityPhysicalizeParams nop;
			nop.type = PE_NONE;
			pEntity->Physicalize(nop);
		}

		pEntity->Physicalize(pp);
	}
	else
	{
		GameWarning("Failed to physicalize actor \'%s\'", pEntity->GetName());
	}

	PhysicalizeLocalPlayerAdditionalParts();

#if ENABLE_GAME_CODE_COVERAGE || ENABLE_SHARED_CODE_COVERAGE
	if (!IsPlayer())
	{
		CCCPOINT(Actor_PhysicalizeNPC);
	}
	else if (gEnv->pGameFramework->GetClientActor() == NULL)
	{
		CCCPOINT(Actor_PhysicalizePlayerWhileNoClient);
	}
	else if (gEnv->pGameFramework->GetClientActor() == this)
	{
		CCCPOINT(Actor_PhysicalizeLocalPlayer);
	}
	else
	{
		CCCPOINT(Actor_PhysicalizeOtherPlayer);
	}
#endif

	//the finish physicalization
	PostPhysicalize();

	CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
	CRY_ASSERT(pBodyDamageManager);
	m_DefaultBodyDamageProfileId = pBodyDamageManager->GetBodyDamage(*GetEntity());

	PhysicalizeBodyDamage();

	pBodyDamageManager->GetBodyDestructibility(*GetEntity(), m_bodyDestructionInstance);

	if (bHidden)
		pEntity->Hide(true);
}

//------------------------------------------------------------------------
bool CActor::LoadFileModelInfo(SmartScriptTable pEntityTable, SmartScriptTable pProperties, SActorFileModelInfo &outFileModelInfo)
{
	assert((bool)pEntityTable);
	assert((bool)pProperties);

	bool bResult = false;

	if (pProperties)
	{
		CScriptSetGetChain propertiesTableChain(pProperties);

		const char* szTemp = 0;
		if (propertiesTableChain.GetValue("fileModel", szTemp))
			outFileModelInfo.sFileName = szTemp;
		if (propertiesTableChain.GetValue("clientFileModel", szTemp))
			outFileModelInfo.sClientFileName = szTemp;
		if (propertiesTableChain.GetValue("shadowFileModel", szTemp))
			outFileModelInfo.sShadowFileName = szTemp;

		propertiesTableChain.GetValue("nModelVariations", outFileModelInfo.nModelVariations);
		propertiesTableChain.GetValue("bUseFacialFrameRateLimiting", outFileModelInfo.bUseFacialFrameRateLimiting);

		// Load IK limbs
		SmartScriptTable pIKLimbs;
		if (pEntityTable && pEntityTable->GetValue("IKLimbs", pIKLimbs))
		{
			const int count = pIKLimbs->Count();
			outFileModelInfo.IKLimbInfo.clear();
			outFileModelInfo.IKLimbInfo.reserve(count);
			for (int limbIndex = 1; limbIndex <= count; ++limbIndex)
			{
				SmartScriptTable pIKLimb;
				if (pIKLimbs->GetAt(limbIndex, pIKLimb))
				{
					SActorIKLimbInfo limbInfo;

					pIKLimb->GetAt(1, limbInfo.characterSlot);
					if (pIKLimb->GetAt(2, szTemp))
						limbInfo.sLimbName = szTemp;
					if (pIKLimb->GetAt(3, szTemp))
						limbInfo.sRootBone = szTemp;
					if (pIKLimb->GetAt(4, szTemp))
						limbInfo.sMidBone = szTemp;
					if (pIKLimb->GetAt(5, szTemp))
						limbInfo.sEndBone = szTemp;
					pIKLimb->GetAt(6, limbInfo.flags);

					outFileModelInfo.IKLimbInfo.push_back(limbInfo);
				}
			}
		}

		bResult = true;
	}

	return bResult;
}

//------------------------------------------------------------------------
bool CActor::SetActorModel(const char* modelName)
{
	bool hasChanged = false;
	if (g_pGameCVars->g_setActorModelFromLua == 0)
	{
		hasChanged = SetActorModelInternal(modelName);
	}
	else
	{
		hasChanged = SetActorModelFromScript();
	}

	IAnimatedCharacter *pAnimatedCharacter = GetAnimatedCharacter();
	if (pAnimatedCharacter)
	{
		pAnimatedCharacter->UpdateCharacterPtrs();
	}

	return hasChanged;
}

//------------------------------------------------------------------------
void CActor::UpdateActorModel()
{
	// Update cache to reflect the current state of the Lua properties
	if (m_LuaCache_Properties)
	{
		IScriptTable *pEntityTable = GetEntity()->GetScriptTable();
		SmartScriptTable pProperties;
		if (pEntityTable && pEntityTable->GetValue("Properties", pProperties))
		{
			LoadFileModelInfo(pEntityTable, pProperties, m_LuaCache_Properties->fileModelInfo);
		}
	}
}

//------------------------------------------------------------------------
bool CActor::FullyUpdateActorModel()
{
	UpdateActorModel();

	const bool hasChangedModel = SetActorModel();
	if (hasChangedModel)
	{
		ReadDataFromXML();

		// Re-physicalize the actor
		Physicalize();

		CItem *pItem = static_cast<CItem*>(GetCurrentItem());
		if (pItem && pItem->IsAttachedToHand())
		{
			pItem->AttachToHand(false);		// Need to remove first otherwise the following attach will be ignored
			pItem->AttachToHand(true);
		}

		return true;
	}
	return false;
}


// Use this function, for example, when the entity slot is somehow reset.
// By invalidating the current model, it will be properly reloaded from
// a save-game.
void CActor::InvalidateCurrentModelName()
{
	m_currModel.clear();
	m_currShadowModel.clear();
}


//------------------------------------------------------------------------
bool CActor::SetActorModelFromScript()
{
	bool hasChangedModel = false;

	if (IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
	{
		HSCRIPTFUNCTION pfnSetActorModel = 0;
		if (pScriptTable->GetValue("SetActorModel", pfnSetActorModel))
		{
			Script::CallReturn(gEnv->pScriptSystem, pfnSetActorModel, pScriptTable, IsClient(), hasChangedModel);
			gEnv->pScriptSystem->ReleaseFunc(pfnSetActorModel);
		}
	}

	return hasChangedModel;
}

//------------------------------------------------------------------------
bool CActor::SetActorModelInternal(const char* modelName)
{
	bool hasChangedModel = false;

	const bool bIsClient = IsClient();

	if (m_LuaCache_Properties && !modelName)
	{
		hasChangedModel = SetActorModelInternal(m_LuaCache_Properties->fileModelInfo);
	}
	else
	{
		IEntity *pEntity = GetEntity();
		IEntityClass *pClass = pEntity->GetClass();

		// Run-time loading
		if (CGameCache::IsLuaCacheEnabled())
		{
			GameWarning("[Game Cache] Warning: Loading file model info for entity class \'%s\' at run-time!", pClass->GetName());
		}

		SActorFileModelInfo fileModelInfo;
		IScriptTable *pEntityTable = pEntity->GetScriptTable();
		SmartScriptTable pProperties;
		if (pEntityTable && pEntityTable->GetValue("Properties", pProperties) &&
			LoadFileModelInfo(pEntityTable, pProperties, fileModelInfo))
		{
			if(modelName)
			{
				fileModelInfo.sFileName = modelName;
				fileModelInfo.sClientFileName = modelName;
			}
			hasChangedModel = SetActorModelInternal(fileModelInfo);
		}
		else
		{
			GameWarning("Failed to load file model info for actor \'%s\'", pEntity->GetName());
		}
	}

	return hasChangedModel;
}

//------------------------------------------------------------------------
bool CActor::SetActorModelInternal(const SActorFileModelInfo &fileModelInfo)
{
	bool hasChangedModel = false;

	IEntity *pEntity = GetEntity();
	const bool bIsClient = IsClient();

	// Get correct file for model
	const string& sFileModel = (bIsClient ? fileModelInfo.sClientFileName : fileModelInfo.sFileName);

	CGameCache::TCachedModelName modelVariationFileName;
	CGameCache::GenerateModelVariation(sFileModel, modelVariationFileName, GetEntity()->GetScriptTable(), fileModelInfo.nModelVariations, -1);

	if (modelVariationFileName.empty())
	{
	}
	else if (strcmpi(m_currModel.c_str(), modelVariationFileName.c_str()) != 0)
	{
		hasChangedModel = true;
		m_currModel = modelVariationFileName.c_str();

		bool bShouldHide = (pEntity->GetCharacter(0) != NULL) && ((pEntity->GetSlotFlags(0) & ENTITY_SLOT_RENDER) == 0);
		pEntity->LoadCharacter(0, modelVariationFileName.c_str());
		if (bShouldHide)
		{
			const uint32 flags = (pEntity->GetSlotFlags(0) & ~ENTITY_SLOT_RENDER);
			pEntity->SetSlotFlags(0, flags);
		}

		// Set IK limbs
		SActorFileModelInfo::TIKLimbInfoVec::const_iterator itLimb = fileModelInfo.IKLimbInfo.begin();
		SActorFileModelInfo::TIKLimbInfoVec::const_iterator itLimbEnd = fileModelInfo.IKLimbInfo.end();
		for (; itLimb != itLimbEnd; ++itLimb)
		{
			const SActorIKLimbInfo &limbInfo = *itLimb;
			CreateIKLimb(limbInfo);
		}

		ICharacterInstance *pCharacter = pEntity->GetCharacter(0);
		if (pCharacter)
		{
			// Set the character to not be force updated
			ISkeletonPose *pSkeletonPose = pCharacter->GetISkeletonPose();
			if (pSkeletonPose)
			{
				pSkeletonPose->SetForceSkeletonUpdate(0);
			}

			IFacialInstance *pFacialInstance = pCharacter->GetFacialInstance();
			if (pFacialInstance)
			{
				pFacialInstance->SetUseFrameRateLimiting(fileModelInfo.bUseFacialFrameRateLimiting);
			}

			// Create the attachments
			IAttachmentManager* pAttachmentManager = pCharacter->GetIAttachmentManager();
			GetOrCreateAttachment(pAttachmentManager, "weapon_bone", "right_item_attachment");
			GetOrCreateAttachment(pAttachmentManager, "weapon_bone", "weapon");
			GetOrCreateAttachment(pAttachmentManager, "alt_weapon_bone01", "left_item_attachment");
			GetOrCreateAttachment(pAttachmentManager, "weapon_bone", "laser_attachment");

			// Let script create any extra attachments
			EntityScripts::CallScriptFunction(GetEntity(), GetEntity()->GetScriptTable(), "CreateAttachments");
		}
	}

	if (bIsClient && !fileModelInfo.sShadowFileName.empty() && 0 != m_currShadowModel.compareNoCase(fileModelInfo.sShadowFileName))
	{
		m_currShadowModel = fileModelInfo.sShadowFileName;
		pEntity->LoadCharacter(5, fileModelInfo.sShadowFileName.c_str());
	}

	return hasChangedModel;
}

//------------------------------------------------------------------------
IAttachment* CActor::GetOrCreateAttachment(IAttachmentManager *pAttachmentManager, const char *boneName, const char *attachmentName)
{
	assert(pAttachmentManager);

	IAttachment* pIAttachment = pAttachmentManager->GetInterfaceByName(attachmentName);
	if (!pIAttachment)
	{
		pIAttachment = pAttachmentManager->CreateAttachment(attachmentName, CA_BONE, boneName);
	}

	return pIAttachment;
}

//------------------------------------------------------------------------
void CActor::PostPhysicalize()
{
	//force the physical proxy to be rebuilt
	m_stance = STANCE_NULL;
	EStance stance =  m_desiredStance != STANCE_NULL ? m_desiredStance : m_params.defaultStance;
	SetStance(stance);

	UpdateStance();

	// [*DavidR | 1/Feb/2010]
	// Disable physical impulses created by explosions. We want to have control over impulses on projectiles in game code
	if (ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0))
	{
		if (IPhysicalEntity* pCharacterPhysics = pCharacter->GetISkeletonPose()->GetCharacterPhysics())
		{
			pe_params_part colltype;
			colltype.flagsColliderAND = ~geom_colltype_explosion;
			pCharacterPhysics->SetParams(&colltype);
		}
	}

	//set player lod always
//	if (IsPlayer())
	{
		IEntityRender *pIEntityRender = (GetEntity()->GetRenderInterface());

		
		{
			IRenderNode *pRenderNode = pIEntityRender->GetRenderNode();

			if (pRenderNode)
			{
				if (IsPlayer())
					pRenderNode->SetViewDistRatio(g_pGameCVars->g_actorViewDistRatio);
				pRenderNode->SetLodRatio(g_pGameCVars->g_playerLodRatio);  
			}
		}
	}

	//CryLogAlways("CActor::PostPhysicalize: %s, inertia: %f, inertiaAccel: %f", GetEntity()->GetName(), m_inertia, m_inertiaAccel);

	if (m_pAnimatedCharacter)
	{
		SAnimatedCharacterParams params = m_pAnimatedCharacter->GetParams();
		params.timeImpulseRecover = GetTimeImpulseRecover();
		m_pAnimatedCharacter->SetParams(params);

		m_pAnimatedCharacter->ResetInertiaCache();
	}
}

//------------------------------------------------------------------------
void CActor::ShutDown()
{
	m_bodyDestructionInstance.DeleteMikeAttachmentEntity();
}

//------------------------------------------------------------------------
bool CActor::IsFallen() const
{
	return GetActorStats()->isInBlendRagdoll;
}

bool CActor::IsDead() const
{
	return( m_health.IsDead() );
}

void CActor::Fall(Vec3 hitPos)
{
	HitInfo hitInfo;
	hitInfo.pos	= hitPos;
	Fall(hitInfo);
}

void CActor::Fall(const HitInfo& hitInfo)
{
	CRY_ASSERT( m_pAnimatedCharacter->GetActionController() );

	if( GetActorStats()->isInBlendRagdoll )
		return;

	GetActorStats()->isInBlendRagdoll = true;

	m_pAnimatedCharacter->GetActionController()->Queue( *new CAnimActionBlendFromRagdollSleep(PP_HitReaction, *this, hitInfo, m_blendRagdollParams.m_blendInTagState, m_blendRagdollParams.m_blendOutTagState) );
}

//------------------------------------------------------------------------
void CActor::OnFall(const HitInfo& hitInfo)
{
	//we don't want noDeath (tutorial) AIs to loose their weapon, since we don't have pickup animations yet
	bool	dropWeapon(true);
	bool  hasDamageTable = false;

	IAISystem *pAISystem=gEnv->pAISystem;
	if (pAISystem)
	{
		if(GetEntity() && GetEntity()->GetAI())
		{	SmartScriptTable props;
		SmartScriptTable propsDamage;

			IAIActor* pAIActor = CastToIAIActorSafe(GetEntity()->GetAI());
			if(pAIActor)
			{
				IAISignalExtraData *pEData = pAISystem->CreateSignalExtraData();	// no leak - this will be deleted inside SendAnonymousSignal
				pEData->point = Vec3(0,0,0);

				pAIActor->SetSignal(1,"OnFallAndPlay",0,pEData);
			}
		}
	}

	// TODO: Does this do anything anymore ?
	CreateScriptEvent("sleep", 0);

	// Stop using mounted items before ragdollizing
	CItem* pCurrentItem = static_cast<CItem*>(GetCurrentItem());
	if(pCurrentItem && pCurrentItem->IsMounted() && pCurrentItem->IsUsed())
		pCurrentItem->StopUse(GetEntityId());

	//Do we want this for the player? (Sure not for AI)
	if(IsPlayer() && dropWeapon)
	{
		//DropItem(GetCurrentItemId(), 1.0f, false);
		if (GetCurrentItemId(false))
			HolsterItem(true);
	}

	// stop shooting
	if ( EntityId currentItem = GetCurrentItemId(true) )
		if ( CWeapon* pWeapon = GetWeapon(currentItem) )
			pWeapon->StopFire();

	//add some twist
	if (!IsClient() && hitInfo.pos.len())
	{
		if(IPhysicalEntity *pPE = GetEntity()->GetPhysics())
		{
			pe_action_impulse imp;
			if( hitInfo.partId != -1 )
			{
				const float scale = 1000.f;

				imp.partid = hitInfo.partId;
				imp.iApplyTime = 0;
				imp.point = hitInfo.pos;
				imp.impulse = scale*hitInfo.dir;
			}

			// push the impulse on to the queue.
			m_pImpulseHandler->SetOnRagdollPhysicalizedImpulse(imp);
		}
	}
}

//------------------------------------------------------------------------
void CActor::KnockDown(float backwardsImpulse)
{

}

//------------------------------------------------------------------------
void CActor::SetLookAtTargetId(EntityId targetIdsp, float interpolationTime){}

//------------------------------------------------------------------------
void CActor::SetForceLookAtTargetId(EntityId targetId, float interpolationTime){}

//------------------------------------------------------------------------
void CActor::StandUp()
{
	if ( m_health.IsDead() )
	{
		GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
	}
	else
	{
		if ( GetLinkedVehicle() && GetAnimationGraphState() )
			GetAnimationGraphState()->Hurry();
		else if ( m_currentPhysProfile == eAP_Sleep )
			GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
	}
}

//------------------------------------------------------
void CActor::OnSetStance(EStance desiredStance)
{
	CRY_ASSERT_TRACE(desiredStance >= 0 && desiredStance < STANCE_LAST, ("%s '%s' desired stance %d is out of range 0-%d", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), desiredStance, STANCE_LAST - 1));

#if ENABLE_GAME_CODE_COVERAGE || ENABLE_SHARED_CODE_COVERAGE
	if (m_desiredStance != desiredStance)
	{
		switch (m_desiredStance)
		{
			case STANCE_STAND:
			CCCPOINT_IF(m_isClient,  ActorStance_LocalActorStopStand);
			CCCPOINT_IF(!m_isClient, ActorStance_OtherActorStopStand);
			break;

			case STANCE_CROUCH:
			CCCPOINT_IF(m_isClient,  ActorStance_LocalActorStopCrouch);
			CCCPOINT_IF(!m_isClient, ActorStance_OtherActorStopCrouch);
			break;

			case STANCE_SWIM:
			CCCPOINT_IF(m_isClient,  ActorStance_LocalActorStopSwim);
			CCCPOINT_IF(!m_isClient, ActorStance_OtherActorStopSwim);
			break;
		}

		switch (desiredStance)
		{
			case STANCE_STAND:
			CCCPOINT_IF(m_isClient,  ActorStance_LocalActorStartStand);
			CCCPOINT_IF(!m_isClient, ActorStance_OtherActorStartStand);
			break;

			case STANCE_CROUCH:
			CCCPOINT_IF(m_isClient,  ActorStance_LocalActorStartCrouch);
			CCCPOINT_IF(!m_isClient, ActorStance_OtherActorStartCrouch);
			break;

			case STANCE_SWIM:
			CCCPOINT_IF(m_isClient,  ActorStance_LocalActorStartSwim);
			CCCPOINT_IF(!m_isClient, ActorStance_OtherActorStartSwim);
			break;
		}
	}
#endif

	m_desiredStance = desiredStance;
}

void CActor::SetStance(EStance desiredStance)
{
	OnSetStance( desiredStance );
}

//------------------------------------------------------
void CActor::OnStanceChanged(EStance newStance, EStance oldStance)
{
	if(!gEnv->bMultiplayer)
	{
		EntityScripts::CallScriptFunction(GetEntity(), GetEntity()->GetScriptTable(), "OnStanceChanged", newStance, oldStance);
	}

	if (IsClient())
	{
		CItem* pCurrentItem = GetItem( GetCurrentItemId( false ) );
		if (pCurrentItem != NULL)
		{
			pCurrentItem->OnOwnerStanceChanged( newStance );
		}
	}
}

//------------------------------------------------------
IEntity *CActor::LinkToVehicle(EntityId vehicleId) 
{
	// did this link actually change, or are we just re-linking?
	bool changed=((m_linkStats.linkID!=vehicleId)||gEnv->pSystem->IsSerializingFile())?true:false;

	m_linkStats = SLinkStats(vehicleId,SLinkStats::LINKED_VEHICLE);
	
	IVehicle *pVehicle = m_linkStats.GetLinkedVehicle();
	IEntity *pLinked = pVehicle?pVehicle->GetEntity():NULL;

	IVehicleSeat* pSeat = pVehicle?pVehicle->GetSeatForPassenger(GetEntityId()):NULL;
	if(pSeat && pSeat->IsRemoteControlled())
	{
		return pLinked;
	}
  
	if (m_pAnimatedCharacter)
	{
		bool enabled = pLinked?true:false;
		
		if(gEnv->bServer)
		{
			if(enabled)
			{
				if (changed)
					GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Linked);
			}
			else if(IPhysicalEntity *pPhys = GetEntity()->GetPhysics())
			{
				pe_type type = pPhys->GetType();
				if(type == PE_LIVING)
					GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
				else if(type == PE_ARTICULATED)
					GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
			}
		}

		// if the player is hidden when entering a vehicle, the collider mode
		//	change will be ignored (caused problems in Convoy due to cutscene).
		// (CE-10939) MP: Disconnecting one player while both are sitting in a vehicle
		// triggers this assertion as the linking entity is both player and hidden.
		//assert(!IsPlayer() || !GetEntity()->IsHidden());

		m_pAnimatedCharacter->ForceRefreshPhysicalColliderMode();
		m_pAnimatedCharacter->RequestPhysicalColliderMode(enabled ? eColliderMode_Disabled : eColliderMode_Undefined, eColliderModeLayer_Game, "Actor::LinkToVehicle");
	}
  
  if (pLinked)  
    pLinked->AttachChild(GetEntity(), ENTITY_XFORM_USER|IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
  else
    GetEntity()->DetachThis(IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
  
	return pLinked;
}

IEntity *CActor::LinkToEntity(EntityId entityId, bool bKeepTransformOnDetach) 
{
	m_linkStats = SLinkStats(entityId,SLinkStats::LINKED_FREELOOK);

	IEntity *pLinked = m_linkStats.GetLinked();

	if (m_pAnimatedCharacter)
	{
		bool enabled = pLinked?true:false;

		m_pAnimatedCharacter->ForceRefreshPhysicalColliderMode();
		m_pAnimatedCharacter->RequestPhysicalColliderMode(enabled ? eColliderMode_Disabled : eColliderMode_Undefined, eColliderModeLayer_Game, "Actor::LinkToEntity");
	}

  if (pLinked)
    pLinked->AttachChild(GetEntity(), 0);
  else
		GetEntity()->DetachThis(bKeepTransformOnDetach ? IEntity::ATTACHMENT_KEEP_TRANSFORMATION : IEntity::EAttachmentFlags(), bKeepTransformOnDetach ? ENTITY_XFORM_USER : EEntityXFormFlags());

	return pLinked;
}

void CActor::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_HIDE:
		{
			g_pGame->GetTacticalManager()->RemoveEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Unit);
			if(!gEnv->bMultiplayer)
			{
				SHUDEvent hudevent(eHUDEvent_RemoveEntity);
				hudevent.AddData(SHUDEventData((int)GetEntityId()));
				CHUDEventDispatcher::CallEvent(hudevent);
			}

		} // no break, because the ENTITY_EVENT_INVISIBLE should be executed as well
	case ENTITY_EVENT_INVISIBLE:
		{
			HideAllAttachments(true);
		}	
		break;
	case ENTITY_EVENT_UNHIDE:
		{
			g_pGame->GetTacticalManager()->AddEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Unit);
			SHUDEvent hudevent(eHUDEvent_AddEntity);
			hudevent.AddData(SHUDEventData((int)GetEntityId()));
			CHUDEventDispatcher::CallEvent(hudevent);

		} // no break, because the ENTITY_EVENT_VISIBLE should be executed as well
	case ENTITY_EVENT_VISIBLE:
		{
			if(gEnv->bMultiplayer && IsClient())
			{
				ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
				if (pCharacter)
				{
					ISkeletonPose *pSkeletonPose = pCharacter->GetISkeletonPose();
					if (pSkeletonPose)
					{
						pSkeletonPose->SetForceSkeletonUpdate(2);
					}
				}
			}

			HideAllAttachments(false);
		}	
		break;
  case ENTITY_EVENT_RESET:
    Reset(event.nParam[0]==1);
    break;
	case ENTITY_EVENT_ANIM_EVENT:
		{
			const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
			ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);
			if (pAnimEvent && pCharacter)
			{
				AnimationEvent(pCharacter, *pAnimEvent);
			}
		}
		break;
	case ENTITY_EVENT_DONE:
		{
			// Set all the Item's Action Controllers to NULL as they will be removed in this event in the AnimatedCharacter and we can't leave hanging ptrs.
			ClearItemActionControllers();

			if (IsClient())
			{
				BecomeRemotePlayer();
			}
		}
		break;
	case ENTITY_EVENT_TIMER:
		{
			if (event.nParam[0] == ITEM_SWITCH_TIMER_ID)
			{
				SActorStats* pActorStats = GetActorStats();
				assert(pActorStats);

				if (pActorStats->exchangeItemStats.switchingToItemID != 0)
				{
					SelectItem(pActorStats->exchangeItemStats.switchingToItemID, pActorStats->exchangeItemStats.keepHistory, false);
					pActorStats->exchangeItemStats.switchingToItemID = 0;
				}
				else
				{
					HolsterItem(true);
				}
			}
			else if( event.nParam[0] == ITEM_SWITCH_THIS_FRAME )
			{
				GetActorStats()->exchangeItemStats.switchThisFrame = true;
			}
			else if ( event.nParam[0] == RECYCLE_AI_ACTOR_TIMER_ID )
			{
				AttemptToRecycleAIActor();
			}
		}
		break;
	case ENTITY_EVENT_PREPHYSICSUPDATE:
		{
			PrefetchLine(m_boneTrans, 0);	PrefetchLine(m_boneTrans, 128);	PrefetchLine(m_boneTrans, 256);	PrefetchLine(m_boneTrans, 384);
			static_assert(sizeof(m_boneTrans) > 384, "Invalid type size!");

			ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
			ISkeletonPose *pSkelPose = pCharacter ? pCharacter->GetISkeletonPose() : NULL;
			if (pSkelPose)
			{
				QuatT * __restrict pBoneTrans = m_boneTrans;
				for (uint32 i=0; i<BONE_ID_NUM; i++)
				{
					int boneID = GetBoneID(i);
					if (boneID >= 0)
					{
						pBoneTrans[i] = pSkelPose->GetAbsJointByID(boneID);
					}
				}
			}

			SActorStats* pActorStats = GetActorStats();
			if( pActorStats->exchangeItemStats.switchThisFrame )
			{
				pActorStats->exchangeItemStats.switchThisFrame = false;

				SEntityEvent eventTimer;
				eventTimer.event = ENTITY_EVENT_TIMER;
				eventTimer.nParam[0] = ITEM_SWITCH_TIMER_ID;
				ProcessEvent( eventTimer );
			}

			break;
		}
	case ENTITY_EVENT_INIT:
		{
			Revive(kRFR_FromInit);
			break;
		}
	case ENTITY_EVENT_RELOAD_SCRIPT:
		{
			CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
			assert(pBodyDamageManager);

			pBodyDamageManager->ReloadBodyDamage(*this);
			break;
		}
	case ENTITY_EVENT_ADD_TO_RADAR:
		{
			SHUDEvent hudevent(eHUDEvent_AddEntity);
			hudevent.AddData(SHUDEventData(static_cast<int>(GetEntityId())));
			CHUDEventDispatcher::CallEvent(hudevent);
			break;
		}
	case ENTITY_EVENT_REMOVE_FROM_RADAR:
		{
			SHUDEvent hudevent(eHUDEvent_RemoveEntity);
			hudevent.AddData(SHUDEventData(static_cast<int>(GetEntityId())));
			CHUDEventDispatcher::CallEvent(hudevent);
			break;
		}
  }  
}

uint64 CActor::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_HIDE)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_INVISIBLE)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_UNHIDE)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_VISIBLE)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_RESET)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_ANIM_EVENT)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_DONE)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_TIMER)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_PREPHYSICSUPDATE)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_INIT)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_RELOAD_SCRIPT)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_ADD_TO_RADAR)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_REMOVE_FROM_RADAR);
}

void CActor::BecomeRemotePlayer()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (!pGameRules || pGameRules->IsRealActor(GetEntityId()))
	{
		gEnv->pCryPak->DisableRuntimeFileAccess (false);
	}
	m_isClient = false;
}

bool CActor::BecomeAggressiveToAgent(EntityId agentID)
{
	// return false to disallow it happening
	return true;
}

bool CActor::IsRemote() const
{
	static IEntityClass* sDummyPlayerClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DummyPlayer");
	const bool isAI = !IsPlayer();
	IEntityClass* myClass = GetEntity()->GetClass();

	return gEnv->bMultiplayer && !IsClient() && !(gEnv->bServer && (myClass == sDummyPlayerClass || isAI));
}

void CActor::AddLocalHitImpulse(const SHitImpulse& hitImpulse)
{
	if(m_bAllowHitImpulses)
	{
		m_pImpulseHandler->AddLocalHitImpulse(hitImpulse);
	}
}

void CActor::Update(SEntityUpdateContext& ctx, int slot)
{
#ifdef DEBUG_ACTOR_STATE
	if(g_pGameCVars && g_pGameCVars->g_displayDbgText_actorState)
	{
		IItem* currentItem = GetCurrentItem();
		WATCH_ACTOR_STATE("(team %d, health %8.2f/%8.2f%s%s%s) is (%s) stance=%s", m_teamId, m_health.GetHealth(), GetMaxHealth(), GetEntity()->IsHidden() ? ", $6HIDDEN$o" : "", GetActorStats()->isRagDoll ? ", $5RAGDOLL$o" : "", "", currentItem ? currentItem->GetEntity()->GetClass()->GetName() : "no item", GetStanceInfo(GetStance())->name);
	}

	if (CBodyManagerCVars::IsBodyDestructionDebugEnabled())
	{
		g_pGame->GetBodyDamageManager()->DebugBodyDestructionInstance(*GetEntity(), m_bodyDestructionInstance);
	}
#endif

	m_damageEffectController.UpdateEffects(ctx.fFrameTime);

	// Death is now handled by PlayerStateDead (stephenn).
	// Only update stance for alive characters. Dead characters never request any stance changes
	// but if a different stance is requested for whatever reason (currently it happens after QL)
	// and the animation graph has different death animations for different stances (like for the
	// Hunter currently) then some other death animation may play again to better match the state.
	UpdateStance();

	if(gEnv->bServer)
	{
		m_telemetry.Update();
	}

	UpdateBodyDestruction(ctx.fFrameTime);

	UpdateServerResponseTimeOut(ctx.fFrameTime);

	UpdateLegsColliders();
}

void CActor::UpdateBodyDestruction(float frameTime)
{
	m_bodyDestructionInstance.Update(frameTime);
}

void CActor::ReadDataFromXML(bool isReloading/* = false*/)
{
	const IItemParamsNode* pEntityClassParamsNode = GetEntityClassParamsNode();
	if (!IsPoolEntity())
	{
		m_pImpulseHandler->ReadXmlData(pEntityClassParamsNode);
	}
}

void CActor::InitLocalPlayer()
{
	CryLog("%s '%s' is becoming local actor", GetEntity()->GetClass()->GetName(), GetEntity()->GetName());
	INDENT_LOG_DURING_SCOPE();

	m_isClient = true;

	SetupLocalPlayer();
}

bool CActor::UpdateStance()
{
	if (m_stance != m_desiredStance)
	{
		// If character is animated, postpone stance change until state transition is finished (i.e. in steady state).
		if ((m_pAnimatedCharacter != NULL) && m_pAnimatedCharacter->InStanceTransition())
			return false;

		if (!TrySetStance(m_desiredStance))
		{
#ifdef STANCE_DEBUG
			// Only time this should be allowed is when we're already in a valid state or we have no physics... otherwise warn user!
			// (Temporarily only warn about players, although really NPCs shouldn't be left in STANCE_NULL either.)
			if (m_stance == STANCE_NULL && GetEntity()->GetPhysics() && IsPlayer())
			{
				const Vec3 pos = GetEntity()->GetWorldPos();

				switch (m_tryToChangeStanceCounter)
				{
					case kNumFramesUntilDisplayNullStanceWarning + 1:
					break;

					case kNumFramesUntilDisplayNullStanceWarning:
					DesignerWarning(false, "%s '%s' (%g %g %g) can't change stance from %d '%s' to %d '%s' (tried and failed for %u consecutive frames). Possibly a spawn-point intersecting with level geometry?", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), pos.x, pos.y, pos.z, (int) m_stance, GetStanceName(m_stance), (int) m_desiredStance, GetStanceName(m_desiredStance), m_tryToChangeStanceCounter);
					++ m_tryToChangeStanceCounter;
					break;

					default:
					++ m_tryToChangeStanceCounter;
					CryLog("%s '%s' (%g %g %g) can't change stance from %d '%s' to %d '%s' [Attempt %u]", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), pos.x, pos.y, pos.z, (int) m_stance, GetStanceName(m_stance), (int) m_desiredStance, GetStanceName(m_desiredStance), m_tryToChangeStanceCounter);
					break;
				}
			}
#endif
			return false;
		}

		EStance newStance = m_desiredStance;
		EStance oldStance = m_stance;
		m_stance = newStance;

#ifdef STANCE_DEBUG
		if (m_tryToChangeStanceCounter)
		{
			const Vec3 pos = GetEntity()->GetWorldPos();
			CryLog("%s '%s' (%g %g %g) has finally changed stance from %d '%s' to %d '%s'", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), pos.x, pos.y, pos.z, (int) oldStance, GetStanceName(oldStance), (int) newStance, GetStanceName(m_desiredStance));
			m_tryToChangeStanceCounter = 0;
		}
#endif

		OnStanceChanged(newStance, oldStance);

		// Request new animation stance.
		// AnimatedCharacter has it's own understanding of stance, which might be in conflict.
		// Ideally the stance should be maintained only in one place. Currently the Actor (gameplay) rules.
		if (m_pAnimatedCharacter != NULL)
			m_pAnimatedCharacter->RequestStance(m_stance, GetStanceInfo(m_stance)->name);

		IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();
		if (pPhysEnt != NULL)
		{
			pe_action_awake aa;
			aa.bAwake = 1;
			pPhysEnt->Action(&aa);
		}
	}

	return true;
}

//------------------------------------------------------

bool CActor::TrySetStance(EStance stance)
{
	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();
	int result = 0;
	if (pPhysEnt)
	{
		const SStanceInfo *sInfo = GetStanceInfo(stance);
#ifdef STANCE_DEBUG
		if (sInfo == &m_defaultStance)
		{
			CryLogAlways("%s trying to set undefined stance (%d).\nPlease update the stances tables in the Lua script accordingly.", 
				GetEntity()->GetName(), stance);
		}
#endif

		// don't changes to an invalid stance.
		if( stricmp( sInfo->name, "null" ) == 0 )
		{
			return false;
		}

		pe_player_dimensions playerDim;
		playerDim.heightEye = 0.0f;
		playerDim.heightCollider = sInfo->heightCollider;
		playerDim.sizeCollider = sInfo->size;
		playerDim.heightPivot = sInfo->heightPivot;
		if( m_stance != STANCE_NULL || stance == STANCE_CROUCH )
		{
			playerDim.maxUnproj = max(0.25f, sInfo->heightPivot);
		}
		else
		{
			// If we're coming from a NULL stance, try and be a bit for aggressive at
			// unprojecting from collisions.
			// fixes some last minute s/l bugs on Crysis2.
			playerDim.maxUnproj = max( sInfo->size.x, sInfo->heightPivot );
			playerDim.maxUnproj = max( 0.35f, playerDim.maxUnproj );
			playerDim.dirUnproj = ZERO;		
		}
		playerDim.bUseCapsule = sInfo->useCapsule;
		playerDim.groundContactEps = sInfo->groundContactEps;

		result = pPhysEnt->SetParams(&playerDim);
	}

	return (result != 0);
}

//------------------------------------------------------------------------
float CActor::GetSpeedMultiplier(SActorParams::ESpeedMultiplierReason reason)
{
	float fResult = 1.0f;

	assert(reason >= 0 && reason < SActorParams::eSMR_COUNT);
	if (reason >= 0 && reason < SActorParams::eSMR_COUNT)
		fResult = m_params.speedMultiplier[reason];

	return fResult;
}

//------------------------------------------------------------------------
void CActor::SetSpeedMultipler(SActorParams::ESpeedMultiplierReason reason, float fSpeedMult)
{
	assert(reason >= 0 && reason < SActorParams::eSMR_COUNT);
	if (reason >= 0 && reason < SActorParams::eSMR_COUNT)
		m_params.speedMultiplier[reason] = fSpeedMult;
}

//------------------------------------------------------------------------
void CActor::MultSpeedMultiplier(SActorParams::ESpeedMultiplierReason reason, float fSpeedMult)
{
	assert(reason >= 0 && reason < SActorParams::eSMR_COUNT);
	if (reason >= 0 && reason < SActorParams::eSMR_COUNT)
		m_params.speedMultiplier[reason] *= fSpeedMult;
}

//------------------------------------------------------------------------
void CActor::SetStats(SmartScriptTable &rTable)
{
	SActorStats *pStats = GetActorStats();
	if (pStats)
	{
		rTable->GetValue("inFiring",pStats->inFiring);
	}
}

//------------------------------------------------------------------------
void CActor::SetCloakLayer(bool set, eFadeRules config /*= eAllowFades*/)
{
	if ( !set && m_cloakLayerActive == true )
	{
		m_lastUnCloakTime = gEnv->pTimer->GetCurrTime();
	}

	m_cloakLayerActive = set;

	// new cloak effect
	const EntityId entityId = GetEntityId();
	const bool bCloakFadeByDistance = EntityEffects::Cloak::DoesCloakFadeByDistance(entityId);
	const uint8 cloakColorChannel = EntityEffects::Cloak::GetCloakColorChannel(entityId);
	const bool bIgnoreCloakRefractionColor = EntityEffects::Cloak::IgnoreRefractionColor(entityId);
	const bool bFade = (config == eAllowFades ? true : false);

	EntityEffects::Cloak::CloakEntity(entityId, set, bFade, GetCloakBlendSpeedScale(), bCloakFadeByDistance, cloakColorChannel, bIgnoreCloakRefractionColor);

	CloakSyncAttachments(true);

	// Cloak mounted item
	// (Mounted item's are seperate entities whilst mounted, not attachments)
	CItem* pItem = static_cast<CItem *>(GetCurrentItem());
	if(pItem && pItem->IsMounted())
	{
		CloakSyncEntity(pItem->GetEntityId(),true);
	}
}

//------------------------------------------------------------------------
void CActor::CloakSyncAttachments(bool bFade)
{
	IEntity *pEntity = GetEntity();

	// take care of the attachments on the back
	if (ICharacterInstance *pOwnerCharacter = pEntity->GetCharacter(0))
	{
		if (IAttachmentManager *pAttachmentManager = pOwnerCharacter->GetIAttachmentManager())
		{
			const uint32 attachmentCount = pAttachmentManager->GetAttachmentCount();
			for (uint32 attachmentIndex = 0; attachmentIndex < attachmentCount; ++attachmentIndex)
			{
				if (IAttachment* pAttachment = pAttachmentManager->GetInterfaceByIndex(attachmentIndex))
				{
					CloakSyncAttachment(pAttachment, bFade);
				}
			}
		}
	}
}

//------------------------------------------------------------------------
void CActor::CloakSyncAttachment(IAttachment* pAttachment, bool bFade)
{
	assert(pAttachment);

	if(IAttachmentObject *pAO = pAttachment->GetIAttachmentObject())
	{
		if(pAO->GetAttachmentType()==IAttachmentObject::eAttachment_Entity)
		{
			CEntityAttachment* pEA = static_cast<CEntityAttachment*>(pAO);
			if (CItem* pItem = static_cast<CItem*>(gEnv->pGameFramework->GetIItemSystem()->GetItem(pEA->GetEntityId())))
			{
				//Ensure that the entity has the right flags set, though hidden, so that if we unhide it the flags are correct.
				//	Only do the fade if the object is visible.
				pItem->CloakSync(bFade && !pItem->GetEntity()->IsHidden());
			}
			else
			{
				CloakSyncEntity(pEA->GetEntityId(), bFade);
			}
		}
	}
}

//------------------------------------------------------------------------
void CActor::CloakSyncEntity(EntityId entityId, bool bFade)
{
	//Don't cloak held objects/NPCs
	const bool isPickAndThrowEntity = (GetGrabbedEntityId() == entityId) && !gEnv->bMultiplayer;
	
	if (!isPickAndThrowEntity)
	{
		IEntityRender *pOwnerRP = GetEntity()->GetRenderInterface();
		if (pOwnerRP)
		{
			const uint8 ownerMask = 0;//pOwnerRP->GetMaterialLayersMask();
			const bool isCloaked = (ownerMask&MTL_LAYER_CLOAK) != 0;
			//const bool bCloakFadeByDistance = pOwnerRP->DoesCloakFadeByDistance();
			//const uint8 cloakColorChannel = pOwnerRP->GetCloakColorChannel();
			//const bool bIgnoreCloakRefractionColor = pOwnerRP->DoesIgnoreCloakRefractionColor();

			//EntityEffects::Cloak::CloakEntity(entityId, isCloaked, bFade, GetCloakBlendSpeedScale(), bCloakFadeByDistance, cloakColorChannel, bIgnoreCloakRefractionColor);

			if(CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem())
			{
				pRecordingSystem->OnObjectCloakSync( entityId, GetEntityId(), isCloaked, bFade);
			}
		}
	}
}

//------------------------------------------------------------------------
const float CActor::GetCloakBlendSpeedScale()
{
	return g_pGameCVars->g_cloakBlendSpeedScale;
}

//------------------------------------------------------------------------
void CActor::OnAction(const ActionId& actionId, int activationMode, float value)
{
	if (!gEnv->IsEditing())
	{
		if(!GetActorStats()->bAttemptingStealthKill)
		{
			IItem *pItem = GetCurrentItem();
			if (pItem)
				pItem->OnAction(GetGameObject()->GetEntityId(), actionId, activationMode, value);
		}
	}
}

//------------------------------------------------------------------------
void CActor::CreateScriptEvent(const char *event,float value,const char *str)
{
	EntityScripts::CallScriptFunction(GetEntity(), GetEntity()->GetScriptTable(), "ScriptEvent", event, value, str);
}

bool CActor::CreateCodeEvent(SmartScriptTable &rTable)
{
	return false;
}

void CActor::AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event)
{
	EntityScripts::CallScriptFunction(GetEntity(), GetEntity()->GetScriptTable(), "AnimationEvent", event.m_EventName,  (float)atof(event.m_CustomParameter));
}


// In:	threshold angle, in degrees, that is needed before turning is even considered (>= 0.0f).
// In:	the current angle deviation needs to be over the turnThresholdAngle for longer than this time before the character turns (>= 0.0f).
//
void CActor::SetTurnAnimationParams(const float turnThresholdAngle, const float turnThresholdTime)
{			
	assert(turnThresholdTime >= 0.0f);
	assert(turnThresholdAngle >= 0.0f);

	m_params.AITurnParams.minimumAngle = DEG2RAD(turnThresholdAngle);
	m_params.AITurnParams.maximumDelay = turnThresholdTime;
}


void CActor::RequestFacialExpression(const char* pExpressionName /* = NULL */, f32* sequenceLength) 
{
	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	IFacialInstance* pFacialInstance = (pCharacter ? pCharacter->GetFacialInstance() : 0);
	if (pFacialInstance)
	{
		IFacialAnimSequence* pSequence = pFacialInstance->LoadSequence(pExpressionName);
		pFacialInstance->PlaySequence(pSequence, eFacialSequenceLayer_AIExpression);
		if (pSequence)
		{
			if (sequenceLength)
			{
				Range r = pSequence->GetTimeRange();
				*sequenceLength = r.end - r.start;
			}
		}
		else if (pExpressionName != NULL)
		{
			// Try with a effector channel instead. This behavior would have cleaned any expression
			// on the face (with the previous PlaySequence(NULL...))
			IFacialModel* pFacialModel = pFacialInstance->GetFacialModel();
			IFacialEffectorsLibrary* pLibrary = pFacialModel ? pFacialModel->GetLibrary() : NULL;
			if (pLibrary)
			{
				IFacialEffector* pEffector = pLibrary->Find(pExpressionName);
				if (pEffector)
					pFacialInstance->StartEffectorChannel(pEffector, 1.0f, 0.1f);
			}
		}
	}
}

void CActor::PrecacheFacialExpression(const char* pExpressionName)
{
	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	IFacialInstance* pFacialInstance = (pCharacter ? pCharacter->GetFacialInstance() : 0);
	if (pFacialInstance)
		pFacialInstance->PrecacheFacialExpression(pExpressionName);
}

void CActor::PostSerialize()
{
	if(ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0))
	{
    if(ISkeletonPose *pSkelPose = pCharacter->GetISkeletonPose())
		{
      pSkelPose->SetForceSkeletonUpdate(2);
		}
	}

	m_telemetry.PostSerialize();
}

void CActor::SetChannelId(uint16 id)
{
}

void CActor::SetHealth(float health)
{
	if (health <= 0.0f)
	{
		if (IsGod() > 0) // handled in CPlayer
			return;

		//TODO: Remove this on _RELEASE?
		if (IsClient() == false)
		{
			if (gEnv->pAISystem && GetEntity()->GetAI())
				gEnv->pAISystem->DebugReportDeath(GetEntity()->GetAI());
		}

		health = 0.0f;
	}

	if (gEnv->bServer && g_pGameCVars->sv_pacifist && health < m_health.GetHealth())
		return;
	
	float prevHealth = m_health.GetHealth();

#ifdef _RELEASE
	m_health.SetHealth( min(health, m_health.GetHealthMax()) );
#else
	float fNewHealth = min(health, m_health.GetHealthMax());
	if (!IsPlayer())
		fNewHealth = (float)__fsel(g_pGameCVars->g_instantKillDamageThreshold, __fsel(g_pGameCVars->g_instantKillDamageThreshold - (prevHealth - fNewHealth), fNewHealth, 0.0f), fNewHealth);
	m_health.SetHealth( (float)__fsel(g_pGameCVars->g_maximumDamage, max(m_health.GetHealth() - g_pGameCVars->g_maximumDamage, fNewHealth), fNewHealth) );
#endif

	if (m_health.GetHealth()!=prevHealth && m_health.GetHealth()<=0.0f)
	{
		IItem *pItem = GetCurrentItem();
		IWeapon *pWeapon = pItem ? pItem->GetIWeapon() : NULL;

		if (pWeapon)
			pWeapon->StopFire();
	}
	//m_pGameplayRecorder->Event(GetEntity(), GameplayEvent(eGE_HealthChanged, 0, m_health, 0));
}

void CActor::DamageInfo(EntityId shooterID, EntityId weaponID, IEntityClass *pProjectileClass, float damage, int damageType, const Vec3 hitDirection)
{
}


void InitializeHitRecoilGameEffect()
{
}


void CActor::SetMaxHealth( float maxHealth )
{
	if (gEnv->bMultiplayer && IsPlayer())
	{
#ifndef _RELEASE
		//const int oldMaxHealth = maxHealth;
#endif

		const float fmaxh = maxHealth;
		maxHealth = max(1.0f, floorf(fmaxh * g_pGameCVars->g_maxHealthMultiplier));

#ifndef _RELEASE
		//CryLog ("CActor::SetMaxHealth() - Scaling max health %d for %s '%s' by %.3f, giving %d", oldMaxHealth, GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), g_pGameCVars->g_maxHealthMultiplier, maxHealth);
#endif
	}

	assert (maxHealth > 0.0f);
	m_health.SetHealthMax( maxHealth );

	if (m_health.GetHealth() > 0.0f)
	{
		SetHealth(maxHealth);
	}
}

void CActor::Kill()
{
	if(IsClient())
	{
		// Clear force feedback
		IForceFeedbackSystem* pForceFeedbackSystem = gEnv->pGameFramework->GetIForceFeedbackSystem();
		if(pForceFeedbackSystem)
		{
			pForceFeedbackSystem->StopAllEffects();
		}
	}

	if (!m_health.IsDead())
		SetHealth(0.0f);

	IEntity *pEntity = GetEntity();
	if (IAIObject *pAI = pEntity->GetAI())
	{
		pAI->Event(AIEVENT_DISABLE, NULL);
	}

	SetCloakLayer(false);
	
	if (IVehicle* pVehicle = GetLinkedVehicle())
	{
		if (IVehicleSeat* pVehicleSeat = pVehicle->GetSeatForPassenger(GetEntityId()))
			pVehicleSeat->OnPassengerDeath();
	}

	g_pGame->GetTacticalManager()->RemoveEntity(GetEntityId(), CTacticalManager::eTacticalEntity_Unit);
	if(!gEnv->bMultiplayer)
	{
		SHUDEvent hudevent(eHUDEvent_RemoveEntity);
		hudevent.AddData(SHUDEventData((int)GetEntityId()));
		CHUDEventDispatcher::CallEvent(hudevent);
	}

	RequestFacialExpression("Exp_Dead");

}

//------------------------------------------------------------------------
bool CActor::LoadDynamicAimPoseElement(CScriptSetGetChain& gameParamsTableChain, const char* szName, string& output)
{
	const char* pose;
	pose = 0;
	if (gameParamsTableChain.GetValue(szName, pose))
		output = pose;
	return !output.empty();
}


//------------------------------------------------------------------------
bool CActor::LoadGameParams(SmartScriptTable pEntityTable, SActorGameParams &outGameParams)
{
	assert((bool)pEntityTable);

	bool bResult = false;

	SmartScriptTable pGameParams;
	if (pEntityTable && pEntityTable->GetValue("gameParams", pGameParams))
	{
		SmartScriptTable tempTable;

		if (pGameParams->GetValue("stance", tempTable))
		{
			IScriptTable::Iterator iter = tempTable->BeginIteration();
			int stance;

			while (tempTable->MoveNext(iter))
			{
				SmartScriptTable stanceTable;

				if (iter.value.CopyTo(stanceTable))
				{
					if (stanceTable->GetValue("stanceId",stance))
					{
						if (stance == STANCE_NULL)
							break;

						if (stance > STANCE_NULL && stance < STANCE_LAST)
						{
							SStanceInfo &sInfo = outGameParams.stances[stance];

							CScriptSetGetChain stanceTableChain(stanceTable);
							stanceTableChain.GetValue("normalSpeed",sInfo.normalSpeed);
							stanceTableChain.GetValue("maxSpeed",sInfo.maxSpeed);
							stanceTableChain.GetValue("heightCollider",sInfo.heightCollider);
							stanceTableChain.GetValue("heightPivot",sInfo.heightPivot);
							stanceTableChain.GetValue("groundContactEps",sInfo.groundContactEps);
							stanceTableChain.GetValue("size",sInfo.size);
							stanceTableChain.GetValue("modelOffset",sInfo.modelOffset);

							stanceTableChain.GetValue("viewOffset",sInfo.viewOffset);
							sInfo.leanLeftViewOffset = sInfo.leanRightViewOffset = sInfo.viewOffset;
							stanceTableChain.GetValue("leanLeftViewOffset",sInfo.leanLeftViewOffset);
							stanceTableChain.GetValue("leanRightViewOffset",sInfo.leanRightViewOffset);
							sInfo.whileLeanedLeftViewOffset = sInfo.whileLeanedRightViewOffset = sInfo.viewOffset;
							stanceTableChain.GetValue("whileLeanedLeftViewOffset", sInfo.whileLeanedLeftViewOffset);
							stanceTableChain.GetValue("whileLeanedRightViewOffset", sInfo.whileLeanedRightViewOffset);

							sInfo.peekOverViewOffset = sInfo.viewOffset;
							stanceTableChain.GetValue("peekOverViewOffset", sInfo.peekOverViewOffset);
							sInfo.peekOverWeaponOffset = sInfo.weaponOffset;
							stanceTableChain.GetValue("peekOverWeaponOffset", sInfo.peekOverWeaponOffset);

							stanceTableChain.GetValue("viewDownYMod",sInfo.viewDownYMod);

							stanceTableChain.GetValue("weaponOffset",sInfo.weaponOffset);
								sInfo.leanLeftWeaponOffset = sInfo.leanRightWeaponOffset = sInfo.weaponOffset;
							stanceTableChain.GetValue("leanLeftWeaponOffset",sInfo.leanLeftWeaponOffset);
								stanceTableChain.GetValue("leanRightWeaponOffset",sInfo.leanRightWeaponOffset);
							sInfo.whileLeanedLeftWeaponOffset = sInfo.whileLeanedRightWeaponOffset = sInfo.weaponOffset;
							stanceTableChain.GetValue("whileLeanedLeftWeaponOffset", sInfo.whileLeanedLeftWeaponOffset);
							stanceTableChain.GetValue("whileLeanedRightWeaponOffset", sInfo.whileLeanedRightWeaponOffset);

							stanceTableChain.GetValue("useCapsule",sInfo.useCapsule);

							const char *name = 0;
							if (stanceTableChain.GetValue("name",name))
							{
								cry_strcpy(sInfo.name, name);
							}
						}
					}
				}
			}

			tempTable->EndIteration(iter);
		}

		if (pGameParams->GetValue("boneIDs", tempTable))
		{
			for (uint32 i=0; i<BONE_ID_NUM; i++)
			{
				const char *name = 0;
				if (tempTable->GetValue(s_BONE_ID_NAME[i], name))
				{
					outGameParams.boneNames[i] = name;
				}
			}
		}

		SActorParams &actorParams = outGameParams.actorParams;
		CScriptSetGetChain gameParamsTableChain(pGameParams);
		gameParamsTableChain.GetValue("meeleHitRagdollImpulseScale", actorParams.meeleHitRagdollImpulseScale);

		if (gameParamsTableChain.GetValue("aimFOV", actorParams.aimFOVRadians))
			actorParams.aimFOVRadians = DEG2RAD(actorParams.aimFOVRadians);

		if (gameParamsTableChain.GetValue("lookFOV", actorParams.lookFOVRadians))
			actorParams.lookFOVRadians = DEG2RAD(actorParams.lookFOVRadians);

		if (gameParamsTableChain.GetValue("lookInVehicleFOV", actorParams.lookInVehicleFOVRadians))
			actorParams.lookInVehicleFOVRadians = DEG2RAD(actorParams.lookInVehicleFOVRadians);
		else
			actorParams.lookInVehicleFOVRadians = actorParams.lookFOVRadians;

		gameParamsTableChain.GetValue("allowLookAimStrafing", actorParams.allowLookAimStrafing);
		gameParamsTableChain.GetValue("cornerSmoother", actorParams.cornerSmoother);

		if (gameParamsTableChain.GetValue("maxLookAimAngle", actorParams.maxLookAimAngleRadians))
			actorParams.maxLookAimAngleRadians = DEG2RAD(actorParams.maxLookAimAngleRadians);

		gameParamsTableChain.GetValue("sprintMultiplier", actorParams.sprintMultiplier);
		gameParamsTableChain.GetValue("crouchMultiplier", actorParams.crouchMultiplier);
		gameParamsTableChain.GetValue("strafeMultiplier", actorParams.strafeMultiplier);
		gameParamsTableChain.GetValue("backwardMultiplier", actorParams.backwardMultiplier);

		gameParamsTableChain.GetValue("jumpHeight", actorParams.jumpHeight);

		gameParamsTableChain.GetValue("leanShift", actorParams.leanShift);
		gameParamsTableChain.GetValue("leanAngle", actorParams.leanAngle);

		gameParamsTableChain.GetValue("speedMultiplier", actorParams.internalSpeedMult);

		// View-related

		Vec3 limitDir(ZERO);
		float limitH = 0.f, limitV = 0.f;

		gameParamsTableChain.GetValue("viewLimitDir", limitDir);
		gameParamsTableChain.GetValue("viewLimitYaw", limitH);
		gameParamsTableChain.GetValue("viewLimitPitch", limitV);
		gameParamsTableChain.GetValue("viewFoVScale", actorParams.viewFoVScale);

		actorParams.viewLimits.SetViewLimit(limitDir, limitH, limitV, 0.f, 0.f, SViewLimitParams::eVLS_None);

		gameParamsTableChain.GetValue("canUseComplexLookIK", actorParams.canUseComplexLookIK);

		const char* lookAtSimpleHeadBoneName = 0;
		if (gameParamsTableChain.GetValue("lookAtSimpleHeadBone", lookAtSimpleHeadBoneName))
		{
			actorParams.lookAtSimpleHeadBoneName = lookAtSimpleHeadBoneName;
		}

		gameParamsTableChain.GetValue("aimIKLayer", actorParams.aimIKLayer);
		gameParamsTableChain.GetValue("lookIKLayer", actorParams.lookIKLayer);

		gameParamsTableChain.GetValue("aimIKFadeDuration", actorParams.aimIKFadeDuration);
		gameParamsTableChain.GetValue("proceduralLeaningFactor", actorParams.proceduralLeaningFactor);

		actorParams.useDynamicAimPoses  = LoadDynamicAimPoseElement(gameParamsTableChain, "idleLeftArmAimPose", actorParams.idleDynamicAimPose.leftArmAimPose);
		actorParams.useDynamicAimPoses |= LoadDynamicAimPoseElement(gameParamsTableChain, "idleRightArmAimPose", actorParams.idleDynamicAimPose.rightArmAimPose);
		actorParams.useDynamicAimPoses |= LoadDynamicAimPoseElement(gameParamsTableChain, "idleBothArmsAimPose", actorParams.idleDynamicAimPose.bothArmsAimPose);
		actorParams.useDynamicAimPoses |= LoadDynamicAimPoseElement(gameParamsTableChain, "runLeftArmAimPose", actorParams.runDynamicAimPose.leftArmAimPose);
		actorParams.useDynamicAimPoses |= LoadDynamicAimPoseElement(gameParamsTableChain, "runRightArmAimPose", actorParams.runDynamicAimPose.rightArmAimPose);
		actorParams.useDynamicAimPoses |= LoadDynamicAimPoseElement(gameParamsTableChain, "runBothArmsAimPose", actorParams.runDynamicAimPose.bothArmsAimPose);

		float bothArmsAimFOV = 0.0f;
		if (gameParamsTableChain.GetValue("bothArmsAimFOV", bothArmsAimFOV))
		{
			actorParams.bothArmsAimHalfFOV = DEG2RAD(bothArmsAimFOV) * 0.5f;
		}

		gameParamsTableChain.GetValue("bothArmsAimPitchFactor", actorParams.bothArmsAimPitchFactor);

		{
			float minimumAngleDegrees;
			if (gameParamsTableChain.GetValue("turnThresholdAngle", minimumAngleDegrees))
			{
				actorParams.AITurnParams.minimumAngle = max(DEG2RAD(minimumAngleDegrees), 0.0f);
			}

			gameParamsTableChain.GetValue("turnThresholdTime", actorParams.AITurnParams.maximumDelay);

			float minimumAngleForTurnWithoutDelayDegrees;
			if (gameParamsTableChain.GetValue("minimumAngleForTurnWithoutDelay", minimumAngleForTurnWithoutDelayDegrees))
			{
				actorParams.AITurnParams.minimumAngleForTurnWithoutDelay = max(DEG2RAD(minimumAngleForTurnWithoutDelayDegrees), actorParams.AITurnParams.minimumAngle);
			}
		}

		gameParamsTableChain.GetValue("stepThresholdDistance", actorParams.stepThresholdDistance);
		gameParamsTableChain.GetValue("stepThresholdTime", actorParams.stepThresholdTime);
		int defStance;
		if (gameParamsTableChain.GetValue("defaultStance", defStance))
		{
			assert((defStance > STANCE_NULL) && (defStance < STANCE_LAST));
			actorParams.defaultStance = (EStance)defStance;
		}

		if (gameParamsTableChain.GetValue("maxDeltaAngleRateNormal", actorParams.maxDeltaAngleRateNormal))
		{
			actorParams.maxDeltaAngleRateNormal = DEG2RAD(actorParams.maxDeltaAngleRateNormal);
		}
		if (gameParamsTableChain.GetValue("maxDeltaAngleRateAnimTarget", actorParams.maxDeltaAngleRateAnimTarget))
		{
			actorParams.maxDeltaAngleRateAnimTarget = DEG2RAD(actorParams.maxDeltaAngleRateAnimTarget);
		}
		if (gameParamsTableChain.GetValue("maxDeltaAngleMultiplayer", actorParams.maxDeltaAngleMultiplayer))
		{
			actorParams.maxDeltaAngleMultiplayer = DEG2RAD(actorParams.maxDeltaAngleMultiplayer);
		}
		if (gameParamsTableChain.GetValue("maxDeltaAngleRateJukeTurn", actorParams.maxDeltaAngleRateJukeTurn))
		{
			actorParams.maxDeltaAngleRateJukeTurn = DEG2RAD(actorParams.maxDeltaAngleRateJukeTurn);
		}
		gameParamsTableChain.GetValue("smoothedZTurning", actorParams.smoothedZTurning);

		// Physic-related
		SmartScriptTable physicsParams;
		if(pEntityTable->GetValue("physicsParams", physicsParams))
		{
			physicsParams->GetValue("fallNPlayStiffness_scale", actorParams.fallNPlayStiffness_scale);
		}

		//DBAs for character
		SmartScriptTable characterDBAs;
		if (pGameParams->GetValue("characterDBAs", characterDBAs))
		{
			const int tableElements = characterDBAs->Count();
			for (int i = 1; i <= tableElements; ++i)
			{
				if(characterDBAs->GetAtType(i) == svtString)
				{
					const char* dbaGroupName = NULL;
					characterDBAs->GetAt(i, dbaGroupName);

					CRY_ASSERT(dbaGroupName);

					outGameParams.animationDBAs.push_back(string(dbaGroupName));
				}
			}
		}

		bResult = true;
	}

	SmartScriptTable propertiesTable;
	if (pEntityTable && pEntityTable->GetValue("Properties", propertiesTable))
	{
		// End-of-C3 workaround for the fact that a property that should be editable is in the gameparams table
		bool overrideAllowLookAimStrafing;
		if (propertiesTable->GetValue("bOverrideAllowLookAimStrafing", overrideAllowLookAimStrafing))
		{
			outGameParams.actorParams.allowLookAimStrafing = overrideAllowLookAimStrafing;
		}

		SmartScriptTable characterSoundsTable;
		if (propertiesTable->GetValue("CharacterSounds", characterSoundsTable))
		{
			SActorParams &actorParams = outGameParams.actorParams;

			CScriptSetGetChain characterSoundsChain(characterSoundsTable);

			// Footstep Effect Name
			const char* footstepName = 0;
			if (characterSoundsChain.GetValue("footstepEffect", footstepName) &&
				footstepName && footstepName[0])
			{
				actorParams.footstepEffectName = footstepName;
			}

			const char* remoteFootstepName = 0;
			if (characterSoundsChain.GetValue("remoteFootstepEffect", remoteFootstepName) &&
				remoteFootstepName && remoteFootstepName[0])
			{
				actorParams.remoteFootstepEffectName = remoteFootstepName;
			}

			// Foley Effect Name
			const char* foleyName = 0;
			if (characterSoundsChain.GetValue("foleyEffect", foleyName) &&
				foleyName && foleyName[0])
			{
				actorParams.foleyEffectName = foleyName;
			}
			characterSoundsChain.GetValue("bFootstepGearEffect", actorParams.footstepGearEffect);

			const char* footstepIndGearAudioSignal_Walk = 0;
			if (characterSoundsChain.GetValue("footstepIndGearAudioSignal_Walk", footstepIndGearAudioSignal_Walk) &&
				footstepIndGearAudioSignal_Walk && footstepIndGearAudioSignal_Walk[0])
			{
				actorParams.footstepIndGearAudioSignal_Walk = footstepIndGearAudioSignal_Walk;
			}

			const char* footstepIndGearAudioSignal_Run = 0;
			if (characterSoundsChain.GetValue("footstepIndGearAudioSignal_Run", footstepIndGearAudioSignal_Run) &&
				footstepIndGearAudioSignal_Run && footstepIndGearAudioSignal_Run[0])
			{
				actorParams.footstepIndGearAudioSignal_Run = footstepIndGearAudioSignal_Run;
			}
		}
	}

	return bResult;
}

//------------------------------------------------------------------------
void CActor::InitGameParams()
{
	IEntity *pEntity = GetEntity();

	if (m_LuaCache_GameParams)
	{
		const bool reloadCharacterSounds = !gEnv->bMultiplayer && !IsPlayer();
		InitGameParams(m_LuaCache_GameParams->gameParams, reloadCharacterSounds);
	}
	else
	{
		IEntityClass *pClass = pEntity->GetClass();

		// Run-time loading
		if (CGameCache::IsLuaCacheEnabled())
		{
			GameWarning("[Game Cache] Warning: Loading game params for entity class \'%s\' at run-time!", pClass->GetName());
		}

		const bool reloadCharacterSounds = false;
		SActorGameParams gameParams;
		if (LoadGameParams(pEntity->GetScriptTable(), gameParams))
		{
			InitGameParams(gameParams, reloadCharacterSounds);
		}
		else
		{
			GameWarning("Failed to load game params for actor \'%s\'", pEntity->GetName());
		}
	}
}

//------------------------------------------------------------------------
void CActor::SetStanceMaxSpeed(uint32 stance, float fValue)
{
	if ((stance < STANCE_LAST))
	{
		m_stances[stance].maxSpeed = fValue;
	}
}

//------------------------------------------------------------------------
void CActor::InitGameParams(const SActorGameParams &gameParams, const bool reloadCharacterSounds)
{
	// Copy stances over
	for (uint32 stance = 0; stance < STANCE_LAST; ++stance)
	{
		m_stances[stance] = gameParams.stances[stance];
	}

	// Create bone Ids
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	ISkeletonPose *pSkelPose = pCharacter ? pCharacter->GetISkeletonPose() : NULL;
	if (pSkelPose)
	{
		IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
		for (uint32 boneId = 0; boneId < BONE_ID_NUM; ++boneId)
		{
			const string &sName = gameParams.boneNames[boneId];
			if (!sName.empty())
			{
				m_boneIDs[boneId] = rIDefaultSkeleton.GetJointIDByName(sName.c_str());
				if (m_boneIDs[boneId] < 0)
				{
					GameWarning("CActor %s - %s not a valid joint for BoneID %s in character %s", GetEntity()->GetName(), sName.c_str(), s_BONE_ID_NAME[boneId], pCharacter->GetFilePath());
				}
			}
		}
	}

	// Copy params over
	m_params = gameParams.actorParams;

	// Workaround: Need to override sound properties because these are per instance
	// TODO: Move sound values into properties instance cache (needs some more work and changes) 
	if(reloadCharacterSounds)
	{
		IScriptTable* pScriptTable = GetEntity()->GetScriptTable();
		SmartScriptTable propertiesTable;
		if((pScriptTable != NULL) && pScriptTable->GetValue("Properties", propertiesTable))
		{
			assert(propertiesTable.GetPtr() != NULL);

			SmartScriptTable characterSoundsTable;
			if (propertiesTable->GetValue("CharacterSounds", characterSoundsTable))
			{
				CScriptSetGetChain characterSoundsChain(characterSoundsTable);

				// Footstep Effect Name
				const char* footstepName = "";
				if (characterSoundsChain.GetValue("footstepEffect", footstepName))
				{
					m_params.footstepEffectName = footstepName;
				}

				const char* remoteFootstepName = "";
				if (characterSoundsChain.GetValue("remoteFootstepEffect", remoteFootstepName))
				{
					m_params.remoteFootstepEffectName = remoteFootstepName;
				}

				// Foley Effect Name
				const char* foleyName = "";
				if (characterSoundsChain.GetValue("foleyEffect", foleyName))
				{
					m_params.foleyEffectName = foleyName;
				}

				characterSoundsChain.GetValue("bFootstepGearEffect", m_params.footstepGearEffect);

				const char* footstepIndGearAudioSignal_Walk = "";
				if (characterSoundsChain.GetValue("footstepIndGearAudioSignal_Walk", footstepIndGearAudioSignal_Walk))
				{
					m_params.footstepIndGearAudioSignal_Walk = footstepIndGearAudioSignal_Walk;
				}

				const char* footstepIndGearAudioSignal_Run = "";
				if (characterSoundsChain.GetValue("footstepIndGearAudioSignal_Run", footstepIndGearAudioSignal_Run))
				{
					m_params.footstepIndGearAudioSignal_Run = footstepIndGearAudioSignal_Run;
				}
			}
		}
		
	}

	SetSpeedMultipler(SActorParams::eSMR_Internal, m_params.internalSpeedMult);
}

//------------------------------------------------------------------------
void CActor::SetParamsFromLua(SmartScriptTable &rTable)
{
	
}

bool CActor::IsClient() const
{
	return m_isClient || (GetEntityId() == gEnv->pGameFramework->GetClientActorId());
}

bool CActor::IsPlayer() const
{
	return m_isPlayer;
}


bool CActor::SetAspectProfile( EEntityAspects aspect, uint8 profile )
{
	bool res(false);

	if (aspect == eEA_Physics)
	{
		if (!gEnv->bMultiplayer && m_currentPhysProfile==profile && !gEnv->pSystem->IsSerializingFile()) //rephysicalize when loading savegame
			return true;

		switch (profile)
		{
		case eAP_NotPhysicalized:
			{
				SEntityPhysicalizeParams params;
				params.type = PE_NONE;
				GetEntity()->Physicalize(params);
			}
			res=true;
			break;
		case eAP_Spectator:
		case eAP_Alive:
			{
				if(!gEnv->bMultiplayer)
				{
					// if we were asleep, we just want to wakeup
					if (profile==eAP_Alive && (m_currentPhysProfile==eAP_Sleep || m_currentPhysProfile==eAP_Ragdoll))
					{
						ICharacterInstance *pCharacter=GetEntity()->GetCharacter(0);
						if (pCharacter && pCharacter->GetISkeletonAnim())
						{
							IPhysicalEntity *pPhysicalEntity=0;
							Matrix34 delta(IDENTITY);

							if (m_currentPhysProfile==eAP_Sleep)
							{
								QuatTS location(GetEntity()->GetWorldTM());
								pCharacter->GetISkeletonPose()->BlendFromRagdoll(location, pPhysicalEntity, false);
								delta = Matrix34(location);

								if (pPhysicalEntity)
								{
									pe_player_dynamics pd;
									pd.gravity.Set(0.0f, 0.0f, -1.8f);
									pPhysicalEntity->SetParams(&pd);

									pe_params_flags pf;
									pf.flagsOR = pef_ignore_areas;
									pPhysicalEntity->SetParams(&pf);

									pe_params_foreign_data pfd;
									pfd.iForeignData = PHYS_FOREIGN_ID_ENTITY;
									pfd.pForeignData = GetEntity();
									for (int iAuxPhys = -1; ; ++iAuxPhys)
									{
										if (IPhysicalEntity* pent = pCharacter->GetISkeletonPose()->GetCharacterPhysics(iAuxPhys))
										{
											pent->SetParams(&pfd);
										}
										else
										{
											break;
										}
									}

									{
										GetEntity()->SetWorldTM(delta);
										GetEntity()->AssignPhysicalEntity(pPhysicalEntity);
									}
								}
							}
							else
							{
								CRY_ASSERT(m_currentPhysProfile==eAP_Ragdoll);

								pCharacter->GetISkeletonPose()->SetDefaultPose();
								Physicalize(STANCE_NULL);
							}

							assert(m_pAnimatedCharacter);
							m_pAnimatedCharacter->ForceRefreshPhysicalColliderMode();

							PhysicalizeBodyDamage();
						}
					}
					else
					{
						Physicalize(STANCE_NULL);
					}
				}
			}
			res=true;
			break;
		case eAP_Linked:
			// make sure we are alive, for when we transition from ragdoll to linked...
			if (!GetEntity()->GetPhysics() || GetEntity()->GetPhysics()->GetType()!=PE_LIVING)
				Physicalize();
			res=true;
			break;
		case eAP_Sleep:
			RagDollize(true);
			res=true;
			break;
		case eAP_Ragdoll:
			// killed while sleeping?
			RagDollize(false);
			res=true;
			break;
		}

		m_currentPhysProfile = profile;
	}

	return res;
}

bool CActor::GetRagdollContext( CProceduralContextRagdoll** ppRagdollContext ) const
{
	CRY_ASSERT( ppRagdollContext );

	IActionController* piActionController = m_pAnimatedCharacter->GetActionController();
	if( !gEnv->bMultiplayer && piActionController )
	{
		IProceduralContext* piProcContext = piActionController->FindOrCreateProceduralContext(CProceduralContextRagdoll::GetCID());
		if( piProcContext )
		{
			*ppRagdollContext = static_cast<CProceduralContextRagdoll*> (piProcContext);
			return true;
		}
	}
	return false;
}

bool CActor::AllowPhysicsUpdate(uint8 newCounter) const
{
	return AllowPhysicsUpdate(newCounter, m_netPhysCounter);
}

bool CActor::AllowPhysicsUpdate(uint8 newCounter, uint8 oldCounter)
{
	return (newCounter == oldCounter);
}

bool CActor::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags )
{
	NET_PROFILE_SCOPE("Actor NetSerialize", ser.IsReading());
	m_damageEffectController.NetSerialiseEffects(ser, aspect);

	if (aspect == eEA_Physics)
	{
		pe_type type = PE_NONE;
		switch (profile)
		{
		case eAP_NotPhysicalized:
			type = PE_NONE;
			break;
		case eAP_Spectator:
			type = PE_LIVING;
			break;
		case eAP_Alive:
			type = PE_LIVING;
			break;
		case eAP_Sleep:
		case eAP_Ragdoll:
			type = PE_NONE;
			break;
		case eAP_Linked: 	//if actor is attached to a vehicle - don't serialize actor physics additionally
			return true;
			break;
		default:
			return false;
		}

		if (type == PE_NONE)
		{
			return true;
		}

		NET_PROFILE_SCOPE("Physics", ser.IsReading());

		if (ser.IsWriting())
		{
			if (!GetEntity()->GetPhysicalEntity() || GetEntity()->GetPhysicalEntity()->GetType() != type)
			{
				if(type!=PE_LIVING)
				{
					gEnv->pPhysicalWorld->SerializeGarbageTypedSnapshot( ser, type, 0 );
				}
				return true;
			}
		}

		if(type!=PE_LIVING)
		{
			GetEntity()->PhysicsNetSerializeTyped( ser, type, pflags );
		}
	}

	return true;
}


void CActor::ProceduralRecoil( float duration, float kinematicImpact, float kickIn/*=0.8f*/, int arms/*=0*/ )
{
	if(m_pAnimatedCharacter)
	{
		EAnimatedCharacterArms animCharArms;
		switch(arms)
		{
		case 1: animCharArms = eACA_RightArm; break;
		case 2: animCharArms = eACA_LeftArm; break;
		default: animCharArms = eACA_BothArms;
		}
		m_pAnimatedCharacter->TriggerRecoil(duration, kinematicImpact, kickIn, animCharArms);
	}
}

void CActor::HandleEvent( const SGameObjectEvent& event )
{
	switch( event.event )
	{
	case eCGE_OnShoot:
		{
			SActorStats *pStats = GetActorStats();
			if (pStats)
				pStats->inFiring = 10.0f;
		}
		break;
	case eCGE_Ragdollize:
		{
			const bool bSleep = event.paramAsBool;
			if( gEnv->bServer )
			{
				const EActorPhysicalization actorPhys = bSleep ? eAP_Sleep : eAP_Ragdoll;
				GetGameObject()->SetAspectProfile(eEA_Physics, actorPhys);
			}
			else
			{
				RagDollize( bSleep );
			}
		}
		break;
	case eGFE_EnableBlendRagdoll:
		{
			const HitInfo& hitInfo = *static_cast<const HitInfo*>(event.ptr);

			OnFall(hitInfo);
		}
		break;
	case eGFE_DisableBlendRagdoll:
		StandUp();
		break;
	case eCGE_EnablePhysicalCollider:
		{
			assert(m_pAnimatedCharacter);
			m_pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "Actor::HandleEvent");
		}
		break;
	case eCGE_DisablePhysicalCollider:
		{
			assert(m_pAnimatedCharacter);
			m_pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "Actor::HandleEvent");
		}
		break;
	case eGFE_RagdollPhysicalized:
		{
			// Let the actor impulse handler know 
			if(m_pImpulseHandler)
			{
				m_pImpulseHandler->OnRagdollPhysicalized(); 
			}
		}
		break;
	}
}

void CActor::ResetAnimationState()
{
	SetStance(STANCE_RELAXED);
}

void CActor::QueueAnimationState( const char * state )
{
	if (m_pAnimatedCharacter)
		m_pAnimatedCharacter->PushForcedState( state );
}


Vec3 CActor::GetLocalEyePos() const
{
	if (HasBoneID(BONE_CAMERA))
	{
		Vec3 camera = GetBoneTransform(BONE_CAMERA).t;
		if (camera.IsValid())
			return camera;
	}

	if (HasBoneID(BONE_EYE_R) && HasBoneID(BONE_EYE_L))
	{
		Vec3 reye = GetBoneTransform(BONE_EYE_R).t;
		Vec3 leye = GetBoneTransform(BONE_EYE_L).t;
		if (reye.IsValid() && leye.IsValid())
			return (reye+leye)*0.5f;
	}

	return GetStanceInfo(m_stance)->viewOffset;
}

QuatT CActor::GetCameraTran() const
{
	if (HasBoneID(BONE_CAMERA))
	{
		QuatT camera = GetBoneTransform(BONE_CAMERA);
		CRY_ASSERT(camera.IsValid());

		return camera;
	}

	CRY_ASSERT(GetSpectatorState() == eASS_SpectatorMode);
	QuatT returnQuatT;
	returnQuatT.SetIdentity();
	return returnQuatT;
}

QuatT CActor::GetHUDTran() const
{
	if (HasBoneID(BONE_HUD))
	{
		QuatT hud = GetBoneTransform(BONE_HUD);
		CRY_ASSERT(hud.IsValid());

		return hud;
	}

	return GetCameraTran();
}

bool CActor::CheckInventoryRestrictions(const char *itemClassName)
{
	if (g_pGameCVars->g_inventoryNoLimits != 0)
		return true;

	if (m_pInventory)
		return m_pInventory->IsAvailableSlotForItemClass(itemClassName);


	return false;
}

//IK stuff
void CActor::SetIKPos(const char *pLimbName, const Vec3& goalPos, int priority)
{
	int limbID = GetIKLimbIndex(pLimbName);
	if (limbID > -1)
	{
		Vec3 pos(goalPos);
		m_IKLimbs[limbID].SetWPos(GetEntity(),pos,ZERO,0.5f,0.5f,priority);
	}
}

void CActor::CreateIKLimb(const SActorIKLimbInfo &limbInfo)
{
	for (size_t i=0;i<m_IKLimbs.size();++i)
	{
		if (!limbInfo.sLimbName.compare(m_IKLimbs[i].name))
			return;
	}

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(limbInfo.characterSlot);
	if (pCharacter)
	{
		SIKLimb newLimb;
		newLimb.SetLimb(limbInfo.characterSlot, 
			limbInfo.sLimbName.c_str(), 
			pCharacter->GetIDefaultSkeleton().GetJointIDByName(limbInfo.sRootBone.c_str()), 
			pCharacter->GetIDefaultSkeleton().GetJointIDByName(limbInfo.sMidBone.c_str()), 
			pCharacter->GetIDefaultSkeleton().GetJointIDByName(limbInfo.sEndBone.c_str()), 
			limbInfo.flags);

		if (newLimb.endBoneID>-1 && newLimb.rootBoneID>-1)
			m_IKLimbs.push_back(newLimb);
	}
}

int CActor::GetIKLimbIndex(const char *limbName)
{
	for (size_t i=0;i<m_IKLimbs.size();++i)
		if (!strcmp(limbName,m_IKLimbs[i].name))
			return i;

	return -1;
}

void CActor::ProcessIKLimbs(float frameTime)
{
	IEntity *pEntity = GetEntity();

	//first thing: restore the original animation pose if there was some IK
	//FIXME: it could get some optimization.
	for (size_t i=0;i<m_IKLimbs.size();++i)
		m_IKLimbs[i].Update(pEntity,frameTime);
}

IAnimationGraphState * CActor::GetAnimationGraphState()
{
	if (m_pAnimatedCharacter)
		return m_pAnimatedCharacter->GetAnimationGraphState();
	else
		return NULL;
}

void CActor::SetFacialAlertnessLevel(int alertness)
{
	if (m_pAnimatedCharacter)
		m_pAnimatedCharacter->SetFacialAlertnessLevel(alertness);
}

//------------------------------------------------------------------------
void CActor::SwitchToWeaponWithAccessoryFireMode()
{
	if (!m_pInventory)
		return;

	if (m_pInventory->GetCount() < 1)
		return;

	int startSlot = -1;
	EntityId currentItemId;

	currentItemId = m_pInventory->GetCurrentItem();
	
	if (currentItemId)
	{
		startSlot = m_pInventory->FindItem(currentItemId);
	}

	int currentSlot = startSlot;
	int skip = m_pInventory->GetCount(); // maximum number of interactions
	
	while(skip)
	{
		int slot = currentSlot + 1;

		if (slot >= m_pInventory->GetCount())
			slot = 0;

		if (startSlot == slot)
			return;

		EntityId itemId = m_pInventory->GetItem(slot);
		CWeapon* pWeapon = GetWeapon(itemId);
		
		if (pWeapon && pWeapon->CanSelect())
		{
			int numFiremodes = pWeapon->GetNumOfFireModes();
			int accessoryEnabledMode = -1;

			for(int i = 0; i < numFiremodes && accessoryEnabledMode < 0; i++)
			{
				CFireMode* pFiremode = static_cast<CFireMode*>(pWeapon->GetFireMode(i));

				if(pFiremode && pFiremode->IsEnabledByAccessory())
				{
					accessoryEnabledMode = i;
				}
			}

			if(accessoryEnabledMode >= 0)
			{
				if(pWeapon->GetCurrentFireMode() != accessoryEnabledMode)
				{
					static_cast<CWeapon*>(pWeapon)->RequestFireMode(accessoryEnabledMode);
				}
				
				ScheduleItemSwitch(itemId, true, eICT_Primary);

				const EWeaponSwitchSpecialParam specialParam = eWSSP_None;
				const EItemCategoryType category = eICT_Primary;

				SHUDEventWrapper::OnPrepareItemSelected(itemId, category, specialParam );

				return;
			}
		}

		currentSlot = slot;
		--skip;
	}
}

//------------------------------------------------------------------------
void CActor::SelectNextItem(int direction, bool keepHistory, int category)
{
	if (!m_pInventory)
		return;

	if (m_pInventory->GetCount() < 1)
		return;

	int startSlot = -1;
	int delta = direction;
	EntityId currentItemId;
	
	SActorStats* pActorStats = GetActorStats();
	if (!pActorStats)
		return;

	// If in progress of switching, use the item in progress as current item to allow fast switching
	bool bCurrentItemIsSwitchedItem = false;
	if (pActorStats->exchangeItemStats.switchingToItemID != 0)
	{
		IItem* pSwitchingItem = GetItem(pActorStats->exchangeItemStats.switchingToItemID);
		if (pSwitchingItem != NULL)
		{
			IEntityClass* pEntityClass = pSwitchingItem->GetEntity()->GetClass();
			CRY_ASSERT(pEntityClass != NULL);
			const char* switchingItemCategory = m_pItemSystem->GetItemCategory(pEntityClass->GetName());
			int switchingItemCategoryType = GetItemCategoryType(switchingItemCategory);
			if (switchingItemCategoryType & eICT_Primary || switchingItemCategoryType & eICT_Secondary)
			{
				currentItemId = pActorStats->exchangeItemStats.switchingToItemID;
				bCurrentItemIsSwitchedItem = true;
			}
		}

		if (bCurrentItemIsSwitchedItem == false)
		{
			currentItemId = m_pInventory->GetCurrentItem();
		}
	}
	else
	{
		currentItemId = m_pInventory->GetCurrentItem();
	}

	IItem* pCurrentItem = m_pItemSystem->GetItem(currentItemId);

	bool currWeaponExplosive = false; 

	IInventory::EInventorySlots inventorySlot = IInventory::eInventorySlot_Last;
	
	if (pCurrentItem)
	{
		if (const char* const currentWeaponCat = m_pItemSystem->GetItemCategory(pCurrentItem->GetEntity()->GetClass()->GetName()))
		{
			inventorySlot = m_pInventory->GetSlotForItemCategory(currentWeaponCat);
			currWeaponExplosive = (inventorySlot == IInventory::eInventorySlot_Explosives) || (inventorySlot == IInventory::eInventorySlot_Grenades);
		}
	}

	if(	inventorySlot != IInventory::eInventorySlot_Last 
			&& inventorySlot != IInventory::eInventorySlot_Weapon 
			&& (category & eICT_Primary || category & eICT_Secondary)
			&& !(category & eICT_Explosive || category & eICT_Grenade))
	{
		EntityId lastSelectedItem = m_pInventory->GetLastSelectedInSlot(IInventory::eInventorySlot_Weapon);
		startSlot = m_pInventory->FindItem(lastSelectedItem) - delta;
	}
	else if (currentItemId)
	{
		startSlot = m_pInventory->FindItem(currentItemId);
	}

	EntityId itemId = ComputeNextItem(startSlot, category, delta, keepHistory, pCurrentItem, currWeaponExplosive);

	if(itemId)
	{
		if(IsClient())
		{
			EWeaponSwitchSpecialParam specialParam = eWSSP_None;
			if(category == eICT_None)
			{
				specialParam = (direction>0) ? eWSSP_Next : eWSSP_Prev;
			}

			SHUDEventWrapper::OnPrepareItemSelected(itemId, (EItemCategoryType)category, specialParam );
		}

		ScheduleItemSwitch(itemId, keepHistory, category);
	}
}

//------------------------------------------------------------------------
EntityId CActor::ComputeNextItem(const int startSlot, const int category, const int delta, bool& inOutKeepHistory, IItem* pCurrentItem, const bool currWeaponExplosive) const
{
	int slot = startSlot;
	int skip = m_pInventory->GetCount(); // maximum number of interactions
	
	while(skip)
	{
		slot += delta;

		if (slot<0)
			slot = m_pInventory->GetCount()-1;
		else if (slot >= m_pInventory->GetCount())
			slot = 0;

		EntityId itemId = m_pInventory->GetItem(slot);
		IItem *pItem = m_pItemSystem->GetItem(itemId);
		CRY_ASSERT(pItem);
		if (pItem)
		{
			const char* name = pItem->GetEntity()->GetClass()->GetName();
			const char *nextWeaponCatStr = m_pItemSystem->GetItemCategory(name);
			int nextWeaponCat = GetItemCategoryType(nextWeaponCatStr);
			bool isDiferentItemFromCurrent = pCurrentItem ? pItem->GetEntity()->GetClass() != pCurrentItem->GetEntity()->GetClass() : true;
			if (pItem->CanSelect() && (category == eICT_None || category & nextWeaponCat) && isDiferentItemFromCurrent)
			{
				IInventory::EInventorySlots nextWeaponSlot = nextWeaponCat != eICT_None ? m_pInventory->GetSlotForItemCategory(nextWeaponCatStr) : IInventory::eInventorySlot_Last;
				if (currWeaponExplosive && ((nextWeaponSlot == IInventory::eInventorySlot_Explosives) || (nextWeaponSlot == IInventory::eInventorySlot_Grenades)))
				{
					//don't keep the history when switching from explosive to an explosive as the switch back after wants to go to the last primary/secondary weapon
					inOutKeepHistory = false;
				}
				return itemId;

				}
		}

		--skip;
	}
	return 0;
}

//------------------------------------------------------------------------
CItem *CActor::GetItem(EntityId itemId) const
{
	return static_cast<CItem*>(m_pItemSystem->GetItem(itemId));
}

//------------------------------------------------------------------------
CItem *CActor::GetItemByClass(IEntityClass* pClass) const
{
	if (m_pInventory)
	{
		return static_cast<CItem *>(m_pItemSystem->GetItem(m_pInventory->GetItemByClass(pClass)));
	}

	return 0;
}

//------------------------------------------------------------------------
CWeapon *CActor::GetWeapon(EntityId itemId) const
{
	CItem *pItem = static_cast<CItem *>(m_pItemSystem->GetItem(itemId));
	if (pItem)
		return static_cast<CWeapon *>(pItem->GetIWeapon());

	return 0;
}

//------------------------------------------------------------------------
CWeapon *CActor::GetWeaponByClass(IEntityClass* pClass) const
{
	CItem* pItem = GetItemByClass(pClass);
	if (pItem)
		return static_cast<CWeapon *>(pItem->GetIWeapon());
	return 0;
}

//------------------------------------------------------------------------
void CActor::SelectLastItem(bool keepHistory, bool forceNext /* = false */)
{
	if (!m_pInventory)
		return;

	EntityId itemId = m_pInventory->GetLastItem();
	IItem *pItem = m_pItemSystem->GetItem(itemId);

	if (pItem && itemId != GetCurrentItemId(false))
	{
		IWeapon* pWeapon = pItem->GetIWeapon();

		if(pWeapon && pWeapon->OutOfAmmo(false))
		{
			SelectWeaponWithAmmo(itemId, keepHistory);
		}
		else
		{
			ScheduleItemSwitch(pItem->GetEntityId(), keepHistory);
		}
	}
	else if(forceNext)
	{
		SelectNextItem(1,keepHistory,eICT_Primary|eICT_Secondary); //Prevent binoculars to get stuck under certain circumstances
	}
}

//-----------------------------------------------------------------------
void CActor::SelectWeaponWithAmmo(EntityId currentOutOfAmmoId, bool keepHistory)
{
	EntityId bestOutOfAmmoId = 0;
	EntityId secondaryWithAmmoId = 0;

	if (m_pInventory)
	{
		ItemString primaryCat = "primary";
		ItemString secondaryCat = "secondary";

		int numItems = m_pInventory->GetCount();
		EntityId currentItemId = m_pInventory->GetCurrentItem();

		for(int i = 0; i < numItems; i++)
		{
			EntityId itemId = m_pInventory->GetItem(i);

			if(itemId != currentItemId)
			{
				IItem* pItem = m_pItemSystem->GetItem(itemId);
				CRY_ASSERT(pItem);

				if(pItem && pItem->CanSelect())
				{
					const char* name = pItem->GetEntity()->GetClass()->GetName();
					ItemString category = m_pItemSystem->GetItemCategory(name);

					if(category == primaryCat || category == secondaryCat)
					{
						IWeapon* pWeapon = pItem->GetIWeapon();

						if(pWeapon && !pWeapon->OutOfAmmo(false))
						{
							if(category == primaryCat)
							{
								ScheduleItemSwitch(itemId, keepHistory);
								return;
							}
							else if(!secondaryWithAmmoId)
							{
								secondaryWithAmmoId = itemId;
							}
						}
						else
						{
							if((category == primaryCat) && (!bestOutOfAmmoId || currentOutOfAmmoId == itemId))
							{
								bestOutOfAmmoId = itemId;
							}
						}
					}
				}
			}
		}
	}

	EntityId useId = secondaryWithAmmoId;
	
	if(!useId)
	{
		useId = bestOutOfAmmoId ? bestOutOfAmmoId : currentOutOfAmmoId;
	}

	ScheduleItemSwitch(useId, keepHistory);
}

void CActor::ClearItemActionControllers()
{
	if(IItem* pCurrentItem = GetCurrentItem(true))
	{
		pCurrentItem->SetCurrentActionController(NULL);
	}

	if(SActorStats* pStats = GetActorStats())
	{
		if(IItem* pMountedGun = m_pItemSystem->GetItem(pStats->mountedWeaponID))
		{
			pMountedGun->SetCurrentActionController(NULL);
		}
	}

	if(m_pInventory)
	{
		const int numItems = m_pInventory->GetCount();
		for(int i = 0; i<numItems; i++)
		{
			if(IItem* pItem = m_pItemSystem->GetItem(m_pInventory->GetItem(i)))
			{
				pItem->SetCurrentActionController(NULL);
			}
		}

		// This could be a weapon NOT in your inventory list.
		if(IItem* pHolsteredItem = m_pItemSystem->GetItem(m_pInventory->GetHolsteredItem()))
		{
			pHolsteredItem->SetCurrentActionController(NULL);
		}

		// This could be a weapon NOT in your inventory list.
		if(IItem* pCurrentInvItem = m_pItemSystem->GetItem(m_pInventory->GetCurrentItem()))
		{
			pCurrentInvItem->SetCurrentActionController(NULL);
		}
	}
}

void CActor::AttemptToRecycleAIActor()
{
	const bool removalAllowed = EntityScripts::CallScriptFunction( GetEntity(), GetEntity()->GetScriptTable(), "AIWaveAllowsRemoval" );

	if (removalAllowed)
	{
		if (gEnv->IsEditor())
		{
			EntityScripts::CallScriptFunction( GetEntity(), GetEntity()->GetScriptTable(), "ShutDown" );
		}
		else
		{
			SetHealth( 0.0f );
			{
				SEntityPhysicalizeParams params;
				params.type = PE_NONE;
				GetEntity()->Physicalize(params);
			}
			gEnv->pEntitySystem->RemoveEntity( GetEntityId() );
		}
	}
	else
	{
		// Attempt later again
		GetEntity()->SetTimer( RECYCLE_AI_ACTOR_TIMER_ID, 2000 );
	}
}

//-----------------------------------------------------------------------
bool CActor::ScheduleItemSwitch(EntityId itemId, bool keepHistory, int category, bool forceFastSelect)
{
	CItem* pCurrentItem = NULL;

	SActorStats* pActorStats = GetActorStats();
	if (!pActorStats)
	{
		return false;
	}

	// If already switching, current item is the item being switched to
	if (pActorStats->exchangeItemStats.switchingToItemID != 0)
	{
		// Only allow fast switching for primary and secondary weapons
		// current item is checked in CPlayerInput::OnAction
		if (category & eICT_Primary || category & eICT_Secondary)
		{
			pCurrentItem = GetItem(pActorStats->exchangeItemStats.switchingToItemID);
			if (pCurrentItem != NULL)
			{
				IEntityClass* pEntityClass = pCurrentItem->GetEntity()->GetClass();
				CRY_ASSERT(pEntityClass != NULL);
				const char* switchingItemCategory = m_pItemSystem->GetItemCategory(pEntityClass->GetName());
				int switchingItemCategoryType = GetItemCategoryType(switchingItemCategory);
				if (switchingItemCategoryType & eICT_Primary || switchingItemCategoryType & eICT_Secondary)
				{
					// Possible item switch timer already set up if just started deselecting, disable it from firing
					CancelScheduledSwitch();

					SelectItem(itemId, keepHistory, true);

					return false;
				}
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		pCurrentItem = static_cast<CItem*>(GetCurrentItem());
	}

	bool fastDeselect = forceFastSelect;
	const bool isMounted = (pCurrentItem && (pCurrentItem->IsMounted() || pCurrentItem->IsRippingOrRippedOff()));

	if (pCurrentItem && pCurrentItem->HasFastSelect(itemId))
	{
		fastDeselect = true;
	}
	
	if (!isMounted && (gEnv->IsEditing() == false))
	{
		//If it's the same item we have, just ignore it
		if (pCurrentItem && pCurrentItem->GetEntityId() == itemId)
		{
			return false;
		}

		//If already switching, ignore
		if (pActorStats->exchangeItemStats.switchingToItemID != 0)
		{
			return false;
		}

		uint32 deselectDelay = pCurrentItem ? pCurrentItem->StartDeselection(fastDeselect) : 0;

		if(deselectDelay > 0)
		{
			pActorStats->exchangeItemStats.switchingToItemID = itemId;
			pActorStats->exchangeItemStats.keepHistory = keepHistory;

			GetEntity()->SetTimer(ITEM_SWITCH_THIS_FRAME, deselectDelay);

			return true;
		}
	}
	
	SelectItem(itemId, keepHistory, false);

	return false;
}

//------------------------------------------------------------------------
void CActor::SelectItemByName(const char *name, bool keepHistory, bool forceFastSelect)
{
	if (!m_pInventory)
		return;

	if (m_pInventory->GetCount() < 1)
		return;

	IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(name);
	EntityId itemId = m_pInventory->GetItemByClass(pClass);
	IItem *pItem = m_pItemSystem->GetItem(itemId);

	if (pItem)
	{
		ScheduleItemSwitch(itemId, keepHistory, 0, forceFastSelect);
	}
}

//------------------------------------------------------------------------
void CActor::SelectItem(EntityId itemId, bool keepHistory, bool forceSelect)
{
	if (IItem * pItem = m_pItemSystem->GetItem(itemId))
	{
		if (!m_pInventory)
			return;

		if (m_pInventory->GetCount() < 1)
			return;

		if (m_pInventory->FindItem(itemId) < 0)
		{
			GameWarning("Trying to select an item which is not in %s's inventory!", GetEntity()->GetName());
			return;
		}

		if (pItem->GetEntityId() == m_pInventory->GetCurrentItem() && !forceSelect)	
			return;

		if(pItem->GetEntityId() == m_pInventory->GetHolsteredItem()) //unholster selected weapon
			m_pInventory->HolsterItem(false);

		m_pItemSystem->SetActorItem(this, pItem->GetEntityId(), keepHistory);
	}
}

//------------------------------------------------------------------------
EntityId CActor::GetLeftHandObject() const
{
	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	if (pCharacter)
	{
		IAttachmentManager* pAttachMan = pCharacter->GetIAttachmentManager();
		IAttachment* pLeftAttachment = pAttachMan->GetInterfaceByName(g_pItemStrings->left_item_attachment.c_str());
		if (pLeftAttachment && pLeftAttachment->GetIAttachmentObject() && (pLeftAttachment->GetIAttachmentObject()->GetAttachmentType() == IAttachmentObject::eAttachment_Entity))
		{
			CEntityAttachment *pEntityAttachment = (CEntityAttachment *) pLeftAttachment->GetIAttachmentObject();
			return pEntityAttachment->GetEntityId();
		}
	}

	return 0;
}

void CActor::HideLeftHandObject(bool inHide)
{
	EntityId leftHandEnt = GetLeftHandObject();
	if (leftHandEnt)
	{
		IEntity *pEnt = gEnv->pEntitySystem->GetEntity(leftHandEnt);
		if (pEnt)
		{
			pEnt->Hide(inHide);
		}
	}
}

//------------------------------------------------------------------------
void CActor::HolsterItem(bool holster, bool playSelect, float selectSpeedBias, bool hideLeftHandObject)
{
	if (!m_pInventory)
		return;

	if(holster)
	{
		if(hideLeftHandObject)
		{	
			if(!GetGrabbedEntityId())
			{
				HideLeftHandObject(true);
			}
			else
			{
				HolsterItem(true,true,1.0f,false);
			}
		}
	}
	else
	{
		HideLeftHandObject(false);
	}

	if(holster)
	{
		SActorStats* pActorStats = GetActorStats();

		//if interrupting a scheduled switch select the new item immediately to ensure correct selection upon unholster 
		if (pActorStats && pActorStats->exchangeItemStats.switchingToItemID != 0)
		{
			IEntity* pSwitchEntity = gEnv->pEntitySystem->GetEntity(pActorStats->exchangeItemStats.switchingToItemID);

			if(pSwitchEntity && pSwitchEntity->GetClass() != CItem::sBinocularsClass)
			{
				SelectItem(pActorStats->exchangeItemStats.switchingToItemID, pActorStats->exchangeItemStats.keepHistory, false);
			}

			CancelScheduledSwitch();
		}
	}

	if (!holster)
	{
		CItem* pHolsteredItem = GetItem(m_pInventory->GetHolsteredItem());
		if (pHolsteredItem)
			pHolsteredItem->Unholstering(playSelect, selectSpeedBias);
	};

	m_pInventory->HolsterItem(holster);

	if(holster && (GetEntityId() == g_pGame->GetIGameFramework()->GetClientActorId()))
	{
		SHUDEventWrapper::FireModeChanged(NULL, 0);
	}
}

//------------------------------------------------------------------------
bool CActor::UseItem(EntityId itemId)
{
	if (m_health.IsDead())
		return false;

	CItem *pItem=GetItem(itemId);
	bool canBeUsed = pItem != NULL;
	
	CryLog ("UseItem: %s %s wants to use %s %s %s", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), pItem ? pItem->GetEntity()->GetClass()->GetName() : "NULL", pItem ? pItem->GetEntity()->GetName() : "item", canBeUsed ? "which is fine!" : "but it's not allowed!");

	if (!canBeUsed)
		return false;

	CancelScheduledSwitch();

	if (gEnv->bServer || (pItem->GetEntity()->GetFlags()&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY)))
		pItem->Use(GetEntityId());
	else
	{
		if(!m_bAwaitingServerUseResponse)
		{
			GetGameObject()->InvokeRMI(SvRequestUseItem(), ItemIdParam(itemId), eRMI_ToServer);
			SetStillWaitingOnServerUseResponse(true);
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CActor::PickUpItem(EntityId itemId, bool sound, bool select)
{
	CItem *pItem = static_cast<CItem*>(m_pItemSystem->GetItem(itemId));
	if (!pItem || m_health.IsDead())
		return false;

	if (gEnv->bServer || (pItem->GetEntity()->GetFlags()&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY)))
	{
		pItem->PickUp(GetEntityId(), true, select);
		m_pGameplayRecorder->Event(GetEntity(), GameplayEvent(eGE_ItemPickedUp, 0, 0, (void *)(EXPAND_PTR)pItem->GetEntityId()));
	}
	else 
	{
		if(!m_bAwaitingServerUseResponse)
		{
			GetGameObject()->InvokeRMI(SvRequestPickUpItem(), ItemIdParam(itemId, false, select), eRMI_ToServer);
			SetStillWaitingOnServerUseResponse(true);
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CActor::PickUpItemAmmo(EntityId itemId)
{
	CItem *pItem = static_cast<CItem*>(m_pItemSystem->GetItem(itemId));
	if (!pItem || m_health.IsDead())
		return false;

	if (gEnv->bServer || (pItem->GetEntity()->GetFlags()&(ENTITY_FLAG_CLIENT_ONLY|ENTITY_FLAG_SERVER_ONLY)))
	{
		pItem->PickUpAmmo(GetEntityId());

		CWeapon* pCurrWeapon = GetWeapon(GetCurrentItemId(false));
		if(pCurrWeapon && pCurrWeapon->OutOfAmmo(false) && pCurrWeapon->CanReload())
		{
			pCurrWeapon->Reload();
		}

		//m_pGameplayRecorder->Event(GetEntity(), GameplayEvent(eGE_ItemPickedUp, 0, 0, (void *)pItem->GetEntityId()));
	}
	else
	{
		if(!m_bAwaitingServerUseResponse)
		{
			GetGameObject()->InvokeRMI(SvRequestPickUpItem(), ItemIdParam(itemId, true), eRMI_ToServer);
			SetStillWaitingOnServerUseResponse(true);
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CActor::DropItem(EntityId itemId, float impulseScale, bool selectNext, bool bydeath)
{
	CItem *pItem = static_cast<CItem*>(m_pItemSystem->GetItem(itemId));
	if (!pItem)
		return false;

	//Fix editor reseting issue
	//Player dies - Don't drop weapon
	//pItem->ShouldNotDrop() is only true when leaving the game mode into the editor (see EVENT_RESET in Item.cpp)
	if(IsClient() && gEnv->IsEditor() && (m_health.IsDead()||pItem->ShouldNotDrop()))
	{
		return false;
	}

	bool bOK = false;
	if (pItem->CanDrop())
	{
		bool performCloakFade = IsCloaked();

		if (gEnv->bServer)
		{
			pItem->Drop(impulseScale, selectNext, bydeath);

			if (!bydeath)
			{
				// send game event
				// check also if the dropped item was actually the slave (akimbo guns)
				m_pGameplayRecorder->Event(GetEntity(), GameplayEvent(eGE_ItemDropped, 0, 0, (void *)(EXPAND_PTR)(itemId)));
			}
		}
		else
		{
			m_pendingDropEntityId = itemId;
			GetGameObject()->InvokeRMI(SvRequestDropItem(), DropItemParams(itemId, selectNext, bydeath), eRMI_ToServer);
		}

		if (performCloakFade)
		{
			pItem->CloakEnable(false, true);
		}

		bOK = true;
	}
	return bOK;
}

void CActor::ServerExchangeItem(CItem* pCurrentItem, CItem* pNewItem)
{
	if (pCurrentItem && pNewItem)
	{
		if (pNewItem->GetOwnerId() != 0)
		{
			CryLog("%s tried to exchange %s for %s, but the new item already has an owner %d", GetEntity()->GetName(), pCurrentItem->GetEntity()->GetName(), pNewItem->GetEntity()->GetName(), pNewItem->GetOwnerId());
			return;
		}

		IItemSystem* pItemSystem = gEnv->pGameFramework->GetIItemSystem();
		CWeapon* pCurrentWeapon = static_cast<CWeapon*>(pCurrentItem->GetIWeapon());
		CWeapon* pNewWeapon = static_cast<CWeapon*>(pNewItem->GetIWeapon());
		IInventory* pInventory = GetInventory();

		DropItem(pCurrentItem->GetEntityId(), 1.0f, false, false);
		PickUpItem(pNewItem->GetEntityId(), true, true);

		if (pCurrentWeapon && pNewWeapon)
		{
			const CWeaponSharedParams* pCurrentWeaponParams = pCurrentWeapon->GetWeaponSharedParams();
			const CWeaponSharedParams* pNewWeaponParams = pNewWeapon->GetWeaponSharedParams();
			for (size_t i = 0; i < pCurrentWeaponParams->ammoParams.ammo.size(); ++i)
			{
				IEntityClass* pAmmoType = pCurrentWeaponParams->ammoParams.ammo[i].pAmmoClass;
				int bonusAmmo = pCurrentWeapon->GetBonusAmmoCount(pAmmoType);

				for (size_t j = 0; j < pNewWeaponParams->ammoParams.ammo.size(); ++j)
				{
					if (pNewWeaponParams->ammoParams.ammo[j].pAmmoClass != pAmmoType)
						continue;

					int currentCapacity = pInventory->GetAmmoCapacity(pAmmoType);
					int currentAmmo = pInventory->GetAmmoCount(pAmmoType);
					int newIventoryAmmount = min(currentAmmo + bonusAmmo, currentCapacity);
					int newDropAmmount = max((currentAmmo + bonusAmmo) - currentCapacity, 0);
					pInventory->SetAmmoCount(pAmmoType, newIventoryAmmount);
					pCurrentWeapon->SetBonusAmmoCount(pAmmoType, newDropAmmount);

					break;
				}
			}
		}
	}
}

//---------------------------------------------------------------------
void CActor::ExchangeItem(CItem* pCurrentItem, CItem* pNewItem)
{
	m_telemetry.OnExchangeItem(pCurrentItem, pNewItem);
	if(gEnv->bServer)
	{
		ServerExchangeItem(pCurrentItem, pNewItem);
	}
	else if(!m_bAwaitingServerUseResponse)
	{
		GetGameObject()->InvokeRMI(SvRequestExchangeItem(), ExchangeItemParams(pCurrentItem->GetEntityId(), pNewItem->GetEntityId()), eRMI_ToServer);
		SetStillWaitingOnServerUseResponse(true);
	}
}

//---------------------------------------------------------------------
void CActor::DropAttachedItems()
{
	//Drop weapons attached to the back
	if (gEnv->bServer)
	{
		const int maxNumItems = 16;
		EntityId itemsTodrop[maxNumItems];
		int numItems = 0;
		int totalItems = m_pInventory->GetCount();

		for (int i=0; i < totalItems; i++)
		{
			EntityId itemId = m_pInventory->GetItem(i);
			CItem* pItem = (CItem *)m_pItemSystem->GetItem(itemId);
			if (pItem)
			{
				pItem->OnOwnerDeactivated();

				if (pItem->CanDrop())
				{
					itemsTodrop[numItems++] = itemId;
					if (numItems >= maxNumItems)
						break;
				}
			}
		}

		for (int i = 0; i < numItems; i++)
		{
			EntityId entId = itemsTodrop[i];
			CItem* item = (CItem *)m_pItemSystem->GetItem(entId);
			item->Drop(1.0f,false,true);
			m_pGameplayRecorder->Event(GetEntity(), GameplayEvent(eGE_ItemDropped, 0, 0, (void *)(EXPAND_PTR)entId));
		}
	}
}

//------------------------------------------------------------------------
IItem *CActor::GetCurrentItem(bool includeVehicle/*=false*/) const
{
  if (EntityId itemId = GetCurrentItemId(includeVehicle))
	  return m_pItemSystem->GetItem(itemId);

  return 0;
}

//------------------------------------------------------------------------
EntityId CActor::GetCurrentItemId(bool includeVehicle/*=false*/) const
{
	if (includeVehicle)
	{
		if (IVehicle* pVehicle = GetLinkedVehicle())
		{
			if (EntityId itemId = pVehicle->GetCurrentWeaponId(GetEntityId()))
				return itemId;
		}
	}
  
	if (m_pInventory)
		return m_pInventory->GetCurrentItem();

	return 0;
}

//------------------------------------------------------------------------
IItem *CActor::GetHolsteredItem() const
{
	if (m_pInventory)
	{
		return m_pItemSystem->GetItem(m_pInventory->GetHolsteredItem());
	}

	return NULL;
}

//------------------------------------------------------------------------
EntityId CActor::GetHolsteredItemId() const
{
	if (m_pInventory)
	{
		return m_pInventory->GetHolsteredItem();
	}

	return 0;
}

//------------------------------------------------------------------------
IInventory* CActor::GetInventory() const
{
	return m_pInventory;
}

//------------------------------------------------------------------------
EntityId CActor::NetGetCurrentItem() const
{
	if (m_pInventory)
		return m_pInventory->GetCurrentItem();

	return 0;
}

//------------------------------------------------------------------------
void CActor::NetSetCurrentItem(EntityId id, bool forceDeselect)
{	
	if (id == 0 || forceDeselect)
	{
		if(!GetGrabbedEntityId())
		{
			HolsterItem(true);
		}
		else
		{
			HolsterItem(true,true,1.0f,false); 
		}
	}
	else
	{
		if (GetHolsteredItemId() == id)
		{
			HolsterItem(false);
		}
		else
		{
			const CWeapon* pWeapon = GetWeapon(id);
			const bool force = id == GetCurrentItemId(false) && pWeapon && pWeapon->IsDeselecting();

			static const IEntityClass* pPickAndThrowWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("PickAndThrowWeapon");
			const bool keepHistory = pWeapon && pWeapon->GetEntity()->GetClass() == pPickAndThrowWeaponClass;

			SelectItem(id, keepHistory, force);
		}
	}
}

//------------------------------------------------------------------------
EntityId CActor::NetGetScheduledItem() const
{
	return GetActorStats()->exchangeItemStats.switchingToItemID;
}

//------------------------------------------------------------------------
void CActor::NetSetScheduledItem(EntityId id)
{	
	
	SActorStats* pStats = GetActorStats();

	if(id && id != pStats->exchangeItemStats.switchingToItemID)
	{
		CItem* pCurrentItem = static_cast<CItem*>(GetCurrentItem());
		if(pCurrentItem)
		{
			bool fastDeselect = pCurrentItem->HasFastSelect(id);
			pCurrentItem->StartDeselection(fastDeselect);
		}	
	}

	pStats->exchangeItemStats.switchingToItemID = id;
}


void CActor::PostUpdate(float frameTime)
{

}

bool CActor::IsInMercyTime() const
{
	if (m_isPlayer == false)
	{
		return false;
	}
	else
	{
		bool isInMercyTime = false;
		const IAIObject* pAiObject = GetEntity()->GetAI();
		if (pAiObject != NULL)
		{
			const IAIActor* pAiActor = pAiObject->CastToIAIActor();
			CRY_ASSERT(pAiActor);
			isInMercyTime = pAiActor->IsLowHealthPauseActive();
		}

		return isInMercyTime;
	}
}


float CActor::GetBodyDamageMultiplier(const HitInfo &hitInfo) const
{
	float fMultiplier = 1.0f;

	const TBodyDamageProfileId bodyDamageProfileId = GetCurrentBodyDamageProfileId();
	if (bodyDamageProfileId != INVALID_BODYDAMAGEPROFILEID)
	{
		CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
		CRY_ASSERT(pBodyDamageManager);

		fMultiplier = pBodyDamageManager->GetDamageMultiplier(bodyDamageProfileId, *GetEntity(), hitInfo);
	}

	return fMultiplier;
}

float CActor::GetBodyExplosionDamageMultiplier(const HitInfo &hitInfo) const
{
	float fMultiplier = 1.0f;

	const TBodyDamageProfileId bodyDamageProfileId = GetCurrentBodyDamageProfileId();
	if (bodyDamageProfileId != INVALID_BODYDAMAGEPROFILEID)
	{
		CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
		CRY_ASSERT(pBodyDamageManager);

		fMultiplier = pBodyDamageManager->GetExplosionDamageMultiplier(bodyDamageProfileId, *GetEntity(), hitInfo);
	}

	return fMultiplier;
}

// ===========================================================================
//	Get the body damage part flags via a part ID.
//
//	In:		The part ID.
//	In:		The material ID.
//
//	Returns:	The corresponding body damage part flags or 0 if it could not 
//				be obtained (e.g.: the part ID was invalid).
//
uint32 CActor::GetBodyDamagePartFlags(const int partID, const int materialID) const
{
	const TBodyDamageProfileId bodyDamageProfileId = GetCurrentBodyDamageProfileId();
	if (bodyDamageProfileId != INVALID_BODYDAMAGEPROFILEID)
	{
		CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
		CRY_ASSERT(pBodyDamageManager != NULL);

		HitInfo hitInfo;
		hitInfo.partId = partID;
		hitInfo.material = materialID;
		return pBodyDamageManager->GetPartFlags(bodyDamageProfileId, *GetEntity(), hitInfo);
	}

	return 0;
}


// ===========================================================================
// Get a body damage profile ID for a combination of body damage file names.
//
//  In:     The body damage file name (NULL will ignore).
//  In:     The body damage parts file name (NULL will ignore).
//
//  Returns:    A default exit code (in Lua: A body damage profile ID or
//              -1 on error).
//
TBodyDamageProfileId CActor::GetBodyDamageProfileID(
	const char* bodyDamageFileName, const char* bodyDamagePartsFileName) const
{
	CBodyDamageManager *bodyDamageManager = g_pGame->GetBodyDamageManager();
	CRY_ASSERT(bodyDamageManager != NULL);

	IEntity* entity = GetEntity();
	assert(entity != NULL);
	return bodyDamageManager->GetBodyDamage(*entity, bodyDamageFileName, bodyDamagePartsFileName);
}


// ===========================================================================
// Override the body damage profile ID.
//
//  In:     The ID of the damage profile that should become active or
//          INVALID_BODYDAMAGEPROFILEID if the default one should be used).
//
void CActor::OverrideBodyDamageProfileID(const TBodyDamageProfileId profileID)
{
	CBodyDamageManager *bodyDamageManager = g_pGame->GetBodyDamageManager();
	CRY_ASSERT(bodyDamageManager != NULL);

	if (m_OverrideBodyDamageProfileId != profileID)
	{
		m_OverrideBodyDamageProfileId = profileID;
		PhysicalizeBodyDamage();
	}
}

bool CActor::IsHeadShot(const HitInfo &hitInfo) const
{
	return IsBodyDamageFlag(hitInfo, eBodyDamage_PID_Headshot);
}

bool CActor::IsHelmetShot(const HitInfo & hitInfo) const
{
	return IsBodyDamageFlag(hitInfo, eBodyDamage_PID_Helmet);
}

bool CActor::IsGroinShot(const HitInfo &hitInfo) const
{
	return IsBodyDamageFlag(hitInfo, eBodyDamage_PID_Groin);
}

bool CActor::IsFootShot(const HitInfo &hitInfo) const
{
	return IsBodyDamageFlag(hitInfo, eBodyDamage_PID_Foot);
}

bool CActor::IsKneeShot(const HitInfo &hitInfo) const
{
	return IsBodyDamageFlag(hitInfo, eBodyDamage_PID_Knee);
}

bool CActor::IsWeakSpotShot(const HitInfo &hitInfo) const
{
	return IsBodyDamageFlag(hitInfo, eBodyDamage_PID_WeakSpot);
}


bool CActor::IsBodyDamageFlag(const HitInfo &hitInfo, const EBodyDamagePIDFlags flag) const
{
	bool bResult = false;

	const TBodyDamageProfileId bodyDamageProfileId = GetCurrentBodyDamageProfileId();
	if (bodyDamageProfileId != INVALID_BODYDAMAGEPROFILEID)
	{
		CBodyDamageManager *pBodyDamageManager = g_pGame->GetBodyDamageManager();
		assert(pBodyDamageManager);

		bResult = ((pBodyDamageManager->GetPartFlags( bodyDamageProfileId, *GetEntity(), hitInfo ) & flag) == flag);
	}

	return bResult;
}

void CActor::ProcessDestructiblesHit( const HitInfo& hitInfo, const float previousHealth, const float newHealth )
{
	if (m_bodyDestructionInstance.GetProfileId() != INVALID_BODYDAMAGEPROFILEID)
	{
		g_pGame->GetBodyDamageManager()->ProcessDestructiblesHit(*GetEntity(), m_bodyDestructionInstance, hitInfo, previousHealth, newHealth);
	}
}

void CActor::ProcessDestructiblesOnExplosion( const HitInfo& hitInfo, const float previousHealth, const float newHealth )
{
	if (m_bodyDestructionInstance.GetProfileId() != INVALID_BODYDAMAGEPROFILEID)
	{
		g_pGame->GetBodyDamageManager()->ProcessDestructiblesOnExplosion(*GetEntity(), m_bodyDestructionInstance, hitInfo, previousHealth, newHealth);
	}
}

//------------------------------------------------------------------------

void CActor::SerializeLevelToLevel( TSerialize &ser )
{
	m_pInventory->SerializeInventoryForLevelChange(ser);
	if(ser.IsReading() && IsClient())
	{
		SHUDEvent	hudEvent_initLocalPlayer(eHUDEvent_OnInitLocalPlayer);
		hudEvent_initLocalPlayer.AddData(static_cast<int>(GetEntityId()));
		CHUDEventDispatcher::CallEvent(hudEvent_initLocalPlayer);
	}
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, SvRequestDropItem)
{
	IItem *pItem = GetItem(params.itemId);
	if (!pItem)
	{
		GameWarning("[gamenet] Failed to drop item. Item not found!");
		return false;
	}

	//CryLogAlways("%s::SvRequestDropItem(%s)", GetEntity()->GetName(), pItem->GetEntity()->GetName());

	pItem->Drop(1.0f, params.selectNext, params.byDeath);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, SvRequestPickUpItem)
{
	bool bProcessedRequest = false;
	CItem *pItem = static_cast<CItem*>(GetItem(params.itemId));

	if (!pItem)
	{
		// this may occur if the item has been deleted but the client has not yet been informed
		GameWarning("[gamenet] Failed to pickup item. Item not found!");
		bProcessedRequest = true;
	}

	if(!bProcessedRequest)
	{
		if (m_health.IsDead())
		{
			bProcessedRequest = true;
		}

		if(!bProcessedRequest)
		{
			EntityId ownerId = pItem->GetOwnerId();
			if (!ownerId)
			{
				if (params.pickOnlyAmmo)
				{
					pItem->PickUpAmmo(GetEntityId());

					CWeapon* pCurrWeapon = GetWeapon(GetCurrentItemId(false));
					if(pCurrWeapon && pCurrWeapon->OutOfAmmo(false) && pCurrWeapon->CanReload())
					{
						pCurrWeapon->Reload();
						pCurrWeapon->RequestReload(); //Required to propagate out to clients
					}
				}
				else
				{	
					pItem->PickUp(GetEntityId(), true, params.select);
				}
			}
		}
	}

	// Tell the client their use request has now been processed
	if(!IsClient())
	{
		GetGameObject()->InvokeRMI(CActor::ClUseRequestProcessed(), CActor::NoParams(), eRMI_ToClientChannel, GetChannelId());
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, SvRequestExchangeItem)
{
	CItem* pDropItem = GetItem(params.dropItemId);
	CItem* pPickupItem = GetItem(params.pickUpItemId);

	ServerExchangeItem(pDropItem, pPickupItem);

	// Tell the client their use request has now been processed
	if(!IsClient())
	{
		GetGameObject()->InvokeRMI(CActor::ClUseRequestProcessed(), CActor::NoParams(), eRMI_ToClientChannel, GetChannelId());
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, SvRequestUseItem)
{
	UseItem(params.itemId);

	// Tell the client their use request has now been processed
	if(!IsClient())
	{
		GetGameObject()->InvokeRMI(CActor::ClUseRequestProcessed(), CActor::NoParams(), eRMI_ToClientChannel, GetChannelId());
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClUseRequestProcessed)
{
	CRY_ASSERT(m_bAwaitingServerUseResponse);

	SetStillWaitingOnServerUseResponse(false);

	return true;
}

//------------------------------------------------------------------------
CItem * CActor::StartRevive(int teamId)
{
	// stop using any mounted weapons before reviving
	CItem *pItem=static_cast<CItem *>(GetCurrentItem());
	if (pItem)
	{
		if (pItem->IsMounted())
		{
			pItem->StopUse(GetEntityId());
			pItem=0;
		}
	}

	if (!IsMigrating())
	{
		SetHealth(GetMaxHealth());
	}

	m_teamId = teamId;

	return pItem;
}

//------------------------------------------------------------------------
void CActor::FinishRevive(CItem * pItem)
{
	// This will cover the case when the ClPickup RMI comes in before we're revived
	if (m_netLastSelectablePickedUp)
	{
		pItem=static_cast<CItem *>(m_pItemSystem->GetItem(m_netLastSelectablePickedUp));
	}

	m_netLastSelectablePickedUp=0;

	if (pItem)
	{
		bool soundEnabled=pItem->IsSoundEnabled();
		pItem->EnableSound(false);
		pItem->Select(false);
		pItem->EnableSound(soundEnabled);

		m_pItemSystem->SetActorItem(this, (EntityId)0);
		SelectItem(pItem->GetEntityId(), true, false);
	}

	if (IsClient())
	{
		SupressViewBlending(); // no view blending when respawning // CActor::Revive resets it.

		if (gEnv->bMultiplayer)
		{
			if (CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout())
			{
				pEquipmentLoadout->ClAssignLastSentEquipmentLoadout(this);
			}
		}
	}
}

//------------------------------------------------------------------------
void CActor::NetReviveAt(const Vec3 &pos, const Quat &rot, int teamId, uint8 modelIndex)
{
	CryLog("CActor::NetReviveAt: %s", GetEntity()->GetName());
	INDENT_LOG_DURING_SCOPE();

#if ENABLE_STATOSCOPE
	if(gEnv->pStatoscope)
	{
		CryFixedStringT<128> buffer;
		buffer.Format("CActor::NetReviveAt: %s", GetEntity()->GetName());
		gEnv->pStatoscope->AddUserMarker("Actor", buffer.c_str());
	}
#endif // ENABLE_STATOSCOPE

	if (IVehicle *pVehicle=GetLinkedVehicle())
	{
		if (IVehicleSeat *pSeat=pVehicle->GetSeatForPassenger(GetEntityId()))
			pSeat->Exit(false);
	}

	CItem * currentItem = StartRevive(teamId);

	CGameRules * pGameRules = g_pGame->GetGameRules();

	pGameRules->OnRevive(this);

	GetEntity()->SetWorldTM(Matrix34::Create(Vec3(1,1,1), rot, pos));

	SetModelIndex(modelIndex);
	Revive(kRFR_Spawn);

	BATTLECHATTER(BC_Spawn, GetEntityId());

	pGameRules->EntityRevived_NotifyListeners(GetEntityId());

	if(gEnv->bServer)
	{
		m_netPhysCounter = (m_netPhysCounter + 1)&(PHYSICS_COUNTER_MAX - 1);
	}

	FinishRevive(currentItem);

	if (IsClient())
	{
		g_pGame->GetGameRules()->OnLocalPlayerRevived();
	}

	CActorManager::GetActorManager()->ActorRevived(this);
}

//------------------------------------------------------------------------
void CActor::NetReviveInVehicle(EntityId vehicleId, int seatId, int teamId)
{
	CItem * currentItem = StartRevive(teamId);
	
	g_pGame->GetGameRules()->OnReviveInVehicle(this, vehicleId, seatId, m_teamId);

	Revive(kRFR_Spawn);

	// fix our physicalization, since it's need for some vehicle stuff, and it will be set correctly before the end of the frame
	// make sure we are alive, for when we transition from ragdoll to linked...
	if (!GetEntity()->GetPhysics() || GetEntity()->GetPhysics()->GetType()!=PE_LIVING)
		Physicalize();

	IVehicle *pVehicle=m_pGameFramework->GetIVehicleSystem()->GetVehicle(vehicleId);
	assert(pVehicle);
	if(pVehicle)
	{
		IVehicleSeat *pSeat=pVehicle->GetSeatById(seatId);
		if (pSeat && (!pSeat->GetPassenger() || pSeat->GetPassenger()==GetEntityId()))
			pSeat->Enter(GetEntityId(), false);
	}

	FinishRevive(currentItem);
}

void CActor::FillHitInfoFromKillParams(const CActor::KillParams& killParams, HitInfo &hitInfo) const
{
	hitInfo.type = killParams.hit_type;
	hitInfo.damage = killParams.damage;
	hitInfo.partId = killParams.hit_joint;
	hitInfo.dir = killParams.dir;
	hitInfo.material = killParams.material;
	hitInfo.projectileId = killParams.projectileId;
	hitInfo.shooterId = killParams.shooterId;
	hitInfo.weaponClassId = killParams.weaponClassId;
	hitInfo.projectileClassId = killParams.projectileClassId;
	hitInfo.targetId = killParams.targetId;
	hitInfo.weaponId = killParams.weaponId;
	hitInfo.penetrationCount = killParams.penetration;
	hitInfo.hitViaProxy = killParams.killViaProxy;
	hitInfo.impulseScale			= killParams.impulseScale; 
	hitInfo.forceLocalKill = killParams.forceLocalKill; 

	// Get some needed parameters on the HitInfo structure
	// This means no DeathReaction has been processed
	char projectileClassName[128];
	if (g_pGame->GetIGameFramework()->GetNetworkSafeClassName(projectileClassName, sizeof(projectileClassName), killParams.projectileClassId))
	{
		IEntityClass* pProjectileClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(projectileClassName);
		const SAmmoParams *pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(pProjectileClass);
		hitInfo.bulletType = pAmmoParams ? pAmmoParams->bulletType : 1;
	}

	ICharacterInstance* pCharacter = NULL;
	if ((hitInfo.partId != uint16(-1)) && (pCharacter = GetEntity()->GetCharacter(0)))
	{
		const int FIRST_ATTACHMENT_PARTID = 1000;

		// perhaps it's an attachment?
		if (hitInfo.partId < FIRST_ATTACHMENT_PARTID)
		{
			hitInfo.pos = GetEntity()->GetWorldTM().TransformPoint(pCharacter->GetISkeletonPose()->GetAbsJointByID(hitInfo.partId).t);
		}
		else
		{
			IAttachmentManager* pAttchmentManager = pCharacter->GetIAttachmentManager();
			IAttachment* pAttachment = pAttchmentManager->GetInterfaceByIndex(hitInfo.partId - FIRST_ATTACHMENT_PARTID);
			if (pAttachment)
			{
				hitInfo.pos = pAttachment->GetAttWorldAbsolute().t;
			}
		}
	}
}

//------------------------------------------------------------------------
void CActor::NetKill(const KillParams &killParams)
{
	HitInfo hitInfo;
	FillHitInfoFromKillParams(killParams, hitInfo);

#if ENABLE_STATOSCOPE
	if(gEnv->pStatoscope)
	{
		const char* shooter = "unknown";
		const char* target = "unknown";
		if (gEnv->pEntitySystem)
		{
			IEntity* pShooter = gEnv->pEntitySystem->GetEntity(killParams.shooterId);
			if (pShooter)
			{
				shooter = pShooter->GetName();
			}
			IEntity* pTarget = gEnv->pEntitySystem->GetEntity(killParams.targetId);
			if (pTarget)
			{
				target = pTarget->GetName();
			}
		}

		CryFixedStringT<128> buffer;
		buffer.Format("CActor::NetKill: %s killed %s", shooter, target);
		gEnv->pStatoscope->AddUserMarker("Actor", buffer.c_str());
		CryLog("%s", buffer.c_str());
	}
#endif // ENABLE_STATOSCOPE

	if (killParams.ragdoll)
	{
		CProceduralContextRagdoll* pRagdollContext = NULL;
		if( GetRagdollContext( &pRagdollContext ) )
		{
			pRagdollContext->SetEntityTarget( killParams.targetId );
		}

		GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);

		// If we have no specific impulse override fallback to using old default impulse method
		if(killParams.impulseScale <= 0.0f && !gEnv->bMultiplayer && (killParams.hit_type == CGameRules::EHitType::Collision) )
		{
			pe_action_impulse impulse;
			impulse.partid = hitInfo.partId;
			impulse.point = hitInfo.pos;
			impulse.impulse = 1000.f*hitInfo.dir;
			m_pImpulseHandler->SetOnRagdollPhysicalizedImpulse(impulse);
		}
	}

	g_pGame->GetGameRules()->OnKill(this, hitInfo, killParams.winningKill, killParams.firstKill, killParams.bulletTimeReplay);

	m_netLastSelectablePickedUp=0;

	Kill();

	// Once killed, if override impulse specified and if we werent the killer(who has already applied an impulse), force ragdoll and apply.
	// killParams.ragdoll will only be true if no hit death reaction is currently active. 
	if(killParams.ragdoll && killParams.impulseScale > 0.0f && (hitInfo.shooterId != gEnv->pGameFramework->GetClientActorId()))
	{
		ForceRagdollizeAndApplyImpulse(hitInfo); 
	}

	UnRegisterInAutoAimManager();
	UnRegisterDBAGroups();

	g_pGame->GetGameRules()->OnKillMessage(GetEntityId(), killParams.shooterId);
}

//------------------------------------------------------------------------
void CActor::ForceRagdollizeAndApplyImpulse(const HitInfo& hitInfo)
{
	IEntity* pEntity = GetEntity(); 
	if(IsDead() && pEntity && hitInfo.partId)
	{
		pe_action_impulse peImpulse;
		peImpulse.impulse    = (hitInfo.dir * hitInfo.impulseScale);
		peImpulse.point      = hitInfo.pos;
		peImpulse.partid     = hitInfo.partId;
		peImpulse.iApplyTime = 0; // Apply immediately

		RagDollize(false);
		IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
		m_pImpulseHandler->SetOnRagdollPhysicalizedImpulse( peImpulse );

		CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			pRecordingSystem->OnForcedRagdollAndImpulse( pEntity->GetId(), peImpulse.impulse, hitInfo.pos, hitInfo.partId, 0 );
		}
	}
}

//------------------------------------------------------------------------
void CActor::NetSimpleKill()
{
	CryLog("CActor::NetSimpleKill() '%s", GetEntity()->GetName());
	if (gEnv->bMultiplayer && IsPlayer())
	{
		if (!IsThirdPerson() && g_pGameCVars->pl_switchTPOnKill)
		{
			ToggleThirdPerson();
		}
	}

	Kill();
}

//-----------------------------------------------------------------------
bool CActor::CanRagDollize() const
{
	const SActorStats *pStats = GetActorStats();
	if(!gEnv->bMultiplayer && !m_isClient && pStats && pStats->isGrabbed)
		return false;

	return true;

}
//------------------------------------------------------------------------
void CActor::GetMemoryUsage(ICrySizer * s) const
{
	s->AddObject(this, sizeof(*this));
	GetInternalMemoryUsage(s);
	
}

void CActor::GetInternalMemoryUsage(ICrySizer * s) const
{
	s->AddObject(m_pInventory);
	s->AddObject(m_pAnimatedCharacter);	
	s->AddObject(m_pMovementController);
	s->AddContainer(m_IKLimbs);	

}
//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClRevive)
{
	IGameRulesSpawningModule *pSpawningModule = g_pGame->GetGameRules()->GetSpawningModule();
	EntityId spawnEntity = pSpawningModule->GetNthSpawnLocation(params.spawnPointIdx); 
	IEntity *pSpawnPoint = gEnv->pEntitySystem->GetEntity(spawnEntity);
	
	if (pSpawnPoint == NULL)
	{
		// If we don't know about this entity, something has gone wrong with the spawn point indexing.
		// Returning false will cause a disconnect but it's better than a crash!
		CryLog("CActor::ClRevive() Unable to find spawn point index %d (num known spawns=%d), disconnecting", params.spawnPointIdx, pSpawningModule->GetSpawnLocationCount());
		return false;
	}

	pSpawningModule->SetLastSpawn(GetEntityId(), pSpawnPoint->GetId());

	Vec3 pos = pSpawnPoint->GetWorldPos();
	Quat rot = Quat::CreateRotationXYZ(pSpawnPoint->GetWorldAngles());

	CryLog("CActor::ClRevive(), Actor: '%s' - Spawning at EntityID: %d, '%s', class '%s', position: %.2f %.2f %.2f\n",
					GetEntity()->GetName(), pSpawnPoint->GetId(), pSpawnPoint->GetName(), pSpawnPoint->GetClass()->GetName(), pos.x, pos.y, pos.z);

	NetReviveAt(pos, rot, params.teamId, params.modelIndex);

	IScriptTable *pEntityScript = pSpawnPoint->GetScriptTable();
	if (pEntityScript)
	{
		HSCRIPTFUNCTION spawnedFunc(NULL);
		if (GetEntity()->GetScriptTable() && pEntityScript->GetValue("Spawned", spawnedFunc))
		{
			Script::Call(gEnv->pScriptSystem, spawnedFunc, pEntityScript, GetEntity()->GetScriptTable());
		}
	}
	
	m_netPhysCounter = params.physCounter;

	if (IsClient())
	{
		if (g_pGame->GetRecordingSystem())
		{
			g_pGame->GetRecordingSystem()->StartRecording();
		}
				
		g_pGame->GetUI()->ActivateDefaultState();

		if (GetSpectatorState()==CActor::eASS_None)
		{
			SetSpectatorState(CActor::eASS_Ingame);
		}

		if(IPhysicalEntity* pPhysics = GetEntity()->GetPhysics())
		{
			pe_action_awake paa;
			paa.bAwake = 1;
			pPhysics->Action(&paa);

			pe_player_dynamics pd;
			pd.gravity.z = -9.81f;
			pPhysics->SetParams(&pd);
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClKill)
{
#if USE_LAGOMETER
	CLagOMeter* pLagOMeter = g_pGame->GetLagOMeter();
	if (pLagOMeter)
	{
		pLagOMeter->OnClientReceivedKill(params);
	}
#endif

	if (params.itemIdToDrop)
	{
		CItem *pItem=GetItem(params.itemIdToDrop);
		if (pItem)
		{
			pItem->Drop(1.f, false, true);
		}
	}

	NetKill(params);

	SkillKill::SetFirstBlood(params.firstKill);

	if (CGameRules *pGameRules=g_pGame->GetGameRules())
	{
		CryLog("CActor::ClKill - %d '%s' killed by %d '%s' (damage=%f type=%s)", params.targetId, GetEntity()->GetName(), params.shooterId, pGameRules->GetEntityName(params.shooterId), params.damage, pGameRules->GetHitType(params.hit_type));

		HitInfo dummyHitInfo;
		dummyHitInfo.shooterId = params.shooterId;
		dummyHitInfo.targetId = params.targetId;
		dummyHitInfo.weaponId = params.weaponId;
		dummyHitInfo.projectileId = params.projectileId;
		dummyHitInfo.weaponClassId = params.weaponClassId;
		dummyHitInfo.damage = params.damage;
		dummyHitInfo.material = params.material;
		dummyHitInfo.type = params.hit_type;
		dummyHitInfo.partId = params.hit_joint;
		dummyHitInfo.projectileClassId = params.projectileClassId;
		dummyHitInfo.penetrationCount = params.penetration;
		dummyHitInfo.dir = params.dir;
		dummyHitInfo.weaponId = params.weaponId;
		dummyHitInfo.impulseScale  = params.impulseScale;

		if(IEntity * pKilledEntity = gEnv->pEntitySystem->GetEntity(dummyHitInfo.targetId))
		{
			dummyHitInfo.pos = pKilledEntity->GetWorldPos();
		}
		
		pGameRules->OnEntityKilled(dummyHitInfo);
	}
	else
	{
		GameWarning("CActor::ClKill - %d '%s' killed while no game rules exist!", GetEntityId(), GetEntity()->GetName());
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClKillFPCamData)
{
	CRecordingSystem *crs = g_pGame->GetRecordingSystem();
	CRY_ASSERT(crs);
	if (crs)
	{
		crs->ClProcessKillCamData(this, params);
	}
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, SvKillFPCamData)
{
	CRecordingSystem *crs = g_pGame->GetRecordingSystem();
	CRY_ASSERT(crs);
	if (crs)
	{
		crs->SvProcessKillCamData(this, params);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClSimpleKill)
{
	NetSimpleKill();

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClMoveTo)
{
#ifndef _RELEASE
	Vec3 worldPos = GetEntity()->GetWorldPos();
	CryLog("ClMoveTo: %s '%s' moving from (%.2f %.2f %.2f) to (%.2f %.2f %.2f) while state=%s", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), worldPos.x, worldPos.y, worldPos.z, params.pos.x, params.pos.y, params.pos.z, GetActorStats()->isRagDoll ? ", ragdoll" : "");
#endif

	//teleport
	GetEntity()->SetWorldTM(Matrix34::Create(Vec3(1,1,1), params.rot, params.pos));
	OnTeleported();

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClPickUp)
{
	if (CItem *pItem = GetItem(params.itemId))
	{
		//CryLog("%s::ClPickUp %s", GetEntity()->GetName(), pItem->GetEntity()->GetName());

		if(params.pickOnlyAmmo)
		{
			pItem->PickUpAmmo(GetEntityId());
		}
		else
		{
			pItem->PickUp(GetEntityId(), params.sound, params.select);

			const char *displayName = pItem->GetDisplayName();

			if (params.select)
				m_netLastSelectablePickedUp=params.itemId;
		}
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClClearInventory)
{
	//CryLog("%s::ClClearInventory", GetEntity()->GetName());

	g_pGame->GetGameRules()->ClearInventory(this);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClDrop)
{
	if (params.itemId == m_pendingDropEntityId)
	{
		m_pendingDropEntityId = 0;
	}

	CItem *pItem=GetItem(params.itemId);
	if (pItem)
	{
		pItem->Drop(1.0f, params.selectNext, params.byDeath);
	}
	else if(params.selectNext)
	{
		SelectNextItem(1, false);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClStartUse)
{
	CItem *pItem=GetItem(params.itemId);
	if (pItem)
		pItem->StartUse(GetEntityId()); 

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClStopUse)
{
	CItem *pItem=GetItem(params.itemId);
	if (pItem)
	{
		pItem->StopUse(GetEntityId());

		//This is to guard against a scenario where we StartUse/StopUse item A, resulting in us swapping to
		//	item B in our inventory. Current item is a NetSerialize, StartUse/StopUse is an RMI, so they can arrive
		//	out of order. If they do and the StartUse/StopUse arrive after the NetSerialize, the user is left
		//	without a weapon. This uses the last NetSerialized weapon as the next item to select, if it was not
		//	the weapon we are currently stopping using (e.g. it's out of order) - Rich S
		if(!IsClient() && m_lastNetItemId != params.itemId)
			SelectItem(m_lastNetItemId, false, true);
	}

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CActor, ClAssignWeaponAttachments)
{
	IGameFramework *pGameFramework = g_pGame->GetIGameFramework();
	IItemSystem *pItemSystem = pGameFramework->GetIItemSystem();
	int numAttachments = params.m_attachments.size();

	if( IsClient() )
	{
		if( CEquipmentLoadout* pLoadout = g_pGame->GetEquipmentLoadout() )
		{
			pLoadout->SpawnedWithLoadout( params.m_loadoutIdx );
		}
	}

	CryLog("%s::ClAssignWeaponAttachments numAttachments:%d", GetEntity()->GetName(), numAttachments);
	INDENT_LOG_DURING_SCOPE();

	for(int i=0; i < numAttachments; i++)
	{
		const AttachmentsParams::SWeaponAttachment &data = params.m_attachments[i];
		char className[256];
		if(pGameFramework->GetNetworkSafeClassName(className, sizeof(className), data.m_classId))
		{
			pItemSystem->GiveItem(this, className, false, data.m_default, true, NULL, EEntityFlags(ENTITY_FLAG_CLIENT_ONLY));
		}
	}

	return true;
}

//------------------------------------------------------------------------
const IItemParamsNode* CActor::GetActorParamsNode() const
{
	return g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorParams(GetActorClassName());
}

//------------------------------------------------------------------------
const IItemParamsNode* CActor::GetEntityClassParamsNode() const
{
	return g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorParams(GetEntityClassName());
}

//------------------------------------------------------------------------
bool CActor::IsPoolEntity() const
{
	// [*DavidR | 26/Oct/2010] ToDo: Find a fastest, better way (is getting the bookmarked's entity entityPoolId and the
	// default entity class from there and comparing with the current entity class worth it? probably equally slow)
	return (strcmp(GetEntity()->GetName(), "PoolEntity") == 0);
}

//------------------------------------------------------------------------
void CActor::DumpActorInfo()
{
  IEntity* pEntity = GetEntity();

  CryLog("ActorInfo for %s", pEntity->GetName());
  CryLog("=====================================");
  
  Vec3 entPos(pEntity->GetWorldPos());
  CryLog("Entity Pos: %.f %.f %.f", entPos.x, entPos.y, entPos.z);
  CryLog("Active: %i", pEntity->IsActivatedForUpdates());
  CryLog("Hidden: %i", pEntity->IsHidden());
  CryLog("Invisible: %i", pEntity->IsInvisible());  
  CryLog("Profile: %i", m_currentPhysProfile);
  CryLog("Health: %8.2f", m_health.GetHealth());  
  
  if (IPhysicalEntity* pPhysics = pEntity->GetPhysics())
  { 
    CryLog("Physics type: %i", pPhysics->GetType());
    
    pe_status_pos physicsPos;
    if (pPhysics->GetStatus(&physicsPos))
    {
      CryLog("Physics pos: %.f %.f %.f", physicsPos.pos.x, physicsPos.pos.y, physicsPos.pos.z);
    }

    pe_status_dynamics dyn;
    if (pPhysics->GetStatus(&dyn))
    {   
      CryLog("Mass: %.1f", dyn.mass);
      CryLog("Vel: %.2f %.2f %.2f", dyn.v.x, dyn.v.y, dyn.v.z);
    } 
  }  

  if (IVehicle* pVehicle = GetLinkedVehicle())
  {
    CryLog("Vehicle: %s (destroyed: %i)", pVehicle->GetEntity()->GetName(), pVehicle->IsDestroyed());
    
    IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(GetEntityId());
    CryLog("Seat %i", pSeat ? pSeat->GetSeatId() : 0);
  }

  if (IItem* pItem = GetCurrentItem())
  {
    CryLog("Item: %s", pItem->GetEntity()->GetName());
  }


  CryLog("=====================================");
}

//
//-----------------------------------------------------------------------------
Vec3 CActor::GetWeaponOffsetWithLean(EStance stance, float lean, float peekOver, const Vec3& eyeOffset, const bool useWhileLeanedOffsets) const
{
	//for player just do it the old way - from stance info
	if(IsPlayer())
	{
		return GetStanceInfo(stance)->GetWeaponOffsetWithLean(lean, peekOver);
	}
	else
	{
		return GetWeaponOffsetWithLeanForAI(GetWeapon(GetCurrentItemId()), stance, lean, peekOver, eyeOffset, useWhileLeanedOffsets);
	}
}

//-----------------------------------------------------------------------------
Vec3 CActor::GetWeaponOffsetWithLean(CWeapon* pCurrentWeapon, EStance stance, float lean, float peekOver, const Vec3& eyeOffset, const bool useWhileLeanedOffsets) const
{
	//for player just do it the old way - from stance info
	if(IsPlayer())
	{
		return GetStanceInfo(stance)->GetWeaponOffsetWithLean(lean, peekOver);
	}
	else
	{
		return GetWeaponOffsetWithLeanForAI(pCurrentWeapon, stance, lean, peekOver, eyeOffset, useWhileLeanedOffsets);
	}
}

//-----------------------------------------------------------------------------
Vec3 CActor::GetWeaponOffsetWithLeanForAI(CWeapon* pCurrentWeapon, EStance stance, float lean, float peekOver, const Vec3& eyeOffset, const bool useWhileLeanedOffsets) const
{
	if(pCurrentWeapon)
	{
		if(pCurrentWeapon->AIUseEyeOffset())
			return eyeOffset;

		Vec3	overrideOffset;
		if(pCurrentWeapon->AIUseOverrideOffset(stance, lean, peekOver, overrideOffset))
			return overrideOffset;
	}

	return GetStanceInfo(stance)->GetWeaponOffsetWithLean(lean, peekOver, useWhileLeanedOffsets);
}

//-------------------------------------------------------------------
//This function is called from Equipment Manager only for the client actor
void CActor::NotifyInventoryAmmoChange(IEntityClass* pAmmoClass, int amount)
{
	if(!pAmmoClass)
		return;
}

bool CActor::IsFriendlyEntity(EntityId entityId, bool bUsingAIIgnorePlayer/* = true*/) const
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityId);

	//Faster to not do an early out on entityId == actor->entityId
	if (!pEntity)
	{
		return true;
	}

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (!gEnv->bMultiplayer)
	{
		if (pEntity->GetAI())
		{
			return (!pEntity->GetAI()->IsHostile(GetEntity()->GetAI(), bUsingAIIgnorePlayer));
		}
	}
	else
	{
		if(pGameRules)
		{
			if (pGameRules->GetTeamCount()>=2)
			{
				int iMyTeam=pGameRules->GetTeam(GetEntityId());
				int iClientTeam = pGameRules->GetTeam(entityId);
				if (iClientTeam == 0 || iMyTeam == 0)
				{
					return true;	// entity or actor isn't on a team, hence friendly
				}
				else
				{
					return (iClientTeam==iMyTeam);
				}
			}
			else
			{
				return entityId == GetEntityId();
			}
		}

	}
	return false;
}

void CActor::AddViewAngleOffsetForFrame( const Ang3 &offset )
{

}

//------------------------------------------------------------------------
void CActor::EnableSwitchingItems( bool enable )
{
	m_enableSwitchingItems = enable;
}

//------------------------------------------------------------------------
void CActor::EnableIronSights( bool enable )
{
	m_enableIronSights = enable;
}

//------------------------------------------------------------------------
void CActor::EnablePickingUpItems( bool enable )
{
	m_enablePickupItems = enable;
}

void CActor::UpdateMountedGunController(bool forceIKUpdate)
{
}


//------------------------------------------------------------------------
bool CActor::LoadAutoAimParams(SmartScriptTable pEntityTable, SAutoaimTargetRegisterParams &outAutoAimParams)
{
	assert((bool)pEntityTable);

	bool bResult = false;

	SmartScriptTable pGameParams;
	if (pEntityTable && pEntityTable->GetValue("gameParams", pGameParams))
	{
		SmartScriptTable pAutoAimParams;
		if (pGameParams->GetValue("autoAimTargetParams", pAutoAimParams))
		{
			CScriptSetGetChain chainParams(pAutoAimParams);

			int primaryTargetId = -1, secondaryTargetId = -1, physicsTargetId = -1;
			chainParams.GetValue("primaryTargetBone", primaryTargetId);
			chainParams.GetValue("physicsTargetBone", physicsTargetId);
			chainParams.GetValue("secondaryTargetBone", secondaryTargetId);
			chainParams.GetValue("fallbackOffset", outAutoAimParams.fallbackOffset);
			chainParams.GetValue("innerRadius", outAutoAimParams.innerRadius);
			chainParams.GetValue("outerRadius", outAutoAimParams.outerRadius);
			chainParams.GetValue("snapRadius", outAutoAimParams.snapRadius);
			chainParams.GetValue("snapRadiusTagged", outAutoAimParams.snapRadiusTagged);

			outAutoAimParams.primaryBoneId = primaryTargetId;
			outAutoAimParams.physicsBoneId = physicsTargetId;
			outAutoAimParams.secondaryBoneId = secondaryTargetId;

			bResult = true;
		}
	}

	return bResult;
}

//------------------------------------------------------------------------
void CActor::RegisterInAutoAimManager()
{
	if (m_registeredInAutoAimMng)
		return;

	if (gEnv->IsEditing())
		return;

	if (m_isClient || m_health.IsDead())
		return;

	if (m_LuaCache_GameParams)
	{
		RegisterInAutoAimManager(m_LuaCache_GameParams->autoAimParams);
	}
	else
	{
		IEntity *pEntity = GetEntity();
		IEntityClass *pClass = pEntity->GetClass();

		// Run-time loading
		if (CGameCache::IsLuaCacheEnabled())
		{
			GameWarning("[Game Cache] Warning: Loading auto aim params for entity class \'%s\' at run-time!", pClass->GetName());
		}

		SAutoaimTargetRegisterParams autoAimParams;
		if (LoadAutoAimParams(pEntity->GetScriptTable(), autoAimParams))
		{
			RegisterInAutoAimManager(autoAimParams);
		}
		else
		{
			GameWarning("Failed to load auto aim params for actor \'%s\'", pEntity->GetName());
		}
	}
}

//------------------------------------------------------------------------
void CActor::RegisterInAutoAimManager(const SAutoaimTargetRegisterParams &autoAimParams)
{
	m_registeredInAutoAimMng = ShouldRegisterAsAutoAimTarget() && g_pGame->GetAutoAimManager().RegisterAutoaimTargetActor(*this, autoAimParams);
}

//------------------------------------------------------------------------
void CActor::UnRegisterInAutoAimManager()
{
	if (!m_registeredInAutoAimMng)
		return;

	g_pGame->GetAutoAimManager().UnregisterAutoaimTarget(GetEntityId());

	m_registeredInAutoAimMng = false;
}

//------------------------------------------------------------------------
void CActor::RegisterDBAGroups()
{
	if (m_registeredAnimationDBAs)
		return;

	if (m_health.IsDead() || GetEntity()->IsHidden())
		return;
	
	if (m_LuaCache_GameParams)
	{
		m_registeredAnimationDBAs = g_pGame->GetGameCache().PrepareDBAsFor(GetEntityId(), m_LuaCache_GameParams->gameParams.animationDBAs);
	}
}

//------------------------------------------------------------------------
void CActor::UnRegisterDBAGroups()
{
	if (!m_registeredAnimationDBAs)
		return;

	//The local player should keep assets always loaded
	if (IsClient())
		return;

	g_pGame->GetGameCache().RemoveDBAUser(GetEntityId());
	m_registeredAnimationDBAs = false;
}

//------------------------------------------------------------------------
void CActor::OnAIProxyEnabled( bool enabled )
{
	if (enabled)
	{
		RegisterInAutoAimManager();
		RegisterDBAGroups();
		NotifyInventoryAboutOwnerActivation();
	}
	else
	{
		UnRegisterInAutoAimManager();
		UnRegisterDBAGroups();
		NotifyInventoryAboutOwnerDeactivation();
	}
}

void CActor::OnReused(IEntity *pEntity, SEntitySpawnParams &params)
{
	// set the overridden model, not the one provided by the entity class
	UpdateActorModel();

	// have the overridden model show up
	SetActorModelInternal();
}

bool CActor::IsInteracting() const
{
	return GetActorStats()->mountedWeaponID != INVALID_ENTITYID
		|| this->GetLinkedVehicle() != nullptr;
}

void CActor::StartInteractiveAction( EntityId entityId, int interactionIndex )
{
	
}

void CActor::StartInteractiveActionByName( const char* interaction, bool bUpdateVisibility, float actionSpeed /*= 1.0f*/ )
{

}

void CActor::EndInteractiveAction( EntityId entityId )
{

}

void CActor::LockInteractor(EntityId lockId, bool lock)
{
	SmartScriptTable locker(gEnv->pScriptSystem);
	locker->SetValue("locker", ScriptHandle(lockId));
	locker->SetValue("lockId", ScriptHandle( lock ? lockId : 0));
	locker->SetValue("lockIdx", lock ? 1 : 0);
	GetGameObject()->SetExtensionParams("Interactor", locker);
}

void CActor::AddHeatPulse( const float intensity, const float time )
{

}

void CActor::SetGrabbedByPlayer( IEntity* pPlayerEntity, bool grabbed )
{
	SActorStats* pActorStats = GetActorStats();
	if ((pActorStats != NULL) && (pActorStats->isGrabbed != grabbed))
	{
		if (grabbed)
		{
			IAIObject* pAIObject = GetEntity()->GetAI();
			if (pAIObject && gEnv->pAISystem)
			{
				IAIActor* pAIActor = CastToIAIActorSafe(pAIObject);
				if(pAIActor)
				{
					IAISignalExtraData *pSData = gEnv->pAISystem->CreateSignalExtraData();	
					pSData->point = Vec3(0,0,0);
					pAIActor->SetSignal(1, "OnGrabbedByPlayer", pPlayerEntity, pSData);
				}
				pAIObject->Event(AIEVENT_DISABLE,0);
			}

			if (m_currentPhysProfile == eAP_Sleep)
				GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
			m_pAnimatedCharacter->SetInGrabbedState(true); 
		}
		else
		{
			IAIObject* pAIObject = GetEntity()->GetAI();
			if (pAIObject)
			{
				pAIObject->Event(AIEVENT_ENABLE, 0);
			}

			//Force an animation update if needed here
			SEntityEvent xFormEvent;
			xFormEvent.event = ENTITY_EVENT_XFORM;
			xFormEvent.nParam[0] = ENTITY_XFORM_ROT|ENTITY_XFORM_POS;
			m_pAnimatedCharacter->SendEvent(xFormEvent);

			m_pAnimatedCharacter->SetInGrabbedState(false);
		}

		pActorStats->isGrabbed = grabbed;
	}
}

void CActor::Reset( bool toGame )
{
	if (m_bodyDestructionInstance.GetProfileId() != INVALID_BODYDESTRUCTIBILITYPROFILEID)
	{
		g_pGame->GetBodyDamageManager()->ResetInstance(*GetEntity(), m_bodyDestructionInstance);
	}

	GenerateBlendRagdollTags();
	ReleaseLegsColliders();
}

const char * CActor::GetShadowFileModel()
{
	if (m_LuaCache_Properties)
	{
		return m_LuaCache_Properties->fileModelInfo.sShadowFileName.c_str();
	}
	else
	{
		// If we haven't cached the value, return our normal character model
		IEntity *pEntity = GetEntity();
		return pEntity->GetCharacter(0)->GetFilePath();
	}
}
void CActor::ReloadBodyDestruction()
{
	if (m_bodyDestructionInstance.GetProfileId() != INVALID_BODYDESTRUCTIBILITYPROFILEID)
	{
		g_pGame->GetBodyDamageManager()->GetBodyDestructibility(*GetEntity(), m_bodyDestructionInstance);
	}
}

void CActor::GenerateBlendRagdollTags()
{
	IMannequin &mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
	const CTagDefinition* pTagDefinition = mannequinSys.GetAnimationDatabaseManager().FindTagDef( "Animations/Mannequin/ADB/blendRagdollTags.xml" );

	if( pTagDefinition )
	{
		m_blendRagdollParams.m_blendInTagState = TAG_STATE_EMPTY;
		m_blendRagdollParams.m_blendOutTagState = TAG_STATE_EMPTY;

		{
			const TagID tagID = pTagDefinition->Find( "blendIn" );
			pTagDefinition->Set( m_blendRagdollParams.m_blendInTagState, tagID, true );
		}

		{
			const TagID tagID = pTagDefinition->Find( "blendOut" );
			pTagDefinition->Set( m_blendRagdollParams.m_blendOutTagState, tagID, true );
		}

		{
			const TagID tagID = pTagDefinition->Find( "standup" );
			pTagDefinition->Set( m_blendRagdollParams.m_blendOutTagState, tagID, true );
			pTagDefinition->Set( m_blendRagdollParams.m_blendInTagState, tagID, true );
		}

		{
			const TagID tagID = pTagDefinition->Find( "ragdoll" );
			pTagDefinition->Set( m_blendRagdollParams.m_blendInTagState, tagID, true );
		}
	}
}

void CActor::PhysicalizeLocalPlayerAdditionalParts()
{
	if (!IsClient())
		return;

	IPhysicalEntity* pPhysicalEntity = GetEntity()->GetPhysics();

	CRY_ASSERT(pPhysicalEntity);
	if (!pPhysicalEntity)
		return;

	const bool wasHidden = GetEntity()->IsHidden();

	//Make sure to un-hide before trying to do anything with physics
	if (wasHidden)
		GetEntity()->Hide(false);

	//Note: Main cylinder/capsule is part 100
	const int vegetationBendingPartId = 101;

	//Add extra capsule for vegetation bending
	//Offset slightly to the front so we get good/visible bending for the camera distance 
	{
		primitives::capsule prim;
		prim.axis.Set(0,0,1);
		prim.center.zero(); prim.r = 0.4f; prim.hh = 0.2f;

		IGeometry *pPrimGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::capsule::type, &prim);
		phys_geometry *pGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(pPrimGeom, 0);

		pe_geomparams gp;
		gp.pos = Vec3(0.0f,0.2f,0.7f);
		gp.flags = geom_colltype_foliage;
		gp.flagsCollider = 0;
		pGeom->nRefCount = 0;
		pPrimGeom->Release();

		pPhysicalEntity->AddGeometry(pGeom, &gp, vegetationBendingPartId);
	}

	//Restore hidden state if needed
	if (wasHidden)
		GetEntity()->Hide(true);

}
//------------------------------------------------------------------------
void CActor::ImmuneToForbiddenZone(const bool immune)
{
	const bool bPrev = m_IsImmuneToForbiddenZone;
	m_IsImmuneToForbiddenZone = immune;
	if(bPrev!=immune && IsClient())
	{
		if(CGameRules* pGameRules = g_pGame->GetGameRules())
		{
			pGameRules->CallOnForbiddenAreas(immune?"OnLocalPlayerImmunityOn":"OnLocalPlayerImmunityOff");
		}
	}
}

const bool CActor::ImmuneToForbiddenZone() const
{
	return m_IsImmuneToForbiddenZone; 
}

//------------------------------------------------------------------------
EntityId CActor::SimpleFindItemIdInCategory(const char *category) const
{
	EntityId retItem=0;

	if(IInventory* pInventory = GetInventory())
	{
		IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
		int categoryType = GetItemCategoryType(category);	
		int numItems = pInventory->GetCount();

		for(int i = 0; i < numItems; i++)
		{
			EntityId itemId = pInventory->GetItem(i);

			IItem* pItem = pItemSystem->GetItem(itemId);

			if(pItem && pItem->CanSelect())
			{
				const char* itemCategory = pItemSystem->GetItemCategory(pItem->GetEntity()->GetClass()->GetName());
				int itemCategoryType = GetItemCategoryType(itemCategory);
				if (itemCategoryType & categoryType)
				{
					retItem = itemId;
					break;
				}
			}
		}
	}

	return retItem;
}

void CActor::NotifyInventoryAboutOwnerActivation()
{
	if (!IsClient())
	{
		IInventory* pInventory = GetInventory();
		if (pInventory)
		{
			IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
			const int itemCount = pInventory->GetCount();
			for ( int i = 0; i < itemCount; ++i )
			{
				CItem* pItem = static_cast<CItem *>(pItemSystem->GetItem(pInventory->GetItem(i)));
				if (pItem)
				{
					pItem->OnOwnerActivated();
				}
			}
		}
	}
}

void CActor::NotifyInventoryAboutOwnerDeactivation()
{
	if (!IsClient())
	{
		IInventory* pInventory = GetInventory();
		if (pInventory)
		{
			IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
			const int itemCount = pInventory->GetCount();
			for ( int i = 0; i < itemCount; ++i )
			{
				CItem* pItem = static_cast<CItem *>(pItemSystem->GetItem(pInventory->GetItem(i)));
				if (pItem)
				{
					pItem->OnOwnerDeactivated();
				}
			}
		}
	}
}

void CActor::SetTag(TagID tagId, bool enable)
{
	if(IActionController* pActionController = GetAnimatedCharacter()->GetActionController())
	{
		SAnimationContext &animContext = pActionController->GetContext();

		animContext.state.Set(tagId, enable);
	}
}

void SActorPhysics::Serialize(TSerialize ser, EEntityAspects aspects)
{
	assert( ser.GetSerializationTarget() != eST_Network );
	ser.BeginGroup("PlayerStats");

	if (ser.GetSerializationTarget() != eST_Network)
	{
		//when reading, reset the structure first.
		if (ser.IsReading())
			*this = SActorPhysics();

		ser.Value("gravity", gravity);
		ser.Value("velocity", velocity);
		ser.Value("velocityUnconstrained", velocityUnconstrained);
		ser.Value("groundNormal", groundNormal);
	}

	ser.EndGroup();
}
void CActor::SetTagByCRC(uint32 tagId, bool enable)
{
	if(IActionController* pActionController = GetAnimatedCharacter()->GetActionController())
	{
		SAnimationContext &animContext = pActionController->GetContext();

		animContext.state.SetByCRC(tagId, enable);
	}
}

void CActor::OnSpectateModeStatusChanged( bool spectate )
{
	if(CGameLobby* pGameLobby = g_pGame->GetGameLobby())
	{
		const int channelId = GetGameObject()->GetChannelId();
		//Update spectator status

		if(IsClient())
		{
			SetupLocalPlayer();

			pGameLobby->SetLocalSpectatorStatus(spectate);
		}

		if(gEnv->bServer)
		{
			CGameRules* pGameRules = g_pGame->GetGameRules();
			if(!spectate)
			{
				CRY_ASSERT_MESSAGE( pGameRules->GetTeam(GetEntityId()) == 0, "CActor::SetSpectateStatus - Trying to add ex-spectator to a team but they already have a team" );
				pGameRules->GetPlayerSetupModule()->OnActorJoinedFromSpectate(this, channelId);
			}
			else
			{
				if(pGameRules->IsTeamGame())
				{
					pGameRules->SetTeam(0, GetEntityId());
				}
			}
		}
	}

	GetEntity()->Hide(spectate);
}

void CActor::SetupLocalPlayer()
{
	IEntity *pEntity = GetEntity();

	GetGameObject()->CaptureView(this);

	if(GetSpectatorState() != eASS_SpectatorMode)
	{
		pEntity->SetFlags(pEntity->GetFlags() | ENTITY_FLAG_TRIGGER_AREAS);
		// Invalidate the matrix in order to force an update through the area manager
		pEntity->InvalidateTM(ENTITY_XFORM_POS);

		GetGameObject()->EnablePrePhysicsUpdate( ePPU_Always );

		// always update client's character
		if (ICharacterInstance * pCharacter = pEntity->GetCharacter(0))
			pCharacter->SetFlags(pCharacter->GetFlags() | CS_FLAG_UPDATE_ALWAYS);

		// We get this after we've been revived, so start recording now.
		if (g_pGame->GetRecordingSystem() && (g_pGame->GetHostMigrationState() == CGame::eHMS_NotMigrating))
		{
			g_pGame->GetRecordingSystem()->StartRecording();
		}

		GetGameObject()->AttachDistanceChecker();
	}
	else
	{
		pEntity->SetFlags(pEntity->GetFlags() & ~ENTITY_FLAG_TRIGGER_AREAS);

		GetGameObject()->EnablePrePhysicsUpdate( ePPU_Never );

		if (ICharacterInstance * pCharacter = pEntity->GetCharacter(0))
			pCharacter->SetFlags(pCharacter->GetFlags() & ~CS_FLAG_UPDATE_ALWAYS);
	}
}

void CActor::RequestChangeSpectatorStatus( bool spectate )
{
	CRY_ASSERT_MESSAGE(IsClient(), "CActor::RequestChangeSpectatorStatus is being called on a non-client player");

	if(!CanSwitchSpectatorStatus())
	{
		return;
	}

	EActorSpectatorState desiredState = spectate ? eASS_SpectatorMode : eASS_ForcedEquipmentChange;

	if(desiredState != GetSpectatorState())
	{
		if(GetSpectatorState() != eASS_None)
		{
			m_spectateSwitchTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		}

		if(gEnv->bServer)
		{
			IGameRulesSpectatorModule *pSpectatorModule = g_pGame->GetGameRules()->GetSpectatorModule();
			if( pSpectatorModule )
			{
				pSpectatorModule->ChangeSpectatorMode( this, spectate ? eASM_Follow : CActor::eASM_Fixed, 0, false );
				SetSpectatorState(desiredState);
			}
		}
		else
		{
			CGameRules::ServerSpectatorParams params;
			params.entityId = GetEntityId();
			params.state = desiredState;
			params.mode = spectate ? eASM_Follow : CActor::eASM_Fixed;

			g_pGame->GetGameRules()->GetGameObject()->InvokeRMI(CGameRules::SvSetSpectatorState(), params, eRMI_ToServer);
		}
	}
}

bool CActor::CanSwitchSpectatorStatus() const
{
	if(g_pGameCVars->g_allowSpectators == 0 || m_spectateSwitchTime > 0.f && gEnv->pTimer->GetFrameStartTime().GetSeconds() - m_spectateSwitchTime < g_pGameCVars->g_spectatorOnlySwitchCooldown)
	{
		return false;
	}

	const CGameRules* pGameRules = g_pGame->GetGameRules();
	const int numberOfSpectators = pGameRules->GetSpectatorCount();
	const int numberOfPlayers = pGameRules->GetPlayerCountClient() - numberOfSpectators;
	if(GetSpectatorState() != eASS_SpectatorMode && ((numberOfPlayers <= 2 || numberOfSpectators >= SPECTATE_MAX_ALLOWED) || pGameRules->GetTeamPlayerCount(pGameRules->GetTeam(GetEntityId())) == 1))
	{
		return false;
	}

	return true;
}

IEntityComponent::ComponentEventPriority CActor::GetEventPriority() const
{
	return ENTITY_PROXY_USER + EEntityEventPriority_Actor;
}

void CActor::OnHostMigrationCompleted()
{
	// If we had a request out there in the wild... unlikely to get a response now!
	SetStillWaitingOnServerUseResponse(false);

	CItem *pItem = (CItem*) GetCurrentItem();
	if (pItem)
	{
		CWeapon *pWeapon = (CWeapon*) pItem->GetIWeapon();
		if (pWeapon)
		{
			pWeapon->OnHostMigrationCompleted();
		}
	}
}

void CActor::SetStillWaitingOnServerUseResponse( bool waiting )
{
#ifndef _RELEASE
	if(m_bAwaitingServerUseResponse && waiting)
	{
		CryFatalError("CActor::SetStillWaitingOnServerUseResponse - Two systems are triggering to wait for a server response at the same time.");
	}
#endif
	m_fAwaitingServerUseResponse = 0.f;
	m_bAwaitingServerUseResponse = waiting;
}

void CActor::UpdateServerResponseTimeOut( const float frameTime )
{
	// Fallback for Server Use Response
	if(m_bAwaitingServerUseResponse)
	{
		m_fAwaitingServerUseResponse += frameTime;
		if(m_fAwaitingServerUseResponse>10.f)
		{
			SetStillWaitingOnServerUseResponse(false);
		}
	}
}


void CActor::UpdateLegsColliders()
{
	pe_status_living sl;
	pe_player_dynamics pd;
	if (g_pGameCVars->pl_legs_colliders_dist<=0 || !GetEntity()->GetPhysics() || !GetEntity()->GetPhysics()->GetStatus(&sl) || !GetEntity()->GetPhysics()->GetParams(&pd) || !pd.bActive)
	{
		ReleaseLegsColliders();
		return;
	}
	Vec3 pos = GetEntity()->GetPos();
	Quat q = GetEntity()->GetRotation();
	IPhysicalEntity *pPhysicalEntity = GetEntity()->GetPhysics();

	if (ICharacterInstance *pChar = GetEntity()->GetCharacter(0))
		if (IPhysicalEntity *pCharPhys = pChar->GetISkeletonPose()->GetCharacterPhysics())
			if (!m_pLegsCollider[0])
			{
				pe_params_pos pp; pp.iSimClass=2;
				m_pLegsFrame = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_ARTICULATED,&pp);
				pe_simulation_params simp; simp.collTypes = 0;
				m_pLegsFrame->SetParams(&simp);
				m_pLegsIgnoredCollider = 0;
				pe_params_part ppart; ppart.ipart=0;	
				if (pCharPhys->GetParams(&ppart))
				{
					pe_params_articulated_body pab; 
					pab.bGrounded=1; pab.pHost=pPhysicalEntity;	
					m_pLegsFrame->SetParams(&pab);
					pe_articgeomparams agp;
					agp.flags=agp.flagsCollider = 0; agp.idbody=0;
					m_pLegsFrame->AddGeometry(ppart.pPhysGeom,&agp);
					pe_params_flags pf; pf.flagsAND = ~pef_traceable;
					int bFrameQueued=m_pLegsFrame->SetParams(&pf)-1, bLegsQueued=0;
					pe_geomparams gp; 
					gp.flags = geom_colltype_solid & ~(geom_colltype_player|geom_colltype8);
					gp.flagsCollider = geom_colltype0;
					gp.mass = 5;
					gp.scale = g_pGameCVars->pl_legs_colliders_scale;
					simp.collTypes = ent_sleeping_rigid|ent_rigid;

					char boneName[] = "Bip01 L Calf";
					for(int i=0; i<2; i++,boneName[6]='R')
						if ((ppart.partid=pChar->GetIDefaultSkeleton().GetJointIDByName(boneName))>0 && (ppart.ipart=-1,pCharPhys->GetParams(&ppart)))
						{
							QuatT qbone = pChar->GetISkeletonPose()->GetAbsJointByID(m_iboneLeg[i]=ppart.partid);
							pp.pos = pos + q*qbone.t;	pp.q = q*qbone.q;
							m_pLegsCollider[i] = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_RIGID,&pp);
							m_pLegsCollider[i]->AddGeometry(ppart.pPhysGeom,&gp,ppart.partid);
							pe_action_add_constraint aac;
							aac.flags = local_frames|constraint_no_tears|constraint_ignore_buddy|constraint_no_rotation;
							aac.pt[0].zero(); aac.qframe[0].SetIdentity();
							aac.pt[1] = q*qbone.t; aac.qframe[1] = q*qbone.q;
							aac.maxPullForce = aac.maxBendTorque = 5000.0f;
							aac.pBuddy = m_pLegsFrame; aac.id = 100;
							m_pLegsCollider[i]->Action(&aac,-bFrameQueued>>31);
							bLegsQueued += m_pLegsCollider[i]->SetParams(&simp)-1;
							m_ptSample[i].zero();
							m_bLegActive[i] = 1;
						}
					if (m_pLegsCollider[0] && m_pLegsCollider[1])
					{
						pe_action_add_constraint aac;
						aac.flags = constraint_inactive|constraint_ignore_buddy;
						aac.pBuddy = m_pLegsCollider[1]; aac.pt[0].zero();
						m_pLegsCollider[0]->Action(&aac,-bLegsQueued>>31);
					}
				}
			}	else for(int i=0;i<2;i++)	if (m_pLegsCollider[i])
			{
				QuatT qbone = pChar->GetISkeletonPose()->GetAbsJointByID(m_iboneLeg[i]);
				int bActive = isneg((gEnv->pSystem->GetViewCamera().GetPosition()-pos).len2()-sqr(g_pGameCVars->pl_legs_colliders_dist));
				pe_action_awake aa; aa.bAwake = bActive;
				if (bActive!=m_bLegActive[i])
				{
					pe_params_flags pf; 
					pf.flagsOR=pef_traceable&-bActive; pf.flagsAND=~(pef_traceable&~-bActive);
					m_pLegsCollider[i]->SetParams(&pf);
					pf.flagsOR=pef_disabled&~-bActive; pf.flagsAND=~(pef_disabled&-bActive);
					m_pLegsFrame->SetParams(&pf);
					if (bActive)
					{
						pe_params_pos pp; pp.pos = pos;
						m_pLegsFrame->SetParams(&pp);
						pp.pos = pos + q*qbone.t;	pp.q = q*qbone.q;
						m_pLegsCollider[i]->SetParams(&pp);
					}	else
					{
						m_pLegsCollider[i]->Action(&aa);
						pe_action_update_constraint auc; auc.idConstraint=101; auc.bRemove=1;
						m_pLegsCollider[i]->Action(&auc);	m_pLegsIgnoredCollider=0;
					}
					m_bLegActive[i] = bActive;
				}
				if (bActive)
				{
					pe_action_update_constraint auc;
					auc.idConstraint = 100;	auc.flags = local_frames;
					auc.pt[1] = q*qbone.t; 
					auc.qframe[1] = q*qbone.q;
					m_pLegsCollider[i]->Action(&auc);
					Vec3 ptSample = auc.qframe[1]*Vec3(0,0,1)+auc.pt[1]+pos;
					if ((ptSample-m_ptSample[i]).len2()>sqr(0.005f))
					{
						m_pLegsCollider[i]->Action(&aa); m_ptSample[i] = ptSample;
					}
					if (m_pLegsIgnoredCollider!=sl.pGroundCollider)
					{
						auc.idConstraint=101; auc.bRemove=1;
						m_pLegsCollider[i]->Action(&auc);	
						if (sl.pGroundCollider)
						{
							pe_action_add_constraint aac; aac.id=101; aac.pt[0].zero();
							aac.flags = constraint_ignore_buddy|constraint_inactive;
							aac.pBuddy = sl.pGroundCollider;
							m_pLegsCollider[i]->Action(&aac);
						}
						if (i==1)
							m_pLegsIgnoredCollider = sl.pGroundCollider;
					}
				}
			}
}

void CActor::ReleaseLegsColliders()
{
	for(int i=0;i<2;i++) if (m_pLegsCollider[i])
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pLegsCollider[i]);
	if (m_pLegsFrame)
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pLegsFrame);	
	m_pLegsFrame=m_pLegsCollider[0]=m_pLegsCollider[1] = 0;
}

void CActor::AcquireOrReleaseLipSyncExtension()
{
	// get rid of a possibly acquired lip-sync extension
	if (!m_sLipSyncExtensionType.empty())
	{
		GetGameObject()->ReleaseExtension(m_sLipSyncExtensionType.c_str());
		m_sLipSyncExtensionType = "";
	}

	IGameObject* pGameObject = GetGameObject();
	IEntity* pEntity = pGameObject->GetEntity();

	// attempt to acquire the lip-sync extension: check for holding a "LipSync" script table and if so add the LipSync extension (the specific LipSyncProvider will load its settings from this table)
	if (IScriptTable* pScriptTable = pEntity->GetScriptTable())
	{
		SmartScriptTable pPropertiesTable;
		if (pScriptTable->GetValue("Properties", pPropertiesTable))
		{
			SmartScriptTable pLipSyncTable;
			if (pPropertiesTable->GetValue("LipSync", pLipSyncTable))
			{
				bool enabled = false;
				if (pLipSyncTable->GetValue("bEnabled", enabled) && enabled)
				{
					const char* type = NULL;
					if (pLipSyncTable->GetValue("esLipSyncType", type))
					{
						if (pGameObject->AcquireExtension(type) != NULL)
						{
							m_sLipSyncExtensionType = type;
						}
					}
				}
			}
		}
	}
}
