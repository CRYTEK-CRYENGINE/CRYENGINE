// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Helper to allow AI to see objects
  
 -------------------------------------------------------------------------
  History:
  - 29:04:2010: Created by Kevin Kirst

*************************************************************************/

#ifndef __VISIBLEOBJECTSHELPER_H__
#define __VISIBLEOBJECTSHELPER_H__

#include <CryAISystem/IVisionMap.h>
#include <CryAISystem/VisionMapTypes.h>

class Agent;

typedef void (*TObjectVisibleFunc)(const Agent& agent, EntityId objectId, void *pArg);

// Visible-for rules
enum EVisibleObjectRule
{
	eVOR_Always			= 0x00,		// Object always remains visible when registered (default behavior)
	eVOR_UseMaxViewDist		= 0x01,		// Object is visible based on its max view distance from its render node
	eVOR_OnlyWhenMoving		= 0x02,		// Object is visible only when its not resting
	eVOR_TYPE_MASK			= 0xFF,

	eVOR_FlagNotifyOnSeen		= 0x100,	// Notify on object seen
	eVOR_FlagDropOnceInvisible	= 0x200,	// Drop the object (no longer visible) as soon as it becomes invisible based on its rule
	eVOR_FLAG_MASK			= 0xFF00,

	// Default rule
	eVOR_Default = (eVOR_Always)
};

class CVisibleObjectsHelper
{
public:
	CVisibleObjectsHelper();
	~CVisibleObjectsHelper();

	bool RegisterObject(EntityId objectId, int factionId = 0, uint32 visibleObjectRule = eVOR_Default, TObjectVisibleFunc pObjectVisibleFunc = NULL, void *pObjectVisibleFuncArg = NULL);
	bool RegisterObject(EntityId objectId, const ObservableParams &observableParams, uint32 visibleObjectRule = eVOR_Default, TObjectVisibleFunc pObjectVisibleFunc = NULL, void *pObjectVisibleFuncArg = NULL);
	bool UnregisterObject(EntityId objectId);

	bool SetObjectVisibleParams(EntityId objectId, const ObservableParams &observableParams);
	bool SetObjectVisibleRule(EntityId objectId, uint32 visibleObjectRule);
	bool SetObjectVisibleFunc(EntityId objectId, TObjectVisibleFunc pObjectVisibleFunc, void *pObjectVisibleFuncArg = NULL);

	bool IsObjectVisible(const Agent& agent, EntityId objectId) const;

	void Reset();
	void Update();

private:
	struct SVisibleObject
	{
		uint32 rule;
		float fLastActiveTime;
		EntityId entityId;
		VisionID visionId;
		TObjectVisibleFunc pFunc;
		void *pFuncArg;
		bool bIsObservable;

		SVisibleObject() : rule(eVOR_Default), fLastActiveTime(0.0f), entityId(0), pFunc(NULL), pFuncArg(NULL), bIsObservable(false) {}
	};

	void RegisterVisibility(SVisibleObject &visibleObject, const ObservableParams &observableParams) const;
	void UnregisterVisibility(SVisibleObject &visibleObject) const;
	bool IsObjectVisible(const Agent& agent, const SVisibleObject &visibleObject) const;
	void ClearAllObjects();

	bool CheckVisibilityRule(IEntity *pObject, SVisibleObject &visibleObject, float fCurrTime) const;
	bool CheckVisibilityRule_OnlyWhenMoving(IEntity *pObject, const SVisibleObject &visibleObject) const;

	bool CheckObjectViewDist(const Agent& agent, const SVisibleObject &visibleObject) const;

	typedef std::vector<SVisibleObject*> TActiveVisibleObjects;
	void CheckVisibilityToAI(const TActiveVisibleObjects &activeVisibleObjects, const Agent& agent) const;

	typedef std::map<EntityId, SVisibleObject> TVisibleObjects;
	TVisibleObjects m_VisibleObjects;
};

#endif //__VISIBLEOBJECTSHELPER_H__
