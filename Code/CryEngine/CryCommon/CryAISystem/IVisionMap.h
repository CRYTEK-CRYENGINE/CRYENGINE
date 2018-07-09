// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IVisionMap_h__
#define __IVisionMap_h__

#pragma once

#include <CryPhysics/physinterface.h>

//! This pierceability level (10) will still allow AI to see through leaves and such.
#define AI_VISION_RAY_CAST_PIERCEABILITY (10)

//! These flags will prevent ray casting through everything that is solid, such as walls and the terrain.
#define AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER ((geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (AI_VISION_RAY_CAST_PIERCEABILITY & rwi_pierceability_mask))

//! These flags will prevent ray casting through 'soft cover' colliders.
//! These are colliders that a character can still pass through but should block any vision. Examples of these can be: bushes, tall grass, etc.
#define AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOFT_COVER (geom_colltype14 << rwi_colltype_bit)

#define COVER_OBJECT_TYPES                            (ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid)

#define VISIONMAP_DEFAULT_RAYCAST_FLAGS               (AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER)

enum EChangeHint
{
	eChangedPosition              = 1 << 0,
	eChangedFactionsToObserveMask = 1 << 1,
	eChangedTypesToObserveMask    = 1 << 2,
	eChangedSightRange            = 1 << 3,
	eChangedFaction               = 1 << 4,
	eChangedTypeMask              = 1 << 5,
	eChangedCallback              = 1 << 6,
	eChangedUserData              = 1 << 7,
	eChangedSkipList              = 1 << 8,
	eChangedOrientation           = 1 << 9,
	eChangedFOV                   = 1 << 10,
	eChangedRaycastFlags          = 1 << 11,
	eChangedEntityId              = 1 << 12,
	eChangedUserCondition         = 1 << 13,
	eChangedUserConditionCallback = 1 << 14,
	eChangedObservableMode        = 1 << 15,
	eChangedAll                   = 0xffffffff,
};

enum EVisionPriority
{
	eLowPriority      = 0,
	eMediumPriority   = 1,
	eHighPriority     = 2,
	eVeryHighPriority = 3,
};

enum class EObservableMode
{
	Default = 0,
	Statistical,
	Count            
};

#define VISION_MAP_ALL_TYPES    uint32(-1)
#define VISION_MAP_ALL_FACTIONS uint32(-1)

struct PriorityMapEntry
{
	PriorityMapEntry()
		: fromTypeMask(VISION_MAP_ALL_TYPES)
		, fromFactionMask(VISION_MAP_ALL_FACTIONS)
		, toTypeMask(VISION_MAP_ALL_TYPES)
		, toFactionMask(VISION_MAP_ALL_FACTIONS)
		, priority(eMediumPriority) {};

	uint32          fromTypeMask;
	uint32          fromFactionMask;
	uint32          toTypeMask;
	uint32          toFactionMask;
	EVisionPriority priority;
};

#ifndef _RELEASE
	#define VISION_MAP_STORE_DEBUG_NAMES_FOR_VISION_ID
#endif

struct VisionID
{
	VisionID()
		: m_id(0) {};

	operator uint32() const
	{
		return m_id;
	};

	friend class CVisionMap;

private:
	uint32 m_id;

#ifndef VISION_MAP_STORE_DEBUG_NAMES_FOR_VISION_ID
	VisionID(uint32 id, const char* name)
		: m_id(id) {};
#else
	VisionID(uint32 id, const char* name)
		: m_id(id)
		, m_debugName(name) {};

	string m_debugName;
#endif
};

struct ObservableParams;

struct ObserverParams
{
	enum
	{
		MaxSkipListSize = 32,
	};

	ObserverParams()
		: callback(0)
		, userData(0)
		, userConditionCallback(0)
		, skipListSize(0)
		, faction(0xff)
		, typeMask(0)
		, factionsToObserveMask(0)
		, typesToObserveMask(0)
		, sightRange(50.0f)
		, fovCos(-1.0f)
		, eyePosition(ZERO)
		, eyeDirection(ZERO)
		, updatePeriod(0.3f)
		, entityId(0)
		, raycastFlags(VISIONMAP_DEFAULT_RAYCAST_FLAGS)
	{
	}

	//! Callbacks for when the observer's visibility status changes.
	typedef Functor5<const VisionID&, const ObserverParams&, const VisionID&, const ObservableParams&, bool> Callback;
	Callback callback;
	void*    userData;

	//! Registered observer can perform some extra game-specific visibility checks in this optional callback
	//! and answer, whether the observable is *potentially* visible.
	typedef Functor4wRet<const VisionID&, const ObserverParams&, const VisionID&, const ObservableParams&, bool> UserConditionCallback;
	UserConditionCallback userConditionCallback;

	//! Physics entities to ignore when ray casting.
	uint8            skipListSize;
	IPhysicalEntity* skipList[MaxSkipListSize];

	//! The type mask and faction are used for filtering and determining the priority of the ray casts.
	uint8  faction;
	uint32 typeMask;

	uint32 factionsToObserveMask;
	uint32 typesToObserveMask;

	//! A sight range value of zero or lower will be handled as unlimited range.
	float      sightRange;
	float      fovCos;

	Vec3       eyePosition;
	Vec3       eyeDirection;

	CTimeValue updatePeriod;

	//! An optional entityId to be associated with this observer.
	//! Its purpose is to help the the users identify the entities involved on a change of visibility.
	//! Not to be used internally in the vision map.
	EntityId entityId;

	//! Flags to be set for raycast.
	uint32 raycastFlags;
};

struct ObservableParams
{
	enum
	{
		MaxPositionCount = 6,
	};

	enum
	{
		MaxSkipListSize = 32,
	};

	ObservableParams()
		: callback(0)
		, userData(0)
		, typeMask(0)
		, faction(0)
		, observablePositionsCount(0)
		, skipListSize(0)
		, entityId(0)
		, mode(EObservableMode::Default)
		, collectFullStatisticsOnObservableMaxRange(1.0f)

	{
		memset(observablePositions, 0, sizeof(observablePositions));
	}

	//! Callbacks for when the observable's visibility status changes.
	typedef Functor5<const VisionID&, const ObserverParams&, const VisionID&, const ObservableParams&, bool> Callback;
	Callback callback;
	void*    userData;

	uint32   typeMask;
	uint8    faction;

	//! Observable can have multiple test points.
	uint8 observablePositionsCount;
	Vec3  observablePositions[MaxPositionCount];

	//! Physics entities to ignore when ray casting.
	uint8            skipListSize;
	IPhysicalEntity* skipList[MaxSkipListSize];

	//! An optional entityId to be associated with this observable.
	//! Its purpose is to help the the users identify the entities involved on a change of visibility.
	//! Not to be used internally in the vision map.
	EntityId entityId;

	//! Define the way in which an observer is evaluated (seen/not seen or percentage of points that are seen)
	EObservableMode mode;
	float collectFullStatisticsOnObservableMaxRange;
};

typedef VisionID ObserverID;
typedef VisionID ObservableID;

class IVisionMap
{
public:
	// <interfuscator:shuffle>
	virtual ~IVisionMap() {}

	virtual void                    Reset() = 0;
	virtual VisionID                CreateVisionID(const char* name) = 0;

	virtual void                    RegisterObserver(const ObserverID& observerID, const ObserverParams& params) = 0;
	virtual void                    UnregisterObserver(const ObserverID& observerID) = 0;

	virtual void                    RegisterObservable(const ObservableID& observableID, const ObservableParams& params) = 0;
	virtual void                    UnregisterObservable(const ObservableID& observableID) = 0;

	virtual void                    ObserverChanged(const ObserverID& observerID, const ObserverParams& params, uint32 hint) = 0;
	virtual void                    ObservableChanged(const ObservableID& observableID, const ObservableParams& params, uint32 hint) = 0;

	virtual void                    AddPriorityMapEntry(const PriorityMapEntry& priorityMapEntry) = 0;
	virtual void                    ClearPriorityMap() = 0;

	virtual bool                    IsVisible(const ObserverID& observerID, const ObservableID& observableID) const = 0;
	virtual float                   GetNormalizedVisibiliy(const ObserverID& observerID, const ObservableID& observableID) const = 0;

	virtual const ObserverParams*   GetObserverParams(const ObserverID& observerID) const = 0;
	virtual const ObservableParams* GetObservableParams(const ObservableID& observableID) const = 0;

	virtual void                    Update(float frameTime) = 0;
	// </interfuscator:shuffle>
};

struct VisionMapHelpers
{
	template<class T>
	static bool AddEntityToParamsSkipList(const IEntity& entity, T& params, bool includeParentEntities = true)
	{
		if (params.skipListSize >= T::MaxSkipListSize)
			return false;

		if (IPhysicalEntity* physicalEntity = entity.GetPhysics())
		{
			params.skipList[params.skipListSize] = physicalEntity;
			++params.skipListSize;
		}

		if (includeParentEntities)
		{
			IEntity* parentEntity = entity.GetParent();
			while (parentEntity)
			{
				if (params.skipListSize >= T::MaxSkipListSize)
					return false;

				if (IPhysicalEntity* physicalEntity = parentEntity->GetPhysics())
				{
					params.skipList[params.skipListSize] = physicalEntity;
					++params.skipListSize;
				}

				parentEntity = parentEntity->GetParent();
			}
		}

		return true;
	}
};

#endif // __IVisionMap_h__
