// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Containers/VectorSet.h>
#include <CrySerialization/Forward.h>

#include "ISensorTagLibrary.h"
#include "SensorBounds.h"

namespace Cry
{
	namespace SensorSystem
	{
		struct SSensorMapParams
		{
			inline SSensorMapParams()
				: volumeCount(0)
				, octreeDepth(0)
				, octreeBounds(ZERO)
			{}

			uint32 volumeCount;
			uint32 octreeDepth;
			AABB   octreeBounds;
		};

		enum class SensorVolumeId : uint32
		{
			Invalid = 0xffffffff
		};

		inline bool Serialize(Serialization::IArchive& archive, SensorVolumeId& value, const char* szName, const char* szLabel)
		{
			if (!archive.isEdit())
			{
				return archive(static_cast<uint32>(value), szName, szLabel);
			}
			return true;
		}

		typedef VectorSet<SensorVolumeId> SensorVolumeIdSet;

		enum class ESensorEventType
		{
			Entering,
			Leaving
		};

		struct SSensorEvent
		{
			inline SSensorEvent(ESensorEventType _type, SensorVolumeId _listenerVolumeId, SensorVolumeId _eventVolumeId)
				: type(_type)
				, listenerVolumeId(_listenerVolumeId)
				, eventVolumeId(_eventVolumeId)
			{}

			ESensorEventType type;
			SensorVolumeId   listenerVolumeId;
			SensorVolumeId   eventVolumeId;
		};

		typedef std::function<void(const SSensorEvent&)> SensorEventListener;

		struct SSensorVolumeParams
		{
			inline SSensorVolumeParams()
				: entityId(INVALID_ENTITYID)
			{}

			inline SSensorVolumeParams(EntityId _entityId, const CSensorBounds& _bounds, const SensorTags& _attributeTags, const SensorTags& _listenerTags = SensorTags(), const SensorEventListener& _eventListener = SensorEventListener())
				: entityId(_entityId)
				, bounds(_bounds)
				, attributeTags(_attributeTags)
				, listenerTags(_listenerTags)
				, eventListener(_eventListener)
			{}

			EntityId            entityId;
			CSensorBounds       bounds;
			SensorTags          attributeTags;
			SensorTags          listenerTags;
			SensorEventListener eventListener;
		};

		enum class ESensorMapDebugFlags
		{
			DrawStats = BIT(0),
			DrawVolumes = BIT(1),
			DrawVolumeEntities = BIT(2),
			DrawVolumeTags = BIT(3),
			DrawOctree = BIT(4),
			DrawStrayVolumes = BIT(5),
			Interactive = BIT(6)
		};

		typedef CEnumFlags<ESensorMapDebugFlags> SensorMapDebugFlags;

		struct ISensorMap
		{
			virtual ~ISensorMap() {}

			virtual SensorVolumeId      CreateVolume(const SSensorVolumeParams& params) = 0;
			virtual void                DestroyVolume(SensorVolumeId volumeId) = 0;
			virtual void                UpdateVolumeBounds(SensorVolumeId volumeId, const CSensorBounds& bounds) = 0;
			virtual void                SetVolumeAttributeTags(SensorVolumeId volumeId, const SensorTags& attributeTags, bool bValue) = 0;
			virtual void                SetVolumeListenerTags(SensorVolumeId volumeId, const SensorTags& listenerTags, bool bValue) = 0;
			virtual SSensorVolumeParams GetVolumeParams(SensorVolumeId volumeId) const = 0;

			virtual void                Query(SensorVolumeIdSet& results, const CSensorBounds& bounds, const SensorTags& tags = SensorTags(), SensorVolumeId exclusionId = SensorVolumeId::Invalid) const = 0;

			virtual void                Update() = 0;
			virtual void                Debug(float drawRange, const SensorMapDebugFlags& flags) = 0;

			virtual void                SetOctreeDepth(uint32 depth) = 0;
			virtual void                SetOctreeBounds(const AABB& bounds) = 0;
		};
	}
}