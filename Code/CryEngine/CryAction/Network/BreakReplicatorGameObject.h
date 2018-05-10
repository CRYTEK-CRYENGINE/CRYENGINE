// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BREAK_REP_GAME_OBJECT__H__
#define __BREAK_REP_GAME_OBJECT__H__

#include "ActionGame.h"

/*
   =====================================================================
   class: CBreakRepGameObject
   =====================================================================
 */
class CBreakRepGameObject : public CGameObjectExtensionHelper<CBreakRepGameObject, IGameObjectExtension>
{
public:
	virtual inline bool                 Init(IGameObject* pGameObject);
	virtual inline void                 InitClient(int channelId)                                                       {}
	virtual inline void                 PostInit(IGameObject*)                                                          {}
	virtual inline void                 PostInitClient(int channelId)                                                   {}
	virtual inline bool                 ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params);
	virtual inline void                 PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) {}
	virtual inline void                 Release()                                                                       { delete this; }

	virtual inline void                 FullSerialize(TSerialize ser)                                                   {}
	virtual inline bool                 NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual inline void                 PostSerialize()                                                                 {}
	virtual inline void                 SerializeSpawnInfo(TSerialize ser)                                              {}
	virtual inline ISerializableInfoPtr GetSpawnInfo()                                                                  { return NULL; }
	virtual inline void                 Update(SEntityUpdateContext& ctx, int)                                          {}
	virtual inline void                 HandleEvent(const SGameObjectEvent& event)                                      {}
	virtual inline void                 ProcessEvent(const SEntityEvent& event)                                               {}
	virtual uint64                      GetEventMask() const final { return 0; };
	virtual inline void                 SetChannelId(uint16 id)                                                         {}
	virtual inline void                 PostUpdate(float frameTime)                                                     {}
	virtual inline void                 PostRemoteSpawn()                                                               {}
	virtual inline void                 GetMemoryUsage(ICrySizer* s) const                                              { s->Add(*this); }

public:
	bool m_wasRemoved;
	bool m_removed;
};

inline bool CBreakRepGameObject::Init(IGameObject* pGameObject)
{
	SetGameObject(pGameObject);
	m_removed = false;
	m_wasRemoved = false;
	return true;
}

inline bool CBreakRepGameObject::ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params)
{
	ResetGameObject();
	CRY_ASSERT_MESSAGE(false, "CBreakRepGameObject::ReloadExtension not implemented");
	return false;
}

inline bool CBreakRepGameObject::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	ser.Value("removed", m_removed);

	if (m_removed && m_wasRemoved == false)
	{
		m_wasRemoved = true;
		CActionGame::Get()->FreeBrokenMeshesForEntity(GetGameObject()->GetEntity()->GetPhysics());
	}
	return true;
}

#endif
