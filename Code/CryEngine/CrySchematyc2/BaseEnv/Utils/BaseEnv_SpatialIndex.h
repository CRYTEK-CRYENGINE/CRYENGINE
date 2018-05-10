// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// TODO : Rename volumes to objects?
// TODO : Investigate possibility of storing world position separately for box shapes in order to avoid matrix inverse calculations.

#pragma once

#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Delegate.h>

//struct ICharacterInstance;

namespace SchematycBaseEnv
{
	enum class ESpatialVolumeShape
	{
		None,
		Box,
		Sphere,
		Point
	};

	class CSpatialVolumeBounds
	{
	public:

		CSpatialVolumeBounds();
		explicit CSpatialVolumeBounds(const OBB& rhs);
		explicit CSpatialVolumeBounds(const Sphere& rhs);
		explicit CSpatialVolumeBounds(const Vec3& rhs);
		CSpatialVolumeBounds(const CSpatialVolumeBounds& rhs);

		~CSpatialVolumeBounds();

		static CSpatialVolumeBounds CreateOBB(const Matrix34& volumeTM, const Vec3& pos, const Vec3& size, const Matrix33& rot);
		static CSpatialVolumeBounds CreateSphere(const Matrix34& volumeTM, const Vec3& pos, float radius);
		static CSpatialVolumeBounds CreatePoint(const Matrix34& volumeTM, const Vec3& pos);

		ESpatialVolumeShape GetShape() const;

		const OBB& AsBox() const;
		const Sphere& AsSphere() const;
		const Vec3& AsPoint() const;

		AABB CalculateAABB() const;
		bool Overlap(const CSpatialVolumeBounds& rhs) const;

		void operator = (const CSpatialVolumeBounds& rhs);

	private:

		void Copy(const CSpatialVolumeBounds& rhs);
		void Release();

		ESpatialVolumeShape m_shape;
		uint8               m_storage[sizeof(OBB)];
	};

	enum class ESpatialVolumeTags : uint32 // Hunt specific! How can we register custom tags from game dll?
	{
		None  = 0,
		Begin = BIT(0), // First possible value, do not modify.
		End   = BIT(31) // One bit higher than last possible value, do not modify.
	};

	DECLARE_ENUM_CLASS_FLAGS(ESpatialVolumeTags)

	struct SSpatialVolumeTagName
	{
		SSpatialVolumeTagName();
		SSpatialVolumeTagName(const string& _value);

		bool operator == (const SSpatialVolumeTagName& rhs) const;

		string value; // TODO : Consider using Serialization::StringListValue here for performance.
	};

	bool Serialize(Serialization::IArchive& archive, SSpatialVolumeTagName& value, const char* szName, const char* szLabel);

	struct SSpatialVolumeParams
	{
		SSpatialVolumeParams();
		SSpatialVolumeParams(const CSpatialVolumeBounds& _bounds, ESpatialVolumeTags _tags, ESpatialVolumeTags _monitorTags = ESpatialVolumeTags::None, EntityId _entityId = 0);

		CSpatialVolumeBounds bounds;
		ESpatialVolumeTags   tags;
		ESpatialVolumeTags   monitorTags;
		EntityId             entityId;
	};

	WRAP_TYPE(uint32, SpatialVolumeId, ~0);

	enum class ESpatialIndexEventId
	{
		Entering = 0, // volumeIds[0] = id of monitored volume, volumeIds[1] = id of entering volume
		Leaving       // volumeIds[0] = id of monitored volume, volumeIds[1] = id of leaving volume
	};

	struct SSpatialIndexEvent
	{
		ESpatialIndexEventId id;
		SpatialVolumeId      volumeIds[2];
	};

	typedef TemplateUtils::CDelegate<void (const SSpatialIndexEvent&)> SpatialIndexEventCallback;
	typedef std::vector<SpatialVolumeId>                               SpatialVolumeIds;
	typedef std::vector<SSpatialVolumeTagName>                         SpatialVolumeTagNames;
	typedef std::vector<int>                                           SpatialVolumeTagValues;

	class CSpatialIndex
	{
	private:

		struct SCompareVolumeIds
		{
			inline bool operator () (const SpatialVolumeId& lhs, const SpatialVolumeId& rhs)
			{
				return (lhs.GetValue() & s_volumeIdxMask) < (rhs.GetValue() & s_volumeIdxMask);
			}
		};

		struct SSettings : public Schematyc2::IEnvSettings
		{
			typedef std::vector<string>	UserTags;

			// IEnvSettings
			virtual void Serialize(Serialization::IArchive& archive) override;
			// ~IEnvSettings

			void RefreshTags();

			UserTags                  userTags;
			Serialization::StringList tagNames;
			SpatialVolumeTagValues    tagValues;
		};

		DECLARE_SHARED_POINTERS(SSettings)

		struct SVolume
		{
			SVolume();

			CSpatialVolumeBounds      bounds;
			ESpatialVolumeTags        monitorTags;
			SpatialVolumeIds          monitorCache;
			SpatialVolumeId           id;
			EntityId                  entityId;
			SpatialIndexEventCallback eventCallback;
		};

		struct SBroadPhaseVolume
		{
			SBroadPhaseVolume();

			ESpatialVolumeTags tags;
			AABB               aabb;
		};

		typedef std::vector<SVolume>            Volumes;
		typedef std::vector<SBroadPhaseVolume>  BroadPhaseVolumes;
		typedef std::vector<uint32>             VolumeIdxs;
		typedef std::vector<SSpatialIndexEvent> EventQueue;

	public:

		CSpatialIndex();

		SpatialVolumeId CreateVolume(const SSpatialVolumeParams& params, const SpatialIndexEventCallback& eventCallback = SpatialIndexEventCallback());
		void DestroyVolume(const SpatialVolumeId& id);
		void UpdateVolumeBounds(const SpatialVolumeId& id, const CSpatialVolumeBounds& bounds);
		void SetVolumeTags(const SpatialVolumeId& id, ESpatialVolumeTags tags, bool bValue);
		void SetVolumeMonitorTags(const SpatialVolumeId& id, ESpatialVolumeTags tags, bool bValue);
		SSpatialVolumeParams GetVolumeParams(const SpatialVolumeId& id) const;
		void Query(const SpatialVolumeId& id, ESpatialVolumeTags tags, SpatialVolumeIds& results);
		void Update();

		ESpatialVolumeTags CreateTag(const SSpatialVolumeTagName& tagName) const;
		ESpatialVolumeTags CreateTags(const SpatialVolumeTagNames& tagNames) const;
		const Serialization::StringList& GetTagNames() const;
		const SpatialVolumeTagValues& GetTagValues() const;

	private:

		SpatialVolumeId GenerateVolumeId(uint32 volumeIdx);
		uint32 VolumeIdxFromId(const SpatialVolumeId& id) const;
		bool CompareVolumeIds(const SpatialVolumeId& lhs, const SpatialVolumeId& rhs) const;
		void BroadPhaseQuery(uint32 volumeIdx, ESpatialVolumeTags tags, VolumeIdxs& results) const;
		void UpdateMonitoredVolumes();
		void ProcessEventQueue(EventQueue& eventQueue) const;
		void DebugDraw() const;

	private:

		static const uint32 s_volumeIdxMask;
		static const uint32 s_volumeSaltShift;
		static const uint32 s_volumeSaltMask;

		SSettingsPtr      m_pSettings;
		Volumes           m_volumes;
		BroadPhaseVolumes m_broadPhaseVolumes;
		VolumeIdxs        m_freeVolumes;
		VolumeIdxs        m_broadPhaseResults;
		SpatialVolumeIds  m_monitorResults;
		EventQueue        m_eventQueue;
		uint16            m_salt;
	};

	//Matrix34 CreateVolumeTM(const Matrix34& worldTM, ICharacterInstance* pCharacter = nullptr, int32 boneId = -1);
}
