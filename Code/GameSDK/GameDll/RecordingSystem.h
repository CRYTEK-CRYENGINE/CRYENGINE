// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RECORDINGSYSTEM_H__
#define __RECORDINGSYSTEM_H__

#include "IKTorsoAim_Helper.h"
#include "WeaponFPAiming.h"
#include <CryParticleSystem/IParticles.h>
#include "RecordingBuffer.h"
#include "TracerManager.h"
#include "PlayerPlugin.h"
#include <CryCore/Containers/CryFixedArray.h>
#include "Effects/GameEffects/KillCamGameEffect.h"
#include "Effects/GameEffects/PlayerHealthEffect.h"
#include "Audio/AudioSignalPlayer.h"
#include "GameRulesModules/IGameRulesPickupListener.h"
#include <IWeapon.h>
#include "CorpseManager.h"
#include "RecordingSystemPackets.h"
#include "Actor.h"
#include "RecordingSystemClientSender.h"
#include "RecordingSystemServerForwarder.h"
#include "RecordingSystemDefines.h"

class CActor;
class CBufferUtil;
class CWeapon;
class CPlayer;
class CReplayObject;
class CReplayActor;
struct STracerParams;
struct IRenderNode;
class CItem;

struct SRecordedData
{
	SRecordedData()
	{
		Reset();
	}

	void Reset()
	{
		m_FirstPersonDataContainer.Reset();
		m_tpdatasize = 0;

		m_discardedEntitySpawns.clear();
		m_discardedWeaponAccessories.clear();
		m_discardedParticleSpawns.clear();
		//m_discardedSounds.clear();
		m_discardedEntityAttached.clear();
		m_corpses.clear();
	}

	void GetMemoryUsage(class ICrySizer *pSizer) const
	{
		pSizer->AddContainer(m_discardedEntitySpawns);
		pSizer->AddContainer(m_discardedWeaponAccessories);
		pSizer->AddContainer(m_discardedParticleSpawns);
		//pSizer->AddContainer(m_discardedSounds);
		pSizer->AddContainer(m_discardedEntityAttached);
	}

	void CopyInitialStateTo(SRecordedData &target)
	{
		memcpy(target.m_playerInitialStates, m_playerInitialStates, sizeof(m_playerInitialStates));
	
		target.m_discardedEntitySpawns = m_discardedEntitySpawns;
		target.m_discardedWeaponAccessories = m_discardedWeaponAccessories;
		target.m_discardedParticleSpawns = m_discardedParticleSpawns;
		//target.m_discardedSounds = m_discardedSounds;
		target.m_discardedEntityAttached = m_discardedEntityAttached;
		target.m_corpses = m_corpses;
	}

	const SPlayerInitialState* GetPlayerInitialState( const EntityId playerId ) const;

	SFirstPersonDataContainer m_FirstPersonDataContainer;

	// Buffer used for replaying third person data
	uint32 m_tpdatasize;
	uint8 m_tpdatabuffer[RECORDING_BUFFER_SIZE];

	SPlayerInitialState m_playerInitialStates[MAX_RECORDED_PLAYERS];
	
	TEntitySpawnMap m_discardedEntitySpawns;
	TWeaponAccessoryMap m_discardedWeaponAccessories;
	TParticleCreatedMap m_discardedParticleSpawns;
	//TPlaySoundMap m_discardedSounds;
	TEntityAttachedVec m_discardedEntityAttached;
	CryFixedArray<STrackedCorpse, MAX_CORPSES> m_corpses;
};

class CRecordingSystem
	: public IEntitySystemSink
	, IEntityEventListener
	, IParticleEffectListener
	//, ISoundSystemEventListener
	, IBreakEventListener
	, IWeaponEventListener
	, IGameRulesPickupListener
#ifdef RECSYS_DEBUG
	, IInputEventListener
#endif //RECSYS_DEBUG
{
public:
	CRecordingSystem();
	~CRecordingSystem();

	void Reset(void);
	void Init(XmlNodeRef root);

	static const SRecording_Packet * GetFirstFPActionPacketOfType(const SFirstPersonDataContainer& rFirstPersonData, const SPlaybackInstanceData& rPlaybackInstanceData, uint8 type);
	static ILINE bool KillCamEnabled() { return g_pGameCVars->kc_enable && !g_pGameCVars->g_modevarivar_disableKillCam; }

	void Update(const float frameTime);
	void PostUpdate();
	void RecordEntityInfo();
	void RecordParticleInfo();
	SRecordingEntity::EEntityType RecordEntitySpawn(IEntity* pEntity);

	// IInputEventListener
#ifdef RECSYS_DEBUG
	bool OnInputEvent(const SInputEvent &rInputEvent);
#endif //RECSYS_DEBUG
	//~IInputEventListener

	// IEntitySystemSink
	virtual bool OnBeforeSpawn(SEntitySpawnParams &params);
	virtual void OnSpawn(IEntity *pEntity, SEntitySpawnParams &params);
	virtual bool OnRemove(IEntity *pEntity);
	virtual void OnReused(IEntity *pEntity, SEntitySpawnParams &params);
	//~IEntitySystemSink

	// IEntityEventListener
	virtual void OnEntityEvent(IEntity *pEntity, const SEntityEvent &event);
	//~IEntityEventListener

	// IParticleEffectListener
	virtual void OnCreateEmitter(IParticleEmitter* pEmitter);
	virtual void OnDeleteEmitter(IParticleEmitter* pEmitter);
	//~IParticleEffectListener

	// ISoundSystemEventListener
	//virtual void OnSoundSystemEvent(ESoundSystemCallbackEvent event, ISound *pSound);
	//~ISoundSystemEventListener

	// IBreakEventListener
	virtual void OnBreakEvent(uint16 uBreakEventIndex);
	virtual void OnPartRemoveEvent(int iRemovePartEventIndex);
	virtual void OnEntityDrawSlot(IEntity * pEntity, int32 slot, int32 flags);
	virtual void OnEntityChangeStatObj(IEntity * pEntity, int32 iBrokenObjectIndex, int32 slot, IStatObj * pOldStatObj, IStatObj * pNewStatObj);
	virtual void OnSetSubObjHideMask(IEntity *pEntity, int nSlot, hidemask nSubObjHideMask);
	//~IBreakEventListener

	//Helper for breakage
	void ClearBreakageSpawn(IEntity * pEntity, EntityId entityId);

	// IWeaponEventListener
	virtual void OnShoot(IWeapon *pWeapon, EntityId shooterId, EntityId ammoId, IEntityClass* pAmmoType, const Vec3 &pos, const Vec3 &dir, const Vec3 &vel);
	virtual void OnStartFire(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnStopFire(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnStartReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {}
	virtual void OnEndReload(IWeapon *pWeapon, EntityId shooterId, IEntityClass* pAmmoType) {}
	virtual void OnOutOfAmmo(IWeapon *pWeapon, IEntityClass* pAmmoType) {}
	virtual void OnSetAmmoCount(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnReadyToFire(IWeapon *pWeapon) {}
	virtual void OnPickedUp(IWeapon *pWeapon, EntityId actorId, bool destroyed) {}
	virtual void OnDropped(IWeapon *pWeapon, EntityId actorId) {}
	virtual void OnMelee(IWeapon* pWeapon, EntityId shooterId) {}
	virtual void OnStartTargetting(IWeapon *pWeapon) {}
	virtual void OnStopTargetting(IWeapon *pWeapon) {}
	virtual void OnSelected(IWeapon *pWeapon, bool selected);
	virtual void OnEndBurst(IWeapon *pWeapon, EntityId shooterId) {}
	virtual void OnFireModeChanged(IWeapon *pWeapon, int currentFireMode);
	virtual void OnWeaponRippedOff(CWeapon *pWeapon);
	//~IWeaponEventListener

	// IGameRulesPickupListener
	virtual void OnItemPickedUp(EntityId itemId, EntityId actorId) {}
	virtual void OnItemDropped(EntityId itemId, EntityId actorId) {}
	virtual void OnPickupEntityAttached(EntityId entityId, EntityId actorId);
	virtual void OnPickupEntityDetached(EntityId entityId, EntityId actorId, bool isOnRemove);
	//~IGameRulesPickupListener

	// Event indicating a tracer is being created, returns false to prevent creation of the tracer
	bool OnEmitTracer(const CTracerManager::STracerParams &params);

	void OnPlayerPluginEvent(CPlayer* pPlayer, EPlayerPlugInEvent pluginEvent, void *pData);

	void OnGameRulesInit();
	void OnStartGame();

	void OnKill(IActor *pVictimActor, const HitInfo &hitInfo, bool winningKill, bool bulletTimeKill);

	void OnMountedGunUpdate(CPlayer* pPlayer, float aimrad, float aimUp, float aimDown);
	void OnMountedGunRotate(IEntity* pMountedGun, const Quat& rotation);
	void OnMountedGunEnter(CPlayer* pPlayer, int upAnimId, int downAnimId);
	void OnMountedGunLeave(CPlayer* pPlayer);

	void OnBattleChatter(EBattlechatter chatter, EntityId actorId, int variation);

	void OnAttachAccessory(CItem* pItem);

	void OnPlayAnimation(IEntity *entity, int animId, const CryCharAnimationParams& Params, float speedMultiplier);
	void OnPlayUpperAnimation(IEntity *entity, int animId, const CryCharAnimationParams& Params, float speedMultiplier);

	void OnParticleEmitterMoved(IParticleEmitter* pEmitter, const Matrix34 &loc);
	void OnParticleEmitterTargetSet(IParticleEmitter* pEmitter, const ParticleTarget& target);

	void OnSetTeam(EntityId entityId, int team);

	void OnItemSwitchToHand(CItem* pItem, int hand);

	void OnPlayerHealthEffect(const CPlayerHealthGameEffect::SQueuedHit& hitParams);

	void OnPlayerJoined(EntityId playerId);
	void OnPlayerLeft(EntityId playerId);
	void OnPlayerChangedModel(CPlayer *pPlayer);

	void OnCorpseSpawned(EntityId corpseId, EntityId playerId);
	void OnCorpseRemoved(EntityId corpseId);

	void OnPickAndThrowUsed(EntityId playerId, EntityId pickAndThrowId, EntityId enviromentEntityId, bool bPickedUp);
	void OnForcedRagdollAndImpulse( EntityId playerId, Vec3 vecImpulse, Vec3 pos, int partid, int applyTime );
	void OnApplyImpulseToRagdoll( const EntityId playerId, pe_action_impulse& impulse );

	void OnObjectCloakSync( const EntityId objectId, const EntityId ownerId, const bool cloakSlave, const bool fade = true);// set fade to false to snap to cloaked/uncloaked state

	void OnMannequinSetSlaveController(EntityId inMasterActorId, EntityId inSlaveActorId, uint32 inTargetContext, bool inEnslave, const IAnimationDatabase* piOptionalDatabase);
	void OnMannequinSetParam(EntityId inEntityId, const char *inParamName, QuatT &inQuatParam);
	void OnMannequinSetParamFloat(EntityId inEntityId, uint32 inParamCRC, float inFloatParam);
	void OnMannequinRecordHistoryItem( const SMannHistoryItem &historyItem, const IActionController& actionController, const EntityId entityId );

	void OnInteractiveObjectFinishedUse(EntityId objectId, TagID interactionTypeTag, uint8 interactionIndex);

	ILINE bool IsPlaybackQueued() const { return m_playbackState == ePS_Queued; }
	ILINE bool IsPlayingBack() const { return m_playbackState == ePS_Playing; }
	ILINE bool IsPlayingHighlightsReel() const { return (m_bShowAllHighlights || m_highlightsReel); }
	ILINE bool IsPlayingOrQueued() const { return m_playbackState == ePS_Playing || m_playbackState == ePS_Queued; }
	ILINE bool IsRecording() const { return m_recordingState == eRS_Recording; }
	ILINE bool IsRecordingAndNotPlaying() const { return m_recordingState == eRS_Recording && m_playbackState != ePS_Playing; }

	bool PlayWinningKillcam() { m_bCanPlayWinningKill = m_hasWinningKillcam?true:m_bCanPlayWinningKill; return m_hasWinningKillcam; }
	bool HasWinningKillcam() const { return m_hasWinningKillcam; }

	void StartRecording();
	void StopPlayback();
	void StopHighlightReel();

	void RevertBrokenObjectsToStateAtTime(float fKillCamStartTime);
	void ApplyEntitySlotVisibility();

	size_t GetFirstPersonData(uint8 **data, EntityId victimId, float deathTime, bool skillKill, const float length);
	size_t GetFirstPersonDataForTimeRange(uint8 **data, float fromTime, float toTime);
	size_t GetVictimDataForTimeRange(uint8 **data, EntityId victimID, float fromTime, float toTime, bool bulletTimeKill, float timeOffset);
	void GetTPCameraData(float startTime);
	static void SetFirstPersonData(uint8 *data, size_t datasize, SPlaybackInstanceData& rPlaybackInstanceData, SFirstPersonDataContainer& rFirstPersonDataContainer);

	void UpdateView(SViewParams &viewParams);

	void AddPacket(const SRecording_Packet &packet);
	void SetPlayerVelocity(EntityId eid, Vec3 vel) {m_chrvelmap[eid] = vel;}
	void SetPlayerAimIK(EntityId eid, AimIKInfo aikinf) {m_eaikm[eid] = aikinf;}
	void DiscardingPacket(SRecording_Packet *packet, float recordedTime);
	void OnPlayerFlashed(float duration, float blindAmount);
	void OnPlayerRenderNearestChange(bool renderNearest);

	ILINE CKillCamGameEffect & GetKillCamGameEffect ( ) { return m_killCamGameEffect; }

	void ReRecord(const EntityId entityId);

	void OnTrackerRayCastDataReceived( const QueuedRayID& rayID, const RayCastResult& result );

	// originalEntity should be set to true if the entityId provided is the ID of the real world entity
	// otherwise entityId refers to the replay entity's id
	CReplayActor* GetReplayActor(EntityId entityId, bool originalEntity);
	IEntity* GetReplayEntity(EntityId originalEntityId);
	CItem* GetReplayItem(EntityId originalEntityId);
	static void HideEntityKeepingPhysics(IEntity *pEntity, bool hide);
	ILINE bool IsInBulletTime() const { return m_playInfo.m_view.currView==SPlaybackInfo::eVM_BulletTime; }

	enum eStringCache
	{
		eSC_Sound,
		eSC_Model,
	};

	const char* CacheString(const char* name, eStringCache cache);

	void GetMemoryUsage(class ICrySizer *pSizer) const  
	{ 
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_pBuffer);
		pSizer->AddObject(m_pBufferFirstPerson);

		m_sender.GetMemoryUsage(pSizer);
		m_forwarder.GetMemoryUsage(pSizer);

		pSizer->AddContainer(m_replayActors);
		pSizer->AddContainer(m_replayEntities);
		pSizer->AddContainer(m_replayParticles);
		//pSizer->AddContainer(m_replaySounds);
		pSizer->AddContainer(m_recordingEntities);
		pSizer->AddContainer(m_newParticles);
		pSizer->AddContainer(m_newActorEntities);
		pSizer->AddContainer(m_chrvelmap);
		pSizer->AddContainer(m_eaikm);
		pSizer->AddContainer(m_recordEntityClassFilter);
		pSizer->AddContainer(m_excludedParticleEffects);
		pSizer->AddContainer(m_replacementParticleEffects);
		
		m_recordedData.GetMemoryUsage(pSizer);

		for (int i = 0; i < MAX_HIGHLIGHTS; ++ i)
		{
			m_highlightData[i].m_data.GetMemoryUsage(pSizer);
		}
	}

	IActionController* GetActionController(EntityId entityId);

	void SvProcessKillCamData(IActor *pActor, const CActor::KillCamFPData &packet);
	void ClProcessKillCamData(IActor *pActor, const CActor::KillCamFPData &packet);
	
	static bool ProcessKillCamData(IActor *pActor, const CActor::KillCamFPData &packet, SKillCamStreamData& rStreamData, const bool bFatalOnForwardedPackets = false );
	static void ExtractCompleteStreamData( SKillCamStreamData& streamData, SPlaybackInstanceData& rPlaybackInstanceData, SFirstPersonDataContainer& rFirstPersonDataContainer);

	void AddKillCamData(IActor *pActor, EntityId victim, bool bulletTimeKill, bool bSendToAll=false);
	void UpdateSendQueues();

	void RegisterReplayMannListener( class CReplayMannListener& listener );
	void UnregisterReplayMannListener( const EntityId entityId );

	// LISTENERS:
	void RegisterListener ( IRecordingSystemListener& listener );
	void UnregisterListener ( IRecordingSystemListener& listener );
	void NotifyOnPlaybackRequested( const SPlaybackRequest& info );
	void NotifyOnPlaybackStarted( const SPlaybackInfo& info );
	void NotifyOnPlaybackEnd( const SPlaybackInfo& info );

	// HIGHLIGHTS:
	bool PlayAllHighlights(bool bLoop);
	void SaveHighlight_Queue( const SKillInfo& kill, float length );
	float GetHighlightsReelLength() const;

	static EntityId NetIdToEntityId(uint16 netId);
	static IEntityClass* GetEntityClass_NetSafe(const uint16 classId);

	ILINE const SPlaybackInfo& GetPlaybackInfo() const {return m_playInfo;}

	enum EKillCamHandling
	{
		eKCH_None,
		eKCH_StoreHighlight,
		eKCH_StoreHighlightAndSend
	};

	static EKillCamHandling ShouldHandleKillcam(IActor* pVictimActor, const HitInfo& hitInfo, bool bBulletTimeKill, float& outKillCamDelayTime);
	static EntityId FindWeaponAtTime( const SRecordedData& initialData, CRecordingBuffer& tpData, const EntityId playerId, const float atTime );

private:
	// The purpose of SStackGuard is to provide a mechanism by which certain events be guarded against.
	// This is best explained with an example:
	// The recording system will listen to all entity spawn events to track when new entities are created.
	// During killcam replay we will spawn new fake entities to replace the real ones for replay purposes.
	// The spawning of the fake entities will result in spawn events to be fired off, however we are not
	// interested in listening to spawn events of the fake entities. So in order to guard against recording
	// these events we can use this class.
	// When the object is constructed it increments a counter, and decrements it upon destruction. When
	// the counter is at 0 that means there is no guard in place and the event should be recorded.
	struct SStackGuard
	{
		SStackGuard(int& counter) : m_counter(counter) { counter++; }
		~SStackGuard() { m_counter--; }
		int& m_counter;
	};

	struct SRecordedKill
	{
		SRecordedKill() 
			: m_killCount(1)
			, m_timeTilRecord(0.f)
			, m_startTime(0.f)
		{}
		SRecordedKill(const SKillInfo& kill, float recDelay, float startTime)
			: m_kill(kill)
			, m_killCount(1)
			, m_timeTilRecord(recDelay)
			, m_startTime(startTime)
		{}

		SKillInfo m_kill;
		int m_killCount;
		float m_timeTilRecord;
		float m_startTime;
	};

	struct SHighlightData
	{
		SRecordedData m_data;
		SRecordedKill m_details;

		float m_fun;
		float m_endTime;
		bool m_bUsed;
	};

	struct SWeaponAudioCacheDetails
	{
		EntityId			weaponId;
		IEntityClass* pWeaponClass;
		IEntityClass* pAccessoryClasses[MAX_WEAPON_ACCESSORIES];
		
		SWeaponAudioCacheDetails()
		{
			Clear();
		}

		void Clear()
		{
			weaponId = 0;
			pWeaponClass = NULL;
			for (int i=0; i<MAX_WEAPON_ACCESSORIES; i++)
			{
				pAccessoryClasses[i] = NULL;
			}
		}
	};

	typedef CryFixedArray<SWeaponAudioCacheDetails, MAX_NUM_WEAPONS_AUDIO_CACHED> TWeaponAudioCacheArray;
	TWeaponAudioCacheArray m_cachedWeaponAudioDetails;

	// This macro provides a convenient way to guard against listenting to events we are not interested in.
#define REPLAY_EVENT_GUARD CRecordingSystem::SStackGuard __ReplayEventGuard__(CRecordingSystem::m_replayEventGuard);

	void ReleaseReferences(SRecording_EntitySpawn& entitySpawn);
	void AddReferences(SRecording_EntitySpawn& entitySpawn);
	void ExitWeaponZoom();
	float GetLiveBufferLenSeconds(void);
	float GetBufferLenSeconds(void);
	void FillOutFPCamera(SRecording_FPChar *cam, float playbacktime);
	void PlaybackRecordedFirstPersonActions(float playbacktime);
	float GetFPCamDataTime();
	void DropTPFrame();
	void UpdateRecordedData(SRecordedData *pRecordingData);
	bool GetWeaponAudioCacheDetailsForWeaponId(EntityId inWeaponId, SWeaponAudioCacheDetails &outDetails);
	void ActuallyAudioCacheWeaponDetails(bool enable);
	void HandleAudioCachingOfKillersWeapons();
	void OnPlaybackStart(void);
	void DebugDrawAnimationData(void);
	void DebugDrawMemoryUsage();
	bool QueueStartPlayback(const SPlaybackRequest& request);
	void GoIntoSpectatorMode();
	bool GetVictimPosition(float playbackTime, Vec3& victimPosition) const;
	bool GetKillHitPosition(Vec3& killHitPos) const;
	float GetPlaybackTimeOffset() const;
	void AdjustDeathTime();
	void ExtractBulletPositions();
	void UpdateBulletPosition();
	void SetDofFocusDistance(float focusDistance);
	void SetTimeScale(float timeScale);
	bool GetFrameTimes(float &lowerTime, float &upperTime);
	template <unsigned int N>
	void ExtractVictimPositions(CryFixedArray<SRecording_VictimPosition, N> &victimPositions, float startTime, EntityId victimId, float deathTime, float endTime=-1);
	void HideItem(IEntity* pEntity, bool hide);
	void QueueAddPacket(const SRecording_Packet &packet);
	const STracerParams* GetKillerTracerParams();
	void StopFirstPersonWeaponSounds();
	void UpdateEntitySpawnGeometry(IEntity* pEntity);
	void ClearEntitySpawnGeometry(SRecording_EntitySpawn* pEntitySpawn);
	void RecordEntitySpawnGeometry(SRecording_EntitySpawn* pEntitySpawn, IEntity* pEntity);
	bool IsStealthKill(int animId);
	void UpdatePlayback(float frameTime);
	float PlaybackRecordedFrame(float frameTime);
	void InterpolateRecordedPositions(float frameTime, float playbackTime, float lerpValue);
	void RecordFPCameraPos(const QuatT& view, const float fov, IEntity* pLinkedEntity, IActor* pLinkedActor);
	void RecordFPKillHitPos(IActor& victimActor, const HitInfo& hitInfo);
	void RecordTPCharPacket(IEntity *pEntity, CActor *pActor);
	void RecordWeaponAccessories(CItem* pItem);
	void RecordWeaponSelection(CWeapon *pWeapon, bool selected);
	void RecordActorWeapon();
	void RecordWeaponAccessories();
	void AddQueuedPackets();
	bool ApplyPlayerInitialState(EntityId entityId);
	void UpdateFirstPersonPosition();
	void UpdateThirdPersonPosition(const SRecording_TPChar *tpchar, float frameTime, float playbackTime, const SRecording_TPChar *tpchar2 = NULL, float lerpValue = 0.f);
	void ApplyWeaponShoot(const SRecording_OnShoot *pShoot);
	void ApplyWeaponAccessories(const SRecording_WeaponAccessories *weaponAccessories);
	void ApplyWeaponSelect(const SRecording_WeaponSelect *weaponSelect);
	void ApplyFiremodeChanged(const SRecording_FiremodeChanged *firemodeChanged);
	void ApplyMannequinEvent(const SRecording_MannEvent *pMannEvent, float animTime = 0);
	void ApplyMannequinSetSlaveController(const SRecording_MannSetSlaveController *pMannSetSlave);
	void ApplyMannequinSetParam(const SRecording_MannSetParam *pMannSetParam);
	void ApplyMannequinSetParamFloat(const SRecording_MannSetParamFloat *pMannSetParamFloat);
	void OnPlayerFirstPersonChange(IEntity* pPlayerEntity, EntityId weaponId, bool firstPerson);
	void CloakEnable(IEntityRender* pIEntityRender, bool enable, bool fade);
	void ApplyEntitySpawn(const SRecording_EntitySpawn *entitySpawn, float time = 0.0f);
	void ApplyEntityRemoved(const SRecording_EntityRemoved *entityRemoved);
	void ApplyEntityLocation(const SRecording_EntityLocation *entityLocation, const SRecording_EntityLocation *entityLocation2 = NULL, float lerpValue = 0.f);
	void ApplyEntityHide(const SRecording_EntityHide *entityHide);
	void ApplySpawnCustomParticle(const SRecording_SpawnCustomParticle *spawnCustomParticle);
	bool ApplyParticleCreated(const SRecording_ParticleCreated *particleCreated, float time = 0);
	void ApplyParticleDeleted(const SRecording_ParticleDeleted *particleDeleted);
	void ApplyParticleLocation(const SRecording_ParticleLocation *particleLocation);
	void ApplyParticleTarget(const SRecording_ParticleTarget *particleTarget);
	void ApplyFlashed(const SRecording_Flashed *pFlashed);
	void ApplyRenderNearestChange(const SRecording_RenderNearest *pRenderNearest);
	void SwapFPCameraEndian(uint8* pBuffer, size_t size, bool sending);
	static void RemoveEntityPhysics( IEntity& entity );
	void ApplyPlaySound(const SRecording_PlaySound* pPlaySound, float time = 0);
	void ApplyStopSound(const SRecording_StopSound* pStopSound);
	void ApplySoundParameter(const SRecording_SoundParameter* pSoundParameter);
	void ApplyBattleChatter(const SRecording_BattleChatter* pPacket);
	void ClearStringCaches();
	void ApplyBulletTrail(const SRecording_BulletTrail* pBulletTrail);
	void ApplySingleProceduralBreakEvent(const SRecording_ProceduralBreakHappened* pProceduralBreak);
	void ApplyDrawSlotChange(const SRecording_DrawSlotChange* pDrawSlotChange);
	void ApplyStatObjChange(const SRecording_StatObjChange* pStatObjChange);
	void ApplySubObjHideMask(const SRecording_SubObjHideMask* pHideMask);
	void ApplyTeamChange(const SRecording_TeamChange* pTeamChange);
	void ApplyItemSwitchHand(const SRecording_ItemSwitchHand* pSwitchHand);
	bool ApplyEntityAttached(const SRecording_EntityAttached* pEntityAttached);
	void ApplyEntityDetached(const SRecording_EntityDetached* pEntityDetached);
	void ApplyPlayerHealthEffect(const SRecording_PlayerHealthEffect* pHitEffect);
	void ApplyAnimObjectUpdated(const SRecording_AnimObjectUpdated* pAnimObjectUpdated);
	void ApplyPickAndThrowUsed(const SRecording_PickAndThrowUsed* pPickAndThrowUsed);
	void ApplyForcedRagdollAndImpulse( const SRecording_ForcedRagdollAndImpulse* pImportantImpulse );
	void ApplyImpulseToRagdoll( const SRecording_RagdollImpulse* pRagdollImpulse );
	void ApplyObjectCloakSync(const SRecording_ObjectCloakSync* pEnvWeapCloakSync);
	void ApplyPlayerChangedModel(const SRecording_PlayerChangedModel *pData);

	void RecordSoundParameters();
	void ApplyMountedGunAnimation(const SRecording_MountedGunAnimation* pMountedGunAnimation);
	void ApplyMountedGunRotate(const SRecording_MountedGunRotate* pMountedGunRotate);
	void ApplyMountedGunEnter(const SRecording_MountedGunEnter* pMountedGunEnter);
	void ApplyMountedGunLeave(const SRecording_MountedGunLeave* pMountedGunLeave);
	void ApplyCorpseSpawned(const SRecording_CorpseSpawned *pCorpsePacket);

	IEntity *SpawnCorpse(EntityId originalId, const Vec3 &position, const Quat &rotation, const Vec3 &scale);

	void GetProceduralBreaksDuringKillcam(std::vector<uint16>& indices);
	void CleanUpBrokenObjects();
	uint16 EntityIdToNetId(EntityId entityId);
	ILINE bool FilterItemSuffix(const char* suffix) { return strcmp(suffix, "flag") == 0 || strcmp(suffix, "tick") == 0; }
	const float CalculateSafeRaiseDistance(const Vec3& pos);

	void CameraCollisionAdjustment(const IEntity* target, Vec3& cameraPos) const;
	void ReadFilteredEntityData(IEntityClassRegistry* pClassReg, XmlNodeRef xmlData);
	static void LoadClassData(const IEntityClassRegistry& classReg, XmlNodeRef xmlData, TEntityClassSet& data);

	void ProcessTrackedActors();
	void ProcessReplayEntityInfo(float& updateTimer, Vec3& lastPosition, bool onGround, const Vec3& forwardDirection, const Vec3& currentPosition, float individualUpdateRate, float individualMinMovementSqrd);
	void RequestTrackerRaycast(const Vec3& position, const Vec3& forwardDirection);

	void CleanUpDeferredRays();

	void GetVictimAimTarget(Vec3 &aimTarget, const Vec3 &localPosition, const Vec3 &remotePosition, float lerpValue);

	void AddRecordingEntity( IEntity* pEntity, const SRecordingEntity::EEntityType type );
	void RemoveRecordingEntity( SRecordingEntity& recEnt );

	// HIGHLIGHTS:
	void PlayHighlight(int index);
	void SaveHighlight( const SRecordedKill &details, const int indexOverride = -1 );
	void CleanupHighlight(int index);
	float AnalysePotentialHighlight(const SRecordedKill& details, float& rEarliestTime);

	void SetPlayBackCameraView(bool bSetView);

#ifdef RECSYS_DEBUG
	void DebugStoreHighlight( const float length, const int highlightIndex );
	static void FirstPersonDiscardingPacketStatic(SRecording_Packet *ps, float recordedTime, void *inUserData);
	void PrintCurrentRecordingBuffers ( const char* const msg = NULL );
#endif //RECSYS_DEBUG

	class CRecordingSystemDebug* GetDebug() const { return m_pDebug; }

	CKillCamGameEffect m_killCamGameEffect;
	CAudioSignalPlayer m_activeSoundLoop;
	CAudioSignalPlayer m_kvoltSoundLoop;
	TAudioSignalID m_tracerSignal;

	CServerKillCamForwarder m_forwarder;
	CClientKillCamSender m_sender;
	CKillCamDataStreamer m_streamer;
	class CRecordingSystemDebug* m_pDebug;
	TRecordingSystemListeners m_listeners;

	TReplayActorMap m_replayActors;
	TReplayEntityMap m_replayEntities;
	TReplayParticleMap m_replayParticles;
	//TReplaySoundMap m_replaySounds;
	TInteractiveObjectAnimationMap m_interactiveObjectAnimations;
	TRecEntitiesMap m_recordingEntities;
	std::vector<SRecordingAnimObject> m_recordingAnimEntities;
	std::vector<SRecording_ParticleCreated> m_newParticles;
	std::vector<EntityId> m_newActorEntities;
	std::vector<EntityId> m_itemAccessoriesChanged;
	std::vector<SSoundParamters> m_trackedSounds;
	std::vector<uint16> m_breakEventIndicies;		//A list of indices into the broken objects array in CryAction
	std::vector<IRenderNode*> m_clonedNodes;		//A list of all IRenderNodes that have been cloned for breakables in the killcam
	SRenderNodeCloneLookup m_renderNodeLookup;	//A lookup between the original IRenderNodes and the cloned IRenderNodes
																							//	could possibly be collapsed into m_clonedNodes

	std::vector<class CReplayMannListener *> m_mannequinListeners;

	std::vector<IRenderNode*> m_renderNodeLookupStorage_original;
	std::vector<IRenderNode*> m_renderNodeLookupStorage_clone;

	CRecordingBuffer		*m_pBuffer;
	CRecordingBuffer		*m_pBufferFirstPerson;
	float m_timeScale;
	float m_timeSincePlaybackStart;
	float m_recordedStartTime;
	uint32 m_nReplayEntityNumber;
	SPlaybackInstanceData m_PlaybackInstanceData;

	// Buffer used for sending first person camera data
	uint8 m_firstPersonSendBuffer[FP_RECORDING_BUFFER_SIZE];

	// Recorded packets which are queued to be added to the third person buffer next frame
	uint32 m_queuedPacketsSize;
	uint8 m_pQueuedPackets[RECORDING_BUFFER_QUEUE_SIZE];

	ERecordingState	m_recordingState;
	EPlaybackState m_playbackState;
	EPlaybackSetupStage m_playbackSetupStage;
	EProjectileType m_projectileType;
	EBulletTimePhase m_bulletTimePhase;

	SPlaybackInfo m_playInfo;
	EntityId m_killer;
	EntityId m_victim;
	EntityId m_projectileId;
	EntityId m_killerReplayGunId;

	ChrVelocityMap m_chrvelmap;
	EntityAimIKMap m_eaikm;
	CIKTorsoAim_Helper m_torsoAimIK;
	SParams_WeaponFPAiming m_weaponParams;
	CWeaponFPAiming m_weaponFPAiming;
	SRecording_FPChar m_firstPersonCharacter;
	QuatT m_replayBlendedCamPos;
	Ang3	m_lastView;
	float m_fLastRotSpeed;
	TEntityClassSet m_recordEntityClassFilter;
	TEntityClassSet m_mountedWeaponClasses;
#ifndef _RELEASE
	TEntityClassSet m_validWithNoGeometry;
#endif //_RELEASE
	TCorpseADIKJointsVec m_corpseADIKJointCRCs;
	std::set<IParticleEffect*> m_excludedParticleEffects;
	std::set<IParticleEffect*> m_persistantParticleEffects;
	std::set<uint32> m_excludedSoundEffects;
	TParticleEffectMap m_replacementParticleEffects;
	TStatObjMap m_replacementStatObjs;
	SHighlightRules m_highlightsRules;
	
	IEntityClass* m_pBreakage;
	IEntityClass* m_pRocket;
	IEntityClass* m_pC4Explosive;
	IEntityClass* m_pExplosiveGrenade;
	IEntityClass* m_pLTagGrenade;
	IEntityClass* m_pFlashbang;
	IEntityClass* m_pGrenadeLauncherGrenade;
	IEntityClass* m_pCorpse;
	IEntityClass *m_pInteractiveObjectExClass;

	const IAnimationDatabase*	m_pDatabaseInteractiveObjects;
	const SControllerDef*			m_pPlayerControllerDef;
	FragmentID								m_interactFragmentID;

	TPendingRaycastMap m_PendingTrackerRays;

	SRecordedData m_recordedData;
	SRecordedData *m_pPlaybackData;
	uint8 *m_pTPDataBuffer;
	uint32 m_tpDataBufferSize;

	SHighlightData m_highlightData[MAX_HIGHLIGHTS];
	SHighlightData *m_pHighlightData;
	SRecordedKill m_recordKillQueue;

	static int m_replayEventGuard;
	static int s_savingHighlightsGuard;
	QuatT m_newProjectilePosition;
	char m_soundNames[SOUND_NAMES_SIZE];
	char m_modelNames[MODEL_NAMES_SIZE];
	std::shared_ptr<ITimer> m_replayTimer;
	const char* m_bulletGeometryName;
	const char* m_bulletEffectName;
	Quat m_bulletOrientation;
	Vec3 m_bulletTarget;
	Vec3 m_bulletOrigin;
	Vec3 m_newBulletPosition;
	Vec3 m_postHitCameraPos;
	float m_bulletTravelTime;
	float m_bulletHoverStartTime;
	float m_timeSinceLastRecord;
	float m_timeSinceLastFPRecord;
	float m_C4Adjustment;
	float m_cameraBlendFraction;
	EntityId m_bulletEntityId;
	EntityId m_autoHideIgnoreEntityId;
	EntityId m_gameCameraLinkedToEntityId;
	int m_highlightIndex;

	float m_stressTestTimer;

	bool m_cameraSmoothing;
	bool m_highlightsReel;
	bool m_bLoopHighlightsReel;
	bool m_bShowAllHighlights;
	bool m_killerIsFriendly;
	bool m_clientShot;
	bool m_playedFrame;
	bool m_bulletTimeKill;
	bool m_disableRifling;
	bool m_hasWinningKillcam;
	bool m_bCanPlayWinningKill;

	//static const uint32 m_ambientSoundSemantics = eSoundSemantic_Ambience | eSoundSemantic_Ambience_OneShot | eSoundSemantic_SoundSpot | eSoundSemantic_Particle | eSoundSemantic_TrackView;
};

class CReplayMannListener : public IMannequinListener
{
public:
	CReplayMannListener(EntityId entId, CRecordingSystem &recordingSystem);
	~CReplayMannListener();

	virtual void OnEvent(const SMannHistoryItem &historyItem, const IActionController& actionController);
	ILINE EntityId GetEntityID ( ) const { return m_entId; }

private:
	EntityId				  m_entId;
	CRecordingSystem &m_recordingSystem;
};



#endif // __RECORDINGSYSTEM_H__
