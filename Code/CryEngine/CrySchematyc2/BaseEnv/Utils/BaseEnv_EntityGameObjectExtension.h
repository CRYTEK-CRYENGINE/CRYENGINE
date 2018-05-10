// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseEnv/BaseEnv_Prerequisites.h"

#include <IGameObject.h>

namespace SchematycBaseEnv
{
class CEntityGameObjectExtension : public Schematyc2::INetworkObject, public CGameObjectExtensionHelper<CEntityGameObjectExtension, IGameObjectExtension>
{
public:

	CEntityGameObjectExtension();

	virtual ~CEntityGameObjectExtension();

	// Schematyc2::INetworkObject
	virtual bool   RegisterNetworkSerializer(const Schematyc2::NetworkSerializeCallbackConnection& callbackConnection, int32 aspects) override;
	virtual void   MarkAspectsDirty(int32 aspects) override;
	virtual void   SetAspectDelegation(int32 aspects, const EAspectDelegation delegation) override;
	virtual uint16 GetChannelId() const override;
	virtual bool   ServerAuthority() const override;
	virtual bool   ClientAuthority() const override;
	virtual bool   LocalAuthority() const override;
	// ~Schematyc2::INetworkObject

	// IGameObjectExtension
	virtual bool                 Init(IGameObject* pGameObject) override;
	virtual void                 InitClient(int channelId) override;
	virtual void                 PostInit(IGameObject* pGameObject) override;
	virtual void                 PostInitClient(int channelId) override;
	virtual bool                 ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& spawnParams) override;
	virtual void                 PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& spawnParams) override;

	virtual void                 FullSerialize(TSerialize serialize) override;
	virtual bool                 NetSerialize(TSerialize serialize, EEntityAspects aspects, uint8 profile, int flags) override;
	virtual void                 PostSerialize() override;
	virtual void                 SerializeSpawnInfo(TSerialize serialize) override;
	virtual ISerializableInfoPtr GetSpawnInfo() override;
	virtual void                 Update(SEntityUpdateContext& context, int updateSlot) override;
	virtual void                 PostUpdate(float frameTime) override;
	virtual void                 PostRemoteSpawn() override;
	virtual void                 ProcessEvent(const SEntityEvent& event) override;
	virtual uint64               GetEventMask() const override;
	virtual void                 HandleEvent(const SGameObjectEvent& event) override;
	virtual void                 SetChannelId(uint16 channelId) override;
	virtual void                 GetMemoryUsage(ICrySizer* pSizer) const override;
	// ~IGameObjectExtension

	static const char* s_szExtensionName;

private:

	enum class EObjectState
	{
		Uninitialized,
		Initialized,
		Running
	};

	void CreateObject(bool bEnteringGame);
	void DestroyObject();
	void RunObject();

	Schematyc2::SGUID           m_classGUID;
	Schematyc2::ESimulationMode m_simulationMode;
	EObjectState                m_objectState;
	Schematyc2::IObject*        m_pObject;
};
}
