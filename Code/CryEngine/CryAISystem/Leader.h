// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   Leader.h
   $Id$
   Description: Header for the CLeader class

   -------------------------------------------------------------------------
   History:
   Created by Kirill Bulatsev
   - 01:06:2005   : serialization support added, related refactoring, moved out to separate files ObstacleAllocator, etc


 *********************************************************************/

#ifndef __Leader_H__
#define __Leader_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "AIActor.h"
#include "LeaderAction.h"
#include <CryEntitySystem/IEntity.h>
#include <list>
#include <map>
#include "ObjectContainer.h"

class CAIGroup;
class CLeader;
class CPipeUser;
class CLeaderAction;
struct LeaderActionParams;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct TTarget
{
	Vec3       position;
	CAIObject* object;
	TTarget(Vec3& spot, CAIObject* pObject) : position(spot), object(pObject) {};
};

class CLeader :
	public CAIActor//,public ILeader
{
	//	class CUnitAction;

	friend class CLeaderAction;

public:

	typedef std::map<CUnitImg*, Vec3>               CUnitPointMap;
	typedef std::vector<CLeader*>                   TVecLeaders;
	typedef std::vector<CAIObject*>                 TVecTargets;
	typedef std::list<int>                          TUsableObjectList;
	typedef std::multimap<int, TUnitList::iterator> TClassUnitMap;
	typedef std::map<INT_PTR, Vec3>                 TSpotList;

	CLeader(int iGroupID);
	~CLeader(void);

	////////////////////////////////////////////////////////////////////////////////////////////////
	// SYSTEM/AI SYSTEM RELATED FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////

	void         Update(EUpdateType type);
	void         Reset(EObjectResetType type);
	void         OnObjectRemoved(CAIObject* pObject);
	void         Serialize(TSerialize ser);

	void         SetAIGroup(CAIGroup* pGroup) { m_pGroup = pGroup; }
	CAIGroup*    GetAIGroup()                 { return m_pGroup; }
	virtual void SetAssociation(CWeakRef<CAIObject> refAssociation);

	////////////////////////////////////////////////////////////////////////////////////////////////
	// CLEADER PROPERTIES FUNCTION
	////////////////////////////////////////////////////////////////////////////////////////////////

	// <Title IsPlayer>
	// Description: returns true if this is associated to the player AI object
	bool IsPlayer() const;

	// <Title IsIdle>
	// Description: returns true if there's no LeaderAction active
	bool         IsIdle() const;

	virtual bool IsActive() const { return false; }
	virtual bool IsAgent() const  { return false; }

	////////////////////////////////////////////////////////////////////////////////////////////////
	// TEAM MANAGEMENT FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////

	// <Title GetActiveUnitPlanCount>
	// Description: return the number of units with at least one of the specified properties, which have an active plan (i.e. not idle)
	// Arguments:
	//	uint32 unitPropMask - binary mask of properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//		only the unit with any of the specified properties will be considered
	//		(default = UPR_ALL : all units are checked)
	int GetActiveUnitPlanCount(uint32 unitProp = UPR_ALL) const;

	// <Title DeadUnitNotify>
	// Description: to be called when a unit in the group dies
	// Arguments:
	// CAIObject* pObj = Dying AIObject
	void DeadUnitNotify(CAIActor* pAIObj);

	// <Title AddUnitNotify>
	// Description: to be called when an AI is added to the leader's group
	// Arguments:
	// CAIObject* pObj = new AIObject joining group
	inline void AddUnitNotify(CAIActor* pAIObj)
	{
		if (m_pCurrentAction)
			m_pCurrentAction->AddUnitNotify(pAIObj);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////
	// POSITIONING/TACTICAL FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////

	// <Title GetPreferedPos>
	// Description: Returns the preferred start position for finding obstacles (Dejan?)
	Vec3 GetPreferedPos() const;

	// <Title ForcePreferedPos>
	// Description:
	inline void ForcePreferedPos(const Vec3& pos) { m_vForcedPreferredPos = pos; };

	// <Title MarkPosition>
	// Description: marks a given point to be retrieved later
	// Arguments:
	//	Vec3& pos - point to be marked
	inline void MarkPosition(const Vec3& pos) { m_vMarkedPosition = pos; }

	// <Title GetMarkedPosition>
	// Description: return the previously marked point
	inline const Vec3& GetMarkedPosition() const { return m_vMarkedPosition; }

	// <Title AssignTargetToUnits>
	// Description: redistribute known attention targets amongst the units with the specified properties
	// Arguments:
	//	uint32 unitProp = binary selection mask for unit properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//		only the unit with any of the specified properties will be considered
	//	int maxUnitsPerTarget = specifies the maximum number of units that can have the same attention target
	void AssignTargetToUnits(uint32 unitProp, int maxUnitsPerTarget);

	// <Title GetEnemyLeader>
	// Description: returns the leader puppet (CAIOject*) of the current Leader's attention target if there is
	//	(see CLeader::GetAttentionTarget()
	CAIObject* GetEnemyLeader() const;

	// <Title ShootSpotsAvailable>
	// Description: returns true if there are active shoot spots available
	inline bool ShootSpotAvailable() { return false; }

	////////////////////////////////////////////////////////////////////////////////////////////////
	// ACTION RELATED FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////

	// <Title Attack>
	// Description: starts an attack action (stopping the current one)
	// Arguments:
	//  LeaderActionParams* pParams: leader action parameters - used depending on the action subtype (default: NULL -> default attack)
	//	pParams->subType - leader Action subtype (LAS_ATTACK_FRONT,LAS_ATTACK_ROW etc) see definition of ELeaderActionSubType
	//  pParams->fDuration - maximum duration of the attack action
	//  pParams->unitProperties -  binary selection mask for unit properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//		only the unit with any of the specified properties will be selected for the action
	void Attack(const LeaderActionParams* pParams = NULL);

	// <Title Search>
	// Description: starts a search action (stopping the current one)
	// Arguments:
	//	const Vec3& targetPos - last known target position reference
	//	float distance - maximum distance to search (not working yet - it's hardcoded in AI System)
	//	uint32 unitProperties = binary selection mask for unit properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//		only the unit with any of the specified properties will be selected for the action
	void Search(const Vec3& targetPos, float distance, uint32 unitProperties, int searchSpotAIObjectType);

	// <Title AbortExecution>
	// Description: clear all units' actions and cancel (delete) the current LeaderAction
	// Arguments:
	//	int priority (default 0) max priority value for the unit actions to be canceled (0 = all priorities)
	//		if an unit action was created with a priority value higher than this value, it won't be canceled
	void AbortExecution(int priority = 0);

	// <Title ClearAllPlannings>
	// Description: clear all actions of all the unit with specified unit properties
	// Arguments:
	//	int priority (default 0) max priority value for the unit actions to be canceled (0 = all priorities)
	//		if an unit action was created with a priority value higher than this value, it won't be canceled
	//	uint32 unitProp = binary selection mask for unit properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//		only the unit with any of the specified properties will be considered
	void ClearAllPlannings(uint32 unitProp = UPR_ALL, int priority = 0);

	////////////////////////////////////////////////////////////////////////////////////////////////
	// FORMATION RELATED FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////

	bool                       LeaderCreateFormation(const char* szFormationName, const Vec3& vTargetPos, bool bIncludeLeader = true, uint32 unitProp = UPR_ALL, CAIObject* pOwner = NULL);
	bool                       ReleaseFormation();

	CWeakRef<CAIObject>        GetNewFormationPoint(CWeakRef<CAIObject> refRequester, int iPointType = 0);
	CWeakRef<CAIObject>        GetFormationOwner(CWeakRef<CAIObject> refRequester) const { return m_refFormationOwner;  }
	CWeakRef<CAIObject>        GetFormationPoint(CWeakRef<CAIObject> refRequester) const;

	int                        GetFormationPointIndex(const CAIObject* pAIObject) const;
	CWeakRef<CAIObject>        GetFormationPointSight(const CPipeUser* pRequester) const;
	void                       FreeFormationPoint(CWeakRef<CAIObject> refRequester) const;

	inline CWeakRef<CAIObject> GetFormationOwner() const      { return m_refFormationOwner; }
	inline CWeakRef<CAIObject> GetPrevFormationOwner() const  { return m_refPrevFormationOwner; }

	inline const char*         GetFormationDescriptor() const { return m_szFormationDescriptor.c_str(); }
	int                        AssignFormationPoints(bool bIncludeLeader = true, uint32 unitProp = UPR_ALL);
	bool                       AssignFormationPoint(CUnitImg& unit, int iTeamSize = 0, bool bAnyClass = false, bool bClosestToOwner = false);
	void                       AssignFormationPointIndex(CUnitImg& unit, int index);

	// Called by CAIGroup.
	void OnEnemyStatsUpdated(const Vec3& avgEnemyPos, const Vec3& oldAvgEnemyPos);

protected:
	void           UpdateEnemyStats();
	void           ProcessSignal(AISIGNAL& signal);
	CLeaderAction* CreateAction(const LeaderActionParams* params, const char* signalText = NULL);

private:
	void ChangeStance(EStance stance);

protected:
	CLeaderAction*      m_pCurrentAction;

	CWeakRef<CAIObject> m_refFormationOwner;
	CWeakRef<CAIObject> m_refPrevFormationOwner;
	CWeakRef<CAIObject> m_refEnemyTarget;

	string              m_szFormationDescriptor;
	Vec3                m_vForcedPreferredPos;
	Vec3                m_vMarkedPosition; //general purpose memory of a position - used in CLeaderAction_Search, as
	// last average units' position when there was a live target
	bool                m_bLeaderAlive;
	bool                m_bKeepEnabled; // if true, CLeader::Update() will be executed even if the leader puppet is dead

	CAIGroup*           m_pGroup;
};

inline const CLeader* CastToCLeaderSafe(const IAIObject* pAI) { return pAI ? pAI->CastToCLeader() : 0; }
inline CLeader*       CastToCLeaderSafe(IAIObject* pAI)       { return pAI ? pAI->CastToCLeader() : 0; }

#endif // __Leader_H__
