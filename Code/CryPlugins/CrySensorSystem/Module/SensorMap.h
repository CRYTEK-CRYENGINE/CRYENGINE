// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Containers/VectorMap.h>

#include "../Interface/ISensorMap.h"

#include "OctreePlotter.h"

namespace Cry
{
	namespace SensorSystem
	{
		// Forward declare interfaces;
		struct ISensorTagLibrary;

		class CSensorMap : public ISensorMap
		{
		private:

			struct SCell;

			struct SVolume
			{
				SVolume();

				SensorVolumeId      id;
				SensorVolumeId      prevId;
				SensorVolumeId      nextId;
				SCell*              pCell;
				SSensorVolumeParams params;
				SensorVolumeIdSet   cache;
			};

			typedef std::vector<SVolume> Volumes;
			typedef std::vector<uint32>  FreeVolumes;

			struct SCell
			{
				SCell();

				SensorVolumeId firstVolumeId;
				SensorVolumeId lastVolumeId;
			};

			typedef std::vector<SCell>            Cells;
			typedef VectorMap<uint32, SensorTags> DirtyCells;
			typedef std::vector<SSensorEvent>     Events;

			struct SUpdateStats
			{
				SUpdateStats();

				uint32 queryCount;
				float  time;
			};

		public:

			CSensorMap(ISensorTagLibrary& tagLibrary, const SSensorMapParams& params);

			// ISensorMap

			virtual SensorVolumeId      CreateVolume(const SSensorVolumeParams& params) override;
			virtual void                DestroyVolume(SensorVolumeId volumeId) override;
			virtual void                UpdateVolumeBounds(SensorVolumeId volumeId, const CSensorBounds& bounds) override;
			virtual void                SetVolumeAttributeTags(SensorVolumeId volumeId, const SensorTags& attributeTags, bool bValue) override;
			virtual void                SetVolumeListenerTags(SensorVolumeId volumeId, const SensorTags& listenerTags, bool bValue) override;
			virtual SSensorVolumeParams GetVolumeParams(SensorVolumeId volumeId) const override;

			virtual void                Query(SensorVolumeIdSet& results, const CSensorBounds& bounds, const SensorTags& tags = SensorTags(), SensorVolumeId exclusionId = SensorVolumeId::Invalid) const override;

			virtual void                Update() override;
			virtual void                Debug(float drawRange, const SensorMapDebugFlags& flags) override;

			virtual void                SetOctreeDepth(uint32 depth) override;
			virtual void                SetOctreeBounds(const AABB& bounds) override;

			// ~ISensorMap

		private:

			bool           AllocateVolumes(uint32 volumeCount);
			SensorVolumeId CreateVolumeId(uint32 volumeIdx, uint32 salt) const;
			SVolume*       GetVolume(SensorVolumeId volumeId);
			const SVolume* GetVolume(SensorVolumeId volumeId) const;

			bool           ExpandVolumeId(SensorVolumeId volumeId, uint32& volumeIdx, uint32& salt) const;

			uint32         GetCellIdx(const SCell* pCell) const;
			void           MarkCellsDirty(const AABB& bounds, const SensorTags& tags);
			void           MapVolumeToCell(SVolume& volume);
			void           RemoveVolumeFromCurrentCell(SVolume& volume);

			void           Query(SensorVolumeId volumeId, Events& events);

			void           Remap();

		private:

			void DebugDrawStats() const;
			void DebugDrawVolumesAndOctree(float drawRange, const SensorMapDebugFlags& flags) const;
			void DebugInteractive(float drawRange) const;

		private:

			ISensorTagLibrary& m_tagLibrary;

			Volumes            m_volumes;
			FreeVolumes        m_freeVolumes;
			SensorVolumeIdSet  m_activeVolumes;
			SensorVolumeIdSet  m_strayVolumes;

			COctreePlotter     m_octreePlotter;
			Cells              m_cells;
			DirtyCells         m_dirtyCells;

			SUpdateStats       m_updateStats;

			SensorVolumeIdSet  m_pendingQueries;
			SensorVolumeIdSet  m_queryResults;
		};
	}
}