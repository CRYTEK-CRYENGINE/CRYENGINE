// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   CentralInterestManager.cpp
   $Id$
   $DateTime$
   Description: Global manager that distributes interest events and
             allows personal managers to work together
             Effectively, this is the interface to the Interest System
   ---------------------------------------------------------------------
   History:
   - 06:03:2007 : Created by Matthew Jack

 *********************************************************************/

#ifndef __CentralInterestManager_H__
#define __CentralInterestManager_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "PersonalInterestManager.h"
#include <CryMemory/STLGlobalAllocator.h>

// Forward declarations
struct IPersistantDebug;

// Basic structure for storing which objects are interesting and how much
struct SEntityInterest
{
	SEntityInterest() :
		m_entityId(0), m_fRadius(0.f), m_fInterest(0.f), m_vOffset(ZERO), m_fPause(0.f), m_nbShared(0), m_eSupportedActorClasses(0)
	{
	}

	SEntityInterest(EntityId entityId, float fRadius, float fInterest, const char* szActionName, const Vec3& vOffset, float fPause, int nbShared)
		: m_entityId(0), m_fRadius(0.f), m_fInterest(0.f), m_vOffset(ZERO), m_fPause(0.f), m_nbShared(0), m_eSupportedActorClasses(0)
	{
		Set(entityId, fRadius, fInterest, szActionName, vOffset, fPause, nbShared);
	}

	~SEntityInterest()
	{
		Invalidate();
	}

	bool Set(EntityId entityId, float fRadius, float fInterest, const char* szActionName, const Vec3& vOffset, float fPause, int nbShared);

	bool Set(const SEntityInterest& rhs)
	{
		return Set(rhs.m_entityId, rhs.m_fRadius, rhs.m_fInterest, rhs.m_sActionName.c_str(), rhs.m_vOffset, rhs.m_fPause, rhs.m_nbShared);
	}

	const char*    GetAction() const   { return m_sActionName.empty() ? "None" : m_sActionName.c_str(); }
	void           SetAction(const char* szActionName);
	const IEntity* GetEntity() const   { return gEnv->pEntitySystem->GetEntity(m_entityId); }
	IEntity*       GetEntity()         { return gEnv->pEntitySystem->GetEntity(m_entityId); }
	EntityId       GetEntityId() const { return m_entityId; }
	bool           SupportsActorClass(const char* szActorClass) const;
	bool           IsValid() const     { return m_entityId > 0; }

	void           Invalidate()
	{
		IEntity* pEntity = GetEntity();
		if (pEntity && gAIEnv.pSmartObjectManager)
		{
			gAIEnv.pSmartObjectManager->RemoveSmartObjectState(pEntity, "Registered");
		}
		m_entityId = 0;
		m_sActionName = string();
	}

	void Serialize(TSerialize ser)
	{
		ser.Value("entityId", m_entityId);
		ser.Value("fRadius", m_fRadius);
		ser.Value("fInterest", m_fInterest);
		ser.Value("sActionName", m_sActionName);
		ser.Value("vOffset", m_vOffset);
		ser.Value("fPause", m_fPause);
		ser.Value("nbShared", m_nbShared);
		ser.Value("m_eSupportedActorClasses", m_eSupportedActorClasses);
	}

	EntityId m_entityId;
	float    m_fRadius;
	float    m_fInterest;
	string   m_sActionName;
	Vec3     m_vOffset;
	float    m_fPause;
	int      m_nbShared;

	enum
	{
		eACTOR_CLASS_HUMAN_GRUNT   = 1 << 0,
		eACTOR_CLASS_ALIEN_GRUNT   = 1 << 1,
		eACTOR_CLASS_ALIEN_STALKER = 1 << 2,
	};

	int m_eSupportedActorClasses;
};

// There need only ever be one of these in game, hence a singleton
// Created on first request, which is lightweight

class CCentralInterestManager :
	public ICentralInterestManager,
	public IEntitySystemSink,
	public IEntityEventListener
{
public:
	// Get the CIM singleton
	static CCentralInterestManager* GetInstance();

	// Reset the CIM system
	// Do this when all caches should be emptied, e.g. on loading levels
	// Resetting during game should do no serious harm
	virtual void Reset();

	// Enable or disable (suppress) the interest system
	virtual bool Enable(bool bEnable);

	// Is the Interest System enabled?
	virtual bool IsEnabled()          { return m_bEnabled; }

	bool         IsDebuggingEnabled() { return m_cvDebugInterest ? (m_cvDebugInterest->GetIVal() != 0) : false; }

	// Update the CIM
	virtual void Update(float fDelta);

	void         Serialize(TSerialize ser);

	// Deregister an interesting entity
	virtual void DeregisterInterestingEntity(IEntity* pEntity);

	virtual void ChangeInterestingEntityProperties(IEntity* pEntity, float fRadius = -1.f, float fBaseInterest = -1.f, const char* szActionName = NULL, const Vec3& vOffset = Vec3Constants<float>::fVec3_Zero, float fPause = -1.f, int nbShared = -1);
	virtual void ChangeInterestedAIActorProperties(IEntity* pEntity, float fInterestFilter = -1.f, float fAngleCos = -1.f);

	// Deregister a potential interested AI Actor
	virtual bool DeregisterInterestedAIActor(IEntity* pEntity);

	// Central shared debugging function, should really be a private/friend of PIM
	void AddDebugTag(EntityId id, const char* szString, float fTime = -1.f);

	// Add a class to be notified when an entity is interested or interesting
	virtual void RegisterListener(IInterestListener* pInterestListener, EntityId idInterestingEntity);
	virtual void UnRegisterListener(IInterestListener* pInterestListener, EntityId idInterestingEntity);

	//-------------------------------------------------------------
	// IEntitySystemSink methods, for listening to moving entities
	//-------------------------------------------------------------
	virtual bool OnBeforeSpawn(SEntitySpawnParams&) { return true; }
	virtual void OnSpawn(IEntity* pEntity, SEntitySpawnParams&);
	virtual bool OnRemove(IEntity* pEntity);
	virtual void OnReused(IEntity* pEntity, SEntitySpawnParams& entitySpawnParams) {}

	// IEntityEventListener
	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event);
	// End of IEntityEventListener

	// Expose for DebugDraw
	typedef std::deque<CPersonalInterestManager, stl::STLGlobalAllocator<CPersonalInterestManager>> TVecPIMs;
	const TVecPIMs* GetPIMs() const { return &m_PIMs; }

	// Expose for PIMs
	typedef std::vector<SEntityInterest, stl::STLGlobalAllocator<SEntityInterest>> TVecInteresting;
	TVecInteresting*          GetInterestingEntities()       { return &m_InterestingEntities; }
	const TVecInteresting*    GetInterestingEntities() const { return &m_InterestingEntities; }

	CPersonalInterestManager* FindPIM(IEntity* pEntity);

protected:
	friend class CPersonalInterestManager;

	void OnInterestEvent(IInterestListener::EInterestEvent eInterestEvent, EntityId idActor, EntityId idInterestingEntity);
	bool CanCastRays() { return m_cvCastRays ? (m_cvCastRays->GetIVal()) != 0 : false; }

private:
	// Construct/destruct
	CCentralInterestManager();
	~CCentralInterestManager();

	bool RegisterInterestingEntity(IEntity* pEntity, float fRadius = -1.f, float fBaseInterest = -1.f, const char* szActionName = NULL, const Vec3& vOffset = Vec3Constants<float>::fVec3_Zero, float fPause = -1.f, int nbShared = -1);
	bool RegisterInterestedAIActor(IEntity* pEntity, bool bEnablePIM, float fInterestFilter = -1.f, float fAngleCos = -1.f);

	void RegisterObject(IEntity* pEntity);
	void DeregisterObject(IEntity* pEntity);

	void RegisterFromTable(IEntity* pEntity, const SmartScriptTable& ssTable);
	void RegisterEntityFromTable(IEntity* pEntity, const SmartScriptTable& ssTable);
	void RegisterActorFromTable(IEntity* pEntity, const SmartScriptTable& ssTable);

	bool GatherData(IEntity* pEntity, SActorInterestSettings& actorInterestSettings);
	bool GatherData(IEntity* pEntity, SEntityInterest& entityInterest);

	bool ReadDataFromTable(const SmartScriptTable& ssTable, SEntityInterest& entityInterest);
	bool ReadDataFromTable(const SmartScriptTable& ssTable, SActorInterestSettings& actorInterestSettings);

	void Init();                                        // Initialise as defaults

private:
	// Basic
	static CCentralInterestManager* m_pCIMInstance;     // The singleton
	bool                            m_bEnabled;         // Toggle Interest system on/off
	bool                            m_bEntityEventListenerInstalled;

	typedef std::multimap<EntityId, IInterestListener*> TMapListeners;

	// The tracking lists
	TVecPIMs        m_PIMs;                   // Instantiated PIMs, no NULL, some can be unassigned
	TVecInteresting m_InterestingEntities;    // Interesting objects we might consider pointing out to PIMs
	TMapListeners   m_Listeners;              // Listeners to be notified when an entity is interested OR interesting

	// Performance
	uint32 m_lastUpdated;
	float  m_fUpdateTime;

	// Debug
	ICVar*            m_cvDebugInterest;
	ICVar*            m_cvCastRays;
	IPersistantDebug* m_pPersistentDebug;     // The persistent debugging framework
};

#endif // __CentralInterestManager_H__
