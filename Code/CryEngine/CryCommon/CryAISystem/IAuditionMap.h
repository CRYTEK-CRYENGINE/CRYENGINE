// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/functor.h>

namespace Perception
{
	static const int kAuditionMapMaxEarsPerListener = 4;
	
	enum class EStimulusObstructionHandling
	{
		// This basically means the obstruction detection is disabled and the stimulus can permeate through everything.
		IgnoreAllObstructions = 0,

		// We will cast rays to all ears on any listener that is in range and the 'strength' of the stimulus will
		// dissipate over distance. When an obstructing surface is hit we check the surface's 'obstruction value'.
		// If it is higher than the remaining 'strength' then the stimulus is considered to be blocked. Otherwise
		// we reduce the 'strength' by the amount of 'obstruction value' and continue.
		RayCastWithLinearFallOff,

		Count
	};
	
	// All the parameters of a single ear with which a listener can pick up sound stimuli.
	struct SListenerEarParams
	{
		SListenerEarParams()
			: worldPos(ZERO)
		{}

		explicit SListenerEarParams(const Vec3& worldPos)
			: worldPos(worldPos)
		{}

		Vec3 worldPos;
	};

	// All the parameters that define a sound stimulus that is created in the world somewhere.
	struct SSoundStimulusParams
	{
		SSoundStimulusParams()
			: position(ZERO)
			, radius(0.0f)
			, sourceEntityId(0)
			, faction(0)
			//, pSoundTags( nullptr )
		{}

		SSoundStimulusParams& operator=(const SSoundStimulusParams& other)
		{
			position = other.position;
			radius = other.radius;
			sourceEntityId = other.sourceEntityId;
			faction = other.faction;
			//pSoundTags = other.pSoundTags;
			obstructionHandling = other.obstructionHandling;
			return *this;
		}

		Vec3                                    position;
		float                                   radius;
		EntityId                                sourceEntityId;
		uint8                                   faction;
		//const Stimulus::SoundStimulusMultiTags* pSoundTags;
		EStimulusObstructionHandling            obstructionHandling;
	};

	// All the parameters of a listener that wants to pick up sound stimuli
	// that managed to reach one of its ears.
	struct SListenerParams
	{
		enum
		{
			MaxEarsCount = kAuditionMapMaxEarsPerListener,
		};

		SListenerParams()
			: listeningDistanceScale(0.0f)
			, factionsToListenMask(0)
		{
		}

		SListenerParams(const SListenerParams& other)
			: listeningDistanceScale(other.listeningDistanceScale)
			, factionsToListenMask(other.factionsToListenMask)
			, onSoundHeardCallback(other.onSoundHeardCallback)
			, ears(other.ears)
		{
		}

		float  listeningDistanceScale;
		uint32 factionsToListenMask;

		StaticDynArray<SListenerEarParams, MaxEarsCount> ears;

		typedef Functor1wRet<const SSoundStimulusParams&, bool> UserConditionCallback;
		UserConditionCallback userConditionCallback;

		typedef Functor1<const SSoundStimulusParams&> SoundHeardCallback;
		SoundHeardCallback onSoundHeardCallback;
	};

	struct ListenerParamsChangeOptions
	{
		enum Value
		{
			ChangedListeningDistanceScale = BIT(0),
			ChangedFactionsToListenMask = BIT(1),
			ChangedOnSoundHeardCallback = BIT(2),
			ChangedEars = BIT(3),
			ChangedAll = 0xffffffff,
		};
	};

	// Parameters of a listener that wants to pick up all sound stimuli in the entire world
	struct SGlobalListenerParams
	{
		SGlobalListenerParams() {}
		SGlobalListenerParams(const SGlobalListenerParams& source)
			: onSoundCompletelyProcessedAndReachedOneOrMoreEars(source.onSoundCompletelyProcessedAndReachedOneOrMoreEars)
		{}

		// This callback is triggered when the stimulus has been completely processed
		// and managed to reach one or more ears.
		// So, if ray casting was required for the stimulus, this event will be delayed
		// until all rays have been completed and processed.
		typedef Functor1<const SSoundStimulusParams&> Callback;
		Callback onSoundCompletelyProcessedAndReachedOneOrMoreEars;
	};

	// Each unique listener instance in the world is identified using this ID.
	struct SListenerInstanceId
	{
		static const uint32 s_invalidId = 0;

		SListenerInstanceId() : m_id(s_invalidId) {}
		operator uint32() const	{ return m_id; };
		bool IsValid() const { return (m_id != s_invalidId); }

	protected:
		friend class CAuditionMap;
		SListenerInstanceId(const uint32 id) : m_id(id) {}

	private:
		uint32 m_id;
	};

} //!Perception

struct SDebugInfo;

// This is the audio equivalent of the VisionMap. It basically tracks 'listeners' in the world that
// get notified when a sound stimulus was within their hearing range.
struct IAuditionMap
{
	typedef Functor3wRet<EntityId, const ray_hit&, float&, bool> SoundObstructionOnHitCallback;
	
	virtual ~IAuditionMap() {}

	virtual bool RegisterListener(EntityId entityId, const Perception::SListenerParams& listenerParams) = 0;
	virtual void UnregisterListner(EntityId entityId) = 0;
	virtual void ListenerChanged(EntityId entityId, const Perception::SListenerParams& newListenerParams, const uint32 changesMask) = 0;

	virtual Perception::SListenerInstanceId RegisterGlobalListener(const Perception::SGlobalListenerParams& globalListenerParams, const char* name) = 0;
	virtual void UnregisterGlobalListener(const Perception::SListenerInstanceId& listenerInstanceId) = 0;

	virtual void OnSoundEvent(const Perception::SSoundStimulusParams& soundParams, const char* szDebugText) = 0;
	virtual void SetSoundObstructionOnHitCallback(const SoundObstructionOnHitCallback& callback) = 0;
};