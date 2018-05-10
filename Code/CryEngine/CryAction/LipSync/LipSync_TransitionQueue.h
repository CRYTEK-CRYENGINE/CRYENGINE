// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   LipSync_TransitionQueue.h
//  Version:     v1.00
//  Created:     2014-08-29 by Christian Werle.
//  Description: Automatic start of facial animation when a sound is being played back.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <CryAction/ILipSyncProvider.h>

/// The CLipSyncProvider_TransitionQueue communicates with sound proxy to play synchronized facial animation directly on the transition queue
class CLipSyncProvider_TransitionQueue : public ILipSyncProvider
{
public:
	explicit CLipSyncProvider_TransitionQueue(EntityId entityId);

	// ILipSyncProvider
	void RequestLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) override;
	void StartLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) override;
	void PauseLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) override;
	void UnpauseLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) override;
	void StopLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) override;
	void UpdateLipSync(IEntityAudioComponent* pProxy, const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod) override;
	// ~ILipSyncProvider

	void FullSerialize(TSerialize ser);

private:
	IEntity*            GetEntity();
	ICharacterInstance* GetCharacterInstance();
	void                FindMatchingAnim(const CryAudio::ControlId audioTriggerId, const ELipSyncMethod lipSyncMethod, ICharacterInstance& character, int* pAnimIdOut, CryCharAnimationParams* pAnimParamsOut) const;
	void                FillCharAnimationParams(const bool isDefaultAnim, CryCharAnimationParams* pParams) const;
	void                SynchronizeAnimationToSound(const CryAudio::ControlId audioTriggerId);

private:
	enum EState
	{
		eS_Init,
		eS_Requested,
		eS_Started,
		eS_Paused,
		eS_Unpaused,
		eS_Stopped,
	};

	static uint32 s_lastAnimationToken;

	EntityId      m_entityId;
	int           m_nCharacterSlot;   // the lip-sync animation will be played back on the character residing in this slot of the entity
	int           m_nAnimLayer;       // the lip-sync animation will be played back on this layer of the character
	string        m_sDefaultAnimName; // fallback animation to play if there is no animation matching the currently playing sound in the character's database

	EState        m_state;
	bool          m_isSynchronized;

	// Filled when request comes in:
	int                    m_requestedAnimId;
	CryCharAnimationParams m_requestedAnimParams;
	CAutoResourceCache_CAF m_cachedAnim;

	// Filled when animation is started:
	uint32         m_nCurrentAnimationToken;
	CryAudio::ControlId m_soundId;
};
DECLARE_SHARED_POINTERS(CLipSyncProvider_TransitionQueue);

class CLipSync_TransitionQueue : public CGameObjectExtensionHelper<CLipSync_TransitionQueue, IGameObjectExtension>
{
public:
	// IGameObjectExtension
	virtual void                 GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual bool                 Init(IGameObject* pGameObject) override;
	virtual void                 PostInit(IGameObject* pGameObject) override;
	virtual void                 InitClient(int channelId) override;
	virtual void                 PostInitClient(int channelId) override;
	virtual bool                 ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) override;
	virtual void                 PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) override;
	virtual void                 FullSerialize(TSerialize ser) override;
	virtual bool                 NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int pflags) override;
	virtual void                 PostSerialize() override;
	virtual void                 SerializeSpawnInfo(TSerialize ser) override;
	virtual ISerializableInfoPtr GetSpawnInfo() override;
	virtual void                 Update(SEntityUpdateContext& ctx, int updateSlot) override;
	virtual void                 HandleEvent(const SGameObjectEvent& event) override;
	virtual void                 ProcessEvent(const SEntityEvent& event) override;
	virtual uint64               GetEventMask() const override { return 0; }
	virtual void                 SetChannelId(uint16 id) override;
	virtual void                 PostUpdate(float frameTime) override;
	virtual void                 PostRemoteSpawn() override;
	// ~IGameObjectExtension

	// IEntityComponent
	virtual void OnShutDown() override;
	// ~IEntityComponent

private:
	void InjectLipSyncProvider();

private:
	CLipSyncProvider_TransitionQueuePtr m_pLipSyncProvider;
};
