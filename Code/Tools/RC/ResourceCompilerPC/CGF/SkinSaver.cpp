// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SkinSaver.h"
#include "StringHelpers.h"

namespace Private_SkinSaver
{
	SBoneInitPosMatrix ConvertToBoneInitPosMatrix(const Matrix34& m, float unitSizeInCentimeters)
	{
		SBoneInitPosMatrix boneMatrix;
		for (int i = 0; i < 3; i++)
		{
			Vec3 ort = m.GetColumn(i);
			for (int j = 0; j < 3; j++)
			{
				boneMatrix[i][j] = ort[j];
			}
		}

		Vec3 translation = m.GetColumn3();
		for (int i = 0; i < 3; i++)
		{
			boneMatrix[3][i] = translation[i] * unitSizeInCentimeters;
		}

		return boneMatrix;
	}
}

bool CSkinSaver::SaveUncompiledSkinningInfo(CSaverCGF& cgfSaver, const CSkinningInfo& skinningInfo, const bool bSwapEndian, const float unitSizeInCentimeters)
{
	using namespace Private_SkinSaver;

	assert(skinningInfo.m_arrBoneEntities.size() == skinningInfo.m_arrBonesDesc.size());

	const int numBones = skinningInfo.m_arrBonesDesc.size();

	// Write bone meshes to chunk.

	std::vector<int> boneMeshMap(skinningInfo.m_arrPhyBoneMeshes.size());
	for (int phys = 0, physCount = int(skinningInfo.m_arrPhyBoneMeshes.size()); phys < physCount; ++phys)
	{
		boneMeshMap[phys] = cgfSaver.SaveBoneMesh(bSwapEndian, skinningInfo.m_arrPhyBoneMeshes[phys]);
	}

	// Write bones to chunk.

	DynArray<BONE_ENTITY> tempBoneEntities(skinningInfo.m_arrBoneEntities);
	for (int i = 0, count = int(tempBoneEntities.size()); i < count; ++i)
	{
		tempBoneEntities[i].phys.nPhysGeom = (tempBoneEntities[i].phys.nPhysGeom >= 0 ? boneMeshMap[tempBoneEntities[i].phys.nPhysGeom] : -1);
		StringHelpers::SafeCopyPadZeros(tempBoneEntities[i].prop, sizeof(tempBoneEntities[i].prop), skinningInfo.m_arrBoneEntities[i].prop);
	}
	cgfSaver.SaveBones(bSwapEndian, &tempBoneEntities[0], numBones, numBones * sizeof(BONE_ENTITY));

	SaveBoneNames(cgfSaver, skinningInfo.m_arrBonesDesc, bSwapEndian);
	SaveBoneInitialMatrices(cgfSaver, skinningInfo.m_arrBonesDesc, bSwapEndian, unitSizeInCentimeters);
	return true;
}

// Write bone initial matrices in BoneID order.
int CSkinSaver::SaveBoneInitialMatrices(CSaverCGF& cgfSaver, const DynArray<CryBoneDescData>& bonesDesc, const bool bSwapEndian, const float unitSizeInCentimeters)
{
	using namespace Private_SkinSaver;

	const int numBones = bonesDesc.size();

	std::vector<SBoneInitPosMatrix> boneMatrices(numBones);
	for (int BoneID = 0; BoneID<numBones; BoneID++)
	{
		boneMatrices[BoneID] = ConvertToBoneInitPosMatrix(bonesDesc[BoneID].m_DefaultB2W, unitSizeInCentimeters);
	}
	return cgfSaver.SaveBoneInitialMatrices(bSwapEndian, &boneMatrices[0], numBones, sizeof(SBoneInitPosMatrix)*numBones);
}

// Write bone names in BoneID order.
int CSkinSaver::SaveBoneNames(CSaverCGF& cgfSaver, const DynArray<CryBoneDescData>& bonesDesc, const bool bSwapEndian)
{
	using namespace Private_SkinSaver;

	const int numBones = bonesDesc.size();

	std::vector<char> boneNames;
	boneNames.reserve(32 * numBones);

	for (int BoneID = 0; BoneID<numBones; BoneID++)
	{
		const char* szBoneName = bonesDesc[BoneID].m_arrBoneName;
		size_t len = strlen(szBoneName);
		boneNames.insert(boneNames.end(), szBoneName, szBoneName + len);
		boneNames.push_back('\0');
	}
	// Terminated with double 0
	boneNames.push_back('\0');

	return cgfSaver.SaveBoneNames(bSwapEndian, &boneNames[0], numBones, boneNames.size());
}
