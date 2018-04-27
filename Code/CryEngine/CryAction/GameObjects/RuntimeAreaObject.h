// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CRuntimeAreaObject : public CGameObjectExtensionHelper<CRuntimeAreaObject, IGameObjectExtension>
{
public:

	typedef unsigned int TSurfaceCRC;

	struct SAudioControls
	{
		CryAudio::ControlId audioTriggerId;
		CryAudio::ControlId audioRtpcId;

		explicit SAudioControls(
		  CryAudio::ControlId _audioTriggerId = CryAudio::InvalidControlId,
		  CryAudio::ControlId _audioRtpcId = CryAudio::InvalidControlId)
			: audioTriggerId(_audioTriggerId)
			, audioRtpcId(_audioRtpcId)
		{}
	};

	typedef std::map<TSurfaceCRC, SAudioControls> TAudioControlMap;

	static TAudioControlMap m_audioControls;

	CRuntimeAreaObject();
	virtual ~CRuntimeAreaObject() override;

	// IGameObjectExtension
	virtual bool                 Init(IGameObject* pGameObject) override;
	virtual void                 InitClient(int channelId) override                                                       {}
	virtual void                 PostInit(IGameObject* pGameObject) override                                              {}
	virtual void                 PostInitClient(int channelId) override                                                   {}
	virtual bool                 ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) override;
	virtual void                 PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) override {}
	virtual void                 FullSerialize(TSerialize ser) override                                                   {}
	virtual bool                 NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override;
	virtual void                 PostSerialize() override                                                                 {}
	virtual void                 SerializeSpawnInfo(TSerialize ser) override                                              {}
	virtual ISerializableInfoPtr GetSpawnInfo() override                                                                  { return NULL; }
	virtual void                 Update(SEntityUpdateContext& ctx, int slot) override                                     {}
	virtual void                 HandleEvent(const SGameObjectEvent& gameObjectEvent) override                            {}
	virtual void                 ProcessEvent(const SEntityEvent& entityEvent) override;
	virtual uint64               GetEventMask() const override;
	virtual void                 SetChannelId(uint16 id) override                                                         {}
	virtual void                 PostUpdate(float frameTime) override                                                     { CRY_ASSERT(false); }
	virtual void                 PostRemoteSpawn() override                                                               {}
	virtual void                 GetMemoryUsage(ICrySizer* pSizer) const override;
	// ~IGameObjectExtension

	// IEntityComponent
	virtual void OnShutDown() override;
	// ~IEntityComponent

private:

	struct SAreaSoundInfo
	{
		SAudioControls audioControls;
		float          parameter;

		explicit SAreaSoundInfo(SAudioControls const& _audioControls, float const _parameter = 0.0f)
			: audioControls(_audioControls)
			, parameter(_parameter)
		{}

		~SAreaSoundInfo()
		{}
	};

	typedef std::map<TSurfaceCRC, SAreaSoundInfo>  TAudioParameterMap;
	typedef std::map<EntityId, TAudioParameterMap> TEntitySoundsMap;

	void UpdateParameterValues(IEntity* const pEntity, TAudioParameterMap& paramMap);
	void StopEntitySounds(EntityId const entityId, TAudioParameterMap& paramMap);

	TEntitySoundsMap m_activeEntitySounds;
};
