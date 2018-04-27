// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __INTERACTOR_H__
#define __INTERACTOR_H__

#pragma once

#include "IGameObject.h"
#include "IInteractor.h"

class CWorldQuery;
struct IEntitySystem;

class CInteractor : public CGameObjectExtensionHelper<CInteractor, IInteractor>
{
public:
	CInteractor();
	~CInteractor();

	// IGameObjectExtension
	virtual bool                 Init(IGameObject* pGameObject);
	virtual void                 InitClient(int channelId)                                                       {};
	virtual void                 PostInit(IGameObject* pGameObject);
	virtual void                 PostInitClient(int channelId)                                                   {};
	virtual bool                 ReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params);
	virtual void                 PostReloadExtension(IGameObject* pGameObject, const SEntitySpawnParams& params) {}
	virtual void                 Release();
	virtual void                 FullSerialize(TSerialize ser);
	virtual bool                 NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);
	virtual void                 PostSerialize();
	virtual void                 SerializeSpawnInfo(TSerialize ser) {}
	virtual ISerializableInfoPtr GetSpawnInfo()                     { return 0; }
	virtual void                 Update(SEntityUpdateContext& ctx, int slot);
	virtual void                 HandleEvent(const SGameObjectEvent&);
	virtual void                 ProcessEvent(const SEntityEvent&) {};
	virtual uint64               GetEventMask() const { return 0; }
	virtual void                 SetChannelId(uint16 id)     {};
	virtual void                 PostUpdate(float frameTime) { CRY_ASSERT(false); }
	virtual void                 PostRemoteSpawn()           {};
	virtual void                 GetMemoryUsage(ICrySizer* pSizer) const;
	// ~IGameObjectExtension

	// IInteractor
	virtual bool IsUsable(EntityId entityId) const;
	virtual bool IsLocked() const                { return m_lockEntityId != 0; };
	virtual int  GetLockIdx() const              { return m_lockIdx; };
	virtual int  GetLockedEntityId() const       { return m_lockEntityId; };
	virtual void SetQueryMethods(char* pMethods) { m_queryMethods = pMethods; };
	virtual int  GetOverEntityId() const         { return m_overId; };
	virtual int  GetOverSlotIdx() const          { return m_overIdx; };
	//~IInteractor

private:

	struct SQueryResult
	{
		SQueryResult() : entityId(0), slotIdx(0), hitPosition(0, 0, 0), hitDistance(0) {}
		EntityId entityId;
		int      slotIdx;
		Vec3     hitPosition;
		float    hitDistance;
	};

	typedef std::pair<IEntity*, SQueryResult> TQueryElement;
	typedef std::vector<TQueryElement>        TQueryVector;
	typedef std::map<IEntityClass*, bool>     TUsableClassesMap;

	CWorldQuery*      m_pQuery;

	float             m_useHoverTime;
	float             m_unUseHoverTime;
	float             m_messageHoverTime;
	float             m_longHoverTime;

	ITimer*           m_pTimer;
	IEntitySystem*    m_pEntitySystem;

	string            m_queryMethods;

	SmartScriptTable  m_pGameRules;
	HSCRIPTFUNCTION   m_funcIsUsable;
	HSCRIPTFUNCTION   m_funcOnNewUsable;
	HSCRIPTFUNCTION   m_funcOnUsableMessage;
	HSCRIPTFUNCTION   m_funcOnLongHover;

	SmartScriptTable  m_areUsableEntityList;
	HSCRIPTFUNCTION   m_funcAreUsable;
	TQueryVector      m_frameQueryVector;
	TUsableClassesMap m_usableEntityClasses;

	EntityId          m_lockedByEntityId;
	EntityId          m_lockEntityId;
	int               m_lockIdx;

	EntityId          m_nextOverId;
	int               m_nextOverIdx;
	CTimeValue        m_nextOverTime;
	EntityId          m_overId;
	int               m_overIdx;
	CTimeValue        m_overTime;
	bool              m_sentMessageHover;
	bool              m_sentLongHover;

	float             m_lastRadius;

	static ScriptAnyValue EntityIdToScript(EntityId id);
	void                  PerformQueries(EntityId& id, int& idx);
	void                  UpdateTimers(EntityId id, int idx);

	bool                  PerformRayCastQuery(SQueryResult& r);
	bool                  PerformViewCenterQuery(SQueryResult& r);
	bool                  PerformDotFilteredProximityQuery(SQueryResult& r, float minDot);
	bool                  PerformMergedQuery(SQueryResult& r, float minDot);
	bool                  PerformUsableTestAndCompleteIds(IEntity* pEntity, SQueryResult& r) const;
	int                   PerformUsableTest(IEntity* pEntity) const;
	void                  PerformUsableTestOnEntities(TQueryVector& query);
	float                 LinePointDistanceSqr(const Line& line, const Vec3& point);

	bool                  IsEntityUsable(const IEntity* pEntity);
};

#endif
