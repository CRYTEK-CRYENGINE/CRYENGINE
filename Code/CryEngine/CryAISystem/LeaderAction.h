// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   LeaderAction.h

   Description: Header for CLeaderAction, CUnitImg, classes

   -------------------------------------------------------------------------
   History:
   - 15:11:2004   17:41 : Created by Luciano Morpurgo

 *********************************************************************/

#ifndef __LeaderAction_H__
#define __LeaderAction_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IAgent.h>
#include "AIObject.h"
#include "UnitAction.h"
#include "UnitImg.h"
#include <CrySystem/TimeValue.h>
#include <CryMath/Cry_Math.h>
#include <list>

class CLeader;

enum  ELeaderActionSubType
{
	LAS_DEFAULT = 0,
	LAS_ATTACK_ROW,
	LAS_ATTACK_CIRCLE,
	LAS_ATTACK_FLANK,
	LAS_ATTACK_FLANK_HIDE,
	LAS_ATTACK_FOLLOW_LEADER,
	LAS_ATTACK_FUNNEL,
	LAS_ATTACK_LEAPFROG,
	LAS_ATTACK_HIDE_COVER,
	LAS_ATTACK_FRONT,
	LAS_ATTACK_CHAIN,
	LAS_ATTACK_COORDINATED_FIRE1,
	LAS_ATTACK_COORDINATED_FIRE2,
	LAS_ATTACK_USE_SPOTS,
	LAS_ATTACK_HIDE,
	LAS_ATTACK_SWITCH_POSITIONS,
	LAS_ATTACK_CHASE,
	LAS_SEARCH_DEFAULT,
	LAS_SEARCH_COVER,
	LAS_LAST  // Make sure this one is always the last!
};

struct LeaderActionParams
{
	ELeaderAction        type;
	ELeaderActionSubType subType;
	float                fDuration;
	string               name;
	Vec3                 vPoint;
	Vec3                 vPoint2;
	float                fSize;
	int                  iValue;
	uint32               unitProperties;
	CAIObject*           pAIObject;
	ScriptHandle         id;

	LeaderActionParams(ELeaderAction t, ELeaderActionSubType s) : type(t), subType(s), fDuration(0), vPoint(ZERO), fSize(0), vPoint2(ZERO), pAIObject(NULL), iValue(0)  {};
	LeaderActionParams() : type(LA_LAST), subType(LAS_DEFAULT), fDuration(0), vPoint(ZERO), fSize(0), vPoint2(ZERO), pAIObject(NULL), iValue(0) {};

};

class CLeaderAction
{
protected:
	typedef std::list<std::pair<CUnitImg*, CUnitAction*>> ActionList;
	struct BlockActionList
	{
		BlockActionList(ActionList& blockers, ActionList& blocked)
		{
			ActionList::iterator i, j, iEnd = blockers.end(), jEnd = blocked.end();
			for (i = blockers.begin(); i != iEnd; ++i)
				for (j = blocked.begin(); j != jEnd; ++j)
					j->second->BlockedBy(i->second);
		}
	};
	struct PushPlanFromList
	{
		static void Do(ActionList& actionList)
		{
			ActionList::iterator it, itEnd = actionList.end();
			for (it = actionList.begin(); it != itEnd; ++it)
				it->first->m_Plan.push_back(it->second);
		}
	};

public:
	typedef enum
	{
		ACTION_RUNNING,
		ACTION_DONE,
		ACTION_FAILED
	} eActionUpdateResult;

	CLeaderAction();
	CLeaderAction(CLeader* pLeader);
	virtual ~CLeaderAction();

	ELeaderAction               GetType() const           { return m_eActionType; };
	ELeaderActionSubType        GetSubType() const        { return m_eActionSubType; };
	int                         GetNumLiveUnits() const;
	inline uint32               GetUnitProperties() const { return m_unitProperties; }

	bool                        IsUnitAvailable(const CUnitImg& unit) const;

	virtual eActionUpdateResult Update() { return ACTION_DONE; }
	virtual void                DeadUnitNotify(CAIActor* pObj); // CLeaderAction will manage the inter-dependencies between
	// the died member's actions and other members ones
	virtual void                BusyUnitNotify(CUnitImg&); // CLeaderAction will manage the inter-dependencies between
	// the busy member's actions and other members ones
	virtual void                ResumeUnit(CUnitImg&)                 {}; // CLeaderAction will re-create a planning for the resumed unit
	virtual bool                ProcessSignal(const AISIGNAL& signal) { return false; }
	inline void                 SetPriority(int priority)             { m_Priority = priority; };
	inline int                  GetPriority() const                   { return m_Priority; };

	TUnitList&   GetUnitsList() const;
	virtual void Serialize(TSerialize ser);

	virtual void SetUnitFollow(CUnitImg& unit)                         {};
	virtual void SetUnitHold(CUnitImg& unit, const Vec3& point = ZERO) {};
	virtual void UpdateBeacon() const;
	// Returns pointer to the unit image of the group formation.
	CUnitImg*    GetFormationOwnerImg() const;
	virtual void AddUnitNotify(CAIActor* pUnit) {};

protected:
	CLeader*                   m_pLeader;
	ELeaderAction              m_eActionType;
	ELeaderActionSubType       m_eActionSubType;
	int                        m_Priority;
	uint32                     m_unitProperties;
	IAISystem::ENavigationType m_currentNavType;
	IAISystem::tNavCapMask     m_NavProperties;

protected:
	void        CheckLeaderDistance() const;
	const Vec3& GetDestinationPoint();
	bool        IsUnitTooFar(const CUnitImg& tUnit, const Vec3& vPos) const;
	void        CheckNavType(CAIActor* pMember, bool bSignal);
	void        SetTeamNavProperties();
	void        ClearUnitFlags();

	//////////////////////////////////////////////////////////////////////////
};

class CLeaderAction_Attack : public CLeaderAction
{
public:
	CLeaderAction_Attack(CLeader* pLeader);
	CLeaderAction_Attack();//used for derived classes' constructors
	virtual ~CLeaderAction_Attack();

	virtual bool ProcessSignal(const AISIGNAL& signal);
	virtual void Serialize(TSerialize ser);
protected:
	bool         HasTarget(CAIObject* unit) const;
	bool               m_bInitialized;
	bool               m_bStealth;
	bool               m_bApproachWithNoObstacle;
	bool               m_bNoTarget;
	CTimeValue         m_timeRunning;
	float              m_timeLimit;
	static const float m_CSearchDistance;
	Vec3               m_vDefensePoint;
	Vec3               m_vEnemyPos;

};

class CLeaderAction_Search : public CLeaderAction
{
public:
	struct SSearchPoint
	{
		Vec3 pos;
		Vec3 dir;
		bool bReserved;
		bool bHideSpot;
		SSearchPoint() { bReserved = false; bHideSpot = false; }
		SSearchPoint(const Vec3& p, const Vec3& d) : pos(p), dir(d) { bReserved = false; bHideSpot = false; }
		SSearchPoint(const Vec3& p, const Vec3& d, bool hideSpot) : pos(p), dir(d) { bReserved = false; bHideSpot = hideSpot; }
		virtual ~SSearchPoint(){}
		virtual void Serialize(TSerialize ser)
		{
			ser.Value("pos", pos);
			ser.Value("dir", dir);
			ser.Value("bReserved", bReserved);
			ser.Value("bHideSpot", bHideSpot);
		}

	};
	typedef std::multimap<float, SSearchPoint> TPointMap;
	CLeaderAction_Search(CLeader* pLeader, const LeaderActionParams& params);
	virtual ~CLeaderAction_Search();

	virtual eActionUpdateResult Update();

	virtual bool                ProcessSignal(const AISIGNAL& signal);
	virtual void                Serialize(TSerialize ser);
private:
	void                        PopulateSearchSpotList(Vec3& initPos);

	CTimeValue          m_timeRunning;
	enum  { m_timeLimit = 20 };
	static const float  m_CSearchDistance;
	static const uint32 m_CCoverUnitProperties;
	int                 m_iCoverUnitsLeft;
	Vec3                m_vEnemyPos;
	float               m_fSearchDistance;
	//SetObstacleRefs		m_Passed;
	TPointMap m_HideSpots;
	bool      m_bInitialized;
	int       m_iSearchSpotAIObjectType;
	CAIActor* pSelectedUnit;

};

//----------------------------------------------------------
class CLeaderAction_Attack_SwitchPositions : public CLeaderAction_Attack
{
public:

	CLeaderAction_Attack_SwitchPositions(CLeader* pLeader, const LeaderActionParams& params);
	CLeaderAction_Attack_SwitchPositions(); //used for derived classes' constructors

	virtual ~CLeaderAction_Attack_SwitchPositions();
	virtual eActionUpdateResult Update();
	virtual void                Serialize(TSerialize ser);
	bool                        ProcessSignal(const AISIGNAL& signal);
	virtual void                OnObjectRemoved(CAIObject* pObject);
	virtual void                AddUnitNotify(CAIActor* pUnit);
	void                        UpdateBeaconWithTarget(const CAIObject* pTarget = NULL) const;

protected:
	struct SDangerPoint
	{
		virtual ~SDangerPoint(){}

		SDangerPoint()
			: time(.0f)
			, radius(.0f)
			, point(ZERO)
		{}

		SDangerPoint(float t, float r, Vec3& p)
		{
			time = t;
			radius = r;
			point = p;
		};

		virtual void Serialize(TSerialize ser)
		{
			ser.Value("time", time);
			ser.Value("radius", radius);
			ser.Value("point", point);
		}
		float time;
		float radius;
		Vec3  point;
	};
	struct STargetData
	{
		virtual ~STargetData(){}
		CWeakRef<CAIObject>        refTarget;
		IAISystem::ENavigationType navType;
		IAISystem::ENavigationType targetNavType;
		STargetData() : navType(IAISystem::NAV_UNSET), targetNavType(IAISystem::NAV_UNSET) {}

		virtual void Serialize(TSerialize ser)
		{
			ser.EnumValue("navType", navType, IAISystem::NAV_UNSET, IAISystem::NAV_MAX_VALUE);
			ser.EnumValue("targetNavType", targetNavType, IAISystem::NAV_UNSET, IAISystem::NAV_MAX_VALUE);
			refTarget.Serialize(ser, "refTarget");
		}
	};

	typedef enum
	{
		AS_OFF,
		AS_WAITING_CONFIRM,
		AS_ON
	} eActionStatus;

	struct SSpecialAction
	{
		virtual ~SSpecialAction(){}
		eActionStatus      status;
		CWeakRef<CAIActor> refOwner;
		CTimeValue         lastTime;

		SSpecialAction()
		{
			status = AS_OFF;
			lastTime.SetValue(0);
		}

		SSpecialAction(CWeakRef<CAIActor> _refOwner)
			: refOwner(_refOwner)
		{
			status = AS_OFF;
			lastTime.SetValue(0);
		}

		virtual void Serialize(TSerialize ser)
		{
			refOwner.Serialize(ser, "refOwner");
			ser.Value("lastTime", lastTime);
			ser.EnumValue("status", status, AS_OFF, AS_ON);
		}

	};

	struct SPointProperties
	{
		virtual ~SPointProperties(){}
		SPointProperties()
		{
			bTargetVisible = false;
			fAngle = 0.f;
			fValue = 0.f;
			point.Set(0, 0, 0);
		}

		SPointProperties(Vec3& p) : point(p)
		{
			bTargetVisible = false;
			fAngle = 0.f;
			fValue = 0.f;
		}

		virtual void Serialize(TSerialize ser)
		{
			ser.Value("bTargetVisible", bTargetVisible);
			ser.Value("fAngle", fAngle);
			ser.Value("fValue", fValue);
			ser.Value("point", point);
			refOwner.Serialize(ser, "refOwner");
		}

		bool                bTargetVisible;
		float               fAngle;
		float               fValue;
		Vec3                point;
		CWeakRef<CAIObject> refOwner;

	};

	class CActorSpotListManager
	{
		typedef std::multimap<const CWeakRef<CAIActor>, Vec3> TActorSpotList;

		TActorSpotList m_ActorSpotList;
	public:
		virtual ~CActorSpotListManager(){}
		void AddSpot(const CWeakRef<CAIActor>& refActor, Vec3& point)
		{
			m_ActorSpotList.insert(std::make_pair(refActor, point));
		}

		void RemoveAllSpots(const CWeakRef<CAIActor>& refActor)
		{
			m_ActorSpotList.erase(refActor);
		}

		bool IsForbiddenSpot(const CWeakRef<CAIActor>& refActor, Vec3& point)
		{
			TActorSpotList::iterator it = m_ActorSpotList.find(refActor), itEnd = m_ActorSpotList.end();
			while (it != itEnd && it->first == refActor)
			{
				if (Distance::Point_PointSq(it->second, point) < 1.f)
					return true;

				++it;
			}
			return false;
		}

		virtual void Serialize(TSerialize ser)
		{
			ser.BeginGroup("ActorSpotList");
			int size = m_ActorSpotList.size();
			ser.Value("size", size);
			TActorSpotList::iterator it = m_ActorSpotList.begin();
			if (ser.IsReading())
				m_ActorSpotList.clear();

			Vec3 point;
			CWeakRef<CAIActor> refUnitActor;
			char name[32];

			for (int i = 0; i < size; i++)
			{
				cry_sprintf(name, "ActorSpot_%d", i);
				ser.BeginGroup(name);
				{
					if (ser.IsWriting())
					{
						refUnitActor = it->first;
						point = it->second;
						++it;
					}
					refUnitActor.Serialize(ser, "refUnitActor");
					ser.Value("point", point);

					if (ser.IsReading())
						m_ActorSpotList.insert(std::make_pair(refUnitActor, point));
				}
				ser.EndGroup();

			}
			ser.EndGroup();

		}
	};

	typedef std::vector<Vec3>                                       TShootSpotList;
	typedef std::vector<SDangerPoint>                               TDangerPointList;
	typedef std::map<CWeakRef<CAIActor>, STargetData>               TMapTargetData; // deprecated
	typedef std::vector<SPointProperties>                           TPointPropertiesList;

	typedef std::multimap<CWeakRef<const CAIActor>, SSpecialAction> TSpecialActionMap;

	int          GetFormationPointWithTarget(CUnitImg& unit);
	void         InitPointProperties();
	void         CheckNavType(CUnitImg& unit);
	void         InitNavTypeData();
	STargetData* GetTargetData(CUnitImg& unit);
	void         AssignNewShootSpot(CAIObject* pUnit, int i);
	void         UpdatePointList(CAIObject* pTarget = NULL);
	bool         UpdateFormationScale(CFormation* pFormation);
	void         SetFormationSize(const CFormation* pFormation);
	Vec3         GetBaseDirection(CAIObject* pTarget, bool bUseTargetMoveDir = false);
	void         GetBaseSearchPosition(CUnitImg& unit, CAIObject* pTarget, int method, float distance = 0);
	void         UpdateSpecialActions();
	bool         IsVehicle(const CAIActor* pTarget, IEntity** ppVehicleEntity = NULL) const;
	bool         IsSpecialActionConsidered(const CAIActor* pUnit, const CAIActor* pUnitLiveTarget) const;
protected:

	TMapTargetData        m_TargetData;
	TSpecialActionMap     m_SpecialActions;
	float                 m_fDistanceToTarget;
	float                 m_fMinDistanceToTarget;

	string                m_sFormationType;
	TPointPropertiesList  m_PointProperties;
	bool                  m_bVisibilityChecked;
	bool                  m_bPointsAssigned;
	bool                  m_bAvoidDanger;
	float                 m_fMinDistanceToNextPoint;
	static const float    m_fDefaultMinDistance;
	TDangerPointList      m_DangerPoints;
	Vec3                  m_vUpdatePointTarget;
	float                 m_fFormationSize;
	float                 m_fFormationScale;
	CTimeValue            m_lastScaleUpdateTime;

	IAIObject*            m_pLiveTarget; // no need to be serialized, it's initialized at each update

	CActorSpotListManager m_ActorForbiddenSpotManager;
};

#endif // __LeaderAction_H__
