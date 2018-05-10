// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   SmartObjects.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - ?							: Created by ?

 *********************************************************************/
#ifndef _SMARTOBJECTS_H_
#define _SMARTOBJECTS_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryEntitySystem/IEntitySystem.h>
#include <CryMemory/STLPoolAllocator.h>
#include "Navigation/MNM/MNM.h"

// forward declaration
class CAIActor;
class CSmartObject;
class CSmartObjectClass;
struct CCondition;

struct OffMeshLink_SmartObject;

typedef std::vector<CSmartObjectClass*> CSmartObjectClasses;

struct SNavSOStates
{
	SNavSOStates() : objectEntId(0) {}

	void Clear()
	{
		objectEntId = 0;
		sAnimationDoneUserStates.clear();
		sAnimationDoneObjectStates.clear();
		sAnimationFailUserStates.clear();
		sAnimationFailObjectStates.clear();
	}

	bool IsEmpty() const { return objectEntId == 0; }

	void Serialize(TSerialize ser)
	{
		ser.BeginGroup("SNavSOStates");
		ser.Value("objectEntId", objectEntId);
		ser.Value("sAnimationDoneUserStates", sAnimationDoneUserStates);
		ser.Value("sAnimationDoneObjectStates", sAnimationDoneObjectStates);
		ser.Value("sAnimationFailUserStates", sAnimationFailUserStates);
		ser.Value("sAnimationFailObjectStates", sAnimationFailObjectStates);
		ser.EndGroup();
	}

	EntityId objectEntId;
	string   sAnimationDoneUserStates;
	string   sAnimationDoneObjectStates;
	string   sAnimationFailUserStates;
	string   sAnimationFailObjectStates;
};

///////////////////////////////////////////////
// CSmartObjectEvent represents the priority of usage of some Smart Object
///////////////////////////////////////////////
struct CSmartObjectEvent
{
	CSmartObject* m_pObject;
	CCondition*   m_pCondition;

	// < 0 means this event should be removed
	// 0 means just started
	// 1 means ready to be used
	float m_Delay;
};

///////////////////////////////////////////////
// CSmartObjectBase will be the base for CSmartObject class
///////////////////////////////////////////////
class CSmartObjectBase
{
protected:
	EntityId m_entityId;

public:
	explicit CSmartObjectBase(EntityId entityId) : m_entityId(entityId) {}

	ILINE EntityId         GetEntityId() const { return m_entityId; }
	ILINE IEntity*         GetEntity() const;

	void                   SetEntityId(EntityId id) { m_entityId = id; }

	Vec3                   GetPos() const;
	Vec3                   GetHelperPos(const SmartObjectHelper* pHelper) const;
	Vec3                   GetOrientation(const SmartObjectHelper* pHelper) const;
	ILINE const char*      GetName() const;
	ILINE IPhysicalEntity* GetPhysics() const;
	ILINE CAIObject*       GetAI() const;
	CPipeUser*             GetPipeUser() const;
	CAIActor*              GetAIActor() const;

	void                   OnReused(IEntity* pEntity);
};

enum ESO_Validate
{
	eSOV_Unknown,
	eSOV_InvalidStatic,
	eSOV_InvalidDynamic,
	eSOV_Valid,
};

///////////////////////////////////////////////
// Class Template
///////////////////////////////////////////////
class CClassTemplateData
{
	IStatObj* m_pStatObj;

public:
	CClassTemplateData() : m_pStatObj(0) {}
	~CClassTemplateData() { if (m_pStatObj) m_pStatObj->Release(); }

	string name;

	struct CTemplateHelper
	{
		CTemplateHelper()
			: name("")
			, qt(IDENTITY)
			, radius(.0f)
			, project(false)
		{}

		string name;
		QuatT  qt;
		float  radius;
		bool   project;
	};
	typedef std::vector<CTemplateHelper> TTemplateHelpers;

	string           model;
	TTemplateHelpers helpers;

	const CTemplateHelper* GetHelper(const char* helperName) const
	{
		TTemplateHelpers::const_iterator it, itEnd = helpers.end();
		for (it = helpers.begin(); it != itEnd; ++it)
			if (it->name == helperName)
				return &*it;
		return NULL;
	}

	IStatObj* GetIStateObj()
	{
		if (m_pStatObj)
			return m_pStatObj;

		if (!model.empty())
		{
			m_pStatObj = gEnv->p3DEngine->LoadStatObj("%EDITOR%/Objects/" + model, NULL, NULL, false);
			if (m_pStatObj)
			{
				m_pStatObj->AddRef();
				if (GetHelperMaterial())
					m_pStatObj->SetMaterial(GetHelperMaterial());
			}
		}

		return m_pStatObj;
	}

	static IMaterial* m_pHelperMtl;
	static IMaterial* GetHelperMaterial()
	{
		if (!m_pHelperMtl)
			m_pHelperMtl = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("%EDITOR%/Objects/Helper");
		return m_pHelperMtl;
	};
};
// MapClassTemplateData contains all class templates indexed by name
typedef std::map<string, CClassTemplateData> MapClassTemplateData;

///////////////////////////////////////////////
// CSmartObject contains a pointer to the entity used as smart object and its additional info
///////////////////////////////////////////////
class CSmartObject : public CSmartObjectBase
{
public:
	class CState;
	typedef std::set<CState> SetStates;

	///////////////////////////////////////////////
	// States internally are represented with IDs
	///////////////////////////////////////////////
	class CState
	{
		typedef std::map<string, int>    MapSmartObjectStateIds;
		typedef std::vector<const char*> MapSmartObjectStates;

		static MapSmartObjectStateIds g_mapStateIds;
		static MapSmartObjectStates   g_mapStates;
		static SetStates              g_defaultStates;

		void Assign(const char* state)
		{
			if (*state)
			{
				std::pair<MapSmartObjectStateIds::iterator, bool> pr = g_mapStateIds.insert(std::make_pair(state, static_cast<int>(g_mapStates.size())));
				if (pr.second)   // was insertion made?
					g_mapStates.push_back(pr.first->first.c_str());
				m_StateId = pr.first->second;
			}
			else
			{
				m_StateId = -1;
			}
		}

	protected:
		int m_StateId;

	public:
		explicit CState(const char* state)
		{
			Assign(state);
		}

		CState() : m_StateId(-1) {}

		static void Reset()
		{
			stl::free_container(g_mapStates);
			stl::free_container(g_mapStateIds);
			stl::free_container(g_defaultStates);
		}

		static const SetStates& GetDefaultStates();

		void                    Serialize(TSerialize ser)
		{
			string value = c_str();
			ser.ValueWithDefault("Value", value, "Idle");

			if (ser.IsReading())
			{
				Assign(value.c_str());
			}
		}

		bool               operator<(const CState& other) const  { return m_StateId < other.m_StateId; }
		bool               operator==(const CState& other) const { return m_StateId == other.m_StateId; }
		bool               operator!=(const CState& other) const { return m_StateId != other.m_StateId; }

		int                asInt() const                         { return m_StateId; }
		const char*        c_str() const                         { return (m_StateId >= 0 && m_StateId < (int)g_mapStates.size()) ? g_mapStates[m_StateId] : ""; }

		static int         GetNumStates()                        { return g_mapStates.size(); }
		static const char* GetStateName(int i)                   { return i >= 0 && i < (int)g_mapStates.size() ? g_mapStates[i] : NULL; }
	};

	typedef std::vector<CState> VectorStates;

	class DoubleVectorStates
	{
	public:
		VectorStates positive;
		VectorStates negative;

		string AsString() const
		{
			string temp;
			for (uint32 i = 0; i < positive.size(); ++i)
			{
				if (i) temp += ',';
				temp += positive[i].c_str();
			}
			if (!negative.empty())
			{
				temp += '-';
				for (uint32 i = 0; i < negative.size(); ++i)
				{
					if (i) temp += ',';
					temp += negative[i].c_str();
				}
			}
			return temp;
		}

		string AsUndoString() const
		{
			string temp;
			for (uint32 i = 0; i < negative.size(); ++i)
			{
				if (i) temp += ',';
				temp += negative[i].c_str();
			}
			if (!positive.empty())
			{
				temp += '-';
				for (uint32 i = 0; i < positive.size(); ++i)
				{
					if (i) temp += ',';
					temp += positive[i].c_str();
				}
			}
			return temp;
		}

		bool empty() const { return positive.empty() && negative.empty(); }

		bool operator==(const DoubleVectorStates& other) const
		{
			return positive == other.positive && negative == other.negative;
		}
		bool operator!=(const DoubleVectorStates& other) const
		{
			return positive != other.positive || negative != other.negative;
		}

		bool Matches(const SetStates& states, const SetStates* exceptions = NULL) const
		{
			SetStates::const_iterator statesEnd = states.end();
			VectorStates::const_iterator it, itEnd = positive.end();

			// Check if a required state is missing
			for (it = positive.begin(); it != itEnd; ++it)
				if (states.find(*it) == statesEnd && (!exceptions || (exceptions->find(*it) == exceptions->end())))
					return false;

			itEnd = negative.end();

			// Check if a failing state is present
			for (it = negative.begin(); it != itEnd; ++it)
				if (states.find(*it) != statesEnd && (!exceptions || (exceptions->find(*it) == exceptions->end())))
					return false;

			return true;
		}

		bool ChecksState(CState state) const
		{
			if (std::find(positive.begin(), positive.end(), state) != positive.end())
				return true;
			if (std::find(negative.begin(), negative.end(), state) != negative.end())
				return true;
			return false;
		}
	};

	class CStatePattern : public std::vector<DoubleVectorStates>
	{
	public:
		bool Matches(const SetStates& states, const SetStates* pStatesToNotMatch = NULL) const
		{
			const_iterator it, itEnd = end();
			for (it = begin(); it != itEnd; ++it)
				if (it->Matches(states, pStatesToNotMatch))
					return true;
			return empty();
		}

		string AsString() const
		{
			string temp;
			for (uint32 i = 0; i < size(); ++i)
			{
				if (i)
					temp += " | ";
				temp += at(i).AsString();
			}
			return temp;
		}

		bool ChecksState(CState state) const
		{
			const_iterator it, itEnd = end();
			for (it = begin(); it != itEnd; ++it)
				if (it->ChecksState(state))
					return true;
			return false;
		}
	};

protected:
	friend class CSmartObjectClass;
	friend class CSmartObjectManager;

	CSmartObjectClasses m_vClasses;

	SetStates           m_States;

	float               m_fKey;

	typedef std::vector<CSmartObjectEvent>             VectorEvents;
	typedef std::map<CSmartObjectClass*, VectorEvents> MapEventsByClass;
	MapEventsByClass m_Events;
	float            m_fRandom;

	typedef std::map<CSmartObjectClass*, CTimeValue> MapTimesByClass;
	MapTimesByClass m_mapLastUpdateTimes;

	// LookAt helpers
	float m_fLookAtLimit;
	Vec3  m_vLookAtPos;

	ESO_Validate   m_eValidationResult;

	bool           m_bHidden;

	// returns the size of the AI puppet (or Vec3(ZERO) if it isn't a puppet)
	// x - radius, y - height, z - offset from the ground
	Vec3 MeasureUserSize() const;

	void Reset();

	void UnregisterFromAllClasses();

	// Used with entity reloading
	void OnReused(IEntity* pEntity);

public:
	struct CTemplateHelperInstance
	{
		Vec3         position;
		ESO_Validate validationResult;
	};
	typedef std::vector<CTemplateHelperInstance> TTemplateHelperInstances;

protected:
	struct CTemplateData
	{
		CSmartObjectClass*       pClass;
		float                    userRadius;
		float                    userTop;
		float                    userBottom;
		bool                     staticTest;
		TTemplateHelperInstances helperInstances;

		CTemplateData() : pClass(NULL), userRadius(0.f), userTop(0.f), userBottom(0.f), staticTest(false) {}
	};
	typedef std::map<CClassTemplateData*, CTemplateData> MapTemplates;
	std::unique_ptr<MapTemplates> m_pMapTemplates;

public:
	explicit CSmartObject(EntityId entityId);
	~CSmartObject();

	void Register();

	void Hide(bool hide)  { m_bHidden = hide; }
	bool IsHidden() const { return m_bHidden; }

	void Serialize(TSerialize ser);
	void SerializePointer(TSerialize ser, const char* name, CSmartObject*& pSmartObject);

	bool operator<(const CSmartObject& other) const  { return m_entityId < other.m_entityId; }
	bool operator==(const CSmartObject& other) const { return m_entityId == other.m_entityId; }

	//Adds a class to the current set
	void RegisterSmartObjectClass(CSmartObjectClass* pClass) { m_vClasses.push_back(pClass); }

	//Removes a class from the current set
	void UnregisterSmartObjectClass(CSmartObjectClass* pClass)
	{
		const bool foundClass = stl::find_and_erase(m_vClasses, pClass);
		AIAssert(foundClass);
	}

	CSmartObjectClasses& GetClasses()      { return m_vClasses; }
	const SetStates& GetStates() const { return m_States; }

	void             Use(CSmartObject* pObject, CCondition* pCondition, int eventId = 0, bool bForceHighPriority = false) const;

	/// Measures the user size and applies value to all associated smart object classes
	void ApplyUserSize();

	MapTemplates& GetMapTemplates();

	ESO_Validate  GetValidationResult() const { return m_eValidationResult; }
};

typedef std::pair<CSmartObjectClass*, CSmartObject::CState> PairClassState;

///////////////////////////////////////////////
// CSmartObjectClass maps entities to smart objects
///////////////////////////////////////////////
class CSmartObjectClass
{
protected:
	typedef std::vector<CSmartObject*> VectorSmartObjects;
	VectorSmartObjects m_allSmartObjectInstances;
	unsigned int       m_indexNextObject;

	const string       m_sName;

	bool               m_bSmartObjectUser;
	bool               m_bNeeded;

public:

	// classes now keep track of all their instances
	typedef std::multimap<float, CSmartObject*, std::less<float>, stl::STLPoolAllocator<std::pair<const float, CSmartObject*>, stl::PoolAllocatorSynchronizationSinglethreaded>> MapSmartObjectsByPos;
	MapSmartObjectsByPos m_MapObjectsByPos; // map of all smart objects indexed by their position

	typedef std::vector<SmartObjectHelper*> VectorHelpers;
	VectorHelpers m_vHelpers;

	typedef std::map<string, CSmartObjectClass*> MapClassesByName;
	static MapClassesByName g_AllByName;

	typedef std::vector<CSmartObjectClass*> VectorClasses;
	static VectorClasses           g_AllUserClasses;
	static VectorClasses::iterator g_itAllUserClasses;

	CSmartObjectClass(const char* className);
	~CSmartObjectClass();

	static CSmartObjectClass* find(const char* sClassName);

	const string              GetName() const { return m_sName; }

	void                      RegisterSmartObject(CSmartObject* pSmartObject);
	void                      UnregisterSmartObject(CSmartObject* pSmartObject);
	bool                      RemoveSmartObject(CSmartObject* pSmartObject, bool bCanDelete);

	void                      MarkAsSmartObjectUser()
	{
		m_bSmartObjectUser = true;
		if (m_bNeeded)
		{
			if (std::find(g_AllUserClasses.begin(), g_AllUserClasses.end(), this) == g_AllUserClasses.end())
				g_AllUserClasses.push_back(this);
			g_itAllUserClasses = g_AllUserClasses.end();
		}
	}
	bool IsSmartObjectUser() const { return m_bSmartObjectUser; }

	void MarkAsNeeded()
	{
		m_bNeeded = true;
		if (m_bSmartObjectUser)
		{
			if (std::find(g_AllUserClasses.begin(), g_AllUserClasses.end(), this) == g_AllUserClasses.end())
				g_AllUserClasses.push_back(this);
			g_itAllUserClasses = g_AllUserClasses.end();
		}
	}
	bool          IsNeeded() const { return m_bNeeded; }

	void          FirstObject()    { m_indexNextObject = 0; }
	CSmartObject* NextObject()
	{
		if (m_indexNextObject >= m_allSmartObjectInstances.size())
			return NULL;
		return m_allSmartObjectInstances[m_indexNextObject++];
	}
	CSmartObject* NextVisibleObject()
	{
		unsigned int size = m_allSmartObjectInstances.size();
		while (m_indexNextObject < size && m_allSmartObjectInstances[m_indexNextObject]->IsHidden())
			++m_indexNextObject;
		if (m_indexNextObject >= size)
			return NULL;
		return m_allSmartObjectInstances[m_indexNextObject++];
	}

	void               AddHelper(SmartObjectHelper* pHelper) { m_vHelpers.push_back(pHelper); }
	SmartObjectHelper* GetHelper(const char* name) const
	{
		VectorHelpers::const_iterator it, itEnd = m_vHelpers.end();
		for (it = m_vHelpers.begin(); it != itEnd; ++it)
			if ((*it)->name == name)
				return *it;
		return NULL;
	}

	struct HelperLink
	{
		float              passRadius;
		SmartObjectHelper* from;
		SmartObjectHelper* to;
		CCondition*        condition;
	};
	typedef std::vector<HelperLink> THelperLinks;
	THelperLinks m_vHelperLinks;

	typedef std::set<SmartObjectHelper*> SetHelpers;
	SetHelpers m_setNavHelpers;

	void ClearHelperLinks();
	bool AddHelperLink(CCondition* condition);
	int  FindHelperLinks(const SmartObjectHelper* from, const SmartObjectHelper* to, const CSmartObjectClass* pClass, float radius,
	                     CSmartObjectClass::HelperLink** pvHelperLinks, int iCount = 1);
	int  FindHelperLinks(const SmartObjectHelper* from, const SmartObjectHelper* to, const CSmartObjectClass* pClass, float radius,
	                     const CSmartObject::SetStates& userStates, const CSmartObject::SetStates& objectStates,
	                     CSmartObjectClass::HelperLink** pvHelperLinks, int iCount = 1, const CSmartObject::SetStates* pObjectStatesToExcludeFromMatches = NULL);

	struct UserSize
	{
		float radius;
		float bottom;
		float top;

		UserSize() : radius(0), top(0), bottom(FLT_MAX) {}
		UserSize(float r, float t, float b) : radius(r), top(t), bottom(b) {}
		operator bool() const { return radius > 0 && top > bottom; }
		const UserSize& operator+=(const UserSize& other)
		{
			if (*this)
			{
				if (other)
				{
					if (other.radius > radius)
						radius = other.radius;
					if (other.top > top)
						top = other.top;
					if (other.bottom < bottom)
						bottom = other.bottom;
				}
			}
			else
				*this = other;
			return *this;
		}
	};
	UserSize m_StanceMaxSize;
	UserSize m_NavUsersMaxSize;
	void AddNavUserClass(CSmartObjectClass* pUserClass)
	{
		m_NavUsersMaxSize += pUserClass->m_StanceMaxSize;
	}

	CClassTemplateData* m_pTemplateData;

	// optimization: each user class keeps a list of active update rules
	typedef std::vector<CCondition*> VectorRules;
	VectorRules m_vActiveUpdateRules[3]; // three vectors - one for each alertness level (0 includes all in 1 and 2; 1 includes all in 2)

private:
	void RemoveFromPositionMap(CSmartObject* pSmartObject);
};

///////////////////////////////////////////////
// CCondition defines conditions under which a smart object should be used.
// It defines everything except conditions about the user, because user's condition will be the key of a map later...
///////////////////////////////////////////////
struct CCondition
{
	int                              iTemplateId;

	CSmartObjectClass*               pUserClass;
	CSmartObject::CStatePattern      userStatePattern;

	CSmartObjectClass*               pObjectClass;
	CSmartObject::CStatePattern      objectStatePattern;

	string                           sUserHelper;
	SmartObjectHelper*               pUserHelper;
	string                           sObjectHelper;
	SmartObjectHelper*               pObjectHelper;

	float                            fDistanceFrom;
	float                            fDistanceTo;
	float                            fOrientationLimit;
	bool                             bHorizLimitOnly;
	float                            fOrientationToTargetLimit;

	float                            fMinDelay;
	float                            fMaxDelay;
	float                            fMemory;

	float                            fProximityFactor;
	float                            fOrientationFactor;
	float                            fVisibilityFactor;
	float                            fRandomnessFactor;

	float                            fLookAtPerc;
	CSmartObject::DoubleVectorStates userPreActionStates;
	CSmartObject::DoubleVectorStates objectPreActionStates;
	EActionType                      eActionType;
	string                           sAction;
	CSmartObject::DoubleVectorStates userPostActionStates;
	CSmartObject::DoubleVectorStates objectPostActionStates;

	int                              iMaxAlertness;
	bool                             bEnabled;

	int                              iRuleType; // 0 - normal rule; 1 - navigational rule;
	string                           sEvent;
	string                           sChainedUserEvent;
	string                           sChainedObjectEvent;
	string                           sEntranceHelper;
	string                           sExitHelper;

	string                           sName;
	string                           sDescription;
	string                           sFolder;
	int                              iOrder;

	// exact positioning related
	float  fApproachSpeed;
	int    iApproachStance;
	string sAnimationHelper;
	string sApproachHelper;
	float  fStartWidth;
	float  fDirectionTolerance;
	float  fStartArcAngle;

	bool operator==(const CCondition& other) const;
};

typedef std::multimap<CSmartObjectClass*, CCondition>  MapConditions;
typedef std::multimap<CSmartObjectClass*, CCondition*> MapPtrConditions;

///////////////////////////////////////////////
// Smart Object Event
///////////////////////////////////////////////
class CEvent
{
public:
	const string     m_sName;        // event name
	string           m_sDescription; // description of this event
	MapPtrConditions m_Conditions;   // map of all conditions indexed by user's class

	CEvent(const string& name) : m_sName(name) {}
};

///////////////////////////////////////////////
// CQueryEvent
///////////////////////////////////////////////

class CQueryEvent
{
public:
	CSmartObject* pUser;
	CSmartObject* pObject;
	CCondition*   pRule;
	CQueryEvent*  pChainedUserEvent;
	CQueryEvent*  pChainedObjectEvent;

	CQueryEvent() : pUser(0), pObject(0), pRule(0), pChainedUserEvent(0), pChainedObjectEvent(0)
	{}

	CQueryEvent(const CQueryEvent& other)
	{
		*this = other;
	}

	~CQueryEvent()
	{
		if (pChainedUserEvent)
			delete pChainedUserEvent;
		if (pChainedObjectEvent)
			delete pChainedObjectEvent;
	}

	void Clear()
	{
		pUser = NULL;
		pObject = NULL;
		pRule = NULL;
		if (pChainedUserEvent)
			delete pChainedUserEvent;
		if (pChainedObjectEvent)
			delete pChainedObjectEvent;
		pChainedUserEvent = NULL;
		pChainedObjectEvent = NULL;
	}

	const CQueryEvent& operator=(const CQueryEvent& other)
	{
		pUser = other.pUser;
		pObject = other.pObject;
		pRule = other.pRule;
		pChainedUserEvent = other.pChainedUserEvent ? new CQueryEvent(*other.pChainedUserEvent) : NULL;
		pChainedObjectEvent = other.pChainedObjectEvent ? new CQueryEvent(*other.pChainedObjectEvent) : NULL;
		return *this;
	}

	void Serialize(TSerialize ser);

};

// while querying matches the events will be stored in a container of this type sorted by priority
typedef std::multimap<float, CQueryEvent> QueryEventMap;

///////////////////////////////////////////////
// CSmartObjectManager receives notifications from entity system about entities being spawned and deleted.
// Keeps track of registered classes, and automatically creates smart object representation for every instance belonging to them.
///////////////////////////////////////////////
class CSmartObjectManager final
	: public IEntitySystemSink
	, public IEntityEventListener
	, public ISmartObjectManager
{
private:
	CSmartObject::CState    m_StateAttTarget;
	CSmartObject::CState    m_StateSameGroup;
	CSmartObject::CState    m_StateSameFaction;
	CSmartObject::CState    m_StateBusy;

	CSmartObject::SetStates m_statesToExcludeForPathfinding;

	MapConditions           m_Conditions; // map of all conditions indexed by user's class

	// used for debug-drawing used smart objects
	struct CDebugUse
	{
		CTimeValue    m_Time;
		CSmartObject* m_pUser;
		CSmartObject* m_pObject;
	};
	typedef std::vector<CDebugUse> VectorDebugUse;
	VectorDebugUse m_vDebugUse;

	// Implementation of IEntitySystemSink methods
	///////////////////////////////////////////////
	virtual bool OnBeforeSpawn(SEntitySpawnParams& params) { return true; };
	virtual void OnSpawn(IEntity* pEntity, SEntitySpawnParams& params);
	virtual bool OnRemove(IEntity* pEntity);
	virtual void OnReused(IEntity* pEntity, SEntitySpawnParams& params);

	void         DoRemove(IEntity* pEntity, bool bDeleteSmartObject = true);

	void         AddSmartObjectClass(CSmartObjectClass* soClass);

	bool m_bRecalculateUserSize;
	void RecalculateUserSize();

	bool m_initialized;

public:
	friend class CSmartObject;
	friend class CSmartObjectBase;
	friend class CQueryEvent;

	//////////////////////////////////////////////////////////////////////////
	//ISmartObjectManager/////////////////////////////////////////////////////
	virtual void               RemoveSmartObject(IEntity* pEntity);

	virtual const char*        GetSmartObjectStateName(int id) const;

	virtual void               RegisterSmartObjectState(const char* sStateName);
	virtual void               AddSmartObjectState(IEntity* pEntity, const char* sState);
	virtual void               RemoveSmartObjectState(IEntity* pEntity, const char* sState);
	virtual bool               CheckSmartObjectStates(const IEntity* pEntity, const char* patternStates) const;
	virtual void               SetSmartObjectState(IEntity* pEntity, const char* sStateName);

	virtual void               ModifySmartObjectStates(IEntity* pEntity, const char* listStates);

	virtual void               DrawSOClassTemplate(IEntity* pEntity);
	virtual bool               ValidateSOClassTemplate(IEntity* pEntity);

	virtual uint32             GetSOClassTemplateIStatObj(IEntity* pEntity, IStatObjPtr* ppStatObjectPtrs, uint32& numAllocStatObjects);

	virtual void               ReloadSmartObjectRules();

	virtual int                SmartObjectEvent(const char* sEventName, IEntity*& pUser, IEntity*& pObject, const Vec3* pExtraPoint = NULL, bool bHighPriority = false);

	virtual SmartObjectHelper* GetSmartObjectHelper(const char* className, const char* helperName) const;
	//ISmartObjectManager/////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////

	// IEntityEventListener
	virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event);
	// ~IEntityEventListener

	CSmartObjectManager();
	~CSmartObjectManager();

	bool LoadSmartObjectsLibrary();
	bool IsInitialized() const;//TODO fix initialization of this and CAISystem so this is not needed!

	void DebugDraw();
	void DebugDrawValidateSmartObjectArea() const;
	void DebugDrawBannedNavsos();

	void SoftReset();

	void Update();
	void UpdateBannedSOs(float frameDeltaTime);
	void ResetBannedSOs();

	void Serialize(TSerialize ser);

	// returns the id of the inserted goal pipe if a rule was found or 0 if no rule
	int TriggerEvent(const char* sEventName, IEntity*& pUser, IEntity*& pObject, QueryEventMap* pQueryEvents = NULL, const Vec3* pExtraPoint = NULL, bool bHighPriority = false);

	/// used by heuristic to check is this link passable by current pathfind requester
	bool  GetSmartObjectLinkCostFactorForMNM(const OffMeshLink_SmartObject* pSmartObjectLink, const IEntity* pRequesterEntity, float* fCostMultiplier) const;

	// used by COPTrace to use smart objects in navigation
	int                  GetNavigationalSmartObjectActionTypeForMNM(CPipeUser* pPipeUser, CSmartObject* pSmartObject, CSmartObjectClass* pSmartObjectClass, SmartObjectHelper* pFromHelper, SmartObjectHelper* pToHelper);
	bool                 PrepareNavigateSmartObject(CPipeUser* pPipeUser, CSmartObject* pObject, CSmartObjectClass* pObjectClass, SmartObjectHelper* pFromHelper, SmartObjectHelper* pToHelper);
	void                 UseSmartObject(CSmartObject* pSmartObjectUser, CSmartObject* pObject, CCondition* pCondition, int eventId = 0, bool bForceHighPriority = false);
	/// Used by COPTrace to detect busy state of SO allowing agents to wait instead of simply failing the movement request.
	bool                 IsSmartObjectBusy(const CSmartObject* pSmartObject) const;

	void                 MapPathTypeToSoUser(const string& soUser, const string& pathType);
	bool                 CheckSmartObjectStates(const IEntity* pEntity, const CSmartObject::CStatePattern& pattern) const;

	static CSmartObject* GetSmartObject(EntityId id);
	static void          BindSmartObjectToEntity(EntityId id, CSmartObject* pSO);
	static bool          RemoveSmartObjectFromEntity(EntityId id, CSmartObject* pSO = NULL);

private:
	struct SSOTemplateArea
	{
		Vec3  pt;
		float radius;
		bool  projectOnGround;
	};
	struct SSOUser
	{
		IEntity* entity;
		float    radius;
		float    height;
		float    groundOffset;
	};

	typedef VectorMap<CSmartObject*, float> SmartObjectFloatMap;
	SmartObjectFloatMap                      m_bannedNavSmartObjects;
	std::map<string, string>                 m_MappingSOUserPathType;
	static std::map<EntityId, CSmartObject*> g_smartObjectEntityMap;

	const AgentPathfindingProperties* GetPFPropertiesOfSoUser(const string& soUser);

	static bool                       ValidateSmartObjectArea(const SSOTemplateArea& templateArea, const SSOUser& user, EAICollisionEntities collEntities, Vec3& groundPos);
	int                               FindSmartObjectLinks(const CPipeUser* pPipeUser, CSmartObject* pObject, CSmartObjectClass* pObjectClass,
	                                                       SmartObjectHelper* pFromHelper, SmartObjectHelper* pToHelper, CSmartObjectClass::HelperLink** pvHelperLinks,
	                                                       int iCount = 1, const IEntity* pAdditionalEntity = NULL, const CSmartObject::SetStates* pStatesExcludedFromMatchingTheObjectState = NULL) const;

	typedef std::set<CSmartObject*> SmartObjects;
	static SmartObjects g_AllSmartObjects;

	typedef std::pair<CSmartObject*, CCondition*> PairObjectCondition;
	typedef std::pair<PairObjectCondition, float> PairDelayTime;
	typedef std::vector<PairDelayTime>            VecDelayTimes;

	struct Pred_IgnoreSecond
	{
		bool operator()(const PairDelayTime& A, const PairDelayTime& B) const
		{
			return A.first < B.first;
		}
	};

	// define a map of event updates
	VecDelayTimes m_tmpVecDelayTimes;

	// MapSOHelpers contains all smart object helpers sorted by name of the smart object class to which they belong
	typedef std::multimap<string, SmartObjectHelper> MapSOHelpers;
	MapSOHelpers m_mapHelpers;

	typedef std::map<string, CEvent> MapEvents;
	MapEvents            m_mapEvents; // map of all smart object events indexed by name

	MapClassTemplateData m_mapClassTemplates;
	void               LoadSOClassTemplates();

	void               RescanSOClasses(IEntity* pEntity);

	void               String2Classes(const string& sClass, CSmartObjectClasses& vClasses);
	static void        String2States(const char* listStates, CSmartObject::DoubleVectorStates& states);
	static void        String2StatePattern(const char* sPattern, CSmartObject::CStatePattern& pattern);

	static void        SerializePointer(TSerialize ser, const char* name, CSmartObject*& pSmartObject);
	static void        SerializePointer(TSerialize ser, const char* name, CCondition*& pRule);

	CSmartObjectClass* GetSmartObjectClass(const char* className);

	void               AddSmartObjectCondition(const SmartObjectCondition& conditionData);

	bool               ParseClassesFromProperties(const IEntity* pEntity, CSmartObjectClasses& vClasses);

	void               ClearConditions();
	void               Reset();

	float              CalculateDelayTime(CSmartObject* pUser, const Vec3& posUser,
	                                      CSmartObject* pObject, const Vec3& posObject, CCondition* pCondition) const;
	float              CalculateDot(CCondition* condition, const Vec3& dir, CSmartObject* user) const;
	int                Process(CSmartObject* pSmartObjectUser, CSmartObjectClass* pClass);
	bool               PrepareUseNavigationSmartObject(SAIActorTargetRequest& pReq, SNavSOStates& states, CSmartObject* pSmartObjectUser, CSmartObject* pSmartObject, const CCondition* pCondition, SmartObjectHelper* pFromHelper, SmartObjectHelper* pToHelper);

	void               SetSmartObjectState(IEntity* pEntity, CSmartObject::CState state) const;
	void               SetSmartObjectState(CSmartObject* pSmartObject, CSmartObject::CState state) const;
	void               AddSmartObjectState(IEntity* pEntity, CSmartObject::CState state) const;
	void               AddSmartObjectState(CSmartObject* pSmartObject, CSmartObject::CState state) const;
	void               RemoveSmartObjectState(IEntity* pEntity, CSmartObject::CState state) const;
	void               RemoveSmartObjectState(CSmartObject* pSmartObject, CSmartObject::CState state) const;
	void               ModifySmartObjectStates(IEntity* pEntity, const CSmartObject::DoubleVectorStates& states);
	void               ModifySmartObjectStates(CSmartObject* pSmartObject, const CSmartObject::DoubleVectorStates& states) const;

	void               RebuildNavigation();
	bool               RegisterInNavigation(CSmartObject* pSmartObject, CSmartObjectClass* pClass);
	void               UnregisterFromNavigation(CSmartObject* pSmartObject) const;

	void               RemoveEntity(IEntity* pEntity);

	CEvent*            String2Event(const string& name)
	{
		MapEvents::iterator it = m_mapEvents.insert(MapEvents::value_type(name, name)).first;
		return &it->second;
	}

	// SOClass template related
	void GetTemplateIStatObj(IEntity* pEntity, std::vector<IStatObj*>& statObjects);
	bool ValidateTemplate(IEntity* pEntity, bool bStaticOnly, IEntity* pUserEntity = NULL, int fromTemplateHelperIdx = -1, int toTemplateHelperIdx = -1);
	bool ValidateTemplate(CSmartObject* pSmartObject, bool bStaticOnly, IEntity* pUserEntity = NULL, int fromTemplateHelperIdx = -1, int toTemplateHelperIdx = -1);
	void DrawTemplate(IEntity* pEntity);
	void DrawTemplate(CSmartObject* pSmartObject, bool bStaticOnly);

private:
	IEntity* m_pPreOnSpawnEntity; // the entity for which OnSpawn was called just right before the current OnSpawn call

	int  TriggerEventUserObject(const char* sEventName, CSmartObject* pUser, CSmartObject* pObject, QueryEventMap* pQueryEvents, const Vec3* pExtraPoint, bool bHighPriority);
	int  TriggerEventUser(const char* sEventName, CSmartObject* pUser, QueryEventMap* pQueryEvents, IEntity** ppObjectEntity, const Vec3* pExtraPoint, bool bHighPriority);
	int  TriggerEventObject(const char* sEventName, CSmartObject* pObject, QueryEventMap* pQueryEvents, IEntity** ppUserEntity, const Vec3* pExtraPoint, bool bHighPriority);
};

IEntity* CSmartObjectBase::GetEntity() const
{
	AIAssert(gEnv);
	AIAssert(gEnv->pEntitySystem);
	return gEnv->pEntitySystem->GetEntity(m_entityId);
}

const char* CSmartObjectBase::GetName() const
{
	const IEntity* pEntity = GetEntity();
	return pEntity ? pEntity->GetName() : "<NONE>";
}

IPhysicalEntity* CSmartObjectBase::GetPhysics() const
{
	const IEntity* pEntity = GetEntity();
	return pEntity ? pEntity->GetPhysics() : NULL;
}

CAIObject* CSmartObjectBase::GetAI() const
{
	if (IEntity* pEntity = GetEntity())
	{
		return static_cast<CAIObject*>(pEntity->GetAI());
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
/// MNM integration

struct OffMeshLink_SmartObject : public MNM::OffMeshLink
{
	OffMeshLink_SmartObject()
		: MNM::OffMeshLink(eLinkType_SmartObject, 0)
		, m_pSmartObject(NULL)
		, m_pSmartObjectClass(NULL)
		, m_pFromHelper(NULL)
		, m_pToHelper(NULL)
	{
	}

	OffMeshLink_SmartObject(const EntityId objectId, CSmartObject* _smartObject, CSmartObjectClass* _smartObjectClass, SmartObjectHelper* _fromHelper, SmartObjectHelper* _toHelper)
		: MNM::OffMeshLink(eLinkType_SmartObject, objectId)
		, m_pSmartObject(_smartObject)
		, m_pSmartObjectClass(_smartObjectClass)
		, m_pFromHelper(_fromHelper)
		, m_pToHelper(_toHelper)
	{

	}

	virtual ~OffMeshLink_SmartObject() {};

	virtual MNM::OffMeshLink* Clone() const
	{
		return new OffMeshLink_SmartObject(GetEntityIdForOffMeshLink(), m_pSmartObject, m_pSmartObjectClass, m_pFromHelper, m_pToHelper);
	}

	virtual Vec3 GetStartPosition() const
	{
		return m_pSmartObject->GetHelperPos(m_pFromHelper);
	}

	virtual Vec3 GetEndPosition() const
	{
		return m_pSmartObject->GetHelperPos(m_pToHelper);
	}

	virtual bool CanUse(const IEntity* pRequester, float* costMultiplier) const
	{
		return gAIEnv.pSmartObjectManager->GetSmartObjectLinkCostFactorForMNM(this, pRequester, costMultiplier);
	}

	static LinkType GetType() { return eLinkType_SmartObject; }

	CSmartObject*      m_pSmartObject;
	CSmartObjectClass* m_pSmartObjectClass;
	SmartObjectHelper* m_pFromHelper;
	SmartObjectHelper* m_pToHelper;
};

#endif
