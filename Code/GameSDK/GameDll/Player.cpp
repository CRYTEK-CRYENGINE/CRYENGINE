// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 29:9:2004: Created by Filippo De Luca

*************************************************************************/
#include "StdAfx.h"
#include "Player.h"
#include "Game.h"
#include "GameCVars.h"
#include "GameActions.h"

#include "Weapon.h"
#include "WeaponSystem.h"
#include "GameRules.h"

#include "PlayerMovementController.h"

#include "UI/UIManager.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDEventWrapper.h"

#include "IPlayerEventListener.h"

#include "IInteractor.h"
#include "IPlayerUpdateListener.h"

#include "State.h"
#include "PlayerStateSwim.h"
#include "PlayerStateLedge.h"
#include "PlayerStateFly.h"
#include "PlayerCamera.h"
#include "PlayerRotation.h"
#include "PlayerInput.h"
#include "PlayerStateUtil.h"
#include "NetPlayerInput.h"
#include "AIDemoInput.h"

#include <CryAnimation/CryCharAnimationParams.h>

#include "VehicleClient.h"

#include "HitDeathReactions.h"

#include "ICooperativeAnimationManager.h"

#include "Binocular.h"

#include "ScreenEffects.h"
#include "Utility/CryWatch.h"
#include "Environment/LedgeManager.h"

#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameRulesModules/IGameRulesAssistScoringModule.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "GameRulesModules/IGameRulesScoringModule.h"
#include "GameRulesModules/IGameRulesObjectivesModule.h"
#include "GameRulesModules/IGameRulesVictoryConditionsModule.h"

#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include "RecordingSystem.h"
#include "GodMode.h"
#include "PlayerProgression.h"
#include "Battlechatter.h"

#include "PlayerPlugin_Interaction.h"

#include "BodyDamage.h"
#include <CryAISystem/IAIActor.h>
#include "PickAndThrowWeapon.h"
#include "PickAndThrowProxy.h"

#include "CorpseManager.h"

#include "StatsRecordingMgr.h"
#include "StatsEntityIdRegistry.h"

#include "Network/Lobby/GameLobby.h"

#include "GameMechanismManager/GameMechanismManager.h"

#include "IForceFeedbackSystem.h"
#include "PersistantStats.h"
#include "AutoAimManager.h"

#include "Utility/DesignerWarning.h"
#include "Utility/CryDebugLog.h"

#include "GameCache.h"
#include "SmokeManager.h"

#include <CryCore/TypeInfo_impl.h>

#include "SlideController.h"
#include "Melee.h"
#include "FireMode.h"

#include "ActorImpulseHandler.h"
#include "EquipmentLoadout.h"

#include "LocalPlayerComponent.h"

#include "ProceduralWeaponAnimation.h"

#include <CryExtension/CryCreateClassInstance.h>
#include "EnvironmentalWeapon.h"
#include <CryMath/Cry_GeoDistance.h>
#include "ItemAnimation.h"
#include <CryAISystem/IAIActorProxy.h>
#include "ActorTelemetry.h"
#include "PlayerPluginEventDistributor.h"
#include "PlayerPlugin_InteractiveEntityMonitor.h"

#include "Environment/InteractiveObject.h"

#include "PlayerAnimation.h"
#include "MovementAction.h"
#include "AnimActionAIMovement.h"

#include "SprintStamina.h"
#include "GamePhysicsSettings.h"

#include "TeamVisualizationManager.h"
#include "AI/GameAISystem.h"
#include <CryAISystem/ITargetTrackManager.h>
#include "ProceduralContextRagdoll.h"

#include "UI/UIManager.h"

#include "Nodes/FlowFadeNode.h"

#include <CrySystem/VR/IHMDDevice.h>
#include <CrySystem/VR/IHMDManager.h>

#include <IPerceptionManager.h>

DEFINE_STATE_MACHINE( CPlayer, Movement ); 

#define FOOTSTEPS_DEEPWATER_DEPTH 1  // meters
#define raycast raycast_player

#define LOG_PING_FREQUENCY 40.0f

// enable this to check nan's on position updates... useful for debugging some weird crashes
#if !defined(_RELEASE)
	#define ENABLE_NAN_CHECK
#endif

#if !defined(_RELEASE)
	#define SPECTATE_DEBUG
#endif

#undef CHECKQNAN_FLT
#ifdef ENABLE_NAN_CHECK
#define CHECKQNAN_FLT(x) \
	assert(((*alias_cast<unsigned*>(&x))&0xff000000) != 0xff000000u && (*alias_cast<unsigned*>(&x) != 0x7fc00000))
#else
#define CHECKQNAN_FLT(x) (void*)0
#endif

#define CHECKQNAN_VEC(v) \
	CHECKQNAN_FLT(v.x); CHECKQNAN_FLT(v.y); CHECKQNAN_FLT(v.z)

#define REUSE_VECTOR(table, name, value)	\
	{ if (table->GetValueType(name) != svtObject) \
	{ \
	table->SetValue(name, (value)); \
	} \
		else \
	{ \
	SmartScriptTable v; \
	table->GetValue(name, v); \
	v->SetValue("x", (value).x); \
	v->SetValue("y", (value).y); \
	v->SetValue("z", (value).z); \
	} \
	}

#undef CALL_PLAYER_EVENT_LISTENERS
#define CALL_PLAYER_EVENT_LISTENERS(func) \
{ \
	if (m_playerEventListeners.empty() == false) \
	{ \
		CPlayer::TPlayerEventListeners::const_iterator iter = m_playerEventListeners.begin(); \
		CPlayer::TPlayerEventListeners::const_iterator cur; \
		while (iter != m_playerEventListeners.end()) \
		{ \
			cur = iter; \
			++iter; \
			(*cur)->func; \
		} \
	} \
}

const float SMOKE_EXIT_TIME		= 0.25f;
const float SMOKE_ENTER_TIME	= 0.1f;

const float FLASHBANG_REACT_TIME = 5.0f;

CPlayer::SReactionAnim CPlayer::m_reactionAnims[EReaction_Total] = 
{
	{NULL,												0,											0.0f, -1},
	{"stand_tac_smokeIn_3p_01",   CA_ALLOW_ANIM_RESTART,	0.2f, -1},
	{"stand_tac_smokeLoop_3p_01", CA_LOOP_ANIMATION,			0.0f, -1},
	{"stand_tac_smokeOut_3p_01",  0,											0.0f, -1},
	{"stand_tac_flashIn_3p_01",   CA_ALLOW_ANIM_RESTART,	0.2f, -1},
	{"stand_tac_flashLoop_3p_01", CA_LOOP_ANIMATION,			0.0f, -1},
	{"stand_tac_flashOut_3p_01",  0,											0.0f, -1}
};

struct LoadoutScriptModelData
{
	char*					m_Model1Name;
	char*					m_Model2Name;

	char*					m_ScriptClass;
	char*					m_ScriptClassOveride;
	char*					m_AnimGraphName;
};

LoadoutScriptModelData ScriptModelData[] = 
{
	{
	},
};

namespace
{
	void RegisterGOEvents( CPlayer& player, IGameObject& gameObject )
	{
		const int eventToRegister [] =
		{
			// CPlayer
			eGFE_OnCollision,
			eCGE_AnimateHands,
			eCGE_SetTeam,
			eCGE_Launch,
			eCGE_CoverTransitionEnter,
			eCGE_CoverTransitionExit,
			eGFE_RagdollPhysicalized,
			eGFE_StoodOnChange,

			// CActor
			eCGE_OnShoot,
			eCGE_Ragdollize,
			eGFE_EnableBlendRagdoll,
			eGFE_DisableBlendRagdoll,
			eCGE_EnablePhysicalCollider,
			eCGE_DisablePhysicalCollider,

			// HitDeath.
			eCGE_ReactionEnd,

			// MovementTransitonsController.
			eCGE_AllowStartTransitionEnter,			
			eCGE_AllowStartTransitionExit,			
			eCGE_AllowStopTransitionEnter,						
			eCGE_AllowStopTransitionExit,					
			eCGE_AllowDirectionChangeTransitionEnter,
			eCGE_AllowDirectionChangeTransitionExit
		};

		gameObject.RegisterExtForEvents( &player, eventToRegister, sizeof(eventToRegister)/sizeof(int) );
	}
}

//--------------------
//this function will be called from the engine at the right time, since bones editing must be placed at the right time.
int PlayerProcessBones(ICharacterInstance *pCharacter,void *pvPlayer)
{
	//FIXME: do something to remove gEnv->pTimer->GetFrameTime()
	//process bones specific stuff (IK, torso rotation, etc)
	float timeFrame = gEnv->pTimer->GetFrameTime();

	CPlayer *pPlayer = (CPlayer *)pvPlayer;
	assert(pPlayer);

	ISkeletonAnim* pISkeletonAnim = pCharacter->GetISkeletonAnim();

	//--- Problem: The relative motion extraction lags by one frame.
	//--- This fixes that by applying this frame's motion to the root bone, ensuring that the 
	//--- char's position is always up to date
	//if (pPlayer->IsPlayer())
	//{
	//	EMovementControlMethod horizMCM = pPlayer->GetAnimatedCharacter()->GetMCMH();
	//	if (((horizMCM == eMCM_Animation) || (horizMCM == eMCM_AnimationHCollision)))
	//	{
	//		const QuatT& qt = pISkeletonAnim->GetRelMovement();
	//		pCharacter->GetISkeletonPose()->SetAbsJointByID(0, qt);
	//	}
	//}

	uint32 numAnim = pISkeletonAnim->GetNumAnimsInFIFO(0);
	if (numAnim)
		pPlayer->ProcessIKLimbs(timeFrame);

	pPlayer->PostProcessAnimation(pCharacter);

	return 1;
}

void CPlayer::PostProcessAnimation(ICharacterInstance *pCharacter)
{
	const int curFrameId = gEnv->nMainFrameID;

	if (IsClient() && m_playerCamera && m_isControllingCamera)
	{
		int cameraJnt = GetBoneID(BONE_CAMERA);
		if (cameraJnt >= 0)
		{
			QuatT newCam = pCharacter->GetISkeletonPose()->GetAbsJointByID(cameraJnt);

			// Only use world pos if not in camera space
			IEntity* pEntity = GetEntity();
			Vec3 worldPos = ZERO;
			const int slotIndex = 0; 
			const uint32 entityFlags = pEntity->GetSlotFlags(slotIndex);

			if(!(entityFlags & ENTITY_SLOT_RENDER_NEAREST))
			{
				worldPos = pEntity->GetWorldPos();
			}

			QuatT entityTran(pEntity->GetWorldRotation(), worldPos);
			QuatT offset;
			CItem *pItem = (CItem *)GetCurrentItem();
			if (pItem)
			{
				pItem->GetFPOffset(offset);
			}
			else
			{
				offset.SetIdentity();
			}

			QuatT newTransform = entityTran * newCam * offset.GetInverted();
			QuatT delta = m_lastCameraLocation.GetInverted() * newTransform;

			m_playerCamera->PostUpdate(delta);

			m_isControllingCamera = false;

			if (!IsThirdPerson())
			{
				const float fadeoutNearWallThreshold = g_pGameCVars->g_hmdFadeoutNearWallThreshold; // do a proper implementation
				float fadeAmount(0.f);
				float fadeTime(0.1f);

				if (fadeoutNearWallThreshold > FLT_EPSILON)
				{
					const ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);

					Vec3 eyeOffset(m_eyeOffset);

					int cameraJoint = GetBoneID(BONE_CAMERA);

					if (cameraJoint >= 0)
					{
						eyeOffset = pCharacter->GetISkeletonPose()->GetAbsJointByID(cameraJoint).t;
					}
					else
					{
						CryLogAlways("ERROR: Player Rig does not have a camera bone [BONE_CAMERA]");
					}

					Vec3 cameraOffset = Vec3(0.f, 0.f, eyeOffset.z) + GetBaseQuat() * Vec3(eyeOffset.x, eyeOffset.y, 0.f);

					Vec3 hmdPositionTranslation(ZERO);
					Vec3 hmdPosition(ZERO);
					Quat hmdRotation(IDENTITY);

					IHmdManager* pHmdManager = gEnv->pSystem->GetHmdManager();

					if (pHmdManager && pHmdManager->IsStereoSetupOk())
					{
						const HmdTrackingState& sensorState = pHmdManager->GetHmdDevice()->GetLocalTrackingState();

						if (sensorState.CheckStatusFlags(eHmdStatus_IsUsable))
						{
							hmdRotation = sensorState.pose.orientation;
							hmdPosition = sensorState.pose.position;
						}
					}

					if (!hmdPosition.IsZero())
					{
						hmdPositionTranslation = GetViewQuat() * hmdRotation.GetInverted() * hmdPosition;
					}

					Vec3 eyePosWithHmdTranslation = cameraOffset + hmdPositionTranslation;

					// check for collision and fade to black
					IPhysicalEntity* pSkipEntities[2];
					const int numSkipEntities = GetPhysicalSkipEntities(pSkipEntities, 2);
					const Vec3 worldEyePosWithHmdTranslation = GetEntity()->GetPos() + eyePosWithHmdTranslation;

					// First, check if the camera is already behind a wall
					const Vec3 sweepStart = GetEntity()->GetWorldPos() + Vec3(0.f, 0.f, GetEyeOffset().z) + GetViewQuat() * Vec3(GetEyeOffset().x, GetEyeOffset().y, 0.f);
					const Vec3 sweepDirection = worldEyePosWithHmdTranslation - sweepStart;

					primitives::sphere physPrimitive;
					physPrimitive.center = sweepStart;
					physPrimitive.r = 0.05f;

					geom_contact* pContact;
					const int entTypes = ent_all;
					const int rwiFlags = rwi_stop_at_pierceable;

					float hitDistance = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(physPrimitive.type, &physPrimitive, sweepDirection, entTypes, &pContact, 0, rwiFlags, 0, 0, 0, pSkipEntities, numSkipEntities);

					if (hitDistance > FLT_EPSILON)
					{
						fadeAmount = 1.f;
						fadeTime = 0.01f;
					}
					else
					{
						// If not, check if the camera is close to a wall
						physPrimitive.center = worldEyePosWithHmdTranslation;
						physPrimitive.r = fadeoutNearWallThreshold;

						hitDistance = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(physPrimitive.type, &physPrimitive, ZERO, entTypes, &pContact, 0, rwiFlags, 0, 0, 0, pSkipEntities, numSkipEntities);

						if (hitDistance > FLT_EPSILON)
						{
							const float distanceFromEyes = (pContact->pt - worldEyePosWithHmdTranslation).GetLength();
							fadeAmount = clamp_tpl(1.f - (distanceFromEyes / fadeoutNearWallThreshold) * 0.5f, 0.f, 1.f);
						}
					}
				}

				if (fadeAmount != m_closeToWallFadeoutAmount)
				{
					m_closeToWallFadeoutAmount = fadeAmount;

					CHUDFader* pFader = m_pMasterFader->GetHUDFader(0);

					if (pFader)
					{
						ColorF col = Col_Black;
						pFader->Stop();

						if (fadeAmount > FLT_EPSILON)
						{
							ColorF currentFadeColor = pFader->GetCurrentColor();
							pFader->FadeOut(col, fadeTime, "", true, false, fadeAmount, currentFadeColor.a);
						}
						else
						{
							pFader->FadeIn(col, fadeTime);
						}
					}
				}
			}
		}
	}
}

//--------------------

#define SONAR_VISION_MOVEMENT_SYMBOL_PARTICLES	"perk_fx.sonar_hearing.sonar_movement_symbol"
#define SONAR_VISION_MOVEMENT_SMOKE_PARTICLES		"perk_fx.sonar_hearing.sonar_movement_smoke"
#define CLOAK_AWARENESS_PARTICLES								"perk_fx.sonar_hearing.cloak_awareness"

#ifdef STATE_DEBUG
	EntityId CPlayer::s_StateMachineDebugEntityID = 0;
	void CPlayer::DebugStateMachineEntity( const char* pName )
	{
		assert (pName != 0);
		s_StateMachineDebugEntityID = 0;
		if (0 == strcmp(pName, "1"))
		{
			if (IActor * pActor = gEnv->pGameFramework->GetClientActor())
				s_StateMachineDebugEntityID = pActor->GetEntityId();
		}
		else if (IEntity * pEntity = gEnv->pEntitySystem->FindEntityByName(pName))
		{
			s_StateMachineDebugEntityID = pEntity->GetId();
		}
	}
#endif

CPlayer::CPlayer()
: m_pLocalPlayerInteractionPlugin(NULL)
, m_deferredKnockDownPending(false)
, m_deferredKnockDownImpulse(0.0f)
, m_carryObjId(0)
, m_pIAttachmentGrab(NULL)
, m_stealthKilledById(0)
, m_pPlayerRotation(NULL)
, m_mpModelIndex(MP_MODEL_INDEX_DEFAULT)
, m_ledgeCounter(0)
, m_ledgeID(LedgeId::invalid_id)
, m_ledgeFlags(eLF_NONE)
, m_ladderId(0)
, m_lastLadderLeaveLoc(eLLL_First)
, m_ladderHeightFrac(0.0f)
, m_ladderHeightFracInterped(0.0f)
, m_timeFirstSpawned(0.f)
, m_mountedGunControllerEnabled(true)
, m_pendingLoadoutGroup(-1)
, m_bCanTurnBody(true)
, m_isControllingCamera(false)
, m_bMakeVisibleOnNextSpawn(false)
, m_bDontResetFXUntilNextSpawnRevive(false)
, m_bHasAimLimit(false)
, m_bPlayIntro(false)
, m_lastLedgeTime(0.0f)
, m_lastCachedInteractionIndex(-1)
, m_pPlayerPluginEventDistributor(NULL)
, m_xpBonusMultiplier(100)
, m_pInteractiveEntityMonitorPlugin(NULL)
, m_lastCameraLocation(IDENTITY)
, m_lastReloadTime(0.f)
, m_teamWhenKilled(-1)
#if ENABLE_RMI_BENCHMARK
, m_RMIBenchmarkLast( 0 )
, m_RMIBenchmarkSeq( 0 )
#endif
, m_weaponFPAiming(false)
, m_isInWater(false)
, m_isHeadUnderWater(false)
, m_fOxygenLevel(1.0f)
, m_waterEnter(CryAudio::InvalidControlId)
, m_waterExit(CryAudio::InvalidControlId)
, m_waterDiveIn(CryAudio::InvalidControlId)
, m_waterDiveOut(CryAudio::InvalidControlId)
, m_waterInOutSpeed(CryAudio::InvalidControlId)
{
	m_pPlayerRotation = new CPlayerRotation(*this);
	CRY_ASSERT( m_pPlayerRotation );

	m_pPlayerPluginEventDistributor = new CPlayerPluginEventDistributor;
	CRY_ASSERT( m_pPlayerPluginEventDistributor );

	m_pPlayerTypeComponent = NULL;

	m_currentlyTargettingPlugin.SetOwnerPlayer(this);
	
	m_pInteractor = 0;

	m_timedemo = false;
	m_jumpButtonIsPressed = false;

	m_playerCamera = NULL;

	m_pSprintStamina = NULL;

	m_pVehicleClient = 0;

	m_lastRequestedVelocity.zero();
	m_forcedLookDir.zero();
	m_forcedLookObjectId = 0;

	m_sufferingHighLatency = false;

	m_dropCorpseOnDeath = false;
	m_hideOnDeath = false;

	m_eyeOffset.zero();
	m_weaponOffset.zero();

	m_pMasterFader = new CMasterFader();
	m_closeToWallFadeoutAmount = 0.f;

	m_pIEntityAudioComponent = nullptr;
	m_fLastEffectFootStepTime = 0.f;

	m_vehicleViewDir.Set(0,1,0);

	m_flashbangSignal.SetSignal("FlashbangTinitus");
	
	m_timeOfLastHealthSync = 0.0f;

	m_ragdollTime = 0.0f;
	
	m_footstepCounter = 0;

	m_lastFlashbangShooterId = 0;
	m_lastFlashbangTime = 0.0f;

	m_lastZoomedTime = 0.0f;

	m_netFlashBangStun = false;

	m_reactionOverlay = EReaction_None;
	m_reactionFactor = 1.0f;

	m_thermalVisionBaseHeat = 0.65f;

	m_meleeHitCounter = 0;

	m_numActivePlayerPlugins = 0;
	memset (m_activePlayerPlugins, 0, sizeof(m_activePlayerPlugins));

	m_mountedGunController.InitWithPlayer(this);
	
	// init sound table
	struct 
	{
		EPlayerSounds soundID;
		const char* signalName;
		bool repeated;
	} tmpSoundTable[ESound_Player_Last] = 
	{
		{ ESound_Jump, "PlayerFeedback_Jump", false },
		{ ESound_Fall_Drop, "PlayerFeedback_Fall", false },
		{ ESound_ChargeMelee, "PlayerFeedback_ChargeMelee", true },
		{ ESound_Breathing_UnderWater, "PlayerFeedback_UnderwaterBreathing" , true },
		{ ESound_Gear_Walk, gEnv->bMultiplayer ? "" : "Player_Footstep_Gear_Walk", false},	//not triggered in MP
		{ ESound_Gear_Run, gEnv->bMultiplayer ? "Player_Footstep_Gear_MP_Team1" : "Player_Footstep_Gear_Run", false},	//dynamically set with team
		{ ESound_Gear_Jump, gEnv->bMultiplayer ? "Player_Footstep_Jump_MP" : "Player_Gear_Jump", false},
		{ ESound_Gear_Land, gEnv->bMultiplayer ? "Player_Footstep_Land_MP" : "Player_Gear_Land", false},
		{ ESound_Gear_HeavyLand, gEnv->bMultiplayer ? "Player_Footstep_Gear_MP" : "Player_Gear_HeavyLand", false},
		{ ESound_Gear_Water, "PlayerFeedback_GearWater", true },
		{ ESound_FootStep_Boot, gEnv->bMultiplayer ? "" : "Player_Footstep_Boot", false }, //not triggered in MP
		{ ESound_FootStep_Boot_Armor, gEnv->bMultiplayer ? "" : "Player_Footstep_Boot_Armor", false }, //not triggered in MP
		{ ESound_WaterEnter, "PlayerFeedback_WaterEnter", true },
		{ ESound_WaterExit, "PlayerFeedback_WaterExit", true },
		{ ESound_DiveIn, "PlayerFeedback_DiveIn", false },
		{ ESound_DiveOut, "PlayerFeedback_DiveOut", false },
		{ ESound_EnterMidHealth, "Player_MedHealth_Enter", false},
		{ ESound_ExitMidHealth, "Player_MedHealth_Exit", false},
		{ ESound_MedicalMonitorRegen, "Perk_Regen_Suit", false},
		{ ESound_Player_Last, "", false }  // need to always be the last element
	};

	for (int i=0; tmpSoundTable[i].soundID != ESound_Player_Last; ++i)
	{
		EPlayerSounds soundID = tmpSoundTable[i].soundID;
		assert(soundID >= ESound_Player_First && soundID < ESound_Player_Last);

		SSound& sound = m_sounds[tmpSoundTable[i].soundID];
		if(tmpSoundTable[i].signalName && tmpSoundTable[i].signalName[0])
		{
			sound.audioSignalPlayer.SetSignal( tmpSoundTable[i].signalName );
		}
		sound.isRepeated = tmpSoundTable[i].repeated;
	}

	m_jumpCounter = 0;
	m_jumpVel.zero();
	
	m_netPlayerProgression.Construct(this);

	m_registeredOnHUD = false;

	m_usingSpectatorPhysics = false;
	m_inNetLimbo = false;
	
	m_netCloseCombatSnapTargetId = 0;

	CALL_PLAYER_EVENT_LISTENERS(OnToggleThirdPerson(this,m_stats.isThirdPerson));

	m_waterEnter = CryAudio::StringToId("water_enter");
	m_waterExit = CryAudio::StringToId("water_exit");
	m_waterDiveIn = CryAudio::StringToId("water_dive_in");
	m_waterDiveOut = CryAudio::StringToId("water_dive_out");
	m_waterInOutSpeed = CryAudio::StringToId("water_in_out_speed");
}

CPlayer::~CPlayer()
{
	if(!gEnv->bMultiplayer && IsPlayer())
	{
		if(IPlayerProfileManager *pProfileMan = gEnv->pGameFramework->GetIPlayerProfileManager())
		{
			pProfileMan->RemoveListener(this);
		}
	}

	SAFE_DELETE(m_pPlayerTypeComponent);
	SAFE_DELETE(m_playerCamera);

	LeaveAllPlayerPlugins();
	SAFE_DELETE(m_pLocalPlayerInteractionPlugin);
	SAFE_DELETE(m_pInteractiveEntityMonitorPlugin);

	m_pPlayerInput.reset();
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	if(pCharacter)
		pCharacter->GetISkeletonPose()->SetPostProcessCallback(0,0);
	
	SAFE_DELETE(m_pSprintStamina);

	m_pVehicleClient = NULL;

	if (m_pInteractor)
		GetGameObject()->ReleaseExtension("Interactor");

	CGameRules *pGameRules = g_pGame->GetGameRules();
	IGameRulesPlayerStatsModule *playerStats = pGameRules ? pGameRules->GetPlayerStatsModule() : NULL;
	if (playerStats)
	{
		playerStats->RemovePlayerStats(GetEntityId());
	}

	IGameRulesSpawningModule *spawningModule = pGameRules ? pGameRules->GetSpawningModule() : NULL;
	if (spawningModule)
	{
		spawningModule->PlayerLeft(GetEntityId());
	}

	{
		IGameRulesAssistScoringModule *assistScoringModule = pGameRules ? pGameRules->GetAssistScoringModule() : NULL;
		if (assistScoringModule)
		{
			assistScoringModule->UnregisterAssistTarget(GetEntityId());
		}
	}

	if (InNetLimbo() && (g_pGame->GetHostMigrationState() == CGame::eHMS_NotMigrating))
	{
		// If we were in net limbo and a migration hadn't started, we need to resume the game timer otherwise
		// when we next load into a game, it'll be paused!
		// Note: This will only happen for the local player since no-one else can enter net limbo
		g_pGame->GetIGameFramework()->PauseGame(false, false);
	}

	StateMachineReleaseMovement();
	SAFE_DELETE( m_pPlayerRotation );
	SAFE_DELETE( m_pPlayerPluginEventDistributor );
	SAFE_DELETE( m_pMasterFader );

	// Release effects after state machine (in case some state is trying to do something with them on exit)
	m_hitRecoilGameEffect.Release();
	m_playerHealthEffect.Release();

	if (m_pHitDeathReactions)
		m_pHitDeathReactions->ReleaseReactionAnims(eRRF_Alive | eRRF_OutFromPool | eRRF_AIEnabled);

	if (IsPlayer())
	{
		CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			pRecordingSystem->OnPlayerLeft(GetEntityId());
		}
	}
}

bool CPlayer::Init(IGameObject * pGameObject)
{
	CCCPOINT(PlayerState_Init);

#if ENABLE_PLAYER_HEALTH_REDUCTION_POPUPS
	m_healthAtStartOfUpdate = -1.f;
#endif

	StateMachineInitMovement();

	if (!CActor::Init(pGameObject))
		return false;

	IEntity *pEntity = GetEntity();
	IScriptTable *pEntityScript = pEntity->GetScriptTable();
	m_isAIControlled = false;
	if (pEntityScript)
	{
		HSCRIPTFUNCTION isAIControlledFunc(NULL);
		if (pEntityScript->GetValue("IsAIControlled", isAIControlledFunc))
		{
			Script::CallReturn(gEnv->pScriptSystem, isAIControlledFunc, m_isAIControlled);
			gEnv->pScriptSystem->ReleaseFunc(isAIControlledFunc);
		}
	}

	if (m_isAIControlled)
	{
		m_pAIAnimationComponent.reset( new CAIAnimationComponent(pEntityScript) );
	}

#if ENABLE_PLAYER_MODIFIABLE_VALUES_DEBUGGING
	m_modifiableValues.DbgInit(pEntity);
#endif

	m_stealthKill.Init(this);
	m_spectacularKill.Init(this);
	m_largeObjectInteraction.Init(this);

	// This must come after CActor::Init as it's that function that determines whether we're a player or an AI.
	SelectMovementHierarchy();

	m_heatController.InitWithEntity(pEntity, GetBaseHeat());

	if (m_pAnimatedCharacter)
	{
		m_pAnimatedCharacter->SetAnimationPlayerProxy(&m_animationProxy, 0);
		m_pAnimatedCharacter->SetAnimationPlayerProxy(&m_animationProxyUpper, 1);
	}

	m_pickingUpCarryObject = false;

	if (IsPlayer())
	{
		CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			pRecordingSystem->OnPlayerJoined(GetEntityId());
		}
	}

	if(gEnv->bMultiplayer)
	{
		SetMultiplayerModelName();
	}

	InitMannequinParams();
	Revive(kRFR_FromInit);

	if(IEntityRender* pProxy = pEntity->GetRenderInterface())
	{
		if(IRenderNode* pRenderNode = pProxy->GetRenderNode())
			pRenderNode->SetRndFlags(ERF_REGISTER_BY_POSITION,true);
	}

	EntityId entityId = GetEntityId();

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		IGameRulesPlayerStatsModule *playerStats = pGameRules->GetPlayerStatsModule();
		if (playerStats && IsPlayer())
		{
			playerStats->CreatePlayerStats(entityId, GetChannelId());
		}

		IGameRulesSpawningModule *spawningModule = pGameRules->GetSpawningModule();
		if (spawningModule)
		{
			spawningModule->PlayerJoined(entityId);
		}
		CRY_ASSERT_MESSAGE(spawningModule, "CPlayer::Init() failed to find required gamerules spawning module");

		IGameRulesVictoryConditionsModule *pVictoryConditions = pGameRules->GetVictoryConditionsModule();
		if (pVictoryConditions && IsPlayer())
		{
			pVictoryConditions->OnNewPlayerJoined(GetChannelId());
		}
	}

	{
		IGameRulesAssistScoringModule *assistScoringModule = pGameRules ? pGameRules->GetAssistScoringModule() : NULL;
		if (assistScoringModule)
		{
			assistScoringModule->RegisterAssistTarget(entityId);
		}
	}

	if (IsPlayer())
	{
		pEntity->SetFlags(pEntity->GetFlags() | ENTITY_FLAG_UPDATE_HIDDEN);
	}

	if(gEnv->bMultiplayer || IsClient())
	{
		EnterPlayerPlugin(&m_currentlyTargettingPlugin);		
	}

	DisableStumbling();

	m_logPingTimer = 0.0f;

	UpdateAutoDisablePhys(m_stats.isRagDoll);

	if (gEnv->bServer && gEnv->bMultiplayer && pGameRules)
	{
		IGameRulesPlayerStatsModule *playerStats = pGameRules->GetPlayerStatsModule();
		if (playerStats)
		{
			const SGameRulesPlayerStat *stats = playerStats->GetPlayerStats(entityId);
			if (stats)
			{
				if (stats->flags & SGameRulesPlayerStat::PLYSTATFL_CANTSPAWNTHISROUND)
				{	
					CryLog("CPlayer::Init() creating a player who is NOT allowed to spawn this round. So setting them to spectator state ingame so they can spectate whilst they wait");
					SetSpectatorState(CActor::eASS_Ingame);

					IGameRulesSpectatorModule *specMod = pGameRules->GetSpectatorModule();
					specMod->ChangeSpectatorModeBestAvailable(this, true);
				}
			}
		}
	}

	if (gEnv->bMultiplayer && !gEnv->IsEditor())
	{
		uint16 channelId = GetChannelId();

		CGameLobby *pGameLobby = g_pGame->GetGameLobby();
		if(pGameLobby && channelId)
		{
			pGameLobby->OnPlayerSpawned(channelId);
		}
	}

	if(m_bPlayIntro && IsClient())
		StateMachineHandleEventMovement( PLAYER_EVENT_INTRO_START );
	else
		StateMachineHandleEventMovement( PLAYER_EVENT_INTRO_FINISHED );

	return true;
}

void CPlayer::PostInit( IGameObject * pGameObject )
{
	RegisterGOEvents( *this, *GetGameObject() );

	CCCPOINT(PlayerState_PostInit);

	CActor::PostInit(pGameObject);

	//--- Update animationPlayerProxies & toggle the part visibility for separate character shadow casting
	UpdateThirdPersonState();

	ResetAnimationState();

	if (gEnv->bMultiplayer)
	{
		EUpdateEnableCondition updateCondition = eUEC_WithoutAI;
		if(!IsPlayer())
		{
			updateCondition = eUEC_Always;
		}
		GetGameObject()->SetUpdateSlotEnableCondition( this, 0, updateCondition );
		//GetGameObject()->ForceUpdateExtension(this, 0);
	}

	if (IsPlayer() || gEnv->bMultiplayer)
	{
		SetUpInventorySlotsAndCategories();
	}

	// Register for things like tagnames.
	// Only send this event here when we're not doing a team game.
	// Register players for team games in OnSetTeam.
	CGameRules *pGameRules = g_pGame->GetGameRules();
	if( pGameRules && pGameRules->GetTeamCount()<=1 )
	{
		RegisterOnHUD();
	}
}

void CPlayer::RegisterOnHUD( void )
{
	if( IsPlayer() && !m_registeredOnHUD && gEnv->pRenderer )
	{
		// For all remote players
		CGameRules* pGameRules = g_pGame->GetGameRules(); 
		IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
		if ( (pStateModule && pStateModule->GetGameState() != IGameRulesStateModule::EGRS_Intro) || 
				 !pGameRules->IsIntroSequenceRegistered() || pGameRules->IntroSequenceHasCompletedPlaying())
		{
			// This should only ever be called once and not every team change.
			SHUDEvent hudEvent_remoteEnterGame(eHUDEvent_OnEnterGame_RemotePlayer);
			hudEvent_remoteEnterGame.AddData(static_cast<int>(GetEntityId()));
			CHUDEventDispatcher::CallEvent(hudEvent_remoteEnterGame);

			m_registeredOnHUD = true;
		}
	}
}

void CPlayer::ReloadClientXmlData()
{
	CRY_ASSERT_MESSAGE(IsClient(), "This function should be called only for the client!");

	ReadDataFromXML(true);
}


void CPlayer::ReadDataFromXML(bool isClientReloading /*= false*/)
{
	CActor::ReadDataFromXML(isClientReloading);

	// Actor/C++ class params
	const IItemParamsNode* pParamNode = GetActorParamsNode();
	if (pParamNode)
	{
		m_playerRotationParams.Reset(pParamNode);

		// player only
		if (IsPlayer())
		{
			SHitRecoilGameEffectParams hitRecoilParams;
			m_hitRecoilGameEffect.Initialise(&hitRecoilParams);
			m_hitRecoilGameEffect.Reset(pParamNode);

			m_stealthKill.ReadXmlData(pParamNode, isClientReloading);

			CPlayerStateLedge::SetParamsFromXml(pParamNode);
			CPlayerStateSwim::SetParamsFromXml(pParamNode);
		}

		// LandBob
		const IItemParamsNode* pLandBobNode = pParamNode->GetChild("LandBob");
		if (pLandBobNode)
		{
			SLandBobParams lbParams;
			pLandBobNode->GetAttribute("maxTime", lbParams.maxTime);
			pLandBobNode->GetAttribute("maxBob", lbParams.maxBob);
			pLandBobNode->GetAttribute("maxFallDist", lbParams.maxFallDist);

			m_pAnimatedCharacter->EnableLandBob(lbParams);
		}

		m_largeObjectInteraction.ReadXmlData( pParamNode, isClientReloading );

		// Sprint Stamina settings
		CSprintStamina::LoadSettings( pParamNode->GetChild( "SprintStamina" ) );
	
		// Thermal vision
		const IItemParamsNode* pThermalVisionNode = pParamNode->GetChild("ThermalVision");
		if(pThermalVisionNode)
		{
			const char* paramsName = "SP";
			if(gEnv->bMultiplayer)
			{
				paramsName = "MP";
			}
			const IItemParamsNode* pThermalVisionParams = pThermalVisionNode->GetChild(paramsName);
			if(pThermalVisionParams)
			{
				pThermalVisionParams->GetAttribute("heat",m_thermalVisionBaseHeat);
			}
		}
	}	
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Could not find actor params xml for the player.");
	}

	// Entity class params
	const IItemParamsNode* pEntityClassParamsNode = GetEntityClassParamsNode();
	if (pEntityClassParamsNode)
	{
		if (!IsPoolEntity())
			m_spectacularKill.ReadXmlData(pEntityClassParamsNode);

		if (!m_pPickAndThrowProxy || m_pPickAndThrowProxy->NeedsReloading())
		{
			m_pPickAndThrowProxy = CPickAndThrowProxy::Create(this, pEntityClassParamsNode);
		}
	}
	else
	{
		// No params, no physical proxy
		m_pPickAndThrowProxy.reset();
	}

	if(m_pPlayerTypeComponent)
	{
		const IItemParamsNode* pFollowCameraSettings = pParamNode ? pParamNode->GetChild("FollowCameraSettings") : NULL;
		if (pFollowCameraSettings)
		{
			m_pPlayerTypeComponent->InitFollowCameraSettings(pFollowCameraSettings);
		}
	}
}

void CPlayer::InitLocalPlayer()
{
	CRY_ASSERT(!m_pPlayerTypeComponent);

	if (m_pPlayerTypeComponent)
		return;

	// Make the player a local actor and update the third person state so the player is now in first person state
	CActor::InitLocalPlayer();
	UpdateThirdPersonState();

	CGameLobby* pGameLobby = g_pGame->GetGameLobby();
	if(pGameLobby && pGameLobby->GetSpectatorStatusFromChannelId(GetGameObject()->GetChannelId()))
	{
		RequestChangeSpectatorStatus(true);
	}

	m_pPlayerTypeComponent = new CLocalPlayerComponent(*this);

	ReloadClientXmlData();

	GetGameObject()->SetUpdateSlotEnableCondition( this, 0, eUEC_WithoutAI );

	IInteractor * pInteractor = GetInteractor();
	if (!GetGameObject()->GetUpdateSlotEnables(pInteractor, 0))
	{
		GetGameObject()->EnableUpdateSlot(pInteractor, 0);
	}
	
	if(!gEnv->bMultiplayer)
	{
		if(IPlayerProfileManager *pProfileMan = gEnv->pGameFramework->GetIPlayerProfileManager())
		{
			pProfileMan->AddListener(this, true);
		}
	}

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if (!(gEnv->bServer && g_pGame->IsGameSessionHostMigrating()))
	{
		CRY_ASSERT(!m_pLocalPlayerInteractionPlugin);
		if(!m_pLocalPlayerInteractionPlugin)
		{
			m_pLocalPlayerInteractionPlugin = new CPlayerPlugin_Interaction();
		}
		CRY_ASSERT(m_pLocalPlayerInteractionPlugin);

		m_pLocalPlayerInteractionPlugin->SetOwnerPlayer(this);
		EnterPlayerPlugin(m_pLocalPlayerInteractionPlugin);

		if(gEnv->bMultiplayer && !m_pInteractiveEntityMonitorPlugin)
		{
			m_pInteractiveEntityMonitorPlugin = new CPlayerPlugin_InteractiveEntityMonitor();
			m_pInteractiveEntityMonitorPlugin->SetOwnerPlayer(this);
			EnterPlayerPlugin(m_pInteractiveEntityMonitorPlugin);
		}

		m_playerHealthEffect.InitialSetup(this);
		m_playerHealthEffect.ReStart(); // ReStart to cancel any previous logic in the event of a level reload

		IGameRulesObjectivesModule * pObjectives = pGameRules ? pGameRules->GetObjectivesModule() : NULL;
		if((gEnv->bMultiplayer
			&& pObjectives)
			&& !pObjectives->MustShowHealthEffect(GetEntityId()))
		{
			m_playerHealthEffect.Stop();
		}

		m_pIEntityAudioComponent = GetEntity()->GetOrCreateComponent<IEntityAudioComponent>();
		//m_pIEntityAudioComponent->SetFlags(m_pIEntityAudioComponent->GetFlags()|IEntityAudioComponent::FLAG_DELEGATE_SOUND_ANIM_EVENTS);

		if (m_pIEntityAudioComponent != nullptr)
		{
			// This enables automatic updates of the "absolute_velocity" audio parameter on the Player Character.
			m_pIEntityAudioComponent->SetSwitchState(CryAudio::AbsoluteVelocityTrackingSwitchId, CryAudio::OnStateId);
		}

		m_netPlayerProgression.OwnClientConnected();

		if(pGameRules)
		{
			CBattlechatter* pBattlechatter = pGameRules->GetBattlechatter();
			if(pBattlechatter)
			{
				pBattlechatter->SetLocalPlayer(this);
			}
		}

		CRY_ASSERT_TRACE (m_playerCamera == NULL, ("%s '%s' already has a camera instance when allocating another = memory leak!", GetEntity()->GetClass()->GetName(), GetEntity()->GetName()));
		m_playerCamera = new CPlayerCamera(*this);
	}

	m_pSprintStamina = new CSprintStamina();

	if (g_pGame->IsGameSessionHostMigrating() && pGameRules)
	{
		pGameRules->OnHostMigrationGotLocalPlayer(this);
	}

	CGameMechanismManager::GetInstance()->Inform(kGMEvent_LocalPlayerInit);

	m_pPlayerTypeComponent->InitSoundmoods();

	if (gEnv->bMultiplayer)
	{
		if(pGameLobby)
		{
			pGameLobby->OnHaveLocalPlayer();
		}

		if(m_timeFirstSpawned <= 0.0f)
		{
			IActionFilter* pFilter = g_pGameActions->FilterNotYetSpawned();
			if(pFilter && !pFilter->Enabled())
			{
				pFilter->Enable(true);
			}
		}
	}

	IEntity *pEntity = GetEntity();
	// These flags are needed for the correct handling of the merged mesh collision sounds
	pEntity->SetFlagsExtended(pEntity->GetFlagsExtended() | ENTITY_FLAG_EXTENDED_NEEDS_MOVEINSIDE | ENTITY_FLAG_EXTENDED_CAN_COLLIDE_WITH_MERGED_MESHES);

	GetGameObject()->SetAutoDisablePhysicsMode(eADPM_Never);
	CGodMode::GetInstance().ClearCheckpointData();
	if (gEnv->bMultiplayer)
	{
		CryLog("Local player name = %s", GetEntity()->GetName());
	}

	if (!(g_pGame->IsGameSessionHostMigrating() && gEnv->bServer))
	{
		FullyUpdateActorModel();

		if (gEnv->bMultiplayer)
		{
			CryFixedStringT<64> signalName;
			signalName.Format("Player_Footstep_Gear_MP_Team%d%s", CLAMP(pGameRules->GetTeam(GetEntityId()), 1, 2), IsClient() ? "_FP" : "");
			m_sounds[ESound_Gear_Run].audioSignalPlayer.SetSignal(signalName.c_str());
		}

		ResetScreenFX();
		ResetFPView();
		UnRegisterInAutoAimManager();

		if (gEnv->bMultiplayer)
		{
			const SHUDEvent hudevent_rescanActors(eHUDEvent_RescanActors);
			CHUDEventDispatcher::CallEvent(hudevent_rescanActors);

			OnLocalPlayerChangeTeam();

			CreateInputClass(true);
		}

		PhysicalizeLocalPlayerAdditionalParts();

		StateMachineHandleEventMovement(SStateEvent(PLAYER_EVENT_BECOME_LOCALPLAYER));
	}

	pGameRules->OwnClientConnected_NotifyListeners();

	if (IGameRulesStateModule* pStateModule = pGameRules->GetStateModule())
	{
		pStateModule->OwnClientEnteredGame(*this);
	}

	// Now we *finally* have correct info for client actor id + team id, we need to force a refresh of all players
	CTeamVisualizationManager* pTeamVisManager = g_pGame->GetGameRules()->GetTeamVisualizationManager();
	if (pTeamVisManager)
	{
		pTeamVisManager->OnPlayerTeamChange(GetEntityId());
	}

	// Just create for local player instance, don't think AI needs a 
	// vehicle client, and remote representations of players shouldn't either
	IVehicleSystem* pVehicleSystem = g_pGame->GetIGameFramework()->GetIVehicleSystem();
	IVehicleClient *pVehicleClient = pVehicleSystem->GetVehicleClient();
	m_pVehicleClient = static_cast<CVehicleClient*>(pVehicleClient);
	assert(m_pVehicleClient);
}

bool CPlayer::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	GetGameObject()->UnRegisterExtForEvents( this, NULL, 0 );

	bool bResult = CActor::ReloadExtension(pGameObject, params);

	RegisterGOEvents( *this, *pGameObject );

	m_animationProxy.OnReload();
	m_animationProxyUpper.OnReload();

	if (m_pPickAndThrowProxy)
		m_pPickAndThrowProxy->OnReloadExtension();

	UpdateAutoDisablePhys(m_stats.isRagDoll);

	if (m_isAIControlled)
	{
		m_pAIAnimationComponent.reset(new CAIAnimationComponent(GetEntity()->GetScriptTable()));
	}

	return bResult;
}

void CPlayer::PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	CActor::PostReloadExtension(pGameObject, params);

	InitMannequinParams();

	if (m_pAnimatedCharacter)
	{
		m_pAnimatedCharacter->SetAnimationPlayerProxy(&m_animationProxy, 0);
		m_pAnimatedCharacter->SetAnimationPlayerProxy(&m_animationProxyUpper, 1);
	}

	Reset(false);	// Functionality that also needs resetting on re-entry from editor

	Revive(kRFR_FromInit);

	// Refresh third person setting
	m_stats.isThirdPerson = !m_isClient;
	UpdateThirdPersonState();

	ResetAnimationState();

	if (m_pHitDeathReactions && !IsPoolEntity())
		m_pHitDeathReactions->OnActorReused();

	CGameRules *pGameRules = g_pGame->GetGameRules();
	IGameRulesSpawningModule *spawningModule = pGameRules ? pGameRules->GetSpawningModule() : NULL;
	if (spawningModule)
	{
		spawningModule->PlayerLeft(params.prevId);
		spawningModule->PlayerJoined(params.id);
	}

	CRY_ASSERT_MESSAGE(spawningModule, "CPlayer::Init() failed to find required gamerules spawning module");

	{
		IGameRulesAssistScoringModule *assistScoringModule = pGameRules ? pGameRules->GetAssistScoringModule() : NULL;
		if (assistScoringModule)
		{
			assistScoringModule->UnregisterAssistTarget(params.prevId);
			assistScoringModule->RegisterAssistTarget(params.id);
		}
	}
}

void CPlayer::ResetAnimationState()
{
	if (IsAIControlled())
	{
		m_animActionAIMovementSettings.turnParams = m_params.AITurnParams;
	}
}

void CPlayer::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == ENTITY_EVENT_XFORM)
	{
		int flags = (int)event.nParam[0];
		if (flags & ENTITY_XFORM_ROT && !(flags & (ENTITY_XFORM_USER|ENTITY_XFORM_PHYSICS_STEP)))
		{
			Quat rotation = GetEntity()->GetRotation();

			if (flags & (ENTITY_XFORM_TRACKVIEW|ENTITY_XFORM_EDITOR|ENTITY_XFORM_TIMEDEMO))
			{
				m_pPlayerRotation->SetViewRotation( rotation );
			}

			if ((m_linkStats.linkID == 0) || ((m_linkStats.flags & SLinkStats::LINKED_FREELOOK) == 0))
			{
				m_pPlayerRotation->ResetLinkedRotation( rotation );
			}
		}

		if ((!gEnv->IsEditor() || gEnv->IsEditorGameMode()) && !(flags & (ENTITY_XFORM_USER|ENTITY_XFORM_PHYSICS_STEP)) && IsClient())
		{
			gEnv->p3DEngine->OnCameraTeleport();
		}

		if (m_timedemo && !(flags&ENTITY_XFORM_TIMEDEMO))
		{
			// Restore saved position.
			GetEntity()->SetPos(m_lastKnownPosition);
		}

		m_lastKnownPosition = GetEntity()->GetPos();
	}

	CActor::ProcessEvent(event);

	switch (event.event)
	{
	case ENTITY_EVENT_TIMER:
		{
			if (event.nParam[0] == REFILL_AMMO_TIMER_ID)
			{
				RefillAmmoDone();
			}
		}
		break;
	case ENTITY_EVENT_PREPHYSICSUPDATE:
		{
			bool client(IsClient());
			if (m_pPlayerInput.get())
			{
				m_pPlayerInput->PreUpdate();
			}
			else
			{
				CreateInputClass(client);
			}

			PrePhysicsUpdate();
		}
		break;
	case ENTITY_EVENT_PRE_SERIALIZE:
		{
			SEntityEvent resetEvent(ENTITY_EVENT_RESET);
			ProcessEvent(resetEvent);
		}
		break;
	case ENTITY_EVENT_START_GAME:
		{
			if(gEnv->IsEditor() && IsClient())
				SupressViewBlending();
			break;
		}

	case ENTITY_EVENT_RESET:
		{
			if(gEnv->IsEditor())
			{
				if( event.nParam[0] )
				{
					if (IsClient())
					{
						assert( GetEntityId() == gEnv->pGameFramework->GetClientActor()->GetEntityId() );
						SHUDEvent hudEvent_initLocalPlayerEditor(eHUDEvent_OnInitLocalPlayer);
						hudEvent_initLocalPlayerEditor.AddData(static_cast<int>(GetEntityId()));
						CHUDEventDispatcher::CallEvent(hudEvent_initLocalPlayerEditor);
					}
					else
					{
						RegisterInAutoAimManager();
					}
				}
				else if( event.nParam[0] == 0 )
				{
					if( IsClient() )
					{
						OnEndCutScene();
					}
				}
			}
			if (m_pLocalPlayerInteractionPlugin)
			{
				m_pLocalPlayerInteractionPlugin->Reset();
			}
		}
		break;
	case ENTITY_EVENT_DONE:
		{
			//NB IsClient doesn't work at this point
			if (m_pLocalPlayerInteractionPlugin)
			{
				bool bIsRealActor = true;

				CGameRules *pGameRules = g_pGame->GetGameRules();
				if (pGameRules)
				{
					bIsRealActor = pGameRules->IsRealActor(GetEntityId());
					if (bIsRealActor)
					{
						CBattlechatter* pBattlechatter = pGameRules->GetBattlechatter();
						if(pBattlechatter)
						{
							pBattlechatter->SetLocalPlayer(NULL);
						}
					}
				}

				if (bIsRealActor)
				{
					LeavePlayerPlugin(m_pLocalPlayerInteractionPlugin);
					m_pLocalPlayerInteractionPlugin->SetOwnerPlayer(NULL);
					CCCPOINT(PlayerState_LocalPlayerBeingDestroyed);
					CGameMechanismManager::GetInstance()->Inform(kGMEvent_LocalPlayerDeinit);
				}
			}
			else
			{
				CCCPOINT(PlayerState_NonLocalPlayerBeingDestroyed);
				SHUDEvent hudEvent_remoteLeaveGame(eHUDEvent_OnLeaveGame_RemotePlayer);
				hudEvent_remoteLeaveGame.AddData(static_cast<int>(GetEntityId()));
				CHUDEventDispatcher::CallEvent(hudEvent_remoteLeaveGame);
			}

			LeaveAllPlayerPlugins();

			// If we're currently stealth killing someone, make sure that player is killed
			if (gEnv->bServer && m_stealthKill.IsBusy())
			{
				m_stealthKill.ForceFinishKill();
			}
			// Force exit pick and throw
			CEnvironmentalWeapon *pEnvWeapon = static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(m_stats.pickAndThrowEntity, "EnvironmentalWeapon"));
			if( pEnvWeapon )
			{
				pEnvWeapon->ForceDrop();
			}

			IVehicle *pVehicle = GetLinkedVehicle();
			if (pVehicle)
			{
				pVehicle->ClientEvictPassenger(this);
				GetEntity()->Hide(true);
			}
		}
		break;
	case ENTITY_EVENT_ATTACH_THIS:
		StateMachineHandleEventMovement( PLAYER_EVENT_ATTACH );
		break;
	case ENTITY_EVENT_DETACH_THIS:
		StateMachineHandleEventMovement( PLAYER_EVENT_DETACH );
		break;
	case ENTITY_EVENT_ENABLE_PHYSICS:
		{
			if(!m_stats.isRagDoll && event.nParam[0]) //If we've just received a new articulated entity and we're not ragdolling, reset mass to 0
			{
				ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
				IPhysicalEntity* pCharacterPhysics = pCharacter ? pCharacter->GetISkeletonPose()->GetCharacterPhysics() : NULL;
				if (pCharacterPhysics != NULL)
				{
					pe_simulation_params simParams;
					simParams.mass = 0.0f;
					pCharacterPhysics->SetParams(&simParams);
				}
			}
		}
		break;
	case ENTITY_EVENT_SET_AUTHORITY:
		{
			const bool auth = event.nParam[0] ? true : false;
			// we've been given authority of this entity, mark the physics as changed
			// so that we send a current position, failure to do this can result in server/client
			// disagreeing on where the entity is. most likely to happen on restart
			if (auth)
			{
				CHANGED_NETWORK_STATE(this, eEA_Physics | ASPECT_RANK_CLIENT);

				if (g_pGame->IsGameSessionHostMigrating())
				{
					// If we're migrating, we've probably set our selected item before we were allowed to, resend it here
					CHANGED_NETWORK_STATE(this, ASPECT_CURRENT_ITEM);
				}
			}
		}
	}
}

uint64 CPlayer::GetEventMask() const
{
	return CActor::GetEventMask()
		| ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_TIMER)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_PREPHYSICSUPDATE)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_PRE_SERIALIZE)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_START_GAME)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_RESET)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_DONE)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_ATTACH_THIS)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_DETACH_THIS)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_ENABLE_PHYSICS)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_SET_AUTHORITY);

}

void CPlayer::SetChannelId(uint16 id)
{
	CActor::SetChannelId(id);
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::SetViewRotation( const Quat &rotation )
{
	m_pPlayerRotation->SetViewRotation( rotation );
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::SetViewRotationAndKeepBaseOrientation( const Quat &rotation )
{
	m_pPlayerRotation->SetViewRotationAndKeepBaseOrientation( rotation );
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::SetForceLookAt(const Vec3& lookAtDirection, const bool bForcedLookAtBlendingEnabled)
{
	m_pPlayerRotation->SetForceLookAt(lookAtDirection, bForcedLookAtBlendingEnabled);
}

//////////////////////////////////////////////////////////////////////////
Quat CPlayer::GetViewRotation() const
{
	return m_pPlayerRotation->GetViewQuatFinal();
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::AddViewAngles( const Ang3 &angles )
{
	m_pPlayerRotation->AddViewAngles(angles);
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::EnableTimeDemo( bool bTimeDemo )
{
	m_timedemo = bTimeDemo;
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::AddViewAngleOffsetForFrame(const Ang3 &offset)
{
	if (!m_timedemo)
	{
		m_pPlayerRotation->AddViewAngleOffsetForFrame(offset);
	}
}

void CPlayer::Update(SEntityUpdateContext& ctx, int updateSlot)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

#if ENABLE_RMI_BENCHMARK
	if ( gEnv->bMultiplayer && IsClient() && ( g_pGameCVars->g_RMIBenchmarkInterval > 0 ) )
	{
		int64                   now = gEnv->pTimer->GetAsyncTime().GetMilliSecondsAsInt64();

		if ( now - m_RMIBenchmarkLast >= g_pGameCVars->g_RMIBenchmarkInterval )
		{
			m_RMIBenchmarkLast = now;
			GetGameObject()->InvokeRMI( SvBenchmarkPing(), SRMIBenchmarkParams( eRMIBM_Ping, GetEntityId(), m_RMIBenchmarkSeq, g_pGameCVars->g_RMIBenchmarkTwoTrips ? true : false ), eRMI_ToServer );
			m_RMIBenchmarkSeq = ( m_RMIBenchmarkSeq + 1 ) % RMI_BENCHMARK_MAX_RECORDS;
		}
	}
#endif

#if ENABLE_PLAYER_MODIFIABLE_VALUES_DEBUGGING
	m_modifiableValues.DbgTick();
#endif

#if ENABLE_PLAYER_HEALTH_REDUCTION_POPUPS
	if (g_pGameCVars->g_displayPlayerDamageTaken && !IsClient() && m_healthAtStartOfUpdate > 0.f && m_healthAtStartOfUpdate > m_health.GetHealth())
	{
		int damageTaken = (int)(0.5f + m_healthAtStartOfUpdate - max(0.f, m_health.GetHealth()));
		if (damageTaken > 0)
		{
			Vec3 velocity(cry_random(-1.0f, 1.0f), cry_random(-1.0f, 1.0f), cry_random(1.0f, 2.0f));
			CryWatch3DAdd(string().Format("%d", damageTaken), GetLocalEyePos() + GetEntity()->GetWorldPos(), 2.f, & velocity, 3.f);
		}
	}
	m_healthAtStartOfUpdate = m_health.GetHealth();
#endif

#ifndef _RELEASE
	if (g_pGameCVars->g_tpdeathcam_dbg_alwaysOn && IsClient())
	{
		if (!IsThirdPerson())
		{
			ToggleThirdPerson();
		}
	}
#endif

#if 0
	if (gEnv->bServer && !IsClient() && IsPlayer())
	{
		if (INetChannel *pNetChannel=m_pGameFramework->GetNetChannel(GetChannelId()))
		{
			if (pNetChannel->GetContextViewState()>=eCVS_InGame)
			{
				SufferingHighLatency(pNetChannel->IsSufferingHighLatency(gEnv->pTimer->GetAsyncTime()));
			}
		}
	}
#endif
	
	const float frameTime = ctx.fFrameTime;

#if GAME_CONNECTION_TRACKER_ENABLED
	if( gEnv->bMultiplayer )
	{
		//regularly log our ping so we can discover if we're having bad performance
		if( m_logPingTimer > LOG_PING_FREQUENCY )
		{
			m_logPingTimer = 0.0f;

			CGameLobby* pGameLobby = g_pGame->GetGameLobby();

			if( pGameLobby && pGameLobby->GetState() == eLS_Game )
			{
				CryPing ping;
				SCryMatchMakingConnectionUID conUID = pGameLobby->GetConnectionUIDFromChannelID( GetChannelId() );
				gEnv->pNetwork->GetLobby()->GetMatchMaking()->GetSessionPlayerPing( conUID, &ping );
				CryUserID guid = pGameLobby->GetUserIDFromChannelID( GetChannelId() );

				CGameConnectionTracker* pGameConnectionTracker = g_pGame->GetGameConnectionTracker();
				if(pGameConnectionTracker && guid.IsValid())
				{
					pGameConnectionTracker->Event_Ping(guid,conUID,ping); 
				}
			}
		}
		else
		{
			m_logPingTimer += frameTime;
		}
	}
#endif
	
#ifdef STATE_DEBUG
	const bool shouldDebug = (s_StateMachineDebugEntityID == GetEntityId());
#else
	const bool shouldDebug = false;
#endif

	StateMachineUpdateMovement( frameTime, shouldDebug );

	m_heatController.Update(frameTime);

	UpdatePlayerPlugins(frameTime);

	StateMachineHandleEventMovement( SStateEventUpdate( frameTime ) );

	// old state machine returned here.
	// not doing that breaks a lot of assumptions now
	// TODO: needs to be stateful also.
	if( IsDead() )
	{
		return;
	}

	m_stats.zeroVelocityForTime = (float)__fsel(m_stats.zeroVelocityForTime, m_stats.zeroVelocityForTime, 0.f) - ctx.fFrameTime;

	CActor::Update(ctx,updateSlot);

	float fHealth = m_health.GetHealth();

	CWeapon* pCurrentWeapon = GetWeapon(GetCurrentItemId());

	bool client(IsClient());
	if ( !m_stats.isRagDoll )
	{
		UpdateStats(frameTime);

		if(m_pPlayerTypeComponent)
		{
			m_pPlayerTypeComponent->UpdateStumble(frameTime);
		}
	}
	else
	{
		WATCH_ACTOR_STATE ("Not updating (ragdoll=%d health=%8.2f)", m_stats.isRagDoll, fHealth);
	}

	if (m_pPlayerInput.get())
	{
		m_pPlayerInput->Update();
	}
	else
	{
		CreateInputClass(client);
	}
	
	if(client && m_isPlayer)
	{
		UpdateHealthRegeneration(fHealth, frameTime);
		UpdateClient(frameTime);
	}

	UpdateFrameMovementModifiersAndWeaponStats(pCurrentWeapon, ctx.fCurrTime);

	if (gEnv->bServer && m_ragdollTime > 0.0f)
	{
		float fNewRagdollTime = m_ragdollTime - frameTime;

		if (fNewRagdollTime <= 0.0f)
		{
			if (GetGameObject()->GetAspectProfile(eEA_Physics) == eAP_Alive)
			{
				GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);
			}

			fNewRagdollTime = 0.0f;
		}

		m_ragdollTime = fNewRagdollTime;
	}

	{
		CRY_PROFILE_REGION(PROFILE_GAME, "Player Update :: UpdateListeners");

		TPlayerUpdateListeners::iterator it = m_playerUpdateListeners.begin();
		TPlayerUpdateListeners::iterator end = m_playerUpdateListeners.end();
		for (; it!=end; ++it)
		{
			(*it)->Update(frameTime);
		}
	}

	if (m_pHitDeathReactions)
	{
		m_pHitDeathReactions->Update(frameTime);
	}

	if (m_pSprintStamina)
	{
		float staminaConsumptionScale = 1.0f;
		const CSprintStamina::UpdateContext updateCtx( GetEntityId(), frameTime, staminaConsumptionScale, IsSprinting(), client );

		m_pSprintStamina->Update( updateCtx );

		if (m_pSprintStamina->HasChanged())
			CALL_PLAYER_EVENT_LISTENERS(OnSprintStaminaChanged(this, m_pSprintStamina->Get()));

#ifdef DEBUG_SPRINT_STAMINA
		m_pSprintStamina->Debug( GetEntity() );
#endif
	}

	if (m_isInWater != m_playerStateSwim_WaterTestProxy.IsInWater())
	{
		m_isInWater = m_playerStateSwim_WaterTestProxy.IsInWater();
		CALL_PLAYER_EVENT_LISTENERS(OnIsInWater(this, m_isInWater));
	}

	if (m_isHeadUnderWater != m_playerStateSwim_WaterTestProxy.IsHeadUnderWater())
	{
		m_isHeadUnderWater = m_playerStateSwim_WaterTestProxy.IsHeadUnderWater();
		CALL_PLAYER_EVENT_LISTENERS(OnHeadUnderwater(this, m_isHeadUnderWater));
	}

	if (m_fOxygenLevel != m_playerStateSwim_WaterTestProxy.GetOxygenLevel())
	{
		m_fOxygenLevel = m_playerStateSwim_WaterTestProxy.GetOxygenLevel();
		CALL_PLAYER_EVENT_LISTENERS(OnOxygenLevelChanged(this, m_fOxygenLevel));
	}

	if(gEnv->bMultiplayer)
	{
		if (!gEnv->bServer && client)
		{
			CheckSendXPChanges();
		}
	}

#ifndef _RELEASE
	if (client)
	{
		if (g_pGameCVars->g_animatorDebug)
		{
			ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
			IAttachmentManager* pAttachMan = pCharacter->GetIAttachmentManager();
			ISkeletonPose* pSkel = pCharacter->GetISkeletonPose();
			IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
			int32 jntID = rIDefaultSkeleton.GetJointIDByName("Bip01 LHand2Weapon_IKBlend");
			IAttachment* pLeftAttachment = pAttachMan->GetInterfaceByName("left_weapon");
			IAttachment* pRightAttachment = pAttachMan->GetInterfaceByName("weapon");

			IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();

			SAuxGeomRenderFlags currRenderFlags = pAuxGeom->GetRenderFlags();
			EAuxGeomPublicRenderflags_DepthTest depthTest = currRenderFlags.GetDepthTestFlag();

			currRenderFlags.SetDepthTestFlag(e_DepthTestOff);
			pAuxGeom->SetRenderFlags(currRenderFlags);

			if(pLeftAttachment)
			{
				QuatTS leftQuatt = pLeftAttachment->GetAttWorldAbsolute();

				pAuxGeom->DrawSphere(leftQuatt.t, 0.02f, ColorB(0,0,255,255));
				pAuxGeom->DrawLine(leftQuatt.t, ColorB(0,0,255,255), leftQuatt.t + leftQuatt.GetColumn1(), ColorB(0,0,255,255));
			}

			if(pRightAttachment)
			{
				QuatTS rightQuatt = pRightAttachment->GetAttWorldAbsolute();

				pAuxGeom->DrawSphere(rightQuatt.t, 0.02f, ColorB(255,0,0,255));
				pAuxGeom->DrawLine(rightQuatt.t, ColorB(255,0,0,255), rightQuatt.t + rightQuatt.GetColumn1(), ColorB(255,0,0,255));
			}

			if (jntID >= 0)
			{
				float blendFactor = pSkel->GetRelJointByID(jntID).t.x;
				volatile static float XPOS = 500.0f;
				volatile static float YPOS = 100.0f;
				float offColor[4] = { 0.2f, 0.2f, 0.2f, 1 };
				float onColor[4]  = { 0.0f, 1.0f, 0.0f, 1 };

				IRenderAuxText::Draw2dLabel(XPOS, YPOS, 2.5f, (blendFactor > 0.0001f) ? onColor : offColor, false, "LHand IK Blend %.2f", blendFactor);
			}

			currRenderFlags.SetDepthTestFlag(depthTest);
			pAuxGeom->SetRenderFlags(currRenderFlags);
		}

		static int sHideArms = 0;

		if(g_pGameCVars->g_hideArms != sHideArms)
		{
			ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
			IAttachmentManager* pAttachMan = pCharacter->GetIAttachmentManager();

			IAttachment* pUpperbodyAttachment = pAttachMan->GetInterfaceByName("upperbody");

			if(pUpperbodyAttachment)
			{
				pUpperbodyAttachment->HideAttachment(g_pGameCVars->g_hideArms);
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, "upperbody attachment not found while hiding arms");
			}


			sHideArms = g_pGameCVars->g_hideArms;
		}

#ifdef PICKANDTHROWWEAPON_DEBUGINFO
		if (g_pGameCVars->pl_pickAndThrow.debugDraw!=0)
		{
			const char* const name = "PickAndThrowWeapon";
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( name );
			EntityId pickAndThrowWeaponId = GetInventory()->GetItemByClass( pClass );
			IItem* pItem = m_pItemSystem->GetItem( pickAndThrowWeaponId );
			if (pItem)
			{
				CPickAndThrowWeapon* pPTW = static_cast<CPickAndThrowWeapon*>(pItem);
				pPTW->DebugDraw();
			}
		}
#endif //PICKANDTHROWWEAPON_DEBUGINFO

		if (g_pGameCVars->pl_debug_pickable_items != 0)
		{
			CItem* pItem = static_cast<CItem*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(GetCurrentInteractionInfo().interactiveEntityId));
			CWeapon* pWeapon = pItem ? static_cast<CWeapon*>(pItem->GetIWeapon()) : NULL;
			if (pWeapon)
			{
				pWeapon->ShowDebugInfo();
			}
		}
	}
#endif  // !_RELEASE
	
}

void CPlayer::CreateInputClass(bool client)
{
	CCCPOINT (PlayerState_CreateInputClassDuringUpdate);

	// init input systems if required
	if (client) //|| ((demoMode == 2) && this == gEnv->pGameFramework->GetIActorSystem()->GetOriginalDemoSpectator()))
	{
#if (USE_DEDICATED_INPUT)
		if ( gEnv->IsDedicated() )
		{
			m_pPlayerInput.reset( new CDedicatedInput(this) );
		}
		else if (g_pGameCVars->g_playerUsesDedicatedInput)
		{
			m_pPlayerInput.reset( new CDedicatedInput(this) );
		}
		else
#endif
		{
			m_pPlayerInput.reset( new CPlayerInput(this) );
		}
	}
	else if (GetGameObject()->GetChannelId())
	{
		m_pPlayerInput.reset( new CNetPlayerInput(this) );
	}
	else
	{
		if (gEnv->bServer)
			m_pPlayerInput.reset( new CAIInput(this) );
		else
			m_pPlayerInput.reset( new CNetPlayerInput(this) );
	}

	if (m_pPlayerInput.get())
	{
		GetGameObject()->EnablePostUpdates(this);

		if(m_pPlayerInput->GetType() == IPlayerInput::NETPLAYER_INPUT && m_recvPlayerInput.isDirty)
		{
			m_pPlayerInput->SetState(m_recvPlayerInput);
			m_recvPlayerInput.isDirty = false;
		}
	}
}

void CPlayer::CreateInputClass(IPlayerInput::EInputType inputType)
{
	m_pPlayerInput.reset();

	switch(inputType)
	{
	case IPlayerInput::PLAYER_INPUT:
		m_pPlayerInput.reset( new CPlayerInput(this) );
		break;
	case IPlayerInput::NETPLAYER_INPUT:
		m_pPlayerInput.reset( new CNetPlayerInput(this) );
		break;
	case IPlayerInput::AI_INPUT:
		m_pPlayerInput.reset( new CAIInput(this) );
		break;
	case IPlayerInput::DEDICATED_INPUT:
		m_pPlayerInput.reset( new CDedicatedInput(this) );
		break;
	default:
		break;
		assert(0);
	}
}

void CPlayer::SetTagByCRC( uint32 tagCRC, bool enable )
{
	if( IActionController* pActionController = GetAnimatedCharacter()->GetActionController() )
	{
		SAnimationContext &animContext = pActionController->GetContext();
		animContext.state.SetByCRC( tagCRC, enable );
	}
}

void CPlayer::UpdateAnimationState(const SActorFrameMovementParams &frameMovementParams)
{
	//--- Update variable scope contexts
	CWeapon *pWeapon = GetWeapon(GetCurrentItemId());
	ICharacterInstance *pICharInst = pWeapon ? pWeapon->GetEntity()->GetCharacter(0) : NULL;
	IActionController *pActionController = GetAnimatedCharacter()->GetActionController();
	IMannequin &mannequinSys = gEnv->pGameFramework->GetMannequinInterface();

	if (IsAIControlled() && pActionController)
	{
		UpdateAIAnimationState(frameMovementParams, pWeapon, pICharInst, pActionController, mannequinSys);
	}
	else if (IsPlayer() && pActionController)
	{
		SAnimationContext &animContext = pActionController->GetContext();

		//--- Update tags
		bool isOutOfAmmo = false;
			
		if (pWeapon)
		{		
			int firemodeIdx = pWeapon->GetCurrentFireMode();
			IFireMode *pFireMode = pWeapon->GetFireMode(firemodeIdx);
			if (pFireMode)
			{
			
				IEntityClass* pAmmoClass = pFireMode->GetAmmoType();
				int weaponAmmoCount = pWeapon->GetAmmoCount(pAmmoClass);
				int inventoryAmmoCount = pWeapon->GetInventoryAmmoCount(pAmmoClass);

				isOutOfAmmo = ((weaponAmmoCount + inventoryAmmoCount) == 0);
			}
		}

		if (!animContext.state.GetDef().IsGroupSet(animContext.state.GetMask(), PlayerMannequin.tagGroupIDs.item))
		{
			animContext.state.Set(PlayerMannequin.tagIDs.nw, true);
			animContext.state.SetGroup(PlayerMannequin.tagGroupIDs.zoom, TAG_ID_INVALID);
			animContext.state.SetGroup(PlayerMannequin.tagGroupIDs.firemode, TAG_ID_INVALID);
		}

		animContext.state.Set(PlayerMannequin.tagIDs.outOfAmmo, isOutOfAmmo);
		const bool aimEnabled = !IsSprinting() || (pWeapon && pWeapon->IsReloading());
		animContext.state.Set(PlayerMannequin.tagIDs.aiming, (frameMovementParams.aimIK || m_isPlayer) && aimEnabled);
		
		const Vec3 referenceVel = GetActorPhysics().velocity;
		float speedXY = referenceVel.GetLength2D();

		//		CryWatch("Vel (%f, %f, %f)", m_stats.velocity.x, m_stats.velocity.y, m_stats.velocity.z);
		//		CryWatch("LastRequestedVel (%f, %f, %f)", m_lastRequestedVelocity.x, m_lastRequestedVelocity.y, m_lastRequestedVelocity.z);
		//		CryWatch("DesVel (%f, %f, %f)", desiredVelocity.x, desiredVelocity.y, desiredVelocity.z);

		TagID movementTag = IsSprinting() ? PlayerMannequin.tagIDs.sprint : TAG_ID_INVALID;
		TagID moveDir = TAG_ID_INVALID;
		if (speedXY > 0.5f)
		{
			Vec3 velXY;
			velXY.Set(referenceVel.x / speedXY, referenceVel.y / speedXY, 0.0f);

			const float signedAngle	= Ang3::CreateRadZ(velXY, GetEntity()->GetForwardDir());
			const float unsignedAngle = fabs_tpl(signedAngle);
			if (unsignedAngle < gf_PI*0.25f)
			{
				moveDir = PlayerMannequin.tagIDs.forward;
			}
			else if (unsignedAngle > gf_PI*0.75f)
			{
				moveDir = PlayerMannequin.tagIDs.backward;
			}
			else if (signedAngle > 0.0f)
			{
				moveDir = PlayerMannequin.tagIDs.right;
			}
			else
			{
				moveDir = PlayerMannequin.tagIDs.left;
			}
		}

		animContext.state.SetGroup(PlayerMannequin.tagGroupIDs.moveDir, moveDir);
		animContext.state.SetGroup(PlayerMannequin.tagGroupIDs.moveSpeed, movementTag);
	}
}

void CPlayer::PrePhysicsUpdate()
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	// TODO: This whole function needs to be optimized.

	const float frameTime = gEnv->pTimer->GetFrameTime();

	if (!IsClient())
	{
		InterpLadderPosition(frameTime);
	}

	if (IVehicle* pVehicle = GetLinkedVehicle())
	{
		pVehicle->UpdatePassenger(frameTime, GetEntityId());
	}

	if (m_deferredKnockDownPending)
	{
		CommitKnockDown();
	}

	if (!m_pAnimatedCharacter)
		return;

	IEntity* pEntity = GetEntity();
	if (pEntity->IsHidden() && !(pEntity->GetFlags() & ENTITY_FLAG_UPDATE_HIDDEN))
		return;

#ifdef PLAYER_MOVEMENT_DEBUG_ENABLED
	m_movementDebug.Debug(GetEntity());
#endif

	const bool bIsClient(IsClient());

	IItem* pCurrentItem = GetCurrentItem();

	SActorFrameMovementParams frameMovementParams;
	if (m_pMovementController)
	{
		m_pMovementController->Update(frameTime, frameMovementParams);

		if (m_stats.isInBlendRagdoll || m_stealthKill.IsBusy() || m_spectacularKill.IsBusy())
		{
			frameMovementParams.deltaAngles.Set(0.0f, 0.0f, 0.0f);
		}

		SPlayerRotationParams::EAimType aimType = GetCurrentAimType();
		m_pPlayerRotation->Process(pCurrentItem, frameMovementParams,
			m_playerRotationParams.m_verticalAims[IsThirdPerson() ? 1 : 0][aimType],
			frameTime);

		if (CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem())
		{
			const EntityId playerEntityId = GetEntityId();
			pRecordingSystem->SetPlayerVelocity(playerEntityId, GetActorPhysics().velocity);
		}

		CPlayerStateUtil::UpdatePlayerPhysicsStats( *this, m_actorPhysics, frameTime );

		const SPlayerPrePhysicsData prePhysicsData( frameTime, frameMovementParams );
		const SStateEventPlayerMovementPrePhysics prePhysicsEvent( &prePhysicsData );

#ifdef STATE_DEBUG
		if (g_pGameCVars->pl_watchPlayerState >= (bIsClient? 1 : 2))
		{
			// NOTE: outputting this info here is 'was happened last frame' not 'what was decided this frame' as it occurs before the prePhysicsEvent is dispatched
			// also IsOnGround and IsInAir can possibly both be false e.g. - if you're swimming
			// May be able to remove this log now the new HSM debugging is in if it offers the same/improved functionality
			CryWatch ("%s stance=%s flyMode=%d %s %s%s%s%s", GetEntity()->GetEntityTextDescription().c_str(), GetStanceName(GetStance()), m_stats.flyMode, IsOnGround() ? "ON-GROUND" : "IN-AIR", IsThirdPerson() ? "THIRD-PERSON" : "FIRST-PERSON", IsDead() ? "DEAD" : "ALIVE", m_stats.isScoped ? " SCOPED" : "", m_stats.isInBlendRagdoll ? " FALLNPLAY" : "");
		}
#endif

		StateMachineHandleEventMovement( STATE_DEBUG_APPEND_EVENT( prePhysicsEvent ) );

		if (frameMovementParams.stance != STANCE_NULL)
		{
			SetStance(frameMovementParams.stance);
		}

		const float frameTimeInv = (float)__fsel(-frameTime, 0.0f, __fres(frameTime + FLT_EPSILON));
		m_weaponParams.inputRot = frameMovementParams.deltaAngles * frameTimeInv;
		m_weaponParams.inputMove = frameMovementParams.desiredVelocity;

		if (bIsClient)
		{
			CMovementRequest mr;
			mr.ClearLookTarget();
			m_pMovementController->RequestMovement( mr );

			g_pGame->GetAutoAimManager().SetCloseCombatSnapTarget(0, 0.f, 0.f);
		}

		m_pMovementController->PostUpdate(frameTime);

		if (m_stats.mountedWeaponID)
		{
			UpdateMountedGunController(false);
		}

		//--- STAP, delayed IK update until after the PostUpdate so that the AimDirection is up to date
		if (m_linkStats.CanDoIK() || (gEnv->bMultiplayer && GetLinkedVehicle()))
		{
			SetIK(frameMovementParams);
		}
	}

	CWeapon* pCurrentWeapon = pCurrentItem ? static_cast<CWeapon*>(pCurrentItem->GetIWeapon()) : NULL;
	UpdateEyeOffsets(pCurrentWeapon, frameTime);

	if (bIsClient)
	{
		//--- STAP must be updated here to ensure that the most recent values are there for the 
		//--- animation processing pass
		const Vec3 cameraPositionForTorso = GetFPCameraPosition(false);
		UpdateFPIKTorso(frameTime, pCurrentItem, cameraPositionForTorso);
	}

	if (!IsAIControlled())
		UpdateFPAiming();

	if (m_pImpulseHandler && g_pGameCVars->pl_impulseEnabled)
	{
		m_pImpulseHandler->Update(frameTime);
	}

	UpdateAnimationState(frameMovementParams);
}

void CPlayer::UpdateEyeOffsets(CWeapon* pCurrentWeapon, float frameTime)
{
	CHECKQNAN_VEC(m_eyeOffset);
	const bool useWhileLeanedOffsets = !IsPlayer();

	//players use a faster interpolation, and using only the Z offset. A bit different for AI.
	const float leanAmount = m_pPlayerRotation->GetLeanAmount();
	Vec3 svOffset = GetStanceViewOffset(m_stance, &leanAmount, true, useWhileLeanedOffsets);
	if (IsClient())
	{
		svOffset += GetFPCameraOffset();
	}

	CRY_ASSERT(svOffset.IsValid());

	if (!IsMigrating())
	{
		const float interpFactor = IsPlayer() ? 15.0f : 5.0f;
		Interpolate(m_eyeOffset, svOffset,interpFactor,frameTime);
	}
	else
	{
		m_eyeOffset = svOffset;
	}

	//const float XPOS = 200.0f;
	//const float YPOS = 40.0f;
	//const float FONT_SIZE = 2.0f;
	//const float FONT_COLOUR[4] = {1,1,1,1};
	//IRenderAuxText::Draw2dLabel(XPOS, YPOS, FONT_SIZE, FONT_COLOUR, false, "UpdateEyeoffset: (%f %f %f)", m_eyeOffset.x, m_eyeOffset.y, m_eyeOffset.z);

	CHECKQNAN_VEC(m_eyeOffset);

	CHECKQNAN_VEC(m_weaponOffset);
	Interpolate(m_weaponOffset, GetWeaponOffsetWithLean(pCurrentWeapon, m_stance, m_pPlayerRotation->GetLeanAmount(), m_pPlayerRotation->GetLeanPeekOverAmount(), m_eyeOffset, useWhileLeanedOffsets), 5.0f, frameTime);
	CHECKQNAN_VEC(m_weaponOffset);
}

IActorMovementController * CPlayer::CreateMovementController()
{
	return new CPlayerMovementController(this);
}

void CPlayer::SetIK( const SActorFrameMovementParams& frameMovementParams )
{
	if (!m_pAnimatedCharacter)
		return;

	IEntity * pEntity = GetEntity();
	ICharacterInstance * pCharacter = pEntity->GetCharacter(0);
	if (!pCharacter)
		return;

	SMovementState curMovementState;
	m_pMovementController->GetMovementState(curMovementState);

	// -----------------------------------
	// LOOKING
	// -----------------------------------

	if (IsPlayer())
	{
		IAnimationPoseBlenderDir *pIPoseBlenderLook = pCharacter->GetISkeletonPose()->GetIPoseBlenderLook();
		if (pIPoseBlenderLook)
		{
			pIPoseBlenderLook->SetTarget(curMovementState.eyePosition + (curMovementState.aimDirection * 5.0f));
		}
	}
	else
	{
		bool lookEnabled = frameMovementParams.lookIK && !m_stats.isGrabbed && m_pAnimatedCharacter->IsLookIkAllowed();
		Vec3 lookTarget = frameMovementParams.lookTarget;
		if (lookEnabled && !IsClient() && GetLinkedVehicle())
		{
			// look in 'vehicleviewdir' (NOTE: this code should probably be somewhere else, including the vehicleViewDir concept)
			lookTarget = curMovementState.eyePosition + 5.0f * m_vehicleViewDir;
		}

		bool animationComponentHandledLooking = false;
		if (IsAIControlled())
		{
			CAIAnimationComponent* pAiAnimationComponent = GetAIAnimationComponent();
			CRY_ASSERT(pAiAnimationComponent);

			animationComponentHandledLooking = pAiAnimationComponent->UpdateLookingState(lookEnabled, lookTarget);
		}
		
		if (!animationComponentHandledLooking)
		{
			const uint32 lookIKLayer = GetLookIKLayer(m_params);
			const float lookFov = GetLookFOV(m_params);
			m_lookAim.UpdateLook(this, pCharacter, lookEnabled, lookFov, lookTarget, lookIKLayer);
		}
	}

	// -----------------------------------
	// AIMING 
	// -----------------------------------

	Vec3 aimTarget = frameMovementParams.aimTarget;
	bool aimEnabled = frameMovementParams.aimIK && m_pAnimatedCharacter->IsAimIkAllowed();

	const float AIMIK_FADEOUT_TIME = 0.25f;

	bool allowAimIK = IsPlayer();

	if (allowAimIK)
	{
		if (IsThirdPerson()) 
		{
			Vec3 cameraPosition;
			if (IsClient())
				cameraPosition = GetViewMatrix().GetTranslation();
			else
				cameraPosition = curMovementState.eyePosition;

			const Vec3 down = Vec3(0, 0, -1);
			const float dotProd = curMovementState.aimDirection.dot(down);
			const float angle = acos(dotProd);

			SPlayerRotationParams::EAimType aimType = GetCurrentAimType();
			const SAimAccelerationParams &params = GetPlayerRotationParams().GetVerticalAimParams(aimType, !IsThirdPerson());
			const float minAngle = DEG2RAD(90 + params.angle_min);

			// Angle is relative to downwards direction, 0 degrees = straight down, 90 degrees = horizontal, 180 = straight up
			if (angle < minAngle)
			{
				Vec3 crossProd = curMovementState.aimDirection.cross(down);
				Quat adjustment = Quat::CreateRotationAA(angle - minAngle, crossProd.GetNormalized());
				curMovementState.aimDirection = Vec3Constants<float>::fVec3_OneY * (adjustment * GetViewRotation()).GetInverted();
			}
			const Vec3 cameraAimDirection = curMovementState.aimDirection.normalized() * g_pGameCVars->cl_tpvMaxAimDist;

			ray_hit hit;
			const uint32 flags = rwi_colltype_any(geom_collides) | rwi_force_pierceable_noncoll | rwi_stop_at_pierceable;
			IPhysicalEntity* pSkipEnt = pEntity->GetPhysics();      
			gEnv->pPhysicalWorld->RayWorldIntersection(cameraPosition, cameraAimDirection, ent_all, flags, &hit, 1, &pSkipEnt, 1);

			if (hit.dist >= g_pGameCVars->cl_tpvDist + 1.0f) // Target is actually in front of the gun
			{
				aimTarget = hit.pt;
			}
			else
			{
				aimTarget = cameraPosition + cameraAimDirection;
			}

			m_thirdPersonAimTarget = (aimTarget - curMovementState.weaponPosition).normalized();

			if (g_pGameCVars->cl_tpvDebugAim)
			{
				const ColorB colPlayerAim(0, 255, 0);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(curMovementState.weaponPosition, colPlayerAim, aimTarget, colPlayerAim);
				gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(hit.pt, 0.1f, colPlayerAim);
			}
		}
		else
		{
			aimTarget = curMovementState.eyePosition + curMovementState.aimDirection * 5.0f; // If this is too close the aiming will fade out.
		}

		CWeapon *pWeapon = GetWeapon(GetCurrentItemId());
		aimEnabled = !IsSprinting() || (pWeapon && pWeapon->IsReloading());
	}
	else if (!aimEnabled)
	{
		// Even for AI we need to guarantee a valid aim target position (only needed for LAW/Rockets though).
		// We should not set aimEnabled to true though, because that will force aiming for all weapons.
		// The AG templates will communicate an animation synched "force aim" flag to the animation playback.

		if (frameMovementParams.lookIK)
		{
			// Use look target
			aimTarget = frameMovementParams.lookTarget;
		}
		else
		{
			// Synthesize target
			aimTarget = curMovementState.eyePosition + curMovementState.aimDirection * 5.0f; // If this is too close the aiming will fade out.
		}
	}

	if (CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem())
	{
		AimIKInfo aiki(aimEnabled, aimTarget);
		pRecordingSystem->SetPlayerAimIK(GetEntityId(), aiki);
	}

	const int32 aimIKLayer = GetAimIKLayer(m_params);

	if (!m_params.useDynamicAimPoses)
	{
		ISkeletonPose * pSkeletonPose = pCharacter->GetISkeletonPose();

		if (IsAIControlled())
		{
			CAIAnimationComponent* pAiAnimationComponent = GetAIAnimationComponent();
			CRY_ASSERT(pAiAnimationComponent);
			pAiAnimationComponent->UpdateAimingState(pSkeletonPose, aimEnabled, aimTarget, aimIKLayer, AIMIK_FADEOUT_TIME);
		}
		else
		{
			IAnimationPoseBlenderDir *pIPoseBlenderAim = pSkeletonPose->GetIPoseBlenderAim();
			if (pIPoseBlenderAim)
			{
				pIPoseBlenderAim->SetTarget(aimTarget);
				pIPoseBlenderAim->SetPolarCoordinatesSmoothTimeSeconds(0.f);
			}

			if (ICharacterInstance * pCharacterShadow = GetShadowCharacter())
			{
				ISkeletonPose * pSkeletonPoseShadow = pCharacterShadow->GetISkeletonPose();
				IAnimationPoseBlenderDir *pIPoseBlenderAimShadow = pSkeletonPoseShadow->GetIPoseBlenderAim();
				if (pIPoseBlenderAimShadow)
				{
					pIPoseBlenderAimShadow->SetTarget(aimTarget);
					pIPoseBlenderAimShadow->SetPolarCoordinatesSmoothTimeSeconds(0.f);
				}
			}
		}
	}
	else
	{
		m_lookAim.UpdateDynamicAimPoses(this, pCharacter, m_params, aimEnabled, aimIKLayer, aimTarget, curMovementState);
	}
}

void CPlayer::UpdateView(SViewParams &viewParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

#ifndef _RELEASE
	if (g_pGameCVars->pl_debug_view != 0)
		CryLog("CPlayer::UpdateView: Pre Pos(%f, %f, %f) Rot(%f, %f, %f, %f)", viewParams.position.x, viewParams.position.y, viewParams.position.z, viewParams.rotation.v.x, viewParams.rotation.v.y, viewParams.rotation.v.z, viewParams.rotation.w);
#endif //_RELEASE

	if (m_playerCamera)
	{
		bool reOrientPlayerToView = m_playerCamera->Update(viewParams, gEnv->pTimer->GetFrameTime());
		if (reOrientPlayerToView)
		{
			SetViewRotation(viewParams.rotation);
		}
	}

	viewParams.blend = m_viewBlending;
	m_viewBlending = true;	// only disable blending for one frame

	//store the view matrix, without vertical component tough, since its going to be used by the VectorToLocal function.
	Vec3 forward(viewParams.rotation.GetColumn1());
	Vec3 up(m_pPlayerRotation->GetBaseQuat().GetColumn2());
	Vec3 right(-(up % forward));

	m_clientViewMatrix.SetFromVectors(right,up%right,up,viewParams.position);
	m_clientViewMatrix.OrthonormalizeFast();

	// finally, update the network system
#if FULL_ON_SCHEDULING
	if (gEnv->bMultiplayer)
	{
		INetContext* pNetContext = g_pGame->GetIGameFramework()->GetNetContext();
		if(pNetContext)
		{
			pNetContext->ChangedFov( GetEntityId(), viewParams.fov );
		}
	}
#endif

	m_isControllingCamera = true;

	CRecordingSystem* recordingSystem = g_pGame->GetRecordingSystem();
	if(recordingSystem && recordingSystem->IsPlayingBack())
	{
		recordingSystem->UpdateView(viewParams);
		m_isControllingCamera = false;
	}

#ifndef _RELEASE
	if (g_pGameCVars->pl_debug_view != 0)
	{
		CryLog("CPlayer::UpdateView: Post Pos(%f, %f, %f) Rot(%f, %f, %f, %f)", viewParams.position.x, viewParams.position.y, viewParams.position.z, viewParams.rotation.v.x, viewParams.rotation.v.y, viewParams.rotation.v.z, viewParams.rotation.w);
		CryWatch("CPlayer::UpdateView: (%f %f %f)  Rot(%f, %f, %f, %f)", viewParams.position.x, viewParams.position.y, viewParams.position.z, viewParams.rotation.v.x, viewParams.rotation.v.y, viewParams.rotation.v.z, viewParams.rotation.w);
	}
#endif //_RELEASE

	// pass view params to UI manager (used for per entity tags, 3d hud)
	g_pGame->GetUI()->ProcessViewParams(viewParams);
}

void CPlayer::PostUpdateView(SViewParams &viewParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if (CItem *pItem=GetItem(GetInventory()->GetCurrentItem()))
		pItem->PostFilterView(viewParams);

	const bool bRelativeToParent = false;
	const int slotIndex = 0; 
	bool bWorldSpace = (!(GetEntity()->GetSlotFlags(slotIndex) & ENTITY_SLOT_RENDER_NEAREST))?true:false;
	
	CRecordingSystem* recordingSystem = g_pGame->GetRecordingSystem();
	if(recordingSystem && recordingSystem->IsPlayingBack() && !m_isControllingCamera)
	{
		bWorldSpace = recordingSystem->GetPlaybackInfo().m_view.bWorldSpace;
	}

	if (DoSTAPAiming())
	{
		m_lastCameraLocation.t = GetFPCameraPosition(bWorldSpace);
		m_lastCameraLocation.q = GetViewQuatFinal();

		// Apply entity rotation if rendering in camera space
		if(!bWorldSpace)
		{
			m_lastCameraLocation.t = GetEntity()->GetWorldRotation() * m_lastCameraLocation.t;
		}
	}
	else
	{
		const Vec3 playerPos = (bWorldSpace)?GetEntity()->GetWorldPos():ZERO;
		const QuatT worldTransform(GetEntity()->GetWorldRotation(), playerPos);
		const QuatT &localCamera = GetCameraTran();

		m_lastCameraLocation = worldTransform * localCamera;
	}
}

void CPlayer::RegisterPlayerEventListener(IPlayerEventListener *pPlayerEventListener)
{
	stl::push_back_unique(m_playerEventListeners, pPlayerEventListener);
}

void CPlayer::UnregisterPlayerEventListener(IPlayerEventListener *pPlayerEventListener)
{
	stl::find_and_erase(m_playerEventListeners, pPlayerEventListener);
}

void CPlayer::RegisterPlayerUpdateListener(IPlayerUpdateListener *pPlayerUpdateListener)
{
	stl::push_back_unique(m_playerUpdateListeners, pPlayerUpdateListener);
}

void CPlayer::UnregisterPlayerUpdateListener(IPlayerUpdateListener *pPlayerUpdateListener)
{
	stl::find_and_erase(m_playerUpdateListeners, pPlayerUpdateListener);
}

IEntity *CPlayer::LinkToVehicle(EntityId vehicleId) 
{
	EntityId previousLinkedVehicleId = 0;
	if(vehicleId == 0)	//if we're unlinking, see if we can get previously linked vehicle id
	{
		IVehicle* pVehicle = GetLinkedVehicle();
		if(pVehicle)
		{
			previousLinkedVehicleId = pVehicle->GetEntityId();
		}
	}

	IEntity *pLinkedEntity = CActor::LinkToVehicle(vehicleId);

	EntityId playerId = GetEntityId();
	if (pLinkedEntity)
	{
		IVehicle *pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(vehicleId);
		
		if (IVehicleSeat *pSeat = pVehicle->GetSeatForPassenger(playerId))
		{
			CALL_PLAYER_EVENT_LISTENERS(OnEnterVehicle(this,pVehicle->GetEntity()->GetClass()->GetName(),pSeat->GetSeatName(),m_stats.isThirdPerson));

			if(IsClient() && pSeat->IsAutoAimEnabled())
			{
				m_linkStats.flags |= SLinkStats::LINKED_FREELOOK;
			}
		}

		// don't interpolate to vehicle camera (otherwise it intersects the vehicle)
		if(IsClient())
		{
			SupressViewBlending();
		}
	}
	else
	{
		CALL_PLAYER_EVENT_LISTENERS(OnExitVehicle(this));
		m_vehicleViewDir.Set(0,1,0);

		// don't interpolate back from vehicle camera (otherwise you see your own legs)
		if(IsClient())
		{
			SupressViewBlending();
			if(IsThirdPerson())
			{
				ToggleThirdPerson();
			}

			// Prevent further interactions this frame
			GetPlayerEntityInteration().JustInteracted();
		}
	}

	return pLinkedEntity;
}

IEntity *CPlayer::LinkToEntity(EntityId entityId, bool bKeepTransformOnDetach) 
{
#ifdef __PLAYER_KEEP_ROTATION_ON_ATTACH
	Quat rotation = GetEntity()->GetRotation();
#endif
	IEntity *pLinkedEntity = CActor::LinkToEntity(entityId, bKeepTransformOnDetach);

	if (pLinkedEntity)
	{
#ifdef __PLAYER_KEEP_ROTATION_ON_ATTACH
		if (bKeepTransformOnDetach)
		{
			assert( rotation.IsValid() );
			m_linkStats.viewQuatLinked = m_linkStats.baseQuatLinked = rotation;
			m_viewQuatFinal = m_viewQuat = m_baseQuat = rotation;
		}
		else
#endif
		m_pPlayerRotation->SetViewRotation( Quat::CreateIdentity() );
	}

	return pLinkedEntity;
}

void CPlayer::LinkToMountedWeapon(EntityId weaponId) 
{
	m_stats.mountedWeaponID = weaponId;
 
	if(!MountedGunControllerEnabled())
		return;

	if (weaponId)
	{
		m_mountedGunController.OnEnter(weaponId);
	}
	else
	{
		m_mountedGunController.OnLeave();
	}
}

void CPlayer::StartInteractiveAction(EntityId entityId, int interactionIndex)
{
	if(IsPlayer())
	{
		gEnv->pGameFramework->AllowSave( false );
	}

	if(entityId)
	{
		SStateEventInteractiveAction actionEvent( entityId, true, interactionIndex );
		StateMachineHandleEventMovement(actionEvent);

		if(IsClient())
		{
			CHANGED_NETWORK_STATE(this, ASPECT_INTERACTIVE_OBJECT);
		}
	}
}

void CPlayer::SetAnimatedCharacterParams( const SAnimatedCharacterParams& params )
{
	if( !m_isPlayer && !IsDead() && !gEnv->bMultiplayer )
	{
		SAnimatedCharacterParams newParams = params;
		newParams.inertia = 0.0f;
		newParams.inertiaAccel = 0.0f;
		newParams.timeImpulseRecover = 0.0f;

		m_pAnimatedCharacter->SetParams( newParams );

		return;
	}
	m_pAnimatedCharacter->SetParams( params );
}



void CPlayer::SwitchPlayerInput(IPlayerInput* pNewPlayerInput)
{
	m_pPlayerInput.reset(pNewPlayerInput);
}

bool CPlayer::IsInteracting() const
{
	return CActor::IsInteracting()
		|| IsOnLadder()
		|| IsOnLedge();
}

void CPlayer::StartInteractiveActionByName( const char* interaction, bool bUpdateVisibility, float actionSpeed /*= 1.0f*/ )
{
	if(IsPlayer())
	{
		gEnv->pGameFramework->AllowSave( false );
	}

	SStateEventInteractiveAction actionEvent( interaction, bUpdateVisibility, actionSpeed );
	StateMachineHandleEventMovement(actionEvent);
}

void CPlayer::EndInteractiveAction(EntityId entityId)
{
	if(IsPlayer())
	{
		gEnv->pGameFramework->AllowSave( true );
	}
}

bool CPlayer::IsInteractiveActionDone() const
{
	return( !m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_InteractiveAction ) );
}


//------------------------------------------------------------------------
bool CPlayer::GetForcedLookDir(Vec3& vDir) const
{
	if (m_forcedLookObjectId)
	{
		IEntity* pForcedLookObject = gEnv->pEntitySystem->GetEntity(m_forcedLookObjectId);
		if (pForcedLookObject)
		{
			vDir = pForcedLookObject->GetPos() - GetEntity()->GetPos();
			vDir.Normalize();
			return true;
		}
	}

	if (!m_forcedLookDir.IsZero())
	{
		vDir = m_forcedLookDir;
		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CPlayer::SetForcedLookDir(const Vec3& vDir)
{
	m_forcedLookDir = vDir;
	m_forcedLookObjectId = 0;
}

//------------------------------------------------------------------------
void CPlayer::ClearForcedLookDir()
{
	m_forcedLookDir.zero();
}

//------------------------------------------------------------------------
EntityId CPlayer::GetForcedLookObjectId() const
{
	return m_forcedLookObjectId;
}

//------------------------------------------------------------------------
void CPlayer::SetForcedLookObjectId(EntityId entityId)
{
	m_forcedLookDir.zero();
	m_forcedLookObjectId = entityId;
}

//------------------------------------------------------------------------
void CPlayer::ClearForcedLookObjectId()
{
	m_forcedLookObjectId = 0;
}

//------------------------------------------------------------------------
bool CPlayer::CanMove() const
{
	return (!m_pHitDeathReactions || m_pHitDeathReactions->CanActorMove()) &&
		!gEnv->pGameFramework->GetICooperativeAnimationManager()->IsActorBusy(GetEntityId());
}

void CPlayer::SufferingHighLatency(bool highLatency)
{
	if (highLatency && !m_sufferingHighLatency)
	{
		if (IsClient() && !gEnv->bServer)
			g_pGameActions->FilterNoConnectivity()->Enable(true);
	}
	else if (!highLatency && m_sufferingHighLatency)
	{
		if (IsClient() && !gEnv->bServer)
			g_pGameActions->FilterNoConnectivity()->Enable(false);

		// deal with vehicles here as well
	}

	// the following is done each frame on the server, and once on the client, when we're suffering high latency
	if (highLatency)
	{
		if (m_pPlayerInput.get())
			m_pPlayerInput->Reset();

		if (IVehicle *pVehicle=GetLinkedVehicle())
		{
			if (IActor *pActor=pVehicle->GetDriver())
			{
				if (pActor->GetEntityId()==GetEntityId())
				{
					if (IVehicleMovement *pMovement=pVehicle->GetMovement())
						pMovement->ResetInput();
				}
			}
		}
	}

	m_sufferingHighLatency = highLatency;
}

const char* CPlayer::GetActorClassName() const
{ 
	return "CPlayer";
}

void CPlayer::SetViewInVehicle(Quat viewRotation)
{
	m_vehicleViewDir = viewRotation * Vec3(0,1,0); 
	//CHANGED_NETWORK_STATE(this, ASPECT_VEHICLEVIEWDIR_CLIENT);	// MartinSh: Disabled for now to save bandwidth, it doesn't seem to have any effect at the moment anyway
}

void CPlayer::SetStance(EStance desiredStance)
{
	StateMachineHandleEventMovement( SStateEventStanceChanged(desiredStance) );
}

void CPlayer::OnStanceChanged(EStance newStance, EStance oldStance)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	CActor::OnStanceChanged(newStance, oldStance);

    CRY_ASSERT_TRACE(newStance == m_stance, ("Expected 'newStance' (%d) to be equal to 'm_stance' (%d)", newStance, m_stance.Value()));
	CRY_ASSERT_TRACE(newStance != oldStance, ("Expected 'newStance' (%d) to be different to 'oldStance' (%d)", newStance, oldStance));

	if (IsClient())
	{
		CCCPOINT_IF(oldStance == STANCE_CROUCH, PlayerMovement_LocalPlayerStopCrouch);
		CCCPOINT_IF(newStance == STANCE_CROUCH, PlayerMovement_LocalPlayerStartCrouch);
	}
	else
	{
		CCCPOINT_IF(oldStance == STANCE_CROUCH, PlayerMovement_NonLocalPlayerStopCrouch);
		CCCPOINT_IF(newStance == STANCE_CROUCH, PlayerMovement_NonLocalPlayerStartCrouch);
	}

	if(IsPlayer() && !IsAIControlled())
	{
		if(IActionController* pActionController = GetAnimatedCharacter()->GetActionController())
		{
			SetStanceTag(newStance, pActionController->GetContext().state);
		}
	}

	CALL_PLAYER_EVENT_LISTENERS(OnStanceChanged(this, newStance));
}

void CPlayer::SetStanceTag(EStance stance, CTagState& tagState)
{
	TagID tagID = TAG_ID_INVALID;

	switch (stance)
	{
	case STANCE_CROUCH:
		tagID = PlayerMannequin.tagIDs.crouch;
		break;
	case STANCE_SWIM:
		tagID = PlayerMannequin.tagIDs.swim;
		break;
	default:
		break;
	}

	tagState.SetGroup(PlayerMannequin.tagGroupIDs.stance, tagID);
}

float CPlayer::CalculatePseudoSpeed(bool wantSprint, float speedOverride) const
{
	if( m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Spectator ) )
	{
		return 0.0f;
	}

	if (wantSprint)
	{
		return 1.0f;
	}
	else
	{
		const float MIN_WALK_SPEED = 0.1f;
		const float MAX_WALK_SPEED = 2.0f; // crossover point between walk & run

		const float MIN_PSEUDO_SPEED = 0.1f;
		const float WALK_PSEUDO_SPEED = 0.5f; // crossover point between walk & run
		const float MAX_PSEUDO_SPEED = 0.9f;

		const float MAX_SPEED = MIN_WALK_SPEED + (MAX_WALK_SPEED - MIN_WALK_SPEED)*(MAX_PSEUDO_SPEED - MIN_PSEUDO_SPEED)/(WALK_PSEUDO_SPEED - MIN_PSEUDO_SPEED); // tune MAX_SPEED to reach pseudospeed=WALK_PSEUDO_SPEED at speed=MAX_WALK_SPEED
		const float fINV_MAGNITUDE = 1.0f / (MAX_SPEED - MIN_WALK_SPEED);

		float speed = (float) __fsel(speedOverride, speedOverride, GetActorPhysics().velocity.len());

		const float ret = min((speed - MIN_WALK_SPEED) * fINV_MAGNITUDE, 1.0f);

		return (float)__fsel(speed - MIN_WALK_SPEED, MIN_PSEUDO_SPEED + (ret * (MAX_PSEUDO_SPEED - MIN_PSEUDO_SPEED)), 0.0f);

		//Slower code below so you can work out what the hell is happening above

// 		if (speed < MIN_WALK_SPEED)
// 		{
// 			return 0.0f;
// 		}
// 		else
// 		{
// 			float ret = ((speed - MIN_WALK_SPEED) * fINV_MAGNITUDE);
// 			if (ret > 1.0f)
// 				ret = 1.0f;
// 			return MIN_PSEUDO_SPEED + (ret * (MAX_PSEUDO_SPEED - MIN_PSEUDO_SPEED));
// 		}
	}
}

float CPlayer::GetStanceMaxSpeed(EStance stance) const
{
	return GetStanceInfo(stance)->maxSpeed * GetTotalSpeedMultiplier() * GetWeaponMovementFactor() * GetModifiableValues().GetValue(kPMV_MovementSpeedMultiplier);
}

void CPlayer::SetParamsFromLua(SmartScriptTable &rTable)
{
	CActor::SetParamsFromLua(rTable);

	int followHead = m_stats.followCharacterHead.Value();
	rTable->GetValue("followCharacterHead", followHead);
	m_stats.followCharacterHead = followHead;
}

void CPlayer::InitGameParams(const SActorGameParams &gameParams, const bool reloadCharacterSounds)
{
	CActor::InitGameParams(gameParams, reloadCharacterSounds);

	if (!gEnv->bMultiplayer)
	{
		if (m_params.footstepIndGearAudioSignal_Run.empty())
			m_sounds[ESound_Gear_Run].audioSignalPlayer.InvalidateSignal();
		else
			m_sounds[ESound_Gear_Run].audioSignalPlayer.SetSignal(m_params.footstepIndGearAudioSignal_Run);
			
		if (m_params.footstepIndGearAudioSignal_Walk.empty())
			m_sounds[ESound_Gear_Walk].audioSignalPlayer.InvalidateSignal();
		else
			m_sounds[ESound_Gear_Walk].audioSignalPlayer.SetSignal(m_params.footstepIndGearAudioSignal_Walk);
	} 
}

void CPlayer::SetStats(SmartScriptTable &rTable)
{
	CActor::SetStats(rTable);
	m_stats.inFiring = GetActorStats()->inFiring;
}


//------------------------------------------------------------------------
void CPlayer::UpdateStats(float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);
	
	//The results from GetEntity() are used without checking in
	//	CPlayer::Update, so should be safe here!
	CRY_ASSERT(GetEntity());	

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();
	if (!pPhysEnt)
		return;
	const bool isLinkedToVehicle = (GetLinkedVehicle() != NULL);
	if (isLinkedToVehicle && gEnv->IsDedicated())
	{
		// leipzig: force inactive (was active on ded. servers)
		pe_player_dynamics paramsGet;
		if (pPhysEnt->GetParams(&paramsGet))
		{      
			if (paramsGet.bActive && m_pAnimatedCharacter)
				m_pAnimatedCharacter->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "Player::UpdateStats");
		}			
	}  

	const bool isClient(IsClient());

	//update some timers
	m_stats.inFiring = max(0.0f,m_stats.inFiring - frameTime);

	if(isClient)
	{
		UpdateFlashbangEffect(frameTime);
	}

	m_spectacularKill.Update(frameTime);
	m_stealthKill.Update(frameTime);
	m_largeObjectInteraction.Update( frameTime );

	Vec3 smokeTestPos = GetEntity()->GetSlotWorldTM(0) * GetBoneTransform(BONE_SPINE).t;
	float insideFactor = 0.0f;
	bool inSmoke = CSmokeManager::GetSmokeManager()->IsPointInSmoke(smokeTestPos, insideFactor);
	const float multiplier[2] = {-1.0f, 1.0f};
	float newSmokeTime = clamp_tpl(m_stats.fInSmokeTime + (frameTime * multiplier[inSmoke]), 0.0f, SMOKE_EXIT_TIME);
	m_stats.fInSmokeTime = newSmokeTime;
	m_stats.bIsInSmoke = inSmoke;

	UpdateReactionOverlay(frameTime);
}

void CPlayer::SetReactionOverlay(EReactionOverlay overlay)
{
	ICharacterInstance *pCharInstance = GetEntity()->GetCharacter(0);
	if (pCharInstance)
	{
		if (m_reactionAnims[overlay].animID < 0)
		{
			IAnimationSet *animSet = pCharInstance->GetIAnimationSet();

			for (int i=EReaction_None+1; i<EReaction_Total; i++)
			{
				m_reactionAnims[i].animID = animSet->GetAnimIDByName(m_reactionAnims[i].name);

				if (m_reactionAnims[i].animID < 0)
				{
					GameWarning("Could not find reaction anim %s", m_reactionAnims[i].name);
				}
			}
		}

		CryCharAnimationParams params;
		params.m_fTransTime = m_reactionAnims[overlay].blend;
		params.m_nLayerID = 10;
		params.m_nFlags = m_reactionAnims[overlay].flags;
		pCharInstance->GetISkeletonAnim()->StartAnimationById(m_reactionAnims[overlay].animID, params);
		m_reactionTimer = pCharInstance->GetIAnimationSet()->GetDuration_sec(m_reactionAnims[overlay].animID);
		m_reactionOverlay = overlay;
	}
}

void CPlayer::UpdateReactionOverlay(float frameTime)
{
	if (IsThirdPerson() && gEnv->bMultiplayer)
	{
		bool bIsInPickAndThrowMode	= IsInPickAndThrowMode();
		bool shouldReactToSmoke			= !bIsInPickAndThrowMode && (m_stats.fInSmokeTime > SMOKE_ENTER_TIME);
		bool shouldReactToFlashBang = !bIsInPickAndThrowMode && m_netFlashBangStun;
		int oldReactionOverlay = m_reactionOverlay;

		switch (m_reactionOverlay)
		{
		case EReaction_None:
			if (shouldReactToFlashBang)
			{
				SetReactionOverlay(EReaction_FlashEnter);
			}
			else if (shouldReactToSmoke)
			{
				SetReactionOverlay(EReaction_SmokeEnter);
			}
			break;
		case EReaction_SmokeEnter:
			m_reactionTimer -= frameTime;

			if (m_reactionTimer <= 0.0f)
			{
				SetReactionOverlay(EReaction_SmokeLoop);
			}
			break;
		case EReaction_SmokeLoop:
			if (!shouldReactToSmoke)
			{
				SetReactionOverlay(EReaction_SmokeExit);
				m_reactionOverlay = EReaction_None;
			}
			break;
		case EReaction_FlashEnter:
			m_reactionTimer -= frameTime;

			if (m_reactionTimer <= 0.0f)
			{
				SetReactionOverlay(EReaction_FlashLoop);
			}
			break;
		case EReaction_FlashLoop:
			if (!shouldReactToFlashBang)
			{
				SetReactionOverlay(EReaction_FlashExit);
				m_reactionOverlay = EReaction_None;
			}
			break;
		}

		if ((m_reactionOverlay != EReaction_None) || (oldReactionOverlay != EReaction_None))
		{
			float targetReactionFactor = 1.0f;
			CWeapon *pWeapon = GetWeapon(GetCurrentItemId());
			if (pWeapon && (pWeapon->IsBusy() || pWeapon->IsReloading()))
			{
				targetReactionFactor = 0.0f;
			}

			Interpolate(m_reactionFactor, targetReactionFactor, 10.0f, frameTime);

			ICharacterInstance *pCharInstance = GetEntity()->GetCharacter(0);
			if (pCharInstance)
			{
				pCharInstance->GetISkeletonAnim()->SetLayerBlendWeight(10, m_reactionFactor);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void SetupPlayerCharacterVisibility(IEntity* playerEntity, bool isThirdPerson, int shadowCharacterSlot, bool forceDontRenderNearest)
{
	ICharacterInstance *mainChar			= playerEntity->GetCharacter(0);

	if (mainChar == NULL)
		return;

	IAttachmentManager *attachmentMan		= mainChar->GetIAttachmentManager();
	ICharacterInstance *shadowChar			= NULL;
	IAttachmentManager *attachmentManShadow	= NULL;
	if (shadowCharacterSlot >= 0)
	{
		shadowChar = playerEntity->GetCharacter(shadowCharacterSlot);
		if (shadowChar)
		{
			attachmentManShadow = shadowChar->GetIAttachmentManager();
		}
	}

	bool showShadowChar = g_pGameCVars->g_showShadowChar != 0;

	uint32 flags = playerEntity->GetSlotFlags(0);
	uint32 currentFlags = flags;

	if (isThirdPerson || forceDontRenderNearest || g_pGameCVars->g_detachCamera || !g_pGameCVars->pl_renderInNearest)
	{
		flags &= ~ENTITY_SLOT_RENDER_NEAREST;
	}
	else
	{
		flags |= ENTITY_SLOT_RENDER_NEAREST;
	}

	CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();

	if(pRecordingSystem && !isThirdPerson && currentFlags != flags)
	{
		pRecordingSystem->OnPlayerRenderNearestChange((flags & ENTITY_SLOT_RENDER_NEAREST) != 0);
	}

	playerEntity->SetSlotFlags(0, flags);

	if (attachmentManShadow)
	{
		for (int32 i=0; i<attachmentManShadow->GetAttachmentCount(); i++)
		{
			attachmentManShadow->GetInterfaceByIndex(i)->HideAttachment(!showShadowChar);
		}
	}

	IAttachment *attachment;

	//--- Toggle 3P attachments
	const char *ppFirstPersonParts[] = { "arms_1p" };
	const char *ppThirdPersonParts[] = { "arms_3p", "head", "eye_left", "eye_right", "lower_body", "upper_body", "googles", "bag_01", "bag_02", "bag_03", "bag_04", "bag_05", "bag_06", "visor" };

	const int numFirstPersonParts = CRY_ARRAY_COUNT(ppFirstPersonParts);
	const int numThirdPersonParts = CRY_ARRAY_COUNT(ppThirdPersonParts);

	for (int i = 0; i < numFirstPersonParts; ++ i)
	{
		attachment = attachmentMan->GetInterfaceByName(ppFirstPersonParts[i]);
		if (attachment)
		{
			attachment->HideAttachment(isThirdPerson);
		}
	}
	for (int i = 0; i < numThirdPersonParts; ++ i)
	{
		attachment = attachmentMan->GetInterfaceByName(ppThirdPersonParts[i]);
		if (attachment)
		{
			attachment->HideAttachment(!isThirdPerson);
		}
	}

	if (isThirdPerson)
	{
		for (int32 i=0; i<attachmentMan->GetAttachmentCount(); i++)
		{
			attachment = attachmentMan->GetInterfaceByIndex(i);
			attachment->HideInShadow(false);
			attachment->HideInRecursion(false);
		}
		if (attachmentManShadow)
		{
			for (int32 i=0; i<attachmentManShadow->GetAttachmentCount(); i++)
			{
				attachment = attachmentManShadow->GetInterfaceByIndex(i);
				attachment->HideInShadow(true);
				attachment->HideInRecursion(true);
			}
		}
	}
	else
	{
		for (int32 i=0; i<attachmentMan->GetAttachmentCount(); i++)
		{
			attachment = attachmentMan->GetInterfaceByIndex(i);
			attachment->HideInRecursion(shadowChar ? true : false);
		}
		if (attachmentManShadow)
		{
			for (int32 i=0; i<attachmentManShadow->GetAttachmentCount(); i++)
			{
				attachment = attachmentManShadow->GetInterfaceByIndex(i);
				attachment->HideInShadow(false);
				attachment->HideInRecursion(false);
			}
		}
	}
}

void CPlayer::RefreshVisibilityState()
{
	bool forceDontRenderNearest = (GetLinkedVehicle() != NULL);

	CItem* pCurrentItem = static_cast<CItem*>(GetCurrentItem());
	if (pCurrentItem && pCurrentItem->IsMounted())
	{
		forceDontRenderNearest = false;
	}
	else if (m_stealthKill.IsBusy())
	{
		forceDontRenderNearest = true;
	}

	const bool isThirdPerson = IsThirdPerson();
	SetupPlayerCharacterVisibility(GetEntity(), isThirdPerson, GetShadowCharacterSlot(), forceDontRenderNearest);

	if (m_pPlayerTypeComponent)
		m_pPlayerTypeComponent->RefreshVisibilityState( isThirdPerson );

	IAnimatedCharacter *pAnimatedCharacter = GetAnimatedCharacter();
	if (pAnimatedCharacter)
	{
		pAnimatedCharacter->UpdateCharacterPtrs();
	}
}

//-----------------------------------------------------------------------------
void CPlayer::ToggleThirdPerson()
{
	SetThirdPerson(!m_stats.isThirdPerson);
}

void CPlayer::SetThirdPerson(bool thirdPersonEnabled, bool force)
{
	if ((CanToggleCamera() || force) && m_stats.isThirdPerson != thirdPersonEnabled)
	{
		m_stats.isThirdPerson = thirdPersonEnabled;

		UpdateThirdPersonState();

		CALL_PLAYER_EVENT_LISTENERS(OnToggleThirdPerson(this,m_stats.isThirdPerson));
	}
}

void CPlayer::UpdateThirdPersonState()
{
	bool isFirstPerson = !IsThirdPerson();

	m_animationProxy.SetFirstPerson(isFirstPerson);
	m_animationProxyUpper.SetFirstPerson(isFirstPerson);

	// For SP we update this based on if you are Third Person.
	if(!gEnv->bMultiplayer)
	{
		if(IAnimatedCharacter* pAnimChar = GetAnimatedCharacter())
		{
			pAnimChar->GetGroundAlignmentParams().SetFlag(eGA_PoseAlignerUseRootOffset, !isFirstPerson);
		}
	}

	if (m_stats.isThirdPerson)
	{
		m_torsoAimIK.Disable(true);
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(GetPickAndThrowEntity());
		if (pEntity)
		{
			const int nSlotCount = pEntity->GetSlotCount();
			for (int i = 0; i < nSlotCount; ++i)
			{
				const uint32 flags = pEntity->GetSlotFlags(i);
				pEntity->SetSlotFlags(i, flags & ~ENTITY_SLOT_RENDER_NEAREST);
			}
		}
	}
	else
	{
		m_torsoAimIK.Enable(true);
		IEntity *pEntity = gEnv->pEntitySystem->GetEntity(GetPickAndThrowEntity());
		if (pEntity)
		{
			const int nSlotCount = pEntity->GetSlotCount();
			for (int i = 0; i < nSlotCount; ++i)
			{
				const uint32 flags = pEntity->GetSlotFlags(i);
				pEntity->SetSlotFlags(i, flags | ENTITY_SLOT_RENDER_NEAREST);
			}
		}
	}

	if(IsPlayer() && !IsAIControlled())
	{
		SetTag(PlayerMannequin.tagIDs.FP, isFirstPerson);
	}

	RefreshVisibilityState();
}

int CPlayer::IsGod()
{
	if (!m_pGameFramework->CanCheat())
		return 0;

	CGodMode& godMode = CGodMode::GetInstance();

	EGodModeState godModeState = godMode.GetCurrentState();

	// Demi-Gods are not Gods
	if (eGMS_DemiGodMode == godModeState)
		return 0;

	//check if is the player
	if (IsPlayer())
		return (int)godModeState;

	//check if is a squadmate
	IAIActor* pAIActor = CastToIAIActorSafe(GetEntity()->GetAI());
	if (pAIActor)
	{
		int group=pAIActor->GetParameters().m_nGroup;
		if(group>= 0 && group<10)
			return (godModeState == eGMS_TeamGodMode?1:0);
	}
	return 0;
}

bool CPlayer::IsThirdPerson() const
{
	//force thirdperson view for non-clients
  return !m_isClient || m_stats.isThirdPerson;	
}

void CPlayer::SpawnCorpse()
{
	IEntity *pEntity = GetEntity();
	if(gEnv->bMultiplayer && pEntity && m_dropCorpseOnDeath && g_pGameCVars->pl_spawnCorpseOnDeath)
	{
		if(IPhysicalEntity* pOriginalPhysEnt = pEntity->GetPhysics())
		{
			if(pOriginalPhysEnt->GetType() != PE_ARTICULATED)
			{
				CryLog("[CORPSE] Forcing to Ragdoll instantly before corpsing");
				SEntityPhysicalizeParams entityPhysParams;

				entityPhysParams.nSlot = 0;
				entityPhysParams.mass = 120.0f;
				entityPhysParams.bCopyJointVelocities = true;
				entityPhysParams.nLod = 1;

				pe_player_dimensions playerDim;
				pe_player_dynamics playerDyn;

				playerDyn.gravity.z = 15.0f;
				playerDyn.kInertia = 5.5f;

				entityPhysParams.pPlayerDimensions = &playerDim;
				entityPhysParams.pPlayerDynamics = &playerDyn;

				// Now physicalize as Articulated Physics.
				entityPhysParams.type = PE_ARTICULATED;
				pEntity->Physicalize(entityPhysParams);

				if(ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
				{
					pCharacter->GetISkeletonAnim()->StopAnimationsAllLayers();
					if(IAnimationPoseBlenderDir* pIPoseBlenderLook = pCharacter->GetISkeletonPose()->GetIPoseBlenderLook())
					{
						pIPoseBlenderLook->SetState(false);
					}
				}

				if(IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics())
				{
					SetRagdollPhysicsParams(pPhysicalEntity, false);
				}

				// Set ViewDistRatio.
				pEntity->SetViewDistRatio(g_pGameCVars->g_actorViewDistRatio);
			}
		}

		if (pEntity->GetPhysics())
		{
			ICharacterInstance *pCharInst = pEntity->GetCharacter(0);

			if (pCharInst)
			{
				IEntityClass *pEntityClass =  gEnv->pEntitySystem->GetClassRegistry()->FindClass("Corpse");
				assert(pEntityClass);

				CCCPOINT_IF(IsClient(), PlayerState_LocalPlayerCloneOnDeath);
				CCCPOINT_IF(!IsClient(), PlayerState_NonLocalPlayerCloneOnDeath);

				SEntitySpawnParams params;
				params.pClass = pEntityClass;
				params.sName = "Corpse";

				params.vPosition = pEntity->GetPos();
				params.qRotation = pEntity->GetRotation();

				params.nFlags |= (ENTITY_FLAG_NEVER_NETWORK_STATIC|ENTITY_FLAG_CLIENT_ONLY);

				IEntity *pCloneEntity = gEnv->pEntitySystem->SpawnEntity(params, true);
				assert(pCloneEntity);

				pCloneEntity->SetFlags(pCloneEntity->GetFlags() | (ENTITY_FLAG_CASTSHADOW));

				pEntity->MoveSlot(pCloneEntity, 0);
				pCharInst->SetFlags( pCharInst->GetFlags() | CS_FLAG_UPDATE );

				//This is to fix a rare issue where you can potentially receive the spawn corpse message while the player is still
				//	in the stealth kill animation.
				if(m_stats.bStealthKilled)
				{
#ifndef _RELEASE
					CryLogAlways("[ERROR] Spawned corpse while in stealth kill. Game used to crash shortly, now hopefully won't... Contact Rich S");
#endif
					GetHitDeathReactions()->EndCurrentReaction();
				}

				pEntity->LoadCharacter(0, pCharInst->GetFilePath());

				EntityId corpseId = pCloneEntity->GetId();

				// Upon creating a corpse it defaults to a model's default material. Ensure is using dead material
				CTeamVisualizationManager* pTeamVisManager = g_pGame->GetGameRules()->GetTeamVisualizationManager();
				if(pTeamVisManager)
				{
					CGameRules* pGameRules = g_pGame->GetGameRules();
					pTeamVisManager->RefreshTeamMaterial(pCloneEntity, false, pGameRules ? pGameRules->GetThreatRating(gEnv->pGameFramework->GetClientActorId(), GetEntityId())==CGameRules::eFriendly : true );
				}

				CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
				if (pRecordingSystem)
				{
					// Re-record the corpse now that it's got a character
					pRecordingSystem->ReRecord(corpseId);
					pRecordingSystem->OnCorpseSpawned(corpseId, pEntity->GetId());
				}

				CCorpseManager* pCorpseManager = g_pGame->GetGameRules()->GetCorpseManager();
				CRY_ASSERT_MESSAGE(pCorpseManager, "NO CORPSE MANAGER INSTANTIATED");
				pCorpseManager->RegisterCorpse(corpseId, pCloneEntity->GetWorldPos(), GetBaseHeat());

				IAnimatedCharacter *pAnimatedCharacter = GetAnimatedCharacter();
				if (pAnimatedCharacter)
				{
					pAnimatedCharacter->UpdateCharacterPtrs();
				}
			}
		}
	}

	m_dropCorpseOnDeath = false;
}

void CPlayer::RestartMannequin()
{
	IActionController *pActionController = m_pAnimatedCharacter ? m_pAnimatedCharacter->GetActionController() : NULL;
	if (pActionController)
	{
		if( IsPlayer() && !IsAIControlled() )
		{
			pActionController->Resume();

			SAnimationContext &animContext = pActionController->GetContext();
			animContext.state.Set(PlayerMannequin.tagIDs.localClient, IsClient());
			animContext.state.SetGroup(PlayerMannequin.tagGroupIDs.playMode, gEnv->bMultiplayer ? PlayerMannequin.tagIDs.MP : PlayerMannequin.tagIDs.SP);
			animContext.state.Set(PlayerMannequin.tagIDs.FP, !IsThirdPerson());

			SetStanceTag(GetStance(), animContext.state);

			//--- Install persistent aimPose action
			pActionController->Queue(*new CPlayerBackgroundAction(PP_Movement, PlayerMannequin.fragmentIDs.aimPose));
			//--- Install persistent weaponPose action
			pActionController->Queue(*new CPlayerBackgroundAction(PP_Lowest, PlayerMannequin.fragmentIDs.weaponPose));

			CActionItemIdle *itemIdle = new CActionItemIdle(PP_Lowest, PlayerMannequin.fragmentIDs.idle, PlayerMannequin.fragmentIDs.idle_break, TAG_STATE_EMPTY, *this);
			pActionController->Queue(*itemIdle);

			CPlayerMovementAction *movementAction = new CPlayerMovementAction(PP_Movement);
			pActionController->Queue(*movementAction);

			m_weaponFPAiming.RestartMannequin();
		}
	}
}

void CPlayer::Revive( EReasonForRevive reasonForRevive )
{
	if (reasonForRevive == kRFR_Spawn)
	{
		m_teamWhenKilled = -1;
	}
	

	if(GetSpectatorState() == eASS_SpectatorMode)
	{
		return;
	}

	IActionController *pActionController = GetAnimatedCharacter()->GetActionController();
	if (pActionController)
	{
		pActionController->Reset();
		pActionController->SetFlag(AC_PausedUpdate, false);

		m_weaponFPAiming.ReleaseActions();
		m_weaponFPAiming.SetActionController(pActionController);

		if (m_pAIAnimationComponent.get())
		{
			m_pAIAnimationComponent->ResetMannequin();
		}
	}

	//Do not move to later in the function as it relies on the validity of m_stats.bStealthKilled
	SpawnCorpse();

#if CAPTURE_REPLAY_LOG
	if (IsPlayer() && (reasonForRevive == kRFR_FromInit || reasonForRevive == kRFR_Spawn))
	{
		gEnv->pConsole->ExecuteString("memReplayLabel spawnPlayer");
	}
#endif

	if( gEnv->bMultiplayer )
	{
		HandleMPPreRevive();

		if (!gEnv->bServer)
		{
			SHUDEvent event(eHUDEvent_OnPlayerRevive);
			event.AddData((int)GetEntityId());
			CHUDEventDispatcher::CallEvent(event);
		}
	}

	if(m_hideOnDeath && reasonForRevive != kRFR_StartSpectating)
	{
		m_hideOnDeath = false;
		GetEntity()->Hide(false);
	}

	if (reasonForRevive == kRFR_Spawn)
	{
		if(m_bMakeVisibleOnNextSpawn)
		{
			//CryLogAlways("[Visible on next spawn] %d %s - REVIVING - m_bMakeVisibleOnNextSpawn set, making visible!]", GetEntityId(), GetEntity()->GetName()); 
			GetEntity()->Invisible(false); 
			GetEntity()->Hide(false); 
			m_bMakeVisibleOnNextSpawn = false;
		}
	}

	CActor::Revive(reasonForRevive);

	StateMachineResetMovement();
	SelectMovementHierarchy();

	assert(g_pGame);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	const bool isClient = IsClient();
	IEntity* pEntity = GetEntity();

	// This controls fadeToBlack effects. In most cases we want to remove screen effects when we revive (spectate OR true revive). 
	// However, in certain cases we only want to reset fade to black screen effects on a true revive. 
	if ( !m_bDontResetFXUntilNextSpawnRevive|| 
		   reasonForRevive == kRFR_Spawn )
	{
		ResetScreenFX();
		m_bDontResetFXUntilNextSpawnRevive = false; 
	}

	m_heatController.Revive(GetBaseHeat());

	m_stats.bStealthKilled = false;
	m_stats.lastAttacker = 0;

	m_actions = 0;

	m_ragdollTime = 0.0f;
	m_lastReloadTime = 0.f;

	m_bCanTurnBody = true;

	if (isClient == m_stats.isThirdPerson)
		ToggleThirdPerson();
	
	RefreshVisibilityState();

	if ((reasonForRevive == kRFR_Spawn) && gEnv->bMultiplayer)
	{
		m_playerStateSwim_WaterTestProxy.Reset( true );
	}

	ExitPickAndThrow(true);

	const Vec3 oldGravity = GetActorPhysics().gravity; 
	// HAX: to fix player spawning and floating in dedicated server: Marcio fix me?
	if (gEnv->IsEditor() == false)  // AlexL: in editor, we never keep spectator mode
	{
		SSpectatorInfo  specInfo = m_stats.spectatorInfo;
		m_stats = SPlayerStats();
		m_stats.spectatorInfo = specInfo;
	}
	else
	{
		m_stats = SPlayerStats();
		m_playerStateSwim_WaterTestProxy.Reset( true );
	}
	m_actorPhysics.gravity = oldGravity; //Restore the gravity (it's set initialy in PostPhysicalize, and was being override here!)

	m_eyeOffset = GetStanceViewOffset(STANCE_STAND);
	m_weaponOffset.zero();

	m_reactionOverlay = EReaction_None;

	//m_baseQuat.SetIdentity();
	m_pPlayerRotation->SetViewRotationOnRevive( pEntity->GetRotation() );
	m_clientViewMatrix.SetIdentity();

	m_lastRequestedVelocity.zero();
	m_forcedLookDir.zero();
	m_forcedLookObjectId = 0;

	m_viewBlending = true;

	if (reasonForRevive != kRFR_StartSpectating)
	{
		m_fDeathTime = 0.0f;  // we need to know this value whilst spectating so we can display the respawn countdown and so on
	}
	
	pEntity->SetFlags(pEntity->GetFlags() | (ENTITY_FLAG_CASTSHADOW));
	pEntity->SetSlotFlags(0, pEntity->GetSlotFlags(0) | ENTITY_SLOT_RENDER);

	if (m_pPlayerInput.get())
		m_pPlayerInput->Reset();

	ICharacterInstance *pCharacter = pEntity->GetCharacter(0);

	if (pCharacter)
		pCharacter->EnableStartAnimation(true);

	ResetAnimations();
	
	if (reasonForRevive != kRFR_FromInit || GetISystem()->IsSerializingFile() == 1)
		ResetAnimationState();

	if(reasonForRevive != kRFR_FromInit && isClient)
	{		
		if(gEnv->bMultiplayer)
		{
			IView *pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetViewByEntityId(GetEntityId());
			if(pView)
				pView->ResetShaking();
				
			// cancel voice recording (else channel can be stuck open)
			g_pGame->GetIGameFramework()->EnableVoiceRecording(false);
		}
	}

	// marcio: reset pose on the dedicated server (dedicated server doesn't update animationgraphs)
	if (pCharacter && !gEnv->IsClient() && gEnv->bServer)
	{
		pCharacter->GetISkeletonPose()->SetDefaultPose();
	}

	if (isClient && m_pPlayerTypeComponent)
	{
		//--- Ensure the close combat target is cleared to avoid being pulled towards him on respawn
		g_pGame->GetAutoAimManager().SetCloseCombatSnapTarget(0, 0.f, 0.f);

		ResetFPView();
		ResetInteractor();
		m_playerHealthEffect.ReStart();

		IGameRulesObjectivesModule * objectives = pGameRules->GetObjectivesModule();
		if((gEnv->bMultiplayer
			&& objectives)
			&& !objectives->MustShowHealthEffect(GetEntityId()))
		{
			m_playerHealthEffect.Stop();
		}
		
		m_pPlayerTypeComponent->Revive();

		g_pGameActions->FilterNoMove()->Enable(false);
		SetClientSoundmood(ESoundmood_Alive);

		if (m_stats.cinematicFlags != 0)
		{
			ResetCinematicFlags();
		}

		// restore interactor
		IInteractor * pInteractor = GetInteractor();
		if (!GetGameObject()->GetUpdateSlotEnables(pInteractor, 0))
		{
			GetGameObject()->EnableUpdateSlot(pInteractor, 0);
		}
	}	

	m_torsoAimIK.Reset();
	if (IsThirdPerson())
	{
		m_torsoAimIK.Disable(true);
	}
	else
	{
		m_torsoAimIK.Enable(true);
	}
	m_lookAim.Reset();

	if (reasonForRevive == kRFR_Spawn)
	{
		m_lastFlashbangShooterId = 0;	//Clear last flashbangShooter (and time)
		m_lastFlashbangTime = 0.0f;
		CHANGED_NETWORK_STATE(this, ASPECT_FLASHBANG_SHOOTER_CLIENT);
	
		if(isClient)
		{
			CCCPOINT(PlayerState_LocalPlayerSpawn);
			CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnSpawn)); // local player revive

			if (gEnv->bMultiplayer)
			{
				if (g_pGameCVars->g_gameRules_preGame_StartSpawnedFrozen)
				{
					IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
					if (pStateModule && pStateModule->GetGameState() == IGameRulesStateModule::EGRS_PreGame)
					{
						g_pGameActions->FilterMPPreGameFreeze()->Enable(true);
					}
				}

				IGameRulesRoundsModule *rounds=pGameRules->GetRoundsModule();
				if (rounds)
				{
					rounds->OnLocalPlayerSpawned();	
				}

				if (!gEnv->bServer)
				{
					// Clients have their loadout already when they revive due to RMI ordering, server has to do this elsewhere
					OnReceivingLoadout();
				}
			}
		}
		else
		{
			CCCPOINT(PlayerState_NonLocalPlayerSpawn);
		}

		if(gEnv->bMultiplayer)
		{
			// disable spectating if we are spawning
			SetSpectatorModeAndOtherEntId(0, 0, true);

			if (m_timeFirstSpawned == 0.f)
			{
				m_timeFirstSpawned = pGameRules->GetServerTime();

				if (isClient && reasonForRevive == kRFR_Spawn)
				{
					IActionFilter* pFilter = g_pGameActions->FilterNotYetSpawned();
					if(pFilter && pFilter->Enabled())
					{
						pFilter->Enable(false);
					}
				}
			}
		}
	}

	if (m_pHitDeathReactions)
		m_pHitDeathReactions->OnRevive();

	m_stealthKilledById = 0;

	RestartMannequin();

	if(gEnv->bMultiplayer && reasonForRevive == kRFR_Spawn)
	{
		// Do this every time since spawned corpses steals our character and its overridden team materials
		CTeamVisualizationManager* pTeamVisManager = pGameRules->GetTeamVisualizationManager();
		if(pTeamVisManager)
		{
			pTeamVisManager->RefreshPlayerTeamMaterial(GetEntityId()); 
		}
	}

	// Whilst in intro mode, we don't want to see other players watching their intros flying around in-front of us.
	IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
	if (pGameRules->IsIntroSequenceRegistered() && pStateModule && pStateModule->GetGameState() == IGameRulesStateModule::EGRS_Intro && !pGameRules->IntroSequenceHasCompletedPlaying())
	{
		// Whenever a player is *validly* spawning, we don't interfere, if they are about to start their intro too.. we will.
		// When they spawn, they will be visible again.
		if(!IsClient() && pGameRules->IsIntroSequenceCurrentlyPlaying())
		{
			pEntity->Invisible(true); 
		}
	}
	CALL_PLAYER_EVENT_LISTENERS(OnRevive(this, IsGod() > 0));
}

bool CPlayer::WasFriendlyWhenKilled(EntityId entityId) const
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entityId);

	//Faster to not do an early out on entityId == actor->entityId
	if (!pEntity)
	{
		return true;
	}

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(pGameRules)
	{
		if (pGameRules->GetTeamCount()>=2)
		{
			int entityTeam = pGameRules->GetTeam(entityId);
			if (entityTeam == 0 || m_teamWhenKilled == 0)
			{
				return true;	// entity or actor isn't on a team, hence friendly
			}
			else
			{
				return (entityTeam==m_teamWhenKilled);
			}
		}
		else
		{
			return entityId == GetEntityId();
		}
	}
	return false;
}

void CPlayer::Kill()
{
	if(IsClient())
	{
		m_pPlayerTypeComponent->OnKill();

		m_playerHealthEffect.OnKill();

		if (m_stats.flashBangStunTimer > 0.0f)
		{
			StopFlashbangEffects();
		}
		else
		{
			StopTinnitus();
		}

		ResetInteractor();
	}

	if( gEnv->bMultiplayer )
	{
		//if we have a pending loadout change (just finished one life race change etc.), change our loadout
		if( m_pendingLoadoutGroup != -1 )
		{
			if( IsClient() )
			{
				//change loadout group
				{
					CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
					pEquipmentLoadout->InvalidateLastSentLoadout();
					pEquipmentLoadout->SetPackageGroup( (CEquipmentLoadout::EEquipmentPackageGroup)m_pendingLoadoutGroup );
					pEquipmentLoadout->SetSelectedPackage( 0 );

					CGameRules *pGameRules = g_pGame->GetGameRules();
					pGameRules->SetPendingLoadoutChange();

// 					pFlashMenu->ScheduleInitialize(CFlashFrontEnd::eFlM_IngameMenu, eFFES_equipment_select);
// 					pFlashMenu->SetBlockClose(true);
				}
			}

			m_pendingLoadoutGroup = -1;
				
		}
	}

	CActor::Kill();

	if(m_hideOnDeath)
	{
		GetEntity()->Hide(true);
	}

	if (gEnv->bMultiplayer)
	{
		if (IsInPickAndThrowMode())
		{
			LockInteractor(m_stats.pickAndThrowEntity, false);

			m_stats.pickAndThrowEntity = 0;
			m_stats.isInPickAndThrowMode = false;

			IEntityClass *pNoWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("NoWeapon");
			EntityId noWeaponId = GetInventory()->GetItemByClass(pNoWeaponClass); 

			SelectItem(noWeaponId, false, true);
		}
	}
}

Vec3 CPlayer::GetStanceViewOffset(EStance _stance,const float *pLeanAmt, bool withY, bool useWhileLeanedOffsets) const
{	
	//--- Skip the NULL Entry, it gives bad startup camera
	EStance stance = max(STANCE_STAND, _stance);

	Vec3 offset(GetStanceInfo(stance)->viewOffset);
	if (!withY)
		offset.y = 0.0f;

	float leanAmt = 0.0f;

	//apply leaning
	if (!pLeanAmt)
		leanAmt += m_pPlayerRotation->GetLeanAmount();
	else
		leanAmt += *pLeanAmt;

	if (!IsPlayer())
	{
		offset = GetStanceInfo(stance)->GetViewOffsetWithLean(leanAmt, m_pPlayerRotation->GetLeanPeekOverAmount(), useWhileLeanedOffsets);
	}

	return offset;
}		

void CPlayer::SetRagdollPhysicsParams(IPhysicalEntity * pPhysEnt, bool fallAndPlay)
{
	pe_simulation_params sp;
	sp.gravity = sp.gravityFreefall = GetActorPhysics().gravity;

	sp.dampingFreefall = 0.0f;
	sp.mass = GetActorPhysics().mass * 2.0f;
	if(sp.mass <= 0.0f)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Tried ragdollizing player with 0 mass.");
		sp.mass = 80.0f;
	}

	pPhysEnt->SetParams(&sp);

	pe_params_part pp;
	if (!fallAndPlay) // Don't disable bullet collisions and so if the ragdoll is part of Fall and Play
	{
		pp.flagsAND = ~(geom_colltype_player|geom_colltype_foliage_proxy|geom_colltype_foliage|geom_colltype_explosion);
	}
	if (gEnv->bMultiplayer)
	{
		pp.flagsAND &= ~(geom_colltype_player|geom_colltype_vehicle|geom_colltype6|geom_colltype8);
	}	

	pPhysEnt->SetParams(&pp);

	if (!fallAndPlay)
	{
		// Set Collision Class to ragdoll.
		g_pGame->GetGamePhysicsSettings()->AddCollisionClassFlags(*pPhysEnt, gcc_ragdoll);
	}
}


void CPlayer::UnRagdollize()
{
	CRY_ASSERT(gEnv->bMultiplayer && !gEnv->bServer);
	CryLog("CPlayer::UnRagdollize() '%s' isRagDoll=%s", GetEntity()->GetName(), m_stats.isRagDoll ? "true" : "false");
	if(m_stats.isRagDoll)
	{
		if (m_pAnimatedCharacter)
		{
			m_pAnimatedCharacter->ResetState();
			m_pAnimatedCharacter->UpdateCharacterPtrs();
		}
		
		Physicalize();

		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);

		if (pCharacter)
		{
			pCharacter->EnableStartAnimation(true);
		}

		ResetAnimations();
		ResetAnimationState();
		RestartMannequin();

		m_stats.isRagDoll = false;
	}
}

void CPlayer::RagDollize( bool fallAndPlay )
{
	if (!fallAndPlay && m_pPickAndThrowProxy)
	{
		m_pPickAndThrowProxy->Unphysicalize();
	}

	CProceduralContextRagdoll* pRagdollContext = NULL;
	if( GetRagdollContext( &pRagdollContext ) && pRagdollContext->GetAspectProfileScope() )
	{
		// If we've no entity target, we're likely called directly (i.e. not through Mannequin).
		// In this case, we just force our EntityID into the context.
		// Note: the targetting was done for enslaved action controllers
		if( pRagdollContext->GetEntityTarget() == 0 )
		{
			pRagdollContext->SetEntityTarget( GetEntity()->GetId() );
		}
		pRagdollContext->QueueRagdoll( fallAndPlay );
		return;
	}

	if (m_stats.isRagDoll && !gEnv->pSystem->IsSerializingFile())
	{
		if(!IsPlayer() && !fallAndPlay)
			DropAttachedItems();
		return;
	}

	OnRagdollize();

	m_stats.followCharacterHead = 1;

	if (!fallAndPlay)
	{
		if(!IsPlayer())
		{
			DropAttachedItems();
		}
		m_heatController.CoolDown(0.1f);
	}
}

void CPlayer::OnRagdollize()
{
	SActorStats *pStats = GetActorStats();

	if (GetLinkedVehicle())
		return;

	if (pStats && (!pStats->isRagDoll || gEnv->pSystem->IsSerializingFile()))
	{
		GetGameObject()->SetAutoDisablePhysicsMode(eADPM_Never);

		ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
		if (pCharacter)
		{
			// dead guys shouldn't blink
			pCharacter->EnableProceduralFacialAnimation(false);
			//Anton :: SetDefaultPose on serialization
			if(gEnv->pSystem->IsSerializingFile() && pCharacter->GetISkeletonPose())
				pCharacter->GetISkeletonPose()->SetDefaultPose();
		}

		if( !gEnv->bMultiplayer )
		{
			m_pHitDeathReactions->OnRagdollize( false );
		}
		else
		{
			SRagdollizeParams params;
			params.mass = GetActorPhysics().mass;
			params.sleep = false;
			params.stiffness = GetActorParams().fallNPlayStiffness_scale;

			SGameObjectEvent event( eGFE_QueueRagdollCreation, eGOEF_ToExtensions );
			event.ptr = &params;

			GetGameObject()->SendEvent( event );
		}

		pStats->isRagDoll = true;

		CCCPOINT(CActor_RagDollize);
	}

	PhysicalizeBodyDamage();
}

void CPlayer::PostRagdollPhysicalized( bool fallAndPlay )
{
	if(gEnv->bMultiplayer )
	{
		ResetAnimations();
		if( !fallAndPlay && IsDead() && g_pGameCVars->pl_switchTPOnKill && !IsThirdPerson() )
		{
			ToggleThirdPerson();
		}
	}

	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();

	if (pPhysEnt)
	{
		SetRagdollPhysicsParams(pPhysEnt, fallAndPlay);
	}

	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	if (!fallAndPlay)
	{
		if (pCharacter)
		{
			pCharacter->EnableStartAnimation(false);
			pCharacter->EnableProceduralFacialAnimation(false);
		}
	}

	GetGameObject()->SetAutoDisablePhysicsMode(eADPM_Never);
}

void CPlayer::PostPhysicalize()
{
	CActor::PostPhysicalize();

	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	if (!pCharacter)
		return;
	ISkeletonPose * pSkeletonPose = pCharacter->GetISkeletonPose();
	pSkeletonPose->SetPostProcessCallback(PlayerProcessBones,this);
	pe_simulation_params sim;
	sim.maxLoggedCollisions = 5;
	pe_params_flags flags;
	flags.flagsOR = pef_log_collisions;

	IPhysicalEntity * pPhysicalEnt = pSkeletonPose->GetCharacterPhysics();
  if (!pPhysicalEnt)
    return;

	pPhysicalEnt->SetParams(&sim);
	pPhysicalEnt->SetParams(&flags);	

	// Set the Collision Class to player_body.
	g_pGame->GetGamePhysicsSettings()->AddCollisionClassFlags(*pPhysicalEnt, gcc_player_body);

	//set a default offset for the character, so in the editor the bbox is correct
	if(m_pAnimatedCharacter)
	{
		// Physicalize() was overriding some things for spectators on loading (eg gravity). Forcing a collider mode update
		//	will reinit it properly.
		if(GetSpectatorMode() != CActor::eASM_None)
		{
			EColliderMode mode = m_pAnimatedCharacter->GetPhysicalColliderMode();
			m_pAnimatedCharacter->ForceRefreshPhysicalColliderMode();
			m_pAnimatedCharacter->RequestPhysicalColliderMode(mode, eColliderModeLayer_Game, "Player::PostPhysicalize" );
		}
	}

	// Set correct value for gravity
	IPhysicalEntity *pPhysEnt = GetEntity()->GetPhysics();
	if (pPhysEnt)
	{
		if (!gEnv->bMultiplayer)
		{
			if (IsAIControlled())
			{
				g_pGame->GetGamePhysicsSettings()->AddCollisionClassFlags(*pPhysEnt, gcc_ai);

				pe_params_collision_class gcc_params;
				gcc_params.collisionClassOR.ignore = gcc_ai;
				pPhysEnt->SetParams(&gcc_params);
			}
		}

		pe_player_dynamics pd;
		if (pPhysEnt->GetParams(&pd))
			m_actorPhysics.gravity=pd.gravity;

		//--- Disable the landing nod on players as we now handle this in animated character
		pe_player_dynamics pdSet;
		pdSet.nodSpeed = 0.0f;
		
		
		//--- Set the client inertia
		const bool isRemoteClient = IsRemote();
		if (isRemoteClient)
		{
			m_inertia = (float)__fsel(g_pGameCVars->pl_clientInertia, g_pGameCVars->pl_clientInertia, m_inertia);
			m_inertiaAccel = m_inertia;
			pdSet.kInertia = m_inertia;
			pdSet.kInertiaAccel = m_inertia;
			pdSet.minSlideAngle = 75.f;
			pdSet.maxClimbAngle = 75.f;
		}
		pPhysEnt->SetParams(&pdSet);

		// Set the Collision Class to player_capsule.
		g_pGame->GetGamePhysicsSettings()->AddCollisionClassFlags(*pPhysEnt, gcc_player_capsule);
	}
}

void CPlayer::UpdateSpectator(float frameTime)
{
	const bool  isLocal = IsClient();
	if (isLocal)
	{
		switch(GetSpectatorMode())
		{
		case eASM_Follow:
			{
				EntityId spectatorTarget = GetSpectatorTarget();
				SSpectatorInfo& specInfo = m_stats.spectatorInfo;

				CGameRules *pGameRules = g_pGame->GetGameRules();
				IGameRulesSpawningModule*	pSpawningMo = pGameRules->GetSpawningModule();
				int numLives = pSpawningMo->GetNumLives();
				if (numLives > 0)
				{
					int remainingLives = pSpawningMo->GetRemainingLives(spectatorTarget);

					if (remainingLives == 0)
					{
						// spectator target has been eliminated move along onto the next player
						float newTime = specInfo.dataU.follow.invalidTargetTimer + frameTime;

						if (newTime > g_pGameCVars->g_spectate_skipInvalidTargetAfterTime)
						{
							CryLog("CPlayer::UpdateSpectator() we've been looking at an invalid target for long enough, finding a new valid target");
							newTime=0.f;

							IGameRulesSpectatorModule *pSpectatorModule = pGameRules->GetSpectatorModule();
							if (pSpectatorModule)
							{
								EntityId nextTargetId = pSpectatorModule->GetNextSpectatorTarget(GetEntityId(), 1);
								SetSpectatorModeAndOtherEntId(CActor::eASM_Follow, nextTargetId);
							}
						}

						specInfo.dataU.follow.invalidTargetTimer = newTime;

					}
				}

				if(specInfo.rotateSpeedSingleFrame)
				{
					specInfo.yawSpeed = 0.f;
					specInfo.pitchSpeed = 0.f;
					specInfo.rotateSpeedSingleFrame = false;
				}
			}
			break;
		case eASM_Killer:
			{
				CGameRules *pGameRules = g_pGame->GetGameRules();
				IGameRulesSpectatorModule *pSpectatorModule = pGameRules->GetSpectatorModule();
				CRY_ASSERT( pSpectatorModule );

				float killerCamDuration = 0.f;
				float deathCam = 0.f, killCam = 0.f;
				pSpectatorModule->GetPostDeathDisplayDurations( GetEntityId(), m_teamWhenKilled, false, false, deathCam, killerCamDuration, killCam );

				const float currentTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
				const float timeOut = m_stats.spectatorInfo.dataU.killer.startTime + killerCamDuration;

				if( currentTime > timeOut )
				{
					pSpectatorModule->ChangeSpectatorModeBestAvailable(this, true);
				}
			}
			break;
		}
	}
			


#ifdef SPECTATE_DEBUG
	if(g_pGameCVars->g_spectate_Debug)
	{
		if ((g_pGameCVars->g_spectate_Debug == 1 && IsClient()) || g_pGameCVars->g_spectate_Debug==2)
		{
			if (g_pGameCVars->g_spectate_Debug == 2)
			{
				CryWatch("Player: %s (isLocal=%d)", GetEntity()->GetName(), IsClient());
			}

			const CActor::EActorSpectatorState specatorState = GetSpectatorState();
			const char* szSpecState;
			switch(specatorState)
			{
			case eASS_None:
				szSpecState = "eASS_None, Just joined - fixed spectating, no actor.";
				break;
			case eASS_Ingame:
				szSpecState = "eASS_Ingame, Ingame, dead ready to respawn, with actor.";
				break;
			case eASS_ForcedEquipmentChange:
				szSpecState = "eASS_ForcedEquipmentChange, has actor, forced to change loadout probably due to round change or race swap";
				break;
			case eASS_SpectatorMode:
				szSpecState = "eASS_SpectatorMode, Is currently viewing the game as a spectator - has an actor, but will not respawn unless they leave this mode";
				break;
			default:
				szSpecState = "INVALID SPECTATOR STATE!";
				break;
			}

			EActorSpectatorMode spectatorMode = (EActorSpectatorMode)GetSpectatorMode();
			const char* szSpecMode;

			switch(spectatorMode)
			{
			case eASM_None: 
				szSpecMode = "eASM_None = 0, // normal, non-spectating";
				break;
			//case eASM_FirstMPMode: // fall-thtough intentional
			case eASM_Fixed: 
				szSpecMode = "eASM_Fixed or eASM_FirstMPMode, fixed position camera";
				break;
			case eASM_Free:
				szSpecMode = "eASM_Free, free roaming, no collisions";
				break;
			//case eASM_LastMPMode:
			case eASM_Follow:
				szSpecMode = "eASM_LastMPMode or eASM_Follow, follows an entity in 3rd person";
				break;
			case eASM_Cutscene: 
				szSpecMode = "eASM_Cutscene, HUDInterfaceEffects.cpp sets this";
				break;
			default :
				szSpecMode = "UNKNOW SPECTATOR MODE!";
				break;
			}

			const EntityId watchingId = GetSpectatorTarget();
			const IEntity* pWatchingEnt = gEnv->pEntitySystem->GetEntity(watchingId);
			const char* szWatching = "not watching anyone";
			if(pWatchingEnt)
			{
				szWatching = pWatchingEnt->GetName();
			}

			CryWatch("Spectator State  : %s", szSpecState);
			CryWatch("Spectator Mode   : %s", szSpecMode);
			if(spectatorMode == eASM_Follow)
			{
				const char* settingsName = GetCurrentFollowCameraSettings().m_settingsName;
				CryWatch("	Follow Settings: %s", settingsName);
			}
			CryWatch("Spectator Target : %s", szWatching);
		}
	}
#endif // !defined(_RELEASE)
}

void CPlayer::PostUpdate(float frameTime)
{
	CActor::PostUpdate(frameTime);
	if (m_pPlayerInput.get())
		m_pPlayerInput->PostUpdate();

	// When animation controls the entity directly, update the base/view matrices to reflect this
	IAnimatedCharacter* pAnimChar = GetAnimatedCharacter();
	if (pAnimChar)
	{
		if (!IsAIControlled())
		{
			m_pPlayerRotation->SetBaseQuat(GetEntity()->GetRotation());
		}
		else
		{
			EMovementControlMethod mcmh = pAnimChar->GetMCMH();
			if ((mcmh == eMCM_Animation) || (mcmh == eMCM_AnimationHCollision))
			{
				SetViewRotation(pAnimChar->GetAnimLocation().q);
			
				if (!IsThirdPerson())
				{
					const Quat viewRotation = GetBaseQuat() * GetCameraTran().q;
					SetViewRotation(viewRotation);
				}
			}
		}
	}

	UpdateSpectator(frameTime);
}

void CPlayer::CameraShake(float angle,float shift,float duration,float frequency,Vec3 pos,int ID,const char* source) 
{
	float angleAmount(max(-90.0f,min(90.0f,angle)) * gf_PI/180.0f);
	float shiftAmount(shift);
  
  if (IVehicle* pVehicle = GetLinkedVehicle())
  {
    if (IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(GetEntityId()))    
      pSeat->OnCameraShake(angleAmount, shiftAmount, pos, source);    
  }

	Ang3 shakeAngle(
		cry_random(0.0f,1.0f)*angleAmount*0.15f, 
		(angleAmount*std::min(1.0f,std::max(-1.0f,cry_random(-1.0f, 1.0f)*7.7f)))*1.15f,
		cry_random(-1.0f,1.0f)*angleAmount*0.05f
		);

	Vec3 shakeShift(cry_random(-1.0f, 1.0f)*shiftAmount,0,cry_random(-1.0f, 1.0f)*shiftAmount);

	IView *pView = g_pGame->GetIGameFramework()->GetIViewSystem()->GetViewByEntityId(GetEntityId());
	if (pView)
		pView->SetViewShake(shakeAngle,shakeShift,duration,frequency,0.5f,ID);
}
  

void CPlayer::ResetAnimations()
{
	ICharacterInstance *pCharacter = GetEntity()->GetCharacter(0);
	ICharacterInstance *pShadowChar = GetShadowCharacter();

	if (pCharacter)
	{
		pCharacter->GetISkeletonAnim()->StopAnimationsAllLayers();
		
		IAnimationPoseBlenderDir* pIPoseBlenderLook = pCharacter->GetISkeletonPose()->GetIPoseBlenderLook();
		if (pIPoseBlenderLook)
		{
			pIPoseBlenderLook->SetState(false);
			pIPoseBlenderLook->SetLayer(GetLookIKLayer(m_params));
		}
	}

	if (pShadowChar)
	{
		pShadowChar->GetISkeletonAnim()->StopAnimationsAllLayers();
	}
}


//////////////////////////////////////////////////////////////////////////
// if, for some reason, a hit going to be applied into this player should instead be totally ignored
bool CPlayer::ShouldFilterOutHit ( const HitInfo& hit )
{
	if (IsInPickAndThrowMode() && GetInventory())
	{
		// grab current item (dont care about vehicle weapons hence 'false' param)
		static IEntityClass* pPickAndThrowWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("PickAndThrowWeapon");
		IItem* pItem																  = GetCurrentItem(false); 
		if(pItem)
		{
			if (pItem->GetEntity()->GetClass() == pPickAndThrowWeaponClass)
			{
				CPickAndThrowWeapon* pPickAndThrowWeapon = static_cast<CPickAndThrowWeapon*>(pItem);
				return pPickAndThrowWeapon->ShouldFilterOutHitOnOwner( hit );
			}
		}
	}
	
	return false;
}


//////////////////////////////////////////////////////////////////////////
// if, for some reason, explosion damage is going to be applied into this player should instead be totally ignored
bool CPlayer::ShouldFilterOutExplosion ( const HitInfo& hitInfo )
{
	if (IsInPickAndThrowMode() && GetInventory())
	{
		// grab current item (dont care about vehicle weapons hence 'false' param)
		static IEntityClass* pPickAndThrowWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("PickAndThrowWeapon");
		IItem* pItem																  = GetCurrentItem(false); 
		if(pItem)
		{
			if (pItem->GetEntity()->GetClass() == pPickAndThrowWeaponClass)
			{
				CPickAndThrowWeapon* pPickAndThrowWeapon = static_cast<CPickAndThrowWeapon*>(pItem);
				return pPickAndThrowWeapon->ShouldFilterOutExplosionOnOwner( hitInfo );
			}
		}
	}

	return false;
}


void CPlayer::SetHealth(float health )
{
	if( health != m_health.GetHealth() )
	{
		const bool isClient = IsClient();

		float oldHealth = m_health.GetHealth();

		CActor::SetHealth(health);

		CALL_PLAYER_EVENT_LISTENERS(OnHealthChanged(this, m_health.GetHealth()));

		if(oldHealth > m_health.GetHealth())
		{
			CCCPOINT_IF(isClient, PlayerState_LocalPlayerHealthReduced);
			CCCPOINT_IF(!isClient, PlayerState_OtherPlayerHealthReduced);

			InformHealthHasBeenReduced();
		}

		if(isClient)
		{
			if ( (oldHealth > 0.0f) && (oldHealth < m_health.GetHealth()) && (m_health.GetHealth() == m_health.GetHealthMax()) )
			{
				CPersistantStats::GetInstance()->ClientRegenerated();
			}
		}

		if ((m_health.GetHealth() <= 0.0f) && (IsGod() > 0))
		{
			CALL_PLAYER_EVENT_LISTENERS(OnDeath(this, true));
		}

		if ((oldHealth > 0.0f) && (m_health.GetHealth() <= 0.0f))	//client deathFX are triggered in the lua gamerules
		{
			StateMachineHandleEventMovement( PLAYER_EVENT_DEAD );

			if ( isClient )
			{
// 				if (CFlashFrontEnd *pFlashFrontEnd = g_pGame->GetFlashMenu())
// 				{
// 					pFlashFrontEnd->OnLocalPlayerDeath();
// 				}
			}
		}
		else if(isClient && m_pPlayerTypeComponent)
		{
			m_pPlayerTypeComponent->UpdatePlayerLowHealthStatus(oldHealth);
		}

		CHANGED_NETWORK_STATE(this, ASPECT_HEALTH);
	}
}


void CPlayer::SetClientSoundmood(EClientSoundmoods soundmood)
{
	CRY_ASSERT(IsClient());
	m_pPlayerTypeComponent->SetClientSoundmood(soundmood);
}

CPlayer::EClientSoundmoods CPlayer::FindClientSoundmoodBestFit() const
{
	CRY_ASSERT(IsClient());
	return m_pPlayerTypeComponent->FindClientSoundmoodBestFit();
}

void CPlayer::AddClientSoundmood(EClientSoundmoods soundmood)
{
	CRY_ASSERT(IsClient());
	m_pPlayerTypeComponent->AddClientSoundmood(soundmood);
}

void CPlayer::RemoveClientSoundmood(EClientSoundmoods soundmood)
{
	CRY_ASSERT(IsClient());
	m_pPlayerTypeComponent->RemoveClientSoundmood(soundmood);
}

void CPlayer::OnStartRecordingPlayback()
{
	SetClientSoundmood(ESoundmood_Killcam);
}

void CPlayer::OnStopRecordingPlayback()
{
	if(GetSpectatorMode() == CActor::eASM_None)
	{
		SetClientSoundmood(ESoundmood_Alive);
	}
	else
	{
		SetClientSoundmood(ESoundmood_Spectating);
	}
}

void CPlayer::OnRecordingPlaybackBulletTime(bool bBulletTimeActive)
{
	if (bBulletTimeActive)
	{
		SetClientSoundmood(ESoundmood_KillcamSlow);
	}
	else if (ESoundmood_KillcamSlow == m_pPlayerTypeComponent->GetSoundmood())
	{
		SetClientSoundmood(ESoundmood_Killcam);
	}
}

//-------------------------------------------------
// This calculates the regeneration amount in a given amount of time, assuming that the
// m_lastTimeDamaged value was updated once or less during that period

float CPlayer::GetRegenerationAmount(float frameTime)
{
	const float elapsedTime = gEnv->pTimer->GetCurrTime() - m_lastTimeDamaged.GetSeconds();
	float regenAmount = 0.0f;
	float timeThreshold = 0.f;
	float recharge = 0.f;

	// player can't regenerate with head underwater
	if (!m_isHeadUnderWater)
	{
		if (gEnv->bMultiplayer)
		{
			timeThreshold = g_pGameCVars->pl_health.normal_threshold_time_to_regenerateMP;
			const int regenRate = g_pGameCVars->g_mpRegenerationRate;
			if (regenRate == 1)
			{
				recharge = g_pGameCVars->pl_health.normal_regeneration_rateMP * frameTime;
			}
			else if (regenRate == 0)
			{
				recharge = g_pGameCVars->pl_health.slow_regeneration_rateMP * frameTime;
			}
			else  if (regenRate == 2)
			{
				recharge = g_pGameCVars->pl_health.fast_regeneration_rateMP * frameTime;
			}
			else //if (regenRate == 3)
			{
				recharge = 0.f;
			}
		}
		else
		{
			timeThreshold = g_pGameCVars->pl_health.normal_threshold_time_to_regenerateSP;
			recharge = g_pGameCVars->pl_health.normal_regeneration_rateSP * frameTime;
		}
		regenAmount = (float)__fsel((timeThreshold-elapsedTime), 0.0f, recharge);
	}

	return regenAmount;
}

//-------------------------------------------------
void CPlayer::UpdateHealthRegeneration(float fHealth, float frameTime)
{
	IGameRulesObjectivesModule * objectives = g_pGame->GetGameRules()->GetObjectivesModule();
	if(!objectives || objectives->CanPlayerRegenerate(GetEntityId()))
	{
		//Health update
		const float maxHealth = GetMaxHealth();
		if(fHealth < maxHealth)
		{
			//Check if it's possible to update
			const float regenAmount = GetRegenerationAmount(frameTime);
			if(regenAmount > 0.0f)
			{
				CCCPOINT_IF(IsClient(), PlayerState_LocalPlayerHealthRegenerate);
				CCCPOINT_IF(! IsClient(), PlayerState_OtherPlayerHealthRegenerate);
				SetHealth(min(fHealth+regenAmount,maxHealth));
			}
		}
	}
}

//------------------------------------------------------------------------

void CPlayer::SetAngles(const Ang3 &angles) 
{
	Matrix33 rot(Matrix33::CreateRotationXYZ(angles));
	CMovementRequest mr;
	mr.SetLookTarget( GetEntity()->GetWorldPos() + 20.0f * rot.GetColumn(1) );
	m_pMovementController->RequestMovement(mr);
}

Ang3 CPlayer::GetAngles() 
{
	if(IsClient() && GetLinkedVehicle())
		return Ang3(m_clientViewMatrix);

	return Ang3( m_pPlayerRotation->GetViewQuatFinal() );
}

void CPlayer::AddAngularImpulse(const Ang3 &angular,float deceleration,float duration)
{
	m_pPlayerRotation->AddAngularImpulse( angular, deceleration, duration );
}

void CPlayer::SelectNextItem(int direction, bool keepHistory, int category)
{
	IItem* pItem = GetCurrentItem();
	bool rippedOff = pItem && (static_cast<CItem*>(pItem)->IsRippedOff());

	if(!CanSwitchItems())
		return;
	
	if(rippedOff)
		UseItem(pItem->GetEntityId());
	else
		CActor::SelectNextItem(direction, keepHistory, category);
}

void CPlayer::HolsterItem_NoNetwork(bool holster, bool playSelect, float selectSpeedBias, bool hideLeftHandObject)
{
	CActor::HolsterItem(holster, playSelect, selectSpeedBias,hideLeftHandObject);	
}

void CPlayer::HolsterItem(bool holster, bool playSelect, float selectSpeedBias, bool hideLeftHandObject)
{
	HolsterItem_NoNetwork(holster, playSelect, selectSpeedBias, hideLeftHandObject);

	CHANGED_NETWORK_STATE(this, ASPECT_CURRENT_ITEM);
}

void CPlayer::SelectLastItem(bool keepHistory, bool forceNext /* = false */)
{
	if (IsInPickAndThrowMode())
		return;
		
	CActor::SelectLastItem(keepHistory, forceNext);
}

void CPlayer::SelectItemByName(const char *name, bool keepHistory, bool forceFastSelect)
{
	if (IsInPickAndThrowMode())
		return;
		
	CActor::SelectItemByName(name, keepHistory, forceFastSelect);
}

bool CPlayer::ScheduleItemSwitch(EntityId itemId, bool keepHistory, int category, bool forceFastSelect)
{
	if(CActor::ScheduleItemSwitch(itemId, keepHistory,category, forceFastSelect))
	{
		CHANGED_NETWORK_STATE(this, ASPECT_CURRENT_ITEM);
		return true;
	}

	return false;
}

void CPlayer::SelectItem(EntityId itemId, bool keepHistory, bool forceSelect)
{
	if (gEnv->bMultiplayer && IsClient())
	{
// 		CFlashFrontEnd* pFrontEnd = g_pGame->GetFlashMenu();
// 		if(pFrontEnd!=NULL && pFrontEnd->IsCustomizing())
// 		{
// 			pFrontEnd->Schedule(eSchedule_Clear);
// 		}
	}

	if (IsInPickAndThrowMode())
	{
		//check we actually have a pick and throw weapon, we could be just selecting it now
		EntityId currentItem = GetCurrentItemId();
		if( currentItem != 0 && g_pGame->GetIGameFramework()->QueryGameObjectExtension( currentItem, "PickAndThrowWeapon" ) )
		{
			return;
		}
	}

	CActor::SelectItem(itemId, keepHistory, forceSelect);

	CHANGED_NETWORK_STATE(this, ASPECT_CURRENT_ITEM);
}

void CPlayer::NotifyCurrentItemChanged(IItem* newItem)
{
	StateMachineHandleEventMovement( SStateEvent::CreateStateEvent( PLAYER_EVENT_WEAPONCHANGED, newItem ) );
	}

bool CPlayer::SetAspectProfile(EEntityAspects aspect, uint8 profile )
{
	CProceduralContextRagdoll* pRagdollContext = NULL;
	if( GetRagdollContext( &pRagdollContext ) )
	{
		pRagdollContext->SetAspectProfileScope( true );
	}

	bool res=CActor::SetAspectProfile(aspect, profile);

	uint8 currentphys=GetGameObject()->GetAspectProfile(eEA_Physics);
	bool setViewFromOrientation = false;

	if (aspect == eEA_Physics && profile==eAP_Alive)
	{
		m_stats.isRagDoll=false;

		if (currentphys==eAP_Sleep)
		{
			setViewFromOrientation = true;

			if (m_pPickAndThrowProxy)
				m_pPickAndThrowProxy->Physicalize();
		}
	}

	if (res && setViewFromOrientation)
	{
		QuatT quat( GetEntity()->GetWorldTM() );
		SetViewRotation( quat.q ); // sets the view to the actual orientation. else, the actor would try to rotate back to the view position while standing up, or jump after)
	}

	if( pRagdollContext )
	{
		pRagdollContext->SetAspectProfileScope( false );
	}

	return res;
}

void CPlayer::InformHealthHasBeenReduced()
{
	m_lastTimeDamaged = gEnv->pTimer->GetCurrTime();
}

bool CPlayer::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (!CActor::NetSerialize(ser, aspect, profile, flags))
		return false;

	PrefetchLine(&m_currentlyTargettingPlugin, 0);

	bool reading=ser.IsReading();

	switch(aspect)
	{
		case ASPECT_HEALTH:
			NetSerialize_Health(ser, reading);
			break;
		case ASPECT_CURRENT_ITEM:
			NetSerialize_CurrentItem(ser, reading);
			break;
		case ASPECT_RANK_CLIENT:
			m_netPlayerProgression.Serialize(ser, aspect);
			break;
		case ASPECT_SNAP_TARGET:
			NetSerialize_SnapTarget(ser, reading);
			break;
		case ASPECT_INTERACTIVE_OBJECT:
			NetSerialize_InteractiveObject(ser, reading);
			break;
		case ASPECT_SPECTATOR:
			NetSerialize_Spectator(ser, reading);
			break;
 		case ASPECT_LEDGEGRAB_CLIENT:
 			NetSerialize_LedgeGrab(ser, reading);
 			break;
		case ASPECT_JUMPING_CLIENT:
			NetSerialize_Jumping(ser, reading);
			break;
		case ASPECT_FLASHBANG_SHOOTER_CLIENT:
			NetSerialize_FlashBang(ser, reading);
			break;		
		case ASPECT_BATTLECHATTER_CLIENT:
			{
				CGameRules* pGameRules = g_pGame->GetGameRules();
				CBattlechatter* pBattlechatter = pGameRules->GetBattlechatter();
				pBattlechatter->NetSerialize(this, ser, aspect);
				break;
			}			
		case ASPECT_LAST_MELEE_HIT:
			NetSerialize_Melee(ser, reading);
			break;
		case ASPECT_VEHICLEVIEWDIR_CLIENT:
			ser.Value("VehicleViewRotation", m_vehicleViewDir, 'dir0');
			break;
		case ASPECT_INPUT_CLIENT:
			NetSerialize_InputClient(ser, reading);
			break;
		case ASPECT_INPUT_CLIENT_AUGMENTED:
			NetSerialize_InputClient_Aug(ser, reading);
			break;
		case ASPECT_STEALTH_KILL:
			NetSerialize_StealthKill(ser, reading);
			break;
		case ASPECT_LADDER_SERVER:
			NetSerialize_Ladder(ser, reading);
			break;
		default:
			break;
	}

	m_currentlyTargettingPlugin.NetSerialize(ser, aspect, profile, flags);

	if (g_pGame->GetGameRules()->GetPlayerStatsModule())
	{
		g_pGame->GetGameRules()->GetPlayerStatsModule()->NetSerialize(GetEntityId(), ser, aspect, profile, flags);
	}

	return true;
}

void CPlayer::NetSerialize_FlashBang( TSerialize ser, bool bReading )
{
	m_netFlashBangStun = (m_stats.flashBangStunTimer > 0.0f);
	EntityId previousFlashbangShooterId = m_lastFlashbangShooterId;
	ser.Value("LastFlashbangShooter", m_lastFlashbangShooterId, 'eid');
	ser.Value("FlashBangStun", m_netFlashBangStun, 'bool');
	if(bReading && m_lastFlashbangShooterId != previousFlashbangShooterId)
	{
		m_lastFlashbangTime = gEnv->pTimer->GetCurrTime();
	}
}

void CPlayer::NetSerialize_Jumping( TSerialize ser, bool bReading )
{
	NET_PROFILE_SCOPE("Jumping", bReading);
	ser.Value("JumpVelocity", m_jumpVel, 'jmpv');
	ser.Value("jumpCounter", this, &CPlayer::GetJumpCounter, &CPlayer::SetJumpCounter, 'ui3');
}

void CPlayer::NetSerialize_LedgeGrab( TSerialize ser, bool bReading )
{
	NET_PROFILE_SCOPE("Ledge Grab", bReading);
	ser.Value("ledgeId", m_ledgeID, 'ui16');
	
	CRY_ASSERT_MESSAGE(m_ledgeFlags <= 3, "ledgeflags are out of range of compression policy ui2!" );
	ser.Value("ledgeFlags", m_ledgeFlags, 'ui2'); // must be serialised BEFORE ledgeCounter

	ser.Value("ledgeCounter", this, &CPlayer::GetLedgeCounter, &CPlayer::SetLedgeCounter, 'ui3');
}

void CPlayer::SetSpectatorTarget(EntityId targetId)
{
	assert(m_stats.spectatorInfo.mode==eASM_Follow||m_stats.spectatorInfo.mode==eASM_Killer);

	EntityId& currTarget = *m_stats.spectatorInfo.GetOtherEntIdPtrForCurMode();
	
	if(IsClient() && currTarget != targetId && m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Spectator ) )
	{
		StateMachineHandleEventMovement( PLAYER_EVENT_RESET_SPECTATOR_SCREEN );
		currTarget = targetId;
	}	
}

void CPlayer::SetSpectatorFixedLocation(EntityId locId)
{
	assert(m_stats.spectatorInfo.mode==eASM_Fixed);

	if(IsClient() && m_stats.spectatorInfo.dataU.fixed.location != locId  && m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Spectator ))
	{
		StateMachineHandleEventMovement( PLAYER_EVENT_RESET_SPECTATOR_SCREEN );
		m_stats.spectatorInfo.dataU.fixed.location = locId;
	}
}

float CPlayer::GetSpectatorOrbitYawSpeed() const
{
	const SSpectatorInfo& specInfo = m_stats.spectatorInfo;
	return specInfo.yawSpeed;
}

void CPlayer::SetSpectatorOrbitYawSpeed( float yawSpeed, bool singleFrame )
{
	SSpectatorInfo& specInfo = m_stats.spectatorInfo;
	specInfo.yawSpeed = yawSpeed; 
	specInfo.rotateSpeedSingleFrame = singleFrame;
}

bool CPlayer::CanSpectatorOrbitYaw() const
{ 
	CRY_ASSERT_MESSAGE(m_pPlayerTypeComponent, "CPlayer::CanSpectatorOrbitYaw() - Localplayercomponent not found. This function should only be called on the client player");

#ifndef _RELEASE
	return g_pGameCVars->g_spectate_follow_orbitEnable > 0 || (m_pPlayerTypeComponent->GetCurrentFollowCameraSettings().m_flags & SFollowCameraSettings::eFCF_AllowOrbitYaw);
#else
	return (m_pPlayerTypeComponent->GetCurrentFollowCameraSettings().m_flags & SFollowCameraSettings::eFCF_AllowOrbitYaw) > 0;
#endif // _RELEASE
}

float CPlayer::GetSpectatorOrbitPitchSpeed() const
{
	const SSpectatorInfo& specInfo = m_stats.spectatorInfo;
	return specInfo.pitchSpeed;
}

void CPlayer::SetSpectatorOrbitPitchSpeed( float pitchSpeed, bool singleFrame )
{
	SSpectatorInfo& specInfo = m_stats.spectatorInfo;
	specInfo.pitchSpeed = pitchSpeed; 
	specInfo.rotateSpeedSingleFrame = singleFrame;
}

bool CPlayer::CanSpectatorOrbitPitch() const
{ 
	CRY_ASSERT_MESSAGE(m_pPlayerTypeComponent, "CPlayer::CanSpectatorOrbitPitch() - Localplayercomponent not found. This function should only be called on the client player");

#ifndef _RELEASE
	return g_pGameCVars->g_spectate_follow_orbitEnable > 0 || (m_pPlayerTypeComponent->GetCurrentFollowCameraSettings().m_flags & SFollowCameraSettings::eFCF_AllowOrbitPitch);
#else
	return (m_pPlayerTypeComponent->GetCurrentFollowCameraSettings().m_flags & SFollowCameraSettings::eFCF_AllowOrbitPitch) > 0;
#endif // _RELEASE
}

void CPlayer::NetSerialize_Spectator( TSerialize ser, bool bReading )
{
	NET_PROFILE_SCOPE("Spectator", ser.IsReading());

	SSpectatorInfo*  spinf = &m_stats.spectatorInfo;
	uint8 mode = 0;
	EntityId id = 0;

	if(!bReading)
	{
		EntityId *ptr = spinf->GetOtherEntIdPtrForCurMode();
		mode = spinf->mode;
		id = ptr ? *ptr : 0;
	}

	uint8 state = m_stats.spectatorInfo.state;
	uint8 stateWas = state;

	ser.Value("state", state, 'ui2');
	ser.Value("mode", mode, 'spec');
	ser.Value("id", id, 'eid');
	int teamId = m_teamId;
	ser.Value("team", teamId, 'team');

	if (bReading)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		if (teamId != m_teamId)
		{
			if (pGameRules)
			{
				pGameRules->ClDoSetTeam(teamId, GetEntityId());
			}
		}

		if (IsClient())
		{
			CRY_ASSERT(!gEnv->bServer);
			OnLocalSpectatorStateSerialize((CActor::EActorSpectatorState)state, (CActor::EActorSpectatorState)m_stats.spectatorInfo.state);
		}

		if (state != m_stats.spectatorInfo.state)
		{
			SetSpectatorState(state);
		}

		bool bIntro = false; 
		if(pGameRules)
		{
			IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
			if (pStateModule)
			{
				bIntro = (pStateModule->GetGameState() == IGameRulesStateModule::EGRS_Intro) && g_pGame->GetGameRules()->IsIntroSequenceRegistered();
			}
		}

#ifndef _RELEASE
		if(g_pGame->GetGameRules()->IntroSequenceHasCompletedPlaying() && bIntro)
		{
			CryLogAlways("[SPECTATE NETSERIALISE]: Still in intro state, but intro has completed playing and we are being told to enter spec-tate mode - did we finish intro before server? - will handle and accept netserialize request");
		}
#endif // #ifndef _RELEASE

		// if intro sequence has COMPLETED playing, then *always* accept spectator states. Can't rely on being out of EGRS_INTRO yet, as server may have loaded slower than us!
		if (mode != eASM_None && (!bIntro || g_pGame->GetGameRules()->IntroSequenceHasCompletedPlaying()))	// the only way to disable spectator mode currently should be through spawning OR if you are playing an intro!
		{
			SetSpectatorModeAndOtherEntId(mode, id);
		}

		if(state != stateWas && (state == eASS_SpectatorMode || stateWas == eASS_SpectatorMode))
		{
			OnSpectateModeStatusChanged(state == eASS_SpectatorMode);

			if(state == eASS_SpectatorMode && m_pPlayerTypeComponent)
			{
				m_pPlayerTypeComponent->SetHasEnteredSpectatorMode();
			}
		}
	}
}

void CPlayer::NetSerialize_InputClient( TSerialize ser, bool bReading )
{
	NET_PROFILE_SCOPE("PlayerInput", bReading);
	SSerializedPlayerInput serializedInput;
	if (m_pPlayerInput.get())
	{
		m_pPlayerInput->GetState(serializedInput);
		if (!IsRemote() && !bReading)
		{
			serializedInput.position	= GetEntity()->GetPos();
			serializedInput.physCounter = GetNetPhysCounter();
		}
	}
	
	NET_PROFILE_BEGIN("SerializedInput::Serialize", ser.IsReading());
	serializedInput.Serialize(ser, (EEntityAspects)ASPECT_INPUT_CLIENT);

	// STANCE_NULL is set initially for AI players.
	assert(serializedInput.stance == static_cast<uint8>(STANCE_NULL) ||
		serializedInput.stance < STANCE_LAST);	// asserting here catches both read and write

	NET_PROFILE_END();

	if (bReading)
	{
		if(m_pPlayerInput.get())
		{
			m_pPlayerInput->SetState(serializedInput);
		}
		else
		{
			m_recvPlayerInput = serializedInput;
			m_recvPlayerInput.isDirty = true;
		}
	}

	{
		ESlideState prevSlideState = eSlideState_None;
		const bool bSlideActive = m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Sliding );
		if( bSlideActive )
		{
			if( m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_ExitingSlide ) )
			{
				prevSlideState = eSlideState_ExitingSlide;
			}
			else
			{
				prevSlideState = eSlideState_Sliding;
			}
		}

		int slidingState = prevSlideState;
		ser.Value("sliding", slidingState , 'ui2');

		if (ser.IsReading() && (slidingState != prevSlideState))
		{
			int playerStateEvent = EVENT_NONE;
			if( slidingState != eSlideState_None )
			{
				switch( slidingState )
				{
				case eSlideState_Sliding:
					playerStateEvent = PLAYER_EVENT_SLIDE;

					m_stateMachineMovement.StateMachineAddFlag( EPlayerStateFlags_NetSlide );
					break;
				}
			}
			else if( bSlideActive )
			{
				m_stateMachineMovement.StateMachineClearFlag( EPlayerStateFlags_NetSlide|EPlayerStateFlags_NetExitingSlide );

				switch( slidingState )
				{
				case eSlideState_Sliding:
					m_stateMachineMovement.StateMachineAddFlag( EPlayerStateFlags_NetSlide );
					break;
				case eSlideState_ExitingSlide:
					m_stateMachineMovement.StateMachineAddFlag( EPlayerStateFlags_NetExitingSlide );
					break;
				case eSlideState_None:
					playerStateEvent = PLAYER_EVENT_SLIDE;
					break;
				}
			}

			if( playerStateEvent != EVENT_NONE )
			{
				StateMachineHandleEventMovement( playerStateEvent );
			}
		}
	}
}

void CPlayer::NetSerialize_InputClient_Aug( TSerialize ser, bool bReading )
{
	// This aspect is used for serialising what the player is standing on
	NET_PROFILE_SCOPE("PlayerInput_Aug", bReading);
	SSerializedPlayerInput serializedInput;

	if (m_pPlayerInput.get())
		m_pPlayerInput->GetState(serializedInput);

	if (!bReading && !IsRemote())
	{
		serializedInput.position = GetEntity()->GetPos();
	}
	
	NET_PROFILE_BEGIN("SerializedInput::Serialize", ser.IsReading());
	serializedInput.Serialize(ser, (EEntityAspects)ASPECT_INPUT_CLIENT_AUGMENTED);
	NET_PROFILE_END();

	if (bReading && m_pPlayerInput.get())
	{
		m_pPlayerInput->SetState(serializedInput);
	}
}

void CPlayer::NetSerialize_InteractiveObject( TSerialize ser, bool bReading )
{
	ser.Value("interactionIndex", m_lastCachedInteractionIndex, 'i8');

	// read/write interacting entity ID, and start interaction if reading
	EntityId interactingEntityId = GetInteractingEntityId(); 
	ser.Value("interactEnt", interactingEntityId, 'eid');

	if(bReading)
	{
		if( interactingEntityId != 0)
		{
			CInteractiveObjectEx *pInteractiveObjEx = static_cast<CInteractiveObjectEx*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(interactingEntityId, "InteractiveObjectEx"));
			if(pInteractiveObjEx)
			{
				// Make sure we run through the interactive objects start use func, rather than bypassing, else its state will not be set to eState_InUse
				pInteractiveObjEx->StartUseSpecific(GetEntityId(), m_lastCachedInteractionIndex);
			}
		}
	}

	m_largeObjectInteraction.NetSerialize(ser);
}

void CPlayer::NetSerialize_SnapTarget( TSerialize ser, bool bReading )
{
	EntityId previousSnapTargetId = m_netCloseCombatSnapTargetId;

	if(!bReading)
	{
		m_netCloseCombatSnapTargetId = g_pGame->GetAutoAimManager().GetCloseCombatSnapTarget();
	}

	ser.Value("snapTargId", m_netCloseCombatSnapTargetId, 'eid');

	if(m_netCloseCombatSnapTargetId != previousSnapTargetId)
	{
		if(previousSnapTargetId)
		{
			IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(previousSnapTargetId);
			IAnimatedCharacter* pTargChar = pActor ? pActor->GetAnimatedCharacter() : NULL;

			if(pTargChar)
			{
				if (pTargChar->GetPhysicalColliderMode() == eColliderMode_NonPushable)
				{
					pTargChar->RequestPhysicalColliderMode(eColliderMode_Pushable, eColliderModeLayer_Game, "NetSerialiseSnapTarget");
				}
			}
		}

		IActor* pActor = NULL;
		if(m_netCloseCombatSnapTargetId)
		{
			pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_netCloseCombatSnapTargetId);
			IAnimatedCharacter* pTargChar = pActor ? pActor->GetAnimatedCharacter() : NULL;

			if(pTargChar)
			{
				if (pTargChar->GetPhysicalColliderMode() == eColliderMode_Pushable)
				{
					pTargChar->RequestPhysicalColliderMode(eColliderMode_NonPushable, eColliderModeLayer_Game, "NetSerialiseSnapTarget");
				}
			}
		}

	}
}

void CPlayer::NetSerialize_StealthKill( TSerialize ser, bool bReading )
{
	bool inKill = false; 
	CActor* pVictim = NULL;
	EntityId killVictim = 0;
	uint8	animIndex = 0;

	if(!bReading)
	{
		inKill = m_stealthKill.IsBusy();
		pVictim = m_stealthKill.GetTarget();

		if(inKill && pVictim)
		{
			killVictim = pVictim->GetEntityId();
			animIndex = m_stealthKill.GetAnimIndex();
		}
	}

	ser.Value("inStealthKill", inKill, 'bool');
	ser.Value("stealthKillTarget", killVictim, 'eid');

	if(animIndex > 3)
		CryFatalError("Index used for stealth kill too high. Value %d, max 3 Change the compression policy of sk_animIdx to use more bits", animIndex);

	ser.Value("sk_animIdx", animIndex, 'ui2');

	if(ser.IsReading())
	{
		NetSetInStealthKill(inKill, killVictim, animIndex);
	}
}

void CPlayer::NetSerialize_CurrentItem( TSerialize ser, bool bReading )
{
	NET_PROFILE_SCOPE("Item", bReading);

	EntityId currentItemId = 0;
	bool itemIdIsScheduledSwitch = false;
		
	if(!bReading)
	{
		const SActorStats::SItemExchangeStats& stats = GetActorStats()->exchangeItemStats;
		if(stats.switchingToItemID)
		{
			currentItemId = stats.switchingToItemID;
			itemIdIsScheduledSwitch = true;
		}
		else
		{
			currentItemId = NetGetCurrentItem();
		}
	}
	
	bool isCurrentItemValid = currentItemId ? true : false;

	ser.Value("isCurrentItemValid", isCurrentItemValid, 'bool');
	//If itemIdIsScheduledSwitch is true then currentItemId is actually the item ID that is currently being switched to. This saves sending two entityIds over - knowing the
	//item we're scheduled to switch to is enough to decide which deselect anim to play (Normal or fast)
	ser.Value("itemIdIsScheduledSwitch", itemIdIsScheduledSwitch, 'bool');
	ser.Value("currentItemId", currentItemId, 'eid');

	m_lastNetItemId = currentItemId;

	bool isDeselecting = false;
	if(!bReading)
	{
		CItem *pItem = GetItem(currentItemId);
		if(pItem && pItem->IsDeselecting())
		{
			isDeselecting = true;
		}
	}

	ser.Value("isDeselecting", isDeselecting, 'bool');

	if(bReading)
	{
		// if we have a current item id or we have legitimately sent 0, then do stuff
		if(currentItemId || !isCurrentItemValid)
		{
			if(itemIdIsScheduledSwitch)
			{
				NetSetScheduledItem(currentItemId); //Schedule switch to start deselect animations
			}
			else if(currentItemId)
			{
				NetSetScheduledItem(0);
				NetSetCurrentItem(currentItemId, isDeselecting);
			}
		}
	}
}

void CPlayer::NetSerialize_Health( TSerialize ser, bool bReading )
{
	NET_PROFILE_SCOPE("Health", ser.IsReading());
	if(bReading)
	{
		float health = 0.0f;
		ser.Value("health", health, 'hlth');

		float newHealth = (health * 0.01f) * m_health.GetHealthMax();

		// Only set the health if the player is dead and the health is 0 or less
		// or if the player is alive and the health is greater than 0. The transition
		// from alive to dead and vice versa is handled by RMIs.
		if (m_health.IsDead() == (newHealth <= 0.0f))
		{
			//--- Ensure the health recharge time works correctly for remote players
			float healthRegeneration = 0.0f;
			float currentFrameTime = gEnv->pTimer->GetAsyncTime().GetSeconds();

			if(m_health.GetHealth() < m_health.GetHealthMax())
			{
				float timeSinceLastHealthSync = (currentFrameTime - m_timeOfLastHealthSync);
				healthRegeneration = GetRegenerationAmount(timeSinceLastHealthSync);
			}

			if (newHealth < m_health.GetHealth() - healthRegeneration)	//account for health regeneration
			{
				InformHealthHasBeenReduced();
			}
			SetHealth(newHealth);
			m_timeOfLastHealthSync = currentFrameTime;
		}
	}
	else
	{
		float health = m_health.GetHealth() * 100.f / m_health.GetHealthMax();
		ser.Value("health", health, 'hlth');
	}

	ser.Value("lastAttacker", m_stats.lastAttacker, 'eid'); //need to serialise this at the same time as InformHealthHasBeenReduced is called to ensure data isn't out of sync
}

void CPlayer::NetSerialize_Melee( TSerialize ser, bool bReading )
{
	uint8 previousHitCounter = m_meleeHitCounter;

	SMeleeHitParams		NetHitParams, *pHitParams;
	if(bReading || !m_pPlayerTypeComponent)
		pHitParams = &NetHitParams;
	else
		pHitParams = m_pPlayerTypeComponent->GetLastMeleeParams();

	ser.Value("meleeHitCounter", m_meleeHitCounter, 'ui2');
	ser.Value("boostedMelee", pHitParams->m_boostedMelee, 'bool');
	ser.Value("normal", pHitParams->m_hitNormal, 'dir1');
	ser.Value("offset", pHitParams->m_hitOffset, 'pHAn');
	CRY_ASSERT(pHitParams->m_surfaceIdx < 256);
	ser.Value("surfaceIdx", pHitParams->m_surfaceIdx, 'ui8');
	ser.Value("lastMeleedActorId", pHitParams->m_targetId, 'eid');

	if (bReading && (m_meleeHitCounter != previousHitCounter))
	{
		DoMeleeMaterialEffect(*pHitParams);
	}
}

void CPlayer::NetSerialize_Ladder( TSerialize ser, bool bReading )
{
	EntityId newLadderId = m_ladderId;
	float newHeightFrac = m_ladderHeightFrac;
	ser.Value("ladderId", newLadderId, 'eid');
	ser.Value("ladderHeightFrac", newHeightFrac, 'unit');

	if (bReading)
	{
		if (gEnv->bServer)
		{
			// The server does not accept ladderId changes, those should come from SvRequestUseLadder/SvRequestLeaveFromLadder
			if (newLadderId != m_ladderId) return;
			m_ladderHeightFrac = newHeightFrac;
		}
		else if (IsClient())
		{
			// If this information describes ourselves, ignore it entirely, since it must be out-of-date
			return;
		}
		else
		{
			// Never leave a ladder, that should come from ClLeaveFromLadder
			if (newLadderId == 0) return;
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(newLadderId);

			// Change in ladderId, means we are entering a ladder
			if (m_ladderId != newLadderId)
			{
				if (m_ladderId)
				{
					StateMachineHandleEventMovement(SStateEventLeaveLadder(eLLL_Drop));
				}
				if (pEntity)
				{
					m_ladderId = newLadderId;
					m_ladderHeightFrac = m_ladderHeightFracInterped = newHeightFrac;
					m_lastLadderLeaveLoc = eLLL_First;
				
					SStateEventLadder ladderEvent(pEntity);
					StateMachineHandleEventMovement(ladderEvent);

					SStateEventLadderPosition ladderPositionEvent(m_ladderHeightFrac);
					StateMachineHandleEventMovement(ladderPositionEvent);
				}
			}
			else
			{
				// Just update the height fraction, this will be interpolated automatically
				m_ladderHeightFrac = newHeightFrac;
			}
		}
	}
}

void CPlayer::FullSerialize( TSerialize ser )
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Player serialization");

	if(IsClient())
	{
		CGodMode::GetInstance().SetNewCheckpoint(GetEntity()->GetWorldTM());
	}
	
	const bool bIsReading = ser.IsReading();

	// Cut from CActor::FullSerialize(ser) to preserve the logic of reading the player state.
	// CActor cannot be instantiated anyway + noone else calls CActor::FullSerialize
	{
		assert(ser.GetSerializationTarget() != eST_Network);

		ser.BeginGroup("CActor");

		if( bIsReading )
		{
			float health    = 0.0f;
			float healthMax = 0.0f;
			ser.Value("health", health );		
			ser.Value("maxHealth", healthMax);
			m_health.SetHealthMax(healthMax);
			m_health.SetHealth(health);
			Revive(kRFR_FromInit);
		}
		else
		{
			float health    = m_health.GetHealth();
			float healthMax = m_health.GetHealthMax();
			ser.Value("health", health );		
			ser.Value("maxHealth", healthMax);
		}

		StateMachineSerializeMovement( SStateEventSerialize( ser ) );

		ser.EndGroup();

		m_linkStats.Serialize(ser);

		m_telemetry.Serialize(ser);
	}
	// end of cut from CActor::FullSerialize.

	m_stealthKill.Serialize(ser);

	if (m_pHitDeathReactions)
	{
		m_pHitDeathReactions->FullSerialize(ser);
	}

	m_pMovementController->Serialize(ser);

	if(ser.IsReading())
	{
		CCCPOINT(PlayerState_FullSerializeRead);
		ResetScreenFX();
	}
	else
	{
		CCCPOINT(PlayerState_FullSerializeWrite);
	}

	ser.BeginGroup( "BasicProperties" );
	// skip matrices... not supported
	// skip animation to play for now
	// skip currAnimW
	ser.Value( "eyeOffset", m_eyeOffset );

	ser.Value( "weaponOffset", m_weaponOffset ); 
	ser.EndGroup();

	// SerializeMovement
	m_pPlayerRotation->FullSerialize( ser );

	//serializing stats
	uint8 oldCinematicFlags = m_stats.cinematicFlags;

	m_actorPhysics.Serialize(ser, eEA_All);
	m_stats.Serialize(ser, eEA_All);
	
	UpdatePlayerCinematicStatus(oldCinematicFlags,m_stats.cinematicFlags);

	if(m_pLocalPlayerInteractionPlugin)
	{
		const SInteractionInfo& info = m_pLocalPlayerInteractionPlugin->GetCurrentInteractionInfo();
		EntityId id = info.lookAtInfo.lookAtTargetId;
		ser.Value("lookAtObjectId", id);

		if(ser.IsReading() && id != info.lookAtInfo.lookAtTargetId)
		{
			m_pLocalPlayerInteractionPlugin->SetLookAtTargetId(id);
		}
	}
	
	//input-actions
	IPlayerInput::EInputType inputType = m_pPlayerInput.get() ? m_pPlayerInput->GetType() : IPlayerInput::NULL_INPUT;

	ser.EnumValue("InputType", inputType, IPlayerInput::NULL_INPUT, IPlayerInput::LAST_INPUT_TYPE);

	if (inputType != IPlayerInput::NULL_INPUT)
	{
		if (ser.IsReading())
			CreateInputClass(inputType);

		if (m_pPlayerInput.get())
			m_pPlayerInput->SerializeSaveGame(ser);
	}

	ser.Value("mountedWeapon", m_stats.mountedWeaponID);

	if (m_stats.mountedWeaponID)
	{
		if (CItem* pItem = GetItem(m_stats.mountedWeaponID))
			pItem->ApplyViewLimit(GetEntityId(), true);
	}

	if (IsClient())
	{
		// Staging params
		if (ser.IsWriting())
		{
			m_stagingParams.Serialize(ser);

			//Ledge grab fix after serialization
			bool ledgeGrabbing = m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Ledge );
			ser.Value("clientLedgeGrabbing", ledgeGrabbing);
		}
		else
		{
			SStagingParams stagingParams;
			stagingParams.Serialize(ser);
			if (stagingParams.bActive || stagingParams.bActive != m_stagingParams.bActive)
			{
				StagePlayer(stagingParams.bActive, &stagingParams);
			}

			//Stamp/Ledge grab fix after serialization
			bool stamping = false;
			ser.Value("clientStamping", stamping);
			bool ledgeGrabbing = false;
			ser.Value("clientLedgeGrabbing", ledgeGrabbing);
		}
	}
	
	// Make sure we try registration after we serialized Health etc.
	//
	// This doesn't fix all problems caused by the fact that PostReloadExtension
	// calls RegisterXXX functions before FullSerialize is called
	//
	if (bIsReading)
	{
		RegisterInAutoAimManager();
		
		// We SHOULD call the following too but it would only solve a visual
		// glitch, not a game mechanic, so we don't do this for DLC1
		// RegisterDBAGroups();	 
	}	
}

void CPlayer::OnLocalSpectatorStateSerialize(CActor::EActorSpectatorState newState, CActor::EActorSpectatorState curState)
{
	if (newState != curState)
	{
		if (newState == CActor::eASS_ForcedEquipmentChange)
		{
			{
				g_pGame->GetGameRules()->FreezeInput(false);
				CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();
				CRY_ASSERT(pEquipmentLoadout);
				if (pEquipmentLoadout)
				{
					// force all clients to resend loadouts even if they're the same now that the server thinks you've not sent any
					pEquipmentLoadout->InvalidateLastSentLoadout();
					pEquipmentLoadout->SetHasPreGameLoadoutSent(false);
				}

// 				pFlashMenu->ScheduleInitialize(CFlashFrontEnd::eFlM_IngameMenu, eFFES_equipment_select);
// 				pFlashMenu->SetBlockClose(true);
			}
		}
	}
}

void CPlayer::PostSerialize()
{
	CActor::PostSerialize();

	if (IScriptTable* pScriptTable = GetEntity()->GetScriptTable())
	{
		if (pScriptTable->HaveValue("OnPlayerPostSerialize"))
		{
			Script::CallMethod(pScriptTable, "OnPlayerPostSerialize");
		}
	}

	StateMachineHandleEventMovement( SStateEvent( STATE_EVENT_POST_SERIALIZE ) );

	if( m_desiredStance == STANCE_CROUCH )
	{
		IPlayerInput* piInput = GetPlayerInput();
		if( piInput && piInput->GetType() == IPlayerInput::PLAYER_INPUT )
		{
			static_cast<CPlayerInput*> (piInput)->AddCrouchAction();
		}
	}

	UpdateThirdPersonState();

	if (m_pHitDeathReactions)
	{
		m_pHitDeathReactions->PostSerialize();
	}

	if (IsClient())
	{
		ResetFPView();

		if(m_pVehicleClient)
			m_pVehicleClient->Reset();
		SupressViewBlending();

		CALL_PLAYER_EVENT_LISTENERS(OnHealthChanged(this, m_health.GetHealth()));

		//Make sure AI update mode is correct
		IAIObject* pAIObject = GetEntity()->GetAI();
		if (pAIObject)
		{
			IAIActorProxy* pAIActorProxy = pAIObject->GetProxy();
			if (pAIActorProxy)
			{
				pAIActorProxy->UpdateMeAlways(true);
			}
		}

		m_lastTimeDamaged.SetValue(0);
	}
}

void SPlayerStats::Serialize(TSerialize ser, EEntityAspects aspects)
{
	assert( ser.GetSerializationTarget() != eST_Network );
	ser.BeginGroup("PlayerStats");

	if (ser.GetSerializationTarget() != eST_Network)
	{
		//when reading, reset the structure first.
		if (ser.IsReading())
			*this = SPlayerStats();

		ser.Value("inMovement", inMovement);
		ser.Value("fallSpeed", fallSpeed);
		ser.Value("isRagDoll", isRagDoll);
		ser.Value("stuckTimeout", stuckTimeout);

		ser.Value("isThirdPerson", isThirdPerson);

		followCharacterHead.Serialize(ser, "followCharacterHead");
		isHidden.Serialize(ser, "isHidden");
		inAir.Serialize(ser, "inAir");
		onGround.Serialize(ser, "onGround");
		speedFlat.Serialize(ser, "flatSpeed");

		//FIXME:serialize cheats or not?
		//int godMode(g_pGameCVars->g_godMode);
		//ser.Value("godMode", godMode);
		//g_pGameCVars->g_godMode = godMode;
		//ser.Value("flyMode", flyMode);
		//

		ser.Value("cinematicFlags", cinematicFlags);
	}

	ser.EndGroup();
}

bool CPlayer::CreateCodeEvent(SmartScriptTable &rTable)
{	
	return CActor::CreateCodeEvent(rTable);
}

void CPlayer::PlayAction(const char *action,const char *extension, bool looping) 
{
	CryFatalError("CPlayer::PlayAction: This should not be called anymore");
}

void CPlayer::AnimationControlled(bool activate, bool bUpdateVisibility/*=true*/)
{
		m_stats.followCharacterHead = activate?1:0;

		if (!activate || bUpdateVisibility)
		{
			RefreshVisibilityState(); 
		}

		// Disable player movement during animation controlled events.
		// DT: 16735: X360 - SP : Mission 01 - Battery Park : HUD - The crouch / stand icon appears when pressing B (crouch) during a cinematic.
		if (IsClient())
		{
			g_pGameActions->FilterNoMove()->Enable( activate );
		}
}

void CPlayer::PartialAnimationControlled(  bool activate, const PlayerCameraAnimationSettings& cameraAnimationSettings )
{
	// This stat is still needed by STAP, we should sort this out better
	m_stats.partialCameraAnimFactor = activate ? max(cameraAnimationSettings.positionFactor, cameraAnimationSettings.rotationFactor) : 0.0f;

	if (m_playerCamera)
	{
		if (activate)
		{
			m_playerCamera->SetCameraModeWithAnimationBlendFactors( eCameraMode_PartialAnimationControlled, cameraAnimationSettings, "Entering partial animation controlled" );
		}
		else
		{
			m_playerCamera->SetCameraMode( eCameraMode_Default, "Leaving partial animation controlled" );
		}
	}
}

void CPlayer::OnCollision(EventPhysCollision *physCollision)
{
	if(gEnv->bMultiplayer && m_pHitDeathReactions && m_pHitDeathReactions->IsInDeathReaction() && (physCollision->pEntity[1] != physCollision->pEntity[0]))
	{
		//If another player has collided with us we shall ragdoll (allowing the player to push us out the way)
		IPhysicalEntity* pHitByPhysicalEntity = physCollision->pEntity[1];
		if (pHitByPhysicalEntity == GetEntity()->GetPhysics())
		{
			pHitByPhysicalEntity = physCollision->pEntity[0];
		}

		if(pHitByPhysicalEntity)
		{
			if( IEntity *pHitByEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pHitByPhysicalEntity) )
			{
				if( pHitByEntity != GetEntity() )
				{
					if( IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pHitByEntity->GetId()) )
					{
						if(pActor->IsPlayer())
						{
							CPlayer *pHitByPlayer = static_cast<CPlayer *>(pActor);

							if (!pHitByPlayer->GetStealthKill().IsBusy()) // no ragdolling immediately when we're being stealth killed!
							{
#if !defined(_RELEASE)
								CryLog("CPlayer::OnCollision() has detected closeup death, turning on ragdoll immediately for players attacker=%s; (local=%d) victim=%s (local=%d) is already ragdoll='%s'", pActor->GetEntity()->GetName(), pActor->IsClient(), GetEntity()->GetName(), IsClient(), static_cast<CActor*>(pActor)->GetActorStats()->isRagDoll ? "true" : "false");
#endif
								RagDollize(false);
							}
						}
					}
				}
			}
		}

	}
}

void CPlayer::UpdateVisibility()
{
	bool forceDontRenderNearest = (IsOnLadder() && !g_pGameCVars->pl_ladderControl.ladder_renderPlayerLast);
#if ENABLE_MINDCONTROL
	forceDontRenderNearest = forceDontRenderNearest || IsMindControlling();
#endif
	const bool isThirdPerson = IsThirdPerson();
	SetupPlayerCharacterVisibility(GetEntity(), isThirdPerson, GetShadowCharacterSlot(), forceDontRenderNearest);

	m_fpCompleteBodyVisible = isThirdPerson; //Will force a refresh if needed

	IAnimatedCharacter *pAnimatedCharacter = GetAnimatedCharacter();
	if (pAnimatedCharacter)
	{
		pAnimatedCharacter->UpdateCharacterPtrs();
	}

	//Update currently selected item's view
	IInventory* pInventory = GetInventory();
	if (pInventory)
	{
		CItem *pCurrentItem = static_cast<CItem*>(GetItem(pInventory->GetCurrentItem()));      
		if(pCurrentItem && pCurrentItem->IsSelected())
		{
			//pCurrentItem->SetViewChange(!isThirdPerson);
		}
	}
}

void CPlayer::HandleEvent( const SGameObjectEvent& event )
{
	bool bHandled = false;

	switch(event.event)
	{
	case eGFE_StoodOnChange:
		StateMachineHandleEventMovement( SStateEventGroundColliderChanged( event.paramAsBool ) );
		break;
	case eGFE_RagdollPhysicalized:
		PostRagdollPhysicalized( static_cast<SRagdollizeParams*> (event.ptr)->sleep );
		StateMachineHandleEventMovement(PLAYER_EVENT_RAGDOLL_PHYSICALIZED);
		// Let actor handle it too
		CActor::HandleEvent(event); 
		break;
	case eGFE_OnCollision:
		{
			EventPhysCollision *physCollision = reinterpret_cast<EventPhysCollision *>(event.ptr);
			OnCollision(physCollision);
		}
		break;
	case eCGE_AnimateHands:
		CreateScriptEvent("AnimateHands",0,(const char *)event.param);
		break;
	case eCGE_SetTeam:
		{
			const STeamChangeInfo * teamChangeInfo = (const STeamChangeInfo *)event.param;
			//CryLog("CPlayer::HandleEvent being told SetTeam(%d=>%d) for '%s'", teamChangeInfo->oldTeamNum, teamChangeInfo->newTeamNum, GetEntity()->GetName());

			m_teamId = teamChangeInfo->newTeamNum;

			RegisterOnHUD();

			SHUDEvent hudevent(eHUDEvent_PlayerSwitchTeams);
			hudevent.AddData(SHUDEventData((int)GetEntityId()));
			hudevent.AddData(teamChangeInfo->oldTeamNum);
			CHUDEventDispatcher::CallEvent(hudevent);

			OnChangeTeam();

			// Changing a player's team will change various properties in lua (i.e. filemodel) so we need to re-cache the data
			CGameCache &gameCache = g_pGame->GetGameCache();
			gameCache.RefreshActorInstance(GetEntityId(), GetEntity()->GetScriptTable());
			PrepareLuaCache();

			CryFixedStringT<64> signalName;
			signalName.Format("Player_Footstep_Gear_MP_Team%d%s", (teamChangeInfo->newTeamNum == 2) ? 2 : 1, IsClient() ? "_FP" : "");
			m_sounds[ESound_Gear_Run].audioSignalPlayer.SetSignal(signalName.c_str());

			if (teamChangeInfo->oldTeamNum == 0)
			{
				FullyUpdateActorModel();

				// Changing the actor model will re-physicalize the player, if they're supposed to be dead we need to ragdoll them again
				if (IsDead() == true)
				{
					m_stats.isRagDoll = false;
					RagDollize(false);
				}
			}
			CTeamVisualizationManager* pTeamVisManager = g_pGame->GetGameRules()->GetTeamVisualizationManager();
			if(pTeamVisManager)
			{
				pTeamVisManager->OnPlayerTeamChange(GetEntityId()); 
			}

			if(g_pGame->GetGameLobby())
			{		
				g_pGame->GetGameLobby()->OnPlayerSwitchedTeam(GetChannelId(), teamChangeInfo->newTeamNum);
			}
		}
		break;
	case eCGE_Launch:
		{
			//--- Initiate rag-doll between now & the end of the animation
			CAnimationPlayerProxy *animPlayerProxy = GetAnimatedCharacter()->GetAnimationPlayerProxy(0);
			const float expectedDurationSeconds = animPlayerProxy ? animPlayerProxy->GetTopAnimExpectedSecondsLeft(GetEntity(), 0) : -1;
			if (0 <= expectedDurationSeconds)
			{
				m_ragdollTime = expectedDurationSeconds;
			}
		}
		break;
	case eCGE_CoverTransitionEnter:
		{	
			const float FAST_AIM_IK_FADE_OUT = 0.2f;
			ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
			if (pCharacter)
			{
				IAnimationPoseBlenderDir* pIPoseBlenderAim = pCharacter->GetISkeletonPose()->GetIPoseBlenderAim();
				if (pIPoseBlenderAim)
					pIPoseBlenderAim->SetFadeOutSpeed(min(FAST_AIM_IK_FADE_OUT, m_params.aimIKFadeDuration));
			}
		}
		break;
	case eCGE_CoverTransitionExit:
		{
			ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
			if (pCharacter)
			{
				IAnimationPoseBlenderDir* pIPoseBlenderAim = pCharacter->GetISkeletonPose()->GetIPoseBlenderAim();
				if (pIPoseBlenderAim)
					pIPoseBlenderAim->SetFadeOutSpeed(m_params.aimIKFadeDuration);
		}
		}
		break;
	default:
		{
			// HitDeathReactions event handling
			bHandled = m_pHitDeathReactions && m_pHitDeathReactions->HandleEvent(event);

			// Movement controller event handling
			CPlayerMovementController *pPMC = static_cast<CPlayerMovementController *>(GetMovementController());
			bHandled = bHandled || (pPMC && pPMC->HandleEvent(event));

			if (!bHandled)
			{
				CActor::HandleEvent(event);
			}
			break;
		}
	}
}

//------------------------------------------------------------------------
// animation-based footsteps sound playback
void CPlayer::ExecuteFootStep(ICharacterInstance* pCharacter, const float frameTime, const int32 nFootJointID)
{
	CRY_ASSERT(nFootJointID >= 0);

	const EBonesID boneId = (GetBoneID(BONE_FOOT_L)==nFootJointID) ? BONE_FOOT_L : BONE_FOOT_R;
	if (GetBoneID(boneId) >= 0)
	{
		m_footstepCounter++;

		ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
		CRY_ASSERT(pSkeletonAnim);

		const float relativeSpeed = pSkeletonAnim->GetCurrentVelocity().GetLength() * 0.142f;

		m_fLastEffectFootStepTime = gEnv->pTimer->GetAsyncCurTime();

		f32 fZRotation = 0.0f;
		Vec3 vDeltaMovment(0,0,0);
		if (frameTime > 0.0f)
		{
			fZRotation = abs( RAD2DEG( pSkeletonAnim->GetRelMovement().q.GetRotZ() ) ) / frameTime;
			Vec3 vRelTrans = pSkeletonAnim->GetRelMovement().t;
			Vec3 vCurrentVel = pSkeletonAnim->GetCurrentVelocity() * frameTime;
			vDeltaMovment = ( vRelTrans - vCurrentVel) / frameTime;
		}

		ISkeletonPose *pSkeletonPose = pCharacter->GetISkeletonPose();
		CRY_ASSERT(pSkeletonPose);

		// Setup FX params
		SMFXRunTimeEffectParams params;
		
		params.audioProxyEntityId = GetEntityId();
		//params.audioProxyEntityId = g_pGameCVars->g_FootstepSoundsFollowEntity ? GetEntityId() : 0;

		params.angle = GetEntity()->GetWorldAngles().z + (gf_PI * 0.5f);
		params.pos = params.decalPos = GetEntity()->GetSlotWorldTM(0) * GetBoneTransform(boneId).t;
		params.decalPos = params.pos;

		params.AddAudioRtpc("character_speed", relativeSpeed);
		params.AddAudioRtpc("turn", fZRotation);
		params.AddAudioRtpc("acceleration", vDeltaMovment.y); 
		//params.soundSemantic = eSoundSemantic_Physics_Footstep;
		params.audioProxyOffset = GetEntity()->GetWorldTM().GetInverted().TransformVector(params.pos - GetEntity()->GetWorldPos());


		params.playSoundFP = !IsThirdPerson();

		//create effects
		IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects;

		TMFXEffectId effectId = InvalidEffectId;

		CryFixedStringT<16> sEffectWater = "water_shallow";

		bool usingWaterEffectId = false;
		const float feetWaterLevel = gEnv->p3DEngine->GetWaterLevel(&params.pos);

		if (feetWaterLevel != WATER_LEVEL_UNKNOWN)
		{
			const float depth = feetWaterLevel - params.pos.z;
			if (depth >= 0.0f)
			{
				usingWaterEffectId = true;
				
				// Plug water hits directly into water sim
				// todo: move out of foot-step for more continuous ripple gen (currently skips during sprint, looks weird).
				if (gEnv->p3DEngine)
				{
					gEnv->p3DEngine->AddWaterRipple(GetEntity()->GetWorldPos(), 1.0f, relativeSpeed * 2);
				}


				if(!gEnv->bMultiplayer && IsClient() && IsSprinting())
				{
 					float radius = 15.0f + relativeSpeed;
					TargetTrackHelpers::SStimulusEvent stimulusEventInfo;
					stimulusEventInfo.vPos = GetEntity()->GetWorldPos();
					stimulusEventInfo.eStimulusType = TargetTrackHelpers::eEST_Sound;
					stimulusEventInfo.eTargetThreat = AITHREAT_INTERESTING;
					tAIObjectID targetAIObjectId = GetEntity()->GetAI() ? GetEntity()->GetAI()->GetAIObjectID() : 0;
					gEnv->pAISystem->GetTargetTrackManager()->HandleStimulusEventInRange( targetAIObjectId, "SoundWaterRipple", stimulusEventInfo, radius );

					g_pGame->GetGameAISystem()->GetEnvironmentDisturbanceManager().AddObservableEvent( GetEntity()->GetWorldPos(), 2.0f, "OnWaterRippleSeen", GetEntityId() );
				}

				// trigger water splashes from here
				if (depth > FOOTSTEPS_DEEPWATER_DEPTH)
				{
					sEffectWater = "water_deep";
				}
			}
		}

		// Effect is called by footstepEffectName which is defined in BasicActor.lua as "footstep"
		if(!usingWaterEffectId)
		{
			effectId = pMaterialEffects->GetEffectId(GetFootstepEffectName(), GetActorPhysics().groundMaterialIdx);
		}
		else
		{
			effectId = pMaterialEffects->GetEffectIdByName(GetFootstepEffectName(), sEffectWater);
		}

		// Gear Effect

		// Gear and Foley sounds will be improved soon in a way that like footsteps, foley signals can be added to the animation
		// which can be processes here as well. For now we play gear for humans

		TMFXEffectId gearEffectId = InvalidEffectId;
		TMFXEffectId gearSearchEffectId = InvalidEffectId;

		if (!gEnv->bMultiplayer && m_params.footstepGearEffect)
		{
			EStance stance = GetStance();
			float gearEffectPossibility = relativeSpeed*1.3f;


			if(stance == STANCE_CROUCH)
			{
				gearEffectPossibility *= 10.0f;
			}
			else if ((stance == STANCE_STEALTH) && (cry_random(0.0f, 1.0f) < 0.3f))
			{
				if (IAIObject* pAI = GetEntity()->GetAI())
				{
					if (pAI->GetProxy() && pAI->GetProxy()->GetAlertnessState() > 0)
					{
						gearSearchEffectId = pMaterialEffects->GetEffectIdByName(m_params.footstepEffectName.c_str(), "gear_search");
					}
				}
			}

			if(cry_random(0.0f, 1.0f) < gearEffectPossibility)
			{
				gearEffectId = pMaterialEffects->GetEffectIdByName(m_params.footstepEffectName.c_str(), "gear");
			}
		}

		//Play effects
		EPlayerSounds playerSound = relativeSpeed>6 ? ESound_Gear_Run : ESound_Gear_Walk;
		if (m_sounds[playerSound].audioSignalPlayer.HasValidSignal())
			PlaySound(playerSound, true, "speed", relativeSpeed, NULL, 0, 1.0f, true);

		if(effectId != InvalidEffectId)
		{
			pMaterialEffects->ExecuteEffect(effectId, params);
		}
		if(gearEffectId != InvalidEffectId)
		{
			pMaterialEffects->ExecuteEffect(gearEffectId, params);
		}
		if(gearSearchEffectId != InvalidEffectId)
		{
			pMaterialEffects->ExecuteEffect(gearSearchEffectId, params);
		}

#if !defined(_RELEASE)
		//////////////////////////////////////////////////////////////////////////
		// DEBUG INFO
		if (g_pGameCVars->g_FootstepSoundsDebug != 0)
		{
			const char* color = "";
			if (vDeltaMovment.y < 0.33f)
				color = "$6"; // Yellow
			else if (vDeltaMovment.y < 0.66f)
				color = "$8"; // Orange
			else
				color = "$4"; // Red

			if (gEnv->IsEditor())
				CryLog("%s[%s] speed=%3.2f, turn=%3.2f, acceleration=%3.1f", color, GetEntity()->GetName(), relativeSpeed, fZRotation, vDeltaMovment.y);
			else
				CryWatch("%s[%s] speed=%3.2f, turn=%3.2f, acceleration=%3.1f", color, GetEntity()->GetName(), relativeSpeed, fZRotation, vDeltaMovment.y);
		}
		//////////////////////////////////////////////////////////////////////////
#endif // _RELEASE

		const bool skip = (IsCloaked() && !IsSprinting());
		if (!skip)
		{
			ExecuteFootStepsAIStimulus(relativeSpeed, 0.0f);
		}
	}
}

//------------------------------------------------------------------------
void CPlayer::UpdateSilentFeetSoundAdjustment()
{
}

//------------------------------------------------------------------------
const char* CPlayer::GetFootstepEffectName() const
{
	if(IsRemote())
	{
		return m_params.remoteFootstepEffectName.c_str();
	}
	else
	{
		return m_params.footstepEffectName.c_str();
	}
}

//------------------------------------------------------------------------
// animation-based foley sound playback
void CPlayer::ExecuteFoleySignal(ICharacterInstance* pCharacter, const float frameTime, const char* sFoleyActionName, const int32 nBoneJointID)
{
	CRY_ASSERT(nBoneJointID >= 0);

	ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
	CRY_ASSERT(pSkeletonAnim);

	const float relativeSpeed = pSkeletonAnim->GetCurrentVelocity().GetLength() * 0.142f;

	f32 fZRotation = 0.0f;
	Vec3 vDeltaMovment(0,0,0);
	if (frameTime > 0.0f)
	{
		float const fInvFrameTime = __fres(frameTime);
		fZRotation = abs( RAD2DEG( pSkeletonAnim->GetRelMovement().q.GetRotZ() ) ) * fInvFrameTime;
		Vec3 vRelTrans = pSkeletonAnim->GetRelMovement().t;
		Vec3 vCurrentVel = pSkeletonAnim->GetCurrentVelocity() * frameTime;
		vDeltaMovment = ( vRelTrans - vCurrentVel) * fInvFrameTime;
	}

	ISkeletonPose *pSkeletonPose = pCharacter->GetISkeletonPose();
	CRY_ASSERT(pSkeletonPose);

	// Setup FX params
	SMFXRunTimeEffectParams params;
	params.audioProxyEntityId = g_pGameCVars->g_FootstepSoundsFollowEntity ? GetEntityId() : 0;
	params.angle = GetEntity()->GetWorldAngles().z + (gf_PI * 0.5f);
	params.pos = params.decalPos = GetEntity()->GetSlotWorldTM(0) * pSkeletonPose->GetAbsJointByID(nBoneJointID).t;
	params.decalPos = params.pos;

	params.AddAudioRtpc("speed", relativeSpeed);
	params.AddAudioRtpc("turn", fZRotation);
	params.AddAudioRtpc("acceleration", vDeltaMovment.y); 
	//params.soundSemantic = eSoundSemantic_Animation;
	params.audioProxyOffset = GetEntity()->GetWorldTM().GetInverted().TransformVector(params.pos - GetEntity()->GetWorldPos());

	params.playSoundFP = !IsThirdPerson();

	//create effects
	IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects;
	CryFixedStringT<32> sEffectName = "default";

	if (sFoleyActionName[0])
		sEffectName = sFoleyActionName;

	// Effect is called by foleyEffectName which is defined in BasicActor.lua as "foley"
	TMFXEffectId effectId = pMaterialEffects->GetEffectIdByName(m_params.foleyEffectName.c_str(), sEffectName);

	if(effectId != InvalidEffectId)
	{
		pMaterialEffects->ExecuteEffect(effectId, params);
	}

#if !defined(_RELEASE)
	//////////////////////////////////////////////////////////////////////////
	// DEBUG INFO
	if (g_pGameCVars->g_FootstepSoundsDebug != 0) //TODO maybe create specific Foley Debug CVar
	{
		const char* color = "";
		if (vDeltaMovment.y < 0.33f)
			color = "$6"; // Yellow
		else if (vDeltaMovment.y < 0.66f)
			color = "$8"; // Orange
		else
			color = "$4"; // Red

		if (gEnv->IsEditor())
			CryLog("Foley %s[%s] Effect: %s speed=%3.2f, turn=%3.2f, acceleration=%3.1f", color, GetEntity()->GetName(), sEffectName.c_str(), relativeSpeed, fZRotation, vDeltaMovment.y);
		else
			CryWatch("Foley %s[%s] Effect: %s speed=%3.2f, turn=%3.2f, acceleration=%3.1f", color, GetEntity()->GetName(), sEffectName.c_str(), relativeSpeed, fZRotation, vDeltaMovment.y);
	}
	//////////////////////////////////////////////////////////////////////////
#endif // _RELEASE
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::ExecuteGroundEffectAnimEvent(ICharacterInstance* pCharacter, const float frameTime, const char* szEffectName, const int32 nJointID)
{
	ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
	CRY_ASSERT(pSkeletonAnim);

	const float relativeSpeed = pSkeletonAnim->GetCurrentVelocity().GetLength() * 0.142f;

	f32 fZRotation = 0.0f;
	Vec3 vDeltaMovment(0,0,0);
	if (frameTime > 0.0f)
	{
		fZRotation = abs( RAD2DEG( pSkeletonAnim->GetRelMovement().q.GetRotZ() ) ) / frameTime;
		Vec3 vRelTrans = pSkeletonAnim->GetRelMovement().t;
		Vec3 vCurrentVel = pSkeletonAnim->GetCurrentVelocity() * frameTime;
		vDeltaMovment = ( vRelTrans - vCurrentVel) / frameTime;
	}

	ISkeletonPose *pSkeletonPose = pCharacter->GetISkeletonPose();
	CRY_ASSERT(pSkeletonPose);

	// Setup FX params
	SMFXRunTimeEffectParams params;
	params.audioProxyEntityId = g_pGameCVars->g_FootstepSoundsFollowEntity ? GetEntityId() : 0;
	params.angle = GetEntity()->GetWorldAngles().z + (gf_PI * 0.5f);
	params.pos = params.decalPos = GetEntity()->GetSlotWorldTM(0) * ((nJointID > -1) ? pSkeletonPose->GetAbsJointByID(nJointID).t : Vec3Constants<float>::fVec3_Zero);
	params.decalPos = params.pos;

	params.AddAudioRtpc("speed", relativeSpeed);
	params.AddAudioRtpc("turn", fZRotation);
	params.AddAudioRtpc("acceleration", vDeltaMovment.y); 
	//params.soundSemantic = eSoundSemantic_Animation;
	params.audioProxyOffset = GetEntity()->GetWorldTM().GetInverted().TransformVector(params.pos - GetEntity()->GetWorldPos());

	params.playSoundFP = !IsThirdPerson();

	//create effects
	IMaterialEffects* pMaterialEffects = gEnv->pMaterialEffects;

	TMFXEffectId effectId = InvalidEffectId;

	CryFixedStringT<16> sEffectWater = "water_shallow";

	bool usingWaterEffectId = false;
	const float feetWaterLevel = gEnv->p3DEngine->GetWaterLevel(&params.pos);

	if (feetWaterLevel != WATER_LEVEL_UNKNOWN)
	{
		const float depth = feetWaterLevel - params.pos.z;
		if (depth >= 0.0f)
		{
			usingWaterEffectId = true;

			if (depth > FOOTSTEPS_DEEPWATER_DEPTH)
			{
				sEffectWater = "water_deep";
			}
		}
	}

	if(!usingWaterEffectId)
	{
		effectId = pMaterialEffects->GetEffectId(szEffectName, GetActorPhysics().groundMaterialIdx);
	}
	else
	{
		effectId = pMaterialEffects->GetEffectIdByName(szEffectName, sEffectWater);
	}

	if(effectId != InvalidEffectId)
	{
		pMaterialEffects->ExecuteEffect(effectId, params);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::OnKillAnimEvent(const AnimEventInstance &event)
{
	// [*DavidR | 7/Sep/2010] ToDo: Find a better, network-compatible way to kill players and centralize all the code 
	// similar to this (present in SpectacularKill, StealthKill and PickAndThrowWeapon) in that place
	HitInfo hitInfo;
	hitInfo.shooterId = GetEntityId();
	hitInfo.targetId = GetEntityId();
	hitInfo.damage = 99999.0f; // CERTAIN_DEATH
	hitInfo.dir = GetEntity()->GetForwardDir();
	hitInfo.normal = -hitInfo.dir; // this has to be in an opposite direction from the hitInfo.dir or the hit is ignored as a 'backface' hit
	if (event.m_CustomParameter && *event.m_CustomParameter)
	{
		// Specifying a hit type will	rely on the normal hit/death pipeline
		hitInfo.type = g_pGame->GetGameRules()->GetHitTypeId(event.m_CustomParameter);
	}
	else 
	{
		// If no hit type is specified we assume the intention was just to trigger the 
		// ragdoll right away (overriding the normal hit/death pipeline)
		GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Ragdoll);

		hitInfo.type = g_pGame->GetGameRules()->GetHitTypeId("event");
	}

	g_pGame->GetGameRules()->ClientHit(hitInfo);
}

//////////////////////////////////////////////////////////////////////////
bool CPlayer::CanBreakGlass() const
{
	const bool bCanBreakGlassLedge = (m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Ledge) ) ? false : ((gEnv->pTimer->GetAsyncCurTime() - m_lastLedgeTime) < 1.0f);

	// allow clients and servers to break the glass, as only breaks on the server are sent out and all others are calculated locally
	// breaks that the client calculates locally and then also receives from the server are harmless
	// this will not be perfect, as IsSprinting(), etc could return different values on different machines depending
	// on the state of netserialization, but this is better than it having no syncing at all
	return gEnv->bMultiplayer && (IsSprinting() || bCanBreakGlassLedge);
}

//////////////////////////////////////////////////////////////////////////
bool CPlayer::MustBreakGlass() const
{
	// In some cases we want to force the player to break through glass
	// even if they don't actually exert enough force to do so
	return gEnv->bMultiplayer && (CanBreakGlass() || IsSliding());
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::ExecuteFootStepsAIStimulus(const float relativeSpeed, const float noiseSupression)
{
	if (IPerceptionManager::GetInstance())
	{
		//handle AI sound recognition *************************************************
		float fStandingRadius = g_pGameCVars->ai_perception.movement_standingRadiusDefault;
		float fCrouchRadius = g_pGameCVars->ai_perception.movement_crouchRadiusDefault;
		float fMovingBaseMult = g_pGameCVars->ai_perception.movement_movingSurfaceDefault;

		if (g_pGameCVars->ai_perception.movement_useSurfaceType > 0)
		{
			IMaterialManager *pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
			CRY_ASSERT(pMaterialManager);
			ISurfaceType *pSurface = pMaterialManager->GetSurfaceTypeManager()->GetSurfaceType(GetActorPhysics().groundMaterialIdx);
			if (pSurface) 
			{
				const ISurfaceType::SSurfaceTypeAIParams *pAiParams = pSurface->GetAIParams();
				if (pAiParams)
				{
					fStandingRadius = pAiParams->fFootStepRadius;
					fCrouchRadius = pAiParams->crouchMult;
					fMovingBaseMult = pAiParams->movingMult;
				}
			}
		}

		// Use correct base radius
		float fBaseRadius = fStandingRadius;
		float fMovementRadius = g_pGameCVars->ai_perception.movement_standingMovingMultiplier;
		const EStance stance = GetStance();
		if (!IsSprinting() && stance == STANCE_CROUCH)
		{
			fBaseRadius = fCrouchRadius;
			fMovementRadius = g_pGameCVars->ai_perception.movement_crouchMovingMultiplier;
		}

		const float fFootstepSpeed = m_lastRequestedVelocity.GetLength();
		if (fFootstepSpeed > FLT_EPSILON)
		{
			float fFootstepRadius = fBaseRadius + (fFootstepSpeed * fMovingBaseMult * fMovementRadius);
			fFootstepRadius *= (1.0f - noiseSupression);

			if(fFootstepRadius > 0.0f)
			{
				SAIStimulus stim(AISTIM_SOUND, AISOUND_MOVEMENT, GetEntityId(), 0,
					GetEntity()->GetWorldPos() + GetEyeOffset(), ZERO, fFootstepRadius);
				IPerceptionManager::GetInstance()->RegisterStimulus(stim);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CPlayer::ShouldUpdateNextFootStep() const
{
	//Cull distant dudes
	//if (!IsClient())
	//{
	//	const Vec3& actorPos = GetEntity()->GetWorldPos();

	//	IAudioListener const* const pAudioListener = gEnv->pSoundSystem->GetListener(gEnv->pSoundSystem->GetClosestActiveListener(actorPos));

	//	if (pAudioListener != NULL)
	//	{
	//		f32 const fLengthSq = (pAudioListener->GetPosition() - actorPos).GetLengthSquared();
	//		//distance check - running footsteps fx on all AIs is expensive
	//		if (fLengthSq > g_pGameCVars->g_footstepSoundMaxDistanceSq)
	//		{
	//			return false;
	//		}
	//	}
	//	else
	//	{
	//		return false;
	//	}
	//}

	return ((gEnv->pTimer->GetAsyncCurTime() - m_fLastEffectFootStepTime) > 0.2f);
	//return false;
}

//////////////////////////////////////////////////////////////////////////

void CPlayer::SwitchDemoModeSpectator(bool activate)
{
	if(!IsDemoPlayback())
		return;

	m_stats.isThirdPerson = !activate;

	CItem *pItem = GetItem(GetInventory()->GetCurrentItem());
	if(pItem)
		pItem->UpdateFPView(0);

	IVehicle* pVehicle = GetLinkedVehicle();
	if (pVehicle)
	{
		IVehicleSeat* pVehicleSeat = pVehicle->GetSeatForPassenger( GetEntityId() );
		if (pVehicleSeat)
			pVehicleSeat->SetView( activate ? FirstVehicleViewId : InvalidVehicleViewId );
	}

	if (activate)
	{
		IScriptSystem * pSS = gEnv->pScriptSystem;
		pSS->SetGlobalValue( "g_localActor", GetGameObject()->GetEntity()->GetScriptTable() );
		pSS->SetGlobalValue( "g_localActorId", ScriptHandle( GetGameObject()->GetEntityId() ) );
	}

	GetGameObject()->SetAutoDisablePhysicsMode(eADPM_Never);
}

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
//------------------------------------------------------
void CPlayer::SetFlyMode(uint8 flyMode)
{
	if (GetSpectatorMode())
		return;

	m_stats.flyMode = flyMode;

	if (m_stats.flyMode>2)
		m_stats.flyMode = 0;

	if(m_pAnimatedCharacter)
		m_pAnimatedCharacter->RequestPhysicalColliderMode((m_stats.flyMode==2)?eColliderMode_Disabled:eColliderMode_Undefined, eColliderModeLayer_Game, "Player::SetFlyMode");

	CCCPOINT_IF(flyMode == 0, PlayerState_FlyMode_Off);
	CCCPOINT_IF(flyMode == 1, PlayerState_FlyMode_FlyCollisionsOn);
	CCCPOINT_IF(flyMode == 2, PlayerState_FlyMode_FlyCollisionsOff);
}
#endif

void CPlayer::SetSpectatorModeAndOtherEntId(const uint8 _mode, const EntityId _othEntId, bool isSpawning)
{
	//CryLog("%s setting spectator mode %d", GetEntity()->GetName(), mode);

	uint8  mode = _mode;
	EntityId  othEntId = _othEntId;

	SSpectatorInfo*  spinf = &m_stats.spectatorInfo;

	bool server=gEnv->bServer;

	CGameRules*  pGameRules = g_pGame->GetGameRules();
	IGameRulesSpectatorModule*  specmod = pGameRules->GetSpectatorModule();

	if(gEnv->IsClient() && (mode || spinf->mode) && m_pPlayerInput.get())
		m_pPlayerInput->Reset();

	EntityId localClientId = gEnv->pGameFramework->GetClientActorId();
	bool isLocalPlayer = localClientId != 0 && GetEntityId() == localClientId ? true : false;

	if (!othEntId && mode)
	{
		// CCTV locations (and probably standard spectator locations soon too) are no longer registered with the network, which means it's no longer possible to send their
		// entity ids over the network. So here if a client received a zero "other entity" id along with a valid mode then we just have them choose an id for themselves. (It turns
		// out it's not actually important for other players to know what spectator or CCTV point a certain player is currently spectating from... And the same may well be true
		// for knowing which character a certain player is "following" in Follow spectator mode too?)
		switch (mode)
		{
		case eASM_Fixed:
			CRY_ASSERT_MESSAGE(!server, "The server should've already worked out a valid spectator \"other entity\" for himself!");
			othEntId = specmod->GetRandomSpectatorLocation();
			CRY_ASSERT(othEntId);
			break;
		case eASM_Free:
			break;
		case eASM_Follow:
			CRY_ASSERT_MESSAGE(!server, "The server should've already worked out a valid spectator \"other entity\" for himself!");
			CRY_TODO(18,3,2010, "Could clients choose their own Follow subject here too, meaning no spectator \"other entities\" would need to be sent over the network...?");
			break;
		default:
			CRY_ASSERT_MESSAGE(0, string().Format("Unexpected spectator mode '%d'", mode));
		}
	}

	if (mode && !spinf->mode)  // turning (any) spectator mode ON
	{
		if (isLocalPlayer)
		{
			IActionFilter* pFilter = g_pGameActions->FilterNotYetSpawned();
			if(pFilter && !pFilter->Enabled())
			{
				pFilter->Enable(true);
			}
		}

		if (IVehicle *pVehicle=GetLinkedVehicle())
		{
			if (IVehicleSeat *pSeat=pVehicle->GetSeatForPassenger(GetEntityId()))
				pSeat->Exit(false);
		}

		spinf->Reset();
		spinf->mode = mode;
		spinf->SetOtherEntIdForCurMode(othEntId);

		switch (mode)
		{
		case eASM_Follow:
			{
				m_stats.spectatorInfo.dataU.follow.invalidTargetTimer = 0.f;

				if(isLocalPlayer)
				{
					static const uint32 kDefaultCRC = CCrc32::ComputeLowercase("Default");
					const bool setOk = SetCurrentFollowCameraSettings(kDefaultCRC);
					CRY_ASSERT_MESSAGE(setOk,"Could not set the view mode to \"Default\"");
				}
			}
			break;
		case eASM_Killer:
			{
				m_stats.spectatorInfo.dataU.killer.startTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();

				if(isLocalPlayer)
				{
					static const uint32 kKillerCRC = CCrc32::ComputeLowercase("Killer");
					const bool setOk = SetCurrentFollowCameraSettings(kKillerCRC);
					CRY_ASSERT_MESSAGE(setOk,"Could not set the view mode to \"Killer\"");
				}
			}
			break;
		}

		Revive(kRFR_StartSpectating);

		if (server)
		{
			GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Spectator);
			CHANGED_NETWORK_STATE(this, ASPECT_SPECTATOR);
		}

		GetEntity()->Hide(true);

		m_stats.inAir=0.0f;
		m_stats.onGround=0.0f;

		StateMachineHandleEventMovement( PLAYER_EVENT_SPECTATE );

		if( gEnv->IsClient() && isLocalPlayer && m_pPlayerTypeComponent)
		{
			g_pGame->GetUI()->ActivateDefaultState(); // should pick "mp_spectator" unless killcam or endgame etc.
			SetClientSoundmood(ESoundmood_Spectating);
		}

		if (ICharacterInstance *pCharacter=GetEntity()->GetCharacter(0))
		{
			pCharacter->GetISkeletonPose()->DestroyCharacterPhysics(1);
		}

		CRY_ASSERT(m_pAnimatedCharacter);
		m_pAnimatedCharacter->ForceRefreshPhysicalColliderMode();
		m_pAnimatedCharacter->RequestPhysicalColliderMode( eColliderMode_Spectator, eColliderModeLayer_Game, "Actor::SetAspectProfile");
		m_usingSpectatorPhysics = true;

		if( isLocalPlayer )
		{
			StateMachineHandleEventMovement( PLAYER_EVENT_RESET_SPECTATOR_SCREEN );
		}
	}
	else if (!mode && spinf->mode)  // turning (any) spectator mode OFF
	{
		if (isLocalPlayer)
		{
			IActionFilter* pFilter = g_pGameActions->FilterNotYetSpawned();
			if(pFilter && pFilter->Enabled())
			{
				pFilter->Enable(false);
			}
		}

		GetEntity()->Hide(false);

		if (server)
		{
			GetGameObject()->SetAspectProfile(eEA_Physics, eAP_Alive);
			CHANGED_NETWORK_STATE(this, ASPECT_SPECTATOR);
		}

		spinf->Reset();  // sets mode = CActor::eASM_None
		spinf->state = CActor::eASS_Ingame;

		m_stats.inAir=0.0f;
		m_stats.onGround=0.0f;
	}
	else if ((mode != spinf->mode) || (othEntId != spinf->GetOtherEntIdForCurMode()))  // mode or data has changed (enough)
	{
		if (server)
		{
			//SetHealth(GetMaxHealth());
			CHANGED_NETWORK_STATE(this, ASPECT_SPECTATOR);
		}

		spinf->Reset();
		spinf->mode = mode;
		spinf->SetOtherEntIdForCurMode(othEntId);

		if( gEnv->IsClient() && isLocalPlayer )
		{
			g_pGame->GetUI()->ActivateDefaultState();
		}
	}

	if(isSpawning && m_usingSpectatorPhysics)
	{
		m_usingSpectatorPhysics = false;
		CRY_ASSERT(m_pAnimatedCharacter);
							
		m_pAnimatedCharacter->RequestPhysicalColliderMode( eColliderMode_Undefined, eColliderModeLayer_Game, "Actor::SetAspectProfile");

		if (IPhysicalEntity *pPhysics=GetEntity()->GetPhysics())
		{
			if (ICharacterInstance *pCharacter=GetEntity()->GetCharacter(0))
			{
				pCharacter->GetISkeletonPose()->DestroyCharacterPhysics(2);

				if (IPhysicalEntity *pCharPhysics=pCharacter->GetISkeletonPose()->GetCharacterPhysics())
				{
					pe_params_articulated_body body;
					body.pHost=pPhysics;
					pCharPhysics->SetParams(&body);
				}
			}
		} // end crazy spectator physics
	}
}

void CPlayer::MoveToSpectatorTargetPosition()
{
	// called when our target entity moves.
	IEntity* pTarget = gEnv->pEntitySystem->GetEntity(GetSpectatorTarget());
	if(!pTarget)
		return;

	Matrix34 tm = pTarget->GetWorldTM();
	tm.AddTranslation(Vec3(0,0,2));
	GetEntity()->SetWorldTM(tm);
}



bool CPlayer::UseItem(EntityId itemId)
{
	bool bOK = false;

	if(gEnv->bMultiplayer && IsClient() && m_pLocalPlayerInteractionPlugin)
	{
		EInteractionType currType = m_pLocalPlayerInteractionPlugin->GetCurrentInteractionInfo().interactionType;

		if(currType == eInteraction_GameRulesDrop || currType == eInteraction_GameRulesPickup)
		{
			return false;
		}
	}

	bOK = CActor::UseItem(itemId);
	if (bOK)
	{
		CALL_PLAYER_EVENT_LISTENERS(OnItemUsed(this, itemId));
	}

	return bOK;
}

bool CPlayer::PickUpItem(EntityId itemId, bool sound, bool select)
{
	const bool bOK = CActor::PickUpItem(itemId, sound, select);
	if (bOK)
	{
		CALL_PLAYER_EVENT_LISTENERS(OnItemPickedUp(this, itemId));
	}

	// Record 'PickupItem' telemetry stats.

	IEntity	*pPickupEntity = gEnv->pEntitySystem->GetEntity(itemId);

	CStatsRecordingMgr::TryTrackEvent(this, eGSE_PickupItem, pPickupEntity ? pPickupEntity->GetClass()->GetName() : "???");

    SHUDEvent hudevent(eHUDEvent_RemoveEntity); // We want to remove a scanned item as soon as it's picked up
    hudevent.AddData(SHUDEventData((int)itemId));
    CHUDEventDispatcher::CallEvent(hudevent);

	return bOK;
}

bool CPlayer::DropItem(EntityId itemId, float impulseScale, bool selectNext, bool byDeath)
{
	const bool bOK = CActor::DropItem(itemId, impulseScale, selectNext, byDeath);
	if (bOK)
	{
		CALL_PLAYER_EVENT_LISTENERS(OnItemDropped(this, itemId));
	}

	// Record 'DropItem' telemetry stats.

	IEntity	*pDropEntity = gEnv->pEntitySystem->GetEntity(itemId);

	CStatsRecordingMgr::TryTrackEvent(this, eGSE_DropItem, pDropEntity ? pDropEntity->GetName() : "???");

	return bOK;
}

void CPlayer::NetKill(const KillParams &killParams)
{
	//CryLog("CPlayer::NetKill: %s", GetEntity()->GetName());

	m_teamWhenKilled = killParams.targetTeam;

#if !defined(_RELEASE)
	CRY_ASSERT_TRACE(!m_dropCorpseOnDeath, ("Getting multiple deaths for %s '%s' (%s%shealth=%8.2f)", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), IsClient() ? "client, " : "", GetEntity()->GetAI() ? "AI, " : "", m_health.GetHealth()));
#endif

	bool vehicleDestructionDeath = killParams.hit_type == CGameRules::EHitType::VehicleDestruction;
	
	m_hideOnDeath = vehicleDestructionDeath;
	m_dropCorpseOnDeath = !vehicleDestructionDeath;
	//--- Ensure the character is killed before the death reaction code is triggered
	if (!m_health.IsDead())
	{
		SetHealth(0);
	}

	//--- Ensure the AI is killed - if it exists
	if ( GetEntity() && GetEntity()->GetAI() )
	{
		GetEntity()->GetAI()->Event(AIEVENT_DISABLE, NULL);
	}

	bool bRagdoll = killParams.ragdoll;

	if (IsClient())
	{
		// Disable aim IKs, otherwise they will override the camera bone movement, which could invalidate FP death animations
		m_torsoAimIK.Disable(false);

		if(gEnv->bMultiplayer)
		{
			if( IVehicle* pVehicle = GetLinkedVehicle() )
			{
				if( IVehicleSeat* pSeat = pVehicle->GetSeatForPassenger(GetEntityId()) )
				{
					pSeat->Exit(false, true);
				}
				else
				{
					LinkToVehicle(0);
				}
			}
		}

	}

	bool doingHitDeathReaction = false;

	// Note: We don't want to invoke any death reactions on AI who are killed during savegame loading
	if (!killParams.fromSerialize && g_pGameCVars->g_hitDeathReactions_enable && m_pHitDeathReactions)
	{
		// If hitDeathReactions system fails, rely on CActor::NetKill to decide if we should trigger the ragdoll from
		// KillParams struct parameters
		doingHitDeathReaction = m_pHitDeathReactions->OnKill(killParams);
	}

	bRagdoll &= !doingHitDeathReaction;

	// Interrupt ongoing spectacular kill
	//  [7/30/2010 davidr] ToDo: Manage this inside CSpectacularKill through events/msgs/whatever
	if (m_spectacularKill.IsBusy())
		m_spectacularKill.End(true);

	CActor::KillParams modifiedKillParams(killParams);
	modifiedKillParams.ragdoll = bRagdoll;

	SetRecentKiller(killParams.shooterId, killParams.hit_type);

	if(gEnv->bMultiplayer)
	{	
		if(killParams.hit_type == CGameRules::EHitType::PunishFall)
		{
			if(IsClient())
			{
				m_pPlayerTypeComponent->StartFreeFallDeath();
			}
		}
		else if(!IsThirdPerson() && g_pGameCVars->pl_switchTPOnKill)
		{
			ToggleThirdPerson();
		}

		if(killParams.shooterId == gEnv->pGameFramework->GetClientActorId() && killParams.shooterId != killParams.targetId)
		{
			CAudioSignalPlayer::JustPlay("Human_Feedback_KilledByPlayerHit_MP", killParams.shooterId);
		}
	}

	if(m_pSprintStamina != NULL)
	{
		m_pSprintStamina->Reset( GetEntityId() );

		if (m_pSprintStamina->HasChanged())
		{
			CALL_PLAYER_EVENT_LISTENERS(OnSprintStaminaChanged(this, m_pSprintStamina->Get()));
		}
	}

	// Otherwise PlayerState::PrePhysicsUpdate will cause the player no longer gets updated and some systems
	// (like the HitDeathReactions) need updating even when dead
	m_stats.isInBlendRagdoll=false;

	CActor::NetKill(modifiedKillParams);
}

bool CPlayer::IsInFreeFallDeath() const
{
	return IsClient() ? m_pPlayerTypeComponent->IsInFreeFallDeath() : false;
}

void CPlayer::PlaySound(EPlayerSounds soundID, bool play, const char* paramName, float paramValue, const char* paramName2, float paramValue2, float volume, bool playOnRemote )
{
	// Make sure to keep audio functionality even when not in game mode!
	if (IsClient() || playOnRemote) //currently this is only supposed to be heard by the client (not 3D, not MP)
	{
		switch (soundID)
		{
		case CPlayer::ESound_Player_First:
			break;
		case CPlayer::ESound_Jump:
			if (gEnv->pInput && play)
			{
				gEnv->pInput->ForceFeedbackEvent(SFFOutputEvent(eIDT_Gamepad, eFF_Rumble_Basic, SFFTriggerOutputData::Initial::Default, 0.05f, 0.05f, 0.1f));
			}
			break;
		case CPlayer::ESound_Fall_Drop:
			if (gEnv->pInput && play)
			{
				gEnv->pInput->ForceFeedbackEvent(SFFOutputEvent(eIDT_Gamepad, eFF_Rumble_Basic, SFFTriggerOutputData::Initial::Default, 0.2f, 0.3f, 0.2f));
			}
			break;
		case CPlayer::ESound_ChargeMelee:
			break;
		case CPlayer::ESound_Breathing_UnderWater:
			break;
		case CPlayer::ESound_Gear_Walk:
			break;
		case CPlayer::ESound_Gear_Run:
			break;
		case CPlayer::ESound_Gear_Jump:
			break;
		case CPlayer::ESound_Gear_Land:
			break;
		case CPlayer::ESound_Gear_HeavyLand:
			break;
		case CPlayer::ESound_Gear_Water:
			break;
		case CPlayer::ESound_FootStep_Boot:
			break;
		case CPlayer::ESound_FootStep_Boot_Armor:
			break;
		case CPlayer::ESound_DiveIn:
			if (m_pIEntityAudioComponent)
			{
				if (m_waterDiveIn != CryAudio::InvalidControlId)
				{
					if (m_waterInOutSpeed != CryAudio::InvalidControlId)
					{
						m_pIEntityAudioComponent->SetParameter(m_waterInOutSpeed, paramValue);
					}

					m_pIEntityAudioComponent->ExecuteTrigger(m_waterDiveIn);
				}
			}
			break;
		case CPlayer::ESound_DiveOut:
			if (m_pIEntityAudioComponent)
			{
				if (m_waterDiveOut != CryAudio::InvalidControlId)
				{
					if (m_waterInOutSpeed != CryAudio::InvalidControlId)
					{
						m_pIEntityAudioComponent->SetParameter(m_waterInOutSpeed, paramValue);
					}

					m_pIEntityAudioComponent->ExecuteTrigger(m_waterDiveOut);
				}
			}
			break;
		case CPlayer::ESound_WaterEnter:
			if (m_pIEntityAudioComponent && m_waterEnter != CryAudio::InvalidControlId)
			{
				m_pIEntityAudioComponent->ExecuteTrigger(m_waterEnter);
			}
			break;
		case CPlayer::ESound_WaterExit:
			if (m_pIEntityAudioComponent && m_waterExit != CryAudio::InvalidControlId)
			{
				m_pIEntityAudioComponent->ExecuteTrigger(m_waterExit);
			}
			break;
		case CPlayer::ESound_EnterMidHealth:
			break;
		case CPlayer::ESound_ExitMidHealth:
			break;
		case CPlayer::ESound_MedicalMonitorRegen:
			break;
		case CPlayer::ESound_Player_Last:
			break;
		default:
			CRY_ASSERT(soundID < ESound_Player_Last);
			break;
		}
	}
}


//-----------------------------------------------------------------------
void CPlayer::GetMemoryUsage(ICrySizer * pSizer) const
{
	pSizer->AddObject(this,sizeof(*this));
	GetInternalMemoryUsage(pSizer);
}

void CPlayer::GetInternalMemoryUsage(ICrySizer * pSizer) const
{
	pSizer->AddObject(m_pPlayerInput);
#ifdef PLAYER_MOVEMENT_DEBUG_ENABLED
	m_movementDebug.GetInternalMemoryUsage(pSizer);
#endif
	pSizer->AddObject(m_hitRecoilGameEffect);
	pSizer->AddObject(m_playerHealthEffect);
	CActor::GetInternalMemoryUsage(pSizer); // collect memory of parent class
	if (m_pPickAndThrowProxy)
		pSizer->AddObject(m_pPickAndThrowProxy.get(), sizeof(CPickAndThrowProxy));
}

#ifdef PLAYER_MOVEMENT_DEBUG_ENABLED
void CPlayer::DebugGraph_AddValue(const char* id, float value) const
{
	m_movementDebug.DebugGraph_AddValue(id, value);
}
#endif

//-----------------------------------------------------------------------
bool CPlayer::CanFire() const
{
	const bool isMelee = false;
	return CanFireOrMelee(isMelee);
}

//-----------------------------------------------------------------------
bool CPlayer::CanMelee() const
{
	const bool isMelee = true;
	return CanFireOrMelee(isMelee);
}

//-----------------------------------------------------------------------
bool CPlayer::CanToggleCamera() const
{
	const int flags = EPlayerStateFlags_OnLadder;
	return !m_stateMachineMovement.StateMachineAnyActiveFlag(flags);
}

//-----------------------------------------------------------------------
bool CPlayer::CanFireOrMelee(bool isMelee) const
{
	bool bCanFire = true;

	if (!IsPlayer())
	{
		bCanFire = CanFire_AI();
	}
	else if (gEnv->IsDedicated())
	{
		bCanFire = CanFire_DedicatedClient();
	}
	else
	{
		bCanFire = CanFire_Player(isMelee);
	}

	return bCanFire;
}

//-----------------------------------------------------------------------
bool CPlayer::CanFire_AI() const
{
	return !IsCloaked();
}

//-----------------------------------------------------------------------
bool CPlayer::CanFire_DedicatedClient() const
{
	// dedicated clients can always fire
	return true;
}

//-----------------------------------------------------------------------
bool CPlayer::CanFire_Player(bool isMelee) const
{
	//1- Player can not fire if weapon is lowered
	bool isWeaponLower = IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeapon) || IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeaponMP);
	bool isMovementRestricted = IsCinematicFlagActive(SPlayerStats::eCinematicFlag_RestrictMovement);
	if ((isMelee && isMovementRestricted) || (!isMelee && isWeaponLower))
	{
		return false;
	}

	//2- Player can not fire while his weapon is underwater level
	if(!IsWeaponUnderWater())
	{
		return true;
	}
	else
	{
		CWeapon* pWeapon = GetWeapon(GetCurrentItemId());
		return pWeapon && pWeapon->CanFireUnderWater();
	}
}

bool CPlayer::IsWeaponUnderWater() const
{
	//	(doesn't apply in vehicles - CVehicleWeapon::CanFire() takes care of this,
	//	 else it breaks in 3rd person view)
	const Vec3 weaponPosition = GetEntity()->GetWorldPos() + GetWeaponOffset();
	if (!GetLinkedVehicle() && (m_playerStateSwim_WaterTestProxy.GetWaterLevel() > weaponPosition.z))
		return true;

	return false;
}

//-----------------------------------------------------------------------
void CPlayer::OnIntroSequenceFinished()
{
	RegisterOnHUD(); 

	IEntity* pEntity = GetEntity(); 
	// CryLogAlways("[OnIntroSequenceFinished()] [%d %s] [Time First spawned %.3f, IsDead %s]", pEntity->GetId(), pEntity->GetName(), m_timeFirstSpawned, IsDead() ? "TRUE" : "FALSE");

	// Any remote players *already* fully spawned, need to be unhidden.
	// Those players still spectating, or watching their own intros, need to be made visble on next spawn
	if( m_timeFirstSpawned > 0.0f && !IsDead() )
	{
		// CryLogAlways("[OnIntroSequenceFinished() - making entity VISIBLE DIRECTLY! - not dead + has spawned]"); 
		GetEntity()->Invisible(false);
		GetEntity()->Hide(false); 
	}
	else
	{
		// CryLogAlways("[OnIntroSequenceFinished() - making entity visible on next spawn - dead or has not already spawned]"); 
		m_bMakeVisibleOnNextSpawn = true; 
	}
}

//-----------------------------------------------------------------------
bool CPlayer::IsSprinting() const
{
	//Only for the player
	return (IsPlayer() && m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Sprinting ) );
}

bool CPlayer::IsSwimming() const
{
	return( m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Swimming ) );
}

bool CPlayer::IsHeadUnderWater() const
{
	return m_playerStateSwim_WaterTestProxy.IsHeadUnderWater();
}

bool CPlayer::IsInAir() const
{
	return m_stateMachineMovement.StateMachineActiveFlag(EPlayerStateFlags_InAir);
}

void CPlayer::SetViewLimits(Vec3 dir, float rangeH, float rangeV)
{
	m_params.viewLimits.SetViewLimit(dir, rangeH, rangeV, 0.0f, 0.0f, SViewLimitParams::eVLS_None);
}

//------------------------------------------------------------------------
void
CPlayer::StagePlayer(bool bStage, SStagingParams* pStagingParams /* = 0 */)
{
	if (IsClient() == false)
		return;

	EStance stance = STANCE_NULL;
	bool bLock = false;
	if (bStage == false)
	{
		m_params.viewLimits.ClearViewLimit(SViewLimitParams::eVLS_Stage);
	}
	else if (pStagingParams != 0)
	{
		bLock = pStagingParams->bLocked;

		m_params.viewLimits.SetViewLimit(	pStagingParams->vLimitDir,
																			pStagingParams->vLimitRangeH,
																			pStagingParams->vLimitRangeV,
																			0.f,
																			0.f,
																			SViewLimitParams::eVLS_Stage);

		stance = pStagingParams->stance;
		if (bLock)
		{
			IPlayerInput* pPlayerInput = GetPlayerInput();
			if(pPlayerInput)
				pPlayerInput->Reset();
		}
	}
	else
	{
		bStage = false;
	}

	if (bLock || m_stagingParams.bLocked)
	{
		g_pGameActions->FilterNoMove()->Enable(bLock);
	}
	m_stagingParams.bActive = bStage;
	m_stagingParams.bLocked = bLock;
	m_stagingParams.vLimitDir = m_params.viewLimits.GetViewLimitDir();
	m_stagingParams.vLimitRangeH = m_params.viewLimits.GetViewLimitRangeH();
	m_stagingParams.vLimitRangeV = m_params.viewLimits.GetViewLimitRangeV();
	m_stagingParams.stance = stance;
	if (stance != STANCE_NULL)
		SetStance(stance);
}

void CPlayer::ResetScreenFX()
{
	if (IsClient() && m_pPlayerTypeComponent)
	{	
		m_pPlayerTypeComponent->ResetScreenFX();
	}
}

void CPlayer::ResetFPView()
{
	if (gEnv->pRenderer)
	{
		float defaultFov = gEnv->bMultiplayer ? 60.0f : 55.0f;
		gEnv->pRenderer->EF_Query(EFQ_SetDrawNearFov, defaultFov);
	}
}

void CPlayer::ForceRefreshStanceAndEyeOffsetNow()
{
	UpdateStance();

	m_eyeOffset = GetStanceViewOffset(m_stance);
}

void CPlayer::NotifyObjectGrabbed(bool bIsGrab, EntityId objectId, bool bIsNPC, bool bIsTwoHanded /*= false*/, float fThrowSpeed)
{
	CALL_PLAYER_EVENT_LISTENERS(OnObjectGrabbed(this, bIsGrab, objectId, bIsNPC, bIsTwoHanded));

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(objectId);
	IEntityScriptComponent* pScriptProx = (IEntityScriptComponent*)pEntity->GetProxy(ENTITY_PROXY_SCRIPT);
	IScriptTable *pTable = pScriptProx->GetScriptTable();

	if(pTable && pTable->HaveValue("OnPickup"))
	{
		ScriptAnyValue arg1 = bIsGrab;
		ScriptAnyValue arg2 = fThrowSpeed;

		Script::CallMethod(pTable, "OnPickup", arg1, arg2);
	}

	if (!gEnv->bMultiplayer)
	{
		if (bIsNPC && bIsGrab)
		{
			if (CGameAISystem* pGameAISystem = g_pGame->GetGameAISystem())
			{
				if (GameAI::DeathManager* pDeathManager = pGameAISystem->GetDeathManager())
				{
					pDeathManager->OnAgentGrabbedByPlayer(objectId);
				}
			}
		}
	}
}

struct RecursionFlagLock
{
	RecursionFlagLock(bool& flag) : m_bFlag(flag) { m_bFlag = true; }
	~RecursionFlagLock() { m_bFlag = false; }
	bool m_bFlag;
};

//--------------------------------------------------------
void CPlayer::AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event)
{
	CActor::AnimationEvent(pCharacter,event);

	bool isMainCharacter = (pCharacter != GetShadowCharacter());
	if (isMainCharacter)
	{
		if (m_spectacularKill.AnimationEvent(pCharacter, event))
			return;

		// HitDeathReactions can "eat" animation events
		if (m_pHitDeathReactions && m_pHitDeathReactions->OnAnimationEvent(event))
			return;

		// Some weapons can also "eat" animation events
		static IEntityClass* pPickAndThrowWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("PickAndThrowWeapon");
		static IEntityClass* pJAWClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("JAW");
		IItem* pItem																  = GetCurrentItem(false); 
		if(pItem)
		{
			if (pItem->GetEntity()->GetClass() == pPickAndThrowWeaponClass)
			{
				CPickAndThrowWeapon* pPickAndThrowWeapon = static_cast<CPickAndThrowWeapon*>(pItem);
				if(pPickAndThrowWeapon->OnAnimationEvent(event))
				{
					// Pick and throw has consumed this event
					return;
				}
			}
			else if(pItem->GetEntity()->GetClass() == pJAWClass)
			{
				static_cast<CWeapon*>(pItem)->AnimationEvent(pCharacter, event);
			}
		}

		const bool isClient = m_isClient;
		const SActorAnimationEvents& animEventsTable = GetAnimationEventsTable();
		static const uint32 audioTriggerCRC = CCrc32::ComputeLowercase("audio_trigger");

		if (animEventsTable.m_meleeHitId == event.m_EventNameLowercaseCRC32)
		{
			CWeapon* pWeapon = GetWeapon(GetCurrentItemId());
			CMelee* pMelee = pWeapon ? pWeapon->GetMelee() : NULL;

			if (pMelee)
				pMelee->OnMeleeHitAnimationEvent();
		}
		else if (event.m_EventNameLowercaseCRC32 == audioTriggerCRC)
		{
			//Only client ones (the rest are processed in AudioProxy)
			if (isClient && m_pIEntityAudioComponent)
			{
				CryAudio::AuxObjectId auxObjectId = CryAudio::InvalidAuxObjectId;

				if (event.m_BonePathName && event.m_BonePathName[0] && pCharacter)
				{
					ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
					IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
					int nJointID = rIDefaultSkeleton.GetJointIDByName(event.m_BonePathName);
					if (nJointID >= 0)
					{
						auxObjectId = stl::find_in_map(m_cJointAudioProxies, nJointID, CryAudio::InvalidAuxObjectId);
						if (auxObjectId == CryAudio::InvalidAuxObjectId)
						{
							auxObjectId = m_pIEntityAudioComponent->CreateAudioAuxObject();
							m_cJointAudioProxies[nJointID] = auxObjectId;
						}

						m_pIEntityAudioComponent->SetAudioAuxObjectOffset(Matrix34(pSkeletonPose->GetAbsJointByID(nJointID)), auxObjectId);
					}
				}
				CryAudio::ControlId const triggerId = CryAudio::StringToId(event.m_CustomParameter);
				m_pIEntityAudioComponent->ExecuteTrigger(triggerId, auxObjectId);

				REINST("needs verification!");
				/*int flags = FLAG_SOUND_DEFAULT_3D;
				if (strchr(event.m_CustomParameter, ':') == NULL)
					flags |= FLAG_SOUND_VOICE;

				if(m_pIEntityAudioComponent)
					m_pIEntityAudioComponent->PlaySound(event.m_CustomParameter, offset, FORWARD_DIRECTION, flags, 0, eSoundSemantic_Animation, 0, 0);*/
			}

		}
		else if (animEventsTable.m_forceFeedbackId == event.m_EventNameLowercaseCRC32)
		{
			if (isClient && (event.m_CustomParameter != NULL))
			{
				IForceFeedbackSystem* pForceFeedback = g_pGame->GetIGameFramework()->GetIForceFeedbackSystem();
				ForceFeedbackFxId fxId = pForceFeedback->GetEffectIdByName(event.m_CustomParameter);
				pForceFeedback->PlayForceFeedbackEffect(fxId, SForceFeedbackRuntimeParams(1.0f, 0.0f));
			}
		}
		else if (animEventsTable.m_ragdollStartId == event.m_EventNameLowercaseCRC32)
		{
			if( !m_stats.isInBlendRagdoll )
			{
				//--- Initiate rag-doll between now & the end of the animation
				CAnimationPlayerProxy *animPlayerProxy = GetAnimatedCharacter()->GetAnimationPlayerProxy(0);
				const float expectedDurationSeconds = animPlayerProxy ? animPlayerProxy->GetTopAnimExpectedSecondsLeft(GetEntity(), 0) : -1;
				if (0 <= expectedDurationSeconds)
				{
					m_ragdollTime = cry_random(0.0f, 1.0f) * expectedDurationSeconds;
				}
			}
		}
		else if(animEventsTable.m_footstepSignalId == event.m_EventNameLowercaseCRC32)
		{
			OnFootStepAnimEvent(pCharacter, event.m_BonePathName);
		}
		else if(animEventsTable.m_foleySignalId == event.m_EventNameLowercaseCRC32)
		{
			OnFoleyAnimEvent(pCharacter, event.m_CustomParameter, event.m_BonePathName);
		}
		else if(animEventsTable.m_footStepImpulseId == event.m_EventNameLowercaseCRC32)
		{
			OnFootStepImpulseAnimEvent(pCharacter, event);
		}
		else if(animEventsTable.m_swimmingStrokeId == event.m_EventNameLowercaseCRC32)
		{
			OnSwimmingStrokeAnimEvent(pCharacter);
		}
		else if (animEventsTable.m_groundEffectId == event.m_EventNameLowercaseCRC32)
		{
			OnGroundEffectAnimEvent(pCharacter, event);
		}
		else if (animEventsTable.m_grabObjectId == event.m_EventNameLowercaseCRC32)
		{
			if (m_pickingUpCarryObject && m_pIAttachmentGrab)
			{
				CEntityAttachment *entAttach = new CEntityAttachment();
				entAttach->SetEntityId(m_carryObjId);
				m_pIAttachmentGrab->AddBinding(entAttach);
			}
		}
		else if (animEventsTable.m_stowId == event.m_EventNameLowercaseCRC32)
		{
			CWeapon *pWeapon = GetWeapon(GetCurrentItemId());
			if (pWeapon)
			{
				if (pWeapon->IsDeselecting())
				{
					pWeapon->AttachToHand(false);
					pWeapon->AttachToBack(true);
				}
			}
		}
		else if(animEventsTable.m_weaponLeftHandId == event.m_EventNameLowercaseCRC32)
		{
			CItem* pCurrentItem = static_cast<CItem*>(GetCurrentItem());
			
			if(pCurrentItem)
			{
				pCurrentItem->SwitchToHand(1);
			}
		}
		else if(animEventsTable.m_weaponRightHandId == event.m_EventNameLowercaseCRC32)
		{
			CItem* pCurrentCItem = static_cast<CItem*>(GetCurrentItem());

			if(pCurrentCItem)
			{
				pCurrentCItem->SwitchToHand(0);
			}
		}
		else if(animEventsTable.m_killId == event.m_EventNameLowercaseCRC32)
		{
			OnKillAnimEvent(event);
		}
		else if(animEventsTable.m_deathBlow == event.m_EventNameLowercaseCRC32)
		{
			if (m_stats.spectacularKillPartner != 0)
			{
				IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(m_stats.spectacularKillPartner);
				if (pActor && (pActor->GetActorClass() == CPlayer::GetActorClassType()))
				{
					CPlayer* pPartner = static_cast<CPlayer*>(pActor);
					pPartner->GetSpectacularKill().AnimationEvent(pPartner->GetEntity()->GetCharacter(0), event);
				}
				// DeathBlow animation events with parameter "1" are marking the death as pending for the SpectacularKill system,
				// so we should ignore them here
				else if (!event.m_CustomParameter || (event.m_CustomParameter[0] != '1'))
				{
					OnKillAnimEvent(event);
				}
			}
			// DeathBlow animation events with parameter "1" are marking the death as pending for the SpectacularKill system,
			// so we should ignore them here
			else if (!event.m_CustomParameter || (event.m_CustomParameter[0] != '1'))
			{
				OnKillAnimEvent(event);
			}
		}
		else if (animEventsTable.m_startFire == event.m_EventNameLowercaseCRC32)
		{
			CWeapon *pWeapon = GetWeapon(GetCurrentItemId());
			if (pWeapon)
			{
				pWeapon->OnAnimationEventStartFire(event.m_CustomParameter);
			}
		}
		else if (animEventsTable.m_stopFire == event.m_EventNameLowercaseCRC32)
		{
			CWeapon *pWeapon = GetWeapon(GetCurrentItemId());
			if (pWeapon)
			{
				pWeapon->OnAnimationEventStopFire();
			}
		}
		else if (animEventsTable.m_shootGrenade == event.m_EventNameLowercaseCRC32)
		{
			if(IAIActor* aiActor = CastToIAIActorSafe(GetEntity()->GetAI()))
			{
				const SOBJECTSTATE& state = aiActor->GetState();
				if(CWeapon *secondaryWeapon = static_cast<CWeapon *>(aiActor->GetProxy()->GetSecWeapon(state.requestedGrenadeType)))
					secondaryWeapon->OnAnimationEventShootGrenade(event);
			}
		}
	}
	else  
	{
		const SActorAnimationEvents& animEventsTable = GetAnimationEventsTable();
		if(animEventsTable.m_footstepSignalId == event.m_EventNameLowercaseCRC32)
		{
			if (event.m_BonePathName && event.m_BonePathName[0])
			{
				OnFootStepAnimEvent(pCharacter, event.m_BonePathName);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::OnFootStepAnimEvent(ICharacterInstance* pCharacter, const char* boneName)
{
	int32 nFootJointId = 0;

	if (boneName && (boneName[0] != 0x00) && pCharacter)
	{
		IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
		nFootJointId = rIDefaultSkeleton.GetJointIDByName(boneName);
	}

	if (nFootJointId != -1)
	{
		if (ShouldUpdateNextFootStep())
		{
			const float fFrameTime = gEnv->pTimer->GetFrameTime();
			ExecuteFootStep(pCharacter, fFrameTime, nFootJointId);
		}
	}
	else
		GameWarning("%s has incorrect foot JointID in animation triggered footstep sounds", GetEntity()->GetName());
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::OnFoleyAnimEvent(ICharacterInstance* pCharacter, const char* CustomParameter, const char* boneName)
{
	int32 nBoneJointId = 0;

	if (boneName && (boneName[0] != 0x00) && pCharacter)
	{
		IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
		nBoneJointId = rIDefaultSkeleton.GetJointIDByName(boneName);
	}

	if (nBoneJointId != -1)
	{
		const float fFrameTime = gEnv->pTimer->GetFrameTime();
		ExecuteFoleySignal(pCharacter, fFrameTime, CustomParameter, nBoneJointId);
	}
	else
		GameWarning("%s has incorrect foot JointID in animation triggered foley sounds", GetEntity()->GetName());
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::OnGroundEffectAnimEvent(ICharacterInstance* pCharacter, const AnimEventInstance &event)
{
	int32 nJointId = -1;
	if (event.m_BonePathName && event.m_BonePathName[0] && pCharacter)
	{
		IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
		nJointId = rIDefaultSkeleton.GetJointIDByName(event.m_BonePathName);
	}

	const float fFrameTime = gEnv->pTimer->GetFrameTime();
	ExecuteGroundEffectAnimEvent(pCharacter, fFrameTime, event.m_CustomParameter, nJointId);
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::OnFootStepImpulseAnimEvent(ICharacterInstance* pCharacter, const AnimEventInstance &event)
{
	// special event for smart object animations, trigger a downwards impulse from the specified bone position
	//	(intended for vehicle-climb SO, initially, but should work more generally too)
	IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
	const int32 nFootJointId = rIDefaultSkeleton.GetJointIDByName(event.m_BonePathName);

	if (nFootJointId != -1)
	{
		ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
		const QuatT foot = pSkeletonPose->GetAbsJointByID(nFootJointId);

		Vec3 pos = GetEntity()->GetSlotWorldTM(0) * foot.t;
		Vec3 dir = event.m_vDir.GetNormalizedSafe();
		pos -= dir * 0.05f;

		m_deferredFootstepImpulse.DoCollisionTest(pos, dir, 0.25f,(float)atof(event.m_CustomParameter), GetEntity()->GetPhysics());
	}
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::OnSwimmingStrokeAnimEvent(ICharacterInstance* pCharacter)
{
	const Vec3 pos = GetEntity()->GetSlotWorldTM(0) * GetBoneTransform(BONE_HEAD).t;
	const float waterLevel = gEnv->p3DEngine->GetWaterLevel(&pos);
	if (waterLevel != WATER_LEVEL_UNKNOWN)
	{
		const float depth = waterLevel - pos.z;
		if (depth <= 0.2f)
		{
			const float relativeSpeed = max(0.2f, GetLastRequestedVelocity().GetLength() * 0.5f);
			gEnv->p3DEngine->AddWaterRipple(GetEntity()->GetWorldPos(), 1.0f, relativeSpeed);
		}
	}
}

void CPlayer::RequestEnterPickAndThrow( EntityId entityPicked )
{
	if(!gEnv->bMultiplayer)
	{
		EnterPickAndThrow(entityPicked);
	}
	else
	{
		CEnvironmentalWeapon *pEnvWeapon = static_cast<CEnvironmentalWeapon*>(g_pGame->GetIGameFramework()->QueryGameObjectExtension(entityPicked, "EnvironmentalWeapon"));
		if(pEnvWeapon)
		{
			pEnvWeapon->RequestUse(GetEntityId());
		}
	}
}

void CPlayer::EnterPickAndThrow( EntityId entityPicked, bool selectImmediately /*=true*/, bool forceSelect /*=false*/ )
{
	const SPlayerStats::ECinematicFlags flags = static_cast<SPlayerStats::ECinematicFlags>(SPlayerStats::eCinematicFlag_LowerWeapon|SPlayerStats::eCinematicFlag_LowerWeaponMP|SPlayerStats::eCinematicFlag_HolsterWeapon);
	if (!(IsInCinematicMode() && IsCinematicFlagActive(flags)))
	{
		const char* const name = "PickAndThrowWeapon";
		IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( name );
		EntityId pickAndThrowWeaponId = GetInventory()->GetItemByClass( pClass );

		CRY_ASSERT_MESSAGE( pickAndThrowWeaponId, "Player does not have the 'PickAndThrowWeapon' item" );
		if (pickAndThrowWeaponId)
		{
			m_stats.prevPickAndThrowEntity = 0;
			m_stats.pickAndThrowEntity = entityPicked;
			
			if( selectImmediately )
			{
				SelectItem( pickAndThrowWeaponId, true, forceSelect );
			}
			else if(IsClient())
			{
				ScheduleItemSwitch( pickAndThrowWeaponId, true, eICT_Primary | eICT_Secondary, true );
			}

			m_stats.isInPickAndThrowMode = true;

			LockInteractor(entityPicked, true);
		}

		// Record 'Grab' telemetry stats.

		IEntity	*pPickAndThrowEntity = gEnv->pEntitySystem->GetEntity(entityPicked);

		CStatsRecordingMgr::TryTrackEvent(this, eGSE_Grab, pPickAndThrowEntity ? pPickAndThrowEntity->GetName() : "???");
	}
}

void CPlayer::ExitPickAndThrow(bool forceInstantDrop)
{
	if (IsInPickAndThrowMode())
	{
		LockInteractor(m_stats.pickAndThrowEntity, false);

		if(forceInstantDrop)
		{
			DeselectWeapon();
		}

		m_stats.prevPickAndThrowEntity = m_stats.pickAndThrowEntity;
		m_stats.pickAndThrowEntity = 0;
		m_stats.isInPickAndThrowMode = false;

		if(!forceInstantDrop)
		{
			SelectLastItem(false, true);
		}
	}
}

bool CPlayer::HasShadowCharacter() const
{
	return GetEntity()->GetCharacter(GetShadowCharacterSlot()) != NULL;
}

int  CPlayer::GetShadowCharacterSlot() const
{
	return 5;
}

ICharacterInstance *CPlayer::GetShadowCharacter() const
{
	return GetEntity()->GetCharacter(GetShadowCharacterSlot());
}

IInteractor *CPlayer::GetInteractor()
{
	if(IsClient())
	{
		if (!m_pInteractor)
			m_pInteractor = (IInteractor*) GetGameObject()->AcquireExtension("Interactor");
		return m_pInteractor;
	}

	return NULL;
}

void CPlayer::UnlockInteractor(EntityId unlockId)
{
	IInteractor* pInteractor = GetInteractor();

	if(pInteractor && pInteractor->GetLockedEntityId() == unlockId)
	{
		LockInteractor(unlockId, false);
	}
}

// NB: This function crashes if passed a NULL damageType... so please don't do it!
void CPlayer::DamageInfo(EntityId shooterID, EntityId weaponID, IEntityClass *pProjectileClass, float damage, int damageType, const Vec3 hitDirection)
{
	CRY_ASSERT (damageType);

	CActor::DamageInfo(shooterID, weaponID, pProjectileClass, damage, damageType, hitDirection);
	if (IsClient())
	{
		m_hitRecoilGameEffect.AddHit(this, pProjectileClass, damage, damageType, hitDirection);

		float hitSpeed = 100.0f;
		if (pProjectileClass)
		{
			const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(pProjectileClass);
			if (pAmmoParams)
			{
				hitSpeed = pAmmoParams->speed;
			}
		}
		m_playerHealthEffect.OnHit(hitDirection, damage, hitSpeed);
	}

	m_stats.lastAttacker = shooterID;
	const float maxDamageForHeatingInv = 0.005f;
	const float maxHeatPulse = 0.4f;
	const float maxPulseTime = 1.5f;
	const float damageFactor = clamp_tpl(damage * maxDamageForHeatingInv, 0.0f, 1.0f);

	m_heatController.AddHeatPulse(maxHeatPulse * damageFactor, maxPulseTime * (damageFactor * damageFactor));
}

bool CPlayer::CanDoSlideKick() const
{
	if( IsSliding() )
	{
		const SPlayerSlideControl& slideCvars = gEnv->bMultiplayer ? g_pGameCVars->pl_sliding_control_mp : g_pGameCVars->pl_sliding_control;

		const SPlayerStats& stats = *GetActorStats();
		bool canSlide =
			!IsSwimming() &&
			(stats.pickAndThrowEntity == 0) &&
			((stats.onGround > 0.0f)) &&
			(stats.speedFlat >= slideCvars.min_speed_threshold) &&
			!IsMovementRestricted() &&
			!HasHeavyWeaponEquipped();

		return canSlide;
	}
	return false;
}

bool CPlayer::IsPlayerOkToAction() const
{
	//-- eAP_Sleep only used in Single player mode, so wrap it with a multiplayer flag test to remove horrible GetAspectProfile() call during multiplayer games.
	//-- Ideally, we'll want to remove it completely at some point.
	return !m_health.IsDead() && !m_stats.isInBlendRagdoll && (gEnv->bMultiplayer || (GetGameObject()->GetAspectProfile(eEA_Physics)!=eAP_Sleep)); 
}
	

bool CPlayer::IsOnLedge() const
{
	 return m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Ledge );
}

bool CPlayer::HasBeenOffLedgeSince( float fTimeSinceOnLedge ) const
{
	if( IsOnLedge() )
	{
		return false;
	}

	return( (gEnv->pTimer->GetAsyncCurTime() - m_lastLedgeTime) > fTimeSinceOnLedge );
}

bool CPlayer::IsOnGround() const
{
	return (m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Ground ));
}

bool CPlayer::IsSliding() const
{
	return( m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Sliding ) );
}

bool CPlayer::IsExitingSlide() const
{
	return( m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_ExitingSlide ) );
}

//--------------------------------------------------------------
void CPlayer::OnTeleported()
{
	//fix state here for example grabbing objects, turrets, vehicles ..
	StateMachineHandleEventMovement( PLAYER_EVENT_GROUND );

	ResetAnimations();
}

//--------------------------------------------------------------
bool CPlayer::CanSwitchItems() const
{
	if (!m_health.IsDead() && (m_stats.animationControlledID != 0)
				|| (m_stats.mountedWeaponID != 0) 
				|| IsSwimming() 
				|| IsSliding() 
				|| IsInPickAndThrowMode() 
				|| IsOnLedge()
				|| IsOnLadder()
				|| (m_stats.bAttemptingStealthKill || m_stats.bStealthKilled || m_stats.bStealthKilling)
				|| (m_stats.cinematicFlags & SPlayerStats::eCinematicFlag_HolsterWeapon)
				)
	{
		return false;
	}

	if (g_pGame->GetGameRules() && !g_pGame->GetGameRules()->CanPlayerSwitchItem(GetEntity()->GetId()))
		return false;

	if (!m_enableSwitchingItems)
	{
		return false;
	}

	if(IsStillWaitingOnServerUseResponse())
	{
		return false;
	}

	return true;
}

//-----------------------------------------
bool CPlayer::HasHeavyWeaponEquipped() const
{
	return m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_CurrentItemIsHeavy );
}

SPlayerRotationParams::EAimType CPlayer::GetCurrentAimType() const
{
	EStance playerStance = GetStance();

	if (IsSliding())
	{
		return SPlayerRotationParams::EAimType_SLIDING;
	}
	else if (IsSprinting())
	{
		return SPlayerRotationParams::EAimType_SPRINTING;
	}
	else if (playerStance == STANCE_CROUCH)
	{
		return SPlayerRotationParams::EAimType_CROUCH;
	}
	else if (IsSwimming())
	{
		return SPlayerRotationParams::EAimType_SWIM;
	}
	else if (GetCurrentItem() && static_cast<CItem*>(GetCurrentItem())->IsMounted())
	{
		return SPlayerRotationParams::EAimType_MOUNTED_GUN;
	}

	return SPlayerRotationParams::EAimType_NORMAL;
}




SAimAccelerationParams::SAimAccelerationParams()
	:	angle_min(-80.0f)
	,	angle_max(80.0f)
{
}


void SPlayerRotationParams::Reset(const IItemParamsNode* pRootNode)
{
	const IItemParamsNode* pParamNode = pRootNode->GetChild("PlayerRotation");
	if (pParamNode)
	{
		ReadAimParams(pParamNode, "Normal", EAimType_NORMAL, true);
		ReadAimParams(pParamNode, "Crouch", EAimType_CROUCH, true);
		ReadAimParams(pParamNode, "Sliding", EAimType_SLIDING, true);
		ReadAimParams(pParamNode, "Sprinting", EAimType_SPRINTING, true);
		ReadAimParams(pParamNode, "Swim", EAimType_SWIM, true);
		ReadAimParams(pParamNode, "MountedGun", EAimType_MOUNTED_GUN, true);
	}

	pParamNode = pRootNode->GetChild("PlayerRotation3rdPerson");
	if (pParamNode)
	{
		ReadAimParams(pParamNode, "Normal", EAimType_NORMAL, false);
		ReadAimParams(pParamNode, "Crouch", EAimType_CROUCH, false);
		ReadAimParams(pParamNode, "Sliding", EAimType_SLIDING, false);
		ReadAimParams(pParamNode, "Sprinting", EAimType_SPRINTING, false);
		ReadAimParams(pParamNode, "Swim", EAimType_SWIM, false);
		ReadAimParams(pParamNode, "MountedGun", EAimType_MOUNTED_GUN, false);
	}
}



void SPlayerRotationParams::ReadAimParams(const IItemParamsNode* pRootNode, const char* aimTypeName, EAimType aimType, bool firstPerson)
{
	const IItemParamsNode* pAimNode = pRootNode->GetChild(aimTypeName);

	if (pAimNode)
	{
		ReadAccelerationParams(pAimNode->GetChild("Horizontal"), &m_horizontalAims[firstPerson ? 0 : 1][aimType]);
		ReadAccelerationParams(pAimNode->GetChild("Vertical"), &m_verticalAims[firstPerson ? 0 : 1][aimType]);
	}
}



void SPlayerRotationParams::ReadAccelerationParams(const IItemParamsNode* pNode, SAimAccelerationParams* output)
{
	if (pNode)
	{
		pNode->GetAttribute("angle_min", output->angle_min);
		pNode->GetAttribute("angle_max", output->angle_max);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CPlayer::RegisterKill(IActor *pKilledActor, int hit_type)
{
}

float CPlayer::GetReloadSpeedScale() const
{
	return GetModifiableValues().GetValue(kPMV_WeaponReloadSpeedScale);
}

float CPlayer::GetOverchargeDamageScale( ) const
{
	return GetModifiableValues().GetValue(kPMV_WeaponOverchargeDamageMultiplier);
}

void CPlayer::OnReceivingLoadout()
{
}

void CPlayer::AddAmmoToInventory(IInventory* pInventory, IEntityClass* pAmmoClass, IWeapon* pWeapon, int totalCount, int totalCapacity, int increase)
{
	pInventory->SetAmmoCapacity(pAmmoClass, totalCapacity);
	pInventory->SetAmmoCount(pAmmoClass, totalCount);

	if (IsClient())
	{
		SHUDEvent eventPickUp(eHUDEvent_OnAmmoPickUp);
		eventPickUp.AddData(SHUDEventData((void*)pWeapon));
		eventPickUp.AddData(SHUDEventData(increase));
		eventPickUp.AddData(SHUDEventData((void*)pAmmoClass));
		CHUDEventDispatcher::CallEvent(eventPickUp);
	}
}

void CPlayer::UpdatePlayerPlugins(const float dt)
{
	for(int i = 0; i < m_numActivePlayerPlugins; ++i)
	{
		m_activePlayerPlugins[i]->Update(dt);
	}
}

int CPlayer::GetXPBonusModifiedXP(int baseXP) 
{
	const f32 fModifier = (((f32)m_xpBonusMultiplier)/100.f)+0.00001f; //Precision loss - slightly over not problematic, slightly under is
	const int iModified = (int)(((f32)baseXP * fModifier) + 0.5f);   //+0.5f means that we get rounding up if >= x.5f and rounding down if <x.5f
 	return iModified;
}

#undef raycast

bool CPlayer::SetActorModel(const char* modelName)
{
	if (m_pAnimatedCharacter)
	{
		m_pAnimatedCharacter->SetShadowCharacterSlot(GetShadowCharacterSlot());
	}

	bool hasChangedModel = CActor::SetActorModel(modelName);

	m_torsoAimIK.Reset();
	m_lookAim.Reset();

	//--- Update animationPlayerProxies & toggle the part visibility for separate character shadow casting
	m_animationProxy.SetFirstPerson(!IsThirdPerson());
	m_animationProxyUpper.SetFirstPerson(!IsThirdPerson());
	
	RefreshVisibilityState();

	ICharacterInstance *mainChar = GetEntity()->GetCharacter(0);
	if (mainChar)
	{
		//--- Initialise the weapon params cached model pointers
		m_weaponParams.skelAnim = mainChar->GetISkeletonAnim();
		m_weaponParams.characterInst = mainChar;
	}

	if (hasChangedModel && IsPlayer())
	{
		CRecordingSystem *pRecordingSystem = g_pGame->GetRecordingSystem();
		if (pRecordingSystem)
		{
			pRecordingSystem->OnPlayerChangedModel(this);
		}
	}

	return hasChangedModel;
}

void CPlayer::EnterPlayerPlugin(CPlayerPlugin * pluginClass)
{
	if(pluginClass)
	{
		bool bOk = true;

#if !defined(_RELEASE)
		if(IsClient())
			CryLog("[PLAYER PLUG-IN] %s '%s' attempting to enter plug-in '%s'", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), PLAYER_PLUGIN_DETAILS(pluginClass));
#endif

		if (pluginClass->IsEntered())
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[PLAYER PLUG-IN] %s '%s' tried to enter player plug-in '%s' which is already entered", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), PLAYER_PLUGIN_DETAILS(pluginClass));
			bOk = false;
		}
		else if (pluginClass->GetOwnerPlayer() == NULL)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[PLAYER PLUG-IN] %s '%s' tried to enter player plug-in '%s' with no owner set", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), PLAYER_PLUGIN_DETAILS(pluginClass));
			bOk = false;
		}
		else if (pluginClass->GetOwnerPlayer() != this)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[PLAYER PLUG-IN] %s '%s' tried to enter player plug-in '%s' assigned to different owner", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), PLAYER_PLUGIN_DETAILS(pluginClass));
			bOk = false;
		}
		
		CRY_ASSERT_TRACE (bOk, ("%s %s can't enter player plug-in class '%s'", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), PLAYER_PLUGIN_DETAILS(pluginClass)));

		if (bOk)
		{
			if (m_numActivePlayerPlugins < k_maxActivePlayerPlugIns)
			{
				CRY_ASSERT_MESSAGE(this == pluginClass->GetOwnerPlayer(), string().Format("Player '%s' shouldn't enter a player plug-in owned by player '%s'", GetEntity()->GetName(), pluginClass->GetOwnerPlayer() ? pluginClass->GetOwnerPlayer()->GetEntity()->GetName() : "NULL"));
				pluginClass->Enter(m_pPlayerPluginEventDistributor);

				CRY_ASSERT_MESSAGE(pluginClass->IsEntered(), "CPlayerPlugin was entered but IsEntered() is still returning false! Maybe the base class Enter function wasn't called from the subclass?");
				ASSERT_IS_NULL(m_activePlayerPlugins[m_numActivePlayerPlugins]);
				m_activePlayerPlugins[m_numActivePlayerPlugins] = pluginClass;
				m_numActivePlayerPlugins++;
			}
#if PLAYER_PLUGIN_DEBUGGING
			else
			{
				CryLog ("Can't enter plug-in \"%s\" because array of size %d is full:", pluginClass->DbgGetClassDetails().c_str(), k_maxActivePlayerPlugIns);
				for (int i = 0; i < k_maxActivePlayerPlugIns; ++ i)
				{
					CryLog ("    %d/%d: %s", i + 1, k_maxActivePlayerPlugIns, m_activePlayerPlugins[i]->DbgGetClassDetails().c_str());
				}
				CRY_ASSERT_MESSAGE(0, string().Format("Can't enter plug-in \"%s\" because array of size %d is full! Maybe you need to increase k_maxActivePlayerPlugIns...", pluginClass->DbgGetClassDetails().c_str(), k_maxActivePlayerPlugIns));
			}
#endif
		}
	}
}

void CPlayer::LeavePlayerPlugin(CPlayerPlugin * pluginClass)
{
	if(pluginClass)
	{
		bool bOk = true;

#if !defined(_RELEASE)
		if(IsClient())
			CryLog("[PLAYER PLUG-IN] %s '%s' attempting to leave plug-in '%s'", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), PLAYER_PLUGIN_DETAILS(pluginClass));
#endif

		if (! pluginClass->IsEntered())
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[PLAYER PLUG-IN] %s '%s' tried to leave player plug-in '%s' which isn't currently entered", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), PLAYER_PLUGIN_DETAILS(pluginClass));
			bOk = false;
		}
		else if (pluginClass->GetOwnerPlayer() == NULL)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[PLAYER PLUG-IN] %s '%s' tried to leave player plug-in '%s' with no owner set", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), PLAYER_PLUGIN_DETAILS(pluginClass));
			bOk = false;
		}
		else if (pluginClass->GetOwnerPlayer() != this)
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "[PLAYER PLUG-IN] %s '%s' tried to leave player plug-in '%s' assigned to different owner", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), PLAYER_PLUGIN_DETAILS(pluginClass));
			bOk = false;
		}

		CRY_ASSERT_TRACE (bOk, ("%s %s can't leave player plug-in class '%s'", GetEntity()->GetClass()->GetName(), GetEntity()->GetName(), PLAYER_PLUGIN_DETAILS(pluginClass)));

		if (bOk)
		{
			pluginClass->Leave();
			CRY_ASSERT_MESSAGE(!pluginClass->IsEntered(), "CPlayerPlugin was left but IsEntered() is still returning true! Maybe the base class Leave function wasn't called from the subclass?");

			for(int i = 0; i < m_numActivePlayerPlugins; i++)
			{
				if(m_activePlayerPlugins[i] == pluginClass)
				{
					m_numActivePlayerPlugins--;
					m_activePlayerPlugins[i] = m_activePlayerPlugins[m_numActivePlayerPlugins];
					m_activePlayerPlugins[m_numActivePlayerPlugins] = NULL;
					return;
				}
			}

			CRY_ASSERT_MESSAGE(false, string().Format("Player was told to leave plug-in %p which isn't in the list of active plug-ins!", pluginClass));
		}
	}
}

void CPlayer::LeaveAllPlayerPlugins()
{
	for(int i = 0; i < m_numActivePlayerPlugins; i++)
	{
		ASSERT_IS_NOT_NULL (m_activePlayerPlugins[i]);
		assert(m_activePlayerPlugins[i]->IsEntered());
		m_activePlayerPlugins[i]->Leave();
		CRY_ASSERT_MESSAGE(!m_activePlayerPlugins[i]->IsEntered(), "CPlayerPlugin was left but IsEntered() is still returning true! Maybe the base class Leave function wasn't called from the subclass?");
		m_activePlayerPlugins[i] = NULL;
	}

	m_numActivePlayerPlugins = 0;
}

void CPlayer::OnReturnedToPool()
{
	CActor::OnReturnedToPool();

	if (m_pHitDeathReactions)
		m_pHitDeathReactions->OnActorReturned();
}

void CPlayer::OnAIProxyEnabled(bool enabled)
{
	CActor::OnAIProxyEnabled(enabled);

	if (m_pHitDeathReactions)
	{
		if (enabled)
			m_pHitDeathReactions->RequestReactionAnims(eRRF_AIEnabled);
		else
			m_pHitDeathReactions->ReleaseReactionAnims(eRRF_AIEnabled);
	}
}

void CPlayer::Physicalize( EStance stance/*=STANCE_NULL*/ )
{
	bool bHidden = GetEntity()->IsHidden();
	if (bHidden)
		GetEntity()->Hide(false);

	SpawnCorpse();

	CActor::Physicalize(stance);

	if (m_pPickAndThrowProxy)
	{
		m_pPickAndThrowProxy->Physicalize();
	}

	//Disable physics hit reactions on client (only way it is setting skeleton mass to 0)
	if (IsClient())
	{
		ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
		IPhysicalEntity* pCharacterPhysics = pCharacter ? pCharacter->GetISkeletonPose()->GetCharacterPhysics() : NULL;
		if (pCharacterPhysics != NULL)
		{
			pe_simulation_params simParams;
			simParams.mass = 0.0f; 

			pCharacterPhysics->SetParams(&simParams);
		}
	}

	InitHitDeathReactions();

	if (bHidden)
		GetEntity()->Hide(true);
}

void CPlayer::UpdateMountedGunController( bool forceIKUpdate )
{
	const float frameTime = gEnv->pTimer->GetFrameTime();

  if (MountedGunControllerEnabled())
  {
	  m_mountedGunController.Update(m_stats.mountedWeaponID, frameTime);
  }
}

void CPlayer::StartFlashbangEffects(const float flashBangTime, const EntityId shooterId)
{
	if(!m_health.IsDead() && m_stats.spectatorInfo.mode == eASM_None)	//if you do a flashbang when you are dead it doesn't clear the effects properly
	{
		m_lastFlashbangShooterId = shooterId;
		m_lastFlashbangTime = gEnv->pTimer->GetCurrTime();
		CHANGED_NETWORK_STATE(this, ASPECT_FLASHBANG_SHOOTER_CLIENT);

		m_stats.flashBangStunLength = m_stats.flashBangStunTimer = flashBangTime;
		m_stats.flashBangStunMult = g_pGameCVars->g_flashBangMinSpeedMultiplier * GetModifiableValues().GetValue(kPMV_FlashBangStunMultiplier);

		StartTinnitus();

		SHUDEvent hudEvent_onBlind(eHUDEvent_OnBlind);
		hudEvent_onBlind.ReserveData(1);
		SHUDEventData dataAlpha(0.0f);
		hudEvent_onBlind.AddData(dataAlpha);
		CHUDEventDispatcher::CallEvent(hudEvent_onBlind);
	}
}

void CPlayer::StartTinnitus()
{
	if(!m_health.IsDead() && m_stats.spectatorInfo.mode == eASM_None)
	{
		CAudioSignalPlayer::JustPlay("FlashbangEnter");

		Vec3 zero = Vec3(0.0f, 0.0f, 0.0f);

		EntityId playerId = GetEntityId();
		//m_flashbangSignal.Play(playerId);
		m_flashbangSignal.SetParam(playerId, "effect", 1.0f);
	}
}

void CPlayer::UpdateFlashbangEffect(float frameTime)
{
	if (m_stats.flashBangStunTimer > 0.f)
	{
		m_stats.flashBangStunTimer = max(0.f, m_stats.flashBangStunTimer - frameTime);

		if (m_stats.flashBangStunTimer <= 0.f)
		{
			StopFlashbangEffects();
		}
		else
		{
			m_stats.flashBangStunMult = LERP(m_stats.flashBangStunMult, 1.f, min(1.f, (1.f/(m_stats.flashBangStunTimer*g_pGameCVars->g_flashBangSpeedMultiplierFallOffEase)) * frameTime)); 
			float stunFracRemaining = clamp_tpl(m_stats.flashBangStunTimer * __fres(m_stats.flashBangStunLength), 0.0f, 1.0f);

			UpdateTinnitus(stunFracRemaining);

			const float  stunFracBlindThresh = clamp_tpl((1.f - g_pGameCVars->g_blinding_flashbangRecoveryDelayFrac), 0.f, 1.f);

			float  blindFrac = 1.f;
			if (stunFracBlindThresh > 0.f && stunFracRemaining <= stunFracBlindThresh)
			{
				blindFrac = clamp_tpl((stunFracRemaining * __fres(stunFracBlindThresh)), 0.f, 1.f);
			}

			SHUDEvent hudEvent_onBlind(eHUDEvent_OnBlind);
			hudEvent_onBlind.ReserveData(1);
			SHUDEventData dataAlpha(1.f - blindFrac);
			hudEvent_onBlind.AddData(dataAlpha);
			CHUDEventDispatcher::CallEvent(hudEvent_onBlind);
		}
	}
}

void CPlayer::UpdateTinnitus(float tinnitusFraction)
{
	EntityId playerId = GetEntityId();
	m_flashbangSignal.SetParam(playerId, "effect", tinnitusFraction);
}

void CPlayer::StopFlashbangEffects()
{
	CHANGED_NETWORK_STATE(this, ASPECT_FLASHBANG_SHOOTER_CLIENT);

	m_stats.flashBangStunMult = 1.f;

	StopTinnitus();

	CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnEndBlind));
}

void CPlayer::StopTinnitus()
{
	//m_flashbangSignal.Stop(GetEntityId());
	CAudioSignalPlayer::JustPlay("FlashbangLeave");
}

void CPlayer::SetUpInventorySlotsAndCategories()
{
	//Use same config for now

	IInventory* pInventory = GetInventory();
	if (gEnv->bMultiplayer)
	{
		//Two slots for primary/secondary weapon
		pInventory->SetInventorySlotCapacity(IInventory::eInventorySlot_Weapon, g_pGameCVars->g_inventoryWeaponCapacity);
		pInventory->AssociateItemCategoryToSlot("primary", IInventory::eInventorySlot_Weapon);
		pInventory->AssociateItemCategoryToSlot("secondary", IInventory::eInventorySlot_Weapon);
		pInventory->AssociateItemCategoryToSlot("special", IInventory::eInventorySlot_Weapon);

		//Two slots for explosives grenades
		pInventory->SetInventorySlotCapacity(IInventory::eInventorySlot_Explosives, g_pGameCVars->g_inventoryExplosivesCapacity);
		pInventory->AssociateItemCategoryToSlot("grenade", IInventory::eInventorySlot_Explosives);
		pInventory->AssociateItemCategoryToSlot("explosive", IInventory::eInventorySlot_Explosives);
	}
	else
	{
		pInventory->SetInventorySlotCapacity(IInventory::eInventorySlot_Weapon, g_pGameCVars->g_inventoryWeaponCapacity);
		pInventory->AssociateItemCategoryToSlot("primary", IInventory::eInventorySlot_Weapon);
		pInventory->AssociateItemCategoryToSlot("secondary", IInventory::eInventorySlot_Weapon);

		pInventory->SetInventorySlotCapacity(IInventory::eInventorySlot_Explosives, g_pGameCVars->g_inventoryExplosivesCapacity);
		pInventory->AssociateItemCategoryToSlot("explosive", IInventory::eInventorySlot_Explosives);

		pInventory->SetInventorySlotCapacity(IInventory::eInventorySlot_Grenades, g_pGameCVars->g_inventoryGrenadesCapacity);
		pInventory->AssociateItemCategoryToSlot("grenade", IInventory::eInventorySlot_Grenades);

		pInventory->SetInventorySlotCapacity(IInventory::eInventorySlot_Special, g_pGameCVars->g_inventorySpecialCapacity);
		pInventory->AssociateItemCategoryToSlot("special", IInventory::eInventorySlot_Special);
	}

}

void CPlayer::BlendPartialCameraAnim(float target, float blendTime)
{
	if(blendTime > 0.f)
	{
		m_stats.partialCameraAnimTarget = target;
		m_stats.partialCameraAnimBlendRate = (target - m_stats.partialCameraAnimFactor) / blendTime;
	}
	else
	{
		m_stats.partialCameraAnimFactor = target;
		m_stats.partialCameraAnimTarget = 0.f;
		m_stats.partialCameraAnimBlendRate = 0.f;
	}
}

void CPlayer::UpdatePartialCameraAnim(float timeStep)
{
	if(m_stats.partialCameraAnimBlendRate != 0.f)
	{
		float newCameraAnimFactor = m_stats.partialCameraAnimFactor + (timeStep * m_stats.partialCameraAnimBlendRate);

		bool exceededTarget = ((m_stats.partialCameraAnimFactor > m_stats.partialCameraAnimTarget) != (newCameraAnimFactor > m_stats.partialCameraAnimTarget));

		m_stats.partialCameraAnimFactor = newCameraAnimFactor;

		if(exceededTarget) 
		{
			m_stats.partialCameraAnimBlendRate = 0.f;
			m_stats.partialCameraAnimFactor = m_stats.partialCameraAnimTarget;
		}
	}
}

bool CPlayer::DoSTAPAiming() const
{
	if(m_stats.forceSTAP==SPlayerStats::eFS_Off)
	{
		return false;
	}
	else if ((GetGameConstCVar(g_stapEnable) == 2) || (m_stats.forceSTAP==SPlayerStats::eFS_On))
	{
		return true;
	}
	else
	{
		return (GetGameConstCVar(g_stapEnable) != 0) && !IsThirdPerson() && (m_stats.partialCameraAnimFactor == 0.0f);
	}
}

void CPlayer::UpdateFPAiming()
{
	ICharacterInstance * pCharacter = GetEntity()->GetCharacter(0);
	CRY_ASSERT(pCharacter);
	if (pCharacter == 0)
	{
		return;
	}

	ISkeletonPose* pSkeleton = pCharacter->GetISkeletonPose();

	m_weaponParams.skelAnim = pCharacter->GetISkeletonAnim();
	m_weaponParams.characterInst = pCharacter;
	m_weaponParams.flags.ClearAllFlags();

	const EntityId currentItem = GetCurrentItemId(true);
	CItem * pItem = GetItem(currentItem);
	CWeapon* pWeapon = pItem ? static_cast<CWeapon *>(pItem->GetIWeapon()) : 0;
	const bool torsoAimIK	= DoSTAPAiming();
	const bool isSliding = IsSliding();
	const bool isThirdPerson = IsThirdPerson();
	const bool inVehicle = (GetLinkedVehicle() != NULL);
	
	bool enableWeaponAim = (GetStance() != STANCE_NULL) && (pWeapon != NULL) && (torsoAimIK || isThirdPerson) && !isSliding && !inVehicle;
	enableWeaponAim = enableWeaponAim && pWeapon->UpdateAimAnims(m_weaponParams) && !isThirdPerson;

	//Notice 'enableWeaponAim' takes priority over this one, so when using a MG on a vehicle torsoAimIK will be enabled
	const bool disableAimTorsoIK = (!torsoAimIK || inVehicle || IsOnLadder());
	const bool disableSnapAimTorsoIK = inVehicle;

	//const float XPOS = 200.0f, YPOS = 60.0f, FONT_SIZE = 2.0f, FONT_COLOUR[4] = {1,1,1,1};
	//IRenderAuxText::Draw2dLabel(XPOS, YPOS, FONT_SIZE, FONT_COLOUR, false, "UpdateFPAiming: %s w:%s", enableWeaponAim ? "update" : "dont update", pWeapon ? "Armed" : "UnArmed");

	if (enableWeaponAim)
	{
		SMovementState curMovementState;
		m_pMovementController->GetMovementState(curMovementState);

		Quat worldToLocal(IDENTITY);
		if (m_params.mountedWeaponCameraTarget.IsZero() && (GetLinkedVehicle() == NULL))
			worldToLocal = !m_pPlayerRotation->GetBaseQuat();
		else
			worldToLocal = !GetEntity()->GetWorldRotation();

		m_torsoAimIK.Enable();

		m_weaponParams.flags.AddFlags( (pWeapon->IsZoomed() || pWeapon->IsZoomingIn()) ? eWFPAF_zoomed : 0 );
		m_weaponParams.flags.AddFlags( HasHeavyWeaponEquipped() ? eWFPAF_heavyWeapon : 0 );
		m_weaponParams.flags.AddFlags( m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Ground ) ? eWFPAF_onGround : 0 );
		m_weaponParams.flags.AddFlags( m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Swimming ) ? eWFPAF_swimming : 0 );
		m_weaponParams.flags.AddFlags( m_stateMachineMovement.StateMachineActiveFlag( EPlayerStateFlags_Ledge ) ? eWFPAF_ledgeGrabbing : 0 );
		m_weaponParams.flags.AddFlags( (IsSprinting() && !pWeapon->IsReloading()) ? eWFPAF_sprinting : 0 );
		m_weaponParams.flags.AddFlags( (GetStance() == STANCE_CROUCH) ? eWFPAF_crouched : 0 );
		m_weaponParams.flags.SetFlags( eWFPAF_jump, IsJumping());
		m_weaponParams.flags.AddFlags( pWeapon->IsZoomStable() ? eWFPAF_aimStabilized : 0 );

		m_weaponParams.aimDirection = worldToLocal * curMovementState.aimDirection;
		m_weaponParams.velocity	= worldToLocal * (curMovementState.movementDirection * curMovementState.desiredSpeed);
		m_weaponParams.velocity.z = GetActorPhysics().velocity.z;
		m_weaponParams.position   = curMovementState.pos;
		m_weaponParams.groundDistance = fabs_tpl(gEnv->p3DEngine->GetTerrainElevation(curMovementState.pos.x, curMovementState.pos.y) - curMovementState.pos.z);
		m_weaponParams.runToSprintBlendTime = pWeapon->GetParams().runToSprintBlendTime;
		m_weaponParams.sprintToRunBlendTime = pWeapon->GetParams().sprintToRunBlendTime;
		m_weaponParams.zoomTransitionFactor = pWeapon->GetZoomTransition();
	}
	else
	{
		if (disableAimTorsoIK)
		{
			m_torsoAimIK.Disable(disableSnapAimTorsoIK);
		}
		else
		{
			m_torsoAimIK.Enable(!disableSnapAimTorsoIK);
		}
	}

	m_weaponParams.flags.AddFlags( IsThirdPerson() ? eWFPAF_thirdPerson : 0 );
	m_weaponFPAiming.SetActive(enableWeaponAim);
	m_weaponFPAiming.Update(m_weaponParams);
}

bool CPlayer::IsJumping() const
{
	return( m_stateMachineMovement.StateMachineAnyActiveFlag(EPlayerStateFlags_Jump) );
}

bool CPlayer::IsOnLadder() const
{
	return( m_stateMachineMovement.StateMachineActiveFlag(EPlayerStateFlags_OnLadder) );
}

const Vec3 CPlayer::GetFPCameraPosition(bool worldSpace) const
{
	Vec3 fpCameraPosition = m_params.mountedWeaponCameraTarget.IsZero() ? m_eyeOffset : m_params.mountedWeaponCameraTarget;
	if (worldSpace)
	{
		fpCameraPosition = GetEntity()->GetWorldPos() + (GetEntity()->GetWorldRotation() * fpCameraPosition);
	}

	return fpCameraPosition;
}

const Vec3 CPlayer::GetFPCameraOffset() const
{
	Vec3 frameOffset(0.0f, 0.0f, 0.0f);

	bool canOffset = m_linkStats.CanMoveCharacter() && !IsThirdPerson() && !m_stats.followCharacterHead && (!IsOnLedge()) && (!IsSliding());

	if (canOffset)
	{
		const SStanceInfo *stanceInfo = GetStanceInfo(m_stance);
		const float lookDown = m_pPlayerRotation->GetViewAngles().x * 2.0f / gf_PI;
		frameOffset.y += max(-lookDown,0.0f)*stanceInfo->viewDownYMod;

		const float zOffset = 0.075f;
		const float zCrouchLookDownOffset = 0.09f;

		switch(m_stance)
		{
		case STANCE_SWIM:
			{
				// Swim anims are centered so, we have to offset somewhat to
				// prevent intersections with geometry.
				const float commonOffset = lookDown * zOffset;

				// prevents intersection looking up.
				frameOffset.z -= commonOffset;

				// Prevents intersection at left and right edges when looking left and right.
				const float yOffset = 0.06f;
				frameOffset.y -= (yOffset+fabs_tpl(commonOffset)*2.0f);
			}
			break;
		case STANCE_CROUCH:
			frameOffset.z -= (float)__fsel(lookDown, lookDown*zOffset, lookDown*zCrouchLookDownOffset);
			break;
		}
	}

	return (frameOffset);
}

const QuatT& CPlayer::GetLastSTAPCameraDelta() const
{
	return m_pPlayerTypeComponent->GetLastSTAPCameraDelta();
}


void CPlayer::UpdateFPIKTorso(float fFrameTime, IItem * pCurrentItem, const Vec3& cameraPosition)
{
	CRY_ASSERT(IsClient());

	if (m_pPlayerTypeComponent)
	{
		m_pPlayerTypeComponent->UpdateFPIKTorso(fFrameTime, pCurrentItem, cameraPosition);
	}
}

void CPlayer::HasJumped(const Vec3 &jumpVel)
{
	CRY_ASSERT(!IsRemote());

	IPhysicalEntity* pEnt = GetEntity()->GetPhysics();

	pe_status_dynamics dynStat;
	pEnt->GetStatus(&dynStat);

	m_jumpCounter = (m_jumpCounter + 1)&(JUMP_COUNTER_MAX - 1);
	m_jumpVel			= jumpVel + dynStat.v;
	CHANGED_NETWORK_STATE(this, ASPECT_INPUT_CLIENT);
	CHANGED_NETWORK_STATE(this, ASPECT_JUMPING_CLIENT);	
}

uint8 CPlayer::GetJumpCounter() const
{
	return m_jumpCounter;
}

void CPlayer::SetJumpCounter(uint8 counter)
{
	if(IsRemote() && m_jumpCounter != counter)
	{
		m_jumpCounter = counter;

		CMovementRequest request;
		request.SetJump();
		GetMovementController()->RequestMovement(request);
	}
}

void CPlayer::HasClimbedLedge(const uint16 ledgeID, bool comingFromOnGround, bool comingFromSprint)
{
	const LedgeId newLedgeId(ledgeID);

	if (newLedgeId.IsValid())
	{
		m_ledgeCounter = (m_ledgeCounter + 1)&(JUMP_COUNTER_MAX - 1);
		m_ledgeID = newLedgeId;

		m_ledgeFlags = comingFromOnGround ? eLF_FROM_ON_GROUND : eLF_NONE;
		m_ledgeFlags |= comingFromSprint ? eLF_FROM_SPRINTING : eLF_NONE;

		CHANGED_NETWORK_STATE(this, ASPECT_LEDGEGRAB_CLIENT);	
	}
	else
	{
		CryLog("Not net-serialising a ledge grab as the id %d is not valid", ledgeID);
	}
}

uint8 CPlayer::GetLedgeCounter() const
{
	return m_ledgeCounter;
}

void CPlayer::SetLedgeCounter( uint8 counter)
{
	if(IsRemote() && m_ledgeCounter != counter)
	{
		m_ledgeCounter = counter;

		SLedgeTransitionData transitionData(LedgeId::invalid_id);

		SLedgeTransitionData::EOnLedgeTransition ledgeTransition = CPlayerStateLedge::GetBestTransitionToLedge( *this, GetEntity()->GetPos(), LedgeId(m_ledgeID), &transitionData);
		if(SLedgeTransitionData::eOLT_None == ledgeTransition)
		{
			ledgeTransition = SLedgeTransitionData::eOLT_MidAir;
		}

		CRY_ASSERT( LedgeId(m_ledgeID).IsValid() );

		transitionData.m_nearestGrabbableLedgeId = m_ledgeID;
		transitionData.m_ledgeTransition = ledgeTransition;
		transitionData.m_comingFromOnGround = (m_ledgeFlags & eLF_FROM_ON_GROUND) != 0;
		transitionData.m_comingFromSprint = (m_ledgeFlags & eLF_FROM_SPRINTING) != 0;

		SStateEventLedge ledgeEvent( transitionData );
		StateMachineHandleEventMovement( ledgeEvent );
	}
}

void CPlayer::OnUseLadder(EntityId ladderId, float heightFrac)
{
	CRY_ASSERT(ladderId != 0 && m_ladderId == 0);
	m_ladderId = ladderId;
	m_ladderHeightFrac = heightFrac;
	m_ladderHeightFracInterped = heightFrac;
	m_lastLadderLeaveLoc = eLLL_First;
	if (gEnv->bServer)
	{
		CHANGED_NETWORK_STATE(this, ASPECT_LADDER_SERVER);
	}
	else if (IsClient())
	{
		GetGameObject()->InvokeRMI(CPlayer::SvRequestUseLadder(), CPlayer::SRequestUseLadderParams(ladderId, heightFrac), eRMI_ToServer);
	}
}

void CPlayer::OnLeaveLadder(ELadderLeaveLocation leaveLocation)
{
	CRY_ASSERT(m_ladderId != 0);
	m_lastLadderLeaveLoc = leaveLocation;
	m_ladderId = 0;
	m_ladderHeightFrac = 0.0f;
	m_ladderHeightFracInterped = 0.0f;
	if (gEnv->bServer)
	{
		GetGameObject()->InvokeRMI(CPlayer::ClLeaveFromLadder(), SRequestLeaveLadderParams(leaveLocation), eRMI_ToOtherClients);
	}
	else if (IsClient())
	{
		GetGameObject()->InvokeRMI(CPlayer::SvRequestLeaveFromLadder(), SRequestLeaveLadderParams(leaveLocation), eRMI_ToServer);
	}

	if (IsThirdPerson())
	{
		m_torsoAimIK.Disable(true);
	}
	else
	{
		m_torsoAimIK.Enable(true);
	}
}

void CPlayer::OnLadderPositionUpdated(float ladderFrac)
{
	if (m_ladderHeightFrac != ladderFrac)
	{
		m_ladderHeightFrac = ladderFrac;
		if (gEnv->bServer || IsClient())
		{
			CHANGED_NETWORK_STATE(this, ASPECT_LADDER_SERVER);
		}
	}
}

void CPlayer::InterpLadderPosition(float frameTime)
{
	CRY_ASSERT(!IsClient());
	if (m_ladderId)
	{
		const float SmoothingConst = 8.0f;
		float delta = m_ladderHeightFrac - m_ladderHeightFracInterped;
		const float smoothing = approxOneExp(frameTime * SmoothingConst);
		m_ladderHeightFracInterped += delta * smoothing;
		SStateEventLadderPosition ladderEvent(m_ladderHeightFracInterped);
		StateMachineHandleEventMovement(ladderEvent);
	}
}


void CPlayer::TriggerMeleeReaction()
{
	CWeapon* weapon = GetWeapon(GetCurrentItemId());
	if (weapon)
	{
		weapon->TriggerMeleeReaction();
	}
}

struct CStowItem
{
	CStowItem(CPlayer *_player, EntityId _itemId, IAttachment *_pIAttachmentGrab, IAttachment *_pIAttachmentStow)
		: player(_player), itemId(_itemId), pIAttachmentGrab(_pIAttachmentGrab), pIAttachmentStow(_pIAttachmentStow) {};
	CPlayer *player;
	EntityId itemId;
	IAttachment *pIAttachmentGrab;
	IAttachment *pIAttachmentStow;

	void execute(CItem *_this)
	{
		if (pIAttachmentGrab)
		{
			pIAttachmentGrab->ClearBinding();
			_this->AttachToHand(false);
			_this->AttachToHand(true);
		}
		CEntityAttachment *pEntityAttachment = new CEntityAttachment();
		pEntityAttachment->SetEntityId(itemId);
		pIAttachmentStow->AddBinding(pEntityAttachment );
	}
};

void CPlayer::ReloadPickAndThrowProxy()
{
	const IItemParamsNode* pEntityClassParamsNode = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActorParams(GetEntityClassName());
	if (pEntityClassParamsNode)
	{
		const bool bPhysicalize = !m_pPickAndThrowProxy || m_pPickAndThrowProxy->IsActive();

		m_pPickAndThrowProxy = CPickAndThrowProxy::Create(this, pEntityClassParamsNode);
		if (m_pPickAndThrowProxy && bPhysicalize)
		{
			m_pPickAndThrowProxy->Physicalize();
		}
	}
	else if (m_pPickAndThrowProxy)
	{
		m_pPickAndThrowProxy.reset();
	}
}

void CPlayer::SetupAimIKProperties()
{
	ICharacterInstance* pCharacter = GetEntity()->GetCharacter(0);
	if (pCharacter)
	{
		ISkeletonPose* pSkeleton = pCharacter->GetISkeletonPose();
		IAnimationPoseBlenderDir* pIPoseBlenderAim = pSkeleton->GetIPoseBlenderAim();
		if (pIPoseBlenderAim)
		{
			pIPoseBlenderAim->SetFadeInSpeed(m_params.aimIKFadeDuration);
			pIPoseBlenderAim->SetFadeOutSpeed(m_params.aimIKFadeDuration);
	}

	}
}

void CPlayer::Reset( bool toGame )
{
	CActor::Reset(toGame);

	StateMachineResetMovement();
	SelectMovementHierarchy();

	SetLastTimeInLedge( 0.0f );

	SetupAimIKProperties();
	DisableStumbling();

	m_playerStateSwim_WaterTestProxy.Reset( true );

	GetSpectacularKill().Reset();
}

//////////////////////////////////////////////////////////////////////////
/*virtual*/ 
void CPlayer::RequestFacialExpression(const char* pExpressionName /* = NULL */, f32* sequenceLength/* = NULL*/)
{
	if (!m_pHitDeathReactions || !m_pHitDeathReactions->IsInReaction())
		CActor::RequestFacialExpression(pExpressionName, sequenceLength);
}

//////////////////////////////////////////////////////////////////////////
const QuatT& CPlayer::GetAnimationRelativeMovement(int slot /* = 0 */) const
{
	ICharacterInstance* pUserCharacter = GetEntity()->GetCharacter(slot);
	ISkeletonAnim* pSkeletonAnimation = pUserCharacter ? pUserCharacter->GetISkeletonAnim() : NULL;

	if (pSkeletonAnimation)
	{
		return pSkeletonAnimation->GetRelMovement();
	}
	else
	{
		static QuatT defaultRelativeMovement(IDENTITY, ZERO);
		return defaultRelativeMovement;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPlayer::GetPlayerProgressions(int* outXp, int* outRank, int* outDefault, int* outStealth, int* outArmour, int* outReincarnations  )
{
	m_netPlayerProgression.GetValues(outXp, outRank, outDefault, outStealth, outArmour, outReincarnations);
}

void CPlayer::StealthKillInterrupted(EntityId interruptorId)
{
	if(m_stealthKill.IsBusy())
	{
		if(m_stealthKill.GetTargetId() == interruptorId)
		{
			m_stealthKill.Abort();
		}
		else
		{
			m_stealthKill.Leave(m_stealthKill.GetTarget());
		}
	}
}

void CPlayer::NetSetInStealthKill(bool inKill, EntityId targetId, uint8 animIndex)
{
	if(!IsDead() && !IsClient())
	{
		bool currentlyInKill = m_stealthKill.IsBusy();

		IActor* pTargetActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(targetId);

		if(inKill && pTargetActor)
		{		
			CActor* pCurrentTarget = m_stealthKill.GetTarget();

			if(currentlyInKill)
			{
				if(pTargetActor == pCurrentTarget)
				{
					return;
				}

				m_stealthKill.Leave(pCurrentTarget);
			}

			if(!pTargetActor->IsDead())
			{
				if(m_stats.bStealthKilled)
				{
					UnRagdollize();
				}
				
				if(pTargetActor->IsPlayer())
				{
					CPlayer* pTargetPlayer = static_cast<CPlayer*>(pTargetActor);
					pTargetPlayer->StealthKillInterrupted(GetEntityId());
				}

				IEntity* pPlayerEntity = GetEntity();
				IEntity* pTargetEntity = pTargetActor->GetEntity();

				/*g_pGame->GetIGameFramework()->GetIPersistantDebug()->Begin("STEALTHKILL_NETPREKILL", false);
				g_pGame->GetIGameFramework()->GetIPersistantDebug()->AddSphere(pTargetEntity->GetWorldPos(), 0.2f, ColorF(1.f, 1.f, 0.f, 1.f), 120.f);
				g_pGame->GetIGameFramework()->GetIPersistantDebug()->AddLine(pTargetEntity->GetWorldPos(), pTargetEntity->GetWorldTM().GetColumn1() + pTargetEntity->GetWorldPos(), ColorF(1.f, 1.f, 0.f, 1.f), 120.f);
				g_pGame->GetIGameFramework()->GetIPersistantDebug()->AddSphere(pPlayerEntity->GetWorldPos(), 0.2f, ColorF(0.f, 1.f, 0.f, 1.f), 120.f);
				g_pGame->GetIGameFramework()->GetIPersistantDebug()->AddLine(pPlayerEntity->GetWorldPos(), pPlayerEntity->GetWorldTM().GetColumn1() + pPlayerEntity->GetWorldPos(), ColorF(0.f, 1.f, 0.f, 1.f), 120.f);*/

				m_stealthKill.Enter(targetId, animIndex);
			}
		}
	}
}

void CPlayer::StopStealthKillTargetMovement(EntityId playerId)
{
	//Stop moving the target player on their local machine
	IActor* pTargetActor = gEnv->pGameFramework->GetIActorSystem()->GetActor(playerId);
	if(pTargetActor)
	{
		CRY_ASSERT_MESSAGE(pTargetActor->GetActorClass() == CPlayer::GetActorClassType(), "CPlayer::NetSetInStealthKill - Expected player; got something else");

		CPlayer* pTargetPlayer = static_cast<CPlayer*>(pTargetActor);
		pTargetPlayer->BlockMovementInputsForTime(2.f);
	}
};

void CPlayer::CaughtInStealthKill(EntityId killerId)
{
	IInventory *pInventory = GetInventory();
	EntityId itemId = pInventory ? pInventory->GetCurrentItem() : 0;
	if (itemId)
	{
		CItem *pItem = GetItem(itemId);
		if (pItem && pItem->IsUsed())
			pItem->StopUse(GetEntityId());
		else if (pItem)
		{
			if(gEnv->bServer)
			{
				DropItem(itemId, 1.0f, false, false);
			}
			else
			{
				pItem->HideItem(true);
			}
		}
	}

	if(IsClient())
	{
		IEntity* pKillerEntity = gEnv->pEntitySystem->GetEntity(killerId);

		CRY_ASSERT_MESSAGE(pKillerEntity, "Invalid killer ID for stealth kill");
		if(pKillerEntity)
		{
			Vec3 killDirection = GetEntity()->GetWorldPos() - pKillerEntity->GetWorldPos();
			killDirection.Normalize();

			SHUDEvent hitEvent(eHUDEvent_OnShowHitIndicator);
			hitEvent.ReserveData(3);
			hitEvent.AddData(killDirection.x);
			hitEvent.AddData(killDirection.y);
			hitEvent.AddData(killDirection.z);
			CHUDEventDispatcher::CallEvent(hitEvent);
		}
		
		SetRecentKiller(killerId, CGameRules::EHitType::StealthKill);

		if(!IsThirdPerson())
		{
			ToggleThirdPerson();
		}
	}

	if(gEnv->bServer)
	{
		SetHealth(1);
	}

	SActorStats* pStats = GetActorStats();

	if(pStats)
	{
		pStats->bStealthKilled = true;
	}
}

void CPlayer::AttemptStealthKill(EntityId enemyEntityId)
{
	if(m_stealthKill.CanExecuteOn(static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(enemyEntityId)), false)==CStealthKill::CE_YES)
	{
		m_stealthKill.Enter(enemyEntityId);
	}
	else
	{
		m_stealthKill.SetTargetForAttackAttempt(enemyEntityId);
		m_stats.bAttemptingStealthKill = true;

		if(gEnv->bMultiplayer)
		{
			CHANGED_NETWORK_STATE(this, ASPECT_STEALTH_KILL);
		}
	}
}

void CPlayer::FailedStealthKill()
{
	m_stats.bAttemptingStealthKill = false;

	IItem* pItem = GetCurrentItem(false);
	IWeapon* pWeapon = pItem ? pItem->GetIWeapon() : NULL;

	if(pWeapon && pWeapon->CanMeleeAttack())
	{
		pWeapon->MeleeAttack();
	}
}

void CPlayer::StoreDelayedKillingHitInfo(HitInfo delayedHit)
{
	if(m_stealthKillDelayedHit.damage <= 0.f)
	{
		m_stealthKillDelayedHit = delayedHit;
#if USE_LAGOMETER
		m_stealthKillDelayedHit.lagOMeterHitId = 0;	// Ignore these delayed hits when measuring latency because they distort the results
#endif
	}
}

HitInfo& CPlayer::GetDelayedKillingHitInfo()
{
	return m_stealthKillDelayedHit;
}

//==========================================================================================

// Note: This can't be a normal constructor because when being constructed in the Player's initialisation list it needs to be passed the player's this pointer - which isn't allowed, because you're not allowed to pass a "this" in "this's" initialisation list :S
void SNetPlayerProgression::Construct(CPlayer* player)
{
	m_player = player;

	m_serVals.xp = 0;
	m_serVals.rank = -1;
	m_serVals.reincarnations = -1;
	m_serVals.defaultMode = -1;
	m_serVals.stealth = -1;
	m_serVals.armour = -1;
}

#if (USE_DEDICATED_INPUT)
// Give dummy players random progression values.
void SNetPlayerProgression::SetRandomValues()
{
	if (CPlayerProgression *pPlayerProgression = CPlayerProgression::GetInstance())
	{
		int maxRank = pPlayerProgression->GetData(EPP_MaxRank);
		if (maxRank == 0)
		{
			maxRank = 10;
		}
		m_serVals.rank = cry_random(1, maxRank);
		m_serVals.xp = pPlayerProgression->GetXPForRank(m_serVals.rank);
		m_serVals.reincarnations = std::max(cry_random(-10, 9), 0);	// make it more likely to be 0
	}
}
#endif

void SNetPlayerProgression::Serialize(TSerialize ser, EEntityAspects aspect)
{
	CRY_ASSERT(aspect == CPlayer::ASPECT_RANK_CLIENT);

	const bool  reading = ser.IsReading();

	NET_PROFILE_SCOPE("Ranks", reading);

#define PLAYER_PRBLOB_NUMBITS					(32)

#define PLAYER_PRBLOB_DEFPRO_SHIFT		(PLAYER_PRBLOB_NUMBITS - 5)								// 27
#define PLAYER_PRBLOB_STLPRO_SHIFT		(PLAYER_PRBLOB_DEFPRO_SHIFT - 5)					// 22
#define PLAYER_PRBLOB_ARMPRO_SHIFT		(PLAYER_PRBLOB_STLPRO_SHIFT - 5)					// 17
#define PLAYER_PRBLOB_RANK_SHIFT			(PLAYER_PRBLOB_ARMPRO_SHIFT - 7)					// 10
#define PLAYER_PRBLOB_REINCAR_SHIFT		(PLAYER_PRBLOB_RANK_SHIFT - 4)						// 6

#define PLAYER_PRBLOB_DEFPRO_MASK			((0x1f) << PLAYER_PRBLOB_DEFPRO_SHIFT)		// (0x1f)==00011111
#define PLAYER_PRBLOB_STLPRO_MASK			((0x1f) << PLAYER_PRBLOB_STLPRO_SHIFT)		// (0x1f)==00011111
#define PLAYER_PRBLOB_ARMPRO_MASK			((0x1f) << PLAYER_PRBLOB_ARMPRO_SHIFT)		// (0x1f)==00011111
#define PLAYER_PRBLOB_RANK_MASK				((0x7f) << PLAYER_PRBLOB_RANK_SHIFT)			// (0x7f)==01111111
#define PLAYER_PRBLOB_REINCAR_MASK		((0x0f) << PLAYER_PRBLOB_REINCAR_SHIFT)		// (0x0f)==00001111

	uint16  xp;
	uint32  blob;

	CRY_ASSERT((sizeof(blob) * 8) == PLAYER_PRBLOB_NUMBITS);

	if(!reading)
	{
		if (m_player->IsClient())
			SyncOnLocalPlayer(false);

		xp = m_serVals.xp;

		blob = 0;
		blob |= ((m_serVals.defaultMode + 1) << PLAYER_PRBLOB_DEFPRO_SHIFT);
		blob |= ((m_serVals.stealth + 1) << PLAYER_PRBLOB_STLPRO_SHIFT);
		blob |= ((m_serVals.armour + 1) << PLAYER_PRBLOB_ARMPRO_SHIFT);
		blob |= ((m_serVals.rank + 1) << PLAYER_PRBLOB_RANK_SHIFT);
		blob |= ((m_serVals.reincarnations + 1) << PLAYER_PRBLOB_REINCAR_SHIFT);

		//CryLog("[tlh] SNetPlayerProgression::Serialize: WRITING progressions: 0x%x, rank=%d, reincarnations=%d, stealth=%d, armour=%d, default=%d", blob, m_serVals.rank, m_serVals.reincarnations, m_serVals.stealth, m_serVals.armour, m_serVals.defaultMode);
	}

	ser.Value("progress_xp", xp, 'ui16');
	ser.Value("progressions", blob, 'i32');

	if(reading)
	{
		int  defaultMode =	(((blob & PLAYER_PRBLOB_DEFPRO_MASK) >> PLAYER_PRBLOB_DEFPRO_SHIFT) - 1);
		int  stealth =	(((blob & PLAYER_PRBLOB_STLPRO_MASK) >> PLAYER_PRBLOB_STLPRO_SHIFT) - 1);
		int  armour =		(((blob & PLAYER_PRBLOB_ARMPRO_MASK) >> PLAYER_PRBLOB_ARMPRO_SHIFT) - 1);
		int  rank =			(((blob & PLAYER_PRBLOB_RANK_MASK) >> PLAYER_PRBLOB_RANK_SHIFT) - 1);
		int  reincarnations = (((blob & PLAYER_PRBLOB_REINCAR_MASK) >> PLAYER_PRBLOB_REINCAR_SHIFT) - 1);

		//CryLog("[tlh] CPlayer::SNetPlayerProgression::Serialize: READING progressions: 0x%x, rank=%d, reincarnations=%d, stealth=%d, armour=%d, default=%d", blob, rank, reincarnations, stealth, armour, defaultMode);

		SetSerializedValues(xp, rank, defaultMode, stealth, armour, reincarnations);
	}
}

// Sets rank from player progression module for local player and serializes if it needs to
void SNetPlayerProgression::SyncOnLocalPlayer(const bool serialized/* = true*/)
{
	CRY_ASSERT(m_player->IsClient());

	CPlayerProgression*  pp = CPlayerProgression::GetInstance();
	CRY_ASSERT(pp);

	int  newVal;

	// serVals
	{
		bool  changeNetState = false;

		newVal = pp->GetData(EPP_XP);
		newVal = std::min((int)newVal, (int)0xffff);
		if (serialized && (m_serVals.xp != newVal))
			changeNetState = true;
		m_serVals.xp = newVal;

		newVal = pp->GetData(EPP_Rank);
		newVal = std::min((int)newVal, (int)0xff);
		if (serialized && (m_serVals.rank != newVal))
			changeNetState = true;
		m_serVals.rank = newVal;

		newVal = pp->GetData(EPP_Reincarnate);
		newVal = std::min((int)newVal, (int)0xff);
		if (serialized && (m_serVals.reincarnations != newVal))
			changeNetState = true;
		m_serVals.reincarnations = newVal;

		if (changeNetState)
		{
			CHANGED_NETWORK_STATE(m_player, CPlayer::ASPECT_RANK_CLIENT);
		}
	}
}

void SNetPlayerProgression::GetValues( int* outXp, int* outRank, int* outDefault, int* outStealth, int* outArmour, int* outReincarnations )
{
	if (m_player->IsClient())
		SyncOnLocalPlayer();

	if (outXp)
		(*outXp) = m_serVals.xp;
	if (outRank)
		(*outRank) = m_serVals.rank;
	if (outDefault)
		(*outDefault) = m_serVals.defaultMode;
	if (outStealth)
		(*outStealth) = m_serVals.stealth;
	if (outArmour)
		(*outArmour) = m_serVals.armour;
	if (outReincarnations)
		(*outReincarnations) = m_serVals.reincarnations;
	//
}

void SNetPlayerProgression::OwnClientConnected()
{
	CRY_ASSERT(m_player->IsClient());

	SyncOnLocalPlayer();  // Sets rank from playerprogression module for local player and serializes if it needs to
}

void SNetPlayerProgression::SetSerializedValues( int newXp, int newRank, int newDefault, int newStealth, int newArmour, int newReincarnations )
{
	if (!m_player->IsClient() && m_serVals.rank != newRank && m_serVals.rank != -1 && newRank != -1)
	{
		SHUDEvent promotionEvent(eHUDEvent_OnPlayerPromotion);
		promotionEvent.ReserveData(2);
		promotionEvent.AddData( static_cast<int>(m_player->GetEntityId()) );
		promotionEvent.AddData( newRank );
		CHUDEventDispatcher::CallEvent(promotionEvent);
	}

	m_serVals.xp = newXp;
	m_serVals.rank = newRank;
	m_serVals.reincarnations = newReincarnations;
	m_serVals.defaultMode = newDefault;
	m_serVals.stealth = newStealth;
	m_serVals.armour = newArmour;
}

void SXPEvents::SerializeWith(TSerialize ser)
{
	ser.Value("xpcount", numEvents, 'ui4');

	for (int i=0, m=numEvents; i<m; i++)
	{
		ser.Value("xpdelta", events[i].xpDelta, 'ui16');
		int reason=events[i].xpReason;
		ser.Value("xpreason", reason, 'ui8');
		events[i].xpReason=EXPReason(reason);
	}
}

void CPlayer::LogXPChangeToTelemetry(
	int				inXPDelta,
	EXPReason	inReason)
{
	if (gEnv->bMultiplayer && IsClient())
	{
		if (gEnv->bServer)
		{
			m_telemetry.OnXPChanged(inXPDelta,inReason);							// on the server, the client should record their own xp via telemetry
		}
		else																												// on a client, the change should be batched up and sent to the server for recording into the telemetry
		{
			if (m_netXPEvents.numEvents>=CRY_ARRAY_COUNT(m_netXPEvents.events))
			{
				// full? flush
				CheckSendXPChanges();
			}

			int		index;
			if (m_netXPEvents.numEvents<CRY_ARRAY_COUNT(m_netXPEvents.events))
			{
				index=m_netXPEvents.numEvents++;
			}
			else
			{
				index=CRY_ARRAY_COUNT(m_netXPEvents.events)-1;
			}

			m_netXPEvents.events[index].xpDelta=inXPDelta;
			m_netXPEvents.events[index].xpReason=inReason;
		}
	}
}

void CPlayer::CheckSendXPChanges()
{
	CTimeValue		now=gEnv->pTimer->GetFrameStartTime();

	if (m_netXPEvents.numEvents>0)
	{
		if ((now-m_netXPSendTime)>=g_pGameCVars->g_telemetry_xp_event_send_interval || m_netXPEvents.numEvents>=CRY_ARRAY_COUNT(m_netXPEvents.events))
		{
			GetGameObject()->InvokeRMI(SvOnXPChanged(),m_netXPEvents,eRMI_ToServer);
			m_netXPEvents.numEvents=0;
			m_netXPSendTime=now;
		}
	}
	else
	{
		m_netXPSendTime=now;
	}
}

IMPLEMENT_RMI(CPlayer, SvOnXPChanged)
{
	for (int i=0, m=params.numEvents; i<m; i++)
	{
		EXPReason reason = params.events[i].xpReason;
		int xpAwarded = params.events[i].xpDelta;

		m_telemetry.OnXPChanged(xpAwarded, reason);
	}

	return true;
}

//-------------------------------------------------------------------------

IMPLEMENT_RMI(CPlayer, ClDelayedDetonation)
{
	if(params.entityId)
	{
		CPersistantStats * pStats = CPersistantStats::GetInstance();
		if ( pStats )
		{
			pStats->ClientDelayedExplosion(params.entityId);
		}

		SHUDEvent eventDelay(eHUDEvent_OnExplosiveDetonationDelayed);
		eventDelay.AddData(SHUDEventData((int)params.entityId));
		CHUDEventDispatcher::CallEvent(eventDelay);

		CAudioSignalPlayer::JustPlay("DelayedDetonation_PerformedDelay");
	}

	return true;
}

//-------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, SvRequestStealthKill)
{
	EntityId mountedWeaponId = 0;
	CActor* pVictim = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(params.victimId));
	if (pVictim)
	{
		if(!IsDead() && !GetActorStats()->bStealthKilled && !pVictim->IsDead() && !pVictim->GetLinkedVehicle() && (!pVictim->IsPlayer() || !(static_cast<SPlayerStats*>(pVictim->GetActorStats())->bStealthKilled)))
		{
			NetSetInStealthKill(true, params.victimId, params.animIndex);
			return true;
		}

		if(IItem* pItem = pVictim->GetCurrentItem())
		{
			if(pItem->IsUsed())
			{
				mountedWeaponId = pItem->GetEntityId();
			}
		}
	}
	GetGameObject()->InvokeRMI(CPlayer::ClAbortStealthKill(), CPlayer::TwoEntityParams(params.victimId, mountedWeaponId), eRMI_ToClientChannel, GetChannelId());
	return true;
}

//-------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, ClAbortStealthKill)
{
	m_stealthKill.Abort(params.entityA_Id, params.entityB_Id);

	return true;
}

//-------------------------------------------------------------------------
void CPlayer::OnChangeTeam()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	int currentTeamId = pGameRules->GetTeam(this->GetEntityId());

	CryComment("[CG] CPlayer::OnChangeTeam() '%s' now on team '%i', channelId=%i", GetEntity()->GetName(), currentTeamId, GetChannelId());

	if (pGameLobby && IsPlayer() && currentTeamId && GetChannelId())
	{
		//SetMultiplayerModelName();

		if (!IsClient())
		{
			CryComment("    not local player");
			// Someone else has changed team
			IActor *pClientActor = g_pGame->GetIGameFramework()->GetClientActor();
			if (pClientActor)
			{
				int clientTeamId = pGameRules->GetTeam(pClientActor->GetEntityId());
				CryComment("        local player is on team %i", clientTeamId);
				if (clientTeamId)
				{
					if (clientTeamId != currentTeamId)
					{
						pGameLobby->MutePlayerByChannelId(this->GetChannelId(), true, SSessionNames::SSessionName::MUTE_REASON_WRONG_TEAM);
					}
					else
					{
						pGameLobby->MutePlayerByChannelId(this->GetChannelId(), false, SSessionNames::SSessionName::MUTE_REASON_WRONG_TEAM);
					}
				}
			}
		}
		else
		{
			CryComment("    is local player");
			OnLocalPlayerChangeTeam();
		}
	}
}

//-------------------------------------------------------------------------
void CPlayer::OnLocalPlayerChangeTeam()
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	const bool bDeferCloakColourSwitch = (gEnv->bMultiplayer && IsDead() && pGameRules->GetGameMode()==eGM_Gladiator);

	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	int clientTeamId = pGameRules->GetTeam(GetEntityId());
	if (clientTeamId && pGameLobby)
	{
		// We've changed team, need to flip the muting on all players
		IActorIteratorPtr pActorIterator = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
		while (IActor* pActor = pActorIterator->Next())
		{
			if (pActor->IsPlayer() && pActor->GetChannelId())
			{
				int teamId = pGameRules->GetTeam(pActor->GetEntityId());
				const bool mutePlayer = (clientTeamId != teamId);
				pGameLobby->MutePlayerByChannelId(pActor->GetChannelId(), mutePlayer, SSessionNames::SSessionName::MUTE_REASON_WRONG_TEAM);
			}

			if(!bDeferCloakColourSwitch)
			{
				// When changing team, any cloaked players need the cloak effect refreshing so it's using the correct friendly/ememy values
				CActor *pActorProp = static_cast<CActor *>(pActor);
				if (this!=pActorProp && pActorProp && pActorProp->IsCloaked())
				{
					pActorProp->SetCloakLayer(false);
					pActorProp->SetCloakLayer(true);
				}
			}
		}
	}

// 	CMPMenuHub *mpMenuHub = g_pGame->GetFlashMenu() ? g_pGame->GetFlashMenu()->GetMPMenu() : NULL;
// 	if (mpMenuHub)
// 	{
// 		mpMenuHub->OnLocalPlayerChangedTeam(clientTeamId);
// 	}
}

//////////////////////////////////////////////////////////////////////////
/// These functions are called for cut-scenes flagged with 'NO_PLAYER'
//////////////////////////////////////////////////////////////////////////

void CPlayer::OnBeginCutScene()
{
	if (gEnv->IsEditor() && gEnv->IsEditing())
		return;

	SetHealth( GetMaxHealth() );
	GetEntity()->Hide(true);

	//Let the item decide what to do with itself during the cut-scene
	CItem* pCurrentItem = static_cast<CItem*>(GetCurrentItem());
	if (pCurrentItem)
	{
		pCurrentItem->OnBeginCutScene();
	}

	m_playerHealthEffect.Stop();
	
	//Reset input (there might be movement deltas and so on left)
	IPlayerInput* pPlayerInput = GetPlayerInput();
	if(pPlayerInput)
	{
		pPlayerInput->Reset();
	}

	CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_HideMouseWheel));
}

void CPlayer::OnEndCutScene()
{
	if (gEnv->IsEditor() && gEnv->IsEditing())
		return;

	GetEntity()->Hide(false);

	// Luciano: skip item holster and reset if loading during a cutscene
	// (OnEndCutScene should not be called during save)
	if(gEnv->pSystem->IsSerializingFile()==0) 
	{
		CItem* pCurrentItem = static_cast<CItem*>(GetCurrentItem());
		if (pCurrentItem)
		{
			pCurrentItem->OnEndCutScene();
		}
		else if (GetHolsteredItem() != NULL)
		{
			HolsterItem(false);
		}
	}

	IGameRulesObjectivesModule * objectives = g_pGame->GetGameRules()->GetObjectivesModule();
	if(!gEnv->bMultiplayer
		|| !objectives
		|| objectives->MustShowHealthEffect(GetEntityId()))
	{
		m_playerHealthEffect.Start();
	}

	SetViewRotation( Quat::CreateRotationVDir( gEnv->pSystem->GetViewCamera().GetViewdir() ) );
}

void CPlayer::UpdatePlayerCinematicStatus(uint8 oldFlags, uint8 newFlags)
{
	if(oldFlags == 0 && newFlags==0)
		return;

	if(oldFlags != 0 && newFlags==0)
	{
		ResetCinematicStatus(oldFlags);
		return;
	}

	bool dropPickupItem = false;
	if ((newFlags & SPlayerStats::eCinematicFlag_HolsterWeapon) != 0)
	{
		if ((oldFlags & SPlayerStats::eCinematicFlag_HolsterWeapon) == 0)
		{
			IInventory* pInventory = GetInventory();
			if (pInventory && !pInventory->GetHolsteredItem())  // inventory does not support "pile holstering". So, if there is an item already holstered, we just dont do it (this can happen if the cinematic is triggered when the player has the visor enabled and he had a normal weapon equiped before)
				pInventory->SetHolsteredItem(GetCurrentItemId());
			SelectItemByName("NoWeapon", true);
		}
		dropPickupItem = true;
	}
	if ((newFlags & SPlayerStats::eCinematicFlag_LowerWeapon) !=0 ||
			(newFlags & SPlayerStats::eCinematicFlag_LowerWeaponMP) != 0 )
	{
		//Allow to go to lower main weapon, after being holstered
		if ((oldFlags & SPlayerStats::eCinematicFlag_HolsterWeapon) != 0)
		{
			IItem* pCurrentItem = GetCurrentItem();
			if (pCurrentItem)
			{
				if (pCurrentItem->GetEntity()->GetClass() == CItem::sNoWeaponClass)
				{
					SelectLastItem(false, true);
				}
			}		
		}
		dropPickupItem = true;
	}
	if( dropPickupItem && m_stats.pickAndThrowEntity != 0 )
	{
		//Drop object
		GetCurrentItem()->Select(false);
		ExitPickAndThrow();
	}
}

void CPlayer::SetCinematicFlag(SPlayerStats::ECinematicFlags flag)
{
	UpdatePlayerCinematicStatus(m_stats.cinematicFlags,flag );
	m_stats.cinematicFlags |= flag;

	//Lower/Holster are exclusive
	if (flag == SPlayerStats::eCinematicFlag_LowerWeapon || flag == SPlayerStats::eCinematicFlag_LowerWeaponMP)
	{
		m_stats.cinematicFlags &= ~SPlayerStats::eCinematicFlag_HolsterWeapon;
	}
	if (flag == SPlayerStats::eCinematicFlag_HolsterWeapon)
	{
		m_stats.cinematicFlags &= ~(SPlayerStats::eCinematicFlag_LowerWeapon|SPlayerStats::eCinematicFlag_LowerWeaponMP);
	}
}

void CPlayer::ResetCinematicFlags()
{
	ResetCinematicStatus(m_stats.cinematicFlags);
	m_stats.cinematicFlags = 0;
}

void CPlayer::ResetCinematicStatus(uint8 oldFlags)
{
	if ((oldFlags & SPlayerStats::eCinematicFlag_HolsterWeapon) != 0)
	{
		HolsterItem(false, true);
	}

	EnableSwitchingItems(true);

}

bool CPlayer::CanFall() const
{
	const bool bCanFall = !m_isPlayer && g_pGameCVars->pl_health.enable_FallandPlay && !IsFallen() && GetLinkedVehicle()==NULL;
	if( !bCanFall )
	{
		return false;
	}

	SmartScriptTable props;
	SmartScriptTable propsDamage;
	if(GetEntity()->GetScriptTable() && GetEntity()->GetScriptTable()->GetValue("Properties", props))
		if(!props->GetValue("Damage", propsDamage))
			return false;

	int canFall(0);
	if( !propsDamage->GetValue("CanFall", canFall) || (canFall == 0) )
	{
		return false;
	}

	return !IsPlayingSmartObjectAction();
}

void CPlayer::KnockDown(float backwardsImpulse)
{
	m_deferredKnockDownPending = true;
	m_deferredKnockDownImpulse = std::max(m_deferredKnockDownImpulse, backwardsImpulse);
}

void CPlayer::SetLookAtTargetId( EntityId targetId, float interpolationTime )
{
	if (m_pLocalPlayerInteractionPlugin)
	{
		m_pLocalPlayerInteractionPlugin->SetLookAtTargetId(targetId);
		m_pLocalPlayerInteractionPlugin->SetLookAtInterpolationTime(interpolationTime);
	}
}

void CPlayer::SetForceLookAtTargetId( EntityId targetId, float interpolationTime )
{
	if (m_pLocalPlayerInteractionPlugin)
	{
		m_pLocalPlayerInteractionPlugin->EnableForceLookAt();
		m_pLocalPlayerInteractionPlugin->SetLookAtTargetId(targetId);
		m_pLocalPlayerInteractionPlugin->SetLookAtInterpolationTime(interpolationTime);
	}
}

const SInteractionInfo& CPlayer::GetCurrentInteractionInfo() const
{
	static SInteractionInfo defaultInteractionInfo;

	return m_pLocalPlayerInteractionPlugin ? m_pLocalPlayerInteractionPlugin->GetCurrentInteractionInfo(): defaultInteractionInfo;
}

void CPlayer::ResetInteractor()
{
	EntityId lockedId = m_pInteractor ? m_pInteractor->GetLockedEntityId() : 0;
	if (lockedId)
	{
		LockInteractor(lockedId, false);
	}

	if (m_pLocalPlayerInteractionPlugin)
	{
		m_pLocalPlayerInteractionPlugin->Reset();
	}
}

EntityId CPlayer::GetInteractingEntityId() const
{
	return m_stats.animationControlledID;
}

float CPlayer::GetTimeEnteredLowHealth() const
{ 
	return m_pPlayerTypeComponent ? m_pPlayerTypeComponent->GetTimeEnteredLowHealth() : 0.0f;
}

void CPlayer::AddHeatPulse( const float intensity, const float time )
{
	m_heatController.AddHeatPulse(intensity, time);
}

float CPlayer::GetSprintStaminaLevel() const
{
	return (m_pSprintStamina != NULL) ? m_pSprintStamina->Get() : 1.0f;
}

void CPlayer::UpdateFrameMovementModifiersAndWeaponStats(CWeapon* pWeapon, float currentTime)
{
	float totalSpeedMultiplier = (IsCinematicFlagActive(SPlayerStats::eCinematicFlag_WalkOnly) == false) ? 1.0f : 0.5f;

	for (int smr = 0; smr < SActorParams::eSMR_COUNT; ++smr)
	{
		totalSpeedMultiplier *= m_params.speedMultiplier[smr];
	}

	//Do we care about any of this for remote clients or AI? - Rich S
	if (pWeapon)
	{
		bool isUsingMouse = false;

		IPlayerInput* pIPlayerInput = GetPlayerInput();
		if (pIPlayerInput && (pIPlayerInput->GetType() == IPlayerInput::PLAYER_INPUT))
		{
			isUsingMouse = (static_cast<CPlayerInput*>(pIPlayerInput)->IsAimingWithMouse());
		}

		const float globalRotationModifier = (1.0f - m_params.viewFoVScale) * g_pGameCVars->g_fovToRotationSpeedInfluence;  
		float rotationModifier = pWeapon->GetRotationModifier(isUsingMouse) * clamp_tpl(1.0f - globalRotationModifier, 0.0f , 1.0f);

		m_frameMovementModifiers.SetFrameModfiers(pWeapon->GetMovementModifier(), rotationModifier, totalSpeedMultiplier);

		if (pWeapon->IsZoomed())
		{
			m_lastZoomedTime = currentTime;
		}
	}
	else
	{
		m_frameMovementModifiers.SetFrameModfiers(1.0f, 1.0f, totalSpeedMultiplier);
	}
}

void CPlayer::CommitKnockDown()
{
	const float backwardsImpulse = m_deferredKnockDownImpulse;

	m_deferredKnockDownPending = false;
	m_deferredKnockDownImpulse = 0.0f;

	if (m_stats.animationControlledID != 0)
		return;

	const bool dropHeavyWeapon = false;
	SetBackToNormalWeapon(dropHeavyWeapon);

	float knowDownSpeed = 1.0f;
	StartInteractiveActionByName("KnockDown", false, knowDownSpeed);

	{
		AABB bbox;
		GetEntity()->GetPhysicsWorldBounds(bbox);
		if (!bbox.IsEmpty())
		{
			const Vec3 impulse = (m_pPlayerRotation->GetBaseQuat().GetColumn1() + Vec3(0.0f, 0.0f, 0.15f)).GetNormalized() * -backwardsImpulse;

			GetEntity()->AddImpulse(-1, bbox.GetCenter(), impulse, true, 1.0f);
		}
	}
}

void CPlayer::SetBackToNormalWeapon(const bool dropHeavyWeapon)
{
	CWeapon* pCurrentWeapon = GetWeapon(GetCurrentItemId());
	SetBackToNormalWeapon(pCurrentWeapon, dropHeavyWeapon);
}

void CPlayer::SetBackToNormalWeapon(CWeapon* pCurrentWeapon, const bool dropHeavyWeapon)
{
	if (pCurrentWeapon)
	{
		if(pCurrentWeapon->IsHeavyWeapon())
		{
			if(dropHeavyWeapon)
			{
				pCurrentWeapon->StopUse(GetEntityId());
			}
		}
		else if (m_stats.pickAndThrowEntity != 0)
		{
			//Drop object
			pCurrentWeapon->Select(false);
			ExitPickAndThrow();
		}
	}
}

void CPlayer::EnableStumbling(PlayerActor::Stumble::StumbleParameters* stumbleParameters)
{
	//EnableStumbling is only ever called on a local player. If a local player does not have a player type
	//	component, this will crash. Which is fine, because something has gone wrong.
	m_pPlayerTypeComponent->EnableStumbling(stumbleParameters);
}

void CPlayer::DisableStumbling()
{
	//DisableStumbling is blindly called in the CPlayer::Init() function, regardless of whether
	//	the player is local or not. Therefore we need to check for player type component validity
	if(m_pPlayerTypeComponent)
		m_pPlayerTypeComponent->DisableStumbling();
}

// This is a candidate for being server only - whenever a client calls it
// its just going to end up out of sync with the server, and the state 
// is server controlled with respect to its serialisation
void CPlayer::SetSpectatorState( uint8 state )
{
	if (gEnv->bServer && IsClient())
	{
		OnLocalSpectatorStateSerialize((CActor::EActorSpectatorState)state, (CActor::EActorSpectatorState)m_stats.spectatorInfo.state);
	}

	if (m_stats.spectatorInfo.state != state)
	{
		m_stats.spectatorInfo.state = state;
		CHANGED_NETWORK_STATE(this, ASPECT_SPECTATOR);
	}
}

void CPlayer::InitHitDeathReactions()
{
	if (!m_pHitDeathReactions && !IsPoolEntity())
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "HitDeathReactions_Instances");
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, GetEntity()->GetName());

		m_pHitDeathReactions.reset(new CHitDeathReactions(*this));
		CRY_ASSERT(m_pHitDeathReactions.get());
	}
}

void CPlayer::RefillAmmo()
{
	if(gEnv->bMultiplayer)
	{
		CWeapon* pCurrentWeapon = GetWeapon(GetCurrentItemId());
		if(pCurrentWeapon && pCurrentWeapon->OutOfAmmo(false))
		{
			// If we have no ammo in our clip and we have ammo supplies available.. reload!
			pCurrentWeapon->Reload(true);
			return;
		}
	}

	// does this require LowerWeaponMP handling
	if (!IsCinematicFlagActive(SPlayerStats::eCinematicFlag_LowerWeapon))
	{
		SetCinematicFlag(SPlayerStats::eCinematicFlag_LowerWeapon);

		// Note: 'REFILL_AMMO_TIMER_ID' Just controls how long the weapon is offscreen for, before being raised again,
		// it DOES NOT actually trigger any assignment of ammo to a client
		GetEntity()->SetTimer(REFILL_AMMO_TIMER_ID, (int)(g_pGameCVars->pl_refillAmmoDelay * 1000.0f));
	}
}

void CPlayer::RefillAmmoDone()
{
	const uint8 newFlags = m_stats.cinematicFlags &(~SPlayerStats::eCinematicFlag_LowerWeapon);

	ResetCinematicFlags(); //Reset all first

	//Re-stablish whatever was there before without 'lower weapon' (probably nothing, but just in case)
	UpdatePlayerCinematicStatus(m_stats.cinematicFlags, newFlags); 
	m_stats.cinematicFlags = newFlags;
}

void CPlayer::UpdateClient( const float frameTime )
{
	CRY_ASSERT(IsClient());

	if (m_pPlayerTypeComponent)
	{
		m_pPlayerTypeComponent->Update(frameTime);
	}
}

float CPlayer::GetBaseHeat() const
{
	return m_thermalVisionBaseHeat;
}

const float CPlayer::GetCloakBlendSpeedScale()
{
	return g_pGameCVars->g_cloakBlendSpeedScale;
}

void CPlayer::SetCloakLayer( bool set , eFadeRules config /*= eAllowFades*/)
{
	CActor::SetCloakLayer(set, config);
}

void CPlayer::OnMeleeHit( const SMeleeHitParams &params )
{
	m_meleeHitCounter = (m_meleeHitCounter + 1) & (0x3);
	m_pPlayerTypeComponent->SetLastMeleeParams(params);
		
	CHANGED_NETWORK_STATE(this, ASPECT_LAST_MELEE_HIT);

	DoMeleeMaterialEffect(params);
}

void CPlayer::DoMeleeMaterialEffect(const SMeleeHitParams& rHitParams)
{
	IEntity *pTarget = gEnv->pEntitySystem->GetEntity(rHitParams.m_targetId);
	if (pTarget)
	{
		Vec3 hitPos = pTarget->GetWorldPos() + rHitParams.m_hitOffset;
		CMelee::PlayHitMaterialEffect(hitPos, rHitParams.m_hitNormal, rHitParams.m_boostedMelee, rHitParams.m_surfaceIdx);
	}
}

//////////////////////////////////////////////////////////////////////////

void SDeferredFootstepImpulse::DoCollisionTest( const Vec3 &startPos, const Vec3 &dir, float distance, float impulseAmount, IPhysicalEntity* pSkipEntity )
{
	//Only queue if there is not other one pending
	if (m_queuedRayId == 0)
	{
		m_queuedRayId = g_pGame->GetRayCaster().Queue(
			RayCastRequest::MediumPriority,
			RayCastRequest(startPos, dir * 0.25f,
			ent_sleeping_rigid|ent_rigid,
			(geom_colltype_ray|geom_colltype13)<<rwi_colltype_bit|rwi_colltype_any|rwi_force_pierceable_noncoll|rwi_ignore_solid_back_faces|8,
			&pSkipEntity,
			pSkipEntity ? 1 : 0),
			functor(*this, &SDeferredFootstepImpulse::OnRayCastDataReceived));

		m_impulseAmount = dir * impulseAmount;
	}
}

void SDeferredFootstepImpulse::OnRayCastDataReceived( const QueuedRayID& rayID, const RayCastResult& result )
{
	CRY_ASSERT(rayID == m_queuedRayId);

	if((result.hitCount > 0) && (result.hits[0].pCollider))
	{
		pe_action_impulse impulse;
		impulse.point = result.hits[0].pt;
		impulse.impulse = m_impulseAmount;
		impulse.partid = result.hits[0].partid;
		result.hits[0].pCollider->Action(&impulse, true);
	}

	m_impulseAmount.zero();
	m_queuedRayId = 0;
}

void SDeferredFootstepImpulse::CancelPendingRay()
{
	if (m_queuedRayId != 0)
	{
		g_pGame->GetRayCaster().Cancel(m_queuedRayId);
	}
	m_queuedRayId = 0;
}
//////////////////////////////////////////////////////////////////////////

void CPlayer::SaveToProfile(IPlayerProfile* pProfile, bool online, unsigned int reason)
{
}

void CPlayer::LoadFromProfile(IPlayerProfile* pProfile, bool online, unsigned int reason)
{
}

void CPlayer::SerializeLevelToLevel( TSerialize &ser )
{
	CActor::SerializeLevelToLevel(ser);
}

void CPlayer::HandleMPPreRevive()
{
	CRY_ASSERT( gEnv->bMultiplayer );

	//set model based on race and team
	SetMultiplayerModelName();
}

//----------------------------------------------------------------------
void CPlayer::PrepareLuaCache()
{
	LoadoutScriptModelData Data = ScriptModelData[ 0 ];
	if( Data.m_ScriptClassOveride != NULL )
	{
		const CGameCache &gameCache = g_pGame->GetGameCache();

		IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( Data.m_ScriptClassOveride );

		m_LuaCache_PhysicsParams.reset(gameCache.GetActorPhysicsParams(pClass));
		m_LuaCache_GameParams.reset(gameCache.GetActorGameParams(pClass));
		m_LuaCache_Properties.reset(gameCache.GetActorProperties(GetEntityId()));
	}
	else
	{
		//use default
		CActor::PrepareLuaCache();
	}
	
}

void CPlayer::SetMultiplayerModelName()
{
	CRY_ASSERT( gEnv->bMultiplayer );
	
	CGameRules *pGameRules = g_pGame->GetGameRules();
	int teamId = m_teamId;
	int teamDiff = g_pGameCVars->g_teamDifferentiation;
	
	IEntity *pEntity = GetEntity();
	SmartScriptTable propertiesTable;
	if( pEntity->GetScriptTable()->GetValue( "Properties", propertiesTable ) )
	{
		const char* pModelName;

		if (m_mpModelIndex != MP_MODEL_INDEX_DEFAULT)
		{
			CEquipmentLoadout *pLoadout = g_pGame->GetEquipmentLoadout();
			pModelName = pLoadout->GetModelName(m_mpModelIndex);
		}
		else
		{
			LoadoutScriptModelData ModelData = ScriptModelData[ 0 ];

			if( teamDiff != 0 && teamId == 2 )
			{
				pModelName = ModelData.m_Model2Name;
			}
			else
			{
				pModelName = ModelData.m_Model1Name;
			}
		}

		propertiesTable->SetValue( "fileModel", pModelName );
		propertiesTable->SetValue( "shadowFileModel", pModelName );
		propertiesTable->SetValue( "clientFileModel", pModelName );
	}


	//	ideally we should only do this if we change
	const bool bUpdatedActorModel = FullyUpdateActorModel();
	if(bUpdatedActorModel)
	{
		CTeamVisualizationManager* pTeamVisManager = g_pGame->GetGameRules()->GetTeamVisualizationManager();
		if(pTeamVisManager)
		{
			pTeamVisManager->RefreshPlayerTeamMaterial(pEntity->GetId());
		}
	}

	CRecordingSystem* pRecordingSystem = g_pGame->GetRecordingSystem();
	if( pRecordingSystem )
	{
		pRecordingSystem->OnSetTeam( GetEntityId(), teamId );
	}

}

CPlayerEntityInteraction& CPlayer::GetPlayerEntityInteration()
{
	return m_pPlayerTypeComponent->GetPlayerEntityInteraction();
}

const SFollowCameraSettings& CPlayer::GetCurrentFollowCameraSettings() const
{
	CRY_ASSERT_MESSAGE(m_pPlayerTypeComponent, "CPlayer::GetCurrentFollowCameraSettings() - Localplayercomponent not found. This function should only be called on the client player");

	return m_pPlayerTypeComponent->GetCurrentFollowCameraSettings();
}

void CPlayer::ChangeCurrentFollowCameraSettings(bool increment)
{
	CRY_ASSERT_MESSAGE(m_pPlayerTypeComponent, "CPlayer::ChangeCurrentFollowCameraSettings() - Localplayercomponent not found. This function should only be called on the client player");

	return m_pPlayerTypeComponent->ChangeCurrentFollowCameraSettings(increment);
}

bool CPlayer::SetCurrentFollowCameraSettings(uint32 crcName)
{
	CRY_ASSERT_MESSAGE(m_pPlayerTypeComponent, "CPlayer::SetCurrentFollowCameraSettings() - Localplayercomponent not found. This function should only be called on the client player");

	return m_pPlayerTypeComponent->SetCurrentFollowCameraSettings(crcName);
}

void CPlayer::TriggerLoadoutGroupChange( CEquipmentLoadout::EEquipmentPackageGroup group, bool forOneLifeOnly )
{
	//this will only work for the local player
	if( IsClient() )
	{
		CGameRules* pGameRules = g_pGame->GetGameRules();

		//send command to server to force a respawn(by setting spectator state)
		if( gEnv->bServer )
		{
			IGameRulesSpectatorModule *pSpectatorModule = pGameRules->GetSpectatorModule();
			if (pSpectatorModule)
			{
				pSpectatorModule->ChangeSpectatorMode( this ,CActor::eASM_Free, 0, false );
			}

			SetSpectatorState(CActor::eASS_ForcedEquipmentChange);
		}
		else
		{
			CGameRules::ServerSpectatorParams params;
			params.entityId = GetEntityId();
			params.state = CActor::eASS_ForcedEquipmentChange;
			params.mode = CActor::eASM_Free;

			pGameRules->GetGameObject()->InvokeRMI(CGameRules::SvSetSpectatorState(), params, eRMI_ToServer);
		}

		// and change loadout group
		{
			CEquipmentLoadout *pEquipmentLoadout = g_pGame->GetEquipmentLoadout();

			//if we want to change back on death, remember what we should change back to
			//but ignore this if we already have a pending change
			if( forOneLifeOnly && m_pendingLoadoutGroup == -1 )
			{
				m_pendingLoadoutGroup = pEquipmentLoadout->GetPackageGroup();
			}

			CRY_ASSERT(pEquipmentLoadout);
			if (pEquipmentLoadout)
			{
				pEquipmentLoadout->InvalidateLastSentLoadout();
				pEquipmentLoadout->SetPackageGroup( group );
			}

// 			pFlashMenu->ScheduleInitialize(CFlashFrontEnd::eFlM_IngameMenu, eFFES_equipment_select);
// 			pFlashMenu->SetBlockClose(true);
		}
	}
}

void SMicrowaveBeamParams::SerializeWith(TSerialize ser)
{
	ser.Value("direction", direction, 'dir0');
	ser.Value("position", position, 'wrld');
}

void CPlayer::RequestMicrowaveBeam(const SMicrowaveBeamParams& params)
{
	if(gEnv->bServer)
	{
		GetGameObject()->InvokeRMI( ClDeployMicrowaveBeam(), SMicrowaveBeamParams(params.position, params.direction), eRMI_ToRemoteClients);
		DeployMicrowaveBeam(params);
	}
	else
	{
		GetGameObject()->InvokeRMI(SvRequestMicrowaveBeam(), params, eRMI_ToServer);
	}
}

void CPlayer::DeployMicrowaveBeam(const SMicrowaveBeamParams& params)
{
}

IMPLEMENT_RMI(CPlayer, SvRequestMicrowaveBeam)
{
	return true;
}

IMPLEMENT_RMI(CPlayer, ClDeployMicrowaveBeam)
{
	DeployMicrowaveBeam(params);
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, SvRequestUseLadder)
{
	if(!IsDead())
	{
		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(params.ladderId))
		{
			SStateEventLadder ladderEvent(pEntity);
			StateMachineHandleEventMovement(ladderEvent);

			SStateEventLadderPosition ladderPositionEvent(params.initialHeightFrac);
			StateMachineHandleEventMovement(ladderPositionEvent);

			return true;
		}
		else
		{
			CryLog("CPlayer::SvRequestUseLadder - %s unable to find entity with id: %d", GetEntity()->GetName(), params.ladderId);
		}
	}
	else
	{
		CryLog("CPlayer::SvRequestUseLadder - %s cannot use the ladder as they are dead", GetEntity()->GetName());
	}

	// Somehow the server doesn't agree the client should be on the ladder
	// But at this point, the client already (predictively) entered on the ladder in the local simulation
	// Therefore, inform the sending client that it shouldn't be on the ladder at all
	GetGameObject()->InvokeRMI(CPlayer::ClLeaveFromLadder(), SRequestLeaveLadderParams(eLLL_Drop), eRMI_ToClientChannel, GetChannelId());
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, SvRequestLeaveFromLadder)
{
	if (IsOnLadder())
	{
		SStateEventLeaveLadder ladderLeaveEvent(params.leaveLocation);
		StateMachineHandleEventMovement(ladderLeaveEvent);
	}
	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, ClLeaveFromLadder)
{
	if (IsOnLadder())
	{
		SStateEventLeaveLadder ladderLeaveEvent(params.leaveLocation);
		StateMachineHandleEventMovement(ladderLeaveEvent);
	}
	return true;
}

//------------------------------------------------------------------------
void CPlayer::SerializeSpawnInfo(TSerialize ser )
{
	ser.Value("teamId", m_teamId, 'team');
	ser.Value("physCounter", m_netPhysCounter, 'ui2');
	ser.Value("modelIndex", m_mpModelIndex, MP_MODEL_INDEX_NET_POLICY);
	
	bool bShowIntro = m_bPlayIntro;
	ser.Value("bInIntro", bShowIntro, 'bool');

	m_bPlayIntro = bShowIntro;
}

void CPlayer::SetPlayIntro(bool playIntro)
{
	if(playIntro != m_bPlayIntro)
	{
		m_bPlayIntro = playIntro;

		if(m_bPlayIntro && IsClient())
			StateMachineHandleEventMovement( PLAYER_EVENT_INTRO_START );
		else
			StateMachineHandleEventMovement( PLAYER_EVENT_INTRO_FINISHED );
	}	
}

namespace CPlayerGetSpawnInfo
{
	struct SInfo : public ISerializableInfo
	{
		int teamId;
		uint8 netPhysCounter;
		uint8 modelIndex;
		bool	bShowIntro;

		void SerializeWith( TSerialize ser )
		{
			ser.Value("teamId", teamId, 'team');
			ser.Value("physCounter", netPhysCounter, 'ui2');
			ser.Value("modelIndex", modelIndex, MP_MODEL_INDEX_NET_POLICY);
			ser.Value("bShowIntro", bShowIntro, 'bool');
		}
	};
}

//------------------------------------------------------------------------

int CPlayer::GetPhysicalSkipEntities(IPhysicalEntity** pSkipList, const int maxSkipSize) const
{
	int skipCount = 0;

	if(maxSkipSize > 0)
	{
		// For now we just care about pick and throw objects
		EntityId pickAndThrowHeldObjectId = GetPickAndThrowEntity();
		if(IsInPickAndThrowMode() && pickAndThrowHeldObjectId)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(pickAndThrowHeldObjectId); 
			if(pEntity)
			{
				IPhysicalEntity* pPhysEnt = pEntity->GetPhysics(); 
				if(pPhysEnt)
				{
					++skipCount;
					*pSkipList = pPhysEnt;
				}
			}
		}
		else
		{
			IPhysicalEntity *pPlayerPhysics = GetEntity()->GetPhysics();
			if (pPlayerPhysics)
			{
				if(skipCount < maxSkipSize)
				{
					pSkipList[skipCount] = pPlayerPhysics;
					++skipCount;
				}
			}
			ICharacterInstance *pChar = GetEntity()->GetCharacter(0);
			if (pChar)
			{
				ISkeletonPose *pPose = pChar->GetISkeletonPose();
				if (pPose)
				{
					if(skipCount < maxSkipSize)
					{
						pSkipList[skipCount] = pPose->GetCharacterPhysics();
						++skipCount;
					}
				}
			}
		}
	}
	return skipCount; 
}

//------------------------------------------------------------------------
ISerializableInfoPtr CPlayer::GetSpawnInfo()
{
	CPlayerGetSpawnInfo::SInfo *p = new CPlayerGetSpawnInfo::SInfo();

	p->bShowIntro			= m_bPlayIntro;
	p->teamId					= 0;
	
	if(CGameRules *pGameRules=g_pGame->GetGameRules())
	{
		p->teamId	= pGameRules->GetTeam(GetEntityId());
	}	
	
	p->netPhysCounter = m_netPhysCounter;
	p->modelIndex			= m_mpModelIndex;

	return p;
}

void CPlayer::BecomeRemotePlayer()
{
	IVehicleClient *pVehicleClient = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicleClient();
	if(pVehicleClient) // in the middle of game shutdown, m_pVehicleClient can't be trusted, so we get it from the vehicle system
	{
		IVehicle *pVehicle = GetLinkedVehicle();
		if(pVehicle)
		{
			IVehicleSeat *pVehicleSeat = pVehicle->GetSeatForPassenger(GetEntityId());
			if(pVehicleSeat)
			{
				pVehicleClient->OnExitVehicleSeat(pVehicleSeat);
			} // pVehicleSeat
		} // pVehicle
	} // pVehicleClient

	ResetScreenFX();

	// deliberately done last as this will reset m_isClient, which ResetScreenFX relies on
	// on being truthful 
	CActor::BecomeRemotePlayer();
}

//------------------------------------------------------------------------
void CPlayer::DeselectWeapon()
{
	if (IsInPickAndThrowMode() && GetInventory())
	{
		static IEntityClass* pPickAndThrowWeaponClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("PickAndThrowWeapon");
		IItem* pItem = GetCurrentItem(false); 
		if(pItem)
		{
			if (pItem->GetEntity()->GetClass() == pPickAndThrowWeaponClass)
			{
				CPickAndThrowWeapon* pPickAndThrowWeapon = static_cast<CPickAndThrowWeapon*>(pItem);

				pPickAndThrowWeapon->Select( false );
			}
		}
	}
}

//------------------------------------------------------------------------
void CPlayer::SelectMovementHierarchy()
{
	// Force the state machine in the proper hierarchy
	if (IsAIControlled())
	{
		CRY_ASSERT(!IsPlayer());

		StateMachineHandleEventMovement(PLAYER_EVENT_ENTRY_AI);
	}
	else
	{
		StateMachineHandleEventMovement(PLAYER_EVENT_ENTRY_PLAYER);
	}
}

//------------------------------------------------------------------------
void CPlayer::InitMannequinParams()
{
	if (IsPlayer())
	{
		InitPlayerMannequin(GetAnimatedCharacter()->GetActionController());
	}

	m_mountedGunController.InitMannequinParams();
}

//------------------------------------------------------------------------
void CPlayer::ApplyMeleeImpulse( const Vec3& impulseDirection, float impulseStrength )
{
	//Individually control horizontal and vertical impulse values for consistency (We don't want large knock-ups when aiming upwards)
	Vec3 impulse = impulseDirection;
	impulse.z = 0.f;
	impulse.Normalize();
	impulse.x *= g_pGameCVars->pl_melee.mp_knockback_strength_hor;
	impulse.y *= g_pGameCVars->pl_melee.mp_knockback_strength_hor;
	impulse.z = g_pGameCVars->pl_melee.mp_knockback_strength_vert;

	impulse *= impulseStrength;

	pe_action_impulse action_impulse;
	action_impulse.iApplyTime = 0;
	action_impulse.impulse = impulse;
	if(IPhysicalEntity* pPhysics = GetEntity()->GetPhysics())
	{
		pPhysics->Action(&action_impulse);
	}

	GetImpulseHander()->SetOnRagdollPhysicalizedImpulse(action_impulse); 
}

//------------------------------------------------------------------------
bool CPlayer::ShouldMuteWeaponSoundStimulus() const
{
	return false;
}


void CPlayer::OnPickedUpPickableAmmo( IEntityClass* pAmmoType, int count )
{
	CALL_PLAYER_EVENT_LISTENERS(OnPickedUpPickableAmmo(this, pAmmoType, count ));
}



// In:	threshold angle, in degrees, that is needed before turning is even considered (>= 0.0f)
// In:	the current angle deviation needs to be over the turnThresholdAngle for longer than this time before the character turns (>= 0.0f)
//
void CPlayer::SetTurnAnimationParams(const float turnThresholdAngle, const float turnThresholdTime)
{
	assert(turnThresholdAngle >= 0.0f);
	assert(turnThresholdTime >= 0.0f);

	CActor::SetTurnAnimationParams(turnThresholdAngle, turnThresholdTime);

	m_animActionAIMovementSettings.turnParams = m_params.AITurnParams;
}


//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, ClApplyMeleeImpulse)
{
	ApplyMeleeImpulse(params.dir, params.strength);

	return true;
}

//------------------------------------------------------------------------
IMPLEMENT_RMI(CPlayer, ClIncrementIntStat)
{
	if(CPersistantStats* pStats = CPersistantStats::GetInstance())
	{
		pStats->IncrementClientStats((EIntPersistantStats)params.m_stat, 1);
	}

	return true;
}


#if ENABLE_RMI_BENCHMARK

IMPLEMENT_RMI( CPlayer, SvBenchmarkPing )
{
	pNetChannel->LogRMIBenchmark( eRMIBA_Handle, params, RMIBenchmarkCallback, this );
	GetGameObject()->InvokeRMI( ClBenchmarkPong(), SRMIBenchmarkParams( eRMIBM_Pong, params.entity, params.seq, params.twoRoundTrips ), eRMI_ToOwnClient );

	return true;
}

IMPLEMENT_RMI( CPlayer, ClBenchmarkPong )
{
	pNetChannel->LogRMIBenchmark( eRMIBA_Handle, params, RMIBenchmarkCallback, this );

	if ( params.twoRoundTrips )
	{
		GetGameObject()->InvokeRMI( SvBenchmarkPang(), SRMIBenchmarkParams( eRMIBM_Pang, params.entity, params.seq, params.twoRoundTrips ), eRMI_ToServer );
	}

	return true;
}

IMPLEMENT_RMI( CPlayer, SvBenchmarkPang )
{
	pNetChannel->LogRMIBenchmark( eRMIBA_Handle, params, RMIBenchmarkCallback, this );
	GetGameObject()->InvokeRMI( ClBenchmarkPeng(), SRMIBenchmarkParams( eRMIBM_Peng, params.entity, params.seq, params.twoRoundTrips ), eRMI_ToOwnClient );

	return true;
}

IMPLEMENT_RMI( CPlayer, ClBenchmarkPeng )
{
	pNetChannel->LogRMIBenchmark( eRMIBA_Handle, params, RMIBenchmarkCallback, this );

	return true;
}

void CPlayer::RMIBenchmarkCallback( ERMIBenchmarkLogPoint point0, ERMIBenchmarkLogPoint point1, int64 delay, void* pUserData )
{
	static const char* const  pEnds[] = { "client", "server" };
	static const char* const  pActions[] = { "queueing", "sending", "receiving", "handling" };
	static const char* const  pThreads[] = { "game", "network" };
	static const char* const  pMessages[] = { "ping", "pong", "pang", "peng" };

	ERMIBenchmarkEnd          end0 = RMIBenchmarkGetEnd( point0 );
	ERMIBenchmarkThread       thread0 = RMIBenchmarkGetThread( point0 );
	ERMIBenchmarkAction       action0 = RMIBenchmarkGetAction( point0 );
	ERMIBenchmarkMessage      message0 = RMIBenchmarkGetMessage( point0 );
	ERMIBenchmarkEnd          end1 = RMIBenchmarkGetEnd( point1 );
	ERMIBenchmarkThread       thread1 = RMIBenchmarkGetThread( point1 );
	ERMIBenchmarkAction       action1 = RMIBenchmarkGetAction( point1 );
	ERMIBenchmarkMessage      message1 = RMIBenchmarkGetMessage( point1 );
	CryFixedStringT< 128 >    description;

	if (point0 == eRMIBLP_ClientGameThreadQueuePing && point1 == eRMIBLP_ClientNetworkThreadSendPing)
	{
		description = "Benchmark RMI Client Game to Network Ping";
	}
	else if (point0 == eRMIBLP_ServerNetworkThreadReceivePing && point1 == eRMIBLP_ServerGameThreadHandlePing)
	{
		description = "Benchmark RMI Server Network to Game Ping";
	}
	else if (point0 == eRMIBLP_ServerGameThreadHandlePing && point1 == eRMIBLP_ServerGameThreadQueuePong)
	{
		description = "Benchmark RMI Server Game Ping Pong";
	}
	else if (point0 == eRMIBLP_ServerGameThreadQueuePong && point1 == eRMIBLP_ServerNetworkThreadSendPong)
	{
		description = "Benchmark RMI Server Game to Network Pong";
	}
	else if (point0 == eRMIBLP_ClientNetworkThreadReceivePong && point1 == eRMIBLP_ClientGameThreadHandlePong)
	{
		description = "Benchmark RMI Client Network to Game Pong";
	}
	else if (point0 == eRMIBLP_ServerNetworkThreadReceivePing && point1 == eRMIBLP_ServerNetworkThreadSendPong)
	{
		description = "Benchmark RMI Server Network Ping Pong";
	}
	else if (point0 == eRMIBLP_ClientNetworkThreadSendPing && point1 == eRMIBLP_ClientNetworkThreadReceivePong)
	{
		description = "Benchmark RMI Client Network Ping Pong";
	}
	else if (point0 == eRMIBLP_ClientGameThreadQueuePing && point1 == eRMIBLP_ClientGameThreadHandlePong)
	{
		description = "Benchmark RMI Client Game Ping Pong";
	}
	else
	{
		// Should only reach here if g_RMIBenchmarkTwoTrips is enabled
		description.Format( "Benchmark RMI %s %s thread %s %s to %s %s thread %s %s delay", pEnds[ end0 ], pThreads[ thread0 ], pActions[ action0 ], pMessages[ message0 ], pEnds[ end1 ], pThreads[ thread1 ], pActions[ action1 ], pMessages[ message1 ] );
	}

	CTelemetryCollector*      pTelemetry = static_cast< CTelemetryCollector* >( g_pGame->GetITelemetryCollector() );

	if ( pTelemetry )
	{
		pTelemetry->LogEvent( description.c_str(), ( float )delay );
	}
	else
	{
		CryLogAlways( "%s: %" PRIi64, description.c_str(), delay );
	}
}

#endif
