// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BodyDamage.h"
#include "BodyManagerCVars.h"

#include "Actor.h"
#include "GameRules.h"

#define BODYDAMAGE_LIVING_ENTITY_CAPSULE_PARTID 100

CBodyDamageProfile::JointId CBodyDamageProfile::JointId::GetJointIdFromPartId( IEntity& characterEntity, const int partId )
{
	ICharacterInstance* pCharacterInstance = characterEntity.GetCharacter(0);
	if (pCharacterInstance)
	{
		ISkeletonPose* pSkeletonPose = pCharacterInstance->GetISkeletonPose();
		CRY_ASSERT(pSkeletonPose);
		IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
		IAttachmentManager* pAttachmentManager = pCharacterInstance->GetIAttachmentManager();
		CRY_ASSERT(pAttachmentManager);

		if (IAttachment* pAttachment = pAttachmentManager->GetInterfaceByPhysId( partId ))
		{
			return JointId(pAttachment->GetName());
		}
		else if ((partId >= 0) && (partId < (int)rIDefaultSkeleton.GetJointCount()))
		{
			const char* boneName = rIDefaultSkeleton.GetJointNameByID(pSkeletonPose->getBonePhysParentOrSelfIndex(partId));
			if ((boneName != NULL))
			{
				return JointId(boneName);
			}
		}
	}

	return JointId();
}

CBodyDamageProfile::JointId CBodyDamageProfile::JointId::GetJointIdFromPartId( IDefaultSkeleton& rIDefaultSkeleton, ISkeletonPose& skeletonPose, const int partId )
{
	if (partId >= 0)
	{
		const char* boneName = rIDefaultSkeleton.GetJointNameByID(skeletonPose.getBonePhysParentOrSelfIndex(partId));
		if ((boneName != NULL) && *boneName)
		{
			return JointId(boneName);
		}
	}

	return JointId();
}

CBodyDamageProfile::MatMappingId CBodyDamageProfile::MatMappingId::GetMatMappingIdFromPartId( IDefaultSkeleton& rIDefaultSkeleton, ISkeletonPose& skeletonPose, const int partId )
{
	if (partId >= 0)
	{
		const char* boneName = rIDefaultSkeleton.GetJointNameByID(skeletonPose.getBonePhysParentOrSelfIndex(partId));
		if ((boneName != NULL) && *boneName)
		{
			return MatMappingId(boneName);
		}
	}

	return MatMappingId();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CBodyDamageProfile::SMaterialMappingEntry::SMaterialMappingEntry()
	: materialsCount(0)
{
	memset(materials, 0, sizeof(materials));
}

CBodyDamageProfile::CEffectiveMaterials::CEffectiveMaterials(CBodyDamageProfile& bodyDamageProfile, IDefaultSkeleton& rIDefaultSkeleton, ISkeletonPose& skeletonPose, IPhysicalEntity& physicalEntity)
	: m_bodyDamageProfile(bodyDamageProfile)
	, m_skeletonPose(skeletonPose)
	, m_rICharacterModelSkeleton(rIDefaultSkeleton)
	, m_physicalEntity(physicalEntity)
{

}

void CBodyDamageProfile::CEffectiveMaterials::LoadEffectiveMaterials(const XmlNodeRef& parentNode, const char* boneName /*= NULL*/, int boneId /*= -1*/)
{
	for (int index = 0; index < parentNode->getChildCount(); ++index)
	{
		XmlNodeRef node = parentNode->getChild(index);
		if (0 == strcmp(node->getTag(), "EffectiveMaterial"))
		{
			LoadEffectiveMaterial(node, boneName, boneId);
		}
		else if (0 == strcmp(node->getTag(), "Bone"))
		{
			const char* boneName1 = NULL;
			if (node->getAttr("name", &boneName1))
			{
				int namedBoneId = m_rICharacterModelSkeleton.GetJointIDByName(boneName1);

				if (namedBoneId >= 0)
					LoadEffectiveMaterials(node, boneName1, namedBoneId);
				else
					GameWarning("BodyDamage: Invalid bone name [%s] in effective material", boneName1);
			}
		}
	}
}

void CBodyDamageProfile::CEffectiveMaterials::Load(const XmlNodeRef& parentNode)
{
	XmlNodeRef effectiveMaterialsNode = parentNode->findChild("EffectiveMaterials");
	if (effectiveMaterialsNode)
	{
		LoadEffectiveMaterials(effectiveMaterialsNode);
	}
}

void CBodyDamageProfile::CEffectiveMaterials::LoadEffectiveMaterial(const XmlNodeRef& effectiveMaterial, const char* boneName /*= NULL*/, int boneId /*= -1*/)
{
	const char* sourceMaterial = NULL;
	const char* targetMaterial = NULL;

	if (effectiveMaterial->getAttr("source", &sourceMaterial) && effectiveMaterial->getAttr("target", &targetMaterial))
	{
		IMaterialManager *pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
		assert(pMaterialManager);
		int sourceMaterialId = pMaterialManager->GetSurfaceTypeIdByName(sourceMaterial);
		int targetMaterialId = pMaterialManager->GetSurfaceTypeIdByName(targetMaterial);
		if (sourceMaterialId && targetMaterialId)
		{
			if (boneName && boneId >= 0)
			{
				if (!m_effectiveMaterialsByBone[JointId(boneName)].insert(std::make_pair(sourceMaterialId, targetMaterialId)).second)
					GameWarning("BodyDamage: EffectiveMaterial source already used [%s] in bone [%s]", sourceMaterial, boneName);
			}
			else
			{
				if (!m_effectiveMaterials.insert(std::make_pair(sourceMaterialId, targetMaterialId)).second)
					GameWarning("BodyDamage: EffectiveMaterial source already used [%s]", sourceMaterial);
			}
		}
		else
		{
			if (!sourceMaterial)
				GameWarning("BodyDamage: Can't find source EffectiveMaterial [%s]", sourceMaterial);

			if (!targetMaterialId)
				GameWarning("BodyDamage: Can't find target EffectiveMaterial [%s]", targetMaterial);
		}
	}
}

void CBodyDamageProfile::CEffectiveMaterials::UpdateMapping( const char* jointName, const int physicsJointId )
{
	const JointId jointId(jointName); 

	TEffectiveMaterialsByBone::const_iterator effectiveMaterialsByBone = m_effectiveMaterialsByBone.find(jointId);
	if (effectiveMaterialsByBone != m_effectiveMaterialsByBone.end())
		UpdateMapping(jointName, physicsJointId, effectiveMaterialsByBone->second);
	else
		UpdateMapping(jointName, physicsJointId, m_effectiveMaterials);
}

void CBodyDamageProfile::CEffectiveMaterials::UpdateMapping(const char* jointName, const int physicsJointId, const TEffectiveMaterials& effectiveMaterials)
{
	IMaterialManager *pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
	assert(pMaterialManager);

	for (TEffectiveMaterials::const_iterator it = effectiveMaterials.begin(); it != effectiveMaterials.end(); ++it)
	{
		ISurfaceType* pSourceMaterial = pMaterialManager->GetSurfaceType(it->first);
		ISurfaceType* pTargetMaterial = pMaterialManager->GetSurfaceType(it->second);

		if (pSourceMaterial && pTargetMaterial)
		{
			UpdateMapping(jointName, physicsJointId, *pSourceMaterial, *pTargetMaterial);
		}
	}
}

void CBodyDamageProfile::CEffectiveMaterials::UpdateMapping(const char* jointName, const int physicsJointId, ISurfaceType& sourceMaterial, ISurfaceType& targetMaterial)
{
	int sourceMaterialId = sourceMaterial.GetId();
	int targetMaterialId = targetMaterial.GetId();

	pe_params_part part;
	part.partid = physicsJointId;

	if (m_physicalEntity.GetParams(&part) && part.pMatMapping)
	{
		const JointId jointId(jointName);
		const MatMappingId matMappingId(jointName);

		TMaterialIds& appliedMaterialIds = m_jointIdsApplied.insert(std::make_pair(jointId, TMaterialIds())).first->second;
		SMaterialMappingEntry& mappingEntry = m_bodyDamageProfile.InsertMappingEntry(matMappingId, part);

		for (int materialIndex = 0; materialIndex < mappingEntry.materialsCount; ++materialIndex)
		{
			if (mappingEntry.materials[materialIndex] == sourceMaterialId)
			{
				if (!stl::find(appliedMaterialIds, materialIndex))
				{
					mappingEntry.materials[materialIndex] = targetMaterialId;
					appliedMaterialIds.push_back(materialIndex);

					LogEffectiveMaterialApplied(-1, sourceMaterial.GetName(), targetMaterial.GetName(), physicsJointId, materialIndex);
				}
			}
		}

		UpdatePhysicsPartById(matMappingId, part, mappingEntry, appliedMaterialIds);
	}

}

void CBodyDamageProfile::CEffectiveMaterials::FinalizeMapping()
{
	IMaterialManager *pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
	assert(pMaterialManager);

	pe_status_nparts status_nparts;
		int nParts = m_physicalEntity.GetStatus(&status_nparts);
		for (int partIndex = 0; partIndex < nParts; partIndex++)
		{
			pe_params_part part;
			part.ipart = partIndex;
	
			if (m_physicalEntity.GetParams(&part))
			{
				const JointId jointId = JointId::GetJointIdFromPartId( m_rICharacterModelSkeleton,m_skeletonPose, part.partid );
				if (jointId==JointId())
					continue;
				const MatMappingId matMappingId = MatMappingId::GetMatMappingIdFromPartId( m_rICharacterModelSkeleton, m_skeletonPose, part.partid );

				TMaterialIds& appliedMaterialIds = m_jointIdsApplied.insert(std::make_pair(jointId, TMaterialIds())).first->second;
				SMaterialMappingEntry& mappingEntry = m_bodyDamageProfile.InsertMappingEntry(matMappingId, part);
				for (int materialIndex = 0; materialIndex < mappingEntry.materialsCount; ++materialIndex)
				{
					if (!stl::find(appliedMaterialIds, materialIndex))
					{
						int sourceMaterialId = part.pMatMapping[materialIndex];
						TEffectiveMaterials::const_iterator targetMaterialIt = m_effectiveMaterials.find(sourceMaterialId);
						if (targetMaterialIt != m_effectiveMaterials.end())
						{
							ISurfaceType* pTargetMaterial = pMaterialManager->GetSurfaceType(targetMaterialIt->second);
							if (pTargetMaterial)
							{
								int targetMaterialId = pTargetMaterial->GetId();
	
								mappingEntry.materials[materialIndex] = targetMaterialId;
								appliedMaterialIds.push_back(materialIndex);
	
								LogEffectiveMaterialApplied(sourceMaterialId, NULL, pTargetMaterial->GetName(), part.partid, materialIndex);
							}
						}
					}
				}
	
				UpdatePhysicsPartById(matMappingId, part, mappingEntry, appliedMaterialIds);
			}
		}
}

void CBodyDamageProfile::CEffectiveMaterials::UpdatePhysicsPartById(const MatMappingId& matMappingId, const pe_params_part& part, SMaterialMappingEntry& mappingEntry, const TMaterialIds& appliedMaterialIds)
{
	if (appliedMaterialIds.empty())
	{
		m_bodyDamageProfile.RemoveMappingEntry(matMappingId);
	}
	else
	{
		pe_params_part changedPart;
		changedPart.ipart = part.ipart;
		changedPart.pMatMapping = mappingEntry.materials;
		changedPart.nMats = mappingEntry.materialsCount;
		m_physicalEntity.SetParams(&changedPart);
	}
}

CBodyDamageProfile::SMaterialMappingEntry& CBodyDamageProfile::InsertMappingEntry( const MatMappingId& matMappingId, const pe_params_part& part )
{
	std::pair<std::map<MatMappingId, SMaterialMappingEntry>::iterator, bool> insertResult = m_effectiveMaterialsMapping.insert(std::make_pair(matMappingId, SMaterialMappingEntry()));
	SMaterialMappingEntry& insertedEntry = insertResult.first->second;
	if (insertResult.second)
	{
		if (part.nMats <= SMaterialMappingEntry::MATERIALS_ARRAY_MAX_SIZE)
		{
			insertedEntry.materialsCount = part.nMats;
			memcpy(insertedEntry.materials, part.pMatMapping, sizeof(int) * part.nMats);
		}
		else
		{
			GameWarning("Not enough room to clone materials mapping");
			insertedEntry.materialsCount = SMaterialMappingEntry::MATERIALS_ARRAY_MAX_SIZE;
			memcpy(insertedEntry.materials, part.pMatMapping, sizeof(insertedEntry.materials));
		}
	}
	return insertedEntry;
}

void CBodyDamageProfile::RemoveMappingEntry(const MatMappingId& matMappingId)
{
	m_effectiveMaterialsMapping.erase(matMappingId);
}

bool CBodyDamageProfile::PhysicalizeEntity(IPhysicalEntity* pPhysicalEntity, IDefaultSkeleton* pIDefaultSkeleton) const
{
	assert(pPhysicalEntity);
	assert(pIDefaultSkeleton);
	bool bResult = (pPhysicalEntity != NULL) && (pIDefaultSkeleton != NULL);

	if (bResult)
	{
		TMaterialMappingEntries::const_iterator itEnd = m_effectiveMaterialsMapping.end();
		for (TMaterialMappingEntries::const_iterator it = m_effectiveMaterialsMapping.begin(); it != itEnd; ++it)
		{
			SMaterialMappingEntry materialMappingEntry(it->second);

			uint16 boneId = pIDefaultSkeleton->GetJointIDByName(it->first.GetName());

			pe_params_part part;
			part.partid = boneId;
			part.pMatMapping = materialMappingEntry.materials;
			part.nMats = materialMappingEntry.materialsCount;
			if (pPhysicalEntity->SetParams(&part) == 0)
				bResult = false;
		}
	}

	return bResult;
}

void CBodyDamageProfile::CEffectiveMaterials::LogEffectiveMaterialApplied(int sourceMaterialId, const char* sourceMaterial, const char* targetMaterial, int jointId, int materialIndex) const
{
	if (CBodyManagerCVars::IsBodyDamageLogEnabled())
	{
		if (!sourceMaterial)
		{
			IMaterialManager *pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
			assert(pMaterialManager);

			if (ISurfaceType* pSourceMaterial = pMaterialManager->GetSurfaceType(sourceMaterialId))
				sourceMaterial = pSourceMaterial->GetName();
		}

		CryLog("BodyDamage: Applying effective material Source [%s] Target [%s] BoneName [%s] BoneId [%d] Index [%d]", 
			sourceMaterial, targetMaterial, m_rICharacterModelSkeleton.GetJointNameByID(jointId), jointId, materialIndex);
	}
}

CBodyDamageProfile::CPart::CPart(const char* name, uint32 flags, int id)
	: m_name(name)
	, m_flags(flags)
	, m_id(id)
{

}

int CBodyDamageProfile::CPart::GetNextId()
{
	static int idGenerator = 0;
	return idGenerator++;
}

const CBodyDamageProfile::TMaterialIds* CBodyDamageProfile::CPart::GetMaterialsByJointId(const JointId& jointId) const
{
	TJointIds::const_iterator foundMaterial = m_jointIds.find(jointId);
	return foundMaterial != m_jointIds.end() ? &foundMaterial->second : NULL;
}

void CBodyDamageProfile::CPart::LoadElements(
	const XmlNodeRef& partNode, IDefaultSkeleton& skeletonPose, 
	IAttachmentManager& attachmentManager, CEffectiveMaterials& effectiveMaterials,
	const CBodyDamageProfile& ownerDamageProfile)
{
	for (int partElemIndex = 0; partElemIndex < partNode->getChildCount(); ++partElemIndex)
	{
		XmlNodeRef partElem = partNode->getChild(partElemIndex);
		if (0 == strcmp("Bone", partElem->getTag()))
		{
			const char* boneName = NULL;
			if (partElem->getAttr("name", &boneName))
			{
#ifndef _RELEASE
				// Need to perform the verification here because the bone names are not explicitly stored.
				const CPart* partThatUsesBoneName = ownerDamageProfile.FindPartWithBoneName(boneName);
				if (partThatUsesBoneName != NULL)
				{
					GameWarning("BodyDamage: Bone name [%s] for part [%s] is already assigned to part [%s]!", 
						boneName, m_name.c_str(), partThatUsesBoneName->GetName().c_str());
				}
#endif

				AddBone(partElem, boneName, skeletonPose, effectiveMaterials);
			}
		}
		else if (0 == strcmp("Attachment", partElem->getTag()))
		{
			const char* attachmentName = NULL;
			if (partElem->getAttr("name", &attachmentName))
				AddAttachment(attachmentName, attachmentManager);
		}
	}
}

void CBodyDamageProfile::CPart::AddBone(const XmlNodeRef& boneNode, const char* boneName, IDefaultSkeleton& rIDefaultSkeleton, CEffectiveMaterials& effectiveMaterials)
{
	const int16 boneId = rIDefaultSkeleton.GetJointIDByName(boneName);

	if (boneId >= 0)
	{
		std::pair<TJointIds::iterator, bool> insertResult = m_jointIds.insert(std::make_pair(JointId(boneName), TMaterialIds()));
		if (insertResult.second)
		{
			TMaterialIds& materialIds = insertResult.first->second;

			AddMaterial(boneNode, boneName, materialIds);

			effectiveMaterials.UpdateMapping( boneName, boneId );
		}
		else
			GameWarning("BodyDamage: Bone name [%s] is already available in part [%s]", boneName, m_name.c_str());
	}
	else
		GameWarning("BodyDamage: Invalid bone name [%s] for part [%s]", boneName, m_name.c_str());
}

void CBodyDamageProfile::CPart::AddMaterial(const XmlNodeRef& boneNode, const char* boneName, TMaterialIds &materialIds) 
{
	IMaterialManager *pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
	assert(pMaterialManager);

	for (int materialNodeIndex = 0; materialNodeIndex < boneNode->getChildCount(); ++materialNodeIndex)
	{
		XmlNodeRef materialNode = boneNode->getChild(materialNodeIndex);
		if (0 == strcmp("Material", materialNode->getTag()))
		{
			const char* materialName = NULL;
			if (materialNode->getAttr("name", &materialName))
			{
				if (int materialId = pMaterialManager->GetSurfaceTypeIdByName(materialName))
					materialIds.push_back(materialId);
				else
					GameWarning("BodyDamage: Invalid material name [%s] in bone [%s] for part [%s]", materialName, boneName, m_name.c_str());
			}
		}
	}
}

void CBodyDamageProfile::CPart::AddAttachment(const char* attachmentName, IAttachmentManager& attachmentManager)
{
	int32 attachmentId = attachmentManager.GetIndexByName(attachmentName);

	if (attachmentId >= 0)
	{
		if (!m_jointIds.insert(std::make_pair(JointId(attachmentName), TMaterialIds())).second)
			GameWarning("BodyDamage: Attachment name [%s] is already available in part [%s]", attachmentName, m_name.c_str());
	}
	else
		GameWarning("BodyDamage: Invalid attachment name [%s] for part [%s]", attachmentName, m_name.c_str());
}

CBodyDamageProfile::CBodyDamageProfile(TBodyDamageProfileId id)
: m_bInitialized(false)
, m_id(id)
{
	assert(id != INVALID_BODYDAMAGEPROFILEID);
}

void CBodyDamageProfile::LoadXmlInfo(const SBodyDamageDef &bodyDamageDef, bool bReload)
{
	if (bReload || (!m_bInitialized && !m_partsRootNode && !m_damageRootNode))
	{
		if (CBodyManagerCVars::g_bodyDamage_log) 
			CryLog("BodyDamage: Loading XML files BodyParts [%s] BodyDamage [%s]", 
			bodyDamageDef.bodyPartsFileName.c_str(), 
			bodyDamageDef.bodyDamageFileName.c_str());

		m_partsRootNode = LoadXml(bodyDamageDef.bodyPartsFileName.c_str());
		m_damageRootNode = LoadXml(bodyDamageDef.bodyDamageFileName.c_str());

		//--- Validate the hittypes
		CRY_ASSERT(g_pGame->GetGameRules()->GetHitTypeId("collision") != -1);
		CRY_ASSERT(g_pGame->GetGameRules()->GetHitTypeId("melee") != -1);
	}
}

bool CBodyDamageProfile::Init(const SBodyCharacterInfo& characterInfo, bool loadEffectiveMaterials /*= true*/, bool bReload /*= false*/)
{
	bool bResult = m_bInitialized;

	if (!m_bInitialized || bReload)
	{
		if (m_partsRootNode && m_damageRootNode)
		{
			CEffectiveMaterials effectiveMaterials(*this, *characterInfo.pIDefaultSkeleton, *characterInfo.pSkeletonPose, *characterInfo.pPhysicalEntity);

			if (loadEffectiveMaterials) 
				effectiveMaterials.Load(m_damageRootNode);

			LoadParts(m_partsRootNode, *characterInfo.pIDefaultSkeleton, *characterInfo.pAttachmentManager, effectiveMaterials);
			LoadMultipliers(m_damageRootNode);
			LoadImpulseFilters(m_damageRootNode, *characterInfo.pIDefaultSkeleton);
			LoadExplosionMultipliers(m_damageRootNode);

			if (loadEffectiveMaterials) 
				effectiveMaterials.FinalizeMapping();

			// Drop info once its been initialized
			m_partsRootNode = NULL;
			m_damageRootNode = NULL;
			m_bInitialized = true;

			bResult = true;
		}
	}

	return bResult;
}

XmlNodeRef CBodyDamageProfile::LoadXml(const char* fileName) const
{
	XmlNodeRef rootNode = GetISystem()->LoadXmlFromFile(fileName);
	if (rootNode && 0 == strcmp(rootNode->getTag(), "BodyDamage"))
	{
		return rootNode;
	}

	return XmlNodeRef();
}

void CBodyDamageProfile::LoadMultipliers(const XmlNodeRef& rootNode)
{
	XmlNodeRef multipliersNode = rootNode->findChild("Multipliers");
	if (multipliersNode)
	{
		multipliersNode->getAttr("globalDefaultMultiplier", m_defaultMultipliers.m_global);
		multipliersNode->getAttr("collisionDefaultMultiplier", m_defaultMultipliers.m_collision);

		for (int multiplierIndex = 0; multiplierIndex < multipliersNode->getChildCount(); ++multiplierIndex)
		{
			XmlNodeRef multiplierNode = multipliersNode->getChild(multiplierIndex);
			LoadMultiplier(multiplierNode);
		}
	}
}

void CBodyDamageProfile::LoadImpulseFilters(const XmlNodeRef& rootNode, IDefaultSkeleton& skeletonPose)
{
	XmlNodeRef impulseFiltersNode = rootNode->findChild("ImpulseFilters");

	if (impulseFiltersNode)
	{
		for (int impFiltIndex = 0; impFiltIndex < impulseFiltersNode->getChildCount(); ++impFiltIndex)
		{
			XmlNodeRef multiplierNode = impulseFiltersNode->getChild(impFiltIndex);
			LoadImpulseFilter(multiplierNode, skeletonPose);
		}
	}
}

void CBodyDamageProfile::LoadParts(const XmlNodeRef& rootNode, IDefaultSkeleton& skeletonPose, IAttachmentManager& attachmentManager, CEffectiveMaterials& effectiveMaterials)
{
	XmlNodeRef partsNode = rootNode->findChild("Parts");
	if (partsNode)
	{
		for (int partIndex = 0; partIndex < partsNode->getChildCount(); ++partIndex)
		{
			XmlNodeRef partNode = partsNode->getChild(partIndex);
			const char* partName = NULL;
			if (partNode->getAttr("name", &partName))
			{
				CPart part(partName, LoadPartFlags(partNode), partIndex);
				part.LoadElements(partNode, skeletonPose, attachmentManager, effectiveMaterials, *this);
				m_parts.push_back(part);
			}
		}
		IndexParts();
	}
}

uint32 CBodyDamageProfile::LoadPartFlags(const XmlNodeRef& partNode) const
{
	uint32 flags = eBodyDamage_PID_None;

	const char* flagString = NULL;
	if (partNode->getAttr("flags", &flagString))
	{
		if(strstr(flagString, "headshot"))
		{
			flags |= eBodyDamage_PID_Headshot;
		}
		if(strstr(flagString, "helmet"))
		{
			flags |= eBodyDamage_PID_Helmet; 
		}
		if(strstr(flagString, "foot"))
		{
			flags |= eBodyDamage_PID_Foot;
		}
		if(strstr(flagString, "pelvis"))
		{
			flags |= eBodyDamage_PID_Groin;
		}
		if(strstr(flagString, "knee"))
		{
			flags |= eBodyDamage_PID_Knee;
		}
		if (strstr(flagString, "weakspot"))
		{
			flags |= eBodyDamage_PID_WeakSpot;
		}
	}
	return flags;
}

void CBodyDamageProfile::LoadMultiplier(const XmlNodeRef& multiplierNode)
{
	const char* partName = NULL;
	float multiplierValue = 0.0f;
	if (multiplierNode->getAttr("part", &partName) && multiplierNode->getAttr("value", multiplierValue))
	{
		TParts::const_iterator foundPart = std::find_if(m_parts.begin(), m_parts.end(), CPartByNameFunctor(partName));
		if (foundPart != m_parts.end())
		{
			std::pair<TPartIdsToMultipliers::iterator, bool> newElement = m_partIdsToMultipliers.insert(TPartIdsToMultipliers::value_type(foundPart->GetId(), SBodyPartDamageMultiplier(multiplierValue)));

			CRY_ASSERT(newElement.second);

			//Read melee and collision specific settings
			multiplierNode->getAttr("valueMelee", newElement.first->second.meleeMultiplier);
			multiplierNode->getAttr("valueCollision", newElement.first->second.collisionMultiplier);
			multiplierNode->getAttr("valueAimedHit", newElement.first->second.defaultMultiplier[eBHC_Aimed]);

			//Read bullet multipliers, if any...
			const int bulletMultiplierCount = multiplierNode->getChildCount();
			if (bulletMultiplierCount > 0)
			{
				TProjectileMultipliers& bulletMultipliers = newElement.first->second.bulletMultipliers;

				bulletMultipliers.reserve(bulletMultiplierCount);

				for (int i = 0; i < bulletMultiplierCount; ++i)
				{
					const XmlNodeRef& bulletNode = multiplierNode->getChild(i);

					float bulletMultiplierValue = 1.0f;
					float bulletMultiplierAimedValue = 1.0f;

					uint16 classId(~uint16(0));

					if (g_pGame->GetIGameFramework()->GetNetworkSafeClassId(classId, bulletNode->getTag()))
					{
						bulletNode->getAttr("value", bulletMultiplierValue);
						bulletMultiplierAimedValue = bulletMultiplierValue;
						
						if(!gEnv->bMultiplayer)
							bulletNode->getAttr("valueAimedHit", bulletMultiplierAimedValue);

						bulletMultipliers.push_back(SProjectileMultiplier(classId, bulletMultiplierValue, bulletMultiplierAimedValue));
					}
					else
					{
						GameWarning("BodyDamage: Projectile class [%s] does not exist", bulletNode->getTag());
					}
				}
			}
		}
		else
		{
			GameWarning("BodyDamage: PartName [%s] not found in Multiplier node", partName);
		}
	}
}

void CBodyDamageProfile::LoadExplosionMultipliers( const XmlNodeRef& rootNode )
{
	XmlNodeRef multipliersNode = rootNode->findChild("ExplosionMultipliers");
	if (multipliersNode)
	{
		const int multiplierCount = multipliersNode->getChildCount();
		m_explosionMultipliers.reserve(multiplierCount);

		for (int multiplierIndex = 0; multiplierIndex < multiplierCount; ++multiplierIndex)
		{
			XmlNodeRef multiplierNode = multipliersNode->getChild(multiplierIndex);
			LoadExplosionMultiplier(multiplierNode);
		}
	}
}

void CBodyDamageProfile::LoadExplosionMultiplier( const XmlNodeRef& multiplierNode )
{
	uint16 classId(~uint16(0));

	float explosionMultiplierValue = 1.0f;
	if (g_pGame->GetIGameFramework()->GetNetworkSafeClassId(classId, multiplierNode->getTag()))
	{
		multiplierNode->getAttr("value", explosionMultiplierValue);
		m_explosionMultipliers.push_back(SProjectileMultiplier(classId, explosionMultiplierValue, explosionMultiplierValue));
	}
	else
	{
		GameWarning("BodyDamage: Projectile class [%s] does not exist", multiplierNode->getTag());
	}

}

void CBodyDamageProfile::LoadImpulse( const XmlNodeRef& filterNode, IDefaultSkeleton& skeletonPose, const PartId partID )
{
	SBodyDamageImpulseFilter impulseFilter;
	filterNode->getAttr("scale", impulseFilter.multiplier);

	const char* pProjectileClass = NULL;
	if( filterNode->getAttr("ammoType", &pProjectileClass) )
	{
		g_pGame->GetIGameFramework()->GetNetworkSafeClassId(impulseFilter.projectileClassID, pProjectileClass);
	}

	const char *passToPartName = NULL;
	if (filterNode->getAttr("passTo", &passToPartName))
	{
		int16 boneId = skeletonPose.GetJointIDByName(passToPartName);
		if (boneId >= 0)
		{
			impulseFilter.passOnPartId = boneId;
			filterNode->getAttr("passOnScale", impulseFilter.passOnMultiplier);
		}
	}

	m_impulseFilters.insert( TImpulseFilters::value_type( partID, impulseFilter ) );
}

void CBodyDamageProfile::LoadImpulseFilter(const XmlNodeRef& filterNodeRoot, IDefaultSkeleton& skeletonPose)
{
	const char* partName = NULL;
	PartId partID = uint16(~0);

	if (filterNodeRoot->getAttr("part", &partName))
	{
		TParts::const_iterator foundPart = std::find_if(m_parts.begin(), m_parts.end(), CPartByNameFunctor(partName));

		if (foundPart != m_parts.end())
		{	
			partID = foundPart->GetId();
			
			LoadImpulse( filterNodeRoot, skeletonPose, partID );
	
			const int filterCount = filterNodeRoot->getChildCount();
			for( int i=0; i<filterCount; ++i )
			{
				const XmlNodeRef& filterNode = filterNodeRoot->getChild(i);

				LoadImpulse( filterNode, skeletonPose, partID );
			}
		}
		else
		{
			GameWarning("BodyDamage: PartName [%s] not found in ImpulseFilter node", partName);
		}
	}
}

void CBodyDamageProfile::IndexParts()
{
	for (TParts::const_iterator itParts = m_parts.begin(); itParts != m_parts.end(); ++itParts)
	{
		const CPart& part = *itParts;
		for (TJointIds::const_iterator itJointIds = part.GetJointIds().begin(); itJointIds != part.GetJointIds().end(); ++itJointIds)
		{
			JointId jointId = itJointIds->first;
			const TMaterialIds* materials = part.GetMaterialsByJointId(jointId);
			CRY_ASSERT(materials);
			if (materials)
				m_partsByJointId.insert(std::make_pair(jointId, CPartInfo(part, *materials)));

			CRY_TODO(13, 01, 2010, "Add consistency checks");
		}
	}
}


const CBodyDamageProfile::CPart* CBodyDamageProfile::FindPartWithBoneName(const char* boneName) const
{
	IF_LIKELY (boneName != NULL)
	{
		const JointId searchJointID(boneName);

		TParts::const_iterator partsEnditer = m_parts.end();

		for (TParts::const_iterator partsIter = m_parts.begin() ; partsIter != partsEnditer ; ++partsIter)
		{
			TJointIds::const_iterator itJointIdsEnd =  partsIter->GetJointIds().end();
			for (TJointIds::const_iterator itJointIds = partsIter->GetJointIds().begin() ; itJointIds != itJointIdsEnd ; ++itJointIds)
			{
				const JointId jointId = itJointIds->first;
				if (jointId == searchJointID)
				{
					return &(*partsIter);
				}
			}
		}
	}

	return NULL;
}


const CBodyDamageProfile::CPart* CBodyDamageProfile::FindPart( IEntity& characterEntity, const int partId, int material ) const
{
	const CPart* part = NULL;

	const JointId jointId = JointId::GetJointIdFromPartId( characterEntity, partId );

	TPartsByJointIdRange partsRange = m_partsByJointId.equal_range(jointId);
	for (; partsRange.first != partsRange.second; ++partsRange.first)
	{
		const CPartInfo& currentPart = partsRange.first->second;

		const TMaterialIds& materialIds = currentPart.GetMaterialIds();
		if (materialIds.empty())
		{
			part = &currentPart.GetPart();
		}
		else
		{
			TMaterialIds::const_iterator foundMaterial = std::find(materialIds.begin(), materialIds.end(), material);
			if (foundMaterial != materialIds.end())
			{
				if (CBodyManagerCVars::g_bodyDamage_log)
					LogFoundMaterial(*foundMaterial, currentPart, partId);

				part = &currentPart.GetPart();
				break;
			}
		}
	}

	return part;
}

float CBodyDamageProfile::GetDamageMultiplier(IEntity& characterEntity, const HitInfo& hitInfo) const
{
	float result = GetDefaultDamageMultiplier( hitInfo );
	const char* partName = "None";

	if (const CPart* part = FindPart( characterEntity, hitInfo.partId, hitInfo.material ))
	{
		partName = part->GetName().c_str();

		TPartIdsToMultipliers::const_iterator foundMultiplier = m_partIdsToMultipliers.find(part->GetId());
		if (foundMultiplier != m_partIdsToMultipliers.end())
		{
			const SBodyPartDamageMultiplier& bodyPartDamageInfo = foundMultiplier->second;
			
			float bulletMultiplier = 1.0f;
			const EBulletHitClass hitClass = hitInfo.aimed ? eBHC_Aimed : eBHC_Normal;
			if (FindDamageMultiplierForBullet(bodyPartDamageInfo.bulletMultipliers, hitInfo.projectileClassId, hitClass, bulletMultiplier))
			{
				result = bulletMultiplier;
			}
			else
			{
				result = GetBestMultiplierForHitType(bodyPartDamageInfo, hitInfo.type, hitClass);
			}
		}
	}

	if (CBodyManagerCVars::g_bodyDamage_log)
		LogDamageMultiplier(characterEntity, hitInfo, partName, result);

	return result;
}

float CBodyDamageProfile::GetExplosionDamageMultiplier(IEntity& characterEntity, const HitInfo& hitInfo) const
{
	float result = 1.0f;

	const int explosionMultiplierCount = m_explosionMultipliers.size();
	for (int i = 0; i < explosionMultiplierCount; ++i)
	{
		if (m_explosionMultipliers[i].projectileClassId != hitInfo.projectileClassId)
			continue;

		result = m_explosionMultipliers[i].multiplier[eBHC_Normal];
	}

	if (CBodyManagerCVars::g_bodyDamage_log)
		LogExplosionDamageMultiplier(characterEntity, result);

	return result;
}

bool CBodyDamageProfile::FindDamageMultiplierForBullet( const TProjectileMultipliers& bulletMultipliers, uint16 projectileClassId, EBulletHitClass hitClass, float& multiplier ) const
{
	const int bulletMultiplierCount = bulletMultipliers.size();

	if ((projectileClassId == 0xffff) || (bulletMultiplierCount == 0))
		return false;

	for (int i = 0; i < bulletMultiplierCount; ++i)
	{
		if (bulletMultipliers[i].projectileClassId != projectileClassId)
			continue;

		CRY_ASSERT((hitClass >= 0) && (hitClass < eBHC_Max));

		multiplier = bulletMultipliers[i].multiplier[hitClass];
		return true;
	}

	return false;
}

float CBodyDamageProfile::GetBestMultiplierForHitType( const SBodyPartDamageMultiplier& damageMultipliers, int hitType, EBulletHitClass hitClass ) const
{
	const HitTypeInfo* pHitInfo = g_pGame->GetGameRules()->GetHitTypeInfo(hitType);
	const bool bMelee = (pHitInfo && ((pHitInfo->m_flags & CGameRules::EHitTypeFlag::IsMeleeAttack) != 0));
	const bool useDefault = (hitType != CGameRules::EHitType::Collision) && !bMelee;

	if (useDefault)
	{
		CRY_ASSERT((hitClass >= 0) && (hitClass < eBHC_Max));
		return damageMultipliers.defaultMultiplier[hitClass];
	}
	else
	{
		return (hitType == CGameRules::EHitType::Collision) ? damageMultipliers.collisionMultiplier : damageMultipliers.meleeMultiplier;
	}
}

float CBodyDamageProfile::GetDefaultDamageMultiplier( const HitInfo& hitInfo ) const
{
	IF_UNLIKELY( (hitInfo.partId == BODYDAMAGE_LIVING_ENTITY_CAPSULE_PARTID) && (hitInfo.type == CGameRules::EHitType::Collision) )
		return m_defaultMultipliers.m_collision;

	return m_defaultMultipliers.m_global;
}

void CBodyDamageProfile::GetHitImpulseFilter( IEntity& characterEntity, const HitInfo &hitInfo, SBodyDamageImpulseFilter &impulseFilter) const
{
	bool bFound = false;

	if (!m_impulseFilters.empty() )
	{
		const CPart *part = FindPart( characterEntity,  hitInfo.partId, hitInfo.material );
		if( part )
		{
			const PartId partID = part->GetId();

			TImpulseFilters::const_iterator iFilter = m_impulseFilters.find( partID );
			const TImpulseFilters::const_iterator iEnd = m_impulseFilters.end();
			for( ; (iFilter!=iEnd) && (iFilter->first == partID); ++iFilter )
			{
				if( (iFilter->second.projectileClassID != uint16(~0) ) 
				 && (iFilter->second.projectileClassID == hitInfo.projectileClassId) )
				{
					impulseFilter = iFilter->second;
					bFound = true;
					break;
				}
				else
				if( iFilter->second.projectileClassID == uint16(~0) )
				{
					impulseFilter = iFilter->second;
					bFound = true;
				}
			}
		}
	}	

	if( !bFound )
	{
		impulseFilter.multiplier = 1.0f;
		impulseFilter.passOnMultiplier = 0.0f;
		impulseFilter.passOnPartId = -1;
	}
}


uint32 CBodyDamageProfile::GetPartFlags( IEntity& characterEntity, const HitInfo& hitInfo) const
{
	if (const CPart* part = FindPart( characterEntity, hitInfo.partId, hitInfo.material ))
	{
		return part->GetFlags();
	}

	return 0;
}


void CBodyDamageProfile::LogDamageMultiplier(IEntity& characterEntity, const HitInfo& hitInfo, const char* partName,  const float multiplierValue) const
{
	IMaterialManager *pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
	assert(pMaterialManager);

	const char* materialName = "";
	if (ISurfaceType* surfaceType = pMaterialManager->GetSurfaceType(hitInfo.material))
		materialName = surfaceType->GetName();

	const char* jointName = "";
	bool isAttachment = false;

	if (ICharacterInstance* pCharacterInstance = characterEntity.GetCharacter(0))
	{
		if (IAttachment* pAttachment = pCharacterInstance->GetIAttachmentManager()->GetInterfaceByPhysId(hitInfo.partId))
		{
			jointName = pAttachment->GetName();
			isAttachment = true;
		}
		else
		{
			IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
			jointName = rIDefaultSkeleton.GetJointNameByID(hitInfo.partId);
		}

		const bool hitCapsule = (strlen(jointName) == 0) && (hitInfo.partId == BODYDAMAGE_LIVING_ENTITY_CAPSULE_PARTID);
		if(hitCapsule)
		{
			jointName = "PhysicsCapsule";
		}

		CryLog("BodyDamage: Part [%s] JointId [%d] JointName [%s] IsAttachment [%d] Material [%s] MaterialId [%d] Profile ID [%i] Multiplier [%f]", 
			partName, hitInfo.partId, jointName, isAttachment ? 1 : 0, materialName, hitInfo.material, m_id, multiplierValue);
	}
}

void CBodyDamageProfile::LogExplosionDamageMultiplier(IEntity& characterEntity, const float multiplierValue) const
{
	CryLog("BodyDamage Explosion: Player [%s] Multiplier [%f]", characterEntity.GetName(), multiplierValue);
}

void CBodyDamageProfile::LogFoundMaterial(int materialId, const CPartInfo& part, const int partId) const
{
	IMaterialManager *pMaterialManager = gEnv->p3DEngine->GetMaterialManager();
	assert(pMaterialManager);

	const char* materialName = "";
	if (ISurfaceType* surfaceType = pMaterialManager->GetSurfaceType(materialId))
		materialName = surfaceType->GetName();

	CryLog("BodyDamage: Matched MaterialId [%d] MaterialName [%s] Part [%s] JointId [%d] JointName", 
		materialId, materialName, part.GetPart().GetName().c_str(), partId);
}

bool CBodyDamageProfile::Reload(const SBodyCharacterInfo& characterInfo, const SBodyDamageDef &bodyDamageDef, TBodyDamageProfileId id)
{
	bool bResult = false;

	m_parts.clear();
	m_partsByJointId.clear();
	m_partIdsToMultipliers.clear();
	m_impulseFilters.clear();

	if (characterInfo.pPhysicalEntity)
	{
		LoadXmlInfo(bodyDamageDef, true);
		bResult = Init(characterInfo, false, true);
	}

	return bResult;
}

void CBodyDamageProfile::GetMemoryUsage( ICrySizer *pSizer ) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddContainer(m_parts);	
	//pSizer->AddContainer(m_partsByJointId);
	pSizer->AddContainer(m_partIdsToMultipliers);
	pSizer->AddContainer(m_effectiveMaterialsMapping);		
}

