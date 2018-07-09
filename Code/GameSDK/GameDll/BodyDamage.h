// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef BODY_DAMAGE_H
#define BODY_DAMAGE_H

#include "BodyDefinitions.h"
#include "Utility/CryHash.h"

struct HitInfo;

class CBodyDamageProfile : public _reference_target_t
{
	
	struct JointId : std::unary_function<bool, const JointId&>
	{
		JointId()
			: m_id(0)
		{

		}

		JointId( const char* jointName )
			: m_id(CryStringUtils::HashString(jointName))
		{

		}

		
		bool operator<( const JointId& otherJoint ) const
		{
			return (m_id < otherJoint.m_id);
		}

		bool operator()( const JointId& otherJoint ) const
		{
			return (m_id < otherJoint.m_id);
		}

		bool operator==( const JointId& otherJoint ) const
		{
			return (m_id == otherJoint.m_id);
		}

		static JointId GetJointIdFromPartId( IEntity& characterEntity, const int partId );
		static JointId GetJointIdFromPartId(  IDefaultSkeleton& rIDefaultSkeleton, ISkeletonPose& skeletonPose, const int partId );

	private:
		CryHash m_id;
	};

	struct MatMappingId : std::unary_function<bool, const MatMappingId&>
	{
		MatMappingId()
			: m_jointId(0)
		{

		}

		MatMappingId( const char* jointName )
			: m_jointName(jointName)
			, m_jointId(jointName)
		{

		}


		bool operator<( const MatMappingId& other ) const
		{
			return (m_jointId < other.m_jointId);
		}

		bool operator()( const MatMappingId& other) const
		{
			return (m_jointId < other.m_jointId);
		}

		const char* GetName() const
		{
			return m_jointName.c_str();
		}

		static MatMappingId GetMatMappingIdFromPartId(  IDefaultSkeleton& rIDefaultSkeleton, ISkeletonPose& skeletonPose, const int partId );

	private:
		string  m_jointName;
		JointId m_jointId;
	};

	typedef int MaterialId;
	typedef int PartId;

	typedef std::vector<MaterialId> TMaterialIds;
	typedef std::map<JointId, TMaterialIds> TJointIds;
	typedef std::map<MaterialId, MaterialId> TEffectiveMaterials;
	typedef std::map<JointId, TEffectiveMaterials> TEffectiveMaterialsByBone;

	struct SMaterialMappingEntry
	{
		static const int MATERIALS_ARRAY_MAX_SIZE = 24;

		SMaterialMappingEntry();

		int materialsCount;
		int materials[MATERIALS_ARRAY_MAX_SIZE];

		void GetMemoryUsage( ICrySizer *pSizer ) const{}
	};

	enum EBulletHitClass
	{
		eBHC_Normal = 0,
		eBHC_Aimed	= 1,
		eBHC_Max	= 2
	};

	struct SProjectileMultiplier
	{
		SProjectileMultiplier(uint16 _projectileClass, float _multiplier, float _multiplierAimed)
			: projectileClassId(_projectileClass)
		{
			CRY_ASSERT(projectileClassId != (uint16)(~0));
			multiplier[eBHC_Normal] = _multiplier;
			multiplier[eBHC_Aimed] = _multiplierAimed;
		}

		uint16 projectileClassId;
		float multiplier[eBHC_Max];
	};

	typedef std::vector<SProjectileMultiplier> TProjectileMultipliers;

	struct SBodyPartDamageMultiplier
	{
		SBodyPartDamageMultiplier(float defaultValue)
			: meleeMultiplier(defaultValue)
			, collisionMultiplier(defaultValue)
		{
			defaultMultiplier[eBHC_Normal] = defaultValue;
			defaultMultiplier[eBHC_Aimed]  = defaultValue;
		}

		float defaultMultiplier[eBHC_Max]; 
		float meleeMultiplier;
		float collisionMultiplier;
		TProjectileMultipliers bulletMultipliers;
	};

	class CEffectiveMaterials
	{
	public:
		CEffectiveMaterials(CBodyDamageProfile& bodyDamageProfile, IDefaultSkeleton& rIDefaultSkeleton, ISkeletonPose& skeletonPose, IPhysicalEntity& physicalEntity);

		void Load(const XmlNodeRef& parentNode);

		void UpdateMapping( const char* jointName, const int physicsJointId );
		void FinalizeMapping();

	private:
		void UpdateMapping(const char* jointName, const int physicsJointId, const TEffectiveMaterials& effectiveMaterials);
		void UpdateMapping(const char* jointName, const int physicsJointId, ISurfaceType& sourceMaterial, ISurfaceType& targetMaterial);

		void LoadEffectiveMaterials(const XmlNodeRef& parentNode, const char* boneName = NULL, int boneId = -1);
		void LoadEffectiveMaterial(const XmlNodeRef& effectiveMaterial, const char* boneName = NULL, int boneId = -1);

		void LogEffectiveMaterialApplied(int sourceMaterialId, const char* sourceMaterial, const char* targetMaterial, int jointId, int materialIndex) const;

		void UpdatePhysicsPartById(const MatMappingId& matMappingId, const pe_params_part& part, SMaterialMappingEntry& mappingEntry, const TMaterialIds& appliedMaterialIds);

		TEffectiveMaterialsByBone m_effectiveMaterialsByBone;
		TEffectiveMaterials m_effectiveMaterials;

		CBodyDamageProfile& m_bodyDamageProfile;
		ISkeletonPose& m_skeletonPose;
		IDefaultSkeleton& m_rICharacterModelSkeleton;
		IPhysicalEntity& m_physicalEntity;

		TJointIds m_jointIdsApplied;
	};

	class CPart
	{
	public:
		CPart(const char* name, uint32 flags, int id);

		const string& GetName() const { return m_name; }
		PartId GetId() const { return m_id; }
		const TJointIds& GetJointIds() const { return m_jointIds; }
		const TMaterialIds* GetMaterialsByJointId(const JointId& jointId) const;
		uint32 GetFlags() const { return m_flags; }

		void LoadElements(const XmlNodeRef& partNode, IDefaultSkeleton& skeletonPose, IAttachmentManager& attachmentManager, CEffectiveMaterials& effectiveMaterials, const CBodyDamageProfile& ownerDamageProfile);

		void GetMemoryUsage( ICrySizer *pSizer ) const
		{
			pSizer->AddObject(m_name);
		}
	private:
		void AddBone(const XmlNodeRef& boneNode, const char* boneName, IDefaultSkeleton& rIDefaultSkeleton, CEffectiveMaterials& effectiveMaterials);

		void AddMaterial(const XmlNodeRef& boneNode, const char* boneName, TMaterialIds &materialIds);
		void AddAttachment(const char* attachmentName, IAttachmentManager& attachmentManager);

		static int GetNextId();

		TJointIds m_jointIds;
		string m_name;
		uint32 m_flags;
		PartId m_id;
	};

	class CPartByNameFunctor : std::unary_function<bool, const CPart&>
	{
	public:
		CPartByNameFunctor(const char* name) : m_name(name) {}
		bool operator()(const CPart& part) const { return 0 == strcmp(m_name, part.GetName().c_str()); }
	private:
		const char* m_name;
	};

	class CPartInfo
	{
	public:
		CPartInfo(const CPart& part, const TMaterialIds& materialIds) : m_part(part) , m_materialIds(materialIds) {}

		const TMaterialIds& GetMaterialIds() const { return m_materialIds; }
		const CPart& GetPart() const { return m_part; }

		void GetMemoryUsage( ICrySizer *pSizer ) const{}
	private:
		const TMaterialIds& m_materialIds;
		const CPart& m_part;
	};

	struct SDefaultMultipliers
	{
		SDefaultMultipliers()
			: m_global(1.0f)
			, m_collision(1.0f)
		{

		}

		float m_global;
		float m_collision;
	};

	typedef std::vector<CPart> TParts;
	typedef std::multimap<JointId, CPartInfo> TPartsByJointId;
	typedef std::map<PartId, SBodyPartDamageMultiplier> TPartIdsToMultipliers;

	typedef TPartsByJointId::const_iterator TPartsByJointIdIterator;
	typedef std::pair<TPartsByJointIdIterator, TPartsByJointIdIterator> TPartsByJointIdRange;

	typedef std::map<MatMappingId, SMaterialMappingEntry> TMaterialMappingEntries;
	typedef std::multimap<PartId,SBodyDamageImpulseFilter> TImpulseFilters;

public:
	CBodyDamageProfile(TBodyDamageProfileId id);

	void LoadXmlInfo(const SBodyDamageDef &bodyDamageDef, bool bReload = false);
	bool Init(const SBodyCharacterInfo& characterInfo, bool loadEffectiveMaterials = true, bool bReload = false);
	bool Reload(const SBodyCharacterInfo& characterInfo, const SBodyDamageDef &bodyDamageDef, TBodyDamageProfileId id);

	TBodyDamageProfileId GetId() const { return m_id; }
	bool IsInitialized() const { return m_bInitialized; }

	bool PhysicalizeEntity(IPhysicalEntity* pPhysicalEntity, IDefaultSkeleton* pIDefaultSkeleton) const;

	float  GetDamageMultiplier(IEntity& characterEntity, const HitInfo& hitInfo) const;
	float  GetExplosionDamageMultiplier(IEntity& characterEntity, const HitInfo& hitInfo) const;	
	uint32 GetPartFlags(IEntity& characterEntity, const HitInfo& hitInfo) const;

	SMaterialMappingEntry& InsertMappingEntry( const MatMappingId& matMappingId, const pe_params_part& part);
	void RemoveMappingEntry(const MatMappingId& matMappingId);

	void GetMemoryUsage(ICrySizer *pSizer) const;

	void GetHitImpulseFilter( IEntity& characterEntity, const HitInfo &hitInfo, SBodyDamageImpulseFilter &impulseFilter) const;

	const CPart* FindPartWithBoneName(const char* boneName) const;

private:
	XmlNodeRef LoadXml(const char* fileName) const;
	void LoadDamage(const char* bodyDamageFileName);
	void LoadParts(const XmlNodeRef& rootNode, IDefaultSkeleton& skeletonPose, IAttachmentManager& attachmentManager, CEffectiveMaterials& effectiveMaterials);
	uint32 LoadPartFlags(const XmlNodeRef& partNode) const;
	void LoadMultipliers(const XmlNodeRef& rootNode);
	void LoadMultiplier(const XmlNodeRef& multiplierNode);
	void LoadExplosionMultipliers(const XmlNodeRef& rootNode);
	void LoadExplosionMultiplier(const XmlNodeRef& multiplierNode);
	void LoadImpulseFilters(const XmlNodeRef& rootNode, IDefaultSkeleton& skeletonPose);
	void LoadImpulseFilter(const XmlNodeRef& filterNode, IDefaultSkeleton& skeletonPose);
	void LoadImpulse( const XmlNodeRef& filterNode, IDefaultSkeleton& skeletonPose, const PartId partID );
	void IndexParts();
	const CPart* FindPart( IEntity& characterEntity, const int partId, int material) const;
	void LogDamageMultiplier(IEntity& characterEntity, const HitInfo& hitInfo, const char* partName, const float multiplierValue) const;
	void LogExplosionDamageMultiplier(IEntity& characterEntity, const float multiplierValue) const;
	void LogFoundMaterial(int materialId, const CPartInfo& part, const int partId) const;

	bool FindDamageMultiplierForBullet(const TProjectileMultipliers& bulletMultipliers, uint16 projectileClassId, EBulletHitClass hitClass, float& multiplier) const;
	float GetBestMultiplierForHitType(const SBodyPartDamageMultiplier& damageMultipliers, int hitType, EBulletHitClass hitClass) const;

	float GetDefaultDamageMultiplier( const HitInfo& hitInfo ) const;

private:
	bool m_bInitialized;
	TBodyDamageProfileId m_id;

	// Caching Xml info for later initialization
	XmlNodeRef m_partsRootNode;
	XmlNodeRef m_damageRootNode;

	TParts m_parts;
	TPartsByJointId m_partsByJointId;
	TPartIdsToMultipliers m_partIdsToMultipliers;
	TMaterialMappingEntries m_effectiveMaterialsMapping;
	TImpulseFilters m_impulseFilters;
	TProjectileMultipliers m_explosionMultipliers;

	SDefaultMultipliers m_defaultMultipliers;

};

#endif
