// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 
#pragma once
#include <CryNetwork/INetEntity.h>
#include <CryEntitySystem/IEntity.h>

struct SEntitySchedulingProfiles;

class CNetEntity : public INetEntity
{
public:
	CNetEntity(IEntity *entity_);
	virtual ~CNetEntity();

	virtual bool BindToNetwork(EBindToNetworkMode mode) override;
	virtual bool BindToNetworkWithParent(EBindToNetworkMode mode, EntityId parentId) override;
	virtual void MarkAspectsDirty(NetworkAspectType aspects) override;
	virtual void EnableAspect(NetworkAspectType aspects, bool enable) override;
	virtual void EnableDelegatableAspect(NetworkAspectType aspects, bool enable) override;

	virtual bool CaptureProfileManager(IGameObjectProfileManager* pPM) override;
	virtual void ReleaseProfileManager(IGameObjectProfileManager* pPM) override;
	virtual bool HasProfileManager() override;
	virtual void ClearProfileManager() override;

	virtual void SetChannelId(uint16 id) override;
	virtual uint16 GetChannelId() const override { return m_channelId; };
	virtual NetworkAspectType GetEnabledAspects() const  override;
	virtual void BecomeBound() override { m_isBoundToNetwork = true; }
	virtual bool IsBoundToNetwork() const override { return m_isBoundToNetwork; }
	virtual void DontSyncPhysics() override { m_bNoSyncPhysics = true; }
	virtual bool IsAspectDelegatable(NetworkAspectType aspect) const override;

	virtual uint8 GetDefaultProfile(EEntityAspects aspect) override;

	virtual bool SetAspectProfile(EEntityAspects aspect, uint8 profile, bool fromNetwork) override;
	virtual uint8 GetAspectProfile(EEntityAspects aspect) override;

	virtual void SetAuthority(bool auth) override;
	virtual bool HasAuthority() const override;
	virtual void SetNetworkParent(EntityId id) override;

	virtual bool NetSerializeEntity(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override;
	
	virtual void RmiRegister(SRmiHandler& handler) override;
	virtual SRmiIndex RmiByDecoder(SRmiHandler::DecoderF decoder, SRmiHandler **handler) override;
	virtual SRmiHandler::DecoderF RmiByIndex(const SRmiIndex idx) override;


	static void UpdateSchedulingProfiles();

private:
	NetworkAspectType CombineAspects();
	bool DoSetAspectProfile(EEntityAspects aspect, uint8 profile, bool fromNetwork);

private:
	IEntity                          *m_pEntity;
	IGameObjectProfileManager        *m_pProfileManager;
	const SEntitySchedulingProfiles  *m_schedulingProfiles;
	std::vector<SRmiHandler>         m_rmiHandlers;

	NetworkAspectType m_enabledAspects;
	NetworkAspectType m_delegatableAspects;
	EntityId          m_cachedParentId;

	uint16            m_channelId;
	uint8             m_isBoundToNetwork : 1;
	uint8             m_hasAuthority     : 1;
	uint8             m_bNoSyncPhysics   : 1;
	uint8             m_profiles[NUM_ASPECTS];
};