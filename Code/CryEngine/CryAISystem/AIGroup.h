// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIGroup.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - ?
   - 2 Mar 2009	: Evgeny Adamenkov: Removed IRenderer

 *********************************************************************/
#ifndef _AIGROUP_H_
#define _AIGROUP_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IAISystem.h>
#include "UnitImg.h"
#include "Leader.h"
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAIGroup.h>
#include <CryCore/StlUtils.h>
#include "Adapters.h"

struct IAIGroupTactic;
class CLeader;
typedef uint32 GroupScopeID;

//====================================================================
// CAIGroup
//====================================================================
class CAIGroup
	: public CAIGroupAdapter
{
public:

	typedef std::vector<CWeakRef<const CAIObject>> TSetAIObjects;

	CAIGroup(int groupID);
	~CAIGroup();

	// Inherited IAIGroup
	virtual int  GetGroupId() { return m_groupID; }
	virtual int  GetGroupCount(int flags);
	virtual void NotifyReinfDone(const IAIObject* obj, bool isDone);

	////////////////////////////////////////////////////////////////////////////////////////////////
	// TEAM MANAGEMENT FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////

	void     SetLeader(CLeader* pLeader);
	CLeader* GetLeader();
	//CLeader*		MakeMeLeader(IAIObject* pAIObject);

	// <Title AddMember>
	// Description: Add a new AIObject to the group
	// Arguments:
	//	CAIObject* pMember - new AIObject to add
	void AddMember(CAIActor* pMember);

	// <Title RemoveMember>
	// Description: Removes a member from the group
	// Arguments:
	//	CAIObject* pMember - AIObject to remove ; no members will be removed if pMember is not in the team
	bool RemoveMember(CAIActor* pMember);

	// <Title IsMember>
	// Description: returns true if the given AIObject is in the group
	// Arguments:
	// CAIObject* pMember - AIObject to retrieve
	bool IsMember(const IAIObject* pMember) const;

	// <Title GetUnits>
	// Description: returns the unit (team members) list container
	TUnitList& GetUnits() { return m_Units; }

	// <Title GetUnitImg>
	// Description: returns the unit image of the given AIObject, if it's found in the team
	// Arguments:
	//	CAIObject* pAIObject - AIObject to retrieve
	TUnitList::iterator GetUnit(const CAIActor* pAIObject);

	// <Title SetUnitProperties>
	// Description: set the given unit's properties (UPR_*)
	// See definition of EUnitProperties in IAgent.h for a complete list of unit properties
	// Arguments:
	//	IAIObject* pMember - AIObject to set properties (if it's found in the team)
	//	uint32 unitPropMask - binary mask of properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	void SetUnitProperties(const IAIObject* pMember, uint32 unitPropMask);

	// <Title GetUnitCount>
	// Description: returns the number of units which have any of the specified properties
	// Arguments:
	//	uint32 unitPropMask - binary mask of properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//			only the unit with any of the specified properties will be considered
	//		(default = UPR_ALL : returns the total number of units)
	int GetUnitCount(uint32 unitPropMask = UPR_ALL) const;

	////////////////////////////////////////////////////////////////////////////////////////////////
	// POSITIONING/TACTICAL FUNCTIONS
	////////////////////////////////////////////////////////////////////////////////////////////////

	// <Title GetAttentionTarget>
	// Description: return the IAIObject* most threatening attention target among all the units
	// Arguments:
	//	(optional) bool bHostileOnly - if true, only hostile targets will be considered (default = false)
	//	(optional) bool bLiveOnly - if true, only live targets (=no dummy) will be considered (default = false)

	// Suppressed passedByValue warnings for smart pointers like CWeakRef<>
	// cppcheck-suppress passedByValue
	CWeakRef<CAIObject> GetAttentionTarget(bool bHostileOnly = false, bool bLiveOnly = false, const CWeakRef<CAIObject> refSkipTarget = NILREF) const;

	// <Title GetTargetCount>
	// Description: return the number of targets amongst all units
	// Arguments:
	//	(optional) bool bHostileOnly - if true, only hostile targets will be considered (default = false)
	//	(optional) bool bLiveOnly - if true, only live targets (=no dummy) will be considered (default = false)
	int GetTargetCount(bool bHostileOnly = false, bool bLiveOnly = false) const;

	// <Title GetUnits>
	// Description: returns the unit (team members) list container
	TSetAIObjects& GetTargets() { return m_Targets; }

	// <Title GetAveragePosition>
	// Description: return average position of the units in the team
	// Arguments:
	//	(optional) eAvPositionMode mode - selects the unit variable to be checked for selection:
	//				AVMODE_ANY - all unit will be processed
	//				AVMODE_PROPERTIES - unit properties will be used for filter
	//				AVMODE_CLASS - unit class will be used for filter
	//	(optional) uint32 unitFilter - filter value (UPR_* if properties, UNIT_CLASS_* if class)
	Vec3 GetAveragePosition(eAvPositionMode mode = AVMODE_ANY, uint32 unitClassOrProperties = -1) const;

	// <Title GetForemostUnit>
	// gets the foremost unit in the given direction. starting from the average position
	// Arguments:
	//	Vec3 direction - direction to use
	//	uint32 unitProp = binary selection mask for unit properties (i.e. UPR_COMBAT_GROUND|UPR_COMBAT_RECON)
	//		only the unit with any of the specified properties will be considered
	CAIObject* GetForemostUnit(const Vec3& direction, uint32 unitProp) const;

	// <Title GetEnemyAveragePosition>
	// Description: return the average position of all the known enemy targets
	inline const Vec3& GetEnemyAveragePosition() const { return m_vAverageEnemyPosition; }

	// <Title GetLiveEnemyAveragePosition>
	// Description: return the average position of all the known enemy live targets
	inline const Vec3& GetLiveEnemyAveragePosition() const { return m_vAverageLiveEnemyPosition; }

	// <Title GetEnemyAverageDirection>
	// Description: return the average direction of all the known enemy targets
	// Arguments:
	//	 bool bLiveOnly - if true, only live targets (=no dummy) will be considered
	//	 bool bSmart - if true, average direction will be set to ZERO if the enemy targets' directions are too different from
	//						each other (high variance)
	Vec3 GetEnemyAverageDirection(bool bLiveOnly, bool bSmart) const;

	// <Title GetForemostEnemy>
	// Description: return the enemy which is foremost in the given direction, starting from enemy average position
	// Arguments:
	//	 Vec3 direction - direction to use
	//	 bool bLiveOnly - if true, only live (non dummy) targets are considered
	const CAIObject* GetForemostEnemy(const Vec3& direction, bool bLiveOnly = false) const;

	// <Title GetForemostEnemyPosition>
	// Description: return the position of the enemy which is foremost in the given direction, starting from enemy average position
	// Arguments:
	//	 Vec3 direction - direction to use
	//	 bool bLiveOnly - if true, only live (non dummy) targets are considered
	Vec3 GetForemostEnemyPosition(const Vec3& direction, bool bLiveOnly = false) const;

	// <Title GetEnemyPositionKnown>
	// Description: returns the attention target position of a unit (if specified in the parameter),
	//	or - if unit is not specified - .. Dejan? (not clear what's happening with the distance)
	Vec3 GetEnemyPosition(const CAIObject* pUnit = NULL) const;

	// <Title GetEnemyPositionKnown>
	// Description: returns an alive, hostile attention target amongst the units' - if there is - or the beacon if there's not
	//	(note: seems replaceable by either GetEnemyLiveAveragePosition() or GetForemostEnemyPosition())
	Vec3 GetEnemyPositionKnown() const;

	// Returns true if the scope could have been entered (If no group member already entered it),
	// false otherwise.
	// If the memberID doesn't belong to this group then false is returned
	bool EnterGroupScope(const CAIActor* pMember, const GroupScopeID scopeID, const uint32 allowedConcurrentUsers);
	// The group scope is released by the memberID. If the memberID is not in the scope
	// the function doesn't do anything.
	void                LeaveGroupScope(const CAIActor* pMember, const GroupScopeID scopeID);
	void                LeaveAllGroupScopesForActor(const CAIActor* pMember);
	uint32              GetAmountOfActorsInScope(const GroupScopeID scopeID) const;
	static GroupScopeID GetGroupScopeId(const char* scopeName);

	void                NotifyBodyChecked(const CAIObject* pObject);
	void                OnObjectRemoved(CAIObject* pObject);
	void                UpdateGroupCountStatus();
	void                Update();
	void                Reset();
	void                Serialize(TSerialize ser);
	void                DebugDraw();

private:

	void UpdateReinforcementLogic();

	void SetUnitClass(CUnitImg& unit) const;

	int           m_groupID;          // the group id.
	int           m_count;            // Current group count.
	int           m_countMax;         // Max number of entities in the group.
	int           m_countEnabled;     // Current enabled group count.
	int           m_countEnabledMax;  // Max number of enabled entities in the group.
	int           m_countDead;        // Number of dead entities.
	int           m_countCheckedDead; // Number of dead bodies seen.

	TSetAIObjects m_Targets;
	Vec3          m_vEnemyPositionKnown;
	Vec3          m_vAverageEnemyPosition;
	Vec3          m_vAverageLiveEnemyPosition;
	bool          m_bUpdateEnemyStats;

	TUnitList     m_Units;
	CLeader*      m_pLeader;

	typedef std::set<tAIObjectID>                  MembersInScope;
	typedef std::map<GroupScopeID, MembersInScope> GroupScopes;
	GroupScopes m_groupScopes;

	enum EReinforcementState
	{
		REINF_NONE,
		//		REINF_ALERTED_COMBAT_PENDING,
		//		REINF_ALERTED_COMBAT,
		REINF_DONE_PENDING,
		REINF_DONE,
		LAST_REINF,
	};

	struct SAIReinforcementSpot
	{
		CAIObject* pAI;
		int        type;
		bool       whenAllAlerted;
		bool       whenInCombat;
		int        groupBodyCount;
		float      avoidWhenTargetInRadius;
	};

	struct SAIRefinforcementCallDebug
	{
		SAIRefinforcementCallDebug(const Vec3& from, const Vec3& to, float t, const char* perf) :
			from(from), to(to), t(t), performer(perf) {}
		Vec3   from, to;
		string performer;
		float  t;
	};

	int                               m_reinforcementSpotsAllAlerted;
	int                               m_reinforcementSpotsCombat;
	int                               m_reinforcementSpotsBodyCount;

	bool                              m_pendingDecSpotsAllAlerted;
	bool                              m_pendingDecSpotsCombat;
	bool                              m_pendingDecSpotsBodyCount;

	CWeakRef<CPuppet>                 m_refReinfPendingUnit;

	std::vector<SAIReinforcementSpot> m_reinforcementSpots;

#ifdef CRYAISYSTEM_DEBUG
	std::vector<SAIRefinforcementCallDebug> m_DEBUG_reinforcementCalls;
#endif

	bool                m_reinforcementSpotsAcquired;
	EReinforcementState m_reinforcementState;
};

#endif
