// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryScriptSystem/IScriptSystem.h> // <> required for Interfuscator

//////////////////////////////////////////////////////////////////////////

//! Common statistic events.
enum EStatisticsEvent
{
	eSE_Kill,
	eSE_Score,
	eSE_Shot,
	eSE_Throw,
	eSE_Hit,
	eSE_Activate,
	eSE_Explode,
	eSE_Death,
	eSE_Reload,
	eSE_Position,
	eSE_Health,
	eSE_Stamina,
	eSE_LookDir,
	eSE_LookRotation,
	eSE_Weapon,
	eSE_Damage,
	eSE_Action,
	eSE_Enable,
	eSE_Consume,
	eSE_Connect,
	eSE_Disconnect,
	eSE_TeamChange,
	eSE_Lifetime,
	eSE_Resurrect,
	eSE_Num
};

//////////////////////////////////////////////////////////////////////////

//! Common statistic states.
enum EStatisticsState
{
	eSS_GameSettings,
	eSS_Map,
	eSS_Gamemode,
	eSS_Team,
	eSS_Winner,
	eSS_Weapons,
	eSS_Ammos,
	eSS_PlayerName,
	eSS_ProfileId,
	eSS_PlayerInfo,
	eSS_EntityId,
	eSS_Kind,
	eSS_TriggerParams,
	eSS_Score,
	eSS_Num
};

//////////////////////////////////////////////////////////////////////////
static const size_t INVALID_STAT_ID      = (size_t)-1;
static const size_t MEMORY_LIMIT_MAXIMUM = (size_t)-1;
//////////////////////////////////////////////////////////////////////////

struct IGameStatistics;
struct IGameStatisticsCallback;
struct IStatsTracker;
struct SNodeLocator;

//! Can be used to save arbitrary data structures as statistics parameters while postponing XML construction until serialization stage.
struct IXMLSerializable
{
	// <interfuscator:shuffle>
	virtual ~IXMLSerializable(){}
	virtual void       AddRef() = 0;
	virtual void       Release() = 0;
	virtual XmlNodeRef GetXML(IGameStatistics* pGS) = 0;
	virtual void       GetMemoryStatistics(ICrySizer* pSizer) const = 0;

	//! Indirection to allow dispatch typed notifications to callback.
	virtual void DispatchEventToCallback(const SNodeLocator& locator, size_t eventID, const CTimeValue& time, IGameStatisticsCallback* pCallback) = 0;

	//! Indirection to allow dispatch typed notifications to callback.
	virtual void DispatchStateToCallback(const SNodeLocator& locator, size_t stateID, IGameStatisticsCallback* pCallback) = 0;
	// </interfuscator:shuffle>
};

//! Any Stat value.
enum EStatAnyType
{
	eSAT_NONE = 0,

	eSAT_TINT,
	eSAT_TFLOAT,
	eSAT_ENTITY_ID,
	eSAT_TSTRING,
	eSAT_TXML,
	eSAT_THANDLE,

	eSAT_NUM,
};

struct SStatAnyValue
{
	union
	{
		INT_PTR           ptr;
		int64             iNumber;
		float             fNumber;
		EntityId          eidNumber;
		IXMLSerializable* pSerializable;
	};
	string       str;

	EStatAnyType type;

	SStatAnyValue()
		: type(eSAT_NONE)
	{}

	SStatAnyValue(int value)
		: type(eSAT_TINT)
		, iNumber(value)
	{}

	SStatAnyValue(int64 value)
		: type(eSAT_TINT)
		, iNumber(value)
	{}

	SStatAnyValue(float value)
		: type(eSAT_TFLOAT)
		, fNumber(value)
	{}

	SStatAnyValue(EntityId value)
		: type(eSAT_ENTITY_ID)
		, eidNumber(value)
	{}

	SStatAnyValue(const char* value)
		: type(eSAT_TSTRING)
		, str(value)
	{}

	SStatAnyValue(IXMLSerializable* serializable)
		: type(eSAT_TXML)
		, pSerializable(serializable)
	{
		if (pSerializable)
			pSerializable->AddRef();
	}

	~SStatAnyValue()
	{
		if (type == eSAT_TXML && pSerializable)
			pSerializable->Release();
	}

	SStatAnyValue(const SStatAnyValue& other)
		: type(eSAT_NONE)
	{
		*this = other;
	}

	SStatAnyValue(const ScriptAnyValue& value)
	{
		switch (value.GetType())
		{
		case EScriptAnyType::Boolean:
			type = eSAT_TINT;
			iNumber = value.GetBool() ? 1 : 0;
			break;
		case EScriptAnyType::Handle:
			type = eSAT_THANDLE;
			ptr = reinterpret_cast<INT_PTR>(value.GetScriptHandle().ptr);
			break;
		case EScriptAnyType::Number:
			type = eSAT_TFLOAT;
			fNumber = value.GetNumber();
			break;
		case EScriptAnyType::String:
			type = eSAT_TSTRING;
			str = value.GetString();
			break;
		default:
			CRY_ASSERT_MESSAGE(false, "Invalid type for stat value");
			type = eSAT_NONE;
		}
	}

	SStatAnyValue& operator=(const SStatAnyValue& rhs)
	{
		if (type == eSAT_TXML && pSerializable)
		{
			// Reference the same data structure?
			if (rhs.type == eSAT_TXML && pSerializable == rhs.pSerializable)
				return *this;

			pSerializable->Release();
		}

		type = rhs.type;
		switch (type)
		{
		case eSAT_TSTRING:
			str = rhs.str;
			break;
		case eSAT_TINT:
			iNumber = rhs.iNumber;
			break;
		case eSAT_TFLOAT:
			fNumber = rhs.fNumber;
			break;
		case eSAT_ENTITY_ID:
			eidNumber = rhs.eidNumber;
			break;
		case eSAT_TXML:
			pSerializable = rhs.pSerializable;
			if (pSerializable)
				pSerializable->AddRef();
			break;
		case eSAT_THANDLE:
			ptr = rhs.ptr;
			break;
		}

		return *this;
	}

	bool operator==(const SStatAnyValue& rhs) const
	{
		if (type != rhs.type) return false;
		switch (type)
		{
		case eSAT_TSTRING:
			return str == rhs.str;
		case eSAT_TINT:
			return iNumber == rhs.iNumber;
		case eSAT_TFLOAT:
			return fNumber == rhs.fNumber;
		case eSAT_ENTITY_ID:
			return eidNumber == rhs.eidNumber;
		case eSAT_TXML:
			return pSerializable == rhs.pSerializable;
		case eSAT_THANDLE:
			return ptr == rhs.ptr;
		case eSAT_NONE:
			return true;
		default:
			CRY_ASSERT(false);
			return false;
		}
	}

	bool operator!=(const SStatAnyValue& rhs) const
	{
		return !(*this == rhs);
	}

	bool ToString(stack_string& to) const
	{
		switch (type)
		{
		case eSAT_TSTRING:
			to = str;
			return true;
		case eSAT_TINT:
			to.Format("%" PRIi64, iNumber);
			return true;
		case eSAT_THANDLE:
			to.Format("%p", ptr);
			return true;
		case eSAT_TFLOAT:
			to.Format("%f", fNumber);
			return true;
		case eSAT_ENTITY_ID:
			to.Format("%u", eidNumber);
			return true;
		}

		return false;
	}

	bool IsValidType() const
	{
		return type != eSAT_NONE && type != eSAT_NUM;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->Add(*this);
		if (type == eSAT_TSTRING)
			pSizer->Add(str);
		else if (type == eSAT_TXML && pSerializable)
			pSerializable->GetMemoryStatistics(pSizer);
	}
};

//! Element locator.
enum ENodeLocatorType
{
	eNLT_Scope,
	eNLT_EntityID,

	eNLT_Num,
};

//! Uniquely identifies game elements and scopes.
struct SNodeLocator
{
	size_t elemID;         //!< ID of the element (INVALID_STAT_ID if node is scope).
	size_t scopeID;        //!< Scope the element belongs to.
	uint32 locatorType;    //!< Type of identification.
	uint32 locatorValue;   //!< Identifier.
	uint32 timeStamp;      //!< Used to identify nodes over the whole lifetime.

	virtual ~SNodeLocator(){}

	SNodeLocator(const SNodeLocator& other)
		: elemID(other.elemID)
		, scopeID(other.scopeID)
		, locatorType(other.locatorType)
		, locatorValue(other.locatorValue)
		, timeStamp(other.timeStamp)
	{}

	SNodeLocator()
		: elemID(INVALID_STAT_ID)
		, scopeID(INVALID_STAT_ID)
		, locatorType((uint32) - 1)
		, locatorValue(0)
		, timeStamp(0)
	{}

	SNodeLocator(size_t _elemID, size_t _scopeID, uint32 locType, uint32 locValue = 0)
		: elemID(_elemID)
		, scopeID(_scopeID)
		, locatorType(locType)
		, locatorValue(locValue)
		, timeStamp(0)
	{}

	explicit SNodeLocator(size_t _scopeID)
		: elemID(INVALID_STAT_ID)
		, scopeID(_scopeID)
		, locatorType(eNLT_Scope)
		, locatorValue(0)
		, timeStamp(0)
	{}

	SNodeLocator(size_t _elemID, size_t _scopeID, EntityId entityID)
		: elemID(_elemID)
		, scopeID(_scopeID)
		, locatorType(eNLT_EntityID)
		, locatorValue(entityID)
		, timeStamp(0)
	{}

	SNodeLocator& operator=(const SNodeLocator& rhs)
	{
		elemID = rhs.elemID;
		scopeID = rhs.scopeID;
		locatorType = rhs.locatorType;
		locatorValue = rhs.locatorValue;
		timeStamp = rhs.timeStamp;
		return *this;
	}

	bool operator==(const SNodeLocator& rhs) const
	{
		return elemID == rhs.elemID
		       && scopeID == rhs.scopeID
		       && locatorType == rhs.locatorType
		       && locatorValue == rhs.locatorValue;
	}

	bool operator!=(const SNodeLocator& rhs) const
	{
		return !(*this == rhs);
	}

	bool operator<(const SNodeLocator& rhs) const
	{
		if (elemID < rhs.elemID) return true;
		if (elemID != rhs.elemID) return false;
		if (scopeID < rhs.scopeID) return true;
		if (scopeID != rhs.scopeID) return false;
		if (locatorType < rhs.locatorType) return true;
		if (locatorType != rhs.locatorType) return false;
		return locatorValue < rhs.locatorValue;
	}

	bool isScope() const
	{
		return locatorType == eNLT_Scope;
	}

	virtual void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
};

//////////////////////////////////////////////////////////////////////////

struct IGameStatisticsCallback
{
	// <interfuscator:shuffle>
	virtual ~IGameStatisticsCallback(){}

	//! Called when some tracker reports an event.
	virtual void OnEvent(const SNodeLocator& locator, size_t eventID, const CTimeValue& time, const SStatAnyValue& val) = 0;

	//! Called when some tracker reports a state.
	virtual void OnState(const SNodeLocator& locator, size_t stateID, const SStatAnyValue& val) = 0;

	//! Can be used to convert data from the XML to C++ data type to save space.
	virtual void PreprocessScriptedEventParameter(size_t eventID, SStatAnyValue& value) = 0;
	virtual void PreprocessScriptedStateParameter(size_t stateID, SStatAnyValue& value) = 0;

	//! Called when new game element added or scope pushed.
	virtual void OnNodeAdded(const SNodeLocator& locator) = 0;

	//! Called when element is removed or scope popped (all elements are removed when their scope is popped).
	virtual void OnNodeRemoved(const SNodeLocator& locator, IStatsTracker* tracker) = 0;
	// </interfuscator:shuffle>
};

//////////////////////////////////////////////////////////////////////////

//! Storage for event tracks and states.
struct IStatsContainer
{
	// <interfuscator:shuffle>
	virtual ~IStatsContainer(){}
	virtual void   Init(size_t numEvents, size_t numStates) = 0;
	virtual void   AddEvent(size_t eventID, const CTimeValue& time, const SStatAnyValue& val) = 0;
	virtual void   AddState(size_t stateID, const SStatAnyValue& val) = 0;
	virtual size_t GetEventTrackLength(size_t eventID) const = 0;
	virtual void   GetEventInfo(size_t eventID, size_t idx, CTimeValue& outTime, SStatAnyValue& outParam) const = 0;
	virtual void   GetStateInfo(size_t stateID, SStatAnyValue& outValue) const = 0;
	virtual void   Clear() = 0;
	virtual bool   IsEmpty() const = 0;
	virtual void   GetMemoryStatistics(ICrySizer* pSizer) = 0;
	virtual void   AddRef() = 0;
	virtual void   Release() = 0;
	// </interfuscator:shuffle>
};

typedef _smart_ptr<IStatsContainer> IStatsContainerPtr;

//////////////////////////////////////////////////////////////////////////

struct IStatsTracker
{
	// <interfuscator:shuffle>
	virtual ~IStatsTracker() {}

	//! Adds state to the track.
	virtual void StateValue(size_t stateID, const SStatAnyValue& value = SStatAnyValue()) = 0;

	//! Adds time-tamped event to the track.
	virtual void             Event(size_t eventID, const SStatAnyValue& value = SStatAnyValue()) = 0;

	virtual IStatsContainer* GetStatsContainer() = 0;

	virtual void             GetMemoryUsage(ICrySizer* pSizer) const = 0;
	// </interfuscator:shuffle>
};

//////////////////////////////////////////////////////////////////////////

//! Basic implementation of IXMLSerializable.
class CXMLSerializableBase : public IXMLSerializable
{
public:
	CXMLSerializableBase() : m_refCount(0) {}

	virtual ~CXMLSerializableBase() {}

	virtual void AddRef() { ++m_refCount; }

	virtual void Release()
	{
		CRY_ASSERT(m_refCount > 0);
		if (!(--m_refCount))
			delete this;
	}

	//! Override this methods to send typed notifications to the callback.
	virtual void DispatchEventToCallback(const SNodeLocator& locator, size_t eventID, const CTimeValue& time, IGameStatisticsCallback* pCallback)
	{
		pCallback->OnEvent(locator, eventID, time, this);
	}

	virtual void DispatchStateToCallback(const SNodeLocator& locator, size_t stateID, IGameStatisticsCallback* pCallback)
	{
		pCallback->OnState(locator, stateID, this);
	}

	uint32 GetRefCount() const { return m_refCount; }

private:
	uint32 m_refCount;
};

//////////////////////////////////////////////////////////////////////////

struct SGameStatDesc
{
	//! Unique stat ID.
	size_t statID;

	//! Name of the stat used to export it to the script.
	string scriptName;

	//! Name of the stat used for serialization.
	string serializeName;

	SGameStatDesc() : statID(0)
	{}

	SGameStatDesc(size_t _statID, const char* _scriptName, const char* _serializeName)
		: statID(_statID), scriptName(_scriptName), serializeName(_serializeName)
	{}

	bool IsValid() const { return !scriptName.empty() && !serializeName.empty(); }

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(scriptName);
		pSizer->AddObject(serializeName);
	}
};

struct SGameScopeDesc : public SGameStatDesc
{
	//! The name of the scope tracker inside the script.
	string trackerName;

	SGameScopeDesc()
	{}

	SGameScopeDesc(size_t _statID, const char* _scriptName, const char* _serializeName, const char* _trackerName)
		: SGameStatDesc(_statID, _scriptName, _serializeName), trackerName(_trackerName)
	{}

	bool IsValid() const { return SGameStatDesc::IsValid() && !trackerName.empty(); }

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(trackerName);
		SGameStatDesc::GetMemoryUsage(pSizer);
	}
};

struct SGameElementDesc : public SGameScopeDesc
{
	//! ID of the element type specifier.
	uint32 locatorID;

	//! Name of the type specifier used by element (e.g. eELT_EntityID).
	string locatorName;

	SGameElementDesc() {}
	SGameElementDesc(size_t _statID, const char* _scriptName, uint32 _locatorID, const char* _locatorName,
	                 const char* _serializeName, const char* _trackerName)
		: SGameScopeDesc(_statID, _scriptName, _serializeName, _trackerName)
		, locatorID(_locatorID)
		, locatorName(_locatorName)
	{}

	bool IsValid() const { return SGameScopeDesc::IsValid() && !locatorName.empty(); }

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(locatorName);
		SGameScopeDesc::GetMemoryUsage(pSizer);
	}
};

//////////////////////////////////////////////////////////////////////////
#define GAME_STAT_DESC(e, serializedName)                            SGameStatDesc(e, # e, serializedName)
#define GAME_SCOPE_DESC(e, serializedName, trackerName)              SGameScopeDesc(e, # e, serializedName, trackerName)
#define GAME_ELEM_DESC(elemID, locator, serializedName, trackerName) SGameElementDesc(elemID, # elemID, locator, # locator, serializedName, trackerName)
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////

enum EStatNodeState
{
	eSNS_Dead,
	eSNS_Alive,
};

//////////////////////////////////////////////////////////////////////////

struct IStatsSerializer
{
	// <interfuscator:shuffle>
	virtual ~IStatsSerializer(){}
	virtual void VisitNode(const SNodeLocator& locator, const char* serializeName, IStatsContainer& container, EStatNodeState state) = 0;
	virtual void LeaveNode(const SNodeLocator& locator, const char* serializeName, IStatsContainer& container, EStatNodeState state) = 0;
	// </interfuscator:shuffle>
};

//////////////////////////////////////////////////////////////////////////

//! Use factory objects to specify custom implementations of stats containers.
struct IStatsStorageFactory
{
	// <interfuscator:shuffle>
	virtual ~IStatsStorageFactory(){}
	virtual IStatsContainer* CreateContainer() = 0;
	// </interfuscator:shuffle>
};

//////////////////////////////////////////////////////////////////////////

struct IGameStatistics
{
	// <interfuscator:shuffle>
	//! Register multiple game events at once.
	//! \return false on name or id collision.
	virtual bool RegisterGameEvents(const SGameStatDesc* eventDescs, size_t numEvents) = 0;

	//! Register game event and receive unique id (returns INVALID_STAT_ID if failed).
	virtual size_t               RegisterGameEvent(const char* scriptName, const char* serializeName) = 0;

	virtual size_t               GetEventCount() const = 0;

	virtual size_t               GetEventID(const char* scriptName) const = 0;

	virtual size_t               GetEventIDBySerializeName(const char* serializeName) const = 0;

	virtual const SGameStatDesc* GetEventDesc(size_t eventID) const = 0;

	//! Register multiple game states at once.
	virtual bool RegisterGameStates(const SGameStatDesc* stateDescs, size_t numStates) = 0;

	//! Register game state and receive unique id (returns INVALID_STAT_ID if failed).
	virtual size_t               RegisterGameState(const char* scriptName, const char* serializeName) = 0;

	virtual size_t               GetStateCount() const = 0;

	virtual size_t               GetStateID(const char* scriptName) const = 0;

	virtual const SGameStatDesc* GetStateDesc(size_t stateID) const = 0;

	//! Registers multiple game scopes.
	//! \return false on name or id collision.
	virtual bool RegisterGameScopes(const SGameScopeDesc* scopeDescs, size_t numScopes) = 0;

	//! Adds new game scope on top of the stack.
	virtual IStatsTracker* PushGameScope(size_t scopeID) = 0;

	//! Pops the top scope from the stack.
	//! Optional scope parameter allows to verify that expected scope was popped.
	virtual void PopGameScope(size_t scopeID = INVALID_STAT_ID) = 0;

	//! \return Number of scopes on the stack.
	virtual size_t GetScopeStackSize() const = 0;

	//! \return Scope ID of the scope on the specified depth (0 - top of the stack).
	virtual size_t                GetScopeID(size_t depth = 0) const = 0;

	virtual size_t                GetScopeCount() const = 0;

	virtual size_t                GetScopeID(const char* scriptName) const = 0;

	virtual const SGameScopeDesc* GetScopeDesc(size_t scopeID) const = 0;

	//! Registers multiple game elements.
	//! \return false on name or id collision.
	virtual bool RegisterGameElements(const SGameElementDesc* elemDescs, size_t numElems) = 0;

	//! Adds game element to the specified scope (scope should be on the stack).
	//! Script table can be specified where the tracker should be placed.
	virtual IStatsTracker* AddGameElement(const SNodeLocator& locator, IScriptTable* pTable = 0) = 0;

	//! Removes all elements who match predicate from the scope.
	virtual void                    RemoveElement(const SNodeLocator& locator) = 0;

	virtual size_t                  GetElementCount() const = 0;

	virtual size_t                  GetElementID(const char* scriptName) const = 0;

	virtual const SGameElementDesc* GetElementDesc(size_t elemID) const = 0;

	//! Return Tracker of specified game node.
	//! \return Tracker of specified game node.
	virtual IStatsTracker* GetTracker(const SNodeLocator& locator) const = 0;

	//! Returns element locator by tracker.
	//! \return Element locator by tracker.
	virtual SNodeLocator GetTrackedNode(IStatsTracker* tracker) const = 0;

	//! Create XML node to be used as stat parameter.
	//! Use WrapXMLNode() to pass XML as event parameter.
	virtual XmlNodeRef CreateStatXMLNode(const char* tag = "root") = 0;

	//! Wraps XML node to serializable object.
	virtual IXMLSerializable* WrapXMLNode(const XmlNodeRef& node) = 0;

	//! This method (and similar ones in callback) are used to convert parameter data passed from script to another (possibly class inherited from IXMLSerializable) type to simplify the callback logic.
	virtual void PreprocessScriptedEventParameter(size_t eventID, SStatAnyValue& value) = 0;
	virtual void PreprocessScriptedStateParameter(size_t stateID, SStatAnyValue& value) = 0;

	// Callback registration
	virtual void                     SetStatisticsCallback(IGameStatisticsCallback* pCallback) = 0;
	virtual IGameStatisticsCallback* GetStatisticsCallback() const = 0;

	virtual void                     SetStorageFactory(IStatsStorageFactory* pFactory) = 0;
	virtual IStatsStorageFactory*    GetStorageFactory() const = 0;

	virtual void                     SetMemoryLimit(size_t kb) = 0;
	virtual void                     RegisterSerializer(IStatsSerializer* serializer) = 0;
	virtual void                     UnregisterSerializer(IStatsSerializer* serializer) = 0;

	virtual void                     GetMemoryStatistics(ICrySizer* pSizer) const = 0;
	virtual ~IGameStatistics(){}
	// </interfuscator:shuffle>
};

//! \endcond