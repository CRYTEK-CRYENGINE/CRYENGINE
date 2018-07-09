// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ICryPerceptionSystemPlugin.h"
#include <CrySystem/ICryPluginManager.h>

enum EAIStimulusType
{
	AISTIM_SOUND,
	AISTIM_COLLISION,
	AISTIM_EXPLOSION,
	AISTIM_BULLET_WHIZZ,
	AISTIM_BULLET_HIT,
	AISTIM_GRENADE,
	AISTIM_LAST,
};

enum EAIStimProcessFlags : uchar
{
	AISTIMPROC_EMPTY                     = BIT(0),
	AISTIMPROC_FILTER_LINK_WITH_PREVIOUS = BIT(1), //!< Uses the stimulus filtering from prev stim.
	AISTIMPROC_NO_UPDATE_MEMORY          = BIT(2), //!< This won't update the mem target position of the source.
	AISTIMPROC_ONLY_IF_VISIBLE           = BIT(3), //!< This won't allow the stimulus to be processed if the position is not visible.
	//!< It's currently used only by the AISTIM_EXPLOSION.
};

//! Stimulus Filter describes how the stimulus filter works.
//! When a new stimulus is processed and it is within the radius of existing stimulus
//! the new stimulus is discarded. If the merge option is specified, the new stimulus
//! Will be merged into the existing stimulus iff the stimulus time is less than
//! the processDelay of the stimulus type.
//! The type specifies which one type of existing stimulus that will be considered as filter.
//! The subType specifies a mask of all possible subtypes that will be considered as filter.
//! Before the radius is checked, the radius of the existing stimulus is scaled by the scale.
enum EAIStimulusFilterMerge
{
	AISTIMFILTER_DISCARD,                 //!< Discard new stimulus when inside existing stimulus.
	AISTIMFILTER_MERGE_AND_DISCARD,       //!< Merge new stimulus when inside existing stimulus if (and only if) the
	//!< lifetime of the existing stimulus is less than processDelay, else discard.
};

//! Subtypes of the collision stimulus type
enum SAICollisionObjClassification
{
	AICOL_SMALL,
	AICOL_MEDIUM,
	AICOL_LARGE,
};

struct SAIStimulusParams
{
	EAIStimulusType type;
	EntityId        sender;
	Vec3            position;
	float           radius;
	float           threat;
};

struct SAIStimulusFilter
{
	inline void Set(unsigned char t, unsigned char st, EAIStimulusFilterMerge m, float s)
	{
		scale = s;
		merge = m;
		type = t;
		subType = st;
	}

	inline void Reset()
	{
		scale = 0.0f;
		merge = 0;
		type = 0;
		subType = 0;
	}

	float         scale;      // How much the existing stimulus radius is scaled before checking.
	unsigned char merge;      // Filter merge type, see EAIStimulusFilterMerge.
	unsigned char type;       // Stimulus type to match against.
	unsigned char subType;    // Stimulus subtype _mask_ to match against, or 0 if not applied.
};

//! AI Stimulus record.
//! Stimuli are used to tell the perception manager about important events happening in the game world
//! such as sounds or grenades.
//! When the stimulus is processed, it can be merged with previous stimuli in order to prevent
//! too many stimuli to cause too many reactions.
struct SAIStimulus
{
	SAIStimulus(EAIStimulusType type, unsigned char subType, EntityId sourceId, EntityId targetId,
	            const Vec3& pos, const Vec3& dir, float radius, unsigned char flags = 0) :
		sourceId(sourceId), targetId(targetId), pos(pos), dir(dir),
		radius(radius), type(type), subType(subType), flags(EAIStimProcessFlags(flags))
	{
	}

	EntityId            sourceId; //!< The source of the stimulus.
	EntityId            targetId; //!< Optional target of the stimulus.
	Vec3                pos;      //!< Location of the stimulus.
	Vec3                dir;      //!< Optional direction of the stimulus - for now, should be pre-normalized.
	float               radius;   //!< Radius of the stimulus.
	EAIStimulusType     type;     //!< Stimulation type.
	unsigned char       subType;  //!< Stimulation sub-type.
	EAIStimProcessFlags flags;    //!< Processing flags.
};

// Stimulus descriptor.
// Defines a stimulus type and how the incoming stimuli of the specified type should be handled.
//
// Common stimuli defined in EAIStimulusType are already registered in the perception manager.
// For game specific stimuli, the first stim type should be LAST_AISTIM.
//
// Example:
//		desc.Reset();
//		desc.SetName("Collision");
//		desc.processDelay = 0.15f;
//		desc.duration[AICOL_SMALL] = 7.0f;
//		desc.duration[AICOL_MEDIUM] = 7.0f;
//		desc.duration[AICOL_LARGE] = 7.0f;
//		desc.filterTypes = (1<<AISTIM_COLLISION) | (1<<AISTIM_EXPLOSION);
//		desc.nFilters = 2;
//		desc.filters[0].Set(AISTIM_COLLISION, 0, AISTIMFILTER_MERGE_AND_DISCARD, 0.9f); // Merge nearby collisions
//		desc.filters[1].Set(AISTIM_EXPLOSION, 0, AISTIMFILTER_DISCARD, 2.5f); // Discard collision near explosions
//		pPerceptionMgr->RegisterStimulusDesc(AISTIM_COLLISION, desc);
//
struct SAIStimulusTypeDesc
{
	static const uint32 AI_MAX_FILTERS = 4;

	//! The subtype is sometimes converted to a mask and stored in a byte (8bits), no more than 8 subtypes.
	static const uint32 AI_MAX_SUBTYPES = 8;

	inline void         SetName(const char* n)
	{
		assert(strlen(n) < sizeof(name));
		cry_strcpy(name, n);
	}

	inline void Reset()
	{
		processDelay = 0.0f;
		filterTypes = 0;
		for (int i = 0; i < AI_MAX_SUBTYPES; i++)
			duration[i] = 0.0f;
		for (int i = 0; i < AI_MAX_FILTERS; i++)
			filters[i].Reset();
		name[0] = '\0';
		nFilters = 0;
	}

	float             processDelay;              // Delay before the stimulus is actually sent.
	unsigned int      filterTypes;               // Mask of all types of filters contained in the filter list.
	float             duration[AI_MAX_SUBTYPES]; // Duration of the stimulus, accessed by the subType of SAIStimulus.
	SAIStimulusFilter filters[AI_MAX_FILTERS];   // The filter list.
	char              name[32];                  // Name of the stimulus.
	unsigned char     nFilters;                  // Number of filters in the filter list.
};

struct SAIStimulusEventListenerParams
{
	Vec3  pos;
	float radius;
	int   flags;
};

struct IPerceptionManager
{
	typedef Functor1<const SAIStimulusParams&> EventCallback;

	virtual ~IPerceptionManager() {}

	virtual void               RegisterStimulus(const SAIStimulus& stim) = 0;
	virtual void               IgnoreStimulusFrom(EntityId sourceId, EAIStimulusType type, float time) = 0;
	virtual bool               IsPointInRadiusOfStimulus(EAIStimulusType type, const Vec3& pos) const = 0;

	virtual bool               RegisterStimulusDesc(EAIStimulusType type, const SAIStimulusTypeDesc& desc) = 0;

	virtual void               RegisterAIStimulusEventListener(const EventCallback& eventCallback, const SAIStimulusEventListenerParams& params) = 0;
	virtual void               UnregisterAIStimulusEventListener(const EventCallback& eventCallback) = 0;

	virtual void               SetPriorityObjectType(uint16 type) = 0;

	static IPerceptionManager* GetInstance()
	{
		static SCachedInstancePtr cachedPtr;
		return cachedPtr.Get();
	}

private:
	struct SCachedInstancePtr
	{
		SCachedInstancePtr() :
			m_pInstancePtr(nullptr)
		{}

		IPerceptionManager* Get()
		{
			if (!m_pInstancePtr)
			{
				ICryPerceptionSystemPlugin* perceptionPlugin = gEnv->pSystem->GetIPluginManager()->QueryPlugin<ICryPerceptionSystemPlugin>();
				if (perceptionPlugin)
				{
					m_pInstancePtr = &perceptionPlugin->GetPerceptionManager();
				}
			}
			return m_pInstancePtr;
		}
		IPerceptionManager* m_pInstancePtr;
	};
};
