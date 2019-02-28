// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BaseEnv/Utils/BaseEnv_SpatialIndex.h"

#include <CryRenderer/IRenderAuxGeom.h>

SERIALIZATION_ENUM_BEGIN_NESTED(SchematycBaseEnv, ESpatialVolumeShape, "Spatial volume shape")
	SERIALIZATION_ENUM(SchematycBaseEnv::ESpatialVolumeShape::Box, "Box", "Box")
	SERIALIZATION_ENUM(SchematycBaseEnv::ESpatialVolumeShape::Sphere, "Sphere", "Sphere")
	SERIALIZATION_ENUM(SchematycBaseEnv::ESpatialVolumeShape::Point, "Point", "Point")
SERIALIZATION_ENUM_END()

namespace SchematycBaseEnv
{
	const uint32 CSpatialIndex::s_volumeIdxMask   = 0x0000ffff;
	const uint32 CSpatialIndex::s_volumeSaltShift = 16;
	const uint32 CSpatialIndex::s_volumeSaltMask  = 0xffff0000;

	CSpatialVolumeBounds::CSpatialVolumeBounds()
		: m_shape(ESpatialVolumeShape::None)
	{}

	CSpatialVolumeBounds::CSpatialVolumeBounds(const OBB& rhs)
		: m_shape(ESpatialVolumeShape::Box)
	{
		new (m_storage) OBB(rhs);
	}

	CSpatialVolumeBounds::CSpatialVolumeBounds(const Sphere& rhs)
		: m_shape(ESpatialVolumeShape::Sphere)
	{
		new (m_storage) Sphere(rhs);
	}

	CSpatialVolumeBounds::CSpatialVolumeBounds(const Vec3& rhs)
		: m_shape(ESpatialVolumeShape::Point)
	{
		new (m_storage) Vec3(rhs);
	}

	CSpatialVolumeBounds::CSpatialVolumeBounds(const CSpatialVolumeBounds& rhs)
		: m_shape(rhs.m_shape)
	{
		Copy(rhs);
	}

	CSpatialVolumeBounds::~CSpatialVolumeBounds()
	{
		Release();
	}

	CSpatialVolumeBounds CSpatialVolumeBounds::CreateOBB(const Matrix34& volumeTM, const Vec3& pos, const Vec3& size, const Matrix33& rot)
	{
		const QuatT transform = QuatT(volumeTM) * QuatT(Quat(rot), pos);
		return CSpatialVolumeBounds(OBB::CreateOBB(Matrix33(transform.q), size*0.5f, transform.q.GetInverted() * transform.t));
	}

	CSpatialVolumeBounds CSpatialVolumeBounds::CreateSphere(const Matrix34& volumeTM, const Vec3& pos, float radius)
	{
		return CSpatialVolumeBounds(Sphere(volumeTM.TransformPoint(pos), radius));
	}

	CSpatialVolumeBounds CSpatialVolumeBounds::CreatePoint(const Matrix34& volumeTM, const Vec3& pos)
	{
		return CSpatialVolumeBounds(volumeTM.TransformPoint(pos));
	}

	ESpatialVolumeShape CSpatialVolumeBounds::GetShape() const
	{
		return m_shape;
	}

	const OBB& CSpatialVolumeBounds::AsBox() const
	{
		CRY_ASSERT(m_shape == ESpatialVolumeShape::Box);
		return *reinterpret_cast<const OBB*>(m_storage);
	}

	const Sphere& CSpatialVolumeBounds::AsSphere() const
	{
		CRY_ASSERT(m_shape == ESpatialVolumeShape::Sphere);
		return *reinterpret_cast<const Sphere*>(m_storage);
	}

	const Vec3& CSpatialVolumeBounds::AsPoint() const
	{
		CRY_ASSERT(m_shape == ESpatialVolumeShape::Point);
		return *reinterpret_cast<const Vec3*>(m_storage);
	}

	AABB CSpatialVolumeBounds::CalculateAABB() const
	{
		switch(m_shape)
		{
		case ESpatialVolumeShape::Box:
			{
				return AABB::CreateAABBfromOBB(Vec3(ZERO), AsBox());
			}
		case ESpatialVolumeShape::Sphere:
			{
				return AABB(AsSphere().center, AsSphere().radius);
			}
		case ESpatialVolumeShape::Point:
			{
				return AABB(AsPoint(), AsPoint());
			}
		}
		return AABB(0.0f);
	}

	bool CSpatialVolumeBounds::Overlap(const CSpatialVolumeBounds& rhs) const
	{
		switch(m_shape)
		{
		case ESpatialVolumeShape::Box:
			{
				switch(rhs.m_shape)
				{
				case ESpatialVolumeShape::Box:
					{
						return Overlap::OBB_OBB(Vec3(ZERO), AsBox(), Vec3(ZERO), rhs.AsBox());
					}
				case ESpatialVolumeShape::Sphere:
					{
						return Overlap::Sphere_OBB(rhs.AsSphere(), AsBox());
					}
				case ESpatialVolumeShape::Point:
					{
						return Overlap::Point_OBB(rhs.AsPoint(), Vec3(ZERO), AsBox());
					}
				}
				break;
			}
		case ESpatialVolumeShape::Sphere:
			{
				switch(rhs.m_shape)
				{
				case ESpatialVolumeShape::Box:
					{
						return Overlap::Sphere_OBB(AsSphere(), rhs.AsBox());
					}
				case ESpatialVolumeShape::Sphere:
					{
						return Overlap::Sphere_Sphere(AsSphere(), rhs.AsSphere());
					}
				case ESpatialVolumeShape::Point:
					{
						return Overlap::Point_Sphere(rhs.AsPoint(), AsSphere());
					}
				}
				break;
			}
		case ESpatialVolumeShape::Point:
			{
				switch(rhs.m_shape)
				{
				case ESpatialVolumeShape::Box:
					{
						return Overlap::Point_OBB(AsPoint(), Vec3(ZERO), rhs.AsBox());
					}
				case ESpatialVolumeShape::Sphere:
					{
						return Overlap::Point_Sphere(AsPoint(), rhs.AsSphere());
					}
				// N.B. No case for ESpatialVolumeShape::Point because we should never need to detect overlapping points.
				}
				break;
			}
		}
		return false;
	}

	void CSpatialVolumeBounds::operator = (const CSpatialVolumeBounds& rhs)
	{
		Release();
		Copy(rhs);
	}

	void CSpatialVolumeBounds::Copy(const CSpatialVolumeBounds& rhs)
	{
		m_shape = rhs.m_shape;
		switch(rhs.m_shape)
		{
		case ESpatialVolumeShape::Box:
			{
				new (m_storage) OBB(rhs.AsBox());
				break;
			}
		case ESpatialVolumeShape::Sphere:
			{
				new (m_storage) Sphere(rhs.AsSphere());
				break;
			}
		case ESpatialVolumeShape::Point:
			{
				new (m_storage) Vec3(rhs.AsPoint());
				break;
			}
		}
	}

	void CSpatialVolumeBounds::Release()
	{
		switch(m_shape)
		{
		case ESpatialVolumeShape::Box:
			{
				reinterpret_cast<const OBB*>(m_storage)->~OBB();
				break;
			}
		case ESpatialVolumeShape::Sphere:
			{
				reinterpret_cast<const Sphere*>(m_storage)->~Sphere();
				break;
			}
		case ESpatialVolumeShape::Point:
			{
				reinterpret_cast<const Vec3*>(m_storage)->~Vec3();
				break;
			}
		}
	}

	SSpatialVolumeTagName::SSpatialVolumeTagName() {}

	SSpatialVolumeTagName::SSpatialVolumeTagName(const string& _value)
		: value(_value)
	{}

	bool SSpatialVolumeTagName::operator == (const SSpatialVolumeTagName& rhs) const
	{
		return value == rhs.value;
	}

	bool Serialize(Serialization::IArchive& archive, SSpatialVolumeTagName& value, const char* szName, const char* szLabel)
	{
		/*const Serialization::StringList& tagNames = g_pGame->GetSchematycEnv().GetEntityDetectionVolumeSystem().GetTagNames();
		if(archive.isInput())
		{
			Serialization::StringListValue temp(tagNames, 0);
			archive(temp, szName, szLabel);
			value.value = temp.c_str();
		}
		else if(archive.isOutput())
		{
			const int pos = tagNames.find(value.value.c_str());
			archive(Serialization::StringListValue(tagNames, pos), szName, szLabel);
		}*/
		return true;
	}

	SSpatialVolumeParams::SSpatialVolumeParams()
		: tags(ESpatialVolumeTags::None)
		, monitorTags(ESpatialVolumeTags::None)
		, entityId(0)
	{}

	SSpatialVolumeParams::SSpatialVolumeParams(const CSpatialVolumeBounds& _bounds, ESpatialVolumeTags _tags, ESpatialVolumeTags _monitorTags, EntityId _entityId)
		: bounds(_bounds)
		, tags(_tags)
		, monitorTags(_monitorTags)
		, entityId(_entityId)
	{}

	void CSpatialIndex::SSettings::Serialize(Serialization::IArchive& archive)
	{
		archive(userTags, "userTags", "+User Tags");
		if(archive.isInput())
		{
			RefreshTags();
		}
	}

	void CSpatialIndex::SSettings::RefreshTags()
	{
		tagNames.clear();
		tagValues.clear();

		const Serialization::EnumDescription& tagsEnumDescription = Serialization::getEnumDescription<ESpatialVolumeTags>();
		UserTags::const_iterator              itUserTag = userTags.begin();
		UserTags::const_iterator              itEndUserTag = userTags.end();
		for(int tagValue = static_cast<int>(ESpatialVolumeTags::Begin); tagValue != static_cast<int>(ESpatialVolumeTags::End); tagValue <<= 1)
		{
			const int tagIdx = tagsEnumDescription.indexByValue(tagValue);
			if(tagIdx != -1)
			{
				tagNames.push_back(tagsEnumDescription.labelByIndex(tagIdx));
				tagValues.push_back(tagValue);
			}
			else if(itUserTag != itEndUserTag)
			{
				tagNames.push_back(*itUserTag);
				tagValues.push_back(tagValue);
				++ itUserTag;
			}
		}
	}

	CSpatialIndex::SVolume::SVolume()
		: monitorTags(ESpatialVolumeTags::None)
		, entityId(0)
	{}

	CSpatialIndex::SBroadPhaseVolume::SBroadPhaseVolume()
		: tags(ESpatialVolumeTags::None)
	{}

	CSpatialIndex::CSpatialIndex()
		: m_pSettings(new SSettings())
		, m_salt(0)
	{
		m_pSettings->RefreshTags();
		gEnv->pSchematyc2->GetEnvRegistry().RegisterSettings("spatial_index_settings", m_pSettings);

		m_volumes.reserve(256);
		m_broadPhaseVolumes.reserve(256);
		m_freeVolumes.reserve(64);
		m_broadPhaseResults.reserve(64);
		m_monitorResults.reserve(16);
		m_eventQueue.reserve(64);
	}

	SpatialVolumeId CSpatialIndex::CreateVolume(const SSpatialVolumeParams& params, const SpatialIndexEventCallback& eventCallback)
	{
		uint32 volumeIdx;
		if(m_freeVolumes.empty())
		{
			const uint32 volumeCount = m_volumes.size();
			if(volumeCount > s_volumeIdxMask)
			{
				return SpatialVolumeId();
			}
			else
			{
				volumeIdx = volumeCount;
				m_volumes.push_back(SVolume());
				m_broadPhaseVolumes.push_back(SBroadPhaseVolume());
			}
		}
		else
		{
			volumeIdx = m_freeVolumes.back();
			m_freeVolumes.pop_back();
		}

		SVolume& volume = m_volumes[volumeIdx];
		volume.bounds        = params.bounds;
		volume.monitorTags   = params.monitorTags;
		volume.id            = GenerateVolumeId(volumeIdx);
		volume.entityId      = params.entityId;
		volume.eventCallback = eventCallback;

		volume.monitorCache.clear();
		if(params.monitorTags != ESpatialVolumeTags::None)
		{
			volume.monitorCache.reserve(16);
		}

		SBroadPhaseVolume& broadPhaseVolume = m_broadPhaseVolumes[volumeIdx];
		broadPhaseVolume.tags = params.tags;
		broadPhaseVolume.aabb = params.bounds.CalculateAABB();

		return volume.id;
	}

	void CSpatialIndex::DestroyVolume(const SpatialVolumeId& id)
	{
		if(id != SpatialVolumeId::s_invalid)
		{
			const uint32 volumeCount = m_volumes.size();
			const uint32 volumeIdx = VolumeIdxFromId(id);
			CRY_ASSERT(volumeIdx < volumeCount);
			if(volumeIdx < volumeCount)
			{
				SVolume& volume = m_volumes[volumeIdx];
				CRY_ASSERT(volume.id == id);
				if(volume.id == id)
				{
					volume.bounds                       = CSpatialVolumeBounds();
					volume.monitorTags                  = ESpatialVolumeTags::None;
					volume.eventCallback                = SpatialIndexEventCallback();
					m_broadPhaseVolumes[volumeIdx].tags = ESpatialVolumeTags::None;
					m_freeVolumes.push_back(volumeIdx);
				}
			}
		}
	}

	void CSpatialIndex::UpdateVolumeBounds(const SpatialVolumeId& id, const CSpatialVolumeBounds& bounds)
	{
		if(id != SpatialVolumeId::s_invalid)
		{
			CRY_PROFILE_FUNCTION(PROFILE_GAME);

			const uint32 volumeCount = m_volumes.size();
			const uint32 volumeIdx = VolumeIdxFromId(id);
			CRY_ASSERT(volumeIdx < volumeCount);
			if(volumeIdx < volumeCount)
			{
				SVolume& volume = m_volumes[volumeIdx];
				CRY_ASSERT(volume.id == id);
				if(volume.id == id)
				{
					volume.bounds                       = bounds;
					m_broadPhaseVolumes[volumeIdx].aabb = bounds.CalculateAABB();
				}
			}
		}
	}

	void CSpatialIndex::SetVolumeTags(const SpatialVolumeId& id, ESpatialVolumeTags tags, bool bValue)
	{
		if(id != SpatialVolumeId::s_invalid)
		{
			const uint32 volumeCount = m_volumes.size();
			const uint32 volumeIdx = VolumeIdxFromId(id);
			CRY_ASSERT(volumeIdx < volumeCount);
			if(volumeIdx < volumeCount)
			{
				SVolume& volume = m_volumes[volumeIdx];
				CRY_ASSERT(volume.id == id);
				if(volume.id == id)
				{
					if(bValue)
					{
						m_broadPhaseVolumes[volumeIdx].tags |= tags;
					}
					else
					{
						m_broadPhaseVolumes[volumeIdx].tags &= ~tags;
					}
				}
			}
		}
	}

	void CSpatialIndex::SetVolumeMonitorTags(const SpatialVolumeId& id, ESpatialVolumeTags tags, bool bValue)
	{
		if(id != SpatialVolumeId::s_invalid)
		{
			const uint32 volumeCount = m_volumes.size();
			const uint32 volumeIdx = VolumeIdxFromId(id);
			CRY_ASSERT(volumeIdx < volumeCount);
			if(volumeIdx < volumeCount)
			{
				SVolume& volume = m_volumes[volumeIdx];
				CRY_ASSERT(volume.id == id);
				if(volume.id == id)
				{
					if(bValue)
					{
						volume.monitorTags |= tags;
					}
					else
					{
						volume.monitorTags &= ~tags;
					}
				}
			}
		}
	}

	SSpatialVolumeParams CSpatialIndex::GetVolumeParams(const SpatialVolumeId& id) const
	{
		if(id != SpatialVolumeId::s_invalid)
		{
			const uint32 volumeCount = m_volumes.size();
			const uint32 volumeIdx = VolumeIdxFromId(id);
			CRY_ASSERT(volumeIdx < volumeCount);
			if(volumeIdx < volumeCount)
			{
				const SVolume& volume = m_volumes[volumeIdx];
				CRY_ASSERT(volume.id == id);
				if(volume.id == id)
				{
					SSpatialVolumeParams volumeParams;
					volumeParams.bounds      = volume.bounds;
					volumeParams.tags        = m_broadPhaseVolumes[volumeIdx].tags;
					volumeParams.monitorTags = volume.monitorTags;
					volumeParams.entityId    = volume.entityId;
					return volumeParams;
				}
			}
		}
		return SSpatialVolumeParams();
	}

	void CSpatialIndex::Query(const SpatialVolumeId& id, ESpatialVolumeTags tags, SpatialVolumeIds& results)
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		if(id != SpatialVolumeId::s_invalid)
		{
			const uint32 volumeCount = m_volumes.size();
			const uint32 volumeIdx = VolumeIdxFromId(id);
			CRY_ASSERT(volumeIdx < volumeCount);
			if(volumeIdx < volumeCount)
			{
				SVolume& volume = m_volumes[volumeIdx];
				CRY_ASSERT(volume.id == id);
				if(volume.id == id)
				{
					// Perform broad phase query.
					m_broadPhaseResults.clear();
					BroadPhaseQuery(volumeIdx, tags, m_broadPhaseResults);
					// Perform narrow phase tests.
					for(uint32 otherVolumeIdx : m_broadPhaseResults)
					{
						SVolume& otherVolume = m_volumes[otherVolumeIdx];
						if(volume.bounds.Overlap(otherVolume.bounds))
						{
							results.push_back(otherVolume.id);
						}
					}
					// Sort results by index.
					std::sort(results.begin(), results.end(), SCompareVolumeIds());
				}
			}
		}
	}

	void CSpatialIndex::Update()
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		UpdateMonitoredVolumes();
		ProcessEventQueue(m_eventQueue);
		/*if(EnvCVars::sc_DebugDrawDetectionVolumes == 1)
		{
			DebugDraw();
		}*/
	}

	ESpatialVolumeTags CSpatialIndex::CreateTag(const SSpatialVolumeTagName& tagName) const
	{
		const int tagIdx = m_pSettings->tagNames.find(tagName.value.c_str());
		if(tagIdx != Serialization::StringList::npos)
		{
			return static_cast<ESpatialVolumeTags>(m_pSettings->tagValues[tagIdx]);
		}
		return ESpatialVolumeTags::None;
	}

	ESpatialVolumeTags CSpatialIndex::CreateTags(const SpatialVolumeTagNames& tagNames) const
	{
		ESpatialVolumeTags result = ESpatialVolumeTags::None;
		for(const SSpatialVolumeTagName& tagName : tagNames)
		{
			result |= CreateTag(tagName);
		}
		return result;
	}

	const Serialization::StringList& CSpatialIndex::GetTagNames() const
	{
		return m_pSettings->tagNames;
	}

	const SpatialVolumeTagValues& CSpatialIndex::GetTagValues() const
	{
		return m_pSettings->tagValues;
	}

	SpatialVolumeId CSpatialIndex::GenerateVolumeId(uint32 volumeIdx)
	{
		const uint32          salt = m_salt ++;
		const SpatialVolumeId volumeId((volumeIdx & s_volumeIdxMask) | ((salt << s_volumeSaltShift) & s_volumeSaltMask));
		CRY_ASSERT(VolumeIdxFromId(volumeId) == volumeIdx);
		return volumeId;
	}

	uint32 CSpatialIndex::VolumeIdxFromId(const SpatialVolumeId& id) const
	{
		return id.GetValue() & s_volumeIdxMask;
	}

	inline bool CSpatialIndex::CompareVolumeIds(const SpatialVolumeId& lhs, const SpatialVolumeId& rhs) const
	{
		return (lhs.GetValue() & s_volumeIdxMask) < (rhs.GetValue() & s_volumeIdxMask);
	}

	void CSpatialIndex::BroadPhaseQuery(uint32 volumeIdx, ESpatialVolumeTags tags, VolumeIdxs& results) const
	{
		const SBroadPhaseVolume	broadPhaseVolume = m_broadPhaseVolumes[volumeIdx];
		for(uint32 otherVolumeIdx = 0, volumeCount = m_volumes.size(); otherVolumeIdx < volumeCount; ++ otherVolumeIdx)
		{
			if(otherVolumeIdx != volumeIdx)
			{
				const SBroadPhaseVolume& otherBroadPhaseVolume = m_broadPhaseVolumes[otherVolumeIdx];
				if((tags & otherBroadPhaseVolume.tags) != 0)
				{
					if(Overlap::AABB_AABB(broadPhaseVolume.aabb, otherBroadPhaseVolume.aabb))
					{
						results.push_back(otherVolumeIdx);
					}
				}
			}
		}
	}

	void CSpatialIndex::UpdateMonitoredVolumes()
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		// Update monitored volumes.
		for(SVolume& volume : m_volumes)
		{
			// Query for overlapping volumes?
			m_monitorResults.clear();
			if(volume.monitorTags != ESpatialVolumeTags::None)
			{
				Query(volume.id, volume.monitorTags, m_monitorResults);
			}
			// Check for volumes leaving monitored volume.
			for(uint32 cacheVolumeIdx = 0, cacheVolumeCount = volume.monitorCache.size(); cacheVolumeIdx < cacheVolumeCount; )
			{
				const SpatialVolumeId      cacheVolumeId = volume.monitorCache[cacheVolumeIdx];

				
				SpatialVolumeIds::iterator itLowerBoundVolume = std::lower_bound(m_monitorResults.begin(), m_monitorResults.end(), cacheVolumeId, SCompareVolumeIds());
				
				if((itLowerBoundVolume == m_monitorResults.end()) || (*itLowerBoundVolume != cacheVolumeId))
				{
					SSpatialIndexEvent event;
					event.id           = ESpatialIndexEventId::Leaving;
					event.volumeIds[0] = volume.id;
					event.volumeIds[1] = cacheVolumeId;
					m_eventQueue.push_back(event);

					volume.monitorCache.erase(volume.monitorCache.begin() + cacheVolumeIdx);
					-- cacheVolumeCount;
				}
				else
				{
					++ cacheVolumeIdx;
				}
			}
			// Check for volumes entering monitored volume.
			for(uint32 newVolumeIdx = 0, newVolumeCount = m_monitorResults.size(); newVolumeIdx < newVolumeCount; ++ newVolumeIdx)
			{
				const SpatialVolumeId      newVolumeId = m_monitorResults[newVolumeIdx];
				SpatialVolumeIds::iterator itLowerBoundVolume = std::lower_bound(volume.monitorCache.begin(), volume.monitorCache.end(), newVolumeId);
				if((itLowerBoundVolume == volume.monitorCache.end()) || (*itLowerBoundVolume != newVolumeId))
				{
					volume.monitorCache.insert(itLowerBoundVolume, newVolumeId);

					SSpatialIndexEvent event;
					event.id           = ESpatialIndexEventId::Entering;
					event.volumeIds[0] = volume.id;
					event.volumeIds[1] = newVolumeId;
					m_eventQueue.push_back(event);
				}
			}
		}
	}

	void CSpatialIndex::ProcessEventQueue(EventQueue& eventQueue) const
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		// Send out all events in queue. N.B. This is performed after the update logic in order to reduce cache misses.
		const uint32 volumeCount = m_volumes.size();
		for(const SSpatialIndexEvent& event : eventQueue)
		{
			const SVolume& volume = m_volumes[VolumeIdxFromId(event.volumeIds[0])];
			if(volume.eventCallback)
			{
				volume.eventCallback(event);
			}
		}
		eventQueue.clear();
	}

	void CSpatialIndex::DebugDraw() const
	{
		IRenderAuxGeom& renderAuxGeom = *gEnv->pRenderer->GetIRenderAuxGeom();
		for(const SVolume& volume : m_volumes)
		{
			ColorB color = ColorB(0, 150, 0);
			if(!volume.monitorCache.empty())
			{
				color = ColorB(150, 0, 0);
			}
			else if(volume.monitorTags != ESpatialVolumeTags::None)
			{
				color = ColorB(150, 150, 0);
			}
			switch(volume.bounds.GetShape())
			{
			case ESpatialVolumeShape::Box:
				{
					renderAuxGeom.DrawOBB(volume.bounds.AsBox(), Matrix34(IDENTITY), false, color, eBBD_Faceted);
					break;
				}
			case ESpatialVolumeShape::Sphere:
				{
					SAuxGeomRenderFlags prevRenderFlags = renderAuxGeom.GetRenderFlags();
					renderAuxGeom.SetRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);
					renderAuxGeom.DrawSphere(volume.bounds.AsSphere().center, volume.bounds.AsSphere().radius, ColorB(color.r, color.g, color.b, 64), false);
					renderAuxGeom.SetRenderFlags(prevRenderFlags);
					break;
				}
			case ESpatialVolumeShape::Point:
				{
					renderAuxGeom.DrawSphere(volume.bounds.AsPoint(), 0.1f, color, false);
					break;
				}
			}
		}
	}
}
