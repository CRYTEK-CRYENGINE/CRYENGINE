// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RecordingSystem.h"
#include "Game.h"
#include "GameCVars.h"
#include "ScreenEffects.h"

#include "Actor.h"
#include "Player.h"
#include "Item.h"
#include "GameRules.h"
#include "CryAction.h"

#include "IWeapon.h"
#include "Weapon.h"
#include "Single.h"
#include "PickAndThrowWeapon.h"

#include "UI/UIManager.h"
#include "UI/HUD/HUDEventDispatcher.h"
#include "UI/HUD/HUDEventWrapper.h"

#include <CryEntitySystem/IEntitySystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryEntitySystem/IBreakableManager.h>

#include "IVehicleSystem.h"
#include "WeaponSystem.h"
#include "Projectile.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "HeavyMountedWeapon.h"
#include "ReplayObject.h"
#include "ReplayActor.h"
#include "GameObjects/GameObject.h"
#include "GameRulesModules/IGameRulesStateModule.h"
#include "GameRulesModules/IGameRulesSpectatorModule.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "PersistantStats.h"
#include "Network/Squad/SquadManager.h"
#include "EntityUtility/EntityEffectsCloak.h"
#include "LagOMeter.h"
#include <CrySystem/Profilers/IStatoscope.h>

#include "Utility/CryWatch.h"
#include "Utility/BufferUtil.h"

#include <CryCore/TypeInfo_impl.h>
#include <CryExtension/CryCreateClassInstance.h>

#include "PlayerAnimation.h"
#include "RecordingSystemCompressor.h"
#include "GamePhysicsSettings.h"
#include "RecordingSystemDebug.h"
#include "TeamVisualizationManager.h"
#include "Utility/AttachmentUtils.h"
#include <CryMovie/IMovieSystem.h>

static AUTOENUM_BUILDNAMEARRAY(s_thirdPersonPacketList, ThirdPersonPacketList);
const char* NIGHT_VISION_PE = "NightVision_Active";

// --------------------------------------------------------------------------------
static void DiscardingPacketStatic(SRecording_Packet* ps, float recordedTime, void* inUserData)
{
	g_pGame->GetRecordingSystem()->DiscardingPacket(ps, recordedTime);
}

#ifdef RECSYS_DEBUG
// --------------------------------------------------------------------------------
void CRecordingSystem::FirstPersonDiscardingPacketStatic(SRecording_Packet* ps, float recordedTime, void* inUserData)
{
	const float maxBufferTimeRequired = max(g_pGameCVars->kc_length - g_pGameCVars->kc_kickInTime, g_pGameCVars->kc_kickInTime);
	const float ageOfPacket = (gEnv->pTimer->GetFrameStartTime().GetSeconds() - recordedTime);
	if (!s_savingHighlightsGuard && ageOfPacket < maxBufferTimeRequired && ((CRecordingSystem*)inUserData)->IsRecordingAndNotPlaying())
	{
		RecSysLog("FirstPersonDiscardingPacketStatic(), shouldn't be discarding yet, must have run out of memory");
		CRY_ASSERT_MESSAGE(false, "Recording first person buffer is out of memory, please tell Pete");
	}
}

static size_t maxFirstPersonDataSeen = 0;
static size_t latestFirstPersonDataSize = 0;
#endif //RECSYS_DEBUG

void CRecordingSystem::DiscardingPacket(SRecording_Packet* packet, float recordedTime)
{
#ifdef RECSYS_DEBUG
	const float maxBufferTimeRequired = max(g_pGameCVars->kc_length - g_pGameCVars->kc_kickInTime, g_pGameCVars->kc_kickInTime);
	const float ageOfPacket = (gEnv->pTimer->GetFrameStartTime().GetSeconds() - recordedTime);
	if (!s_savingHighlightsGuard && ageOfPacket < maxBufferTimeRequired && IsRecordingAndNotPlaying())
	{
		RecSysLog("DiscardingPacket(), shouldn't be discarding yet, must have run out of memory");
		CRY_ASSERT_MESSAGE(false, "Recording third person buffer is out of memory, please tell Pete");
	}
#endif //RECSYS_DEBUG

	EntityId playerEntityId = 0;
	switch (packet->type)
	{
	case eTPP_MannEvent:
		playerEntityId = ((SRecording_MannEvent*)packet)->eid;
		break;
	case eTPP_MannSetSlaveController:
		playerEntityId = ((SRecording_MannSetSlaveController*)packet)->masterActorId;
		break;
	case eTPP_WeaponSelect:
		playerEntityId = ((SRecording_WeaponSelect*)packet)->ownerId;
		break;
	case eTPP_FiremodeChanged:
		playerEntityId = ((SRecording_FiremodeChanged*)packet)->ownerId;
		break;
	case eTPP_MountedGunEnter:
		playerEntityId = ((SRecording_MountedGunEnter*)packet)->ownerId;
		break;
	case eTPP_MountedGunLeave:
		playerEntityId = ((SRecording_MountedGunLeave*)packet)->ownerId;
		break;
	case eTPP_EntitySpawn:
		{
			SRecording_EntitySpawn* pEntitySpawn = (SRecording_EntitySpawn*)packet;
			if (pEntitySpawn->entityId)
			{
				m_recordedData.m_discardedEntitySpawns[pEntitySpawn->entityId] = std::make_pair(*pEntitySpawn, recordedTime);
			}
		}
		break;
	case eTPP_EntityHide:
		{
			// Update the hide state of the entity spawn packet
			SRecording_EntityHide* pEntityHide = (SRecording_EntityHide*)packet;
			TEntitySpawnMap::iterator itEntitySpawn = m_recordedData.m_discardedEntitySpawns.find(pEntityHide->entityId);
			if (itEntitySpawn != m_recordedData.m_discardedEntitySpawns.end())
			{
				itEntitySpawn->second.first.hidden = pEntityHide->hide;
			}
		}
		break;
	case eTPP_EntityLocation:
		{
			// Update the location of the entity spawn packet
			SRecording_EntityLocation* pEntityLocation = (SRecording_EntityLocation*)packet;
			TEntitySpawnMap::iterator itEntitySpawn = m_recordedData.m_discardedEntitySpawns.find(pEntityLocation->entityId);
			if (itEntitySpawn != m_recordedData.m_discardedEntitySpawns.end())
			{
				itEntitySpawn->second.first.entitylocation = pEntityLocation->entitylocation;
			}
		}
		break;
	case eTPP_WeaponAccessories:
		{
			SRecording_WeaponAccessories* pWeaponAccessories = (SRecording_WeaponAccessories*)packet;
			m_recordedData.m_discardedWeaponAccessories[pWeaponAccessories->weaponId] = *pWeaponAccessories;
		}
		break;
	case eTPP_EntityRemoved:
		{
			SRecording_EntityRemoved* pEntityRemoved = (SRecording_EntityRemoved*)packet;
			// An entity removed packet cancels out an entity spawn packet

			TEntitySpawnMap::iterator itEntityRemoved = m_recordedData.m_discardedEntitySpawns.find(pEntityRemoved->entityId);
			if (itEntityRemoved != m_recordedData.m_discardedEntitySpawns.end())
			{
				ReleaseReferences(itEntityRemoved->second.first);
				m_recordedData.m_discardedEntitySpawns.erase(itEntityRemoved);
			}

			m_recordedData.m_discardedWeaponAccessories.erase(pEntityRemoved->entityId);
		}
		break;
	case eTPP_ParticleCreated:
		{
			SRecording_ParticleCreated* pParticleCreated = (SRecording_ParticleCreated*)packet;
			if (pParticleCreated->entityId != 0 || m_persistantParticleEffects.count(pParticleCreated->pParticleEffect) != 0)
			{
				// Only keep particle effects which are attached to entities, leave the others
				m_recordedData.m_discardedParticleSpawns[pParticleCreated->pParticleEmitter] = std::make_pair(*pParticleCreated, recordedTime);
				// Don't mess with references - we're copying the packet then removing the original so our count stays the same
			}
		}
		break;
	case eTPP_ParticleDeleted:
		{
			SRecording_ParticleDeleted* pParticleDeleted = (SRecording_ParticleDeleted*)packet;
			// A particle removed packet cancels out a particle spawn packet
			m_recordedData.m_discardedParticleSpawns.erase(pParticleDeleted->pParticleEmitter);
		}
		break;
	case eTPP_PlaySound:
		{
			//SRecording_PlaySound* pPlaySound = (SRecording_PlaySound*)packet;
			//// Only record weapon looping sounds (for now)
			//if (pPlaySound->flags & FLAG_SOUND_LOOP && pPlaySound->semantic == eSoundSemantic_Weapon)
			//{
			//	m_recordedData.m_discardedSounds[pPlaySound->soundId] = std::make_pair(*pPlaySound, recordedTime);
			//	RecSysLogDebug(eRSD_Sound, "Recording discarded looping sounds: %d, %s. num discarded: %" PRISIZE_T, pPlaySound->soundId, pPlaySound->szName, m_recordedData.m_discardedSounds.size());
			//	CRY_ASSERT_MESSAGE(m_recordedData.m_discardedSounds.size() < 64, "We have recorded a lot of discarded looping sounds without stopping them, something may have gone wrong here");
			//}
		}
		break;
	case eTPP_StopSound:
		{
			/*SRecording_StopSound* pStopSound = (SRecording_StopSound*)packet;
			   size_t numErased = m_recordedData.m_discardedSounds.erase(pStopSound->soundId);
			   if (numErased)
			   {
			   RecSysLogDebug(eRSD_Sound, "Removing discarded looping sounds: %d. num discarded: %" PRISIZE_T, pStopSound->soundId, m_recordedData.m_discardedSounds.size());
			   }*/
		}
		break;
	case eTPP_DrawSlotChange:
		{
			SRecording_DrawSlotChange* pDrawSlotChange = (SRecording_DrawSlotChange*)packet;

			// Update the draw slot flags of the entity spawn packet
			TEntitySpawnMap::iterator itEntitySpawn = m_recordedData.m_discardedEntitySpawns.find(pDrawSlotChange->entityId);
			if (itEntitySpawn != m_recordedData.m_discardedEntitySpawns.end())
			{
				SRecording_EntitySpawn& spawnPacket = itEntitySpawn->second.first;
				assert(pDrawSlotChange->iSlot >= 0 && pDrawSlotChange->iSlot < RECORDING_SYSTEM_MAX_SLOTS);
				spawnPacket.slotFlags[pDrawSlotChange->iSlot] = pDrawSlotChange->flags;
			}
		}
		break;
	case eTPP_StatObjChange:
		{
			SRecording_StatObjChange* pStatObjChange = (SRecording_StatObjChange*)packet;

			// Update the stat obj of the entity spawn packet
			TEntitySpawnMap::iterator itEntitySpawn = m_recordedData.m_discardedEntitySpawns.find(pStatObjChange->entityId);
			if (itEntitySpawn != m_recordedData.m_discardedEntitySpawns.end())
			{
				SRecording_EntitySpawn& spawnPacket = itEntitySpawn->second.first;
				assert(pStatObjChange->iSlot >= 0 && pStatObjChange->iSlot < RECORDING_SYSTEM_MAX_SLOTS);
				IStatObj* pPrevStatObj = spawnPacket.pStatObj[pStatObjChange->iSlot];
				if (pPrevStatObj)
				{
					pPrevStatObj->Release();
				}
				spawnPacket.pStatObj[pStatObjChange->iSlot] = pStatObjChange->pNewStatObj;
			}
		}
		break;
	case eTPP_SubObjHideMask:
		{
			SRecording_SubObjHideMask* pHideMask = (SRecording_SubObjHideMask*)packet;

			assert(pHideMask->slot == 0);

			// Update the sub obj hide mask of the entity spawn packet
			TEntitySpawnMap::iterator itEntitySpawn = m_recordedData.m_discardedEntitySpawns.find(pHideMask->entityId);
			if (itEntitySpawn != m_recordedData.m_discardedEntitySpawns.end())
			{
				SRecording_EntitySpawn& spawnPacket = itEntitySpawn->second.first;
				spawnPacket.subObjHideMask = pHideMask->subObjHideMask;
			}
		}
		break;
	case eTPP_EntityAttached:
		{
			SRecording_EntityAttached* pEntityAttached = (SRecording_EntityAttached*)packet;
			m_recordedData.m_discardedEntityAttached.push_back(*pEntityAttached);
		}
		break;
	case eTPP_EntityDetached:
		{
			SRecording_EntityDetached* pEntityDetached = (SRecording_EntityDetached*)packet;
			TEntityAttachedVec::iterator itEntityAttached;
			for (itEntityAttached = m_recordedData.m_discardedEntityAttached.begin(); itEntityAttached != m_recordedData.m_discardedEntityAttached.end(); ++itEntityAttached)
			{
				if (itEntityAttached->entityId == pEntityDetached->entityId && itEntityAttached->actorId == pEntityDetached->actorId)
				{
					m_recordedData.m_discardedEntityAttached.erase(itEntityAttached);
					break;
				}
			}
		}
		break;
	case eTPP_PlayerJoined:
		playerEntityId = ((SRecording_PlayerJoined*) packet)->playerId;
		break;
	case eTPP_PlayerLeft:
		playerEntityId = ((SRecording_PlayerLeft*) packet)->playerId;
		break;
	case eTPP_PlayerChangedModel:
		playerEntityId = ((SRecording_PlayerChangedModel*) packet)->playerId;
		break;
	case eTPP_TeamChange:
		playerEntityId = ((SRecording_TeamChange*) packet)->entityId;
		break;
	case eTPP_CorpseSpawned:
		{
			CRY_ASSERT(m_recordedData.m_corpses.size() < m_recordedData.m_corpses.max_size());
			if (m_recordedData.m_corpses.size() < m_recordedData.m_corpses.max_size())
			{
				SRecording_CorpseSpawned* pCorpsePacket = (SRecording_CorpseSpawned*) packet;
				if (IEntity* pCorpseEntity = gEnv->pEntitySystem->GetEntity(pCorpsePacket->corpseId))
				{
					if (ICharacterInstance* pChar = pCorpseEntity->GetCharacter(0))
					{
						ISkeletonPose* pPose = pChar->GetISkeletonPose();
						IDefaultSkeleton& rIDefaultSkeleton = pChar->GetIDefaultSkeleton();
						if (pPose)
						{
							STrackedCorpse corpse;
							corpse.m_corpseId = pCorpsePacket->corpseId;
							corpse.m_numJoints = min((int32) rIDefaultSkeleton.GetJointCount(), static_cast<int32>(STrackedCorpse::k_maxJoints));

							static uint8 s_ADIKJoints[STrackedCorpse::k_maxJoints];
							memset(&s_ADIKJoints[0], 0, sizeof(s_ADIKJoints[0]) * STrackedCorpse::k_maxJoints);
							TCorpseADIKJointsVec::const_iterator end = m_corpseADIKJointCRCs.end();
							for (TCorpseADIKJointsVec::const_iterator it = m_corpseADIKJointCRCs.begin(); it != end; ++it)
							{
								const int16 jointIdx = rIDefaultSkeleton.GetJointIDByCRC32(*it);
								if (jointIdx != -1)
									s_ADIKJoints[jointIdx] = 1;
							}
							for (int32 i = 0; i < corpse.m_numJoints; i++)
							{
								if (s_ADIKJoints[i])
								{
									QuatT rel(pPose->GetRelJointByID(i));
									rel.t.x = 0.f;
									const int32 parent = rIDefaultSkeleton.GetJointParentIDByID(i);
									if (parent < 0)
									{
										corpse.m_jointAbsPos[i] = rel;
									}
									else
									{
										corpse.m_jointAbsPos[i] = pPose->GetAbsJointByID(parent) * rel;
									}
								}
								else
								{
									corpse.m_jointAbsPos[i] = pPose->GetAbsJointByID(i);
								}
							}
							m_recordedData.m_corpses.push_back(corpse);
						}
					}
				}
			}
			break;
		}
	case eTPP_CorpseRemoved:
		{
			SRecording_CorpseRemoved* pCorpsePacket = (SRecording_CorpseRemoved*) packet;
			EntityId corpseId = pCorpsePacket->corpseId;
			int numCorpses = m_recordedData.m_corpses.size();
			for (int i = 0; i < numCorpses; ++i)
			{
				if (m_recordedData.m_corpses[i].m_corpseId == corpseId)
				{
					m_recordedData.m_corpses.removeAt(i);
					break;
				}
			}
			break;
		}
	case eTPP_ObjectCloakSync:
		{
			playerEntityId = ((SRecording_ObjectCloakSync*) packet)->cloakPlayerId;
			break;
		}
	case eTPP_PickAndThrowUsed:
		{
			playerEntityId = ((SRecording_PickAndThrowUsed*) packet)->playerId;
			break;
		}
	case eTPP_InteractiveObjectFinishedUse:
		{
			SRecording_InteractiveObjectFinishedUse* intObjFinishedUsePacket = (SRecording_InteractiveObjectFinishedUse*)packet;
			m_interactiveObjectAnimations[intObjFinishedUsePacket->objectId] = SInteractiveObjectAnimation(intObjFinishedUsePacket->interactionTypeTag, intObjFinishedUsePacket->interactionIndex);
			break;
		}
	}
	if (playerEntityId != 0)
	{
		if (packet->type == eTPP_PlayerJoined)
		{
			// Find the first free index into the array
			for (int i = 0; i < MAX_RECORDED_PLAYERS; ++i)
			{
				SPlayerInitialState& initialState = m_recordedData.m_playerInitialStates[i];
				if (initialState.playerId == 0)
				{
					initialState.Reset();
					initialState.playerId = ((SRecording_PlayerJoined*) packet)->playerId;
					initialState.playerJoined = *((SRecording_PlayerJoined*) packet);
					break;
				}
			}
		}
		else
		{
			bool bFound = false;
			for (int i = 0; i < MAX_RECORDED_PLAYERS; ++i)
			{
				SPlayerInitialState& initialState = m_recordedData.m_playerInitialStates[i];
				if (initialState.playerId == playerEntityId)
				{
					switch (packet->type)
					{
					case eTPP_MannEvent:
						{
							SRecording_MannEvent* mannEvent = (SRecording_MannEvent*)packet;

							switch (mannEvent->historyItem.type)
							{
							case SMannHistoryItem::Fragment:
								{
									bool installed = false;
									for (uint32 j = 0; j < SPlayerInitialState::MAX_FRAGMENT_TRIGGERS; j++)
									{
										SRecording_MannEvent& triggerSlot = initialState.fragmentTriggers[j];
										ActionScopes scopeMask = triggerSlot.historyItem.scopeMask;

										if (!scopeMask || (scopeMask & mannEvent->historyItem.scopeMask) != 0)
										{
											if (installed)
											{
												triggerSlot.historyItem.type = SMannHistoryItem::None;
												triggerSlot.historyItem.scopeMask = 0;
											}
											else
											{
												triggerSlot = *mannEvent;
											}
											installed = true;
										}
									}
									break;
								}
							case SMannHistoryItem::Tag:
								initialState.tagSetting = *mannEvent;
								break;
							}
						}
						break;
					case eTPP_MannSetSlaveController:
						{
							SRecording_MannSetSlaveController* mannSetSlave = (SRecording_MannSetSlaveController*)packet;
							initialState.mannSetSlaveController = *mannSetSlave;
							break;
						}
					case eTPP_WeaponSelect:
						{
							SRecording_WeaponSelect* pWeaponPacket = (SRecording_WeaponSelect*) packet;
							if ((initialState.weapon.ownerId == 0) || (pWeaponPacket->isSelected) || (pWeaponPacket->weaponId == initialState.weapon.weaponId))
							{
								initialState.weapon = *pWeaponPacket;
							}
						}
						break;
					case eTPP_FiremodeChanged:
						initialState.firemode = *((SRecording_FiremodeChanged*)packet);
						break;
					case eTPP_MountedGunEnter:
						initialState.mountedGunEnter = *((SRecording_MountedGunEnter*)packet);
						break;
					case eTPP_MountedGunLeave:
						initialState.mountedGunEnter.size = 0;
						break;
					case eTPP_PlayerChangedModel:
						initialState.changedModel = *((SRecording_PlayerChangedModel*) packet);
						break;
					case eTPP_PlayerLeft:
						initialState.playerId = 0;
						break;
					case eTPP_TeamChange:
						initialState.teamChange = *((SRecording_TeamChange*) packet);
						break;
					case eTPP_ObjectCloakSync:
						initialState.objCloakParams = *((SRecording_ObjectCloakSync*) packet);
						initialState.objCloakParams.fadeToDesiredCloakTarget = false; // Force snap instantly to cloak/uncloak state now
						break;
					case eTPP_PickAndThrowUsed:
						{
							SRecording_PickAndThrowUsed* pPickAndThrow = (SRecording_PickAndThrowUsed*) packet;
							if (pPickAndThrow->bPickedUp)
							{
								initialState.pickAndThrowUsed = *((SRecording_PickAndThrowUsed*) packet);
							}
							else
							{
								initialState.pickAndThrowUsed.playerId = 0;
							}
						}
						break;
					}
					bFound = true;
					break;
				}
			}
		}
	}
}

int CRecordingSystem::m_replayEventGuard = 0;
int CRecordingSystem::s_savingHighlightsGuard = 0;

CRecordingSystem::CRecordingSystem()
	: m_recordingState(eRS_NotRecording)
	, m_playbackState(ePS_NotPlaying)
	, m_playbackSetupStage(ePSS_None)
	, m_killer(0)
	, m_victim(0)
	, m_projectileId(0)
	, m_killerReplayGunId(0)
	, m_weaponFPAiming(true)
	, m_projectileType(ePT_Grenade)
	, m_bulletTimePhase(eBTP_Approach)
	, m_timeScale(1.f)
	, m_timeSincePlaybackStart(0.f)
	, m_recordedStartTime(0.f)
	, m_queuedPacketsSize(0)
	, m_killerIsFriendly(false)
	, m_newProjectilePosition(IDENTITY)
	, m_clientShot(false)
	, m_playedFrame(false)
	, m_timeSinceLastRecord(0.f)
	, m_timeSinceLastFPRecord(0.f)
	, m_C4Adjustment(-1.0f)
	, m_cameraBlendFraction(0.0f)
	, m_bulletTravelTime(0.f)
	, m_bulletHoverStartTime(0.f)
	, m_bulletEntityId(0)
	, m_bulletTimeKill(false)
	, m_bulletGeometryName("")
	, m_bulletEffectName("")
	, m_disableRifling(false)
	, m_hasWinningKillcam(false)
	, m_stressTestTimer(30.f)
	, m_replayBlendedCamPos(IDENTITY)
	, m_pPlaybackData(NULL)
	, m_pHighlightData(NULL)
	, m_pTPDataBuffer(NULL)
	, m_tpDataBufferSize(0)
	, m_highlightsReel(false)
	, m_bLoopHighlightsReel(false)
	, m_autoHideIgnoreEntityId(0)
	, m_bCanPlayWinningKill(false)
	, m_gameCameraLinkedToEntityId(0)
{
	RecSysLogFunc;

	m_breakEventIndicies.reserve(20);
	m_clonedNodes.reserve(20);
	m_recordingAnimEntities.reserve(20);
	m_newParticles.reserve(50);
	m_newActorEntities.reserve(16);
	m_itemAccessoriesChanged.reserve(10);
	m_trackedSounds.reserve(20);

#ifdef RECSYS_DEBUG
	if (gEnv->pInput)
		gEnv->pInput->AddEventListener(this);

	m_pDebug = new CRecordingSystemDebug(*this);
#endif //RECSYS_DEBUG

	if (gEnv->pParticleManager)
		gEnv->pParticleManager->AddEventListener(this);

	REINST("needs verification!");
	/*if (gEnv->pSoundSystem)
	   gEnv->pSoundSystem->AddEventListener(this, false);*/

	if (g_pGame->GetIGameFramework())
		g_pGame->GetIGameFramework()->AddBreakEventListener(this);

	m_pBuffer = new CRecordingBuffer(RECORDING_BUFFER_SIZE);
	m_pBuffer->SetPacketDiscardCallback(DiscardingPacketStatic, NULL);
	m_pBufferFirstPerson = new CRecordingBuffer(FP_RECORDING_BUFFER_SIZE);
#ifdef RECSYS_DEBUG
	m_pBufferFirstPerson->SetPacketDiscardCallback(FirstPersonDiscardingPacketStatic, this);
#endif //RECSYS_DEBUG

	for (int i = 0; i < MAX_HIGHLIGHTS; ++i)
	{
		m_highlightData[i].m_bUsed = false;
	}

	Reset();

	gEnv->pEntitySystem->AddSink(this, IEntitySystem::OnSpawn | IEntitySystem::OnRemove);

	if (IBreakableManager* pBreakMgr = gEnv->pEntitySystem->GetBreakableManager())
		pBreakMgr->AddBreakEventListener(this);

	// The XML for the KillCamGameEffects hasn't loaded yet...
	m_killCamGameEffect.Initialise();
	m_activeSoundLoop.SetSignal("Killcam_ActiveLoop");
	m_kvoltSoundLoop.SetSignal("Killcam_KvoltLoop");

	m_tracerSignal = g_pGame->GetGameAudio()->GetSignalID("Killcam_Tracer");

	m_replayTimer.reset(gEnv->pTimer->CreateNewTimer());

	//Cache information required for interactive object playback
	IAnimationDatabaseManager& adbManager = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager();
	m_pDatabaseInteractiveObjects = adbManager.Load("Animations/Mannequin/ADB/interactiveObjects.adb");
	m_pPlayerControllerDef = adbManager.LoadControllerDef("Animations/Mannequin/ADB/PlayerControllerDefs.xml");
	const IAnimationDatabase* pDatabasePlayer = adbManager.Load("Animations/Mannequin/ADB/PlayerAnims1P.adb");
	m_interactFragmentID = pDatabasePlayer->GetFragmentID("Interact");
}

void CRecordingSystem::Init(XmlNodeRef root)
{
	IEntitySystem* pEntSys = gEnv->pEntitySystem;
	if (!pEntSys)
		return;

	IEntityClassRegistry* pClassReg = pEntSys->GetClassRegistry();
	if (!pClassReg)
		return;

	if (root == NULL)
		return;

	m_pBreakage = pClassReg->FindClass("Breakage");
	m_pRocket = pClassReg->FindClass("rocket");
	m_pC4Explosive = pClassReg->FindClass("c4explosive");
	m_pExplosiveGrenade = pClassReg->FindClass("explosivegrenade");
	m_pLTagGrenade = pClassReg->FindClass("LTagGrenade");
	m_pFlashbang = pClassReg->FindClass("flashbang");
	m_pGrenadeLauncherGrenade = pClassReg->FindClass("GrenadeLauncherGrenade");
	m_pCorpse = pClassReg->FindClass("Corpse");
	m_pInteractiveObjectExClass = pClassReg->FindClass("InteractiveObjectEx");

	// Load Filtered Entities.
	m_recordEntityClassFilter.clear();
	CRecordingSystem::LoadClassData(*pClassReg, root->findChild("FilteredEntities"), m_recordEntityClassFilter);

	// Load Mounted Weapon Classes.
	m_mountedWeaponClasses.clear();
	CRecordingSystem::LoadClassData(*pClassReg, root->findChild("MountedWeapons"), m_mountedWeaponClasses);

#ifndef _RELEASE
	// Load Valid classes with no Geometry.
	m_validWithNoGeometry.clear();
	CRecordingSystem::LoadClassData(*pClassReg, root->findChild("NoGeometryValid"), m_validWithNoGeometry);
#endif //_RELEASE

	// Load Corpse Joint CRCs.
	m_corpseADIKJointCRCs.clear();
	XmlNodeRef corpseJoints = root->findChild("CorpseADIKJoints");
	if (corpseJoints)
	{
		const int count = corpseJoints->getChildCount();
		m_corpseADIKJointCRCs.resize(count);
		for (int i = 0; i < count; i++)
		{
			XmlNodeRef child = corpseJoints->getChild(i);
			const char* pName = child->getAttr("name");
			m_corpseADIKJointCRCs[i] = CCrc32::ComputeLowercase(pName);
			RecSysLogDebug(eRSD_Config, "ADIKJoint: [0x%08x] %s", m_corpseADIKJointCRCs[i], pName);
		}
	}

	// Load Excluded Particle Effects.
	m_excludedParticleEffects.clear();
	m_replacementParticleEffects.clear();
	m_persistantParticleEffects.clear();
	XmlNodeRef particles = root->findChild("ParticleEffects");
	if (particles)
	{
		if (IParticleManager* pParticleMan = gEnv->pParticleManager)
		{
			XmlNodeRef excluded = particles->findChild("Excluded");
			if (excluded)
			{
				const int count = excluded->getChildCount();
				for (int i = 0; i < count; i++)
				{
					XmlNodeRef child = excluded->getChild(i);
					const char* pName = child->getAttr("name");
					IParticleEffect* pEffect = pParticleMan->FindEffect(pName);
					m_excludedParticleEffects.insert(pEffect);
					RecSysLogDebug(eRSD_Config, "Excluding Particle Effect: [%p] %s", pEffect, pName);
				}
			}

			XmlNodeRef replace = particles->findChild("Replace");
			if (replace)
			{
				const int count = replace->getChildCount();
				for (int i = 0; i < count; i++)
				{
					XmlNodeRef child = replace->getChild(i);
					IParticleEffect* pA = pParticleMan->FindEffect(child->getAttr("replace"));
					IParticleEffect* pB = pParticleMan->FindEffect(child->getAttr("with"));
					int bothWays = 0;
					child->getAttr("bothWays", bothWays);
					if (pA && pB)
					{
						m_replacementParticleEffects[pA] = pB;
						if (bothWays)
							m_replacementParticleEffects[pB] = pA;
					}
				}
			}

			XmlNodeRef persistant = particles->findChild("Persistant");
			if (persistant)
			{
				const int count = persistant->getChildCount();
				for (int i = 0; i < count; i++)
				{
					XmlNodeRef child = persistant->getChild(i);
					m_persistantParticleEffects.insert(pParticleMan->FindEffect(child->getAttr("name")));
				}
			}
		}
		else
		{
			CryFatalError("No Particle manager could be found for RecordingSystem XML Parsing.");
		}
	}

	// Load Excluded Sounds.
	m_excludedSoundEffects.clear();
	XmlNodeRef sounds = root->findChild("Sounds");
	if (particles)
	{
		XmlNodeRef excluded = sounds->findChild("Excluded");
		if (excluded)
		{
			const int count = excluded->getChildCount();
			for (int i = 0; i < count; i++)
			{
				XmlNodeRef child = excluded->getChild(i);
				m_excludedSoundEffects.insert(CCrc32::Compute(child->getAttr("name")));
			}
		}
	}

	XmlNodeRef highlightRules = root->findChild("Highlights");
	if (highlightRules)
	{
		m_highlightsRules.Init(highlightRules);
	}
}

/*static*/ void CRecordingSystem::LoadClassData(const IEntityClassRegistry& classReg, XmlNodeRef xmlData, TEntityClassSet& data)
{
	if (xmlData)
	{
		const int numChildren = xmlData->getChildCount();
		for (int i = 0; i < numChildren; ++i)
		{
			XmlNodeRef child = xmlData->getChild(i);
			if (child->isTag("class"))
			{
				const char* name = child->getAttr("name");
				if (IEntityClass* pEntityClass = classReg.FindClass(name))
				{
					RecSysLogDebug(eRSD_Config, "%s: [%p] %s", xmlData->getTag(), pEntityClass, name);
					data.insert(pEntityClass);
				}
				else
				{
					RecSysLogAlways("Could not find class: %s", name);
					CRY_ASSERT_MESSAGE(false, "Could not find class.");
				}
			}
		}
	}
}

void CRecordingSystem::OnGameRulesInit()
{
	CGameRules* pGameRules = g_pGame->GetGameRules();

	if (pGameRules)
	{
		pGameRules->RegisterPickupListener(this);
	}

	XmlNodeRef xmlData = GetISystem()->LoadXmlFromFile("Scripts/KillCam/KillCam.xml");
	CRY_ASSERT(xmlData != NULL);
	Init(xmlData);

	Reset();

	// Make sure the first packet in the buffer is the frame time packet
	m_pBuffer->Update();

	m_recordingState = eRS_Recording;
}

void CRecordingSystem::OnStartGame()
{
	IStatObj* pCTFFriendly = gEnv->p3DEngine->FindStatObjectByFilename("objects/library/props/cw2_ctf_flag/cw2_ctf_flag_blue_v2.cgf");
	IStatObj* pCTFEnemy = gEnv->p3DEngine->FindStatObjectByFilename("objects/library/props/cw2_ctf_flag/cw2_ctf_flag_red_v2.cgf");

	if (pCTFFriendly && pCTFEnemy)
	{
		m_replacementStatObjs[pCTFFriendly] = pCTFEnemy;
		m_replacementStatObjs[pCTFEnemy] = pCTFFriendly;
	}

	m_killCamGameEffect.Initialise();

	IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
	const IEntityClass* pGeomEntity = pClassRegistry->FindClass("GeomEntity");
	const IEntityClass* pAnimObject = pClassRegistry->FindClass("AnimObject");
	const IEntityClass* pPowerStruggleNode = pClassRegistry->FindClass("PowerStruggleNode");
	const IEntityClass* pParticleEffect = pClassRegistry->FindClass("ParticleEffect");

	IMovieSystem* pMovieSystem = gEnv->pMovieSystem;
	int numSequences = pMovieSystem->GetNumSequences();
	for (int seq = 0; seq < numSequences; ++seq)
	{
		IAnimSequence* pSequence = pMovieSystem->GetSequence(seq);
		if (pSequence)
		{
			int numNodes = pSequence->GetNodeCount();
			for (int node = 0; node < numNodes; ++node)
			{
				IAnimNode* pNode = pSequence->GetNode(node);
				IAnimEntityNode* pEntityNode = pNode ? pNode->QueryEntityNodeInterface() : NULL;

				if (pEntityNode)
				{
					IEntity* pEntity = pEntityNode->GetEntity();
					if (pEntity)
					{
						EntityId entityId = pEntity->GetId();
						const IEntityClass* pEntityClass = pEntity->GetClass();
						if ((pEntityClass == pGeomEntity) ||
						    (pEntityClass == pAnimObject) ||
						    (pEntityClass == pParticleEffect) ||
						    (pEntityClass == pPowerStruggleNode))
						{
							if (m_recordingEntities.count(entityId) == 0)
							{
								const SRecordingEntity::EEntityType type = RecordEntitySpawn(pEntity);

								if (pEntity->IsHidden())
								{
									SRecording_EntityHide hideEvent;
									hideEvent.entityId = entityId;
									hideEvent.hide = true;
									m_pBuffer->AddPacket(hideEvent);
								}

								if (pEntityClass == pAnimObject || pEntityClass == pPowerStruggleNode)
								{
									SRecordingAnimObject animObject;
									animObject.entityId = entityId;
									m_recordingAnimEntities.push_back(animObject);
								}

								AddRecordingEntity(pEntity, type);
							}
						}
					}
				}
			}
		}
	}
}

void CRecordingSystem::Reset(void)
{
	RecSysLogFunc;

	if (IsPlayingOrQueued())
	{
		StopPlayback();
	}

	for (CRecordingBuffer::iterator itPacket = m_pBuffer->begin(); itPacket != m_pBuffer->end(); ++itPacket)
	{
		if (itPacket->type == eTPP_EntitySpawn)
		{
			SRecording_EntitySpawn* pEntitySpawn = static_cast<SRecording_EntitySpawn*>(&(*itPacket));
			ReleaseReferences(*pEntitySpawn);
		}
		else if (itPacket->type == eTPP_StatObjChange)
		{
			SRecording_StatObjChange* pStatObjChange = static_cast<SRecording_StatObjChange*>(&(*itPacket));
			assert(pStatObjChange->pNewStatObj);
			pStatObjChange->pNewStatObj->Release();
		}
	}

	m_pBuffer->Reset();
	m_pBufferFirstPerson->Reset();
	m_recordedData.Reset();
	m_queuedPacketsSize = 0;
	m_tpDataBufferSize = 0;
	m_clientShot = false;
	m_fLastRotSpeed = 0.0f;
	m_lastView = Ang3(0, 0, 0);

	for (TReplayParticleMap::iterator itParticle = m_replayParticles.begin(); itParticle != m_replayParticles.end(); ++itParticle)
	{
		itParticle->second->Release();
	}

	const TEntitySpawnMap::iterator endSpawns = m_recordedData.m_discardedEntitySpawns.end();
	for (TEntitySpawnMap::iterator itSpawns = m_recordedData.m_discardedEntitySpawns.begin(); itSpawns != endSpawns; ++itSpawns)
	{
		ReleaseReferences(itSpawns->second.first);
	}

	const uint32 numListeners = m_mannequinListeners.size();
	for (uint32 i = 0; i < numListeners; i++)
	{
		delete m_mannequinListeners[i];
	}
	m_mannequinListeners.clear();

	m_replayActors.clear();
	m_replayEntities.clear();
	m_replayParticles.clear();
	//m_replaySounds.clear();
	m_interactiveObjectAnimations.clear();

	std::vector<SRecording_ParticleCreated>::iterator itParticle;
	for (itParticle = m_newParticles.begin(); itParticle != m_newParticles.end(); ++itParticle)
	{
		SRecording_ParticleCreated& particle = *itParticle;
		particle.pParticleEmitter->Release();
	}
	m_newParticles.clear();
	m_newActorEntities.clear();
	m_recordingEntities.clear();
	CleanUpDeferredRays();
	m_chrvelmap.clear();
	m_eaikm.clear();

	ClearStringCaches();

	m_hasWinningKillcam = false;

	m_replacementStatObjs.clear();

	m_activeSoundLoop.Reset();
	m_kvoltSoundLoop.Reset();

	for (int i = 0; i < MAX_HIGHLIGHTS; ++i)
	{
		CleanupHighlight(i);
		m_highlightData[i].m_data.Reset();
	}
	m_recordKillQueue.m_timeTilRecord = -1.f;
	m_bShowAllHighlights = false;
	m_highlightIndex = 0;

	m_recordingState = eRS_NotRecording;
	m_playbackState = ePS_NotPlaying;

	m_PlaybackInstanceData.Reset();

	m_forwarder.Reset();
	m_sender.Reset();
	m_streamer.ClearAllStreamData();
	m_killCamGameEffect.SetActive(false);
}

void CRecordingSystem::ReleaseReferences(SRecording_EntitySpawn& entitySpawn)
{
	for (int i = 0; i < RECORDING_SYSTEM_MAX_SLOTS; i++)
	{
		IStatObj* pStatObj = entitySpawn.pStatObj[i];
		if (pStatObj)
		{
			pStatObj->Release();
		}
		entitySpawn.pStatObj[i] = NULL;
	}

	SAFE_RELEASE(entitySpawn.pMaterial);

	if (entitySpawn.pScriptTable)
	{
		entitySpawn.pScriptTable->Release();
		entitySpawn.pScriptTable = NULL;
	}
}

void CRecordingSystem::AddReferences(SRecording_EntitySpawn& entitySpawn)
{
	for (int i = 0; i < RECORDING_SYSTEM_MAX_SLOTS; i++)
	{
		IStatObj* pStatObj = entitySpawn.pStatObj[i];
		if (pStatObj)
		{
			pStatObj->AddRef();
		}
	}

	if (entitySpawn.pMaterial)
	{
		entitySpawn.pMaterial->AddRef();
	}

	if (entitySpawn.pScriptTable)
	{
		entitySpawn.pScriptTable->AddRef();
	}
}

CRecordingSystem::~CRecordingSystem()
{
	RecSysLogFunc;
#ifdef RECSYS_DEBUG
	if (gEnv->pInput)
		gEnv->pInput->RemoveEventListener(this);

	SAFE_DELETE(m_pDebug);
#endif //RECSYS_DEBUG

	if (gEnv->pParticleManager)
		gEnv->pParticleManager->RemoveEventListener(this);

	if (IEntitySystem* pEntSys = gEnv->pEntitySystem)
	{
		pEntSys->RemoveSink(this);

		if (IBreakableManager* pBreakMgr = pEntSys->GetBreakableManager())
			pBreakMgr->RemoveBreakEventListener(this);
	}

	/*if (gEnv->pSoundSystem)
	   gEnv->pSoundSystem->RemoveEventListener(this);*/

	if (g_pGame->GetIGameFramework())
		g_pGame->GetIGameFramework()->RemoveBreakEventListener(this);

	if (CGameRules* pGameRules = g_pGame->GetGameRules())
	{
		pGameRules->UnRegisterPickupListener(this);
	}

	for (TRecEntitiesMap::iterator it = m_recordingEntities.begin(), end = m_recordingEntities.end(); it != end; ++it)
	{
		RemoveRecordingEntity(it->second);
	}

	const uint32 numListeners = m_mannequinListeners.size();
	for (uint32 i = 0; i < numListeners; i++)
	{
		delete m_mannequinListeners[i];
	}
	m_mannequinListeners.clear();

	SAFE_DELETE(m_pBuffer);
	SAFE_DELETE(m_pBufferFirstPerson);
	m_killCamGameEffect.Release();
}

void CRecordingSystem::RegisterListener(IRecordingSystemListener& listener)
{
	m_listeners.push_back(&listener);
}

void CRecordingSystem::UnregisterListener(IRecordingSystemListener& listener)
{
	TRecordingSystemListeners::iterator found = std::find(m_listeners.begin(), m_listeners.end(), &listener);
	if (found != m_listeners.end())
	{
		m_listeners.erase(found);
	}
}

template<unsigned int N>
void CompressVictimData(CryFixedArray<SRecording_VictimPosition, N>& victimPositions, float deathTime)
{
	if (victimPositions.size() == 0)
	{
		return;
	}

	SRecording_VictimPosition s_tempbuffer[N];
	int tempindex = 0;
	int prevKeptIndex = 0;

	// Always keep the first packet
	s_tempbuffer[tempindex++] = victimPositions[0];

	// Discard some of the packets in the middle if they can be interpolated without
	// introducing too much loss in precision
	for (int i = 1; i < (int)victimPositions.size(); i++)
	{
		SRecording_VictimPosition* prev = &victimPositions[prevKeptIndex];
		SRecording_VictimPosition* current = &victimPositions[i];

		bool discardPacket = true;

		// Only consider keeping the packet if the frame time is far apart enough
		if (current->frametime - prev->frametime > 0.01f)
		{
			// Always keep the last packet, and a packet at the time the player died
			if (i == victimPositions.size() - 1 || (current->frametime >= deathTime && prev->frametime < deathTime))
			{
				discardPacket = false;
			}
			else
			{
				SRecording_VictimPosition* next = &victimPositions[i + 1];
				// Check what error would be introduced if the current packet would be removed
				for (int j = (prevKeptIndex + 1); j <= i; j++)
				{
					current = &victimPositions[j];

					float lerpFrac = (current->frametime - prev->frametime) / (next->frametime - prev->frametime);

					Vec3 pred;
					pred.SetLerp(prev->victimPosition, next->victimPosition, lerpFrac);

					// Calculate the difference between the interpolated and actual camera position
					Vec3 preddiff;
					preddiff = pred - current->victimPosition;

					const float positionTolerance = 0.1f;
					if (preddiff.GetLengthSquared() > sqr(positionTolerance))
					{
						// The error is too large to throw the packet away
						discardPacket = false;
						break;
					}
				}
			}
		}

		if (!discardPacket)
		{
			s_tempbuffer[tempindex++] = victimPositions[i];
			prevKeptIndex = i;
		}
	}

	victimPositions.clear();

	for (int i = 0; i < tempindex; ++i)
	{
		victimPositions.push_back(s_tempbuffer[i]);
	}
}

void CRecordingSystem::GetTPCameraData(float startTime)
{
	while (m_pBuffer->size() > 0)
	{
		SRecording_Packet* pPacket = (SRecording_Packet*)m_pBuffer->at(0);
		if (pPacket->type == eRBPT_FrameData)
		{
			SRecording_FrameData* pFramePacket = (SRecording_FrameData*)pPacket;
			if (pFramePacket->frametime >= startTime)
			{
				break;
			}
		}
		else
		{
			CRY_ASSERT_MESSAGE(false, "The first packet in the buffer should always be of type eRBPT_FrameData");
		}
		m_pBuffer->RemoveFrame();
	}
	m_pPlaybackData->m_tpdatasize = m_pBuffer->GetData(m_pPlaybackData->m_tpdatabuffer, RECORDING_BUFFER_SIZE);
}

size_t CRecordingSystem::GetFirstPersonDataForTimeRange(uint8** data, float fromTime, float toTime)
{
	bool bCopying = false;
	size_t offset = 0;
	size_t datasize = 0;
	size_t startOffset = 0;
	// Pick out FPChar packets (the majority)
	while (offset < m_pBufferFirstPerson->size())
	{
		SRecording_Packet* pPacket = (SRecording_Packet*)m_pBufferFirstPerson->at(offset);
		if (pPacket->type == eFPP_FPChar)
		{
			SRecording_FPChar* pChar = (SRecording_FPChar*)pPacket;
			if (pChar->frametime >= toTime)
			{
				break;
			}
			else if (!bCopying && pChar->frametime >= fromTime)
			{
				startOffset = offset;
				bCopying = true;
			}
			if (bCopying)
			{
				memcpy(m_firstPersonSendBuffer + datasize, pPacket, pPacket->size);
				datasize += pPacket->size;
				CRY_ASSERT_MESSAGE(datasize < sizeof(m_firstPersonSendBuffer), "Ran out of memory in m_firstPersonSendBuffer");
			}
		}
		offset += pPacket->size;
	}
	// Add any other first person packets at the end
	size_t endOffset = offset;
	offset = startOffset;
	while (offset < endOffset)
	{
		SRecording_Packet* pPacket = (SRecording_Packet*)m_pBufferFirstPerson->at(offset);
		if (pPacket->type != eFPP_FPChar && pPacket->type != eFPP_KillHitPosition)
		{
			memcpy(m_firstPersonSendBuffer + datasize, pPacket, pPacket->size);
			datasize += pPacket->size;
			CRY_ASSERT_MESSAGE(datasize < sizeof(m_firstPersonSendBuffer), "Ran out of memory in m_firstPersonSendBuffer");
		}
		offset += pPacket->size;
	}
	*data = m_firstPersonSendBuffer;

	return CRecordingSystemCompressor::CompressRaw(m_firstPersonSendBuffer, datasize, m_firstPersonSendBuffer, sizeof(m_firstPersonSendBuffer));
}

size_t CRecordingSystem::GetVictimDataForTimeRange(uint8** data, EntityId victimID, float fromTime, float toTime, bool bulletTimeKill, float timeOffset)
{
	size_t offset = 0;
	size_t datasize = 0;
	bool bInRange = false;
	// Get victim's position data
	CryFixedArray<SRecording_VictimPosition, 500> victimPositions;
	ExtractVictimPositions(victimPositions, fromTime, victimID, toTime);

	if (victimPositions.size())
	{
		// Add on victim position packets
		size_t victimBytes = victimPositions.size() * sizeof(SRecording_VictimPosition);
		memcpy(m_firstPersonSendBuffer + datasize, &victimPositions.at(0), victimBytes);
		datasize += victimBytes;
		RecSysLog("Sending %" PRISIZE_T " bytes worth of victim position data", victimBytes);
	}
	else
	{
		assert(0);
	}

	// Get kill hit position
	while (offset < m_pBufferFirstPerson->size())
	{
		SRecording_Packet* pPacket = (SRecording_Packet*)m_pBufferFirstPerson->at(offset);
		if (pPacket->type == eFPP_FPChar)
		{
			SRecording_FPChar* pChar = (SRecording_FPChar*)pPacket;
			if (pChar->frametime >= toTime)
			{
				break;
			}
			else if (pChar->frametime >= fromTime)
			{
				bInRange = true;
			}
		}
		else if (pPacket->type == eFPP_KillHitPosition)
		{
			if (!bInRange)
			{
				size_t localOffset = offset + pPacket->size;
				while (localOffset < m_pBufferFirstPerson->size())
				{
					SRecording_Packet* pLocalPacket = (SRecording_Packet*)m_pBufferFirstPerson->at(localOffset);
					if (pLocalPacket->type == eFPP_FPChar)
					{
						SRecording_FPChar* pChar = (SRecording_FPChar*)pLocalPacket;
						bInRange = (pChar->frametime >= fromTime) && (pChar->frametime < toTime);
						break;
					}
					localOffset += pLocalPacket->size;
				}
			}
			if (bInRange)
			{
				SRecording_KillHitPosition* pKillHit = (SRecording_KillHitPosition*)pPacket;
				if (pKillHit->victimId == victimID)
				{
					memcpy(m_firstPersonSendBuffer + datasize, pPacket, pPacket->size);
					datasize += pPacket->size;
					CRY_ASSERT_MESSAGE(datasize < sizeof(m_firstPersonSendBuffer), "Ran out of memory in m_firstPersonSendBuffer");
					break;
				}
			}
		}
		offset += pPacket->size;
	}

	if (timeOffset != 0.0f)
	{
		SRecording_PlaybackTimeOffset packet;
		packet.timeOffset = timeOffset;
		memcpy(m_firstPersonSendBuffer + datasize, &packet, packet.size);
		datasize += packet.size;
		CRY_ASSERT_MESSAGE(datasize < sizeof(m_firstPersonSendBuffer), "Ran out of memory in m_firstPersonSendBuffer");
	}
	*data = m_firstPersonSendBuffer;

	return CRecordingSystemCompressor::CompressRaw(m_firstPersonSendBuffer, datasize, m_firstPersonSendBuffer, sizeof(m_firstPersonSendBuffer));
}

size_t CRecordingSystem::GetFirstPersonData(uint8** data, EntityId victimId, float deathTime, bool bulletTimeKill, const float length)
{
	float timeNow = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	float startTime = timeNow - length;

	// Remove all packets before the start time (i.e. 5 seconds ago)
	while (m_pBufferFirstPerson->size() > 0)
	{
		SRecording_Packet* pPacket = (SRecording_Packet*)m_pBufferFirstPerson->at(0);
		if (pPacket->type == eFPP_FPChar)
		{
			SRecording_FPChar* pFPChar = (SRecording_FPChar*)pPacket;
			if (pFPChar->frametime >= startTime)
			{
				break;
			}
		}
		m_pBufferFirstPerson->RemovePacket();
	}

	CryFixedArray<SRecording_VictimPosition, 500> victimPositions;
	if (victimId != 0)
	{
		ExtractVictimPositions(victimPositions, startTime, victimId, deathTime);
	}
	*data = m_firstPersonSendBuffer;
	return CRecordingSystemCompressor::Compress(m_pBufferFirstPerson, m_firstPersonSendBuffer, sizeof(m_firstPersonSendBuffer), victimPositions.begin(), victimPositions.end(), bulletTimeKill ? victimId : 0);
}

template<unsigned int N>
void CRecordingSystem::ExtractVictimPositions(CryFixedArray<SRecording_VictimPosition, N>& victimPositions, float startTime, EntityId victimId, float deathTime, float endTime)
{
	float frametime = 0;
	for (CRecordingBuffer::iterator itPacket = m_pBuffer->begin(); itPacket != m_pBuffer->end(); ++itPacket)
	{
		if (itPacket->type == eRBPT_FrameData)
		{
			frametime = ((SRecording_FrameData*)&*itPacket)->frametime;
		}
		else if (itPacket->type == eTPP_TPChar)
		{
			if (endTime >= 0.0f && endTime < frametime)
			{
				break;
			}
			if (frametime >= startTime)
			{
				SRecording_TPChar* pTPChar = ((SRecording_TPChar*)&*itPacket);
				if (pTPChar->eid == victimId)
				{
					SRecording_VictimPosition victimPos;
					victimPos.victimPosition = pTPChar->entitylocation.t;
					victimPos.frametime = frametime;
					victimPositions.push_back(victimPos);
				}
			}
		}
	}
	CompressVictimData(victimPositions, deathTime);
}

/*static*/ void CRecordingSystem::SetFirstPersonData(uint8* data, size_t datasize, SPlaybackInstanceData& rPlaybackInstanceData, SFirstPersonDataContainer& rFirstPersonDataContainer)
{
	CRY_ASSERT_MESSAGE(datasize <= sizeof(rFirstPersonDataContainer.m_firstPersonData), "Ran out of memory in m_firstPersonData");

#ifdef RECSYS_DEBUG
	maxFirstPersonDataSeen = max(maxFirstPersonDataSeen, datasize);
	latestFirstPersonDataSize = datasize;
#endif //RECSYS_DEBUG

	const int k_packetDataSize = CActor::KillCamFPData::DATASIZE;
	if (!rFirstPersonDataContainer.m_isDecompressed)
	{
		rFirstPersonDataContainer.m_firstPersonDataSize = CRecordingSystemCompressor::Decompress(data, datasize, rFirstPersonDataContainer.m_firstPersonData, sizeof(rFirstPersonDataContainer.m_firstPersonData), k_packetDataSize);
		rFirstPersonDataContainer.m_isDecompressed = true;
	}

	// Extract out the start time on the remote machine (this will be different to m_recordedStartTime because clocks are not synchronised)
	rPlaybackInstanceData.m_remoteStartTime = 0;
	if (rFirstPersonDataContainer.m_firstPersonDataSize > 0)
	{
		SRecording_FPChar* pFPCharData = (SRecording_FPChar*)rFirstPersonDataContainer.m_firstPersonData;
		if (pFPCharData->type == eFPP_FPChar)
		{
			rPlaybackInstanceData.m_remoteStartTime = pFPCharData->frametime;
		}
		else
		{
			CRY_ASSERT_MESSAGE(false, "The first packet in the first person data buffer should be of type eFPP_FPChar");
		}
	}

	// Compute the first person actions offset for playback later
	rPlaybackInstanceData.m_firstPersonActionsOffset = rFirstPersonDataContainer.m_firstPersonDataSize;
	rPlaybackInstanceData.m_victimPositionsOffset = rFirstPersonDataContainer.m_firstPersonDataSize;
	uint8* pPos = rFirstPersonDataContainer.m_firstPersonData;
	while (pPos < rFirstPersonDataContainer.m_firstPersonData + rFirstPersonDataContainer.m_firstPersonDataSize)
	{
		SRecording_Packet* pPacket = (SRecording_Packet*)pPos;
		if (pPacket->type == eFPP_FPChar)
		{
			// Do nothing
		}
		else if (pPacket->type == eFPP_VictimPosition)
		{
			if (rPlaybackInstanceData.m_victimPositionsOffset == rFirstPersonDataContainer.m_firstPersonDataSize)
			{
				rPlaybackInstanceData.m_victimPositionsOffset = pPos - rFirstPersonDataContainer.m_firstPersonData;
			}
		}
		else
		{
			rPlaybackInstanceData.m_firstPersonActionsOffset = pPos - rFirstPersonDataContainer.m_firstPersonData;
			break;
		}
		pPos += pPacket->size;
	}
}

void CRecordingSystem::OnBreakEvent(uint16 uBreakEventIndex)
{
	BreakLogAlways("OnBreakEvent() Index: %d", uBreakEventIndex);
	SRecording_ProceduralBreakHappened breakEvent;
	breakEvent.uBreakEventIndex = uBreakEventIndex;
	m_pBuffer->AddPacket(breakEvent);
}

void CRecordingSystem::OnPartRemoveEvent(int iPartRemovalEventIndex)
{
	BreakLogAlways("OnPartRemoveEvent(), PartRemoveEvent Index: %d", iPartRemovalEventIndex);
}

void CRecordingSystem::OnEntityDrawSlot(IEntity* pEntity, int32 slot, int32 flags)
{
	if (m_recordEntityClassFilter.count(pEntity->GetClass()))
	{
		SRecording_DrawSlotChange drawSlotChange;
		drawSlotChange.entityId = pEntity->GetId();
		drawSlotChange.iSlot = slot;
		drawSlotChange.flags = flags;
		m_pBuffer->AddPacket(drawSlotChange);
	}
}

void CRecordingSystem::ClearBreakageSpawn(IEntity* pEntity, EntityId entityId)
{
	if (pEntity->GetClass() == m_pBreakage)
	{
		if (m_recordedData.m_discardedEntitySpawns.find(entityId) == m_recordedData.m_discardedEntitySpawns.end())
		{
			// Let's pretend the entity always existed (This is to ensure trees appear in killcam before they are broken)
			for (CRecordingBuffer::iterator itPacket = m_pBuffer->begin(); itPacket != m_pBuffer->end(); ++itPacket)
			{
				if (itPacket->type == eTPP_EntitySpawn)
				{
					SRecording_EntitySpawn* pEntitySpawn = (SRecording_EntitySpawn*)&*itPacket;
					if (pEntitySpawn->entityId == entityId)
					{
						m_recordedData.m_discardedEntitySpawns[pEntitySpawn->entityId] = std::make_pair(*pEntitySpawn, gEnv->pTimer->GetFrameStartTime().GetSeconds());
						*pEntitySpawn = SRecording_EntitySpawn();   // Empty the packet in the buffer, so it will get ignored (essentially removing it)
						break;
					}
				}
			}
		}
	}
}

void CRecordingSystem::OnEntityChangeStatObj(IEntity* pEntity, int32 iBrokenObjectIndex, int32 slot, IStatObj* pOldStatObj, IStatObj* pNewStatObj)
{
	EntityId entityId = pEntity->GetId();
	ClearBreakageSpawn(pEntity, entityId);

	SRecording_StatObjChange statObjChange;

	statObjChange.entityId = entityId;
	statObjChange.iSlot = slot;
	statObjChange.pNewStatObj = pNewStatObj;

	pNewStatObj->AddRef();

	m_pBuffer->AddPacket(statObjChange);
}

void CRecordingSystem::OnSetSubObjHideMask(IEntity* pEntity, int nSlot, hidemask nSubObjHideMask)
{
	EntityId entityId = pEntity->GetId();
	ClearBreakageSpawn(pEntity, entityId);

	SRecording_SubObjHideMask hideMask;
	hideMask.entityId = entityId;
	hideMask.slot = nSlot;
	hideMask.subObjHideMask = nSubObjHideMask;

	m_pBuffer->AddPacket(hideMask);
}

void CRecordingSystem::StartRecording()
{
	if (!CRecordingSystem::KillCamEnabled())
		return;

	if (IsPlayingOrQueued())
	{
		StopPlayback();
	}

	RecSysLogFunc;

	m_pBufferFirstPerson->Reset();
	m_recordingState = eRS_Recording;
	m_streamer.ClearAllStreamData();
}

bool CRecordingSystem::QueueStartPlayback(const SPlaybackRequest& request)
{
	CCCPOINT(CRecordingSystem_StartPlayback);

	RecSysLogFunc;

	IEntity* pKiller = gEnv->pEntitySystem->GetEntity(request.kill.killerId);

#if defined(RECSYS_DEBUG)
	if (!request.highlights)
	{
		IEntity* pVictim = gEnv->pEntitySystem->GetEntity(request.kill.victimId);

		const char* killerName = pKiller ? pKiller->GetName() : "unknown";
		const char* victimName = pVictim ? pVictim->GetName() : "unknown";

		RecSysLog("QueueStartPlayback: killer: %s(%d), victim: %s(%d), projectileId: %d, hitType: %d, impulse: %f, %f, %f, deathtime: %f, bulletTimeKill: %d, pProjectileClass: %s",
		          killerName, request.kill.killerId, victimName, request.kill.victimId, request.kill.projectileId, request.kill.hitType,
		          request.kill.impulse.x, request.kill.impulse.y, request.kill.impulse.z,
		          request.kill.deathTime, (int)request.kill.bulletTimeKill,
		          request.kill.pProjectileClass ? request.kill.pProjectileClass->GetName() : "NULL");
	}
#endif

	if (IsPlayingOrQueued())
	{
		// Don't do anything if we are already playing a killcam
		RecSysLog("Playback request ignored");
		return false;
	}

	if (!CRecordingSystem::KillCamEnabled())
		return false;

	if (request.highlights)
	{
		m_highlightsReel = true;
		m_pPlaybackData = &m_pHighlightData->m_data;

		if (IForceFeedbackSystem* pForceFeedback = g_pGame->GetIGameFramework()->GetIForceFeedbackSystem())
		{
			pForceFeedback->SuppressEffects(true);
		}
	}
	else
	{
		m_highlightsReel = false;
		m_pPlaybackData = &m_recordedData;
	}

	CGameRules* pGameRules = g_pGame->GetGameRules();

	if (request.kill.winningKill)
	{
		SHUDEvent winningKillCam(eHUDEvent_OnWinningKillCam);
		winningKillCam.AddData((int)pGameRules->GetTeam(request.kill.killerId));
		CHUDEventDispatcher::CallEvent(winningKillCam);
	}
	else if (request.highlights)
	{
		SHUDEvent highlightsReel(eHUDEvent_OnHighlightsReel);
		CHUDEventDispatcher::CallEvent(highlightsReel);
	}
	else
	{
		IGameRulesRoundsModule* pRoundsModule = pGameRules->GetRoundsModule();
		if (pRoundsModule != NULL && !pRoundsModule->IsInProgress())
		{
			// if we're in a rounds-based game and this kill ended the round (eg. Defender killed last Attacker in Assault) then we don't play a Killcam at all because there's no time to do so before the end-of-round screen's meant to appear
			return false;
		}
	}

	if (!request.highlights)
	{
		// Update player's initial state rank information
		UpdateRecordedData(m_pPlaybackData);
	}

	m_nReplayEntityNumber = 0;
	m_bulletGeometryName = m_bulletEffectName = "";
	m_disableRifling = false;

	if (request.kill.pProjectileClass)
	{
		const SAmmoParams* pAmmoParams = g_pGame->GetWeaponSystem()->GetAmmoParams(request.kill.pProjectileClass);
		if (pAmmoParams)
		{
			SBulletTimeParams* pBulletTimeParams = pAmmoParams->pBulletTimeParams;
			if (pBulletTimeParams)
			{
				m_bulletGeometryName = pBulletTimeParams->geometryName.c_str();
				m_bulletEffectName = pBulletTimeParams->effectName.c_str();
				m_disableRifling = pBulletTimeParams->disableRifling;
			}
		}
	}

	m_streamer.ExpectStreamData(SKillCamExpectedData(request.kill.killerId, request.kill.victimId, request.kill.winningKill), true);

	// Notify Playback Requested.
	NotifyOnPlaybackRequested(request);

	// Store the Request data and Set the extra playback info.
	m_playInfo.Init(request, *m_pPlaybackData, *m_pBuffer);
	if (request.highlights)
	{
		m_playInfo.m_timings.Set(m_pHighlightData->m_details.m_startTime, m_pHighlightData->m_endTime, m_pHighlightData->m_details.m_kill.deathTime);
	}
	else
	{
		const float currTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		const float endTime = currTime + g_pGameCVars->kc_kickInTime;
		const float startTime = endTime - g_pGameCVars->kc_length;
		m_playInfo.m_timings.Set(startTime, endTime, currTime);
	}

	// Store the rest of the playback info. (Will be moved into m_playInfo).
	m_killer = request.kill.killerId;
	m_victim = request.kill.victimId;
	m_projectileId = 0;
	if (request.kill.pProjectileClass)
	{
		if (request.kill.pProjectileClass == m_pRocket)
		{
			m_projectileType = ePT_Rocket;
			m_projectileId = request.kill.projectileId;
		}
		else if (request.kill.pProjectileClass == m_pC4Explosive)
		{
			m_projectileType = ePT_C4;
			m_projectileId = request.kill.projectileId;
		}
		else if (request.kill.pProjectileClass == m_pExplosiveGrenade ||
		         request.kill.pProjectileClass == m_pLTagGrenade ||
		         request.kill.pProjectileClass == m_pFlashbang ||
		         request.kill.pProjectileClass == m_pGrenadeLauncherGrenade)
		{
			m_projectileType = ePT_Grenade;
			m_projectileId = request.kill.projectileId;
		}
	}

	m_playInfo.m_view.currView = SPlaybackInfo::eVM_FirstPerson;

	m_cameraSmoothing = false;
	CActor* pActor = (CActor*)g_pGame->GetIGameFramework()->GetClientActor();
	m_killerIsFriendly = pActor != NULL && pActor->IsFriendlyEntity(m_killer);
	m_newProjectilePosition.SetIdentity();

	m_hasWinningKillcam = request.kill.winningKill;
	m_playbackState = ePS_Queued;

	m_bulletTimePhase = eBTP_Approach;
	m_bulletTimeKill = request.kill.bulletTimeKill;
	m_timeSincePlaybackStart = 0.f;
	m_recordedStartTime = 0.f;
	m_C4Adjustment = -1.0f;

	return true;
}

void CRecordingSystem::UpdateRecordedData(SRecordedData* pRecordingData)
{
	// Update player's rank information to be the latest
	if (CGameLobby* pLobby = g_pGame->GetGameLobby())
	{
		for (int i = 0; i < MAX_RECORDED_PLAYERS; ++i)
		{
			SPlayerInitialState& initialState = pRecordingData->m_playerInitialStates[i];
			if (initialState.playerId != 0)
			{
				if (CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(initialState.playerId)))
				{
					uint16 channelId = pPlayer->GetChannelId();

					uint8 rank = 0, reincarnations = 0;
					pLobby->GetProgressionInfoByChannel(channelId, rank, reincarnations);

					initialState.playerJoined.rank = rank;
					initialState.playerJoined.reincarnations = reincarnations;
				}
			}
		}
	}
}

//////////////////////////////////
// RevertBrokenObjectsToStateAtTime() works out what objects changed their broken state during the
//    duration of the kill cam, then triggers CryAction to make copies of them so that they can
//		have the breaks re-played on them during the killcam
void CRecordingSystem::RevertBrokenObjectsToStateAtTime(float fTime)
{
	//Clone all of the objects and re-set them to their original state, then apply the breaks that have
	//	taken place up to fTime
	if (gEnv->pEntitySystem && gEnv->pEntitySystem->GetBreakableManager())
	{
		//Get the list of all procedural breaks that have occurred in the current kill-cam buffer, in the form of
		//	indicies into the m_brokenObjs list in CActionGame
		GetProceduralBreaksDuringKillcam(m_breakEventIndicies);

		int32 iNumProceduralBreakEvents = m_breakEventIndicies.size();

		m_renderNodeLookupStorage_original.resize(iNumProceduralBreakEvents);
		m_renderNodeLookupStorage_clone.resize(iNumProceduralBreakEvents);

		m_renderNodeLookup.UpdateStoragePointers(m_renderNodeLookupStorage_original, m_renderNodeLookupStorage_clone);

		if (iNumProceduralBreakEvents > 0)
		{
			//////////////////////////////////
			// GLASS BREAKS

			const int32 kFirstProceduralBreakEventIndex = m_breakEventIndicies[0];
			uint16* pBreakEventIndicies = &(m_breakEventIndicies[0]);

			m_clonedNodes.resize(iNumProceduralBreakEvents);

			IRenderNode** pClonedNodes = &(m_clonedNodes[0]);
			int32 iNumClonedNodes = 0;

			//Clone all of the objects and re-set them to their original state, then apply the breaks that have
			//	taken place up to fTime
			gEnv->pGameFramework->CloneBrokenObjectsAndRevertToStateAtTime(kFirstProceduralBreakEventIndex, pBreakEventIndicies, iNumProceduralBreakEvents, pClonedNodes, iNumClonedNodes, m_renderNodeLookup);

			m_breakEventIndicies.resize(iNumProceduralBreakEvents);
			m_clonedNodes.resize(iNumClonedNodes);
		}

		//Resize the render node lookup arrays to avoid memory wastage
		m_renderNodeLookupStorage_clone.resize(m_renderNodeLookup.iNumPairs);
		m_renderNodeLookupStorage_original.resize(m_renderNodeLookup.iNumPairs);

		//The resize calls could result in the position ion memory of the vector storage changing, so we need to update
		//	the render node lookup
		m_renderNodeLookup.UpdateStoragePointers(m_renderNodeLookupStorage_original, m_renderNodeLookupStorage_clone);
	}
}

//////////////////////////////////
// GetProceduralBreaksDuringKillcam() obtains a list of indices of all break events that happened
//		during the period of time that is going to be displayed during the kill cam
void CRecordingSystem::GetProceduralBreaksDuringKillcam(std::vector<uint16>& indices)
{
	uint8* startingplace = m_pTPDataBuffer;
	uint8* endingplace = startingplace + m_tpDataBufferSize;

	while (startingplace < endingplace)
	{
		SRecording_Packet* packet = (SRecording_Packet*) startingplace;
		if (packet->type == eTPP_ProceduralBreakHappened)
		{
			const SRecording_ProceduralBreakHappened* pProceduralBreak = static_cast<const SRecording_ProceduralBreakHappened*>(&*packet);
			BreakLogAlways("Procedural Break occured during killcam, break index: %d", pProceduralBreak->uBreakEventIndex);
			indices.push_back(pProceduralBreak->uBreakEventIndex);
		}

		startingplace = startingplace + packet->size;
	}
}

void CRecordingSystem::CleanUpBrokenObjects()
{
	uint16* pBreakEventIndices = m_breakEventIndicies.empty() ? NULL : &(m_breakEventIndicies[0]);
	const int32 iNumBreakEventIndices = m_breakEventIndicies.size();

	gEnv->pGameFramework->UnhideBrokenObjectsByIndex(pBreakEventIndices, iNumBreakEventIndices);

	if (gEnv->pGameFramework->GetIMaterialEffects())
	{
		gEnv->pGameFramework->GetIMaterialEffects()->ClearDelayedEffects();
	}

	const int iNumClonedNodes = m_clonedNodes.size();
	for (int i = 0; i < iNumClonedNodes; i++)
	{
		gEnv->p3DEngine->UnRegisterEntityDirect(m_clonedNodes[i]);
		gEnv->p3DEngine->DeleteRenderNode(m_clonedNodes[i]);
	}

	m_breakEventIndicies.clear();
	m_clonedNodes.clear();
	m_renderNodeLookup.Reset();
	m_renderNodeLookupStorage_original.clear();
	m_renderNodeLookupStorage_clone.clear();
}

void CRecordingSystem::StopPlayback()
{
	CCCPOINT(CRecordingSystem_StopPlayback);
	m_playbackSetupStage = ePSS_None;

	if (!CRecordingSystem::KillCamEnabled())
		return;

	RecSysLogFunc;

	// Notify Listeners.
	NotifyOnPlaybackEnd(m_playInfo);

	{
		SHUDEvent event(eHUDEvent_FadeCrosshair);
		event.AddData(SHUDEventData(1.f));
		event.AddData(SHUDEventData(0.01f));
		event.AddData(SHUDEventData(0.f));
		event.AddData(SHUDEventData(eFadeCrosshair_KillCam));
		CHUDEventDispatcher::CallEvent(event);
	}

	const char* killerName = "unknown";
	const char* victimName = "unknown";
	if (gEnv->pEntitySystem)
	{
		IEntity* pKiller = gEnv->pEntitySystem->GetEntity(m_killer);
		if (pKiller)
		{
			killerName = pKiller->GetName();
		}
		IEntity* pVictim = gEnv->pEntitySystem->GetEntity(m_victim);
		if (pVictim)
		{
			victimName = pVictim->GetName();
		}
	}
#if ENABLE_STATOSCOPE
	if (gEnv->pStatoscope)
	{
		CryFixedStringT<128> buffer;
		buffer.Format("Stop playback for victim %s from killer %s", victimName, killerName);
		gEnv->pStatoscope->AddUserMarker("KillCam", buffer.c_str());
	}
#endif // ENABLE_STATOSCOPE
	RecSysLog("Stopping replay, killer: %s(%d), victim: %s(%d)", killerName, m_killer, victimName, m_victim);

	// need to zoom out the weapon if zoomed, otherwise the hud gets a little confused
	ExitWeaponZoom();

	m_playbackState = ePS_NotPlaying;
	if (m_pPlaybackData == &m_recordedData)
	{
		m_pPlaybackData->m_FirstPersonDataContainer.Reset();
	}

	m_streamer.ClearStreamData(m_victim);
	m_playInfo.Reset();

	SetPlayBackCameraView(false);

	m_killer = 0;
	m_victim = 0;
	m_projectileId = 0;
	m_killerReplayGunId = 0;

	m_weaponFPAiming.SetActionController(NULL);

	//Remove the breakable objects that were cloned for the killcam
	CleanUpBrokenObjects();

	CleanUpDeferredRays();

	TReplayActorMap::iterator itReplayActor;
	for (itReplayActor = m_replayActors.begin(); itReplayActor != m_replayActors.end(); ++itReplayActor)
	{
		// Remove the cloned player and gun
		CReplayActor* pRA = GetReplayActor(itReplayActor->second, false);
		if (pRA)
		{
			pRA->OnRemove();
			gEnv->pEntitySystem->RemoveEntity(pRA->GetEntityId());
		}
	}
	m_replayActors.clear();

	// Remove all the replay entities
	for (TReplayEntityMap::iterator itEntity = m_replayEntities.begin(); itEntity != m_replayEntities.end(); ++itEntity)
	{
		gEnv->pEntitySystem->RemoveEntity(itEntity->second);
	}
	m_replayEntities.clear();

	if (m_bulletEntityId != 0)
	{
		gEnv->pEntitySystem->RemoveEntity(m_bulletEntityId);
		m_bulletEntityId = 0;
	}

	{
		REPLAY_EVENT_GUARD
		// Remove all the replay particles
		for (TReplayParticleMap::iterator itParticle = m_replayParticles.begin(); itParticle != m_replayParticles.end(); ++itParticle)
		{
			gEnv->pParticleManager->DeleteEmitter(itParticle->second);
			itParticle->second->Release();
		}
		m_replayParticles.clear();

		// Stop all replay sounds
		/*for (TReplaySoundMap::iterator itSound=m_replaySounds.begin(); itSound!=m_replaySounds.end(); ++itSound)
		   {
		   ISound* pSound = gEnv->pSoundSystem->GetSound(itSound->second);
		   if (pSound)
		   {
		    pSound->Stop();
		   }
		   }
		   m_replaySounds.clear();*/
	}

	// Show all the entities that should be shown.
	for (TRecEntitiesMap::iterator it = m_recordingEntities.begin(), end = m_recordingEntities.end(); it != end; ++it)
	{
		SRecordingEntity& recEnt = it->second;
		if (gEnv->pEntitySystem && recEnt.hiddenState == SRecordingEntity::eHS_Unhide)
		{
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(recEnt.entityId))
			{
				HideEntityKeepingPhysics(pEntity, false);
			}
		}
	}

	memset(m_recordedData.m_FirstPersonDataContainer.m_firstPersonData, 0, sizeof(m_recordedData.m_FirstPersonDataContainer.m_firstPersonData));

	m_killCamGameEffect.SetActive(false);
	//m_activeSoundLoop.Stop();
	//m_kvoltSoundLoop.Stop();	// Shouldn't be playing here anymore, let's stop it anyway just to make sure

	// uncache any first person weapon sounds for this killer
	ActuallyAudioCacheWeaponDetails(false);

	SHUDEvent killCamStop(eHUDEvent_OnKillCamStopPlay);
	CHUDEventDispatcher::CallEvent(killCamStop);

	//gEnv->pSoundSystem->SetPlaybackFilter(0);

	if (!m_highlightsReel)
	{
		GoIntoSpectatorMode();
	}

	SetTimeScale(1.f);
	gEnv->pParticleManager->SetTimer(gEnv->pTimer);

	SetDofFocusDistance(-1.f);

	gEnv->p3DEngine->SetPostEffectParam(NIGHT_VISION_PE, 0.0f);

	CPlayer* pPlayer = (CPlayer*)g_pGame->GetIGameFramework()->GetClientActor();
	if (pPlayer)
	{
		pPlayer->OnStopRecordingPlayback();
		// Reset all post effects including flash bang, night vision, thermal vision, etc...
		pPlayer->ResetScreenFX();
		pPlayer->ResetFPView();
		pPlayer->GetPlayerHealthGameEffect().SetKillCamData(0, 1.f, 1.f);
	}

	if (m_highlightsReel)
	{
		m_highlightsReel = false;

		if (IForceFeedbackSystem* pForceFeedback = g_pGame->GetIGameFramework()->GetIForceFeedbackSystem())
		{
			pForceFeedback->SuppressEffects(false);
		}

		if (pPlayer)
		{
			EntityId currentItemId = pPlayer->GetCurrentItemId();

			SHUDEvent event;
			event.eventType = eHUDEvent_OnItemSelected;
			event.eventIntData = currentItemId;

			CHUDEventDispatcher::CallEvent(event);

			if (!m_bShowAllHighlights)
			{
				g_pGame->GetUI()->ActivateDefaultStateImmediate();

				StartRecording();
			}
		}
	}

	if (m_bShowAllHighlights)
	{
		bool bShownNext = false;

		const float previousTime = m_highlightData[m_highlightIndex].m_endTime;
		float nextTime = FLT_MAX;
		int bestIndex = -1;
		for (int i = 0; i < MAX_HIGHLIGHTS; ++i)
		{
			if ((i != m_highlightIndex) &&
			    m_highlightData[i].m_bUsed &&
			    (m_highlightData[i].m_endTime > previousTime) &&
			    (m_highlightData[i].m_endTime < nextTime))
			{
				bestIndex = i;
				nextTime = m_highlightData[i].m_endTime;
			}
		}

		if (bestIndex != -1)
		{
			PlayHighlight(bestIndex);
			bShownNext = true;
		}

		if (!bShownNext)
		{
			if (m_bLoopHighlightsReel)
			{
				// Loop the reel
				PlayAllHighlights(true);
			}
			else
			{
				m_bShowAllHighlights = false;
			}
		}
	}

	CHUDEventDispatcher::CallEvent(SHUDEvent(eHUDEvent_OnKillCamPostStopPlay));
}

void CRecordingSystem::ExitWeaponZoom()
{
	IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
	CReplayActor* pReplayActor = GetReplayActor(m_killer, true);
	IItem* pItem = pReplayActor ? pItemSystem->GetItem(pReplayActor->GetGunId()) : NULL;
	CWeapon* pWeapon = pItem ? static_cast<CWeapon*>(pItem->GetIWeapon()) : NULL;

	if (pWeapon)
	{
		EZoomState zoomState = pWeapon->GetZoomState();
		if (zoomState == eZS_ZoomingIn || zoomState == eZS_ZoomedIn)
		{
			IZoomMode* pZoomMode = pWeapon->GetZoomMode(pWeapon->GetCurrentZoomMode());
			if (pZoomMode)
			{
				pZoomMode->ExitZoom(true);
			}
		}
	}
}

void CRecordingSystem::StopHighlightReel()
{
	m_bShowAllHighlights = false;
	if (IsPlayingOrQueued())
	{
		StopPlayback();
	}
}

void CRecordingSystem::SetTimeScale(float timeScale)
{
	if (m_timeScale == timeScale)
		return;
	const float slowMoLimit = 0.1f;   // If the time scale passes lower than this value then activate the slow motion mood
	if (timeScale < slowMoLimit && !(m_timeScale < slowMoLimit))
	{
		if (CPlayer* pClientPlayer = (CPlayer*)g_pGame->GetIGameFramework()->GetClientActor())
		{
			pClientPlayer->OnRecordingPlaybackBulletTime(true);
		}
	}
	else if (!(timeScale < slowMoLimit) && m_timeScale < slowMoLimit)
	{
		if (CPlayer* pClientPlayer = (CPlayer*)g_pGame->GetIGameFramework()->GetClientActor())
		{
			pClientPlayer->OnRecordingPlaybackBulletTime(false);
		}
	}

	m_replayTimer->SetTimeScale(timeScale);

	TReplayActorMap::iterator itActor;
	for (itActor = m_replayActors.begin(); itActor != m_replayActors.end(); ++itActor)
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(itActor->second);
		if (pEntity)
		{
			ICharacterInstance* pCharacterInstance = pEntity->GetCharacter(0);
			if (pCharacterInstance)
			{
				pCharacterInstance->SetPlaybackScale(timeScale);
			}
		}
	}

	m_timeScale = timeScale;
}

void CRecordingSystem::GoIntoSpectatorMode()
{
	if (CRecordingSystem::KillCamEnabled())
	{
		// Go into spectator mode if we are still dead
		CActor* pLocalActor = (CActor*)g_pGame->GetIGameFramework()->GetClientActor();
		if (pLocalActor != NULL && (pLocalActor->IsDead() || m_highlightsReel))
		{
			CGameRules* pGameRules = g_pGame->GetGameRules();
			IGameRulesSpectatorModule* specmod = pGameRules->GetSpectatorModule();

			EntityId specEntity = 0;
			if (m_highlightsReel && specmod->ModeIsAvailable(pLocalActor->GetEntityId(), CActor::eASM_Fixed, &specEntity))
			{
				specmod->ChangeSpectatorMode(pLocalActor, CActor::eASM_Fixed, specEntity, true, true);
			}
			else if (!m_highlightsReel)
			{
				specmod->ChangeSpectatorModeBestAvailable(pLocalActor, false);
			}

			EntityId currentItemId = pLocalActor->GetCurrentItemId();
			SHUDEvent event;
			event.eventType = eHUDEvent_OnItemSelected;
			event.eventIntData = currentItemId;
			CHUDEventDispatcher::CallEvent(event);

			g_pGame->GetUI()->ActivateDefaultStateImmediate();
		}
	}
}

void CRecordingSystem::OnPlayerFirstPersonChange(IEntity* pPlayerEntity, EntityId weaponId, bool firstPerson)
{
	m_weaponFPAiming.SetActive(firstPerson);
	SetupPlayerCharacterVisibility(pPlayerEntity, !firstPerson, CReplayActor::ShadowCharacterSlot);
	CReplayActor* pReplayActor = CReplayActor::GetReplayActor(pPlayerEntity);
	if (pReplayActor)
	{
		pReplayActor->SetFirstPerson(firstPerson);
	}

	if (CItem* pItem = (CItem*)g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(weaponId))
	{
		CWeapon* pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon());
		if (pWeapon)
		{
			pWeapon->SetOwnerClientOverride(firstPerson);
		}
		if (firstPerson)
			pItem->OnEnterFirstPerson();
		else
			pItem->OnEnterThirdPerson();
	}

	if (firstPerson)
	{
		m_torsoAimIK.Enable(true);
	}
	else
	{
		m_torsoAimIK.Disable(true);
	}

	{
		SHUDEvent event(eHUDEvent_FadeCrosshair);
		event.AddData(SHUDEventData(firstPerson ? 1.f : 0.f));
		event.AddData(SHUDEventData(0.3f));
		event.AddData(SHUDEventData(0.f));
		event.AddData(SHUDEventData(eFadeCrosshair_KillCam));
		CHUDEventDispatcher::CallEvent(event);
	}
}

void CRecordingSystem::ApplyWeaponShoot(const SRecording_OnShoot* pShoot)
{
	// Prevent the killer from firing more bullets while in bullet time mode
	// otherwise we get tracer fire shooting past the bullet which ruins the effect
	IEntity* pReplayEntity = GetReplayEntity(pShoot->weaponId);
	if (pReplayEntity)
	{
		IGameFramework* pGameFramework = gEnv->pGameFramework;
		IItemSystem* pItemSystem = pGameFramework->GetIItemSystem();
		IItem* pItem = pItemSystem->GetItem(pReplayEntity->GetId());
		if (pItem)
		{
			IEntity* pFiringReplayEntity = GetReplayEntity(pItem->GetOwnerId());
			if (m_playInfo.m_view.currView == SPlaybackInfo::eVM_BulletTime || !pFiringReplayEntity || pFiringReplayEntity->GetId() != m_killer)
			{
				CWeapon* pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon());
				if (pWeapon)
				{
					IFireMode* pFireMode = pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
					if (pFireMode)
					{
						pFireMode->ReplayShoot();

						if (!pFiringReplayEntity && m_playInfo.m_view.currView == SPlaybackInfo::eVM_FirstPerson)
						{
							if (const SFireModeParams* pParams = (static_cast<CFireMode*>(pFireMode))->GetShared())
							{
								if (IEntity* pKiller = gEnv->pEntitySystem->GetEntity(pItem->GetOwnerId()))
								{
									if (ICharacterInstance* pCharInst = pKiller->GetCharacter(0))
									{
										if (ISkeletonPose* pPose = pCharInst->GetISkeletonPose())
										{
											pPose->ApplyRecoilAnimation(
											  pParams->proceduralRecoilParams.duration,
											  0.f,
											  pParams->proceduralRecoilParams.kickIn,
											  pParams->proceduralRecoilParams.arms == 1 || pParams->proceduralRecoilParams.arms == 2 ? pParams->proceduralRecoilParams.arms : 3);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void CRecordingSystem::ApplyWeaponAccessories(const SRecording_WeaponAccessories* weaponAccessories)
{
	CItem* pItem = GetReplayItem(weaponAccessories->weaponId);
	if (pItem)
	{
		pItem->RemoveAllAccessories();
		for (int i = 0; i < MAX_WEAPON_ACCESSORIES; i++)
		{
			IEntityClass* pAccessoryClass = weaponAccessories->pAccessoryClasses[i];
			if (pAccessoryClass)
			{
				pItem->AttachAccessory(pAccessoryClass, true, true, true);
			}
		}
		// If the item is hidden make sure the accessories also get hidden
		IEntity* pItemEnt = pItem->GetEntity();
		HideItem(pItemEnt, pItemEnt->IsHidden());
	}
}

void CRecordingSystem::ApplyWeaponSelect(const SRecording_WeaponSelect* weaponSelect)
{
	IEntity* pReplayEntity = NULL;
	CReplayActor* pReplayActor = NULL;
	TReplayActorMap::iterator find_iterator = m_replayActors.find(weaponSelect->ownerId);
	if (find_iterator != m_replayActors.end())
	{
		pReplayActor = GetReplayActor(find_iterator->second, false);
		pReplayEntity = pReplayActor ? pReplayActor->GetEntity() : 0;
	}
	else
	{
		TReplayEntityMap::iterator itReplayEntity = m_replayEntities.find(weaponSelect->ownerId);
		if (itReplayEntity != m_replayEntities.end())
		{
			pReplayEntity = gEnv->pEntitySystem->GetEntity(itReplayEntity->second);
		}
	}

	if (pReplayActor != NULL && pReplayActor->GetGunId() != 0)
	{
		if (!weaponSelect->isSelected)
		{
			TReplayEntityMap::iterator itReplayEntity = m_replayEntities.find(weaponSelect->weaponId);
			if (itReplayEntity != m_replayEntities.end())
			{
				EntityId replayGunId = itReplayEntity->second;
				if (replayGunId != pReplayActor->GetGunId())
				{
					// Packet is in the wrong order, ignore it
					return;
				}
			}
		}

		IItem* pPrevItem = g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pReplayActor->GetGunId());
		if (pPrevItem)
		{
			// Deselect the previous weapon
			pPrevItem->Select(false);
			pReplayActor->SetGunId(0);
		}
	}

	if (pReplayEntity && weaponSelect->weaponId && weaponSelect->isSelected)
	{
		IEntity* pItemEnt = GetReplayEntity(weaponSelect->weaponId);
		if (pItemEnt)
		{
			IWeapon* pIWeapon = NULL;
			EntityId itemEntId = pItemEnt->GetId();
			IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
			IItem* pItem = pItemSystem->GetItem(itemEntId);
			if (pItem)
			{
				CItem* cItem = (CItem*) pItem;
				if (weaponSelect->isRippedOff)
				{
					if (m_mountedWeaponClasses.count(pItemEnt->GetClass()))
					{
						CHeavyMountedWeapon* pHmg = (CHeavyMountedWeapon*)pItem;
						pHmg->FinishRipOff();
						pHmg->UnlinkMountedGun();
					}
				}
				cItem->StartUse(pReplayEntity->GetId());
				if (cItem->GetOwnerId() != pReplayEntity->GetId())
				{
					cItem->SetOwnerId(pReplayEntity->GetId());
				}
				if (pReplayActor)
				{
					pReplayActor->AddItem(itemEntId);
				}
				//cItem->AttachToHand(true);
				cItem->Select(true);
				if (weaponSelect->ownerId == m_killer)
				{
					if (m_playInfo.m_view.currView == SPlaybackInfo::eVM_FirstPerson)
					{
						pIWeapon = pItem->GetIWeapon();
						CWeapon* pWeapon = static_cast<CWeapon*>(pIWeapon);
						if (pWeapon)
						{
							pWeapon->SetCurrentFireMode("Single");
							pWeapon->SetOwnerClientOverride(true);
						}
						cItem->OnEnterFirstPerson();
					}
					m_killerReplayGunId = itemEntId;
				}

				cItem->CloakSync(false);

				// Make sure we set busy false so that we are able to start zooming if necessary
				cItem->SetBusy(false);
			}

			if (weaponSelect->ownerId == m_killer && m_playInfo.m_view.currView == SPlaybackInfo::eVM_FirstPerson)
			{
				SHUDEventWrapper::FireModeChanged(pIWeapon, pIWeapon ? pIWeapon->GetCurrentFireMode() : -1);
			}

			//pItemEnt->GetCharacter(0)->GetISkeletonAnim()->SetDebugging(1);

			// Remove the previous gun and update the id
			if (pReplayActor)
			{
				pReplayActor->SetGunId(itemEntId);
			}

			if (weaponSelect->ownerId == m_killer)
			{
				SHUDEvent weaponSwitch(eHUDEvent_OnKillCamWeaponSwitch);
				weaponSwitch.AddData((int)itemEntId);
				weaponSwitch.AddData(false);
				CHUDEventDispatcher::CallEvent(weaponSwitch);
			}
		}
		else
		{
			RecSysLog("ApplyWeaponSelect Unable to select weapon, replay weapon with id %d not found! ownerId = %d, isRippedOff = %d", weaponSelect->weaponId, weaponSelect->ownerId, (int)weaponSelect->isRippedOff);
			CRY_ASSERT_MESSAGE(false, "Unable to select weapon, replay weapon not found");
		}
	}
}

void CRecordingSystem::ApplyFiremodeChanged(const SRecording_FiremodeChanged* firemodeChanged)
{
	if (firemodeChanged->ownerId == m_killer && m_playInfo.m_view.currView == SPlaybackInfo::eVM_FirstPerson)
	{
		IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
		IItem* pItem = pItemSystem->GetItem(firemodeChanged->weaponId);
		if (pItem)
		{
			SHUDEventWrapper::FireModeChanged(pItem->GetIWeapon(), firemodeChanged->firemode);
		}
	}
}

void CRecordingSystem::RegisterReplayMannListener(CReplayMannListener& listener)
{
	m_mannequinListeners.push_back(&listener);
}

void CRecordingSystem::UnregisterReplayMannListener(const EntityId entityId)
{
	for (std::vector<CReplayMannListener*>::iterator itListener = m_mannequinListeners.begin(); itListener != m_mannequinListeners.end(); ++itListener)
	{
		if ((*itListener)->GetEntityID() == entityId)
		{
			m_mannequinListeners.erase(itListener);
			return;
		}
	}
}

void CRecordingSystem::ApplyMannequinEvent(const SRecording_MannEvent* pMannEvent, float animTime)
{
	CReplayActor* pReplayActor = GetReplayActor(pMannEvent->eid, true);
	if (pReplayActor)
	{
		pReplayActor->ApplyMannequinEvent(pMannEvent->historyItem, animTime);
	}
}

void CRecordingSystem::ApplyMannequinSetSlaveController(const SRecording_MannSetSlaveController* pMannSetSlave)
{
	CReplayActor* pMasterActor = GetReplayActor(pMannSetSlave->masterActorId, true);
	CReplayActor* pSlaveActor = GetReplayActor(pMannSetSlave->slaveActorId, true);

	if (pMasterActor && pSlaveActor)
	{
		IActionController* pMasterActionController = pMasterActor->GetActionController();
		IActionController* pSlaveActionController = pSlaveActor->GetActionController();

		if (pMasterActionController && pSlaveActionController)
		{
			const IAnimationDatabaseManager& animationDatabase = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager();
			const IAnimationDatabase* piOptionalDatabase = animationDatabase.FindDatabase(pMannSetSlave->optionalDatabaseFilenameCRC);

			pMasterActionController->SetSlaveController(*pSlaveActionController, pMannSetSlave->targetContext, pMannSetSlave->enslave, piOptionalDatabase);
		}
	}
	else if (pMasterActor)
	{
		IActionController* pMasterActionController = pMasterActor->GetActionController();

		if (pMannSetSlave->enslave)
		{
			IEntity* pSlaveEntity = GetReplayEntity(pMannSetSlave->slaveActorId);
			if (pMasterActionController && pSlaveEntity)
			{
				ICharacterInstance* pSlaveCharacterInstance = pSlaveEntity->GetCharacter(0);
				if (pSlaveCharacterInstance)
				{
					const IAnimationDatabaseManager& animationDatabase = gEnv->pGameFramework->GetMannequinInterface().GetAnimationDatabaseManager();
					const IAnimationDatabase* piOptionalDatabase = animationDatabase.FindDatabase(pMannSetSlave->optionalDatabaseFilenameCRC);

					pMasterActionController->SetScopeContext(pMannSetSlave->targetContext, *pSlaveEntity, pSlaveCharacterInstance, piOptionalDatabase);
				}
			}
		}
		else
		{
			pMasterActionController->ClearScopeContext(pMannSetSlave->targetContext);
		}
	}
}

void CRecordingSystem::ApplyMannequinSetParam(const SRecording_MannSetParam* pMannSetParam)
{
	if (IActionController* pActionController = GetActionController(pMannSetParam->entityId))
	{
		pActionController->SetParam(pMannSetParam->paramNameCRC, pMannSetParam->quat);
	}
}

void CRecordingSystem::ApplyMannequinSetParamFloat(const SRecording_MannSetParamFloat* pMannSetParamFloat)
{
	if (IActionController* pActionController = GetActionController(pMannSetParamFloat->entityId))
	{
		pActionController->SetParam(pMannSetParamFloat->paramNameCRC, pMannSetParamFloat->param);
	}
}

IActionController* CRecordingSystem::GetActionController(EntityId entityId)
{
	if (CReplayActor* pActor = GetReplayActor(entityId, true))
	{
		return pActor->GetActionController();
	}
	else if (CReplayActor* pActor2 = GetReplayActor(entityId, false))
	{
		return pActor2->GetActionController();
	}

	return NULL;
}

void CRecordingSystem::HideEntityKeepingPhysics(IEntity* pEntity, bool hide)
{
	REPLAY_EVENT_GUARD;
	RecSysLogDebug(eRSD_Visibility, "%s [%d: %s]", hide ? "HideKeepPhys" : "ShowKeepPhys", pEntity->GetId(), pEntity->GetName());

	SEntityEvent event(hide ? ENTITY_EVENT_INVISIBLE : ENTITY_EVENT_VISIBLE);
	event.nParam[0] = 1; // 1 tels physics to ignore it.
	pEntity->SendEvent(event);

	// Propagate invisible flag to the child entities
	const int childCount = pEntity->GetChildCount();
	for (int i = 0; i < childCount; i++)
	{
		IEntity* pChild = pEntity->GetChild(i);
		if (pChild)
		{
			IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pChild->GetId());
			if (!pActor || !pActor->IsPlayer())   // Players are explicitly dealt with elsewhere
			{
				HideEntityKeepingPhysics(pChild, hide);
			}
			else
			{
				RecSysLogDebug(eRSD_Visibility, "Not %s child [%d: %s]", hide ? "hiding" : "showing", pChild->GetId(), pChild->GetName());
			}
		}
	}
}

bool CRecordingSystem::GetWeaponAudioCacheDetailsForWeaponId(EntityId inWeaponId, SWeaponAudioCacheDetails& outDetails)
{
	bool foundWeapon = false;

	outDetails.Clear();

	outDetails.weaponId = inWeaponId;

	// search discardedEntitySpawns for this weapon being created
	TEntitySpawnMap::iterator itDiscardedSpawn = m_pPlaybackData->m_discardedEntitySpawns.find(inWeaponId);
	if (itDiscardedSpawn != m_pPlaybackData->m_discardedEntitySpawns.end())
	{
		SRecording_EntitySpawn* pEntitySpawn = &itDiscardedSpawn->second.first;
		CRY_ASSERT(pEntitySpawn->entityId == inWeaponId);
		if (pEntitySpawn->entityId == inWeaponId)
		{
			// found this weapon spawn
			outDetails.pWeaponClass = pEntitySpawn->pClass;
			foundWeapon = true;
		}
	}
	else
	{
		// we failed to find this weapon being spawned in the discarded entity spawns. now search directly within our active recorded buffer
		uint8* pPos = m_pPlaybackData->m_tpdatabuffer;
		uint8* pEnd = m_pPlaybackData->m_tpdatabuffer + m_pPlaybackData->m_tpdatasize;
		while (pPos < pEnd)
		{
			const SRecording_Packet* pPacket = (SRecording_Packet*)pPos;
			if (pPacket->type == eTPP_EntitySpawn)
			{
				const SRecording_EntitySpawn* pEntitySpawn = static_cast<const SRecording_EntitySpawn*>(pPacket);
				if (pEntitySpawn->entityId == inWeaponId)
				{
					// found this weapon spawn
					outDetails.pWeaponClass = pEntitySpawn->pClass;
					foundWeapon = true;
					break;
				}
			}
			pPos += pPacket->size;
		}
	}

	if (foundWeapon)
	{
		// we've found our weapon. Now search for our weapon accessories
		TWeaponAccessoryMap::iterator itDiscardedAccessory = m_pPlaybackData->m_discardedWeaponAccessories.find(inWeaponId);
		if (itDiscardedAccessory != m_pPlaybackData->m_discardedWeaponAccessories.end())
		{
			SRecording_WeaponAccessories* pWeaponAccessory = &itDiscardedAccessory->second;
			CRY_ASSERT(pWeaponAccessory->weaponId == inWeaponId);
			for (int i = 0; i < MAX_WEAPON_ACCESSORIES; i++)
			{
				outDetails.pAccessoryClasses[i] = pWeaponAccessory->pAccessoryClasses[i];
			}
		}
		else
		{
			// we failed to find this weapons accessories in the discarded weapon accessories, now search directly within our active playback buffer
			uint8* pPos = m_pPlaybackData->m_tpdatabuffer;
			uint8* pEnd = m_pPlaybackData->m_tpdatabuffer + m_pPlaybackData->m_tpdatasize;
			while (pPos < pEnd)
			{
				const SRecording_Packet* pPacket = (SRecording_Packet*)pPos;
				if (pPacket->type == eTPP_WeaponAccessories)
				{
					const SRecording_WeaponAccessories* pWeaponAccessory = static_cast<const SRecording_WeaponAccessories*>(pPacket);
					if (pWeaponAccessory->weaponId == inWeaponId)
					{
						// found this weapon's accessories
						for (int i = 0; i < MAX_WEAPON_ACCESSORIES; i++)
						{
							outDetails.pAccessoryClasses[i] = pWeaponAccessory->pAccessoryClasses[i];
						}
						break;
					}
				}
				pPos += pPacket->size;
			}
		}
	}

	return foundWeapon;
}

void CRecordingSystem::ActuallyAudioCacheWeaponDetails(bool enable)
{
	int len = m_cachedWeaponAudioDetails.size();
	for (int i = 0; i < len; i++)
	{
		SWeaponAudioCacheDetails& details = m_cachedWeaponAudioDetails[i];

		// handle weapon (aliasing required as some classes are grouped together and handled as one parent class
		IEntityClass* pClass = details.pWeaponClass;
		g_pGame->GetWeaponSystem()->GetWeaponAlias().UpdateClass(&pClass);
		CItem::AudioCacheItem(enable, pClass, "", "_fp");

		// handle accessories
		for (int j = 0; j < MAX_WEAPON_ACCESSORIES; j++)
		{
			const IEntityClass* pAccClass = details.pAccessoryClasses[j];
			if (pAccClass)
			{
				CItem::AudioCacheItem(enable, pAccClass, "att_", "_fp");
			}
		}
	}
}

void CRecordingSystem::HandleAudioCachingOfKillersWeapons()
{
	m_cachedWeaponAudioDetails.clear();

	// work on initial weapon
	for (int i = 0; i < MAX_RECORDED_PLAYERS; ++i)
	{
		SPlayerInitialState& initialState = m_pPlaybackData->m_playerInitialStates[i];
		if (initialState.playerId == m_killer)
		{
			CRY_ASSERT(initialState.weapon.ownerId == m_killer);
			initialState.weapon.weaponId;

			SWeaponAudioCacheDetails details;
			bool success = GetWeaponAudioCacheDetailsForWeaponId(initialState.weapon.weaponId, details);

			if (success)
			{
				m_cachedWeaponAudioDetails.push_back(details);
			}

			break;
		}
	}

	// work on any weapon selects during the recording
	uint8* pPos = m_pPlaybackData->m_tpdatabuffer;
	uint8* pEnd = m_pPlaybackData->m_tpdatabuffer + m_pPlaybackData->m_tpdatasize;
	while (pPos < pEnd)
	{
		const SRecording_Packet* pPacket = (SRecording_Packet*)pPos;
		if (pPacket->type == eTPP_WeaponSelect)
		{
			const SRecording_WeaponSelect* pWeaponSelect = static_cast<const SRecording_WeaponSelect*>(pPacket);
			if (pWeaponSelect->ownerId == m_killer)
			{
				// found the killer performing a select.
				bool doIt = true;

				// ensure we haven't already cached this weapon
				for (unsigned int i = 0; i < m_cachedWeaponAudioDetails.size(); i++)
				{
					if (m_cachedWeaponAudioDetails[i].weaponId == pWeaponSelect->weaponId)
					{
						doIt = false;
						break;
					}
				}

				if (doIt)
				{
					// haven't already cached this weapon so add it now
					SWeaponAudioCacheDetails details;
					bool success = GetWeaponAudioCacheDetailsForWeaponId(pWeaponSelect->weaponId, details);
					if (success)
					{
						m_cachedWeaponAudioDetails.push_back(details);
					}
				}
			}
		}
		pPos += pPacket->size;
	}

	// we now have all the details of all weapons active by the killer during the recording
	ActuallyAudioCacheWeaponDetails(true);
}

void CRecordingSystem::OnPlaybackStart(void)
{
	CGameRules* pGameRules = g_pGame->GetGameRules();

	REPLAY_EVENT_GUARD

#if ENABLE_STATOSCOPE
	if (m_playbackSetupStage == ePSS_FIRST)
	{
		const char* killerName = "unknown";
		const char* victimName = "unknown";
		if (gEnv->pEntitySystem)
		{
			if (IEntity* pKiller = gEnv->pEntitySystem->GetEntity(m_playInfo.m_request.kill.killerId))
			{
				killerName = pKiller->GetName();
			}

			if (IEntity* pVictim = gEnv->pEntitySystem->GetEntity(m_playInfo.m_request.kill.victimId))
			{
				victimName = pVictim->GetName();
			}
		}

		if (gEnv->pStatoscope)
		{
			CryFixedStringT<128> buffer;
			buffer.Format("Start playback for victim %s from killer %s", victimName, killerName);
			gEnv->pStatoscope->AddUserMarker("KillCam", buffer.c_str());
		}
	}
#endif // ENABLE_STATOSCOPE

	switch (m_playbackSetupStage)
	{
	case ePSS_AudioAndTracers:
		{
			RecSysLog("OnPlaybackStart: ePSS_AudioAndTracers\n");

			m_playInfo.m_timings.ApplyOffset(GetPlaybackTimeOffset());

#ifdef RECSYS_DEBUG
			float liveBufferLength = GetLiveBufferLenSeconds();
#endif //RECSYS_DEBUG
			if (!m_highlightsReel)
			{
				GetTPCameraData(m_playInfo.m_timings.recStart);
			}
			m_pTPDataBuffer = m_pPlaybackData->m_tpdatabuffer;
			m_tpDataBufferSize = m_pPlaybackData->m_tpdatasize;

#ifdef RECSYS_DEBUG
			if (m_pDebug)
			{
				m_pDebug->PrintThirdPersonPacketData(m_pTPDataBuffer, m_tpDataBufferSize, "TP Playback Buffer");
			}
			RecSysLog("First person camera data (%d bytes)", m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonDataSize);
			RecSysLog("Third person data (%d bytes, %.3f seconds)...", m_tpDataBufferSize, GetBufferLenSeconds());
#endif //RECSYS_DEBUG

			if (m_bulletTimeKill)
			{
				SetTimeScale(1.f);
				gEnv->pParticleManager->SetTimer(m_replayTimer.get());

				AdjustDeathTime();
				ExtractBulletPositions();
			}

			m_torsoAimIK.Reset();
			m_torsoAimIK.Enable();

			CPlayer* pClientPlayer = (CPlayer*)g_pGame->GetIGameFramework()->GetClientActor();
			if (pClientPlayer)
			{
				// Reset all post effects including flash bang, night vision, thermal vision, etc...
				pClientPlayer->ResetScreenFX();
			}

			// Remove any existing tracers
			g_pGame->GetWeaponSystem()->GetTracerManager().ClearCurrentActiveTracers();

			// Filter out all sounds except replay & ambient sounds
			//Takes approx 0.5ms

			//gEnv->pSoundSystem->SetPlaybackFilter((int)(eSoundSemantic_Replay | m_ambientSoundSemantics));

			HandleAudioCachingOfKillersWeapons();

			// Help hide entity poping
			m_killCamGameEffect.SetCurrentMode(CKillCamGameEffect::eKCEM_KillCam);
			m_killCamGameEffect.SetActive(true);

			// The health effect needs to be restarted in order to show any effects that the killer was experiencing.
			if (pClientPlayer)
			{
				pClientPlayer->GetPlayerHealthGameEffect().SetKillCamData(m_killer, CKillCamGameEffect::GetBaseBrightness(CKillCamGameEffect::eKCEM_KillCam), CKillCamGameEffect::GetBaseContrast(CKillCamGameEffect::eKCEM_KillCam));
				pClientPlayer->GetPlayerHealthGameEffect().ReStart();
			}

			// remove the existing hud straight away, it will be reshown when we have data
			g_pGame->GetUI()->ActivateStateImmediate("no_hud");

			break;
		}
	case ePSS_HideExistingEntities:
		{
			RecSysLog("OnPlaybackStart: ePSS_HideExistingEntities\n");
			//This takes approx 4ms
			// Hide all the recorded entities
			IEntitySystem* pEntSys = gEnv->pEntitySystem;
			for (TRecEntitiesMap::iterator it = m_recordingEntities.begin(), end = m_recordingEntities.end(); it != end; ++it)
			{
				SRecordingEntity& recEnt = it->second;
				RecSysLogDebug(eRSD_Visibility, "RecordingEntity: [%s: %d]", (recEnt.hiddenState == SRecordingEntity::eHS_Hide) ? "Hidden" : "Showin", recEnt.entityId);
				if (recEnt.hiddenState == SRecordingEntity::eHS_Unhide)
				{
					if (IEntity* pEntity = pEntSys->GetEntity(recEnt.entityId))
					{
						HideEntityKeepingPhysics(pEntity, true);
					}
				}
			}
			break;
		}
	case ePSS_SpawnNewEntities:
		{
			RecSysLog("OnPlaybackStart: ePSS_SpawnNewEntities\n");
			// Spawn any entities that were created before the start of the playback
			//	This takes ~20ms
			const int numCorpses = m_pPlaybackData->m_corpses.size();
			for (TEntitySpawnMap::iterator it = m_pPlaybackData->m_discardedEntitySpawns.begin(), end = m_pPlaybackData->m_discardedEntitySpawns.end(); it != end; ++it)
			{
				const SRecording_EntitySpawn* pEntitySpawn = &it->second.first;

				if (pEntitySpawn->pClass != m_pCorpse)
				{
					ApplyEntitySpawn(pEntitySpawn, m_playInfo.m_timings.recStart - it->second.second);
				}
				else
				{
					// If this entity is a corpse, we have to do the spawn a bit differently
					for (int i = 0; i < numCorpses; ++i)
					{
						STrackedCorpse& corpse = m_pPlaybackData->m_corpses[i];
						if (corpse.m_corpseId == pEntitySpawn->entityId)
						{
							if (IEntity* pCorpseEntity = SpawnCorpse(corpse.m_corpseId, pEntitySpawn->entitylocation.t, pEntitySpawn->entitylocation.q, pEntitySpawn->entityScale))
							{
								CRY_ASSERT(pEntitySpawn->szCharacterSlot[0]);
								if (pEntitySpawn->szCharacterSlot[0])
								{
									pCorpseEntity->LoadCharacter(0, pEntitySpawn->szCharacterSlot[0]);
									pCorpseEntity->SetFlags(pCorpseEntity->GetFlags() | (ENTITY_FLAG_CASTSHADOW));

									if (ICharacterInstance* pChar = pCorpseEntity->GetCharacter(0))
									{
										if (ISkeletonAnim* pAnim = pChar->GetISkeletonAnim())
										{
											if (ISkeletonPose* pPose = pChar->GetISkeletonPose())
											{
												int32 numJoints = corpse.m_numJoints;
												IAnimationOperatorQueuePtr poseModifier;
												CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), poseModifier);

												IAnimationPoseModifierPtr modPtr = poseModifier;
												pAnim->PushPoseModifier(1, modPtr, "RecordingSystem");

												for (int32 k = 0; k < numJoints; ++k)
												{
													poseModifier->PushPosition(k, IAnimationOperatorQueue::eOp_Override, corpse.m_jointAbsPos[k].t);
													poseModifier->PushOrientation(k, IAnimationOperatorQueue::eOp_Override, corpse.m_jointAbsPos[k].q);
												}
											}
										}
									}
								}
							}
							break;
						}
					}
				}
			}
			break;
		}

	case ePSS_HandleParticles:
		{
			RecSysLog("OnPlaybackStart: ePSS_HandleParticles\n");
			//This takes approx 0.5ms
			TWeaponAccessoryMap::iterator itWeaponAccessory;
			for (itWeaponAccessory = m_pPlaybackData->m_discardedWeaponAccessories.begin(); itWeaponAccessory != m_pPlaybackData->m_discardedWeaponAccessories.end(); ++itWeaponAccessory)
			{
				ApplyWeaponAccessories(&itWeaponAccessory->second);
			}

			// Remove all temporary particle emitters
			gEnv->pParticleManager->DeleteEmitters([](IParticleEmitter& e) { return e.GetSpawnParams().bNowhere; });

			// Spawn any particles that were created before the start of the playback	0.075ms
			TParticleCreatedMap::iterator itParticleSpawn;
			for (itParticleSpawn = m_pPlaybackData->m_discardedParticleSpawns.begin(); itParticleSpawn != m_pPlaybackData->m_discardedParticleSpawns.end(); )
			{
				// TODO: Should set the age of the particle effect as well
				if (ApplyParticleCreated(&itParticleSpawn->second.first, m_playInfo.m_timings.recStart - itParticleSpawn->second.second))
				{
					++itParticleSpawn;
				}
				else
				{
					// If the particle effect failed to spawn then remove it from the list to avoid the list
					// growing with invalid particle effects
					RecSysLogDebug(eRSD_Particles, "Failed to apply particle spawn packet, entityId: %d, effect name: %s", itParticleSpawn->second.first.entityId, itParticleSpawn->second.first.pParticleEffect->GetName());
					TParticleCreatedMap::iterator itErase = itParticleSpawn;
					++itParticleSpawn;
					m_pPlaybackData->m_discardedParticleSpawns.erase(itErase);
				}
			}
			break;
		}
	case ePSS_SpawnRecordingSystemActors:
		{
			RecSysLog("OnPlaybackStart: ePSS_SpawnRecordingSystemActors\n");
			// Create clones of the players so that we can do what we want with them.
			// Takes 22ms
			static IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("ReplayActor");
			assert(pEntityClass);
			int killerTeam = pGameRules->GetTeam(m_killer);

			for (int i = 0; i < MAX_RECORDED_PLAYERS; ++i)
			{
				SPlayerInitialState& initialState = m_pPlaybackData->m_playerInitialStates[i];
				if (initialState.playerId && initialState.playerJoined.bIsPlayer)
				{
					const EntityId playerId = initialState.playerId;

					CryFixedStringT<32> name;
					name.Format("ReplayActor_%u", playerId);

					SEntitySpawnParams params;
					params.pClass = pEntityClass;
					params.sName = name.c_str();

					params.vPosition.Set(0.f, 0.f, 0.f);
					params.qRotation.SetIdentity();

					params.nFlags |= (ENTITY_FLAG_NEVER_NETWORK_STATIC | ENTITY_FLAG_CLIENT_ONLY);

					IEntity* pCloneEntity = gEnv->pEntitySystem->SpawnEntity(params, true);
					if (pCloneEntity)
					{
						const EntityId cloneEntityId = pCloneEntity->GetId();
						const char* pModelName = initialState.changedModel.pModelName;
						if (pModelName)
						{
							pCloneEntity->LoadCharacter(0, pModelName);
							if ((m_killer == playerId) && (initialState.changedModel.pShadowName))
							{
								pCloneEntity->LoadCharacter(CReplayActor::ShadowCharacterSlot, initialState.changedModel.pShadowName);
								ICharacterInstance* pShadowCharacter = pCloneEntity->GetCharacter(CReplayActor::ShadowCharacterSlot);
								if (pShadowCharacter)
								{
									ISkeletonPose* pSkeletonPose = pShadowCharacter->GetISkeletonPose();
									IAnimationPoseBlenderDir* pIPoseBlenderAim = pSkeletonPose->GetIPoseBlenderAim();
									if (pIPoseBlenderAim)
									{
										pIPoseBlenderAim->SetPolarCoordinatesSmoothTimeSeconds(0.f);
										pIPoseBlenderAim->SetFadeoutAngle(DEG2RAD(180));
									}
									ISkeletonAnim* pSkeletonAnim = pShadowCharacter->GetISkeletonAnim();
									if (pSkeletonAnim)
									{
										pSkeletonAnim->SetAnimationDrivenMotion(1);
									}
								}
							}

							pCloneEntity->SetFlags(pCloneEntity->GetFlags() | (ENTITY_FLAG_CASTSHADOW));

							{
								ICharacterInstance* ici = pCloneEntity->GetCharacter(0);
								ISkeletonPose* isp = ici->GetISkeletonPose();
								IAnimationPoseBlenderDir* pIPoseBlenderAim = isp->GetIPoseBlenderAim();
								if (pIPoseBlenderAim)
								{
									pIPoseBlenderAim->SetPolarCoordinatesSmoothTimeSeconds(0.f);
									pIPoseBlenderAim->SetFadeoutAngle(DEG2RAD(180));
								}

								ISkeletonAnim* isa = ici->GetISkeletonAnim();
								if (isa)
								{
									isa->SetAnimationDrivenMotion(1);
								}
							}

							CRecordingSystem::RemoveEntityPhysics(*pCloneEntity);

							const int team = initialState.teamChange.entityId ? initialState.teamChange.teamId : 0;
							const bool isFriendly = pGameRules->GetTeamCount() == 0 ? (playerId == gEnv->pGameFramework->GetClientActorId()) : team == m_playInfo.m_request.kill.localPlayerTeam;

							typedef CryFixedStringT<DISPLAY_NAME_LENGTH> TPlayerDisplayName;
							TPlayerDisplayName playerDisplayname = initialState.playerJoined.displayName;

							// get rank and reincarnation level
							int rank = (int) initialState.playerJoined.rank;
							int reincarnations = (int) initialState.playerJoined.reincarnations;

							class IActionController* pActionController = NULL;
							// Setup the replay actor's data.
							{
								CReplayActor* pReplayActor = GetReplayActor(cloneEntityId, false);
								pReplayActor->SetTeam(team, isFriendly);
								pReplayActor->SetFirstPerson(false);
								pReplayActor->SetRank(rank);
								pReplayActor->SetReincarnations(reincarnations);
								pReplayActor->SetIsOnClientsSquad(initialState.playerJoined.bSameSquad);
								pReplayActor->SetIsClient(initialState.playerJoined.bIsClient);
								pReplayActor->SetName(playerDisplayname);

								pReplayActor->SetupActionController(initialState.playerJoined.pControllerDef, initialState.playerJoined.pAnimDB1P, initialState.playerJoined.pAnimDB3P);
								pActionController = pReplayActor->GetActionController();

								CTeamVisualizationManager* pTeamVisManager = pGameRules->GetTeamVisualizationManager();
								if (pTeamVisManager)
								{
									pTeamVisManager->RefreshTeamMaterial(pCloneEntity, true, isFriendly);
								}

								m_replayActors[playerId] = pCloneEntity->GetId();
							}

							if (m_killer == playerId)
							{
								m_weaponFPAiming.SetActionController(pActionController);
								OnPlayerFirstPersonChange(pCloneEntity, 0, m_playInfo.m_view.currView == SPlaybackInfo::eVM_FirstPerson);
							}

							// Set up the initial states of the clone players
							if (!ApplyPlayerInitialState(playerId))
							{
								// Player didn't exist when the recording was made, hide their clone
								pCloneEntity->Hide(true);
							}

							if (playerId == m_killer)
							{
								// ensure the replayEntity will trigger areas so that during replay reverb areas are updated to the killers perspective rather than where the victim was at death
								// also ensure that it will trigger new reverb areas that the killer is in, rather than just turning off the victim's one
								pCloneEntity->AddFlags(ENTITY_FLAG_TRIGGER_AREAS | ENTITY_FLAG_CAMERA_SOURCE);
							}
						}
						else
						{
							assert(0 && "Entity has no character. Possibly bad assets?");
							RecSysLogAlways("[Recording System]: Entity \"%s\" (%p) has no character.", params.sName, params.pClass);
						}
					}
					else
					{
						assert(0 && "Unable to clone entity");
						RecSysLogAlways("[Recording System]: Unable to spawn entity \"%s\", Class(%p)", params.sName, params.pClass);
					}
				}
			}

			SetPlayBackCameraView(true);

			break;
		}
	case ePSS_StartExistingSounds:
		{
			//RecSysLog("OnPlaybackStart: ePSS_StartExistingSounds\n");
			//// Play any sounds that were created before the start of playback. Takes ~3ms
			//TPlaySoundMap::iterator itSound;
			//for (itSound = m_pPlaybackData->m_discardedSounds.begin(); itSound != m_pPlaybackData->m_discardedSounds.end(); ++itSound)
			//{
			//	ApplyPlaySound(&itSound->second.first, m_playInfo.m_timings.recStart - itSound->second.second);
			//}
			break;
		}
	case ePSS_ApplyBreaks:
		{
			RecSysLog("OnPlaybackStart: ePSS_ApplyBreaksAndItemSuffixes\n");
			//	approx 11ms
			//Any breakable objects need to be reverted to the state that they were in at the start
			//	of the killcam, so we can replay breaks on them. This needs to occur after any entity
			//	cloning by the rest of the system, to avoid objects being cloned twice
			RevertBrokenObjectsToStateAtTime(m_playInfo.m_timings.recStart);

			// Apply the initial entity attachments
			TEntityAttachedVec::iterator itEntityAttached;
			for (itEntityAttached = m_pPlaybackData->m_discardedEntityAttached.begin(); itEntityAttached != m_pPlaybackData->m_discardedEntityAttached.end(); )
			{
				if (ApplyEntityAttached(&*itEntityAttached))
				{
					++itEntityAttached;
				}
				else
				{
					RecSysLog("Failed to apply entity attached packet, entityId: %d, actorId: %d", itEntityAttached->entityId, itEntityAttached->actorId);
					itEntityAttached = m_pPlaybackData->m_discardedEntityAttached.erase(itEntityAttached);
				}
			}
			break;
		}
	case ePSS_ReInitHUD:
		{
			RecSysLog("OnPlaybackStart: ePSS_ReInitHUD\n");
			// Initialise the first person camera position
			FillOutFPCamera(&m_firstPersonCharacter, 0);

			// Notify the HUD of KillCam starting.
			SHUDEvent killCamStart(eHUDEvent_OnKillCamStartPlay);
			killCamStart.AddData(static_cast<void*>(&m_replayActors));
			killCamStart.AddData((int)m_killer);

			CHUDEventDispatcher::CallEvent(killCamStart);

			SHUDEvent event;
			event.eventType = eHUDEvent_OnLookAtChanged;
			event.AddData(0);
			CHUDEventDispatcher::CallEvent(event);

			TReplayActorMap::const_iterator itReplayActor = m_replayActors.begin();
			TReplayActorMap::const_iterator itEnd = m_replayActors.end();
			for (; itReplayActor != itEnd; ++itReplayActor)
			{
				if (itReplayActor->first != m_killer)
				{
					SHUDEventWrapper::PlayerRename(itReplayActor->second, true);
				}
			}

			gEnv->p3DEngine->SetPostEffectParam("HUD3D_Interference", 0.0f, true);
			g_pGame->GetUI()->ActivateStateImmediate("mp_killcam");
			//m_activeSoundLoop.Play();

			// Moved here, to only be cleared when playback actually starts.
			m_bCanPlayWinningKill = false;
			m_hasWinningKillcam = false;

			CPlayer* pClientPlayer = (CPlayer*)g_pGame->GetIGameFramework()->GetClientActor();
			if (pClientPlayer)
			{
				pClientPlayer->OnStartRecordingPlayback();
			}

			// Re-apply the PostEffects to stop solve the render thread overwriting them from earlier.
			m_killCamGameEffect.SetActive(true);

			// Notify Playback Started.
			NotifyOnPlaybackStarted(m_playInfo);
			break;
		}
	default:
		break;
	}

	m_playedFrame = false;
}

void CRecordingSystem::SetPlayBackCameraView(bool bSetView)
{
	// Need to set capture view and "linked to" entity for camera space rendering to work
	IView* pActiveView = gEnv->pGameFramework->GetIViewSystem()->GetActiveView();
	if (pActiveView)
	{
		const bool bOriginalEntity = true;
		CReplayActor* pKillerReplayActor = GetReplayActor(m_killer, bOriginalEntity);
		if (pKillerReplayActor)
		{
			IEntity* pKillerReplayEntity = pKillerReplayActor->GetEntity();
			if (pKillerReplayEntity)
			{
				CGameObject* pKillerReplayGameObject = (CGameObject*) pKillerReplayEntity->GetProxy(ENTITY_PROXY_USER);
				if (pKillerReplayGameObject)
				{
					if (bSetView)
					{
						m_gameCameraLinkedToEntityId = pActiveView->GetLinkedId();
						IEntity* pGameCameraLinkedToEntity = gEnv->pEntitySystem->GetEntity(m_gameCameraLinkedToEntityId);
						if (pGameCameraLinkedToEntity)
						{
							CGameObject* pGameCameraLinkedToGameObject = (CGameObject*)pGameCameraLinkedToEntity->GetProxy(ENTITY_PROXY_USER);
							if (pGameCameraLinkedToGameObject)
							{
								// Creates new view, sets it as active and links to it.
								pKillerReplayGameObject->CaptureView(pGameCameraLinkedToGameObject->GetViewDelegate());
							}
						}
					}
					else
					{
						IView* const pIView = gEnv->pGameFramework->GetIViewSystem()->GetViewByEntityId(m_gameCameraLinkedToEntityId);

						if (pIView != nullptr)
						{
							gEnv->pGameFramework->GetIViewSystem()->SetActiveView(pIView);
						}

						pKillerReplayGameObject->ReleaseView(pKillerReplayGameObject->GetViewDelegate());
					}
				}
			}
		}
	}
}

bool CRecordingSystem::ApplyPlayerInitialState(EntityId entityId)
{
	for (int i = 0; i < MAX_RECORDED_PLAYERS; ++i)
	{
		SPlayerInitialState& initialState = m_pPlaybackData->m_playerInitialStates[i];
		if (initialState.playerId == entityId)
		{
			// If any of the initial state packets don't exist then entity id should be 0
			if (initialState.changedModel.pModelName != NULL)
				ApplyPlayerChangedModel(&initialState.changedModel);
			if (initialState.weapon.ownerId != 0)
				ApplyWeaponSelect(&initialState.weapon);
			if (initialState.firemode.ownerId != 0)
				ApplyFiremodeChanged(&initialState.firemode);
			if (initialState.pickAndThrowUsed.playerId != 0)
				ApplyPickAndThrowUsed(&initialState.pickAndThrowUsed);
			ApplyMannequinEvent(&initialState.tagSetting);
			for (uint32 j = 0; j < SPlayerInitialState::MAX_FRAGMENT_TRIGGERS; j++)
			{
				ApplyMannequinEvent(&initialState.fragmentTriggers[j]);
			}
			if (initialState.mannSetSlaveController.masterActorId > 0)
			{
				ApplyMannequinSetSlaveController(&initialState.mannSetSlaveController);
			}
			if (initialState.mountedGunEnter.ownerId != 0)
				ApplyMountedGunEnter(&initialState.mountedGunEnter);
			if (initialState.objCloakParams.cloakPlayerId)
				ApplyObjectCloakSync(&initialState.objCloakParams);

			return true;
		}
	}
	return false;
}

float CRecordingSystem::GetLiveBufferLenSeconds(void)
{
	float firstFrameTime = 0;
	float lastFrameTime = 0;
	for (CRecordingBuffer::iterator itPacket = m_pBuffer->begin(); itPacket != m_pBuffer->end(); ++itPacket)
	{
		if (itPacket->type == eRBPT_FrameData)
		{
			float frameTime = ((SRecording_FrameData*)&*itPacket)->frametime;
			if (firstFrameTime == 0)
			{
				firstFrameTime = frameTime;
			}
			lastFrameTime = frameTime;
		}
	}
	return lastFrameTime - firstFrameTime;
}

float CRecordingSystem::GetBufferLenSeconds(void)
{
	uint8* startingplace = m_pTPDataBuffer;
	uint8* endingplace = startingplace + m_tpDataBufferSize;

	float starttime = -1.f;
	float endtime = -1.f;

	while (startingplace < endingplace)
	{
		SRecording_Packet* packet = (SRecording_Packet*) startingplace;

		switch (packet->type)
		{
		case eRBPT_FrameData:
			{
				SRecording_FrameData* sfd = (SRecording_FrameData*) startingplace;
				if (starttime == -1.f)
					starttime = sfd->frametime;
				endtime = sfd->frametime;
				break;
			}
		default:
			break;
		}

		startingplace = startingplace + packet->size;
	}

	if (starttime == -1.f || endtime == -1.f)
		return 0.f;

	return (endtime - starttime);
}

void CRecordingSystem::AdjustDeathTime()
{
	// Since we are only recording at a set rate (10 times a second at the time of writing) we need to make
	// sure the death time coincides with one of those snapshots. Clamp it to the previous snapshot because
	// this is what happens to the animation. And we want the bullet hit to coincide with the death animation.
	float unmodifiedDeathTime = m_playInfo.m_timings.relDeathTime;

	uint8* startingplace = m_pTPDataBuffer;
	uint8* endingplace = startingplace + m_tpDataBufferSize;
	while (startingplace < endingplace)
	{
		SRecording_Packet* packet = (SRecording_Packet*) startingplace;
		if (packet->type == eRBPT_FrameData)
		{
			float frameTime = ((SRecording_FrameData*)startingplace)->frametime - m_playInfo.m_timings.recStart;
			if (frameTime < unmodifiedDeathTime)
			{
				m_playInfo.m_timings.relDeathTime = frameTime;
			}
			else
			{
				break;
			}
		}
		startingplace = startingplace + packet->size;
	}

	RecSysLog("Adjusted death time from %f to %f", unmodifiedDeathTime, m_playInfo.m_timings.relDeathTime);
}

void CRecordingSystem::ExtractBulletPositions()
{
	SRecording_FPChar cam;
	FillOutFPCamera(&cam, m_playInfo.m_timings.relDeathTime);
	m_bulletOrigin = cam.camlocation.t;
	m_bulletOrientation = cam.camlocation.q;
	Vec3 hitPos;
	if (GetKillHitPosition(hitPos))
	{
		m_bulletTarget = hitPos;
		Vec3 newDirection = m_bulletTarget - m_bulletOrigin;
		newDirection.Normalize();
		m_bulletOrientation = Quat::CreateRotationVDir(newDirection);
	}
	else
	{
		m_bulletTarget = m_bulletOrigin + 10 * cam.camlocation.q.GetColumn1();
	}
	float travelDist = (m_bulletTarget - m_bulletOrigin).len();
	m_bulletTravelTime = travelDist / g_pGameCVars->kc_bulletSpeed;
	float frac = g_pGameCVars->kc_bulletHoverDist / travelDist;
	m_bulletHoverStartTime = m_playInfo.m_timings.relDeathTime - (frac * m_bulletTravelTime);
}

void CRecordingSystem::FillOutFPCamera(SRecording_FPChar* cam, float playbacktime)
{
	int upperoffset = 0;

	SRecording_FPChar* pFPCharData = (SRecording_FPChar*)m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonData;

	while (true)
	{
		if (upperoffset * sizeof(SRecording_FPChar) >= m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonDataSize || pFPCharData[upperoffset].type != eFPP_FPChar)
		{
			upperoffset--;
			break;
		}

		float fptime = pFPCharData[upperoffset].frametime - m_PlaybackInstanceData.m_remoteStartTime;

		if (fptime > playbacktime)
			break;

		upperoffset++;
	}

	int loweroffset = upperoffset - 1;
	if (loweroffset >= 0)
	{
		CRY_ASSERT_MESSAGE(pFPCharData[loweroffset].type == eFPP_FPChar, "This packet type is not valid");
		CRY_ASSERT_MESSAGE(pFPCharData[upperoffset].type == eFPP_FPChar, "This packet type is not valid");

		float loweroffsettime = pFPCharData[loweroffset].frametime - m_PlaybackInstanceData.m_remoteStartTime;
		float upperoffsettime = pFPCharData[upperoffset].frametime - m_PlaybackInstanceData.m_remoteStartTime;
		float timediff = upperoffsettime - loweroffsettime;

		CRY_ASSERT_MESSAGE(timediff != 0, "The time difference must not be 0");
		float lerpval = clamp_tpl((playbacktime - loweroffsettime) / timediff, 0.f, 1.f);

		cam->camlocation.SetNLerp(pFPCharData[loweroffset].camlocation, pFPCharData[upperoffset].camlocation, lerpval);
		cam->relativePosition.SetNLerp(pFPCharData[loweroffset].relativePosition, pFPCharData[upperoffset].relativePosition, lerpval);
		cam->fov = LERP(pFPCharData[loweroffset].fov, pFPCharData[upperoffset].fov, lerpval);
		cam->frametime = LERP(pFPCharData[loweroffset].frametime, pFPCharData[upperoffset].frametime, lerpval);
		cam->playerFlags = pFPCharData[loweroffset].playerFlags;

		Vec3 velocity = pFPCharData[upperoffset].camlocation.t - pFPCharData[loweroffset].camlocation.t;
		velocity /= timediff;
		Quat deltaRot = pFPCharData[loweroffset].camlocation.q.GetInverted() * pFPCharData[upperoffset].camlocation.q;
		Ang3 eulerAngles = ((Ang3)deltaRot) / timediff;
		m_weaponParams.velocity = velocity;
		m_weaponParams.inputMove = velocity;
		m_weaponParams.inputRot = eulerAngles;
	}
}

bool CRecordingSystem::GetVictimPosition(float playbackTime, Vec3& victimPosition) const
{
	int upperoffset = 0;

	SRecording_VictimPosition* pVictimData = (SRecording_VictimPosition*)(m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonData + m_PlaybackInstanceData.m_victimPositionsOffset);

	while (true)
	{
		if ((uint8*)(pVictimData + upperoffset) >= m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonData + m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonDataSize || pVictimData[upperoffset].type != eFPP_VictimPosition)
		{
			upperoffset--;
			break;
		}

		float time = pVictimData[upperoffset].frametime - m_PlaybackInstanceData.m_remoteStartTime;

		if (time > playbackTime)
			break;

		upperoffset++;
	}

	int loweroffset = upperoffset - 1;
	if (loweroffset >= 0)
	{
		CRY_ASSERT_MESSAGE(pVictimData[loweroffset].type == eFPP_VictimPosition, "This packet type is not valid");
		CRY_ASSERT_MESSAGE(pVictimData[upperoffset].type == eFPP_VictimPosition, "This packet type is not valid");

		float loweroffsettime = pVictimData[loweroffset].frametime - m_PlaybackInstanceData.m_remoteStartTime;
		float upperoffsettime = pVictimData[upperoffset].frametime - m_PlaybackInstanceData.m_remoteStartTime;
		float timediff = upperoffsettime - loweroffsettime;

		CRY_ASSERT_MESSAGE(timediff != 0, "The time difference must not be 0");
		float lerpval = clamp_tpl((playbackTime - loweroffsettime) / timediff, 0.f, 1.f);

		victimPosition.SetLerp(pVictimData[loweroffset].victimPosition, pVictimData[upperoffset].victimPosition, lerpval);
		return true;
	}
	return false;
}

/*static*/ const SRecording_Packet* CRecordingSystem::GetFirstFPActionPacketOfType(const SFirstPersonDataContainer& rFirstPersonData, const SPlaybackInstanceData& rPlaybackInstanceData, uint8 type)
{
	const uint8* pPos = rFirstPersonData.m_firstPersonData + rPlaybackInstanceData.m_firstPersonActionsOffset;
	while (pPos < rFirstPersonData.m_firstPersonData + rFirstPersonData.m_firstPersonDataSize)
	{
		SRecording_Packet* pPacket = (SRecording_Packet*)pPos;
		if (pPacket->type == type)
		{
			return pPacket;
		}
		pPos += pPacket->size;
	}

	return NULL;
}

bool CRecordingSystem::GetKillHitPosition(Vec3& killHitPos) const
{
	Vec3 victimEntityPos;
	if (GetVictimPosition(m_playInfo.m_timings.relDeathTime, victimEntityPos))
	{
		const SRecording_Packet* pPacket = CRecordingSystem::GetFirstFPActionPacketOfType(m_pPlaybackData->m_FirstPersonDataContainer, m_PlaybackInstanceData, eFPP_KillHitPosition);
		if (pPacket)
		{
			SRecording_KillHitPosition* pHitPos = (SRecording_KillHitPosition*)pPacket;
			killHitPos = victimEntityPos + pHitPos->hitRelativePos;
			return true;
		}
	}

	return false;
}

float CRecordingSystem::GetPlaybackTimeOffset() const
{
	const SRecording_Packet* pPacket = CRecordingSystem::GetFirstFPActionPacketOfType(m_pPlaybackData->m_FirstPersonDataContainer, m_PlaybackInstanceData, eFPP_PlaybackTimeOffset);
	if (pPacket)
	{
		SRecording_PlaybackTimeOffset* pTimeOffset = (SRecording_PlaybackTimeOffset*)pPacket;
		return pTimeOffset->timeOffset;
	}
	return 0.0f;
}

void CRecordingSystem::PlaybackRecordedFirstPersonActions(float playbacktime)
{
	REPLAY_EVENT_GUARD

	float adjustedTime = m_PlaybackInstanceData.m_remoteStartTime + playbacktime;
	uint8* pPos = m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonData + m_PlaybackInstanceData.m_firstPersonActionsOffset;
	while (pPos < m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonData + m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonDataSize)
	{
		bool packetProcessed = false;
		SRecording_Packet* pPacket = (SRecording_Packet*)pPos;
		switch (pPacket->type)
		{
		case eFPP_Flashed:
			{
				SRecording_Flashed* pFlashedPacket = (SRecording_Flashed*)pPacket;
				if (pFlashedPacket->frametime <= adjustedTime)
				{
					ApplyFlashed(pFlashedPacket);
					packetProcessed = true;
				}
				break;
			}
		case eFPP_RenderNearest:
			{
				SRecording_RenderNearest* pRenderPacket = (SRecording_RenderNearest*)pPacket;
				if (pRenderPacket->frametime <= adjustedTime)
				{
					ApplyRenderNearestChange(pRenderPacket);
					packetProcessed = true;
				}
				break;
			}
		case eFPP_BattleChatter:
			{
				SRecording_BattleChatter* pBattleChatterPacket = (SRecording_BattleChatter*)pPacket;
				if (pBattleChatterPacket->frametime <= adjustedTime)
				{
					ApplyBattleChatter(pBattleChatterPacket);
					packetProcessed = true;
				}
				break;
			}
		case eFPP_KillHitPosition:
		case eFPP_PlaybackTimeOffset:
			{
				// Do nothing, this data is used elsewhere
				break;
			}
		case eFPP_PlayerHealthEffect:
			{
				SRecording_PlayerHealthEffect* pPlayerHealthEffect = (SRecording_PlayerHealthEffect*)pPacket;
				if (pPlayerHealthEffect->frametime <= adjustedTime)
				{
					ApplyPlayerHealthEffect(pPlayerHealthEffect);
					packetProcessed = true;
				}
			}
			break;
		default:
			CRY_ASSERT_MESSAGE(false, "Unrecognised packet type found");
			break;
		}
		if (packetProcessed)
		{
			pPos += pPacket->size;
		}
		else
		{
			break;
		}
	}
	m_PlaybackInstanceData.m_firstPersonActionsOffset = pPos - m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonData;
}

void CRecordingSystem::DropTPFrame()
{
	uint8* startingplace = m_pTPDataBuffer;
	uint8* endingplace = startingplace + m_tpDataBufferSize;

	int frame = 0;
	while (startingplace < endingplace)
	{
		SRecording_Packet* packet = (SRecording_Packet*) startingplace;

		if (packet->type == eRBPT_FrameData)
		{
			SRecording_FrameData* sfd = (SRecording_FrameData*) startingplace;
			frame++;
			if (frame == 2)
			{
				break;
			}
		}

		startingplace = startingplace + packet->size;
	}

	m_pTPDataBuffer = startingplace;
	m_tpDataBufferSize = (uint32) (endingplace - startingplace);
}

void CRecordingSystem::RecordTPCharPacket(IEntity* pEntity, CActor* pActor)
{
	SRecording_TPChar chr;
	chr.eid = pEntity->GetId();
	IAnimatedCharacter* pAnimatedCharacter = pActor->GetAnimatedCharacter();
	if (pAnimatedCharacter)
	{
		chr.entitylocation = pAnimatedCharacter->GetAnimLocation();
	}
	else
	{
		chr.entitylocation.t = pEntity->GetPos();
		chr.entitylocation.q = pEntity->GetRotation();
	}
	if (m_eaikm[chr.eid].aimIKEnabled)
		chr.playerFlags |= eTPF_AimIk;
	chr.aimdir = m_eaikm[chr.eid].aimIKTarget;
	ChrVelocityMap::iterator itChrVel = m_chrvelmap.find(chr.eid);
	if (itChrVel != m_chrvelmap.end())
		chr.velocity = itChrVel->second;
	if (pActor->IsDead())
		chr.playerFlags |= eTPF_Dead;
	if (const SActorStats* pActorStats = pActor->GetActorStats())
	{
		chr.playerFlags |= (pActorStats->isRagDoll ? eTPF_Ragdoll : 0);
		chr.playerFlags |= (pActorStats->bStealthKilling ? eTPF_StealthKilling : 0);
	}
	chr.healthPercentage = pActor->GetHealthAsRoundedPercentage();

	if (pActor->IsPlayer())
	{
		CPlayer* pPlayer = static_cast<CPlayer*>(pActor);

		if (pPlayer->IsOnGround())
		{
			chr.playerFlags |= eTPF_OnGround;
		}
	}

	if (pEntity->IsInvisible() || pEntity->IsHidden())
	{
		chr.playerFlags |= eTPF_Invisible;
	}

	m_pBuffer->AddPacket(chr);
}

void CRecordingSystem::AddPacket(const SRecording_Packet& packet)
{
	if (CRecordingSystem::KillCamEnabled())
	{
		m_pBuffer->AddPacket(packet);
	}
}

void CRecordingSystem::QueueAddPacket(const SRecording_Packet& packet)
{
	if (CRecordingSystem::KillCamEnabled())
	{
		if (packet.size + m_queuedPacketsSize < RECORDING_BUFFER_QUEUE_SIZE)
		{
			memcpy(m_pQueuedPackets + m_queuedPacketsSize, &packet, packet.size);
			m_queuedPacketsSize += packet.size;
		}
		else
		{
			CRY_ASSERT_MESSAGE(false, "Ran out of memory queueing packets");
		}
	}
}

void CRecordingSystem::AddQueuedPackets()
{
	uint8* pPacket = m_pQueuedPackets;
	while (pPacket < m_pQueuedPackets + m_queuedPacketsSize)
	{
		SRecording_Packet& packet = *(SRecording_Packet*)pPacket;
		m_pBuffer->AddPacket(packet);
		pPacket += packet.size;
	}
	m_queuedPacketsSize = 0;
}

void CRecordingSystem::Update(const float frameTime)
{
#ifdef RECSYS_DEBUG
	if (CRecordingSystem::KillCamEnabled() && g_pGameCVars->kc_debugStressTest)
	{
		if (m_stressTestTimer <= 0.f)
		{
			DebugStoreHighlight(g_pGameCVars->kc_length, 0);
			PlayHighlight(0);
			m_stressTestTimer = cry_random(30.0f, 60.0f);
		}
		else
			m_stressTestTimer -= frameTime;
	}
	if (g_pGameCVars->kc_debugStream)
	{
		m_streamer.DebugStreamData();
	}

	if (g_pGameCVars->kc_debug == eRSD_General && (IsPlayingBack() || IsPlaybackQueued()))
	{
		RecSysWatch("Precached Killer FirstPerson Weapon: [%s]", m_playInfo.m_view.streamingWeaponFPModel.c_str());
		RecSysWatch("Precached Victim ThirdPerson Weapon: [%s]", m_playInfo.m_view.streamingWeaponTPModel.c_str());
	}
#endif //RECSYS_DEBUG

	m_killCamGameEffect.Update(frameTime);

	m_replayTimer->UpdateOnFrameStart();

	UpdateSendQueues();

	if (!CRecordingSystem::KillCamEnabled())
	{
		return;
	}

	if (IsRecording())
	{
		const float RecordFrequency = 1.f / 20.f;
		m_timeSinceLastRecord += frameTime;
		if (m_timeSinceLastRecord >= RecordFrequency)
		{
			m_pBuffer->Update();
			RecordEntityInfo();
			RecordActorWeapon();
			RecordWeaponAccessories();
			RecordSoundParameters();
			m_timeSinceLastRecord = 0.f;
		}

		RecordParticleInfo();
		AddQueuedPackets();

		if (m_recordKillQueue.m_timeTilRecord > 0.f)
		{
			m_recordKillQueue.m_timeTilRecord -= frameTime;
			if (m_recordKillQueue.m_timeTilRecord <= 0.f)
			{
				SaveHighlight(m_recordKillQueue);
			}
		}

#ifdef RECSYS_DEBUG
		if (g_pGameCVars->kc_memStats)
		{
			DebugDrawMemoryUsage();
		}
#endif //RECSYS_DEBUG
	}

	if (IsRecordingAndNotPlaying())
	{
		m_timeSinceLastFPRecord += frameTime;

		//This was being done anyway; need to check the view vector to allow us to have higher temporal resolution if required
		IViewSystem* cvs = gEnv->pGameFramework->GetIViewSystem();
		IView* activeView = cvs->GetActiveView();
		if (activeView)
		{
			const SViewParams* vp = activeView->GetCurrentParams();
			Ang3 currentView = Ang3(vp->rotation);
			QuatT view(vp->position, vp->rotation);

			Ang3 rotLastFrame = (currentView - m_lastView);
			float fAngDiff = ((Vec3)rotLastFrame).len();

			float fRotSpeed = frameTime > 0.0f ? fAngDiff / frameTime : 0.0f;

			static const float kRotationAccelerationThreshold = DEG2RAD(60.f);

			bool bRotationOverride = fabsf(fRotSpeed - m_fLastRotSpeed) > kRotationAccelerationThreshold;

			const float RecordFrequency = 1.f / 20.f;

#if RECSYS_INCREASED_ACCURACY
			const bool bShotOverride = m_clientShot;
#else
			const bool bShotOverride = false;
#endif

			if (m_timeSinceLastFPRecord >= RecordFrequency || bRotationOverride || bShotOverride)
			{
				const EntityId linkedId = activeView->GetLinkedId();
				IEntity* pLinkedEntity = linkedId ? gEnv->pEntitySystem->GetEntity(linkedId) : NULL;
				IActor* pLinkedActor = linkedId ? g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(linkedId) : NULL;
				RecordFPCameraPos(view, vp->fov, pLinkedEntity, pLinkedActor);
				m_timeSinceLastFPRecord = 0.f;
			}

			m_lastView = currentView;
			m_fLastRotSpeed = fRotSpeed;
		}
	}

	m_streamer.Update(frameTime);

	if (IsPlaybackQueued())
	{
		const float currentTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		const float expectedPlaybackTime = m_playInfo.m_timings.recEnd + m_playInfo.m_request.playbackDelay;
		const float timeTilPlayback = expectedPlaybackTime - currentTime;
		m_playInfo.m_view.Precache(timeTilPlayback);

		bool dataReady = m_pPlaybackData && m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonDataSize;
		if (!dataReady)
		{
			SKillCamStreamData* pStreamData = m_streamer.GetExpectedStreamData(m_victim);
			if (pStreamData)
			{
				// When the current kill stream is completely received, extract the data ready to be played.
				if (pStreamData->GetFinalSize())
				{
					ExtractCompleteStreamData(*pStreamData, m_PlaybackInstanceData, m_recordedData.m_FirstPersonDataContainer);
#ifdef RECSYS_DEBUG
					if (m_pDebug)
					{
						m_pDebug->PrintFirstPersonPacketData(m_recordedData.m_FirstPersonDataContainer.m_firstPersonData, m_recordedData.m_FirstPersonDataContainer.m_firstPersonDataSize, "RecordingSystem: Playback Data Fully Received");
					}
#endif    //RECSYS_DEBUG
					// Set the playback data.
					m_pPlaybackData = &m_recordedData;
					dataReady = true;
				}
				else
				{
					// Waiting for data to stream.
					return;
				}
			}
			else
			{
				CryFatalError("We should always expect data if we get to here. QueueStartPlayback() should be the only place that sets StreamData expectations, and sets the m_playbackState to ePS_Queued.");
			}
		}

		if (dataReady)
		{
			bool bCanPlay = m_hasWinningKillcam && m_bCanPlayWinningKill;
			if (!m_hasWinningKillcam)
			{
				bCanPlay = (currentTime > expectedPlaybackTime);
			}

			if (bCanPlay)
			{
				m_playbackState = ePS_Playing;
				m_playbackSetupStage = ePSS_FIRST;
			}
			else
			{
				return;
			}
		}
	}

	if (IsPlayingBack())
	{
		if (m_playbackSetupStage < ePSS_DONE)
		{
			// Called 8 times over 8 frames.
			OnPlaybackStart();
			m_playbackSetupStage = (EPlaybackSetupStage)(m_playbackSetupStage + 1);
		}

		if (m_tpDataBufferSize <= 0 || m_timeSincePlaybackStart > (m_playInfo.m_timings.recEnd - m_playInfo.m_timings.recStart))
		{
			StopPlayback();
		}
		else if (m_playbackSetupStage == ePSS_DONE)
		{
			UpdatePlayback(frameTime);
		}
	}
}

void CRecordingSystem::RecordFPCameraPos(const QuatT& view, const float fov, IEntity* pLinkedEntity, IActor* pLinkedActor)
{
	SRecording_FPChar cam;
	cam.camlocation = view;
	cam.fov = fov;
	cam.frametime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	cam.playerFlags = 0;
	cam.relativePosition.SetIdentity();
	if (m_clientShot)
	{
		cam.playerFlags |= eFPF_FiredShot;
		m_clientShot = false;
	}

	if (pLinkedEntity)
	{
		cam.relativePosition.t = pLinkedEntity->GetPos() - cam.camlocation.t;
		cam.relativePosition.q = cam.camlocation.q.GetInverted() * pLinkedEntity->GetRotation();

		if (pLinkedActor)
		{
			CActor* pCActor = static_cast<CActor*>(pLinkedActor);
			SActorStats* actorStats = pCActor->GetActorStats();
			if (!actorStats->inAir)
				cam.playerFlags |= eFPF_OnGround;
			if (pCActor->IsSprinting())
				cam.playerFlags |= eFPF_Sprinting;
			if (pCActor->IsThirdPerson())
				cam.playerFlags |= eFPF_ThirdPerson;
			SMovementState movementState;
			pCActor->GetMovementController()->GetMovementState(movementState);
			if (CWeapon* pWeapon = pCActor->GetWeapon(pCActor->GetCurrentItemId()))
			{
				EZoomState zoomState = pWeapon->GetZoomState();
				if (zoomState == eZS_ZoomingIn || zoomState == eZS_ZoomedIn)
				{
					cam.playerFlags |= eFPF_StartZoom;
				}
			}

			float value = 0.0f;
			gEnv->p3DEngine->GetPostEffectParam(NIGHT_VISION_PE, value);
			if (value > 0.0f)
			{
				cam.playerFlags |= eFPF_NightVision;
			}
		}
	}
	m_pBufferFirstPerson->AddPacket(cam);
}

float CRecordingSystem::GetFPCamDataTime()
{
	SRecording_FPChar* pFPCharData = (SRecording_FPChar*)m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonData;

	if (m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonDataSize == 0 || pFPCharData[0].type != eFPP_FPChar)
		return 0;

	float startTime = pFPCharData[0].frametime;
	float endTime = startTime;

	int index = 0;
	while (index * sizeof(SRecording_FPChar) < m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonDataSize)
	{
		if (pFPCharData[index].type != eFPP_FPChar)
		{
			break;
		}

		endTime = pFPCharData[index].frametime;
		index++;
	}

	return endTime - startTime;
}

void CRecordingSystem::RecordFPKillHitPos(IActor& victimActor, const HitInfo& hitInfo)
{
	IEntity* pVictimEntity = victimActor.GetEntity();
	if (ICharacterInstance* pCharInstance = pVictimEntity->GetCharacter(0))
	{
		if (ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose())
		{
			if (IPhysicalEntity* pPhysicalEntity = pSkeletonPose->GetCharacterPhysics())
			{
				pe_status_pos physicsPos;
				physicsPos.partid = hitInfo.partId;
				if (pPhysicalEntity->GetStatus(&physicsPos))
				{
					Vec3 hitPos(physicsPos.pos);
					if (physicsPos.pGeom)
					{
						hitPos += physicsPos.q * physicsPos.pGeom->GetCenter();
					}
					Vec3 entityLoc(pVictimEntity->GetPos());
					if (IAnimatedCharacter* pAnimatedCharacter = victimActor.GetAnimatedCharacter())
					{
						entityLoc = pAnimatedCharacter->GetAnimLocation().t;
					}

					SRecording_KillHitPosition killHitPos;
					killHitPos.hitRelativePos = hitPos - entityLoc;
					killHitPos.victimId = pVictimEntity->GetId();
					killHitPos.fRemoteKillTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
					m_pBufferFirstPerson->AddPacket(killHitPos);
				}
			}
		}
	}
}

bool CRecordingSystem::GetFrameTimes(float& lowerTime, float& upperTime)
{
	uint8* startingplace = m_pTPDataBuffer;
	uint8* endingplace = startingplace + m_tpDataBufferSize;
	bool first = true;

	while (startingplace < endingplace)
	{
		SRecording_Packet* packet = (SRecording_Packet*) startingplace;

		if (packet->type == eRBPT_FrameData)
		{
			float frameTime = ((SRecording_FrameData*)startingplace)->frametime;
			frameTime -= m_playInfo.m_timings.recStart;
			if (first)
			{
				lowerTime = frameTime;
				first = false;
			}
			else
			{
				upperTime = frameTime;
				return true;
			}
		}

		startingplace = startingplace + packet->size;
		CRY_ASSERT(packet->size > 0);
	}
	return false;
}

void CRecordingSystem::UpdateBulletPosition()
{
	REPLAY_EVENT_GUARD;
	float timeFromDeath = m_timeSincePlaybackStart - m_playInfo.m_timings.relDeathTime;
	if (timeFromDeath > -m_bulletTravelTime)
	{
		if (m_bulletEntityId == 0 && m_bulletTimePhase == eBTP_Approach)
		{
			IEntityClass* pEntityClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();

			SEntitySpawnParams params;
			params.pClass = pEntityClass;
			params.sName = "KillCamBullet";

			m_newBulletPosition = m_bulletOrigin;
			params.vPosition = m_bulletOrigin;
			params.qRotation = m_bulletOrientation * Quat::CreateRotationZ(gf_PI);

			params.nFlags |= (ENTITY_FLAG_NEVER_NETWORK_STATIC | ENTITY_FLAG_CLIENT_ONLY);

			IEntity* pBulletEntity = gEnv->pEntitySystem->SpawnEntity(params, true);
			pBulletEntity->LoadGeometry(0, m_bulletGeometryName);
			m_bulletEntityId = pBulletEntity->GetId();

			IParticleEffect* pParticleEffect = gEnv->pParticleManager->FindEffect(m_bulletEffectName);
			pBulletEntity->LoadParticleEmitter(1, pParticleEffect);

			m_playInfo.m_view.currView = SPlaybackInfo::eVM_BulletTime;
			SetTimeScale(g_pGameCVars->kc_bulletTravelTimeScale);

			const CCamera& camera = GetISystem()->GetViewCamera();
			Vec3 relative = camera.GetPosition() - m_bulletTarget;
			float focusDist = relative.len();

			// need to zoom out the weapon if zoomed, otherwise the hud gets a little confused
			ExitWeaponZoom();

			SetDofFocusDistance(focusDist);
			/*if (m_projectileType == ePT_Kvolt)
			   {
			   m_kvoltSoundLoop.Play();
			   }*/

			CAudioSignalPlayer::JustPlay("Killcam_Tracer");
		}
		else
		{
			float u = (timeFromDeath / m_bulletTravelTime) + 1.f;
			if (u > 1)
			{
				if (m_bulletTimePhase != eBTP_PostHit)
				{
					gEnv->pEntitySystem->RemoveEntity(m_bulletEntityId);
					m_bulletEntityId = 0;
					SetTimeScale(1.0f);

					SetDofFocusDistance(-1.f);
					m_bulletTimePhase = eBTP_PostHit;

					CAudioSignalPlayer::JustPlay("Killcam_KillShot");

					// Let's spawn a blood splat particle effect
					char effectName[] = "bullet.hit_flesh.a";
					// Randomly change the a into either a, b or c to get a different blood effect each time
					effectName[sizeof(effectName) - 2] = cry_random('a', 'c');
					IParticleEffect* pParticle = gEnv->pParticleManager->FindEffect(effectName);
					if (pParticle)
					{
						Vec3 direction = m_bulletOrientation.GetColumn1();
						pParticle->Spawn(true, IParticleEffect::ParticleLoc(m_bulletTarget, -direction));
					}
				}
			}
			else
			{
				m_newBulletPosition.SetLerp(m_bulletOrigin, m_bulletTarget, u);
				float focusDist = 0.0f;
				if (m_timeSincePlaybackStart >= m_bulletHoverStartTime)
				{
					if (m_timeSincePlaybackStart < g_pGameCVars->kc_bulletHoverTime * g_pGameCVars->kc_bulletHoverTimeScale + m_bulletHoverStartTime)
					{
						if (m_bulletTimePhase != eBTP_Hover)
						{
							SetTimeScale(g_pGameCVars->kc_bulletHoverTimeScale);
							m_bulletTimePhase = eBTP_Hover;
						}
					}
					else
					{
						if (m_bulletTimePhase != eBTP_PostHover)
						{
							// Let's attach a tracer to the bullet and speed up time again
							const STracerParams* pTracerParams = GetKillerTracerParams();
							if (pTracerParams)
							{
								CTracerManager::STracerParams params;
								params.geometry = pTracerParams->geometryFP.c_str();
								params.effect = pTracerParams->effectFP.c_str();
								params.position = m_newBulletPosition;
								params.destination = m_bulletTarget;
								params.speed = g_pGameCVars->kc_bulletSpeed;
								params.lifetime = pTracerParams->lifetime;
								params.scale = pTracerParams->thickness;
								g_pGame->GetWeaponSystem()->GetTracerManager().EmitTracer(params, m_bulletEntityId);
							}
							SetTimeScale(g_pGameCVars->kc_bulletPostHoverTimeScale);
							m_postHitCameraPos = m_newBulletPosition;
							m_bulletTimePhase = eBTP_PostHover;
							//m_kvoltSoundLoop.Stop();
						}
					}
					const CCamera& camera = GetISystem()->GetViewCamera();
					Vec3 relative = camera.GetPosition() - m_newBulletPosition;
					focusDist = relative.len();
				}
				else
				{
					const CCamera& camera = GetISystem()->GetViewCamera();
					Vec3 relative = camera.GetPosition() - m_bulletTarget;
					focusDist = relative.len();
				}

				SetDofFocusDistance(focusDist);
			}
		}
	}
}

void CRecordingSystem::SetDofFocusDistance(float focusDistance)
{
	if (focusDistance >= 0)
	{
		gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 1.0f);
		gEnv->p3DEngine->SetPostEffectParam("Dof_BlurAmount", 1.0f);
		gEnv->p3DEngine->SetPostEffectParam("Dof_FocusRange", 20.0f);
		gEnv->p3DEngine->SetPostEffectParam("Dof_FocusDistance", focusDistance);
	}
	else
	{
		gEnv->p3DEngine->SetPostEffectParam("Dof_Active", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("Dof_BlurAmount", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("Dof_FocusRange", 0.0f);
		gEnv->p3DEngine->SetPostEffectParam("Dof_FocusDistance", 0.0f);
	}
}

void CRecordingSystem::UpdatePlayback(const float frameTime)
{
	if (!IsPlayingHighlightsReel())
	{
		CPersistantStats::GetInstance()->IncrementClientStats(EFPS_KillCamTime, frameTime);
	}

#ifdef RECSYS_DEBUG
	if (g_pGameCVars->kc_debug == eRSD_Particles)
	{
		for (TReplayParticleMap::const_iterator it = m_replayParticles.begin(), end = m_replayParticles.end(); it != end; ++it)
		{
			if (IParticleEmitter* pEmitter = it->second)
			{
				IEntity* pAtt = gEnv->pEntitySystem->GetEntity(pEmitter->GetAttachedEntityId());
				RecSysWatch("Effect[%s] Attached[%s:%s]", pEmitter->GetName(), pAtt ? pAtt->GetClass()->GetName() : "", pAtt ? pAtt->GetName() : "");
			}
		}
	}
#endif //RECSYS_DEBUG

	const float scaledFrameTime = frameTime * m_timeScale;
	float prevPlaybackStartTime = m_timeSincePlaybackStart;
	m_timeSincePlaybackStart += scaledFrameTime;

	// Clear the Precached weapons after you have selected them, to avoid hogging memory.
	if (m_timeSincePlaybackStart > max(m_playInfo.m_timings.relDeathTime - 1.5f, 0.2f))
	{
		m_playInfo.m_view.ClearPrecachedWeapons();
	}

	if (m_bulletTimeKill)
	{
		if (prevPlaybackStartTime < m_bulletHoverStartTime && m_timeSincePlaybackStart > m_bulletHoverStartTime)
		{
			m_timeSincePlaybackStart = m_bulletHoverStartTime;
		}

		UpdateBulletPosition();
	}

	// Update game effect timing for fade out.
	if (m_playInfo.m_view.fadeOut < 1.f)
	{
		const float fadeOutKillerRate = (m_playInfo.m_view.fadeOutRate * frameTime);
		m_playInfo.m_view.fadeOut = min(m_playInfo.m_view.fadeOut + fadeOutKillerRate, 1.f);
	}
	m_killCamGameEffect.SetRemainingTime(max(min((m_playInfo.m_timings.recEnd - (m_playInfo.m_timings.recStart + m_timeSincePlaybackStart)) - 0.1f, (1.f - m_playInfo.m_view.fadeOut) * m_killCamGameEffect.GetFadeOutTimeInv()), 0.f));

	FillOutFPCamera(&m_firstPersonCharacter, m_timeSincePlaybackStart);
	PlaybackRecordedFirstPersonActions(m_timeSincePlaybackStart);

	SPlaybackInfo::View& view = m_playInfo.m_view;

	if (view.currView == SPlaybackInfo::eVM_FirstPerson || view.currView == SPlaybackInfo::eVM_ThirdPerson)
	{
		if (m_firstPersonCharacter.playerFlags & eFPF_ThirdPerson)
			view.currView = SPlaybackInfo::eVM_ThirdPerson;
		else
			view.currView = SPlaybackInfo::eVM_FirstPerson;
	}

	CReplayActor* pReplayActor = GetReplayActor(m_killer, true);

	if ((view.currView == SPlaybackInfo::eVM_FirstPerson) != (view.prevView == SPlaybackInfo::eVM_FirstPerson))
	{
		// When switching to and from first person playback update the killer entity
		if (pReplayActor)
		{
			IEntity* pKillerEntity = pReplayActor->GetEntity();
			if (pKillerEntity)
			{
				OnPlayerFirstPersonChange(pKillerEntity, pReplayActor->GetGunId(), view.currView == SPlaybackInfo::eVM_FirstPerson);
			}
		}
	}

	// Update PrevView.
	view.prevView = view.currView;

	float lowerTime = 0.f, upperTime = 0.f;
	if (GetFrameTimes(lowerTime, upperTime))
	{
		if (m_timeSincePlaybackStart < upperTime)
		{
			float lerpValue = (m_timeSincePlaybackStart - lowerTime) / (upperTime - lowerTime);
			lerpValue = CLAMP(lerpValue, 0.f, 1.f);
			InterpolateRecordedPositions(scaledFrameTime, m_timeSincePlaybackStart, lerpValue);
			return;
		}
	}

	const int MaxIterations = g_pGameCVars->kc_maxFramesToPlayAtOnce;
	int i;
	for (i = 0; i < MaxIterations; i++)
	{
		if (m_timeSincePlaybackStart >= upperTime)
		{
			lowerTime = upperTime;
			upperTime = PlaybackRecordedFrame(scaledFrameTime);
		}
		else
		{
			break;
		}
	}
}

void CRecordingSystem::InterpolateRecordedPositions(float frameTime, float playbackTime, float lerpValue) PREFAST_SUPPRESS_WARNING(6262)
{
	CRY_ASSERT_MESSAGE(lerpValue >= 0.f && lerpValue <= 1.f, "Lerp value should be between 0 and 1");

	int frameCount = -1;
	uint8* startingplace = m_pTPDataBuffer;
	uint8* endingplace = startingplace + m_tpDataBufferSize;

	// Extract out the relevant packets so that they can be processed
	CryFixedArray<SRecording_TPChar, MAX_RECORDED_PLAYERS> tpCharPackets[2];
	CryFixedArray<SRecording_EntityLocation, 256> entityLocationPackets[2];

	while (startingplace < endingplace)
	{
		SRecording_Packet* packet = (SRecording_Packet*) startingplace;

		switch (packet->type)
		{
		case eRBPT_FrameData:
			++frameCount;
			if (frameCount >= 2)
			{
				goto ExitLoop;
			}
			break;
		case eTPP_TPChar:
			assert(frameCount >= 0 && frameCount < 2);
			tpCharPackets[frameCount].push_back(*(SRecording_TPChar*)startingplace);
			break;
		case eTPP_EntityLocation:
			assert(frameCount >= 0 && frameCount < 2);
			entityLocationPackets[frameCount].push_back(*(SRecording_EntityLocation*)startingplace);
			break;
		}

		startingplace = startingplace + packet->size;
	}
ExitLoop:

	int i = 0, j = 0;
	int prevJ;
	bool foundMatch;
	// This logic assumes the order of the packets won't change between frames, but it does allow
	// for removal or insertion of packets
	for (i = 0; i < (int)tpCharPackets[0].size(); ++i)
	{
		SRecording_TPChar* lowerPacket = &tpCharPackets[0][i];
		prevJ = j;
		foundMatch = false;
		while (j < (int)tpCharPackets[1].size())
		{
			SRecording_TPChar* upperPacket = &tpCharPackets[1][j];
			if (upperPacket->eid == lowerPacket->eid)
			{
				// Found matching packets!
				foundMatch = true;
				UpdateThirdPersonPosition(lowerPacket, frameTime, playbackTime, upperPacket, lerpValue);
				break;
			}
			++j;
		}
		if (!foundMatch)
		{
			j = prevJ;
		}
		else
		{
			++j;
		}
	}

	i = 0;
	j = 0;
	for (i = 0; i < (int)entityLocationPackets[0].size(); ++i)
	{
		SRecording_EntityLocation* lowerPacket = &entityLocationPackets[0][i];
		prevJ = j;
		foundMatch = false;
		while (j < (int)entityLocationPackets[1].size())
		{
			SRecording_EntityLocation* upperPacket = &entityLocationPackets[1][j];
			if (upperPacket->entityId == lowerPacket->entityId)
			{
				// Found matching packets!
				foundMatch = true;
				ApplyEntityLocation(lowerPacket, upperPacket, lerpValue);
				break;
			}
			++j;
		}
		if (!foundMatch)
		{
			j = prevJ;
		}
		else
		{
			++j;
		}
	}
}

float CRecordingSystem::PlaybackRecordedFrame(float frameTime)
{
	REPLAY_EVENT_GUARD

	if (m_playedFrame)
	{
		DropTPFrame();
		m_playedFrame = false;
	}

	float recordedFrameTime = 0.f;
	uint8* startingplace = m_pTPDataBuffer;
	uint8* endingplace = startingplace + m_tpDataBufferSize;

	while (startingplace < endingplace)
	{
		SRecording_Packet* packet = (SRecording_Packet*) startingplace;

		switch (packet->type)
		{
		case eRBPT_FrameData:
			recordedFrameTime = ((SRecording_FrameData*)startingplace)->frametime - m_playInfo.m_timings.recStart;
			if (!m_playedFrame)
			{
				m_playedFrame = true;
			}
			else
			{
				m_playbackState = ePS_Playing;
				return recordedFrameTime;
			}
			break;
		case eTPP_OnShoot:
			{
				ApplyWeaponShoot((SRecording_OnShoot*)startingplace);
				break;
			}
		case eTPP_TPChar:
			{
				UpdateThirdPersonPosition((SRecording_TPChar*)startingplace, frameTime, recordedFrameTime);
				break;
			}
		case eTPP_WeaponAccessories:
			{
				ApplyWeaponAccessories((SRecording_WeaponAccessories*)startingplace);
				break;
			}
		case eTPP_WeaponSelect:
			{
				ApplyWeaponSelect((SRecording_WeaponSelect*)startingplace);
				break;
			}
		case eTPP_FiremodeChanged:
			{
				ApplyFiremodeChanged((SRecording_FiremodeChanged*)startingplace);
				break;
			}
		case eTPP_MannEvent:
			{
				ApplyMannequinEvent((SRecording_MannEvent*)startingplace);
				break;
			}
		case eTPP_MannSetSlaveController:
			{
				ApplyMannequinSetSlaveController((SRecording_MannSetSlaveController*)startingplace);
				break;
			}
		case eTPP_MannSetParam:
			{
				ApplyMannequinSetParam((SRecording_MannSetParam*)startingplace);
				break;
			}
		case eTPP_MannSetParamFloat:
			{
				ApplyMannequinSetParamFloat((SRecording_MannSetParamFloat*)startingplace);
				break;
			}
		case eTPP_MountedGunAnimation:
			{
				ApplyMountedGunAnimation((SRecording_MountedGunAnimation*)startingplace);
				break;
			}
		case eTPP_MountedGunRotate:
			{
				ApplyMountedGunRotate((SRecording_MountedGunRotate*)startingplace);
				break;
			}
		case eTPP_MountedGunEnter:
			{
				ApplyMountedGunEnter((SRecording_MountedGunEnter*)startingplace);
				break;
			}
		case eTPP_MountedGunLeave:
			{
				ApplyMountedGunLeave((SRecording_MountedGunLeave*)startingplace);
				break;
			}
		case eTPP_SpawnCustomParticle:
			{
				ApplySpawnCustomParticle((SRecording_SpawnCustomParticle*)startingplace);
				break;
			}
		case eTPP_ParticleCreated:
			{
				ApplyParticleCreated((SRecording_ParticleCreated*)startingplace);
				break;
			}
		case eTPP_ParticleDeleted:
			{
				ApplyParticleDeleted((SRecording_ParticleDeleted*)startingplace);
				break;
			}
		case eTPP_ParticleLocation:
			{
				ApplyParticleLocation((SRecording_ParticleLocation*)startingplace);
				break;
			}
		case eTPP_ParticleTarget:
			{
				ApplyParticleTarget((SRecording_ParticleTarget*)startingplace);
				break;
			}
		case eTPP_EntitySpawn:
			{
				ApplyEntitySpawn((SRecording_EntitySpawn*)startingplace);
				break;
			}
		case eTPP_EntityRemoved:
			{
				ApplyEntityRemoved((SRecording_EntityRemoved*)startingplace);
				break;
			}
		case eTPP_EntityLocation:
			{
				ApplyEntityLocation((SRecording_EntityLocation*)startingplace);
				break;
			}
		case eTPP_EntityHide:
			{
				ApplyEntityHide((SRecording_EntityHide*)startingplace);
				break;
			}
		case eTPP_PlaySound:
			{
				ApplyPlaySound((SRecording_PlaySound*)startingplace);
				break;
			}
		case eTPP_StopSound:
			{
				ApplyStopSound((SRecording_StopSound*)startingplace);
				break;
			}
		case eTPP_SoundParameter:
			{
				ApplySoundParameter((SRecording_SoundParameter*)startingplace);
				break;
			}
		case eTPP_BulletTrail:
			{
				ApplyBulletTrail((SRecording_BulletTrail*)startingplace);
				break;
			}
		case eTPP_ProceduralBreakHappened:
			{
				BreakLogAlways("Killcam replay; break occured, break ID: %d", ((SRecording_ProceduralBreakHappened*)startingplace)->uBreakEventIndex);
				ApplySingleProceduralBreakEvent(((SRecording_ProceduralBreakHappened*)startingplace));
				break;
			}
		case eTPP_DrawSlotChange:
			{
				ApplyDrawSlotChange(((SRecording_DrawSlotChange*)startingplace));
				break;
			}
		case eTPP_StatObjChange:
			{
				ApplyStatObjChange(((SRecording_StatObjChange*)startingplace));
				break;
			}
		case eTPP_SubObjHideMask:
			{
				ApplySubObjHideMask(((SRecording_SubObjHideMask*)startingplace));
				break;
			}
		case eTPP_TeamChange:
			{
				ApplyTeamChange((SRecording_TeamChange*)startingplace);
				break;
			}
		case eTPP_ItemSwitchHand:
			{
				ApplyItemSwitchHand((SRecording_ItemSwitchHand*)startingplace);
				break;
			}
		case eTPP_EntityAttached:
			{
				ApplyEntityAttached((SRecording_EntityAttached*)startingplace);
				break;
			}
		case eTPP_EntityDetached:
			{
				ApplyEntityDetached((SRecording_EntityDetached*)startingplace);
				break;
			}
		case eTPP_AnimObjectUpdated:
			{
				ApplyAnimObjectUpdated((SRecording_AnimObjectUpdated*)startingplace);
				break;
			}
		case eTPP_ObjectCloakSync:
			{
				ApplyObjectCloakSync((SRecording_ObjectCloakSync*)startingplace);
				break;
			}
		case eTPP_PickAndThrowUsed:
			{
				ApplyPickAndThrowUsed((SRecording_PickAndThrowUsed*)startingplace);
				break;
			}
		case eTPP_ForcedRagdollAndImpulse:
			{
				ApplyForcedRagdollAndImpulse((SRecording_ForcedRagdollAndImpulse*)startingplace);
				break;
			}
		case eTPP_RagdollImpulse:
			{
				ApplyImpulseToRagdoll((SRecording_RagdollImpulse*)startingplace);
			}
			break;
		case eTPP_PlayerChangedModel:
			{
				ApplyPlayerChangedModel((SRecording_PlayerChangedModel*)startingplace);
			}
			break;
		case eTPP_CorpseSpawned:      // Deliberate fall through
		case eTPP_CorpseRemoved:
		case eTPP_PlayerJoined:
		case eTPP_PlayerLeft:
		case eTPP_InteractiveObjectFinishedUse:
			{
				// Nothing to do - packets used to store initial state
				break;
			}
		default:
			{
				CRY_ASSERT_MESSAGE(false, "KillCam replay unrecognised packet type");
				break;
			}
		}

		startingplace = startingplace + packet->size;
	}

	return recordedFrameTime;
}

void CRecordingSystem::PostUpdate()
{
	if (IsPlayingBack())
	{
		REPLAY_EVENT_GUARD;

		UpdateFirstPersonPosition();
		// Update the position of the projectile we are following here to avoid frame lag
		if (m_projectileId != 0 && !m_newProjectilePosition.IsIdentity())
		{
			TReplayEntityMap::iterator itProjectile = m_replayEntities.find(m_projectileId);
			if (itProjectile != m_replayEntities.end())
			{
				EntityId replayEntityId = itProjectile->second;
				IEntity* pReplayEntity = gEnv->pEntitySystem->GetEntity(replayEntityId);
				if (pReplayEntity)
				{
					pReplayEntity->SetPosRotScale(m_newProjectilePosition.t, m_newProjectilePosition.q, Vec3(1.0f));
				}
			}
		}
		if (m_bulletEntityId != 0 && !m_newBulletPosition.IsZero())
		{
			IEntity* pBulletEntity = gEnv->pEntitySystem->GetEntity(m_bulletEntityId);
			if (pBulletEntity)
			{
				pBulletEntity->SetPos(m_newBulletPosition);
				if (!m_disableRifling)
				{
					pBulletEntity->SetRotation(m_bulletOrientation * Quat::CreateRotationY(m_replayTimer->GetCurrTime() * g_pGameCVars->kc_bulletRiflingSpeed));
				}
			}
		}
	}
}

void CRecordingSystem::UpdateView(SViewParams& viewParams)
{
#ifdef RECSYS_DEBUG
	if (g_pGameCVars->g_detachCamera)
	{
		return;
	}
#endif //RECSYS_DEBUG

	QuatT targetCameraPosition = IDENTITY;
	// Get the target camera position from the projectile we are following
	if (m_projectileId != 0)
	{
		TReplayEntityMap::iterator itProjectileEntity = m_replayEntities.find(m_projectileId);
		if (itProjectileEntity != m_replayEntities.end())
		{
			IEntity* pProjectileEntity = gEnv->pEntitySystem->GetEntity(itProjectileEntity->second);
			if (pProjectileEntity && !pProjectileEntity->IsHidden())
			{
				Vec3 projPos = pProjectileEntity->GetWorldPos();
				if (!projPos.IsZero())
				{
					TReplayActorMap::iterator itVictim = m_replayActors.find(m_victim);
					if (itVictim != m_replayActors.end())
					{
						IEntity* pVictimEntity = gEnv->pEntitySystem->GetEntity(itVictim->second);
						if (pVictimEntity)
						{
							m_playInfo.m_view.currView = SPlaybackInfo::eVM_ProjectileFollow;
							Vec3 victimFocusPos = pVictimEntity->GetWorldPos();
							Vec3 relative = victimFocusPos - projPos;
							float distToVictim = relative.NormalizeSafe();
							float cameraDist = (m_projectileType == ePT_Rocket) ? g_pGameCVars->kc_largeProjectileDistance : g_pGameCVars->kc_projectileDistance;
							if (cameraDist + distToVictim < g_pGameCVars->kc_projectileMinimumVictimDist)
							{
								cameraDist = g_pGameCVars->kc_projectileMinimumVictimDist - distToVictim;
							}
							Vec3 camPos = projPos - relative * cameraDist;

							switch (m_projectileType)
							{
							case ePT_Rocket:
								camPos.z += g_pGameCVars->kc_largeProjectileHeightOffset;
								break;

							case ePT_C4:
								if (m_C4Adjustment < 0.0f)
								{
									m_C4Adjustment = CalculateSafeRaiseDistance(camPos);
								}
								camPos.z += m_C4Adjustment;
								break;

							default:
								camPos.z += g_pGameCVars->kc_projectileHeightOffset;
								break;
							}

							victimFocusPos.z += g_pGameCVars->kc_projectileVictimHeightOffset;
							Vec3 direction = victimFocusPos - camPos;
							ray_hit rayHit;
							// TODO: Could defer this line test if it proves to be a performance problem
							if (gEnv->pPhysicalWorld->RayWorldIntersection(victimFocusPos, -direction, ent_static | ent_terrain, rwi_stop_at_pierceable, &rayHit, 1))
							{
								camPos = rayHit.pt + rayHit.n * 0.1f;
							}

							CameraCollisionAdjustment(pVictimEntity, camPos);

							direction.Normalize();
							targetCameraPosition.t = camPos;
							targetCameraPosition.q = Quat::CreateRotationVDir(direction);
							if (m_projectileType == ePT_Kvolt)
							{
								m_cameraSmoothing = true;
							}
							else if (m_projectileType == ePT_Grenade)
							{
								if (distToVictim <= g_pGameCVars->kc_grenadeSmoothingDist)
								{
									m_cameraSmoothing = true;
								}
							}
						}
					}
				}
			}
		}
	}

	Vec3 cameraAdjustment = Vec3(0.0f, 0.0f, 0.0f);

	if (m_playInfo.m_view.currView == SPlaybackInfo::eVM_ProjectileFollow && targetCameraPosition.IsIdentity())
	{
		// The projectile must have exploded and is no longer valid, switch to static camera mode.
		m_playInfo.m_view.currView = SPlaybackInfo::eVM_Static;
		m_cameraSmoothing = false;

		if (m_projectileType != ePT_C4 || m_C4Adjustment < 0.0f) // Don't need to raise the camera twice.
		{
			cameraAdjustment.z += CalculateSafeRaiseDistance(viewParams.GetPositionLast());
		}
	}

	switch (m_playInfo.m_view.currView)
	{
	case SPlaybackInfo::eVM_FirstPerson:
		{
			CReplayActor* pKiller = GetReplayActor(m_killer, true);
			IEntity* pKillerEntity = pKiller ? pKiller->GetEntity() : NULL;
			if (pKillerEntity)
			{
				const int slotIndex = 0;
				const uint32 killerEntityFlags = pKillerEntity->GetSlotFlags(slotIndex);

				if (killerEntityFlags & ENTITY_SLOT_RENDER_NEAREST)
				{
					targetCameraPosition.q = m_firstPersonCharacter.camlocation.q /** m_playInfo.m_view.lastSTAPCameraDelta.q*/;

					// First person rendered in camera space in nearest pass
					targetCameraPosition.t = (-m_firstPersonCharacter.relativePosition.t) /*+ m_playInfo.m_view.lastSTAPCameraDelta.t*/;

					viewParams.fov = m_firstPersonCharacter.fov;
					m_cameraSmoothing = false;
					break;
				}
			}
		} // Deliberately fall through here to eVM_ThirdPerson, if not rendering in camera space
	case SPlaybackInfo::eVM_ThirdPerson:
		{
			targetCameraPosition = m_firstPersonCharacter.camlocation;
			viewParams.fov = m_firstPersonCharacter.fov;
			m_cameraSmoothing = false;
		}
		break;
	case SPlaybackInfo::eVM_Static:
		{
			// Keep the last camera position
			targetCameraPosition.t = viewParams.GetPositionLast();
			targetCameraPosition.q = viewParams.GetRotationLast();

			targetCameraPosition.t += cameraAdjustment;

			// But look towards the victim
			TReplayActorMap::iterator itVictim = m_replayActors.find(m_victim);
			if (itVictim != m_replayActors.end())
			{
				IEntity* pVictimEntity = gEnv->pEntitySystem->GetEntity(itVictim->second);
				if (pVictimEntity)
				{
					Vec3 victimFocusPos = pVictimEntity->GetWorldPos();
					CameraCollisionAdjustment(pVictimEntity, targetCameraPosition.t);
					victimFocusPos.z += g_pGameCVars->kc_projectileVictimHeightOffset;
					Vec3 direction = victimFocusPos - targetCameraPosition.t;
					direction.Normalize();
					targetCameraPosition.q = Quat::CreateRotationVDir(direction);
				}
			}
		}
		break;
	case SPlaybackInfo::eVM_BulletTime:
		{
			switch (m_bulletTimePhase)
			{
			case eBTP_Approach:
			case eBTP_Hover:
			case eBTP_PostHover:
				{
					// Follow the bullet
					IEntity* pBulletEntity = gEnv->pEntitySystem->GetEntity(m_bulletEntityId);
					if (pBulletEntity)
					{
						targetCameraPosition.q = m_bulletOrientation;
						targetCameraPosition.t = pBulletEntity->GetPos();
						targetCameraPosition.t += g_pGameCVars->kc_bulletCamOffsetX * m_bulletOrientation.GetColumn0();
						targetCameraPosition.t += g_pGameCVars->kc_bulletCamOffsetY * m_bulletOrientation.GetColumn1();
						targetCameraPosition.t += g_pGameCVars->kc_bulletCamOffsetZ * m_bulletOrientation.GetColumn2();
					}
				}
				break;
			case eBTP_PostHit:
				{
					// Set the post hit camera position
					targetCameraPosition.t = m_postHitCameraPos;
					targetCameraPosition.q = viewParams.GetRotationLast();

					// Get the camera to zoom in and out rapidly just after the hit
					float timeFromDeath = m_timeSincePlaybackStart - m_playInfo.m_timings.relDeathTime;
					const float zoomTime = g_pGameCVars->kc_bulletZoomTime;
					const float zoomDist = g_pGameCVars->kc_bulletZoomDist;
					const float zoomOutRatio = g_pGameCVars->kc_bulletZoomOutRatio;
					Vec3 direction = m_bulletOrientation.GetColumn1();
					float u = -zoomOutRatio;
					if (timeFromDeath < zoomTime)
					{
						u = timeFromDeath / zoomTime;
					}
					else if (timeFromDeath < zoomTime * (2.f + zoomOutRatio))
					{
						u = (2.f * zoomTime - timeFromDeath) / zoomTime;
					}
					targetCameraPosition.t += (u * zoomDist) * direction;
					if (u == -zoomOutRatio)
					{
						m_cameraSmoothing = true;
						// Look towards the victim
						TReplayActorMap::iterator itVictim = m_replayActors.find(m_victim);
						if (itVictim != m_replayActors.end())
						{
							IEntity* pVictimEntity = gEnv->pEntitySystem->GetEntity(itVictim->second);
							if (pVictimEntity)
							{
								AABB entAABB;
								pVictimEntity->GetWorldBounds(entAABB);
								Vec3 victimFocusPos = entAABB.GetCenter();
								Vec3 direction2 = victimFocusPos - targetCameraPosition.t;
								direction2.Normalize();
								targetCameraPosition.q = Quat::CreateRotationVDir(direction2);
							}
						}
					}
				}
				break;
			default:
				CRY_ASSERT_MESSAGE(false, "Unrecognised bullet time phase");
				break;
			}
		}
		break;
	}

	if (m_cameraSmoothing && g_pGameCVars->kc_smoothing > 0)
	{
		const float fFrameTime = gEnv->pTimer->GetFrameTime();
		const QuatT prevLoc(viewParams.GetPositionLast(), viewParams.GetRotationLast());
		QuatT smoothedLoc(prevLoc);
		SmoothCD<Vec3>(smoothedLoc.t, m_playInfo.m_view.camSmoothingRate, fFrameTime, targetCameraPosition.t, g_pGameCVars->kc_smoothing);
		const Vec3 totalDiff(targetCameraPosition.t - prevLoc.t);
		const Vec3 appliedDiff(smoothedLoc.t - prevLoc.t);
		const float lenSqr = totalDiff.GetLengthSquared();
		if (lenSqr > FLT_EPSILON)
		{
			viewParams.position = smoothedLoc.t;
			const float invlen = isqrt_tpl(lenSqr);
			viewParams.rotation = Quat::CreateNlerp(prevLoc.q, targetCameraPosition.q, (appliedDiff * invlen).Dot(totalDiff * invlen));
		}
		else
		{
			viewParams.position = targetCameraPosition.t;
			viewParams.rotation = targetCameraPosition.q;
		}
	}
	else
	{
		viewParams.position = targetCameraPosition.t;
		viewParams.rotation = targetCameraPosition.q;
	}
	m_playInfo.m_view.bWorldSpace = true;
}

const float CRecordingSystem::CalculateSafeRaiseDistance(const Vec3& pos)
{
	const Vec3 upVec = Vec3(0.0f, 0.0f, g_pGameCVars->kc_cameraRaiseHeight);

	// Test if we can raise the camera, in order to prevent tracking through the floor,
	// e.g. When the victim's body happens to fall off an edge or the camera is following C4.
	ray_hit rayHit;
	if (gEnv->pPhysicalWorld->RayWorldIntersection(pos, upVec, ent_static | ent_terrain, rwi_ignore_back_faces, &rayHit, 1))
	{
		return (rayHit.dist * 0.5f);
	}
	else
	{
		return upVec.z;
	}
}

void CRecordingSystem::UpdateFirstPersonPosition()
{
	if (m_killer != 0 && m_playInfo.m_view.currView == SPlaybackInfo::eVM_FirstPerson)
	{
		CReplayActor* pReplayActor = GetReplayActor(m_killer, true);
		if (pReplayActor)
		{
			IEntity* replayEntity = pReplayActor->GetEntity();
			// If this is the killer then we need to synchronise the position with the camera position
			QuatT entityLocation = m_firstPersonCharacter.camlocation;
			entityLocation.t += m_firstPersonCharacter.relativePosition.t;
			entityLocation.q *= m_firstPersonCharacter.relativePosition.q;
			Vec3 unitVector(1, 1, 1);

			ICharacterInstance* pCharacter = replayEntity->GetCharacter(0);

			Vec3 aimDirection = m_firstPersonCharacter.camlocation.q.GetColumn1();
			QuatT viewOffset(IDENTITY);

			m_weaponParams.characterInst = pCharacter;
			m_weaponParams.skelAnim = pCharacter->GetISkeletonAnim();

			bool isZoomed = false;
			IGameFramework* pGameFramework = gEnv->pGameFramework;
			IItemSystem* pItemSystem = pGameFramework->GetIItemSystem();
			bool enableWeaponAim = false;
			if (IItem* pItem = pItemSystem->GetItem(pReplayActor->GetGunId()))
			{
				((CItem*)pItem)->GetFPOffset(viewOffset);
				CWeapon* pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon());
				if (pWeapon)
				{
					enableWeaponAim = pWeapon->UpdateAimAnims(m_weaponParams);
					bool shouldZoom = (m_firstPersonCharacter.playerFlags & eFPF_StartZoom) != 0;
					EZoomState zoomState = pWeapon->GetZoomState();
					bool zoomStarted = (zoomState == eZS_ZoomingIn || zoomState == eZS_ZoomedIn);
					if (shouldZoom != zoomStarted)
					{
						IZoomMode* pZoomMode = pWeapon->GetZoomMode(pWeapon->GetCurrentZoomMode());
						if (pZoomMode)
						{
							if (shouldZoom)
								pZoomMode->StartZoom();
							else
								pZoomMode->ExitZoom(true);
						}
					}
					isZoomed = pWeapon->IsZoomed();
					m_weaponParams.zoomTransitionFactor = pWeapon->GetZoomTransition();
				}
			}

			replayEntity->SetPosRotScale(entityLocation.t, entityLocation.q, unitVector);

			if (enableWeaponAim)
			{
				m_torsoAimIK.Enable();
			}
			else
			{
				m_torsoAimIK.Disable();
			}

			Vec3 relAimDir = !entityLocation.q * aimDirection;
			Vec3 relTorsoPos = !entityLocation.q * (m_firstPersonCharacter.camlocation.t - entityLocation.t);

			ICharacterInstance* pShadowCharacter = replayEntity->GetCharacter(CReplayActor::ShadowCharacterSlot);
			CIKTorsoAim_Helper::SIKTorsoParams IKParams(pCharacter, pShadowCharacter, relAimDir, viewOffset, relTorsoPos);
			m_torsoAimIK.Update(IKParams);

			if (m_torsoAimIK.GetBlendFactor() > 0.9f)
			{
				const QuatT& cameraTran = pReplayActor->GetCameraTran();
				m_playInfo.m_view.lastSTAPCameraDelta = m_torsoAimIK.GetLastEffectorTransform().GetInverted() * cameraTran;
			}
			else
			{
				m_playInfo.m_view.lastSTAPCameraDelta.SetIdentity();
			}

			float value = 0.0f;
			gEnv->p3DEngine->GetPostEffectParam(NIGHT_VISION_PE, value);
			if ((value > 0.0f) != ((m_firstPersonCharacter.playerFlags & eFPF_NightVision) != 0))
			{
				gEnv->p3DEngine->SetPostEffectParam(NIGHT_VISION_PE, (m_firstPersonCharacter.playerFlags & eFPF_NightVision) ? 1.0f : 0.0f);
			}

			// This code is mirrored from CPlayer::UpdateFPAiming()
			m_weaponParams.flags.ClearAllFlags();
			m_weaponParams.aimDirection = relAimDir;
			m_weaponParams.groundDistance = fabs_tpl(gEnv->p3DEngine->GetTerrainElevation(entityLocation.t.x, entityLocation.t.y) - entityLocation.t.z);
			m_weaponParams.flags.AddFlags((m_firstPersonCharacter.playerFlags & eFPF_OnGround) != 0 ? eWFPAF_onGround : 0);
			m_weaponParams.flags.AddFlags((m_firstPersonCharacter.playerFlags & eFPF_Sprinting) != 0 ? eWFPAF_sprinting : 0);
			m_weaponParams.flags.AddFlags(isZoomed ? eWFPAF_zoomed : 0);
			m_weaponParams.position = entityLocation.t;
			m_weaponFPAiming.SetActive(enableWeaponAim);
			m_weaponFPAiming.Update(m_weaponParams);

			if (CItem* pItem = (CItem*)g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pReplayActor->GetGunId()))
			{
				if (pItem->IsMounted())
				{
					float ironSightWeigth = 0.0f;
					IWeapon* pWeapon = pItem->GetIWeapon();
					if (pWeapon)
					{
						IZoomMode* pCurrentZoomMode = pWeapon->GetZoomMode(pWeapon->GetCurrentZoomMode());
						if (pCurrentZoomMode && (pCurrentZoomMode->IsZoomed() || pCurrentZoomMode->IsZoomingInOrOut()))
						{
							ironSightWeigth = 1.0f;
							if (pCurrentZoomMode->IsZoomingInOrOut())
							{
								ironSightWeigth = pCurrentZoomMode->GetZoomTransition();
							}
						}
					}
					const SMountParams* pMountParams = pItem->GetMountedParams();
					QuatT location = m_firstPersonCharacter.camlocation;
					if (pMountParams)
					{
						Vec3 offset = LERP(pMountParams->fpBody_offset, pMountParams->fpBody_offset_ironsight, ironSightWeigth);
						location.t -= location.q * offset;
					}
					pItem->GetEntity()->SetPosRotScale(location.t, location.q, Vec3(1.0f));
				}
			}
		}
		else if (m_playbackSetupStage > ePSS_SpawnRecordingSystemActors)
		{
			RecSysLogDebug(eRSD_General, "RecordingSystem Could not find killer with ID: %d", m_killer);
		}
	}
}

void CRecordingSystem::UpdateThirdPersonPosition(const SRecording_TPChar* tpchar, float frameTime, float playbackTime, const SRecording_TPChar* tpchar2, float lerpValue)
{
	CReplayActor* pReplayActor = NULL;
	TReplayActorMap::const_iterator find_iterator = m_replayActors.find(tpchar->eid);
	if (find_iterator != m_replayActors.end())
	{
		pReplayActor = GetReplayActor(find_iterator->second, false);
	}

	IEntity* pReplayEntity = NULL;
	if (pReplayActor)
	{
		pReplayEntity = pReplayActor->GetEntity();
	}
	else
	{
		TReplayEntityMap::const_iterator itReplayEntity = m_replayEntities.find(tpchar->eid);
		if (itReplayEntity != m_replayEntities.end())
		{
			pReplayEntity = gEnv->pEntitySystem->GetEntity(itReplayEntity->second);
		}
	}

	const bool bHasTPFlagsToTriggerRagdoll = (tpchar->playerFlags & eTPF_Ragdoll) ||
	                                         (tpchar->playerFlags & (eTPF_Dead | eTPF_Invisible)) == (eTPF_Dead | eTPF_Invisible) ||
	                                         (tpchar2 && ((tpchar2->playerFlags & eTPF_Ragdoll) || (tpchar2->playerFlags & (eTPF_Dead | eTPF_Invisible)) == (eTPF_Dead | eTPF_Invisible)));

	const bool shouldBeRagdoll = pReplayActor && (bHasTPFlagsToTriggerRagdoll || (pReplayActor->m_flags & eRAF_HaveSpawnedMyCorpse));
	bool updatePosition = !shouldBeRagdoll && (tpchar->playerFlags & eTPF_Invisible) == 0;

	if (pReplayActor)
	{
		if (tpchar->eid == m_killer && m_playInfo.m_view.currView == SPlaybackInfo::eVM_FirstPerson)
		{
			// Don't update the player if they are the killer and in first person
			updatePosition = false;
			if (pReplayEntity->IsHidden())
			{
				pReplayEntity->Hide(false);
			}
		}

		uint16 prevRAFlags = pReplayActor->m_flags;

#define SetFlags(o, i, oF, iF) { const uint32 mask = -iszero(i & iF); o = ((o | ((~mask) & oF)) & ~(mask & oF)); \
  }

		SetFlags(pReplayActor->m_flags, tpchar->playerFlags, eRAF_StealthKilling, eTPF_StealthKilling);
		SetFlags(pReplayActor->m_flags, tpchar->playerFlags, eRAF_Dead, eTPF_Dead);

#undef SetFlags

		if (tpchar->eid == m_killer)
		{
			// Just Died.
			if ((prevRAFlags & eRAF_Dead) == 0 && (pReplayActor->m_flags & eRAF_Dead) != 0)
			{
				static const float fadeOutTime = 0.5f;
				m_playInfo.m_view.fadeOutRate = 1.f / fadeOutTime;
			}

			// Stopped or started StealthKill.
			if (((prevRAFlags ^ pReplayActor->m_flags) & eRAF_StealthKilling) != 0)
			{
				if ((prevRAFlags & eRAF_StealthKilling) != 0)
				{
					SHUDEvent weaponSwitch(eHUDEvent_OnKillCamWeaponSwitch);
					weaponSwitch.AddData((int)pReplayActor->GetGunId());
					weaponSwitch.AddData(false);
					CHUDEventDispatcher::CallEvent(weaponSwitch);
				}
				else
				{
					SHUDEvent weaponSwitch(eHUDEvent_OnKillCamWeaponSwitch);
					weaponSwitch.AddData(0);
					weaponSwitch.AddData(true);
					CHUDEventDispatcher::CallEvent(weaponSwitch);
				}
			}
		}

		pReplayActor->SetHealthPercentage(tpchar->healthPercentage);

		pReplayActor->SetOnGround((tpchar->playerFlags & eTPF_OnGround) != 0);

		const Vec3 velocity(tpchar2 ? LERP(tpchar->velocity, tpchar2->velocity, lerpValue) : tpchar->velocity);
		pReplayActor->SetVelocity(velocity);

		if (tpchar->playerFlags & eTPF_Invisible)
		{
			if ((pReplayActor->m_flags & (eRAF_Invisible | eRAF_Dead)) == 0)
			{
				HideEntityKeepingPhysics(pReplayEntity, true);
				pReplayActor->m_flags |= eRAF_Invisible;
			}
		}
		else
		{
			if (pReplayActor->m_flags & eRAF_Invisible)
			{
				HideEntityKeepingPhysics(pReplayEntity, false);
				pReplayActor->m_flags &= ~eRAF_Invisible;
			}
		}

		// Ragdollize or Spawn Corpse.
		IPhysicalEntity* pPhysicalEntity = pReplayEntity->GetPhysics();
		bool isRagdoll = (pPhysicalEntity && pPhysicalEntity->GetType() == PE_ARTICULATED);
		if (isRagdoll != shouldBeRagdoll)
		{
			if (shouldBeRagdoll)
			{
				pReplayActor->Ragdollize();
			}
			else
			{
				// Player is no longer a ragdoll, check if we need to replace with a corpse
				uint8* startingplace = m_pPlaybackData->m_tpdatabuffer;   // Annoyingly we can't use the current buffer pos since the packet ordering is not guaranteed
				uint8* endingplace = startingplace + m_pPlaybackData->m_tpdatasize;
				bool bSpawnedCorpse = false;
				while (startingplace < endingplace)
				{
					SRecording_Packet* pPacket = (SRecording_Packet*) startingplace;

					if (pPacket->type == eTPP_CorpseSpawned)
					{
						SRecording_CorpseSpawned* pCorpsePacket = (SRecording_CorpseSpawned*) pPacket;
						if (pCorpsePacket->playerId == tpchar->eid)
						{
							ApplyCorpseSpawned(pCorpsePacket);
							bSpawnedCorpse = true;
							break;
						}
					}

					startingplace += pPacket->size;
				}

				CRecordingSystem::RemoveEntityPhysics(*pReplayEntity);
				if (bSpawnedCorpse)
				{
					// Hide this entity until we move it out the way of the corpse
					pReplayEntity->Hide(true);
					pReplayActor->m_flags |= eRAF_HaveSpawnedMyCorpse;
				}
			}
		}
	}
	if (pReplayEntity)
	{
		bool isCloaked = ((tpchar->playerFlags & eTPF_Cloaked) != 0);
		bool fade = true;

		IEntityRender* pIEntityRender = (pReplayEntity->GetRenderInterface());

		{
			//pIEntityRender->SetEffectLayerParams( tpchar->layerEffectParams );

			bool wasCloaked = false; //((pIEntityRender->GetMaterialLayersMask() & MTL_LAYER_CLOAK) != 0);
			if (pReplayActor && (isCloaked != wasCloaked))
			{
				// Calc cloak settings
				const bool isFriendly = pReplayActor->IsFriendly();
				const uint8 cloakColorChannel = EntityEffects::Cloak::GetCloakColorChannel(isFriendly);
				const bool bCloakFadeByDistance = isCloaked && !isFriendly;

				// Update the player cloak state
				const EntityId replayEntityId = pReplayEntity->GetId();
				const bool bIgnoreCloakRefractionColor = (m_killer == tpchar->eid);
				EntityEffects::Cloak::CloakEntity(replayEntityId, isCloaked, fade, g_pGameCVars->g_cloakBlendSpeedScale, bCloakFadeByDistance, cloakColorChannel, bIgnoreCloakRefractionColor);

				IGameFramework* pGameFramework = gEnv->pGameFramework;
				IItemSystem* pItemSystem = pGameFramework->GetIItemSystem();
				if (CItem* pItem = (CItem*)pItemSystem->GetItem(pReplayActor->GetGunId()))
				{
					// Also update the cloak state of the weapon and its attachments
					pItem->CloakSync(fade);
				}
			}
		}

		QuatT location;

		if (tpchar2)
		{
			location.SetNLerp(tpchar->entitylocation, tpchar2->entitylocation, lerpValue);
		}
		else
		{
			location = tpchar->entitylocation;
		}

		Vec3 originalLocation = location.t;

		if (updatePosition)
		{
			Vec3 myscale(1, 1, 1);

			if (tpchar->eid == m_victim)
			{
#ifdef RECSYS_DEBUG
				if (g_pGameCVars->kc_debugVictimPos)
				{
					IRenderAuxGeom* pRender = gEnv->pAuxGeomRenderer;
					pRender->DrawCylinder(location.t + Vec3(0, 0, 0.75f), Vec3(0, 0, 1), 0.1f, 1.5f, ColorB(255, 0, 0, 255));
				}
#endif  //RECSYS_DEBUG

				// Let's use the position of the victim sent from the killer to ensure it looks like the
				// killer is hitting the player
				GetVictimPosition(playbackTime, location.t);

#ifdef RECSYS_DEBUG
				if (g_pGameCVars->kc_debugVictimPos)
				{
					IRenderAuxGeom* pRender = gEnv->pAuxGeomRenderer;
					pRender->DrawCylinder(location.t + Vec3(0, 0, 0.75f), Vec3(0, 0, 1), 0.1f, 1.5f, ColorB(255, 255, 255, 255));
				}
#endif  //RECSYS_DEBUG
			}

			if (pReplayEntity)
			{
				if (pReplayEntity->IsHidden() && (pReplayActor && !(pReplayActor->m_flags & eRAF_HaveSpawnedMyCorpse)))
				{
					Vec3 dist = location.t - pReplayEntity->GetWorldPos();
					if (dist.GetLengthSquared() > 1.f)
					{
						pReplayEntity->Hide(false);
					}
				}

				pReplayEntity->SetPosRotScale(location.t, location.q, myscale);
			}
		}

		const bool bCanMoveAndAim = (pReplayActor && !shouldBeRagdoll && updatePosition && (pReplayActor->m_flags & eRAF_HaveSpawnedMyCorpse) == 0);
		if (bCanMoveAndAim)
		{
			ICharacterInstance* ici = pReplayEntity->GetCharacter(0);
			if (ici)
			{
				ISkeletonAnim* isa = ici->GetISkeletonAnim();
				ISkeletonPose* pSkeletonPose = ici->GetISkeletonPose();

				Vec3 velocity;
				Vec3 aimTarget;
				if (tpchar2)
				{
					velocity.SetLerp(tpchar->velocity, tpchar2->velocity, lerpValue);
					aimTarget.SetLerp(tpchar->aimdir, tpchar2->aimdir, lerpValue);
				}
				else
				{
					velocity = tpchar->velocity;
					aimTarget = tpchar->aimdir;
				}

				if (g_pGameCVars->kc_useAimAdjustment)
				{
					if (updatePosition && (tpchar->eid == m_victim))
					{
						GetVictimAimTarget(aimTarget, originalLocation, location.t, lerpValue);
					}
				}

				velocity = !location.q * velocity;

				SBasicReplayMovementParams* pMovement = pReplayActor ? &pReplayActor->GetBasicMovement() : NULL;
				if (pMovement)
				{
					pMovement->SetDesiredLocalLocation2(isa, QuatT(velocity, IDENTITY), 1, frameTime, 1.0f);
				}

				IAnimationPoseBlenderDir* pIPoseBlenderAim = pSkeletonPose->GetIPoseBlenderAim();
				if (pIPoseBlenderAim)
				{
					pIPoseBlenderAim->SetTarget(aimTarget);
					pIPoseBlenderAim->SetPolarCoordinatesSmoothTimeSeconds(0.f);
				}

				ICharacterInstance* pCharacterShadow = pReplayEntity->GetCharacter(CReplayActor::ShadowCharacterSlot);
				if (pCharacterShadow)
				{
					ISkeletonPose* pSkeletonPoseShadow = pCharacterShadow->GetISkeletonPose();
					IAnimationPoseBlenderDir* pIPoseBlenderAimShadow = pSkeletonPoseShadow->GetIPoseBlenderAim();
					if (pIPoseBlenderAimShadow)
					{
						pIPoseBlenderAimShadow->SetTarget(aimTarget);
						pIPoseBlenderAimShadow->SetPolarCoordinatesSmoothTimeSeconds(0.f);
					}
					if (pMovement)
					{
						pMovement->SetDesiredLocalLocation2(pCharacterShadow->GetISkeletonAnim(), QuatT(velocity, IDENTITY), 1, frameTime, 1.0f);
					}
				}
			}
		}
	}
	else
	{
		RecSysLog("RecordingSystem Could not find player with ID: %d", tpchar->eid);
	}
}

void CRecordingSystem::CloakEnable(IEntityRender* pIEntityRender, bool enable, bool fade)
{
}

void CRecordingSystem::ApplyEntitySpawn(const SRecording_EntitySpawn* entitySpawn, float time)
{
	if (entitySpawn->pClass == m_pCorpse)
	{
		return;
	}

	if (entitySpawn->entityId == 0 || m_replayEntities.count(entitySpawn->entityId) > 0)
	{
		// Don't spawn the same entity twice
		CRY_ASSERT_MESSAGE(entitySpawn->entityId == 0, "CRecordingSystem::ApplyEntitySpawn Shouldn't be trying to spawn the same entity twice!");
		return;
	}

	SEntitySpawnParams params;
	const char* sClassType = "ReplayObject";
	SmartScriptTable pPropertiesTable;

	if (!entitySpawn->useOriginalClass)
	{
		params.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(sClassType);
	}
	else
	{
		params.pClass = entitySpawn->pClass;
		if (entitySpawn->pScriptTable)
		{
			entitySpawn->pScriptTable->GetValue("Properties", pPropertiesTable);
			if (pPropertiesTable)
			{
				params.pPropertiesTable = pPropertiesTable;
			}
		}

		if (IEntity* pOrigEntity = gEnv->pEntitySystem->GetEntity(entitySpawn->entityId))
		{
			params.pArchetype = pOrigEntity->GetArchetype();
		}
	}
	char sName[64];
	cry_sprintf(sName, "%s%s%d", entitySpawn->useOriginalClass ? "Replay" : "", params.pClass->GetName(), m_nReplayEntityNumber);
	m_nReplayEntityNumber++;
	params.sName = sName;
	params.nFlags = (ENTITY_FLAG_NO_PROXIMITY | ENTITY_FLAG_CLIENT_ONLY | entitySpawn->entityFlags);
	params.vPosition = entitySpawn->entitylocation.t;
	params.qRotation = entitySpawn->entitylocation.q;
	params.vScale = entitySpawn->entityScale;

	if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(params, false))
	{
		m_autoHideIgnoreEntityId = pEntity->GetId();
		bool bSuccess = gEnv->pEntitySystem->InitEntity(pEntity, params);
		m_autoHideIgnoreEntityId = 0;

		if (!bSuccess)
		{
			return;
		}

		if (entitySpawn->useOriginalClass)
		{
			pEntity->EnablePhysics(false);
		}

		CReplayObject* pReplayObject = NULL;
		CGameObject* pGameObject = NULL;
		if (time > 0 && !entitySpawn->useOriginalClass)
		{
			if (pGameObject = (CGameObject*)pEntity->GetProxy(ENTITY_PROXY_USER))
			{
				if (pReplayObject = (CReplayObject*)pGameObject->QueryExtension(sClassType))
				{
					pReplayObject->SetTimeSinceSpawn(time);
				}
			}
		}

		if (!entitySpawn->useOriginalClass)
		{
			for (int i = 0; i < RECORDING_SYSTEM_MAX_SLOTS; i++)
			{
				if (entitySpawn->szCharacterSlot[i])
				{
#if defined(_DEBUG)
					// Verify the string pointed to is still valid
					int length = strlen(entitySpawn->szCharacterSlot[i]);
					const char* pExtension = entitySpawn->szCharacterSlot[i] + length - 3;
					bool validString = (stricmp(pExtension, "cga") == 0 || stricmp(pExtension, "cdf") == 0 || stricmp(pExtension, "chr") == 0);
					CRY_ASSERT_MESSAGE(validString, "The string appears to be corrupt (please tell martinsh)");
#endif
					pEntity->LoadCharacter(i, entitySpawn->szCharacterSlot[i]);
				}
				if (entitySpawn->pStatObj[i])
				{
					IStatObj* pStatObj = entitySpawn->pStatObj[i];
					if (!m_killerIsFriendly)
					{
						TStatObjMap::iterator itReplacement = m_replacementStatObjs.find(entitySpawn->pStatObj[i]);
						if (itReplacement != m_replacementStatObjs.end())
						{
							pStatObj = itReplacement->second;
						}
					}

					pEntity->SetStatObj(pStatObj, i | ENTITY_SLOT_ACTUAL, false);
					pEntity->SetSlotFlags(i, entitySpawn->slotFlags[i]);
				}
				Matrix34 localTM = IDENTITY;
				localTM.SetTranslation(entitySpawn->slotOffset[i]);
				pEntity->SetSlotLocalTM(i, localTM);
			}
		}

		if (entitySpawn->pClass == m_pInteractiveObjectExClass)
		{
			TInteractiveObjectAnimationMap::const_iterator result = m_interactiveObjectAnimations.find(entitySpawn->entityId);
			if (result != m_interactiveObjectAnimations.end())
			{
				if (m_pDatabaseInteractiveObjects && m_pPlayerControllerDef && pReplayObject && pGameObject)
				{
					IMannequin& mannequinSys = gEnv->pGameFramework->GetMannequinInterface();
					SAnimationContext animContext(*m_pPlayerControllerDef);

					IActionController* pActionController = mannequinSys.CreateActionController(pEntity, animContext);
					const uint32 scopeContextID = PlayerMannequin.contextIDs.SlaveObject;
					ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
					TagState fragTags = TAG_STATE_EMPTY;

					// Set up the ActionController to play the correct animation.
					pActionController->SetScopeContext(scopeContextID, *pEntity, pCharacter, m_pDatabaseInteractiveObjects);
					if (const CTagDefinition* pTagDef = m_pPlayerControllerDef->GetFragmentTagDef(m_interactFragmentID))
					{
						pTagDef->Set(fragTags, result->second.m_interactionTypeTag, true);
					}
					IAction* pAction = new TAction<SAnimationContext>(0, m_interactFragmentID, fragTags);
					pAction->SetOptionIdx(result->second.m_interactionIndex);
					pActionController->Queue(*pAction);

					// Set the time increments to non-zero to force the ActionController::Update() to drive the animation to the end.
					// When time increment is zero, animation position will not update. This will be changed to a simpler process by Tom Berry at some point.
					const uint32 totalScopes = pActionController->GetTotalScopes();
					for (uint32 s = 0; s < totalScopes; s++)
					{
						pActionController->GetScope(s)->IncrementTime(0.001f);
					}
					pActionController->Update(1000.0f);

					// "false" here leaves the anim on the transition queue in the animation system so it isn't cleared on pActionController->Release().
					pActionController->ClearScopeContext(scopeContextID, false);

					pActionController->Release();

					// Activate the Entity. Without this, the animation won't play or process.
					if (pReplayObject && pGameObject)
					{
						pGameObject->EnableUpdateSlot(pReplayObject, 0);
						pGameObject->SetUpdateSlotEnableCondition(pReplayObject, 0, eUEC_Always);
					}
				}
			}
		}

		if (entitySpawn->hidden)
			HideItem(pEntity, true);

		if (entitySpawn->pMaterial)
		{
			pEntity->SetMaterial(entitySpawn->pMaterial);
		}

		m_replayEntities[entitySpawn->entityId] = pEntity->GetId();

		if (entitySpawn->subObjHideMask != 0)
		{
			pEntity->SetSubObjHideMask(0, entitySpawn->subObjHideMask);
		}
	}
}

void CRecordingSystem::ApplyEntityRemoved(const SRecording_EntityRemoved* entityRemoved)
{
	if (CReplayActor* pReplayActor = GetReplayActor(entityRemoved->entityId, true))
	{
		if ((pReplayActor->m_flags & eRAF_HaveSpawnedMyCorpse) != 0)
		{
			return;
		}
	}

	TReplayEntityMap::iterator itEntity = m_replayEntities.find(entityRemoved->entityId);
	if (itEntity != m_replayEntities.end())
	{
		EntityId replayEntityId = itEntity->second;
		gEnv->pEntitySystem->RemoveEntity(replayEntityId);
		m_replayEntities.erase(itEntity);
	}
}

void CRecordingSystem::ApplyEntityLocation(const SRecording_EntityLocation* entityLocation, const SRecording_EntityLocation* entityLocation2, float lerpValue)
{
	QuatT location;
	if (entityLocation2)
	{
		location.SetNLerp(entityLocation->entitylocation, entityLocation2->entitylocation, lerpValue);
	}
	else
	{
		location = entityLocation->entitylocation;
	}

	if (entityLocation->entityId != m_projectileId)
	{
		TReplayEntityMap::iterator itEntity = m_replayEntities.find(entityLocation->entityId);
		if (itEntity != m_replayEntities.end())
		{
			EntityId replayEntityId = itEntity->second;
			if (IEntity* pReplayEntity = gEnv->pEntitySystem->GetEntity(replayEntityId))
			{
				if (pReplayEntity->GetClass() != m_pCorpse)
				{
					const Vec3& myscale = pReplayEntity->GetScale();
					pReplayEntity->SetPosRotScale(location.t, location.q, myscale);
				}
			}
		}
	}
	else
	{
		m_newProjectilePosition = location;
	}
}

void CRecordingSystem::ApplyEntityHide(const SRecording_EntityHide* entityHide)
{
	IEntity* pReplayEntity = GetReplayEntity(entityHide->entityId);
	if (pReplayEntity)
	{
		HideItem(pReplayEntity, entityHide->hide);
	}
}

void CRecordingSystem::HideItem(IEntity* pEntity, bool hide)
{
	pEntity->Hide(hide);
	CItem* pItem = (CItem*)g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pEntity->GetId());
	if (pItem)
	{
		const CItem::TAccessoryArray& accessories = pItem->GetAccessories();
		const int numAccessories = accessories.size();
		for (int i = 0; i < numAccessories; i++)
		{
			IEntity* pAccessory = gEnv->pEntitySystem->GetEntity(accessories[i].accessoryId);
			if (pAccessory)
			{
				pAccessory->Hide(hide);
			}
		}
	}
}

const STracerParams* CRecordingSystem::GetKillerTracerParams()
{
	CReplayActor* pReplayActor = GetReplayActor(m_killer, true);
	if (pReplayActor)
	{
		IGameFramework* pGameFramework = gEnv->pGameFramework;
		IItemSystem* pItemSystem = pGameFramework->GetIItemSystem();

		if (IItem* pItem = pItemSystem->GetItem(pReplayActor->GetGunId()))
		{
			IWeapon* pWeapon = pItem->GetIWeapon();
			if (pWeapon)
			{
				CFireMode* pFireMode = (CFireMode*)pWeapon->GetFireMode(pWeapon->GetCurrentFireMode());
				if (pFireMode)
				{
					const SFireModeParams* pFireModeParams = pFireMode->GetShared();
					if (pFireModeParams)
					{
						return &pFireModeParams->tracerparams;
					}
				}
			}
		}
	}
	return NULL;
}

void CRecordingSystem::ApplySpawnCustomParticle(const SRecording_SpawnCustomParticle* spawnCustomParticle)
{
	if (spawnCustomParticle->pParticleEffect)
	{
		spawnCustomParticle->pParticleEffect->Spawn(true, spawnCustomParticle->location);
	}
}

bool CRecordingSystem::ApplyParticleCreated(const SRecording_ParticleCreated* particleCreated, float time)
{
	bool success = false;
	if (particleCreated->pParticleEffect)
	{
		RecSysLogDebug(eRSD_Particles, "Spawning replay particle effect: %s", particleCreated->pParticleEffect->GetName());
		IParticleEffect* pParticleEffect = particleCreated->pParticleEffect;
		if (!m_killerIsFriendly)
		{
			TParticleEffectMap::iterator itReplacementEffect = m_replacementParticleEffects.find(particleCreated->pParticleEffect);
			if (itReplacementEffect != m_replacementParticleEffects.end())
			{
				pParticleEffect = itReplacementEffect->second;
			}
		}
		if (particleCreated->entityId != 0)
		{
			// Attach the particle effect to the replay entity
			TReplayEntityMap::iterator itReplayEntity = m_replayEntities.find(particleCreated->entityId);
			if (itReplayEntity != m_replayEntities.end())
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(itReplayEntity->second);
				if (pEntity)
				{
					if (pEntity->GetWorldPos().IsZero())
					{
						pEntity->SetWorldTM(Matrix34(particleCreated->location));
					}
					int slot = pEntity->LoadParticleEmitter(particleCreated->entitySlot, pParticleEffect);
					if (slot >= 0)
					{
						IParticleEmitter* pParticleEmitter = pEntity->GetParticleEmitter(slot);
						if (pParticleEmitter)
						{
							// We need to keep track of it so that it can be removed at the right time
							m_replayParticles[particleCreated->pParticleEmitter] = pParticleEmitter;
							pParticleEmitter->AddRef();
							success = true;
						}
					}
				}
			}
			else
			{
				RecSysLogDebug(eRSD_Particles, "Particle [%s] could not attach to entity.", particleCreated->pParticleEffect->GetName());
			}
		}
		else
		{
			if (m_replayParticles.count(particleCreated->pParticleEmitter) == 0)
			{
				// The particle effect is not attached to any entity
				IParticleEmitter* pParticleEmitter = pParticleEffect->Spawn(false, particleCreated->location);
				if (pParticleEmitter)
				{
					// We need to keep track of it so that it can be removed at the right time
					m_replayParticles[particleCreated->pParticleEmitter] = pParticleEmitter;
					pParticleEmitter->AddRef();
				}
				success = true;
			}
		}
	}
	return success;
}

void CRecordingSystem::ApplyParticleDeleted(const SRecording_ParticleDeleted* particleDeleted)
{
	TReplayParticleMap::iterator itParticle = m_replayParticles.find(particleDeleted->pParticleEmitter);
	if (itParticle != m_replayParticles.end())
	{
		IParticleEmitter* pReplayParticleEmitter = itParticle->second;
		gEnv->pParticleManager->DeleteEmitter(pReplayParticleEmitter);
		pReplayParticleEmitter->Release();
		m_replayParticles.erase(itParticle);
	}
}

void CRecordingSystem::ApplyParticleLocation(const SRecording_ParticleLocation* particleLocation)
{
	TReplayParticleMap::iterator itParticle = m_replayParticles.find(particleLocation->pParticleEmitter);
	if (itParticle != m_replayParticles.end())
	{
		IParticleEmitter* pReplayParticleEmitter = itParticle->second;
		pReplayParticleEmitter->SetMatrix(particleLocation->location);
	}
}

void CRecordingSystem::ApplyParticleTarget(const SRecording_ParticleTarget* particleTarget)
{
	TReplayParticleMap::iterator itParticle = m_replayParticles.find(particleTarget->pParticleEmitter);
	if (itParticle != m_replayParticles.end())
	{
		IParticleEmitter* pReplayParticleEmitter = itParticle->second;
		ParticleTarget target;
		target.vTarget = particleTarget->targetPos;
		target.vVelocity = particleTarget->velocity;
		target.fRadius = particleTarget->radius;
		target.bTarget = particleTarget->target;
		target.bPriority = particleTarget->priority;
		pReplayParticleEmitter->SetTarget(target);
	}
}

#ifdef RECSYS_DEBUG
bool CRecordingSystem::OnInputEvent(const SInputEvent& rInputEvent)
{
	if (gEnv->pConsole->IsOpened() || !CRecordingSystem::KillCamEnabled() || !g_pGameCVars->kc_debug)
		return false;

	if (rInputEvent.deviceType == eIDT_Keyboard && rInputEvent.state == eIS_Pressed)
	{
		static int highlightIndex = 0;

		if (rInputEvent.keyId == eKI_Minus)
		{
			PrintCurrentRecordingBuffers();
			return true;
		}
		else if (rInputEvent.keyId == eKI_P)
		{
			DebugStoreHighlight(g_pGameCVars->kc_length, highlightIndex);
			PlayHighlight(highlightIndex);
			return true;
		}
		else if (rInputEvent.keyId == eKI_H)
		{
			DebugStoreHighlight(6.0f, highlightIndex);
			return true;
		}
		else if (rInputEvent.keyId == eKI_L)
		{
			PlayHighlight(highlightIndex);
			return true;
		}
		else if (rInputEvent.keyId == eKI_1)
		{
			highlightIndex = 0;
		}
		else if (rInputEvent.keyId == eKI_2)
		{
			highlightIndex = 1;
		}
		else if (rInputEvent.keyId == eKI_3)
		{
			highlightIndex = 2;
		}
		else if (rInputEvent.keyId == eKI_4)
		{
			highlightIndex = 3;
		}
	}
	return false;
}

void CRecordingSystem::DebugStoreHighlight(const float length, const int highlightIndex)
{
	const float currTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	const float startTime = currTime - length;
	SKillInfo kill;
	kill.deathTime = startTime + g_pGameCVars->kc_kickInTime;
	kill.killerId = gEnv->pGameFramework->GetClientActorId();
	kill.localPlayerTeam = g_pGame->GetGameRules()->GetTeam(gEnv->pGameFramework->GetClientActorId());

	SRecordedKill data(kill, 0.f, startTime);
	SaveHighlight(data, highlightIndex);
}
#endif //RECSYS_DEBUG

SRecordingEntity::EEntityType CRecordingSystem::RecordEntitySpawn(IEntity* pEntity)
{
	SRecording_EntitySpawn entitySpawn;
	entitySpawn.pClass = pEntity->GetClass();
	entitySpawn.entityFlags = pEntity->GetFlags() & ENTITY_FLAG_CASTSHADOW; // Just record certain flags (just shadow casting for now)
	entitySpawn.entitylocation.t = pEntity->GetWorldPos();
	entitySpawn.entitylocation.q = pEntity->GetWorldRotation();
	entitySpawn.entityScale = pEntity->GetScale();
	entitySpawn.entityId = pEntity->GetId();
	entitySpawn.hidden = pEntity->IsHidden();
	if (entitySpawn.pMaterial = pEntity->GetMaterial())
	{
		entitySpawn.pMaterial->AddRef();
	}

	RecordEntitySpawnGeometry(&entitySpawn, pEntity);

	SRecordingEntity::EEntityType entityType = SRecordingEntity::eET_Generic;
	CActor* pActor = (CActor*)gEnv->pGameFramework->GetIActorSystem()->GetActor(pEntity->GetId());
	if (pActor)
	{
		// If it is an actor then record the initial weapon
		m_newActorEntities.push_back(pEntity->GetId());
		entityType = SRecordingEntity::eET_Actor;
	}
	else
	{
		CItem* pItem = (CItem*)g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pEntity->GetId());
		if (pItem)
		{
			IWeapon* pWeapon = pItem->GetIWeapon();
			if (pWeapon)
			{
				pWeapon->AddEventListener(this, "CRecordingSystem for the killcam");
			}
			entitySpawn.useOriginalClass = true;
			entityType = SRecordingEntity::eET_Item;
		}
		else
		{
			IVehicle* pVehicle = g_pGame->GetIGameFramework()->GetIVehicleSystem()->GetVehicle(pEntity->GetId());
			if (pVehicle)
			{
				entitySpawn.useOriginalClass = true;
				entitySpawn.pScriptTable = pEntity->GetScriptTable();
				entitySpawn.pScriptTable->AddRef();
			}
		}
	}
	m_pBuffer->AddPacket(entitySpawn);
	return entityType;
}

void CRecordingSystem::UpdateEntitySpawnGeometry(IEntity* pEntity)
{
	const EntityId entityId = pEntity->GetId();
	for (CRecordingBuffer::iterator it = m_pBuffer->begin(), end = m_pBuffer->end(); it != end; ++it)
	{
		if (it->type == eTPP_EntitySpawn)
		{
			SRecording_EntitySpawn* pEntitySpawn = (SRecording_EntitySpawn*)&*it;
			if (pEntitySpawn->entityId == entityId)
			{
				ClearEntitySpawnGeometry(pEntitySpawn);
				RecordEntitySpawnGeometry(pEntitySpawn, pEntity);
			}
		}
	}
	TEntitySpawnMap::iterator itDiscarded = m_recordedData.m_discardedEntitySpawns.find(pEntity->GetId());
	if (itDiscarded != m_recordedData.m_discardedEntitySpawns.end())
	{
		SRecording_EntitySpawn* pEntitySpawn = &itDiscarded->second.first;
		if (pEntitySpawn->entityId == entityId)
		{
			ClearEntitySpawnGeometry(pEntitySpawn);
			RecordEntitySpawnGeometry(pEntitySpawn, pEntity);
		}
	}
}

void CRecordingSystem::ClearEntitySpawnGeometry(SRecording_EntitySpawn* pEntitySpawn)
{
	// Clear out the existing geometry from the spawn packet
	for (int i = 0; i < RECORDING_SYSTEM_MAX_SLOTS; ++i)
	{
		if (pEntitySpawn->pStatObj[i])
		{
			pEntitySpawn->pStatObj[i]->Release();
		}
		pEntitySpawn->szCharacterSlot[i] = NULL;
		pEntitySpawn->pStatObj[i] = NULL;
		pEntitySpawn->slotFlags[i] = 0;
	}
}

void CRecordingSystem::RecordEntitySpawnGeometry(SRecording_EntitySpawn* pEntitySpawn, IEntity* pEntity)
{
	SEntitySlotInfo slotInfo;
	const int numSlots = pEntity->GetSlotCount();

	if (numSlots == 0)
		return;

	if (numSlots > RECORDING_SYSTEM_MAX_SLOTS)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "CRecordingSystem::RecordEntitySpawn, %s has %d slots but only recording %d of them",
		           pEntity->GetClass()->GetName(), numSlots, RECORDING_SYSTEM_MAX_SLOTS);
	}

	int maxSlots = min(numSlots, RECORDING_SYSTEM_MAX_SLOTS);

	enum EScaling
	{
		eS_INVALID   = 0,
		eS_NotScaled = 1,
		eS_Scaled    = 2
	};

	EScaling currentScaling = eS_INVALID;
	float fFoundScale = 0.0f;
	for (int i = 0; i < maxSlots; ++i)
	{
		if (pEntity->GetSlotInfo(i, slotInfo))
		{
			if (slotInfo.pStatObj)
			{
				pEntitySpawn->pStatObj[i] = slotInfo.pStatObj;
				pEntitySpawn->slotFlags[i] = slotInfo.nFlags;

				slotInfo.pStatObj->AddRef();
			}
			else if (slotInfo.pCharacter)
			{
				pEntitySpawn->szCharacterSlot[i] = CacheString(slotInfo.pCharacter->GetFilePath(), eSC_Model);
			}
			pEntitySpawn->slotOffset[i] = slotInfo.pLocalTM->GetTranslation();

			const float fScaleXSqr = slotInfo.pLocalTM->GetColumn0().GetLengthSquared();
			const float fScaleYSqr = slotInfo.pLocalTM->GetColumn1().GetLengthSquared();
			const float fScaleZSqr = slotInfo.pLocalTM->GetColumn2().GetLengthSquared();
			if (fabsf(1.f - fScaleXSqr) > 0.001f && fabsf(fScaleXSqr - fScaleYSqr) < 0.001f && fabsf(fScaleXSqr - fScaleZSqr) < 0.001f)
			{
				CRY_ASSERT_MESSAGE(currentScaling != eS_NotScaled, "A previous slot has no scaling, but this one does!");

				if (currentScaling == eS_INVALID)
				{
					const float fScaleX = sqrtf(fScaleXSqr);
					fFoundScale = fScaleX;
					pEntitySpawn->entityScale *= fScaleX;
					currentScaling = eS_Scaled;
				}
				else if (currentScaling == eS_Scaled)
				{
					CRY_ASSERT_MESSAGE(fabsf(sqr(fFoundScale) - fScaleXSqr) < 0.001f, "More than one different scale being applied to entity slots");
				}
			}
			else
			{
				CRY_ASSERT_MESSAGE(currentScaling == eS_NotScaled || currentScaling == eS_INVALID, "Non scaled slot on the same render proxy as a scaled slot");
				currentScaling = eS_NotScaled;
			}
		}
	}

#if defined(_DEBUG)
	bool geometryFound = false;
	for (int i = 0; i < maxSlots; ++i)
	{
		if (pEntitySpawn->pStatObj[i] || pEntitySpawn->szCharacterSlot[i])
		{
			geometryFound = true;
			break;
		}
	}
	if (!geometryFound && m_validWithNoGeometry.count(pEntitySpawn->pClass) == 0)
	{
		RecSysLog("Tried to record geometry spawn for class[%s]. If this class can validly contain no geometry, please add it to the \"NoGeometryValid\" block in KillCam.xml", pEntitySpawn->pClass->GetName());
		CRY_ASSERT_MESSAGE(false, "Neither a character nor a statobj found in the entity.");
	}
#endif
}

void CRecordingSystem::RecordActorWeapon()
{
	std::vector<EntityId>::iterator itEntity;
	for (itEntity = m_newActorEntities.begin(); itEntity != m_newActorEntities.end(); ++itEntity)
	{
		CActor* pActor = (CActor*)gEnv->pGameFramework->GetIActorSystem()->GetActor(*itEntity);
		if (pActor)
		{
			// If it is an actor then record the initial weapon
			EntityId itemId = pActor->GetCurrentItemId(false);
			CWeapon* pCurrentWeapon = pActor->GetWeapon(itemId);
			if (pCurrentWeapon)
			{
				RecordWeaponSelection(pCurrentWeapon, true);
			}
		}
	}
	m_newActorEntities.clear();
}

void CRecordingSystem::OnAttachAccessory(CItem* pItem)
{
	stl::push_back_unique(m_itemAccessoriesChanged, pItem->GetEntityId());
}

void CRecordingSystem::RecordWeaponAccessories()
{
	std::vector<EntityId>::iterator itEntity;
	for (itEntity = m_itemAccessoriesChanged.begin(); itEntity != m_itemAccessoriesChanged.end(); ++itEntity)
	{
		CItem* pItem = (CItem*)g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(*itEntity);
		if (pItem)
		{
			IEntity* pEntity = pItem->GetEntity();
			RecordWeaponAccessories(pItem);
		}
	}
	m_itemAccessoriesChanged.clear();
}

void CRecordingSystem::RecordWeaponAccessories(CItem* pItem)
{
	SRecording_WeaponAccessories weaponAccessories;
	weaponAccessories.weaponId = pItem->GetEntityId();
	const CItem::TAccessoryArray& accessories = pItem->GetAccessories();
	const int numAccessories = accessories.size();

	if (numAccessories == 0)
	{
		return;
	}

	for (int i = 0; i < MAX_WEAPON_ACCESSORIES; i++)
	{
		if (i < numAccessories)
		{
			weaponAccessories.pAccessoryClasses[i] = accessories[i].pClass;
		}
		else
		{
			weaponAccessories.pAccessoryClasses[i] = NULL;
		}
	}

	m_pBuffer->AddPacket(weaponAccessories);
}

void CRecordingSystem::RecordWeaponSelection(CWeapon* pWeapon, bool selected)
{
	if (m_replayEventGuard == 0)
	{
		SRecording_WeaponSelect weaponSelect;
		weaponSelect.ownerId = pWeapon->GetOwnerId();
		weaponSelect.weaponId = pWeapon->GetEntityId();
		weaponSelect.isSelected = selected;
		if (selected)
		{
			weaponSelect.isRippedOff = pWeapon->IsMountable() && !pWeapon->IsMounted();
		}
		m_pBuffer->AddPacket(weaponSelect);
	}
}

void CRecordingSystem::RecordEntityInfo()
{
	for (TRecEntitiesMap::iterator it = m_recordingEntities.begin(), end = m_recordingEntities.end(); it != end; )
	{
		SRecordingEntity& recEnt = it->second;
		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(recEnt.entityId))
		{
			if (recEnt.type == SRecordingEntity::eET_Actor)
			{
				// If this entity is an actor then record it as a TPChar packet which contains more information
				// such as cloak state, aim IK and velocity
				CActor* pActor = (CActor*)gEnv->pGameFramework->GetIActorSystem()->GetActor(recEnt.entityId);
				if (pActor)
				{
					RecordTPCharPacket(pEntity, pActor);
				}
			}
			// Don't record the location of hidden entities.
			else if (recEnt.hiddenState == SRecordingEntity::eHS_Unhide)
			{
				Matrix34 currentTM = pEntity->GetWorldTM();
				currentTM.OrthonormalizeFast();
				QuatT currentLocation(currentTM);

				// Don't record the position of entities which have not moved since the last frame
				if (currentLocation.t != recEnt.location.t || currentLocation.q != recEnt.location.q)
				{
					IItem* pItem = NULL;
					if (recEnt.type == SRecordingEntity::eET_Item)
					{
						pItem = gEnv->pGameFramework->GetIItemSystem()->GetItem(recEnt.entityId);
					}
					// Don't record the position of selected items because their position will be controlled
					// by the actor it is attached to
					if (!pItem || !pItem->IsSelected())
					{
						SRecording_EntityLocation entityLocation;
						entityLocation.entityId = recEnt.entityId;
						entityLocation.entitylocation = currentLocation;
						m_pBuffer->AddPacket(entityLocation);
						recEnt.location = currentLocation;
					}
				}
			}
			++it;
		}
		else
		{
			// Shouldn't really ever reach this point because CRecordingSystem::OnRemove
			// should have got rid of it. But leave this here to be safe.
			SRecording_EntityRemoved entityRemoved;
			entityRemoved.entityId = recEnt.entityId;
			m_pBuffer->AddPacket(entityRemoved);
			RemoveRecordingEntity(recEnt);
			m_recordingEntities.erase(it++);
		}
	}

	std::vector<SRecordingAnimObject>::iterator itAnimObject;
	for (itAnimObject = m_recordingAnimEntities.begin(); itAnimObject != m_recordingAnimEntities.end(); )
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(itAnimObject->entityId);
		if (pEntity)
		{
			ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
			if (pCharacter)
			{
				ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
				if (pSkeletonAnim)
				{
					int numAnims = pSkeletonAnim->GetNumAnimsInFIFO(0);
					if (numAnims)
					{
						CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(0, 0);
						float time = pSkeletonAnim->GetLayerNormalizedTime(0);

						if ((itAnimObject->animId != anim.GetAnimationId()) || (itAnimObject->fTime != time))
						{
							SRecording_AnimObjectUpdated animObjectUpdated;
							animObjectUpdated.entityId = itAnimObject->entityId;
							animObjectUpdated.animId = anim.GetAnimationId();
							animObjectUpdated.fTime = time;
							m_pBuffer->AddPacket(animObjectUpdated);

							itAnimObject->animId = anim.GetAnimationId();
							itAnimObject->fTime = time;
						}
					}
				}
			}
			++itAnimObject;
		}
		else
		{
			itAnimObject = m_recordingAnimEntities.erase(itAnimObject);
		}
	}
}

void CRecordingSystem::RecordParticleInfo()
{
	std::vector<SRecording_ParticleCreated>::iterator itParticle;
	for (itParticle = m_newParticles.begin(); itParticle != m_newParticles.end(); ++itParticle)
	{
		SRecording_ParticleCreated& particle = *itParticle;
		IParticleEmitter* pEmitter = particle.pParticleEmitter;

		if (IsPlayingBack())
		{
			// Remove any particles from the real world during playback, I'm assuming here that the
			// particle effect is going to be fairly short lived and won't be missed if it is not
			// visible when coming out of the killcam replay.
			REPLAY_EVENT_GUARD
			gEnv->pParticleManager->DeleteEmitter(pEmitter);
		}
		else
		{
			if (m_excludedParticleEffects.count(particle.pParticleEffect) == 0)
			{
				// Record if the particle is attached to any entity or not
#ifdef RECSYS_DEBUG
				if (IEntity* pParentEntity = gEnv->pEntitySystem->GetEntity(pEmitter->GetAttachedEntityId()))
				{
					RecSysLogDebug(eRSD_Particles, "Emitter[%s: %s] Attached to Entity[%s: %s]", particle.pParticleEffect->GetName(), particle.pParticleEmitter->GetName(), pParentEntity->GetClass()->GetName(), pParentEntity->GetName());
					RecSysLogDebugIf(eRSD_General,
					                 (m_recordingEntities.find(pEmitter->GetAttachedEntityId()) == m_recordingEntities.end()),
					                 "NOT RECORDING PARENT ENTITY[%s: %s] FOR PARTICLE[%s: %s]!",
					                 pParentEntity->GetClass()->GetName(), pParentEntity->GetName(),
					                 particle.pParticleEffect->GetName(), particle.pParticleEmitter->GetName());
				}
#endif  //RECSYS_DEBUG
				particle.entityId = pEmitter->GetAttachedEntityId();
				if (IEntity* pParentEntity = gEnv->pEntitySystem->GetEntity(particle.entityId))
				{
					particle.location = QuatTS(pParentEntity->GetWorldTM());
				}
				particle.entitySlot = pEmitter->GetAttachedEntitySlot();
				m_pBuffer->AddPacket(particle);
			}
		}
		pEmitter->Release();
	}
	m_newParticles.clear();
}

bool CRecordingSystem::OnBeforeSpawn(SEntitySpawnParams& params)
{
	CRY_ASSERT_MESSAGE(false, "Shouldn't be getting these events");
	return true;
}

void CRecordingSystem::OnSpawn(IEntity* pEntity, SEntitySpawnParams& params)
{
	if (!IsRecording())
		return;

	const EntityId entityId = pEntity->GetId();

	static const IEntityClass* kPlayerClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Player");
	const bool bIsPlayer = params.pClass == kPlayerClass;
	if (!m_replayEventGuard && (m_recordEntityClassFilter.count(params.pClass)))
	{
		AddRecordingEntity(pEntity, bIsPlayer ? SRecordingEntity::eET_Actor : RecordEntitySpawn(pEntity));
	}
	else
	{
		if (!m_replayEventGuard)
		{
			RecSysLogDebug(eRSD_General, "Not recording Entity spawn of Class[%s:%s]", params.pClass->GetName(), pEntity->GetName());
		}
		if (m_autoHideIgnoreEntityId && (m_autoHideIgnoreEntityId != entityId))
		{
			pEntity->Hide(true);
		}
	}
}

bool CRecordingSystem::OnRemove(IEntity* pEntity)
{
	if (IsRecording() && m_recordEntityClassFilter.count(pEntity->GetClass()))
	{
		RecSysLogDebug(eRSD_General, "Removing entity (%s) of type: %s", pEntity->GetName(), pEntity->GetClass()->GetName());
		RecSysLogDebug(eRSD_Visibility, "Removing entity (%s) of type: %s", pEntity->GetName(), pEntity->GetClass()->GetName());
		EntityId removedEntityId = pEntity->GetId();
		TRecEntitiesMap::iterator find = m_recordingEntities.find(removedEntityId);
		if (find != m_recordingEntities.end())
		{
			static const IEntityClass* kPlayerClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Player");
			if (pEntity->GetClass() != kPlayerClass)
			{
				SRecording_EntityRemoved entityRemoved;
				entityRemoved.entityId = removedEntityId;
				m_pBuffer->AddPacket(entityRemoved);
			}
			RemoveRecordingEntity(find->second);
			UnregisterReplayMannListener(removedEntityId);
			m_recordingEntities.erase(find);
		}
	}
	return true;
}

void CRecordingSystem::OnReused(IEntity* pEntity, SEntitySpawnParams& params)
{
	CRY_ASSERT_MESSAGE(false, "CRecordingSystem::OnReused needs implementing");
}

void CRecordingSystem::OnEntityEvent(IEntity* pEntity, const SEntityEvent& event)
{
	const EntityId entityId = pEntity->GetId();

	switch (event.event)
	{
	case ENTITY_EVENT_UNHIDE:
	case ENTITY_EVENT_HIDE:
		{
			const bool bHiding = (event.event == ENTITY_EVENT_HIDE);
			m_recordingEntities[entityId].hiddenState = bHiding ? SRecordingEntity::eHS_Hide : SRecordingEntity::eHS_Unhide;

			if (m_replayEventGuard)
				return;

			if (IsPlayingBack())
			{
				RecSysLogDebug(eRSD_Visibility, "OnEvent %s during playback [%d: %s]%s",
				               event.event == ENTITY_EVENT_HIDE ? "Hide" : event.event == ENTITY_EVENT_UNHIDE ? "Show" : "????",
				               entityId, pEntity->GetName(), bHiding ? "" : " Suppressing");

				// Suppress the showing of entities while we are replaying.
				if (!bHiding)
				{
					HideEntityKeepingPhysics(pEntity, true);
				}
			}
			if (IsRecording())
			{
				SRecording_EntityHide hideEvent;
				hideEvent.entityId = pEntity->GetId();
				hideEvent.hide = bHiding;
				m_pBuffer->AddPacket(hideEvent);

				RecSysLogDebug(eRSD_Visibility, "Recorded %s Event [%d: %s]", bHiding ? "Hide" : "Show", pEntity->GetId(), pEntity->GetName());
			}
		}
		break;
	}
}

void CRecordingSystem::OnCreateEmitter(IParticleEmitter* pEmitter)
{
	// Only record particles which are not being spawned by a killcam replay
	if (m_replayEventGuard == 0 && IsRecording())
	{
		ILevelSystem* pLevelSystem = g_pGame->GetIGameFramework()->GetILevelSystem();
		if (pLevelSystem->IsLevelLoaded())
		{
			IParticleEffect* pEffectNonConst = const_cast<IParticleEffect*>(pEmitter->GetEffect());
			if (!m_excludedParticleEffects.count(pEffectNonConst))
			{
				if (!pEffectNonConst->IsTemporary())
				{
					SRecording_ParticleCreated particle;
					particle.pParticleEmitter = pEmitter;
					particle.pParticleEffect = pEffectNonConst;
					particle.location = pEmitter->GetLocation();
					particle.entityId = 0;   //pEmitter->GetAttachedEntityId(); This won't have been set yet
					particle.entitySlot = 0; //pEmitter->GetAttachedEntitySlot();
					pEmitter->AddRef();
					m_newParticles.push_back(particle);
				}
			}
		}
		else
		{
			// Currently in the middle of loading the level, this emitter is going to be a permanent
			// part of the level.
		}
		RecSysLogDebug(eRSD_Particles, "Emitter Created: %s (%p)", pEmitter->GetEffect()->GetName(), pEmitter);
	}
}

void CRecordingSystem::OnDeleteEmitter(IParticleEmitter* pEmitter)
{
	// Only record particles which are not being deleted by a killcam replay
	if (m_replayEventGuard == 0 && IsRecording())
	{
		// If the particle emitter is added and removed in the same frame then discard both
		// the particle added and removed packets
		bool deleted = false;
		std::vector<SRecording_ParticleCreated>::iterator itParticle;
		for (itParticle = m_newParticles.begin(); itParticle != m_newParticles.end(); )
		{
			if (itParticle->pParticleEmitter == pEmitter)
			{
				itParticle = m_newParticles.erase(itParticle);
				deleted = true;
				pEmitter->Release();
			}
			else
			{
				++itParticle;
			}
		}

		/*TReplayParticleMap::iterator itReplayParticle;
		   for (itReplayParticle = m_replayParticles.begin(); itReplayParticle != m_replayParticles.end(); )
		   {
		   if (itReplayParticle->second == pEmitter)
		   {
		    pEmitter->Release();
		    itReplayParticle = m_replayParticles.erase(itReplayParticle);
		   }
		   else
		   {
		    itReplayParticle++;
		   }
		   }*/

		if (!deleted)
		{
			SRecording_ParticleDeleted particle;
			particle.pParticleEmitter = pEmitter;

			m_pBuffer->AddPacket(particle);
		}

		RecSysLogDebug(eRSD_Particles, "Emitter Deleted: (%p)", pEmitter);
	}
}

void CRecordingSystem::OnParticleEmitterMoved(IParticleEmitter* pEmitter, const Matrix34& loc)
{
	if (m_replayEventGuard == 0 && IsRecording())
	{
		SRecording_ParticleLocation particle;
		particle.pParticleEmitter = pEmitter;
		particle.location = loc;
		m_pBuffer->AddPacket(particle);
	}
}

void CRecordingSystem::OnParticleEmitterTargetSet(IParticleEmitter* pEmitter, const ParticleTarget& target)
{
	if (m_replayEventGuard == 0 && IsRecording())
	{
		SRecording_ParticleTarget particle;
		particle.pParticleEmitter = pEmitter;
		particle.targetPos = target.vTarget;
		particle.velocity = target.vVelocity;
		particle.radius = target.fRadius;
		particle.target = target.bTarget;
		particle.priority = target.bPriority;
		QueueAddPacket(particle);
	}
}

void CRecordingSystem::OnSetTeam(EntityId entityId, int team)
{
	if (m_replayEventGuard == 0 && IsRecording())
	{
		static IEntityClass* s_pPlayerClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Player");
		static IEntityClass* s_pDummyPlayerClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("DummyPlayer");
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId);
		if (pEntity && ((pEntity->GetClass() == s_pPlayerClass) || (pEntity->GetClass() == s_pDummyPlayerClass) || (m_recordEntityClassFilter.count(pEntity->GetClass()))))
		{
			SRecording_TeamChange teamChange;
			teamChange.entityId = entityId;
			teamChange.teamId = (uint8)team;
			CGameRules* pGameRules = g_pGame->GetGameRules();
			teamChange.isFriendly = pGameRules ? pGameRules->GetThreatRating(gEnv->pGameFramework->GetClientActorId(), entityId) == CGameRules::eFriendly : false;
			m_pBuffer->AddPacket(teamChange);
			// Sometimes the models get changed when setting the team, so we'll update
			// the entity spawn packet with the new models
			UpdateEntitySpawnGeometry(pEntity);
		}
	}
}

void CRecordingSystem::OnItemSwitchToHand(CItem* pItem, int hand)
{
	if (m_replayEventGuard == 0 && IsRecording())
	{
		SRecording_ItemSwitchHand switchHand;
		switchHand.entityId = pItem->GetEntityId();
		switchHand.hand = hand;
		m_pBuffer->AddPacket(switchHand);
	}
}

//void CRecordingSystem::OnSoundSystemEvent(ESoundSystemCallbackEvent event, ISound *pSound)
//{
//	if (m_replayEventGuard == 0)
//	{
//		ESoundSemantic semantic = pSound->GetSemantic();
//		// Only record sounds which are not being spawned by a killcam replay
//		if (semantic != eSoundSemantic_Replay)
//		{
//			// Only record certain types of sounds, ignore ambient sounds, hud sound, voices, etc...
//			static const uint32 k_recordedSemantics = eSoundSemantic_Physics_Collision | eSoundSemantic_Physics_Footstep | eSoundSemantic_Physics_General |
//				eSoundSemantic_Vehicle | eSoundSemantic_Weapon | eSoundSemantic_Explosion | eSoundSemantic_Player_Foley |
//				eSoundSemantic_Animation;
//			if (semantic & k_recordedSemantics)
//			{
//				if (event == SOUNDSYSTEM_EVENT_ON_START)
//				{
//					const char* name = pSound->GetName();
//					const uint32 nameHash = CCrc32::Compute(name);
//					if (!m_excludedSoundEffects.count(nameHash))
//					{
//						RecSysLogDebug(eRSD_Sound, "REC: Start: %d, %s - semantic: %d", pSound->GetId(), name, BitIndex((uint32)pSound->GetSemantic()));
//						SRecording_PlaySound playSound;
//						playSound.szName = CacheString(name, eSC_Sound);
//						if (playSound.szName)
//						{
//							playSound.position = pSound->GetPosition();
//							playSound.soundId = pSound->GetId();
//							playSound.flags = pSound->GetFlags();
//							playSound.semantic = pSound->GetSemantic();
//							ISound_Extended* pExtended = pSound->GetInterfaceExtended();
//							if (pExtended)
//								playSound.entityId = pExtended->GetAudioProxyEntityID();
//							m_pBuffer->AddPacket(playSound);
//
//							// Check if the sound has any parameters, and if it does then we'll need track them
//							// and record any changes to them
//							SSoundParamters soundParameters;
//							soundParameters.soundId = pSound->GetId();
//							int numValidParameters = 0;
//							while(true)
//							{
//								const char* paramName;
//								int index = soundParameters.numParameters;
//								if (pSound->GetParam(index, &soundParameters.parameters[index], NULL, NULL, &paramName, false))
//								{
//									RecSysLogDebug(eRSD_Sound, "Found parameter: %s, %f", paramName, soundParameters.parameters[index]);
//									if (strcmp(paramName, "distance") == 0)
//									{
//										// Don't bother recording the distance parameter this is useless
//										soundParameters.ignoredParameters |= 1 << index;
//									}
//									else
//									{
//										numValidParameters++;
//									}
//									soundParameters.numParameters++;
//									if (soundParameters.numParameters >= MAX_SOUND_PARAMETERS)
//									{
//										float val = 0.f;
//										CRY_ASSERT_MESSAGE(!pSound->GetParam(MAX_SOUND_PARAMETERS, &val), "Sound has too many parameters");
//										break;
//									}
//								}
//								else
//								{
//									break;
//								}
//							}
//							if (numValidParameters > 0)
//							{
//								// Only track sound if it has some interesting parameters & the sound is not already in m_trackedSounds list
//								std::vector<SSoundParamters>::iterator itSoundParam;
//								for (itSoundParam = m_trackedSounds.begin(); itSoundParam != m_trackedSounds.end(); ++itSoundParam)
//								{
//									if (itSoundParam->soundId == soundParameters.soundId)
//									{
//										RecSysLogDebug(eRSD_General, "Already tracking sound: %d, %s", soundParameters.soundId, name);
//										break;
//									}
//								}
//								if (itSoundParam == m_trackedSounds.end())
//								{
//									m_trackedSounds.push_back(soundParameters);
//								}
//							}
//						}
//					}
//				}
//				else if (event == SOUNDSYSTEM_EVENT_ON_STOP)
//				{
//					SRecording_StopSound stopSound;
//					RecSysLogDebug(eRSD_Sound, "REC: Stop: %d, %s", pSound->GetId(), pSound->GetName());
//					stopSound.soundId = pSound->GetId();
//					m_pBuffer->AddPacket(stopSound);
//				}
//				else if (event == SOUNDSYSTEM_EVENT_ON_UNLOAD)
//				{
//					if (gEnv->pSoundSystem)
//						gEnv->pSoundSystem->RemoveEventListener(this);
//				}
//			}
//		}
//	}
//}

/*static*/ CRecordingSystem::EKillCamHandling CRecordingSystem::ShouldHandleKillcam(IActor* pVictimActor, const HitInfo& hitInfo, bool bBulletTimeKill, float& outKillCamDelayTime)
{
	outKillCamDelayTime = 0.0f;   //Shouldn't be required if we're going to early out, but doing it for safety

	// KillCam is Disabled.
	if (!CRecordingSystem::KillCamEnabled())
		return CRecordingSystem::eKCH_None;

	// Get EntityIds.
	const EntityId killerId = hitInfo.shooterId;
	const EntityId victimId = pVictimActor->GetEntityId();

	// Not interested in suicides.
	if (killerId == victimId)
		return CRecordingSystem::eKCH_None;

	// Not interested if there is no Killer.
	if (killerId == 0)
		return CRecordingSystem::eKCH_None;

	// Not interested if there is no killer Actor.
	IActor* pKillerActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(killerId);
	if (!pKillerActor)
		return CRecordingSystem::eKCH_None;

	// Not interested if the Victim is not a Player.
	if (!pVictimActor->IsPlayer())
		return CRecordingSystem::eKCH_None;

	//Work out if there is sufficient time for display of the kill cam, or if it will just end up in the highlights reel (custom games can have
	//		non-standard respawn times)
	if (CGameRules* pGameRules = g_pGame->GetGameRules())
	{
		if (IGameRulesSpectatorModule* pSpectatorModule = pGameRules->GetSpectatorModule())
		{
			float killerCam, deathCam, killCam;
			pSpectatorModule->GetPostDeathDisplayDurations(victimId, static_cast<CPlayer*>(pVictimActor)->GetTeamWhenKilled(), false, bBulletTimeKill, deathCam, killerCam, killCam);

			outKillCamDelayTime = max((killerCam + deathCam) - g_pGameCVars->kc_kickInTime, 0.0f);

			if (killCam < 0.1f)
			{
				return CRecordingSystem::eKCH_StoreHighlight;
			}
		}
	}

	return CRecordingSystem::eKCH_StoreHighlightAndSend;
}

void CRecordingSystem::OnKill(IActor* pVictimActor, const HitInfo& hitInfo, bool bWinningKill, bool bBulletTimeKill)
{
#if defined(DEDICATED_SERVER)
	return;
#endif

	float fKillCamDelayTime;
	EKillCamHandling handling = CRecordingSystem::ShouldHandleKillcam(pVictimActor, hitInfo, bBulletTimeKill, fKillCamDelayTime);

	if (handling == eKCH_None)
		return;

	const EntityId killerId = hitInfo.shooterId;
	const EntityId victimId = pVictimActor->GetEntityId();

	// Who is the client?
	const EntityId localPlayerId = gEnv->pGameFramework->GetClientActorId();
	const bool bKillerIsClient = (killerId == localPlayerId);
	const bool bVictimIsClient = (victimId == localPlayerId);

#ifdef RECSYS_DEBUG
	// CVars to set as a winning or skill kill.
	bWinningKill |= (g_pGameCVars->kc_debugWinningKill != 0);
	bBulletTimeKill |= (g_pGameCVars->kc_debugSkillKill != 0);
#endif //RECSYS_DEBUG

	CGameRules* pGameRules = g_pGame->GetGameRules();

	// If not allowed, disable winning kill.
	bool bWinningKillAllowed = (g_pGameCVars->kc_enableWinningKill != 0);
	if (pGameRules)
	{
		if (IGameRulesRoundsModule* pRoundsModule = pGameRules->GetRoundsModule())
		{
			bWinningKillAllowed = pRoundsModule->ShowKillcamAtEndOfRound();
		}
	}
	bWinningKill &= bWinningKillAllowed;

	//////////////////////////////////////////////////////////////////////////
	// What are we actually going to do?
	bool bHasTimeToDisplayNow = (handling == eKCH_StoreHighlightAndSend);
	bool bSaveHighlight = bKillerIsClient;
	bool bSendData = bKillerIsClient && (bHasTimeToDisplayNow || bWinningKill);
	bool bRequestPlayback = bWinningKill || (bVictimIsClient && bHasTimeToDisplayNow);

	SKillInfo kill;
	kill.killerId = killerId;
	kill.victimId = victimId;
	kill.projectileId = hitInfo.projectileId;
	kill.pProjectileClass = GetEntityClass_NetSafe(hitInfo.projectileClassId);
	kill.localPlayerTeam = pGameRules ? pGameRules->GetTeam(localPlayerId) : 0;
	kill.hitType = hitInfo.type;
	kill.impulse = hitInfo.dir;
	kill.deathTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	kill.bulletTimeKill = bBulletTimeKill;
	kill.winningKill = bWinningKill;

	// Request Playback
	if (bRequestPlayback)
	{
		SPlaybackRequest request;
		request.highlights = false;
		request.playbackDelay = fKillCamDelayTime;
		request.kill = kill;
		QueueStartPlayback(request);
	}

	// Send FP Data
	if (bSendData)
	{
		IActor* pKillerActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(killerId);
		AddKillCamData(pKillerActor, victimId, bBulletTimeKill, bWinningKill);
		RecordFPKillHitPos(*pVictimActor, hitInfo);
	}

	// Save as a highlight.
	if (bSaveHighlight)
	{
		SaveHighlight_Queue(kill, g_pGameCVars->kc_length);
	}
}

bool CRecordingSystem::OnEmitTracer(const CTracerManager::STracerParams& params)
{
	if (IsPlayingBack())
	{
		if (m_replayEventGuard == 0)
		{
			return false;
		}
		else
		{
			CAudioSignalPlayer::JustPlay(m_tracerSignal);
		}
	}
	return true;
}

void CRecordingSystem::OnPlayerPluginEvent(CPlayer* pPlayer, EPlayerPlugInEvent pluginEvent, void* pData)
{
	if (pluginEvent == EPE_BulletTrail)
	{
		STrailInfo* pTrailInfo = (STrailInfo*)pData;
		const SAmmoParams& ammoParams = pTrailInfo->pProjectile->GetAmmoParams();
		if ((ammoParams.bulletType != -1) &&
		    ammoParams.physicalizationType == ePT_Particle &&
		    !pTrailInfo->pProjectile->CheckAnyProjectileFlags(CProjectile::ePFlag_threatTrailEmitted))
		{
			SRecording_BulletTrail bulletTrail;
			bulletTrail.start = pTrailInfo->pProjectile->GetInitialPos();
			bulletTrail.end = pTrailInfo->pos;
			CActor* pActor = (CActor*)g_pGame->GetIGameFramework()->GetClientActor();
			bulletTrail.friendly = pActor && pActor->IsFriendlyEntity(pTrailInfo->pProjectile->GetOwnerId());
			m_pBuffer->AddPacket(bulletTrail);
		}
	}
}

void CRecordingSystem::OnMountedGunUpdate(CPlayer* pPlayer, float aimrad, float aimUp, float aimDown)
{
	SRecording_MountedGunAnimation mountedGun;

	mountedGun.ownerId = pPlayer->GetEntityId();
	mountedGun.aimrad = aimrad;
	mountedGun.aimUp = aimUp;
	mountedGun.aimDown = aimDown;

	m_pBuffer->AddPacket(mountedGun);
}

void CRecordingSystem::OnMountedGunRotate(IEntity* pMountedGun, const Quat& rotation)
{
	SRecording_MountedGunRotate mountedGun;

	mountedGun.gunId = pMountedGun->GetId();
	mountedGun.rotation = rotation;

	m_pBuffer->AddPacket(mountedGun);
}

void CRecordingSystem::OnMountedGunEnter(CPlayer* pPlayer, int upAnimId, int downAnimId)
{
	SRecording_MountedGunEnter mountedGun;
	mountedGun.ownerId = pPlayer->GetEntityId();
	mountedGun.upAnimId = upAnimId;
	mountedGun.downAnimId = downAnimId;
	m_pBuffer->AddPacket(mountedGun);
}

void CRecordingSystem::OnMountedGunLeave(CPlayer* pPlayer)
{
	SRecording_MountedGunLeave mountedGun;
	mountedGun.ownerId = pPlayer->GetEntityId();
	m_pBuffer->AddPacket(mountedGun);
}

void CRecordingSystem::OnShoot(IWeapon* pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3& pos, const Vec3& dir, const Vec3& vel)
{
	if (shooterId == g_pGame->GetIGameFramework()->GetClientActorId())
	{
		m_clientShot = true;
	}
	SRecording_OnShoot packet;
	packet.weaponId = static_cast<CWeapon*>(pWeapon)->GetEntityId();
	AddPacket(packet);
}

void CRecordingSystem::OnSelected(IWeapon* pWeapon, bool selected)
{
	RecordWeaponSelection((CWeapon*)pWeapon, selected);
}

void CRecordingSystem::OnFireModeChanged(IWeapon* pWeapon, int currentFireMode)
{
	CWeapon* pWeaponImpl = static_cast<CWeapon*>(pWeapon);

	SRecording_FiremodeChanged firemodeChanged;
	firemodeChanged.weaponId = pWeaponImpl->GetEntityId();
	firemodeChanged.ownerId = pWeaponImpl->GetOwnerId();
	firemodeChanged.firemode = currentFireMode;
	m_pBuffer->AddPacket(firemodeChanged);
}

void CRecordingSystem::OnWeaponRippedOff(CWeapon* pWeapon)
{
	// CE-10971: Killing a player with a ripped-off HMG would trigger an assertion
	// and the gun would be missing in the kill cam. The gun entity wasn't present
	// in the replay buffer.
	IEntity* pEntity = pWeapon->GetEntity();
	AddRecordingEntity(pEntity, RecordEntitySpawn(pEntity));

	RecordWeaponSelection(pWeapon, true);
}

void CRecordingSystem::OnBattleChatter(EBattlechatter chatter, EntityId actorId, int variation)
{
	if (m_replayEventGuard == 0)
	{
		SRecording_BattleChatter battleChatter;
		battleChatter.entityNetId = EntityIdToNetId(actorId);
		battleChatter.frametime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		battleChatter.chatterType = chatter;
		battleChatter.chatterVariation = (uint8)variation + 1;
		m_pBufferFirstPerson->AddPacket(battleChatter);
	}
}

void CRecordingSystem::DebugDrawAnimationData(void)
{
	// TODO: Implement me, this is only used for debugging purposes though...
	CRY_ASSERT_MESSAGE(false, "This function has not been implemented yet");
#if 0
	const float FONT_SIZE = 1.0f;
	const float FONT_COLOUR[4] = { 0.0f, 1.0f, 1.0f, 1.0f };
	const float TEXT_XPOS = 100.0f;
	const float TEXT_YPOS = 400.0f;
	const float TEXT_INC = 16.0f;

	if (m_pBuffer->GetNumPackets() > 0)
	{
		float xPos = TEXT_XPOS;
		float yPos = TEXT_YPOS;

		CRecordingBuffer::packetIterator it = m_pBuffer->GetPacketListEnd();
		it--;
		while (1)
		{
			CRecordingBuffer::packetStruct* ps = *it;
			SRecording_Packet* packet = (SRecording_Packet*) ps->data;

			switch (packet->type)
			{
			case eTPP_TPAnim:
				{
					SRecording_TPAnim* sad = (SRecording_TPAnim*) ps->data;

					IEntity* replayEntity = gEnv->pEntitySystem->GetEntity(sad->eid);
					if (replayEntity)
					{
						ICharacterInstance* ici = replayEntity->GetCharacter(0);
						IAnimationSet* pAnimationSet = ici->GetIAnimationSet();
						const char* animName = pAnimationSet->GetNameByAnimID(sad->animID);

						IRenderAuxText::Draw2dLabel(xPos, yPos, FONT_SIZE, FONT_COLOUR, false, animName);

						yPos += TEXT_INC;
					}
					break;
				}
			default:
				break;
			}

			if (it == m_pBuffer->GetPacketListBegin())
				break;
			it--;
		}
	}
#endif
}

void CRecordingSystem::DebugDrawMemoryUsage()
{
#ifdef RECSYS_DEBUG
	if (g_pGameCVars->kc_memStats == 1)
	{
		static int memory[eTPP_Last];
		for (int i = 0; i < eTPP_Last; ++i)
		{
			memory[i] = 0;
		}
		int totalMemory = 0;
		int numFrames = 0;
		float firstFrameTime = 0;
		float lastFrameTime = 0;
		for (CRecordingBuffer::iterator itPacket = m_pBuffer->begin(); itPacket != m_pBuffer->end(); ++itPacket)
		{
			if (itPacket->type < eTPP_Last)
			{
				memory[itPacket->type] += itPacket->size;
				totalMemory += itPacket->size;
			}
			else
			{
				CRY_ASSERT_MESSAGE(false, "Unrecognised packet type in recording buffer");
			}
			if (itPacket->type == eRBPT_FrameData)
			{
				float frameTime = ((SRecording_FrameData*)&*itPacket)->frametime;
				if (firstFrameTime == 0)
				{
					firstFrameTime = frameTime;
				}
				lastFrameTime = frameTime;
				numFrames++;
			}
		}

		int numFPFrames = 0;
		float firstFPFrameTime = 0;
		float lastFPFrameTime = 0;
		for (CRecordingBuffer::iterator itPacket = m_pBufferFirstPerson->begin(); itPacket != m_pBufferFirstPerson->end(); ++itPacket)
		{
			if (itPacket->type == eFPP_FPChar)
			{
				SRecording_FPChar* pFPChar = (SRecording_FPChar*)&*itPacket;
				if (firstFPFrameTime == 0)
				{
					firstFPFrameTime = pFPChar->frametime;
				}
				lastFPFrameTime = pFPChar->frametime;
				numFPFrames++;
			}
		}

		float red[] = { 1.f, 0.f, 0.f, 1.f };
		float green[] = { 0.f, 1.f, 0.f, 1.f };
		float white[] = { 1.f, 1.f, 1.f, 1.f };

		float yPos = 10.f;
		const float xPos = 70.f;
		const float ySpacing = 18.0f;

		const int divisions = 20;
		char progress[divisions + 1];
		progress[divisions] = 0;

		if (IsRecordingAndNotPlaying())
		{
			IRenderAuxText::Draw2dLabel(70.f, yPos, 2, red, false, "RECORDING");
		}
		else if (IsPlayingBack())
		{
			IRenderAuxText::Draw2dLabel(70.f, yPos, 2, green, false, "PLAYBACK");
		}

		yPos += 20.f;

		char temp[255];
		cry_sprintf(temp, "First person buffer size: %" PRISIZE_T ", Frames: %d, Time: %f", m_pBufferFirstPerson->size(), numFPFrames, lastFPFrameTime - firstFPFrameTime);
		IRenderAuxText::Draw2dLabel(xPos, yPos, 2, red, false, temp);
		yPos += 20.f;
		IRenderAuxText::Draw2dLabel(xPos, yPos, 2, red, false, "Recording buffer size: %" PRISIZE_T ", Frames: %d, Time: %f", m_pBuffer->size(), numFrames, lastFrameTime - firstFrameTime);
		yPos += 30.f;

		for (int i = 0; i < eTPP_Last; ++i)
		{
			float percentage = (float)memory[i] / (float)totalMemory;

			for (int j = 0; j < divisions; ++j)
			{
				progress[j] = (j < divisions * percentage) ? '+' : '_';
			}

			percentage *= 100.0f;

			if (i >= eRBPT_Custom)
			{
				const int packetIndex = i - eRBPT_Custom;
				assert(packetIndex >= 0 && packetIndex < eTPP_Last - eRBPT_Custom);
				const char* typeName = s_thirdPersonPacketList[packetIndex];
				IRenderAuxText::Draw2dLabel(xPos, yPos, 1.5f, white, false, "[%s] %.1f%% Packet type: %s, size: %d", progress, percentage, typeName, memory[i]);
			}
			else
			{
				IRenderAuxText::Draw2dLabel(xPos, yPos, 1.5f, white, false, "[%s] %.1f%% Packet type: %d, size: %d", progress, percentage, i, memory[i]);
			}
			yPos += ySpacing;
		}
	}
	else if (g_pGameCVars->kc_memStats == 2)
	{
		if (IsRecordingAndNotPlaying())
		{
			float startTime = gEnv->pTimer->GetFrameStartTime().GetSeconds() - g_pGameCVars->kc_length;

			size_t usedSize = m_pBuffer->size();
			size_t pos = 0;
			while (pos < usedSize)
			{
				SRecording_Packet* pPacket = (SRecording_Packet*)m_pBuffer->at(pos);
				if (pPacket->type == eRBPT_FrameData)
				{
					SRecording_FrameData* pFramePacket = (SRecording_FrameData*)pPacket;
					if (pFramePacket->frametime >= startTime)
					{
						break;
					}
				}
				pos += pPacket->size;
			}

			size_t usedMemoryInBuffer = usedSize - pos;
			static size_t maxUsedMemory = 0;
			maxUsedMemory = max(maxUsedMemory, usedMemoryInBuffer);
			CryWatch("CRecordingSystem main buffer usage at %u, most used is %u", usedMemoryInBuffer, maxUsedMemory);

			usedSize = m_pBufferFirstPerson->size();
			pos = 0;
			while (pos < usedSize)
			{
				SRecording_Packet* pPacket = (SRecording_Packet*)m_pBufferFirstPerson->at(pos);
				if (pPacket->type == eFPP_FPChar)
				{
					SRecording_FPChar* pFramePacket = (SRecording_FPChar*)pPacket;
					if (pFramePacket->frametime >= startTime)
					{
						break;
					}
				}
				pos += pPacket->size;
			}

			size_t usedMemoryInBuffer2 = usedSize - pos;
			static size_t maxUsedMemory2 = 0;
			maxUsedMemory2 = max(maxUsedMemory2, usedMemoryInBuffer2);
			CryWatch("CRecordingSystem first person buffer usage at %u, most used is %u", usedMemoryInBuffer2, maxUsedMemory2);

			CryWatch("First person data: last seen: %d, max seen: %d", latestFirstPersonDataSize, maxFirstPersonDataSeen);
		}
	}
#endif //RECSYS_DEBUG
}

void CRecordingSystem::SwapFPCameraEndian(uint8* pBuffer, size_t size, bool sending)
{
#if !defined(_RELEASE)
	if (eBigEndian) // Swap endian on PC
	{
		for (uint8* pPos = pBuffer; pPos < pBuffer + size; )
		{
			SRecording_Packet* pPacket = (SRecording_Packet*)pPos;
			uint16 packetSize = 0;
			if (sending)
			{
				// If we are sending the data then the packet size must be accessed before swapping
				packetSize = pPacket->size;
			}
			SwapEndian(pPacket->size, true);
			SwapEndian(pPacket->type, true);
			if (!sending)
			{
				// If we are receiving the data then the packet size must be accessed after swapping
				packetSize = pPacket->size;
			}
			CRY_ASSERT_MESSAGE(packetSize <= 255, "Packet size is larger than expected, has something gone wrong with endian swapping?");
			CRY_ASSERT_MESSAGE(packetSize != 0, "Packet size is zero, the buffer is corrupt?");
			switch (pPacket->type)
			{
			case eFPP_FPChar:
				{
					SRecording_FPChar* pFPCharPacket = (SRecording_FPChar*)pPacket;
					SwapEndian(pFPCharPacket->camlocation, true);
					SwapEndian(pFPCharPacket->relativePosition, true);
					SwapEndian(pFPCharPacket->fov, true);
					SwapEndian(pFPCharPacket->frametime, true);
					SwapEndian(pFPCharPacket->playerFlags, true);
					break;
				}
			case eFPP_Flashed:
				{
					SRecording_Flashed* pFlashedPacket = (SRecording_Flashed*)pPacket;
					SwapEndian(pFlashedPacket->frametime, true);
					SwapEndian(pFlashedPacket->duration, true);
					SwapEndian(pFlashedPacket->blindAmount, true);
					break;
				}
			case eFPP_RenderNearest:
				{
					SRecording_RenderNearest* pRenderPacket = (SRecording_RenderNearest*)pPacket;
					SwapEndian(pRenderPacket->frametime, true);
					SwapEndian(pRenderPacket->renderNearest, true);
					break;
				}
			case eFPP_VictimPosition:
				{
					SRecording_VictimPosition* pVictimPosPacket = (SRecording_VictimPosition*)pPacket;
					SwapEndian(pVictimPosPacket->frametime, true);
					SwapEndian(pVictimPosPacket->victimPosition, true);
					break;
				}
			case eFPP_BattleChatter:
				{
					SRecording_BattleChatter* pBattleChatter = (SRecording_BattleChatter*)pPacket;
					SwapEndian(pBattleChatter->frametime, true);
					SwapEndian(pBattleChatter->entityNetId, true);
					SwapEndian(pBattleChatter->chatterType, true);
					SwapEndian(pBattleChatter->chatterVariation, true);
					break;
				}
			case eFPP_KillHitPosition:
				{
					SRecording_KillHitPosition* pHitPos = (SRecording_KillHitPosition*)pPacket;
					SwapEndian(pHitPos->hitRelativePos, true);
					SwapEndian(pHitPos->victimId, true);
					break;
				}
			case eFPP_PlayerHealthEffect:
				{
					SRecording_PlayerHealthEffect* pPlayerHealth = (SRecording_PlayerHealthEffect*)pPacket;
					SwapEndian(pPlayerHealth->hitDirection, true);
					SwapEndian(pPlayerHealth->frametime, true);
					SwapEndian(pPlayerHealth->hitStrength, true);
					SwapEndian(pPlayerHealth->hitSpeed, true);
					break;
				}
			case eFPP_PlaybackTimeOffset:
				{
					SRecording_PlaybackTimeOffset* pTimeOffset = (SRecording_PlaybackTimeOffset*)pPacket;
					SwapEndian(pTimeOffset->timeOffset, true);
					break;
				}
			default:
				CRY_ASSERT_MESSAGE(false, "Unhandled packet type");
			}
			pPos += packetSize;
		}
	}
#endif
}

void CRecordingSystem::OnPlayerFlashed(float duration, float blindAmount)
{
	if (IsRecordingAndNotPlaying())
	{
		SRecording_Flashed packet;
		packet.frametime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		packet.duration = duration;
		packet.blindAmount = blindAmount;
		m_pBufferFirstPerson->AddPacket(packet);
	}
}

void CRecordingSystem::ApplyFlashed(const SRecording_Flashed* pFlashed)
{
	gEnv->p3DEngine->SetPostEffectParam("Flashbang_Time", pFlashed->duration * 2);
	gEnv->p3DEngine->SetPostEffectParam("FlashBang_BlindAmount", pFlashed->blindAmount);
	gEnv->p3DEngine->SetPostEffectParam("Flashbang_DifractionAmount", pFlashed->duration);
	gEnv->p3DEngine->SetPostEffectParam("Flashbang_Active", 1.0f);
}

void CRecordingSystem::OnPlayerRenderNearestChange(bool renderNearest)
{
	if (IsRecordingAndNotPlaying())
	{
		SRecording_RenderNearest packet;
		packet.frametime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		packet.renderNearest = renderNearest;
		m_pBufferFirstPerson->AddPacket(packet);
	}
}

void CRecordingSystem::ApplyRenderNearestChange(const SRecording_RenderNearest* pRenderNearest)
{
	CReplayActor* pKiller = GetReplayActor(m_killer, true);
	IEntity* pKillerEntity = pKiller ? pKiller->GetEntity() : NULL;

	if (pKillerEntity)
	{
		uint32 flags = pKillerEntity->GetSlotFlags(0);

		if (pRenderNearest->renderNearest)
		{
			flags |= ENTITY_SLOT_RENDER_NEAREST;
		}
		else
		{
			flags &= ~ENTITY_SLOT_RENDER_NEAREST;
		}
		pKillerEntity->SetSlotFlags(0, flags);
	}
}

void CRecordingSystem::StopFirstPersonWeaponSounds()
{
	/*if (m_killerReplayGunId != 0)
	   {
	   TReplaySoundMap::iterator itReplaySound;
	   for (itReplaySound = m_replaySounds.begin(); itReplaySound != m_replaySounds.end(); ++itReplaySound)
	   {
	    ISound* pSound = gEnv->pSoundSystem->GetSound(itReplaySound->second);
	    if (pSound)
	    {
	      ISound_Extended* pExtended = pSound->GetInterfaceExtended();
	      if (pExtended)
	      {
	        EntityId entityId = pExtended->GetAudioProxyEntityID();
	        if (entityId == m_killerReplayGunId)
	        {
	          pSound->Stop();
	        }
	      }
	    }
	   }
	   }*/
}

void CRecordingSystem::ApplyPlaySound(const SRecording_PlaySound* pPlaySound, float time)
{
	//TReplaySoundMap::iterator itReplaySound = m_replaySounds.find(pPlaySound->soundId);
	//if (itReplaySound == m_replaySounds.end())
	//{
	//	stack_string adjustedSoundName = pPlaySound->szName;
	//	IEntity* pEntity = NULL;

	//	if (pPlaySound->entityId)
	//	{
	//		bool entityIsActor = false;

	//		if(!(pEntity = GetReplayEntity(pPlaySound->entityId)))
	//		{
	//			if(CReplayActor* pActor = GetReplayActor(pPlaySound->entityId, true))
	//			{
	//				pEntity = pActor->GetEntity();
	//				entityIsActor = true;
	//			}
	//		}

	//		bool isFirstPersonSound = (pPlaySound->entityId == m_killer);
	//		if (isFirstPersonSound && (m_playInfo.m_view.currView == SPlaybackInfo::eVM_FirstPerson || m_playInfo.m_view.currView == SPlaybackInfo::eVM_BulletTime))
	//		{
	//			// Should be playing first person sounds here
	//			adjustedSoundName.replace("_3p", "_fp");
	//		}
	//		else // Not FP view, or not FP sound.
	//		{
	//			if(pEntity)
	//			{
	//				if (entityIsActor)
	//				{
	//					// Only replace if this is a weapon sound
	//					adjustedSoundName.replace("_fp", "_3p");
	//				}
	//			}
	//		}
	//	}
	//	ISound* pSound = gEnv->pSoundSystem->CreateSound(adjustedSoundName.c_str(), pPlaySound->flags);
	//	if (pSound)
	//	{
	//		pSound->SetPosition(pPlaySound->position);
	//		pSound->SetMatrix(Matrix34::CreateTranslationMat(pPlaySound->position));
	//		pSound->SetSemantic(eSoundSemantic_Replay);
	//		if (pEntity)
	//		{
	//			IEntityAudioComponent* pIEntityAudioComponent = pEntity->GetOrCreateComponent<IEntityAudioComponent>();
	//			CRY_ASSERT_MESSAGE(pIEntityAudioComponent.get(), "Failed to create sound proxy");
	//			if(pIEntityAudioComponent)
	//			{
	//				pIEntityAudioComponent->PlaySound(pSound);
	//			}
	//		}
	//		else
	//		{
	//			pSound->Play();
	//		}
	//		RecSysLogDebug(eRSD_Sound, "Play: ID[%d] NAME[%s] ADJ[%s]", pSound->GetId(), pPlaySound->szName, adjustedSoundName.c_str());
	//		m_replaySounds[pPlaySound->soundId] = pSound->GetId();
	//	}
	//}
	//else
	//{
	//	ISound* pSound = gEnv->pSoundSystem->GetSound(itReplaySound->second);
	//	if (pSound)
	//	{
	//		pSound->Play();
	//		RecSysLogDebug(eRSD_Sound, "Re-playing existing sound effect: %d, %s", pSound->GetId(), pPlaySound->szName);
	//	}
	//}
}

void CRecordingSystem::ApplyStopSound(const SRecording_StopSound* pStopSound)
{
	/*TReplaySoundMap::iterator itReplaySound = m_replaySounds.find(pStopSound->soundId);
	   if (itReplaySound != m_replaySounds.end())
	   {
	   ISound* pSound = gEnv->pSoundSystem->GetSound(itReplaySound->second);
	   if (pSound)
	   {
	    RecSysLogDebug(eRSD_Sound, "Stop: %d, %s", pSound->GetId(), pSound->GetName());
	    pSound->Stop();
	   }
	   m_replaySounds.erase(itReplaySound);
	   }*/
}

void CRecordingSystem::ApplySoundParameter(const SRecording_SoundParameter* pSoundParameter)
{
	/*TReplaySoundMap::iterator itReplaySound = m_replaySounds.find(pSoundParameter->soundId);
	   if (itReplaySound != m_replaySounds.end())
	   {
	   ISound* pSound = gEnv->pSoundSystem->GetSound(itReplaySound->second);
	   if (pSound)
	   {
	    RecSysLogDebug(eRSD_Sound, "SetParam: %d, %s, param: %d, %f", pSound->GetId(), pSound->GetName(), pSoundParameter->index, pSoundParameter->value);
	    bool success = pSound->SetParam(pSoundParameter->index, pSoundParameter->value);
	    if (!success)
	    {
	      RecSysLogDebug(eRSD_Sound, "Failed to set replay sound parameter: %d, %s, param: %d, %f", pSound->GetId(), pSound->GetName(), pSoundParameter->index, pSoundParameter->value);
	    }
	   }
	   }*/
}

void CRecordingSystem::ApplyBattleChatter(const SRecording_BattleChatter* pPacket)
{
	CBattlechatter* pBattleChatter = g_pGame->GetGameRules()->GetBattlechatter();
	if (pBattleChatter)
	{
		// Get the voice using the original actor id (so as not to polute the battlechatter map with lots of temporary ids)
		EntityId actorId = NetIdToEntityId(pPacket->entityNetId);
		if (actorId)
		{
			CBattlechatter::SVoiceInfo* pInfo = pBattleChatter->GetVoiceInfo(actorId);
			if (pInfo)
			{
				TReplayActorMap::iterator itReplayActor = m_replayActors.find(actorId);
				if (itReplayActor != m_replayActors.end())
				{
					const float currentTime = gEnv->pTimer->GetCurrTime();
					CActor* pActor = static_cast<CActor*>(gEnv->pGameFramework->GetIActorSystem()->GetActor(actorId));

					int killerTeam = -1;
					CReplayActor* pKillerReplayActor = GetReplayActor(m_killer, true);
					if (pKillerReplayActor)
					{
						killerTeam = pKillerReplayActor->GetTeam();
					}

					REINST("needs verification!");
					//pBattleChatter->Play(pInfo, (EBattlechatter)pPacket->chatterType, pActor, itReplayActor->second, currentTime, eSoundSemantic_Replay, ((int)pPacket->chatterVariation)-1, killerTeam);
				}
			}
		}
	}
}

void CRecordingSystem::ClearStringCaches()
{
	// It is only safe to clear this cache immediatly after m_pBuffer has been reset
	*(uint16*)m_soundNames = 0;
	*(uint16*)m_modelNames = 0;
}

const char* CRecordingSystem::CacheString(const char* name, eStringCache cache)
{
	char* pBuffer = NULL;
	int bufferSize = 0;
	if (cache == eSC_Sound)
	{
		pBuffer = m_soundNames;
		bufferSize = SOUND_NAMES_SIZE;
	}
	else if (cache == eSC_Model)
	{
		pBuffer = m_modelNames;
		bufferSize = MODEL_NAMES_SIZE;
	}

	// Store the name in a buffer so that it will still be valid when we want to play it back again
	// alternatively if the name is already in the buffer then return a pointer to that.
	// Ideally the sound system would allow you to create a sound by ID rather than by name, but that
	// is currently not possible.
	if (pBuffer != NULL)
	{
		const uint16 newLength = static_cast<uint16>(strlen(name));
		int offset = 0;
		while (true)
		{
			uint16 stringLength = *(uint16*)(pBuffer + offset);
			if (stringLength > 0)
			{
				offset += 2;
				if (newLength == stringLength && strcmp(pBuffer + offset, name) == 0)
				{
					return pBuffer + offset;
				}
				offset += stringLength + 1;
			}
			else
			{
				break;
			}
		}
		uint16 stringLength = static_cast<uint16>(strlen(name));
		// +1 for NULL terminator, +2 for string size, +2 for string size of next string
		if (offset + stringLength + 5 <= bufferSize)
		{
			*(uint16*)(pBuffer + offset) = stringLength;    // Set the size of the new string
			offset += 2;
			memcpy(pBuffer + offset, name, stringLength + 1);    // Copy the new string into the buffer
			*(uint16*)(pBuffer + offset + stringLength + 1) = 0; // Assign zero size to the next string
			return pBuffer + offset;                             // Return a pointer to the cached string
		}
		else
		{
			RecSysLog("CacheString() ran out of memory in the string cache trying to save '%s'", name);
			CRY_ASSERT_MESSAGE(false, "Ran out of memory in the string cache");
			return NULL;
		}
	}
	return NULL;
}

void CRecordingSystem::ApplyBulletTrail(const SRecording_BulletTrail* pBulletTrail)
{
}

void CRecordingSystem::RecordSoundParameters()
{
	//const float tolerance = 0.01f;
	//std::vector<SSoundParamters>::iterator itSound;
	//for (itSound = m_trackedSounds.begin(); itSound != m_trackedSounds.end(); )
	//{
	//	ISound* pSound = gEnv->pSoundSystem->GetSound(itSound->soundId);
	//	if (pSound)
	//	{
	//		for (int i=0; i<itSound->numParameters; i++)
	//		{
	//			if (!(itSound->ignoredParameters & (1 << i)))
	//			{
	//				float value;
	//				const char* paramName;
	//				if (pSound->GetParam(i, &value, NULL, NULL, &paramName))
	//				{
	//					float diff = fabs(value - itSound->parameters[i]);
	//					if (diff > tolerance)
	//					{
	//						// Parameter has changed
	//						RecSysLogDebug(eRSD_Sound, "REC: ParamChanged! Sound: %s, Param: %s, Value: %f ", pSound->GetName(), paramName, value);
	//						SRecording_SoundParameter soundParam;
	//						soundParam.soundId = itSound->soundId;
	//						soundParam.index = i;
	//						soundParam.value = value;
	//						m_pBuffer->AddPacket(soundParam);
	//						itSound->parameters[i] = value;
	//					}
	//				}
	//				else
	//				{
	//					RecSysLog("Failed to record sound parameter: %d, %s, param index: %d", pSound->GetId(), pSound->GetName(), i);
	//				}
	//			}
	//		}
	//		++itSound;
	//	}
	//	else
	//	{
	//		itSound = m_trackedSounds.erase(itSound);
	//	}
	//}
}

CReplayActor* CRecordingSystem::GetReplayActor(EntityId entityId, bool originalEntity)
{
	if (originalEntity)
	{
		TReplayEntityMap::iterator itReplayActor = m_replayActors.find(entityId);
		if (itReplayActor != m_replayActors.end())
		{
			entityId = itReplayActor->second;
		}
		else
		{
			return NULL;
		}
	}
	return (CReplayActor*)g_pGame->GetIGameFramework()->QueryGameObjectExtension(entityId, "ReplayActor");
}

IEntity* CRecordingSystem::GetReplayEntity(EntityId originalEntityId)
{
	TReplayEntityMap::iterator itReplayEntity = m_replayEntities.find(originalEntityId);
	if (itReplayEntity != m_replayEntities.end())
	{
		return gEnv->pEntitySystem->GetEntity(itReplayEntity->second);
	}
	return NULL;
}

CItem* CRecordingSystem::GetReplayItem(EntityId originalEntityId)
{
	TReplayEntityMap::iterator itReplayEntity = m_replayEntities.find(originalEntityId);
	if (itReplayEntity != m_replayEntities.end())
	{
		return (CItem*)g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(itReplayEntity->second);
	}
	return NULL;
}

void CRecordingSystem::ApplyMountedGunAnimation(const SRecording_MountedGunAnimation* pMountedGunAnimation)
{
	CReplayActor* pReplayActor = GetReplayActor(pMountedGunAnimation->ownerId, true);
	if (pReplayActor)
	{
		ICharacterInstance* pCharacterInstance = pReplayActor->GetEntity()->GetCharacter(0);
		if (pCharacterInstance)
		{
			CMountedGunController::ReplayUpdateThirdPersonAnimations(pCharacterInstance, pMountedGunAnimation->aimrad, pMountedGunAnimation->aimUp, pMountedGunAnimation->aimDown, pMountedGunAnimation->ownerId == m_killer && m_playInfo.m_view.currView == SPlaybackInfo::eVM_FirstPerson);
		}
	}
}

void CRecordingSystem::ApplyMountedGunRotate(const SRecording_MountedGunRotate* pMountedGunRotate)
{
	if (IEntity* pReplayEntity = GetReplayEntity(pMountedGunRotate->gunId))
	{
		pReplayEntity->SetRotation(pMountedGunRotate->rotation);
	}
}

void CRecordingSystem::ApplyMountedGunEnter(const SRecording_MountedGunEnter* pMountedGunEnter)
{
	CReplayActor* pReplayActor = GetReplayActor(pMountedGunEnter->ownerId, true);
	if (pReplayActor)
	{
		ICharacterInstance* pCharacterInstance = pReplayActor->GetEntity()->GetCharacter(0);
		if (pCharacterInstance)
		{
			CMountedGunController::ReplayStartThirdPersonAnimations(pCharacterInstance, pMountedGunEnter->upAnimId, pMountedGunEnter->downAnimId);
		}
	}
}

void CRecordingSystem::ApplyMountedGunLeave(const SRecording_MountedGunLeave* pMountedGunLeave)
{
	if (CReplayActor* pReplayActor = GetReplayActor(pMountedGunLeave->ownerId, true))
	{
		if (ICharacterInstance* pCharacterInstance = pReplayActor->GetEntity()->GetCharacter(0))
		{
			CMountedGunController::ReplayStopThirdPersonAnimations(pCharacterInstance);
		}
	}
}

void CRecordingSystem::ApplyCorpseSpawned(const SRecording_CorpseSpawned* pCorpsePacket)
{
	if (CReplayActor* pPlayerActor = GetReplayActor(pCorpsePacket->playerId, true))
	{
		IEntity* pPlayerEntity = pPlayerActor->GetEntity();
		if (IEntity* pCorpseEntity = SpawnCorpse(pCorpsePacket->corpseId, pPlayerEntity->GetWorldPos(), pPlayerEntity->GetWorldRotation(), pPlayerEntity->GetScale()))
		{
			pPlayerActor->TransitionToCorpse(*pCorpseEntity);
		}
	}
}

void CRecordingSystem::ApplySingleProceduralBreakEvent(const SRecording_ProceduralBreakHappened* pProceduralBreak)
{
	gEnv->pGameFramework->ApplySingleProceduralBreakFromEventIndex(pProceduralBreak->uBreakEventIndex, m_renderNodeLookup);
}

void CRecordingSystem::ApplyDrawSlotChange(const SRecording_DrawSlotChange* pDrawSlotChangePacket)
{
	BreakLogAlways("DS: ApplyDrawSlot: Original EntityID: 0x%08X", pDrawSlotChangePacket->entityId);

	IEntity* pEntity = GetReplayEntity(pDrawSlotChangePacket->entityId);

	//pEntity should always be valid, but we've been having some crashes here. Need to find out under what
	//	circumstances it can break.
	assert(pEntity);
	if (pEntity)
	{
		BreakLogAlways("DS: > Found Replay Entity: 0x%p   ID: 0x%p, applying flags (%d) to slot %d",
		               pEntity, pEntity->GetId(), pDrawSlotChangePacket->flags, pDrawSlotChangePacket->iSlot);
		pEntity->SetSlotFlags(pDrawSlotChangePacket->iSlot, pDrawSlotChangePacket->flags);
	}
	else
	{
		BreakLogAlways("DS: > FAILED TO FIND ENTITY!");
	}
}

void CRecordingSystem::ApplyStatObjChange(const SRecording_StatObjChange* pStatObjChange)
{
	BreakLogAlways("Applying Stat Obj Change during killcam playback");

	if (IEntity* pClonedEntity = GetReplayEntity(pStatObjChange->entityId))
	{
		pClonedEntity->SetStatObj(pStatObjChange->pNewStatObj, pStatObjChange->iSlot, false);

		BreakLogAlways("TB: > Replay, found ReplayEntity : 0x%p, clone of original %d", pClonedEntity, pStatObjChange->entityId);
	}
	else
	{
		BreakLogAlways("TB: > FAILED TO FIND CLONE OF ORIGINAL ENTITY PTR: %d", pStatObjChange->entityId);
	}
}

void CRecordingSystem::ApplySubObjHideMask(const SRecording_SubObjHideMask* pHideMask)
{
	if (IEntity* pEntity = GetReplayEntity(pHideMask->entityId))
	{
		pEntity->SetSubObjHideMask(pHideMask->slot, pHideMask->subObjHideMask);
	}
}

void CRecordingSystem::ApplyTeamChange(const SRecording_TeamChange* pTeamChange)
{
}

void CRecordingSystem::ApplyItemSwitchHand(const SRecording_ItemSwitchHand* pSwitchHand)
{
	if (CItem* pItem = GetReplayItem(pSwitchHand->entityId))
	{
		pItem->SwitchToHand(pSwitchHand->hand);
	}
}

bool CRecordingSystem::ApplyEntityAttached(const SRecording_EntityAttached* pEntityAttached)
{
	IEntity* pEntity = GetReplayEntity(pEntityAttached->entityId);
	CReplayActor* pActor = GetReplayActor(pEntityAttached->actorId, true);
	if (pEntity && pActor)
	{
		static const uint32 crcAttachmentName = CCrc32::ComputeLowercase("left_weapon");
		const bool bFirstPerson = (pEntityAttached->actorId == m_killer && m_playInfo.m_view.currView == SPlaybackInfo::eVM_FirstPerson);
		AttachmentUtils::AttachObject(bFirstPerson, pActor->GetEntity(), pEntity, crcAttachmentName, AttachmentUtils::eAF_Default & (~AttachmentUtils::eAF_SyncCloak));
	}
	return false;
}

void CRecordingSystem::ApplyEntityDetached(const SRecording_EntityDetached* pEntityDetached)
{
	if (CReplayActor* pActor = GetReplayActor(pEntityDetached->actorId, true))
	{
		static const uint32 crcAttachmentName = CCrc32::ComputeLowercase("left_weapon");
		AttachmentUtils::DetachObject(pActor->GetEntity(), GetReplayEntity(pEntityDetached->entityId), crcAttachmentName);
	}
}

void CRecordingSystem::ApplyPlayerHealthEffect(const SRecording_PlayerHealthEffect* pHitEffect)
{
	CPlayer* pClientPlayer = (CPlayer*)g_pGame->GetIGameFramework()->GetClientActor();
	if (pClientPlayer)
	{
		pClientPlayer->GetPlayerHealthGameEffect().OnHit(pHitEffect->hitDirection, pHitEffect->hitStrength, pHitEffect->hitSpeed);
	}
}

void CRecordingSystem::ApplyAnimObjectUpdated(const SRecording_AnimObjectUpdated* pAnimObjectUpdated)
{
	IEntity* pEntity = GetReplayEntity(pAnimObjectUpdated->entityId);
	if (pEntity)
	{
		ICharacterInstance* pCharacter = pEntity->GetCharacter(0);
		if (pCharacter)
		{
			ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim();
			if (pSkeletonAnim)
			{
				CAnimation& anim = pSkeletonAnim->GetAnimFromFIFO(0, 0);
				if ((pSkeletonAnim->GetNumAnimsInFIFO(0) == 0) || (anim.GetAnimationId() != pAnimObjectUpdated->animId))
				{
					pSkeletonAnim->ClearFIFOLayer(0);
					CryCharAnimationParams params;
					pSkeletonAnim->StartAnimationById(pAnimObjectUpdated->animId, params);
				}
				pSkeletonAnim->SetLayerNormalizedTime(0, pAnimObjectUpdated->fTime);
			}
		}
	}
}

void CRecordingSystem::ApplyObjectCloakSync(const SRecording_ObjectCloakSync* pObjectCloakSync)
{
	// Grab replay entitities (we expect both to be valid)
	CReplayActor* pCloakMasterActor = GetReplayActor(pObjectCloakSync->cloakPlayerId, true);
	IEntity* pCloakSlave = GetReplayEntity(pObjectCloakSync->cloakObjectId);
	if (pCloakSlave && pCloakMasterActor)
	{
		IEntity* pCloakMaster = pCloakMasterActor->GetEntity();

		// Grab render proxies
		IEntityRender* pCloakSlaveRP = (pCloakSlave->GetRenderInterface());
		if (!pCloakSlaveRP)
		{
			GameWarning("CRecordingSystem::ApplyObjectCloakSync() - cloak Master/Slave Render proxy ID invalid, aborting cloak sync");
			return;
		}

		bool bSlaveIsCloaked = false;    //(pCloakSlaveRP->GetMaterialLayersMask()&MTL_LAYER_CLOAK) != 0;

		if (bSlaveIsCloaked == pObjectCloakSync->cloakSlave)
		{
			return; // Done
		}
		else
		{
			IEntityRender* pCloakMasterRP = (pCloakMaster->GetRenderInterface());
			//const float cloakBlendSpeedScale		= pCloakMasterRP->GetCloakBlendTimeScale();
			//const bool  bFadeByDistance				= pCloakMasterRP->DoesCloakFadeByDistance();
			//const uint8 colorChannel			    = pCloakMasterRP->GetCloakColorChannel();
			//const bool  bIgnoreCloakRefractionColor = pCloakMasterRP->DoesIgnoreCloakRefractionColor();
			//EntityEffects::Cloak::CloakEntity(pCloakSlave->GetId(), pObjectCloakSync->cloakSlave, pObjectCloakSync->fadeToDesiredCloakTarget, cloakBlendSpeedScale, bFadeByDistance, colorChannel, bIgnoreCloakRefractionColor);
		}
	}
}

void CRecordingSystem::ApplyPickAndThrowUsed(const SRecording_PickAndThrowUsed* pPickAndThrowUsed)
{
	IEntity* pEntity = GetReplayEntity(pPickAndThrowUsed->pickAndThrowId);
	IEntity* pObject = GetReplayEntity(pPickAndThrowUsed->environmentEntityId);
	if (pEntity != NULL && pObject != NULL)
	{
		CPickAndThrowWeapon* pPickAndThrow = static_cast<CPickAndThrowWeapon*>(g_pGame->GetIGameFramework()->GetIItemSystem()->GetItem(pEntity->GetId()));
		CRY_ASSERT(pPickAndThrow);
		if (pPickAndThrow)
		{
			if (pPickAndThrowUsed->bPickedUp)
			{
				pPickAndThrow->SetObjectId(pObject->GetId());
				pPickAndThrow->DecideGrabType();
				pPickAndThrow->QuickAttach();

				if (pPickAndThrowUsed->playerId == m_killer)
				{
					SHUDEventWrapper::ForceCrosshairType(pPickAndThrow, (ECrosshairTypes)pPickAndThrow->GetGrabTypeParams().crossHairType);
				}
			}
			else
			{
				pPickAndThrow->Start_Drop(true);
			}

			if (pPickAndThrowUsed->playerId == m_killer)
			{
				SHUDEvent weaponSwitch(eHUDEvent_OnKillCamWeaponSwitch);
				weaponSwitch.AddData((int)pEntity->GetId());
				weaponSwitch.AddData(false);
				CHUDEventDispatcher::CallEvent(weaponSwitch);
			}
		}
	}
}

void CRecordingSystem::ApplyPlayerChangedModel(const SRecording_PlayerChangedModel* pData)
{
}

void CRecordingSystem::ApplyForcedRagdollAndImpulse(const SRecording_ForcedRagdollAndImpulse* pImportantImpulse)
{
	if (CReplayActor* pActor = GetReplayActor(pImportantImpulse->playerEntityId, true))
	{
		pActor->Ragdollize();

		pe_action_impulse impulse;
		impulse.impulse = pImportantImpulse->vecImpulse;
		impulse.point = pImportantImpulse->pos;
		impulse.partid = pImportantImpulse->partid;
		impulse.iApplyTime = pImportantImpulse->iApplyTime;
		pActor->ApplyRagdollImpulse(impulse);
	}
}

void CRecordingSystem::ApplyImpulseToRagdoll(const SRecording_RagdollImpulse* pRagdollImpulse)
{
	if (CReplayActor* pActor = GetReplayActor(pRagdollImpulse->playerId, true))
	{
		pe_action_impulse impulse;
		impulse.impulse = pRagdollImpulse->impulse;
		impulse.point = pRagdollImpulse->point;
		impulse.partid = pRagdollImpulse->partid;
		pActor->ApplyRagdollImpulse(impulse);
	}
}

uint16 CRecordingSystem::EntityIdToNetId(EntityId entityId)
{
	INetContext* pNetContext = gEnv->pGameFramework->GetNetContext();
	if (pNetContext)
	{
		SNetObjectID netId = pNetContext->GetNetID(entityId, false);
		return netId.id;
	}
	else
	{
		return 0;
	}
}

/*static*/ EntityId CRecordingSystem::NetIdToEntityId(uint16 netId)
{
	INetContext* pNetContext = gEnv->pGameFramework->GetNetContext();
	if (pNetContext)
	{
		SNetObjectID netObjectId(netId, 0);
		pNetContext->Resaltify(netObjectId);
		return pNetContext->GetEntityID(netObjectId);
	}
	else
	{
		return 0;
	}
}

void CRecordingSystem::OnPickupEntityAttached(EntityId entityId, EntityId actorId)
{
	if (m_replayEventGuard == 0 && IsRecording())
	{
		SRecording_EntityAttached entityAttached;
		entityAttached.entityId = entityId;
		entityAttached.actorId = actorId;
		m_pBuffer->AddPacket(entityAttached);
	}
}

void CRecordingSystem::OnPickupEntityDetached(EntityId entityId, EntityId actorId, bool isOnRemove)
{
	if (m_replayEventGuard == 0 && IsRecording())
	{
		SRecording_EntityDetached entityDetached;
		entityDetached.entityId = entityId;
		entityDetached.actorId = actorId;
		m_pBuffer->AddPacket(entityDetached);
	}
}

void CRecordingSystem::OnPlayerHealthEffect(const CPlayerHealthGameEffect::SQueuedHit& hitParams)
{
	if (m_replayEventGuard == 0 && IsRecording() && hitParams.m_hitStrength > 0.1f)
	{
		SRecording_PlayerHealthEffect hitEffect;
		hitEffect.frametime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
		hitEffect.hitDirection = hitParams.m_hitDirection;
		hitEffect.hitStrength = clamp_tpl<float>(hitParams.m_hitStrength, 0.f, 500.f);
		hitEffect.hitSpeed = hitParams.m_hitSpeed;
		m_pBufferFirstPerson->AddPacket(hitEffect);
	}
}

void CRecordingSystem::OnPlayerJoined(EntityId playerId)
{
	if (m_replayEventGuard == 0 && IsRecording())
	{
		SRecording_PlayerJoined joinedPacket;
		joinedPacket.playerId = playerId;

		// Get the squad status relative to the client actor.
		CPlayer* pPlayer = static_cast<CPlayer*>(g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId));
		CGameLobby* pLobby = g_pGame->GetGameLobby();
		CSquadManager* pSM = g_pGame->GetSquadManager();
		if (pLobby && pPlayer && pSM)
		{
			// Uses the lobby & userIds as the player actor isn't fully created yet. Note then that some params don't work in LAN mode - i.e. isLocalPlayer
			uint16 channelId = pPlayer->GetChannelId();
			CryUserID userId = pLobby->GetUserIDFromChannelID(channelId);
			joinedPacket.bSameSquad = pSM->IsSquadMateByUserId(userId);

			CryFixedStringT<DISPLAY_NAME_LENGTH> name;
			pLobby->GetPlayerDisplayNameFromEntity(playerId, name);

			cry_strcpy(joinedPacket.displayName, name.c_str());

			uint8 rank = 0, reincarnations = 0;
			pLobby->GetProgressionInfoByChannel(channelId, rank, reincarnations);

			joinedPacket.rank = rank;
			joinedPacket.reincarnations = reincarnations;

			joinedPacket.bIsClient = pLobby->IsLocalChannelUser(channelId);
			joinedPacket.bIsPlayer = pPlayer->IsPlayer();
		}

		if (pPlayer)
		{
			IAnimatedCharacter* ac = pPlayer->GetAnimatedCharacter();
			if (ac && ac->GetActionController())
			{

				IActionController* pActionController = ac->GetActionController();
				CReplayMannListener* pListener = new CReplayMannListener(playerId, *this);
				pActionController->RegisterListener(pListener);
				RegisterReplayMannListener(*pListener);

				IEntity* pEntity = pPlayer->GetEntity();
				SmartScriptTable pScriptTable = pEntity ? pEntity->GetScriptTable() : NULL;

				const char* szAnimDatabase = NULL;
				pScriptTable->GetValue("AnimDatabase1P", szAnimDatabase);
				joinedPacket.pAnimDB1P = szAnimDatabase;
				szAnimDatabase = NULL;
				pScriptTable->GetValue("AnimDatabase3P", szAnimDatabase);
				joinedPacket.pAnimDB3P = szAnimDatabase;
				szAnimDatabase = NULL;
				pScriptTable->GetValue("ActionController", szAnimDatabase);
				joinedPacket.pControllerDef = szAnimDatabase;
			}
		}

		m_pBuffer->AddPacket(joinedPacket);
	}
}

void CRecordingSystem::OnPlayerLeft(EntityId playerId)
{
	if (m_replayEventGuard == 0 && IsRecording())
	{
		SRecording_PlayerLeft leftPacket;
		leftPacket.playerId = playerId;
		m_pBuffer->AddPacket(leftPacket);

		UnregisterReplayMannListener(playerId);
	}
}

void CRecordingSystem::OnPlayerChangedModel(CPlayer* pPlayer)
{
	if (m_replayEventGuard == 0 && IsRecording())
	{
		EntityId playerId = pPlayer->GetEntityId();

		SRecording_PlayerChangedModel changedModelPacket;
		changedModelPacket.playerId = playerId;

		ICharacterInstance* pCharInstance = pPlayer->GetEntity()->GetCharacter(0);
		if (pCharInstance)
		{
			changedModelPacket.pModelName = CacheString(pCharInstance->GetFilePath(), eSC_Model);
			changedModelPacket.pShadowName = CacheString(pPlayer->GetShadowFileModel(), eSC_Model);

			m_pBuffer->AddPacket(changedModelPacket);
		}
	}
}

void CRecordingSystem::OnCorpseSpawned(EntityId corpseId, EntityId playerId)
{
	if (m_replayEventGuard == 0 && IsRecording())
	{
		SRecording_CorpseSpawned corpsePacket;
		corpsePacket.corpseId = corpseId;
		corpsePacket.playerId = playerId;
		m_pBuffer->AddPacket(corpsePacket);
	}
}

void CRecordingSystem::OnCorpseRemoved(EntityId corpseId)
{
	if (m_replayEventGuard == 0 && IsRecording())
	{
		SRecording_CorpseRemoved corpsePacket;
		corpsePacket.corpseId = corpseId;
		m_pBuffer->AddPacket(corpsePacket);
	}
}

void CRecordingSystem::OnObjectCloakSync(const EntityId objectId, const EntityId ownerId, const bool cloakSlave, const bool fade)
{
	if ((m_replayEventGuard == 0) && (IsRecording()))
	{
		SRecording_ObjectCloakSync objCloakPacket;
		objCloakPacket.cloakPlayerId = ownerId;
		objCloakPacket.cloakObjectId = objectId;
		objCloakPacket.fadeToDesiredCloakTarget = fade;
		objCloakPacket.cloakSlave = cloakSlave;
		m_pBuffer->AddPacket(objCloakPacket);
	}
}

void CRecordingSystem::OnMannequinSetSlaveController(EntityId inMasterActorId, EntityId inSlaveActorId, uint32 inTargetContext, bool inEnslave, const IAnimationDatabase* piOptionalDatabase)
{
	if ((m_replayEventGuard == 0) && (IsRecording()))
	{
		SRecording_MannSetSlaveController mannSetSlaveControllerPacket;
		mannSetSlaveControllerPacket.masterActorId = inMasterActorId;
		mannSetSlaveControllerPacket.slaveActorId = inSlaveActorId;
		CRY_ASSERT_MESSAGE(inTargetContext < 256, "targetContext is stored in a u8 in killcam, yet the targetContext being set is out of range!");
		mannSetSlaveControllerPacket.targetContext = static_cast<uint8>(inTargetContext);
		mannSetSlaveControllerPacket.enslave = inEnslave;
		if (piOptionalDatabase)
		{
			const char* pDBName = piOptionalDatabase->GetFilename();
			mannSetSlaveControllerPacket.optionalDatabaseFilenameCRC = CCrc32::ComputeLowercase(pDBName);
		}
		m_pBuffer->AddPacket(mannSetSlaveControllerPacket);
	}
}

void CRecordingSystem::OnMannequinSetParam(EntityId inEntityId, const char* inParamName, QuatT& inQuatParam)
{
	if ((m_replayEventGuard == 0) && (IsRecording()))
	{
		SRecording_MannSetParam mannSetParam;
		mannSetParam.entityId = inEntityId;
		mannSetParam.paramNameCRC = CCrc32::ComputeLowercase(inParamName);
		mannSetParam.quat = inQuatParam;
		m_pBuffer->AddPacket(mannSetParam);
	}
}

void CRecordingSystem::OnMannequinSetParamFloat(EntityId inEntityId, uint32 inParamCRC, float inFloatParam)
{
	if ((m_replayEventGuard == 0) && (IsRecording()))
	{
		SRecording_MannSetParamFloat mannSetParamFloat;
		mannSetParamFloat.entityId = inEntityId;
		mannSetParamFloat.paramNameCRC = inParamCRC;
		mannSetParamFloat.param = inFloatParam;
		m_pBuffer->AddPacket(mannSetParamFloat);
	}
}

void CRecordingSystem::OnMannequinRecordHistoryItem(const SMannHistoryItem& historyItem, const IActionController& actionController, const EntityId entityId)
{
	SRecording_MannEvent manEvent;
	manEvent.eid = entityId;
	manEvent.historyItem = historyItem;

	// Always set scope
	if (historyItem.fragment >= 0)
	{
		const SAnimationContext& animContext = actionController.GetContext();
		SFragTagState fragTagState(animContext.state.GetMask(), historyItem.tagState);
		manEvent.historyItem.scopeMask = animContext.controllerDef.GetScopeMask(historyItem.fragment, fragTagState);
	}

	AddPacket(manEvent);
}

void CRecordingSystem::OnPickAndThrowUsed(EntityId playerId, EntityId pickAndThrowId, EntityId enviromentEntityId, bool bPickedUp)
{
	if ((m_replayEventGuard == 0) && (IsRecording()))
	{
		SRecording_PickAndThrowUsed pickAndThrowPacket;
		pickAndThrowPacket.playerId = playerId;
		pickAndThrowPacket.pickAndThrowId = pickAndThrowId;
		pickAndThrowPacket.environmentEntityId = enviromentEntityId;
		pickAndThrowPacket.bPickedUp = bPickedUp;
		m_pBuffer->AddPacket(pickAndThrowPacket);
	}
}

void CRecordingSystem::OnForcedRagdollAndImpulse(EntityId playerId, Vec3 vecImpulse, Vec3 pos, int partid, int applyTime)
{
	if ((m_replayEventGuard == 0) && (IsRecording()))
	{
		SRecording_ForcedRagdollAndImpulse impulsePacket;
		impulsePacket.playerEntityId = playerId;
		impulsePacket.vecImpulse = vecImpulse;
		impulsePacket.pos = pos;
		impulsePacket.partid = partid;
		impulsePacket.iApplyTime = applyTime;

		m_pBuffer->AddPacket(impulsePacket);
	}
}

void CRecordingSystem::OnApplyImpulseToRagdoll(const EntityId playerId, pe_action_impulse& impulse)
{
	if ((m_replayEventGuard == 0) && (IsRecording()))
	{
		SRecording_RagdollImpulse impulsePacket;
		impulsePacket.playerId = playerId;
		impulsePacket.impulse = impulse.impulse;
		impulsePacket.point = impulse.point;
		impulsePacket.partid = impulse.partid;
		m_pBuffer->AddPacket(impulsePacket);
	}
}

void CRecordingSystem::CameraCollisionAdjustment(const IEntity* target, Vec3& cameraPos) const
{
	if (!target || !g_pGameCVars->kc_cameraCollision)
		return;

	Vec3 targetWorldPos = target->GetWorldPos();
	targetWorldPos.z += 1.5f;

	const float wallSafeDistance = 0.2f; // how far to keep camera from walls

	const Vec3 dir = cameraPos - targetWorldPos;

	primitives::sphere sphere;
	sphere.center = targetWorldPos;
	sphere.r = wallSafeDistance;

	geom_contact* pContact = 0;
	const float hitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, dir, ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid | ent_independent,
	                                                                       &pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable);

	// even when we have contact, keep the camera the same height above the target
	const float minHeightDiff = dir.z;

	if (hitDist > 0 && pContact)
	{
		const Vec3 normalizedDir = dir.GetNormalizedSafe();
		cameraPos = targetWorldPos + (hitDist * normalizedDir);

		if (cameraPos.z - targetWorldPos.z < minHeightDiff)
		{
			// can't move the camera far enough away from the player in this direction. Try moving it directly up a bit
			sphere.center = cameraPos;

			// (move back just slightly to avoid colliding with the wall we've already found...)
			sphere.center -= normalizedDir * 0.05f;

			const float newHitDist = gEnv->pPhysicalWorld->PrimitiveWorldIntersection(sphere.type, &sphere, Vec3(0, 0, minHeightDiff), ent_static | ent_terrain | ent_rigid | ent_sleeping_rigid | ent_independent,
			                                                                          &pContact, 0, (geom_colltype_player << rwi_colltype_bit) | rwi_stop_at_pierceable);

			float raiseDist = minHeightDiff - (cameraPos.z - targetWorldPos.z) - wallSafeDistance;
			if (newHitDist != 0)
			{
				raiseDist = std::min(minHeightDiff, newHitDist);
			}

			raiseDist = std::max(0.0f, raiseDist);

			cameraPos.z += raiseDist;
		}
	}
}

void CRecordingSystem::ReRecord(const EntityId entityId)
{
	if (IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId))
	{
		OnRemove(entity);
		SRecordingEntity::EEntityType type = RecordEntitySpawn(entity);
		AddRecordingEntity(entity, type);
	}
}

void CRecordingSystem::ProcessReplayEntityInfo(float& updateTimer, Vec3& lastPosition, bool onGround, const Vec3& forwardDirection, const Vec3& currentPosition, float individualUpdateRate, float individualMinMovementSqrd)
{
	if (onGround && lastPosition.GetSquaredDistance(currentPosition) > individualMinMovementSqrd)
	{
		updateTimer += individualUpdateRate;
		lastPosition = currentPosition;

		RequestTrackerRaycast(currentPosition, forwardDirection);
	}
}

void CRecordingSystem::RequestTrackerRaycast(const Vec3& position, const Vec3& forwardDirection)
{
	const Vec3 startPos = position;
	const Vec3 dir = Vec3(0.f, 0.f, -1.0f);

	const int objTypes = ent_all & ~ent_living;
	const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | rwi_force_pierceable_noncoll | rwi_ignore_solid_back_faces | rwi_pierceability(15);

	const uint32 rayID = g_pGame->GetRayCaster().Queue(
	  RayCastRequest::HighPriority,
	  RayCastRequest(startPos, dir,
	                 objTypes,
	                 flags),
	  functor(*this, &CRecordingSystem::OnTrackerRayCastDataReceived));

	m_PendingTrackerRays[rayID] = forwardDirection;
}

void CRecordingSystem::OnTrackerRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result)
{
}

void CRecordingSystem::OnInteractiveObjectFinishedUse(EntityId objectId, TagID interactionTypeTag, uint8 interactionIndex)
{
	if ((m_replayEventGuard == 0) && (IsRecording()))
	{
		SRecording_InteractiveObjectFinishedUse interactiveObjectFinishedUsePacket;
		interactiveObjectFinishedUsePacket.objectId = objectId;
		interactiveObjectFinishedUsePacket.interactionTypeTag = interactionTypeTag;
		interactiveObjectFinishedUsePacket.interactionIndex = interactionIndex;
		m_pBuffer->AddPacket(interactiveObjectFinishedUsePacket);
	}
}

void CRecordingSystem::CleanUpDeferredRays()
{
	TPendingRaycastMap::iterator pendingRaycast = m_PendingTrackerRays.begin();
	for (; pendingRaycast != m_PendingTrackerRays.end(); ++pendingRaycast)
	{
		g_pGame->GetRayCaster().Cancel(pendingRaycast->first);
	}

	m_PendingTrackerRays.clear();
}

void CRecordingSystem::GetVictimAimTarget(Vec3& aimTarget, const Vec3& localPosition, const Vec3& remotePosition, float lerpValue)
{
	// This function attempts to modify the victim's aim direction to reduce the visual effect of lag
	// Steps:
	//	1: Find the angle between the victim's aim direction and the direction to the target as they were locally
	//	2: Get the positions of the victim and killer as the killcam will show them (killcam uses remote data)
	//	3: Adjust aim direction to achieve the same angle

	// First get the position that the killer had locally
	const uint8* startingplace = m_pTPDataBuffer;
	const uint8* endingplace = startingplace + m_tpDataBufferSize;

	// Extract out the relevant packets so that they can be processed
	const SRecording_TPChar* killerPackets[2];
	int index = 0;

	while ((startingplace < endingplace) && (index < 2))
	{
		const SRecording_Packet* packet = (const SRecording_Packet*) startingplace;

		switch (packet->type)
		{
		case eTPP_TPChar:
			{
				const SRecording_TPChar* tpPacket = (const SRecording_TPChar*)packet;
				if (tpPacket->eid == m_killer)
				{
					killerPackets[index] = tpPacket;
					++index;
				}
			}
			break;
		}

		startingplace = startingplace + packet->size;
	}

	if (index != 2)
	{
		// Failed to find enough data about the kill position - bail
		return;
	}

	Vec3 killerLocalPosition;
	killerLocalPosition.SetLerp(killerPackets[0]->entitylocation.t, killerPackets[1]->entitylocation.t, lerpValue);

	// Now determine the angle that we had to that position
	Vec3 aimDirection = aimTarget - localPosition;

	Vec3 directionToKiller = killerLocalPosition - localPosition;
	aimDirection.Normalize();

	Vec2 aimDirection2d(aimDirection.x, aimDirection.y);
	aimDirection2d.Normalize();

	Vec2 directionToKiller2d(directionToKiller.x, directionToKiller.y);
	directionToKiller2d.Normalize();

	float dotProduct = directionToKiller2d.Dot(aimDirection2d);
	dotProduct = clamp_tpl(dotProduct, -1.f, 1.f);
	float angle = acosf(dotProduct);

	if (fabs(angle) > g_pGameCVars->kc_aimAdjustmentMinAngle)
	{
		return;
	}

	// Determine whether the angle should be flipped or not
	Vec2 tangent(aimDirection2d.y, -aimDirection2d.x);
	float temp = tangent.Dot(directionToKiller2d);
	if (temp < 0.f)
	{
		angle = -angle;
	}

	// Get remote direction to the killer
	Vec3 killerPos = m_firstPersonCharacter.camlocation.t + m_firstPersonCharacter.relativePosition.t;
	Vec3 playbackDirectionToKiller;
	Vec2 playbackDirectionToKiller2d;

	playbackDirectionToKiller = killerPos - remotePosition;

	playbackDirectionToKiller2d = Vec2(playbackDirectionToKiller.x, playbackDirectionToKiller.y);
	playbackDirectionToKiller2d.Normalize();

	// Adjust aim target so it has the same angle to the remote direction as the original did to the local direction
	Vec3 dir(playbackDirectionToKiller2d.x, playbackDirectionToKiller2d.y, aimDirection.z);
	aimDirection = Quat::CreateRotationZ(angle) * dir;

	// Switch aim direction back into aim target (target is ~5m ahead)
	aimTarget = aimDirection * 5.f;
	aimTarget += remotePosition;
}

void CRecordingSystem::SaveHighlight(const SRecordedKill& details, const int indexOverride /*= -1*/)
{
	SStackGuard guard(s_savingHighlightsGuard);

	float earliestTime = 0.f;
	const float fun = AnalysePotentialHighlight(details, earliestTime);

	RecSysLogDebug(eRSD_General, "Highlight Fun Factors: New[%.2f] Curr[%.2f][%.2f][%.2f][%.2f]"
	               , fun
	               , m_highlightData[0].m_fun
	               , m_highlightData[1].m_fun
	               , m_highlightData[2].m_fun
	               , m_highlightData[3].m_fun);

	const float currTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();

	if (earliestTime > details.m_kill.deathTime)
	{
		RecSysLog("Not enough data in the buffers to record this far back. Trying to record %.2fseconds.", currTime - earliestTime);
		return;
	}

	if (earliestTime > details.m_startTime)
	{
		// Adjusting the start time. We can still fit the kill in though!
		RecSysLogDebug(eRSD_General, "Reducing highlight length to %.2fseconds, because there is not enough data in the buffers.", currTime - earliestTime);
	}
	const float startTime = max(earliestTime, details.m_startTime);

	int highlightIndex = indexOverride;
	if (highlightIndex == -1)
	{
		float leastFunHighlight = FLT_MAX;
		for (int i = 0; i < MAX_HIGHLIGHTS; i++)
		{
			if (!m_highlightData[i].m_bUsed)
			{
				highlightIndex = i;
				leastFunHighlight = -1.f;
				break;
			}
			else
			{
				if (m_highlightData[i].m_fun < leastFunHighlight)
				{
					leastFunHighlight = m_highlightData[i].m_fun;
					highlightIndex = i;
				}
			}
		}

		if (fun < leastFunHighlight)
		{
			RecSysLog("Not Recording highlight because you were too boring!");
			return;
		}

		if (highlightIndex < 0 || highlightIndex >= MAX_HIGHLIGHTS)
			highlightIndex = 0;
	}

	SHighlightData& data = m_highlightData[highlightIndex];

	if (data.m_bUsed)
	{
		CleanupHighlight(highlightIndex);
	}

	data.m_fun = fun;
	data.m_endTime = currTime;

	while (m_pBuffer->size() > 0)
	{
		SRecording_Packet* pPacket = (SRecording_Packet*)m_pBuffer->at(0);
		if (pPacket->type == eRBPT_FrameData)
		{
			SRecording_FrameData* pFramePacket = (SRecording_FrameData*)pPacket;
			if (pFramePacket->frametime >= startTime)
			{
				break;
			}
		}
		else
		{
			CRY_ASSERT_MESSAGE(false, "The first packet in the buffer should always be of type eRBPT_FrameData");
		}
		m_pBuffer->RemoveFrame();
	}

	data.m_data.m_tpdatasize = m_pBuffer->GetData(data.m_data.m_tpdatabuffer, RECORDING_BUFFER_SIZE);

	uint8* dataptr;
	size_t s = GetFirstPersonData(&dataptr, details.m_kill.victimId, details.m_kill.deathTime, details.m_kill.bulletTimeKill, data.m_endTime - startTime);
	if (s)
	{
		memcpy(data.m_data.m_FirstPersonDataContainer.m_firstPersonData, dataptr, s);
		data.m_data.m_FirstPersonDataContainer.m_firstPersonDataSize = s;
		data.m_data.m_FirstPersonDataContainer.m_isDecompressed = false;
	}

	m_recordedData.CopyInitialStateTo(data.m_data);

	// Update player's initial state rank information
	UpdateRecordedData(&data.m_data);

	// Sort out reference counting
	uint8* startingplace = data.m_data.m_tpdatabuffer;
	uint8* endingplace = startingplace + data.m_data.m_tpdatasize;

	while (startingplace < endingplace)
	{
		SRecording_Packet* pPacket = (SRecording_Packet*) startingplace;

		switch (pPacket->type)
		{
		case eTPP_StatObjChange:
			{
				SRecording_StatObjChange* pStatObjPacket = (SRecording_StatObjChange*) pPacket;
				pStatObjPacket->pNewStatObj->AddRef();
				break;
			}
		case eTPP_EntitySpawn:
			{
				SRecording_EntitySpawn* pEntitySpawnPacket = (SRecording_EntitySpawn*) pPacket;
				AddReferences(*pEntitySpawnPacket);
				break;
			}
		}

		startingplace += pPacket->size;
	}

	const TEntitySpawnMap::iterator endSpawns = data.m_data.m_discardedEntitySpawns.end();
	for (TEntitySpawnMap::iterator itSpawns = data.m_data.m_discardedEntitySpawns.begin(); itSpawns != endSpawns; ++itSpawns)
	{
		AddReferences(itSpawns->second.first);
	}

	data.m_details = details;
	data.m_details.m_startTime = startTime;
	data.m_bUsed = true;
}

void CRecordingSystem::PlayHighlight(int index)
{
	if (g_pGame->GetGameRules() && m_highlightData[index].m_bUsed)
	{
		if (IsPlayingOrQueued())
		{
			StopPlayback();
		}
		else
		{
			m_highlightIndex = index;
			m_pHighlightData = &m_highlightData[index];
			m_pPlaybackData = &m_highlightData[index].m_data;
			SetFirstPersonData(m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonData, m_pPlaybackData->m_FirstPersonDataContainer.m_firstPersonDataSize, m_PlaybackInstanceData, m_pPlaybackData->m_FirstPersonDataContainer);
			SPlaybackRequest request;
			request.playbackDelay = 0.f;
			request.highlights = true;
			request.kill = m_pHighlightData->m_details.m_kill;
			QueueStartPlayback(request);
		}
	}
}

void CRecordingSystem::CleanupHighlight(int index)
{
	RecSysLogDebug(eRSD_General, "Cleanup Highlight: %d", index);
	if (m_highlightData[index].m_bUsed)
	{
		// Sort out reference counting
		uint8* startingplace = m_highlightData[index].m_data.m_tpdatabuffer;
		uint8* endingplace = startingplace + m_highlightData[index].m_data.m_tpdatasize;

		while (startingplace < endingplace)
		{
			SRecording_Packet* pPacket = (SRecording_Packet*) startingplace;

			switch (pPacket->type)
			{
			case eTPP_StatObjChange:
				{
					SRecording_StatObjChange* pStatObjPacket = (SRecording_StatObjChange*) pPacket;
					pStatObjPacket->pNewStatObj->Release();
					break;
				}
			case eTPP_EntitySpawn:
				{
					SRecording_EntitySpawn* pEntitySpawnPacket = (SRecording_EntitySpawn*) pPacket;
					ReleaseReferences(*pEntitySpawnPacket);
					break;
				}
			}

			startingplace += pPacket->size;
		}

		const TEntitySpawnMap::iterator endSpawns = m_highlightData[index].m_data.m_discardedEntitySpawns.end();
		for (TEntitySpawnMap::iterator itSpawns = m_highlightData[index].m_data.m_discardedEntitySpawns.begin(); itSpawns != endSpawns; ++itSpawns)
		{
			ReleaseReferences(itSpawns->second.first);
		}

		m_highlightData[index].m_bUsed = false;
	}
}

void CRecordingSystem::SaveHighlight_Queue(const SKillInfo& kill, float length)
{
	// Don't record kills if we are in the pre-match state etc.
	if (gEnv->bMultiplayer)
	{
		if (CGameRules* pGameRules = g_pGame->GetGameRules())
		{
			if (!pGameRules->HasGameActuallyStarted())
			{
				return;
			}
		}
	}

	float const currTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();

	if (m_recordKillQueue.m_timeTilRecord > 0.f)
	{
		static const float kMaxHighlightDuration = 8.f;
		const float newDuration = (currTime + g_pGameCVars->kc_kickInTime) - m_recordKillQueue.m_startTime;
		if (newDuration > kMaxHighlightDuration)
		{
			RecSysLog("Not extending queued highlight, too long.");
		}
		else
		{
			m_recordKillQueue.m_timeTilRecord = g_pGameCVars->kc_kickInTime;
			m_recordKillQueue.m_killCount++;
			RecSysLog("Extending queued highlight. New length: %.2f seconds. %d kills.", newDuration, m_recordKillQueue.m_killCount);
		}
	}
	else
	{
		RecSysLog("Queuing Highlight Save");
		m_recordKillQueue.m_kill = kill;
		m_recordKillQueue.m_kill.winningKill = false;
		m_recordKillQueue.m_killCount = 1;
		m_recordKillQueue.m_startTime = currTime + g_pGameCVars->kc_kickInTime - length;
		m_recordKillQueue.m_timeTilRecord = g_pGameCVars->kc_kickInTime;
	}
}

bool CRecordingSystem::PlayAllHighlights(bool bLoop)
{
	RecSysLogFunc;
	int bestIndex = -1;
	float earliestTime = FLT_MAX;
	for (int i = 0; i < MAX_HIGHLIGHTS; ++i)
	{
		if (m_highlightData[i].m_bUsed && (m_highlightData[i].m_endTime < earliestTime))
		{
			earliestTime = m_highlightData[i].m_endTime;
			bestIndex = i;
		}
	}
	if (bestIndex != -1)
	{
		m_bShowAllHighlights = true;
		m_bLoopHighlightsReel = bLoop;
		PlayHighlight(bestIndex);

		return true;
	}

	return false;
}

float CRecordingSystem::GetHighlightsReelLength() const
{
	float totalTime = 0.f;
	for (int i = 0; i < MAX_HIGHLIGHTS; ++i)
	{
		if (m_highlightData[i].m_bUsed)
		{
			totalTime += m_highlightData[i].m_endTime - m_highlightData[i].m_details.m_startTime;
		}
	}
	return totalTime;
}

//-----------------------------------------------------------------------------
IEntity* CRecordingSystem::SpawnCorpse(EntityId originalId, const Vec3& position, const Quat& rotation, const Vec3& scale)
{
	SEntitySpawnParams params;
	params.pClass = m_pCorpse;
	char sName[64];
	cry_sprintf(sName, "ReplayCorpse%d", m_nReplayEntityNumber++);
	params.sName = sName;
	params.nFlags = (ENTITY_FLAG_NO_PROXIMITY | ENTITY_FLAG_CLIENT_ONLY);
	params.vPosition = position;
	params.qRotation = rotation;
	params.vScale = scale;
	if (IEntity* pCorpseEntity = gEnv->pEntitySystem->SpawnEntity(params))
	{
		m_replayEntities[originalId] = pCorpseEntity->GetId();
		return pCorpseEntity;
	}
	return NULL;
}

IEntityClass* CRecordingSystem::GetEntityClass_NetSafe(const uint16 classId)
{
	char className[128];
	g_pGame->GetIGameFramework()->GetNetworkSafeClassName(className, sizeof(className), classId);
	return gEnv->pEntitySystem->GetClassRegistry()->FindClass(className);
}

#ifdef RECSYS_DEBUG
void CRecordingSystem::PrintCurrentRecordingBuffers(const char* const msg /*= NULL*/)
{
	if (g_pGameCVars->kc_debugPacketData && m_pDebug)
	{
		m_pDebug->PrintFirstPersonPacketData(*m_pBufferFirstPerson, msg ? msg : "LiveFPRecordingBuffer");
		m_pDebug->PrintThirdPersonPacketData(*m_pBuffer, msg ? msg : "Live3PRecordingBuffer");
	}
}
#endif //RECSYS_DEBUG

void CRecordingSystem::NotifyOnPlaybackRequested(const SPlaybackRequest& info)
{
	TRecordingSystemListeners::const_iterator end = m_listeners.end();
	for (TRecordingSystemListeners::iterator it = m_listeners.begin(); it != end; ++it)
	{
		(*it)->OnPlaybackRequested(info);
	}
}

void CRecordingSystem::NotifyOnPlaybackStarted(const SPlaybackInfo& info)
{
	TRecordingSystemListeners::const_iterator end = m_listeners.end();
	for (TRecordingSystemListeners::iterator it = m_listeners.begin(); it != end; ++it)
	{
		(*it)->OnPlaybackStarted(info);
	}
}

void CRecordingSystem::NotifyOnPlaybackEnd(const SPlaybackInfo& info)
{
	TRecordingSystemListeners::const_iterator end = m_listeners.end();
	for (TRecordingSystemListeners::iterator it = m_listeners.begin(); it != end; ++it)
	{
		(*it)->OnPlaybackEnd(info);
	}
}

/*static*/ void CRecordingSystem::RemoveEntityPhysics(IEntity& entity)
{
	SEntityPhysicalizeParams params;
	params.type = PE_NONE;
	params.nSlot = 0;
	entity.Physicalize(params);

	// Set ViewDistRatio.
	entity.SetViewDistRatio(g_pGameCVars->g_actorViewDistRatio);
}

/*static*/ EntityId CRecordingSystem::FindWeaponAtTime(const SRecordedData& initialData, CRecordingBuffer& tpData, const EntityId playerId, const float atTime)
{
	if (!playerId)
		return 0;

	EntityId weaponId = 0;

	// Get initial state weapon.
	if (const SPlayerInitialState* pInitialState = initialData.GetPlayerInitialState(playerId))
	{
		weaponId = pInitialState->weapon.weaponId;
	}

	// Check any subsequent weapon changes.
	float frameTime = 0.f;
	for (CRecordingBuffer::iterator it = tpData.begin(), end = tpData.end(); it != end; ++it)
	{
		const uint8 packetType = it->type;
		if (packetType == eRBPT_FrameData)
		{
			frameTime = ((SRecording_FrameData*)&*it)->frametime;
			if (frameTime > atTime)
			{
				break;
			}
		}
		else if (packetType == eTPP_WeaponSelect)
		{
			const SRecording_WeaponSelect& packet = (const SRecording_WeaponSelect&)*it;
			if (packet.ownerId == playerId)
			{
				weaponId = packet.weaponId;
			}
		}
	}

	return weaponId;
}

float CRecordingSystem::AnalysePotentialHighlight(const SRecordedKill& details, float& rEarliestTime)
{
	const SHighlightRules& rules = m_highlightsRules;
	CRecordingBuffer& fpData = *m_pBufferFirstPerson;
	CRecordingBuffer& tpData = *m_pBuffer;

	const float fEndTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	const float fStartTime = details.m_startTime;
	const float fKillTime = details.m_kill.deathTime;

	const EntityId killerId = details.m_kill.killerId;
	const EntityId victimId = details.m_kill.victimId;

	SRecording_FPChar* pPrevFPCharPacket = NULL;
	int crouchedChangedCount = 0;
	bool bCrouched = false;
	float fNanoVisionTime = 0.f;
	float fStealthTime = 0.f;
	float fArmourTime = 0.f;
	float fMaxSuitTime = 0.f;
	float fTeabaggableTime = 0.f;
	IEntityClass* pWeaponClass = NULL;

	//////////////////////////////////////////////////////////////////////////
	// DISCARDED DATA
	for (int i = 0; i < MAX_RECORDED_PLAYERS; i++)
	{
		const SPlayerInitialState& initialState = m_recordedData.m_playerInitialStates[i];
		if (initialState.playerId == killerId)
		{
			if (IEntity* pWeapon = gEnv->pEntitySystem->GetEntity(initialState.weapon.weaponId))
			{
				pWeaponClass = pWeapon->GetClass();
			}
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// FP DATA
	float earliestTimeFP = 0.f;
	float dt = 0.f;
	for (CRecordingBuffer::iterator it = fpData.begin(), end = fpData.end(); it != end; ++it)
	{
		switch (it->type)
		{
		case eFPP_FPChar:
			{
				SRecording_FPChar* pPacket = (SRecording_FPChar*)&(*it);
				if (pPrevFPCharPacket)
				{
					dt = pPacket->frametime - pPrevFPCharPacket->frametime;
					if (pPacket->playerFlags & eFPF_NightVision)
						fNanoVisionTime += dt;
					if (pPacket->frametime >= fKillTime && ((bCrouched && pPacket->relativePosition.t.z < -1.45f) || (!bCrouched && pPacket->relativePosition.t.z > -1.15f)))
					{
						bCrouched = !bCrouched;
						crouchedChangedCount++;
					}
				}
				else
				{
					earliestTimeFP = pPacket->frametime;
				}
				pPrevFPCharPacket = pPacket;
			}
			break;
		case eFPP_PlayerHealthEffect:
			break;
		default:
			break;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// TP DATA
	float earliestTimeTP = FLT_MAX;
	float frameTime = 0.f;
	float prevFrameTime = 0.f;
	bool bVictimCorpsed = false;
	Vec3 killerPos(ZERO);
	Vec3 victimPos(ZERO);
	static const float kTeabagDistSqr2D = sqr(2.f);
	for (CRecordingBuffer::iterator it = tpData.begin(), end = tpData.end(); it != end; ++it)
	{
		if (it->type == eRBPT_FrameData)
		{
			frameTime = ((SRecording_FrameData*)&*it)->frametime;
			earliestTimeTP = min(earliestTimeTP, frameTime);
		}
		else if (frameTime > fStartTime && dt > 0.f)
		{
			if (it->type == eTPP_TPChar)
			{
				const SRecording_TPChar& packet = (const SRecording_TPChar&)*it;
				if (packet.eid == killerId)
				{
					dt = frameTime - prevFrameTime;
					killerPos = packet.entitylocation.t;

					// Check Flags.
					if (packet.playerFlags & eTPF_Cloaked)
						fStealthTime += dt;
					if (packet.playerFlags & eFPF_NightVision)
						fNanoVisionTime += dt;
					if ((packet.layerEffectParams & (MTL_LAYER_BLEND_FROZEN | MTL_LAYER_BLEND_WET)) == (MTL_LAYER_BLEND_FROZEN | MTL_LAYER_BLEND_WET))
						fMaxSuitTime += dt;
					else if ((packet.layerEffectParams & MTL_LAYER_BLEND_WET) == MTL_LAYER_BLEND_WET)
						fArmourTime += dt;
					if (frameTime > fKillTime && killerPos.GetSquaredDistance2D(victimPos) < kTeabagDistSqr2D)
						fTeabaggableTime += dt;

					prevFrameTime = frameTime;
				}
				else if (packet.eid == victimId && !bVictimCorpsed)
				{
					victimPos = packet.entitylocation.t;
				}
			}
			else if (it->type == eTPP_WeaponSelect)
			{
				const SRecording_WeaponSelect& packet = (const SRecording_WeaponSelect&)*it;
				if (packet.ownerId == killerId && frameTime < fKillTime)
				{
					if (IEntity* pWeapon = gEnv->pEntitySystem->GetEntity(packet.weaponId))
					{
						pWeaponClass = pWeapon->GetClass();
					}
				}
			}
			else if (it->type == eTPP_CorpseSpawned)
			{
				const SRecording_CorpseSpawned& packet = (const SRecording_CorpseSpawned&)*it;
				bVictimCorpsed |= (packet.playerId == victimId);
			}
		}
	}

	float fun = 1.0f;

	// Hit Type:
	rules.m_hitTypes.Apply(details.m_kill.hitType, fun);

	// Kills:
	rules.m_multiKills.Apply(details.m_killCount, fun);

	// Weapon:
	if (pWeaponClass)
		rules.m_weapons.Apply(pWeaponClass, fun);

	// Special Cases:
	const float fHighlightDuration = fEndTime - fStartTime;

	// eHRM_Stealthed,
	static const float kStealthTimeThreshold = 0.375f;
	if (fStealthTime > kStealthTimeThreshold * fHighlightDuration)
		rules.m_specialCases.Apply(eHRM_Stealthed, fun);

	// eHRM_Armour,
	static const float kArmourTimeThreshold = 0.375f;
	if (fArmourTime > kArmourTimeThreshold * fHighlightDuration)
		rules.m_specialCases.Apply(eHRM_Armour, fun);

	// eHRM_MaxSuit,
	static const float kMaxSuitTimeThreshold = 0.375f;
	if (fMaxSuitTime > kMaxSuitTimeThreshold * fHighlightDuration)
		rules.m_specialCases.Apply(eHRM_MaxSuit, fun);

	// eHRM_NanoVision,
	static const float kNanoVisionTimeThreshold = 0.4f;
	if (fStealthTime > kNanoVisionTimeThreshold * fHighlightDuration)
		rules.m_specialCases.Apply(eHRM_NanoVision, fun);

	// eHRM_Teabag,
	static const float kTeabagTimeThreshold = 1.f;
	if (fTeabaggableTime > kTeabagTimeThreshold && crouchedChangedCount >= 4)
		rules.m_specialCases.Apply(eHRM_Teabag, fun);

	// eHRM_SkillKill,
	if (details.m_kill.bulletTimeKill)
		rules.m_specialCases.Apply(eHRM_SkillKill, fun);

	// Set Return Value.
	rEarliestTime = max(earliestTimeFP, earliestTimeTP);
	return fun;
}

void CRecordingSystem::AddRecordingEntity(IEntity* pEntity, const SRecordingEntity::EEntityType type)
{
	const EntityId entityId = pEntity->GetId();
	RecSysLogDebug(eRSD_Visibility, "AddRecordingEntity [%d] %s: %s", entityId, pEntity->GetClass()->GetName(), pEntity->GetName());

	SRecordingEntity& entity = m_recordingEntities[entityId];
	entity.entityId = entityId;
	entity.type = type;
	entity.hiddenState = pEntity->IsHidden() ? SRecordingEntity::eHS_Hide : SRecordingEntity::eHS_Unhide;
	Matrix34 transfom(pEntity->GetWorldTM());
	transfom.OrthonormalizeFast();
	entity.location.t = transfom.GetTranslation();
	entity.location.q = Quat(transfom);
	RecSysLogDebugIf(eRSD_General, !pEntity->GetScale().IsEquivalent(Vec3Constants<float>::fVec3_One), "Recording scaled entity. Scale[%.3f, %.3f, %.3f] [%d] %s: %s", pEntity->GetScale().x, pEntity->GetScale().y, pEntity->GetScale().z, entityId, pEntity->GetClass()->GetName(), pEntity->GetName());

	if (IsPlayingBack())
	{
		HideEntityKeepingPhysics(pEntity, true);
	}

	gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_HIDE, this);
	gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_UNHIDE, this);
}

void CRecordingSystem::RemoveRecordingEntity(SRecordingEntity& recEnt)
{
	RecSysLogDebug(eRSD_Visibility, "RemoveRecordingEntity [%d] %s", recEnt.entityId, recEnt.hiddenState == SRecordingEntity::eHS_Hide ? "Hidden" : "Shown");
	gEnv->pEntitySystem->RemoveEntityEventListener(recEnt.entityId, ENTITY_EVENT_HIDE, this);
	gEnv->pEntitySystem->RemoveEntityEventListener(recEnt.entityId, ENTITY_EVENT_UNHIDE, this);
}

//////////////////////////////////////////////////////////////////////////

CReplayMannListener::CReplayMannListener(EntityId entId, CRecordingSystem& recordingSystem)
	: m_entId(entId)
	, m_recordingSystem(recordingSystem)
{}

CReplayMannListener::~CReplayMannListener()
{}

void CReplayMannListener::OnEvent(const SMannHistoryItem& historyItem, const IActionController& actionController)
{
	m_recordingSystem.OnMannequinRecordHistoryItem(historyItem, actionController, m_entId);
}

//////////////////////////////////////////////////////////////////////////

void SHighlightRules::Init(XmlNodeRef& node)
{
	IEntityClassRegistry* pClassReg = gEnv->pEntitySystem->GetClassRegistry();
	CGameRules* pGameRules = g_pGame->GetGameRules();

	if (node)
	{
		float add = 0.f, mul = 1.f;
		const int numGroups = node->getChildCount();
		for (int g = 0; g < numGroups; ++g)
		{
			XmlNodeRef group = node->getChild(g);
			//////////////////////////////////////////////////////////////////////////
			if (group->isTag("HitTypes"))
			{
				for (int c = 0, num = group->getChildCount(); c < num; ++c)
				{
					XmlNodeRef child = group->getChild(c);
					add = 0.f;
					child->getAttr("add", add);
					mul = 1.f;
					child->getAttr("mul", mul);
					if (child->isTag("Default"))
						m_hitTypes.SetDefault(add, mul);
					else if (const char* type = child->getAttr("type"))
					{
						if (const int typeId = pGameRules->GetHitTypeId(type))
							m_hitTypes.Add(typeId, add, mul);
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////
			else if (group->isTag("MultiKills"))
			{
				for (int c = 0, num = group->getChildCount(); c < num; ++c)
				{
					XmlNodeRef child = group->getChild(c);
					add = 0.f;
					child->getAttr("add", add);
					mul = 1.f;
					child->getAttr("mul", mul);
					if (child->isTag("Default"))
						m_multiKills.SetDefault(add, mul);
					else
					{
						int kills = 0;
						child->getAttr("kills", kills);
						m_multiKills.Add(kills, add, mul);
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////
			else if (group->isTag("Weapons"))
			{
				for (int c = 0, num = group->getChildCount(); c < num; ++c)
				{
					XmlNodeRef child = group->getChild(c);
					add = 0.f;
					child->getAttr("add", add);
					mul = 1.f;
					child->getAttr("mul", mul);
					if (child->isTag("Default"))
						m_weapons.SetDefault(add, mul);
					else if (const char* name = child->getAttr("name"))
					{
						if (IEntityClass* pWeapon = pClassReg->FindClass(name))
							m_weapons.Add(pWeapon, add, mul);
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////
			else if (group->isTag("Special"))
			{
				for (int c = 0, num = group->getChildCount(); c < num; ++c)
				{
					XmlNodeRef child = group->getChild(c);
					add = 0.f;
					child->getAttr("add", add);
					mul = 1.f;
					child->getAttr("mul", mul);
					if (child->isTag("Stealth"))
						m_specialCases.Add(eHRM_Stealthed, add, mul);
					else if (child->isTag("Armour"))
						m_specialCases.Add(eHRM_Armour, add, mul);
					else if (child->isTag("MaxSuit"))
						m_specialCases.Add(eHRM_MaxSuit, add, mul);
					else if (child->isTag("NanoVision"))
						m_specialCases.Add(eHRM_NanoVision, add, mul);
					else if (child->isTag("Teabag"))
						m_specialCases.Add(eHRM_Teabag, add, mul);
					else if (child->isTag("SkillKill"))
						m_specialCases.Add(eHRM_SkillKill, add, mul);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void SPlaybackInfo::Init(const SPlaybackRequest& request, const SRecordedData& initialData, CRecordingBuffer& tpData)
{
	Reset(&request);

	// Setup initial View.
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(request.kill.killerId))
	{
		const Matrix34& transform = pEntity->GetWorldTM();
		m_view.precacheCamPos = transform.GetTranslation();
		m_view.precacheCamDir = transform.GetColumn1();
		m_view.precacheFlags |= ePF_StartPos;
	}

	// Get weapons for streaming.
	if (CGameSharedParametersStorage* pStorage = g_pGame->GetGameSharedParametersStorage())
	{
		const EntityId killerWeaponId = CRecordingSystem::FindWeaponAtTime(initialData, tpData, request.kill.killerId, request.kill.deathTime - 1.55f);
		if (IEntity* pKillerWeaponEntity = gEnv->pEntitySystem->GetEntity(killerWeaponId))
		{
			if (IEntityClass* pWeaponClass = pKillerWeaponEntity->GetClass())
			{
				if (CItemSharedParams* pItemSharedParams = pStorage->GetItemSharedParameters(pWeaponClass->GetName(), false))
				{
					if (const SGeometryDef* pGeomDef = pItemSharedParams->GetGeometryForSlot(eIGS_FirstPerson))
					{
						m_view.streamingWeaponFPModel = pGeomDef->modelPath.c_str();
						m_view.precacheFlags |= ePF_Weapons;
					}
				}
			}
		}
		const EntityId victimWeaponId = CRecordingSystem::FindWeaponAtTime(initialData, tpData, request.kill.victimId, request.kill.deathTime - 1.55f);
		if (IEntity* pVictimWeaponEntity = gEnv->pEntitySystem->GetEntity(victimWeaponId))
		{
			if (IEntityClass* pWeaponClass = pVictimWeaponEntity->GetClass())
			{
				if (CItemSharedParams* pItemSharedParams = pStorage->GetItemSharedParameters(pWeaponClass->GetName(), false))
				{
					if (const SGeometryDef* pGeomDef = pItemSharedParams->GetGeometryForSlot(eIGS_ThirdPerson))
					{
						m_view.streamingWeaponTPModel = pGeomDef->modelPath.c_str();
						m_view.precacheFlags |= ePF_Weapons;
					}
				}
			}
		}
		//~[PC ONLY]
	}

	// Precache for highlights straight away!
	if (request.highlights)
	{
		m_view.Precache(0.f, true);
	}
}

void SPlaybackInfo::View::Reset()
{
	ClearPrecachedWeapons();
	streamingWeaponFPModel.clear();
	streamingWeaponTPModel.clear();
	lastSTAPCameraDelta.SetIdentity();
	camSmoothingRate.zero();
	precacheCamPos.zero();
	precacheCamDir.Set(0.f, 1.f, 0.f);
	currView = prevView = eVM_FirstPerson;
	bWorldSpace = false;
	precacheFlags = 0;
	fadeOut = fadeOutRate = 0.f;
}

void SPlaybackInfo::View::Precache(const float timeBeforePlayback, const bool bForce /*= false*/)
{
	const bool bPrecacheStartPos = (precacheFlags & ePF_StartPos) != 0 && (precacheFlags & ePF_StartPosDone) == 0 && ((timeBeforePlayback < g_pGameCVars->kc_precacheTimeStartPos) || bForce);
	if (bPrecacheStartPos)
	{
		// Precache the Cam position to start in.
		RecSysLog("Precaching expected start position");
		gEnv->p3DEngine->AddPrecachePoint(precacheCamPos, precacheCamDir, timeBeforePlayback + 0.5f, 0.8f);
		precacheFlags |= ePF_StartPosDone;
	}

	const bool bPrecacheWeapons = (precacheFlags & ePF_Weapons) != 0 && (precacheFlags & ePF_WeaponsDone) == 0 && ((timeBeforePlayback < g_pGameCVars->kc_precacheTimeWeaponModels) || bForce);
	if (bPrecacheWeapons)
	{
		if (ICharacterManager* pCharManager = gEnv->pCharacterManager)
		{
			// Precache the FP weapon.
			if (!streamingWeaponFPModel.empty())
			{
				RecSysLog("Precaching FP Weapon Model: %s", streamingWeaponFPModel.c_str());
				pCharManager->StreamKeepCharacterResourcesResident(streamingWeaponFPModel.c_str(), eFPWeaponLOD, true);
				precacheFlags |= ePF_FPWeaponCached;
			}
			// Precache the TP weapon.
			if (!streamingWeaponTPModel.empty())
			{
				RecSysLog("Precaching TP Weapon Model: %s", streamingWeaponTPModel.c_str());
				pCharManager->StreamKeepCharacterResourcesResident(streamingWeaponTPModel.c_str(), 0, true);
				precacheFlags |= ePF_TPWeaponCached;
			}
		}
		precacheFlags |= ePF_WeaponsDone;
	}
}

void SPlaybackInfo::View::ClearPrecachedWeapons()
{
	ICharacterManager* pCharManager = gEnv->pCharacterManager;

	// Clear FP Weapon Model.
	if ((precacheFlags & ePF_FPWeaponCached) != 0 && pCharManager)
	{
		pCharManager->StreamKeepCharacterResourcesResident(streamingWeaponFPModel.c_str(), eFPWeaponLOD, false);
	}
	precacheFlags &= ~ePF_FPWeaponCached;
	streamingWeaponFPModel.clear();

	// Clear TP Weapon Model.
	if ((precacheFlags & ePF_TPWeaponCached) != 0 && pCharManager)
	{
		pCharManager->StreamKeepCharacterResourcesResident(streamingWeaponTPModel.c_str(), 0, false);
	}
	precacheFlags &= ~ePF_TPWeaponCached;
	streamingWeaponTPModel.clear();
}

const SPlayerInitialState* SRecordedData::GetPlayerInitialState(const EntityId playerId) const
{
	for (int i = 0; i < MAX_RECORDED_PLAYERS; i++)
	{
		const SPlayerInitialState& initialState = m_playerInitialStates[i];
		if (initialState.playerId == playerId)
		{
			return &initialState;
		}
	}
	return NULL;
}
