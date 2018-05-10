// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "MaterialHelpers.h"
#include "FbxScene.h"
#include "FbxMetaData.h"
#include "SandboxPlugin.h"
#include "TextureManager.h"
#include "DialogCommon.h"

#include <CryCore/ToolsHelpers/SettingsManagerHelpers.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>
#include <Cry3DEngine/I3DEngine.h>
#include <Material/Material.h>
#include <Material/MaterialManager.h>
#include <Material/MaterialHelpers.h>

// EditorCommon
#include "FilePathUtil.h"
#include <Controls/QuestionDialog.h>

#include <QFileInfo>
#include <QDir>

void LogPrintf(const char* szFormat, ...);

namespace Private_MaterialHelpers
{

FbxMetaData::TString SerializeMaterialName(const FbxTool::CScene* pScene, const FbxTool::SMaterial* pMaterial)
{
	// RC expects that the dummy material is written with an empty name.
	return pScene->IsDummyMaterial(pMaterial) ? "" : pMaterial->szName;
}

} // namespace Private_MaterialHelpers

void AutoAssignSubMaterialIDs(const std::vector<std::pair<int, QString>>& knownMaterials, FbxTool::CScene* pScene)
{
	FbxTool::CScene::CBatchMaterialSignals batchMaterialSignals(*pScene);

	const size_t numMaterials = pScene->GetMaterialCount();

	// Assign a sub-material index to each material element marked for index auto-assignment.

	std::vector<int> takenIndices(numMaterials, 0);
	std::vector<const FbxTool::SMaterial*> needsIndex;
	needsIndex.reserve(numMaterials);

	for (size_t i = 0; i < numMaterials; ++i)
	{
		const FbxTool::SMaterial* pMaterial = pScene->GetMaterialByIndex(i);
		if (pScene->IsMaterialMarkedForIndexAutoAssignment(pMaterial))
		{
			// We first try to find an index in the .mtl file by name.
			bool bFoundIndex = false;
			for (size_t i = 0; i < knownMaterials.size(); ++i)
			{
				if (knownMaterials[i].second == pMaterial->szName)
				{
					pScene->SetMaterialSubMaterialIndex(pMaterial, knownMaterials[i].first);
					takenIndices[knownMaterials[i].first] = 1;
					bFoundIndex = true;
					break;
				}
			}
			if (!bFoundIndex)
			{
				needsIndex.push_back(pMaterial);
			}
		}
		else if (pScene->GetMaterialSubMaterialIndex(pMaterial) >= 0) // Not marked for deletion?
		{
			// Tag all explicitly set sub-material indices.
			takenIndices[pScene->GetMaterialSubMaterialIndex(pMaterial)] = 1;
		}
	}

	// Try to preserve the original fbx material id's.
	for (size_t i = 0; i < needsIndex.size(); ++i)
	{
		// 0 - is a dummy material index.
		if (needsIndex[i]->id < 1)
		{
			continue;
		}

		const int index = needsIndex[i]->id - 1;
		if (!takenIndices[index])
		{
			pScene->SetMaterialSubMaterialIndex(needsIndex[i], index);
			takenIndices[index] = 1;
			needsIndex[i] = nullptr;
		}
	}
	needsIndex.erase(std::remove(needsIndex.begin(), needsIndex.end(), nullptr), needsIndex.end());

	// Assign default indices to fill up the range.
	// Element corresponding to dummy material is assigned minimum index.
	// Use lexicographic sort order so we increase likelihood of consistent results.
	std::sort(needsIndex.begin(), needsIndex.end(), [pScene](const FbxTool::SMaterial* pLeft, const FbxTool::SMaterial* pRight)
	{
		if (pScene->IsDummyMaterial(pLeft))
		{
		  return true;
		}
		if (pScene->IsDummyMaterial(pRight))
		{
		  return false;
		}
		return string(pLeft->szName) < string(pRight->szName);
	});
	int index = 0;
	for (auto pMaterial : needsIndex)
	{
		while (index + 1 < numMaterials && takenIndices[index])
		{
			++index;
		}
		pScene->SetMaterialSubMaterialIndex(pMaterial, index);
		takenIndices[index] = 1;
	}
}

// !!! XXX
extern FbxTool::EMaterialPhysicalizeSetting Convert(FbxMetaData::EPhysicalizeSetting setting);
extern FbxMetaData::EPhysicalizeSetting     Convert(FbxTool::EMaterialPhysicalizeSetting setting);

void WriteMaterialMetaData(const FbxTool::CScene* pScene, std::vector<FbxMetaData::SMaterialMetaData>& materialMetaData)
{
	using namespace Private_MaterialHelpers;

	for (int i = 0; i < pScene->GetMaterialCount(); ++i)
	{
		const FbxTool::SMaterial* const pMaterial = pScene->GetMaterialByIndex(i);
		assert(pMaterial);

		const int subMaterial = pScene->GetMaterialSubMaterialIndex(pMaterial);

		materialMetaData.emplace_back();
		FbxMetaData::SMaterialMetaData& materialData = materialMetaData.back();
		materialData.name = SerializeMaterialName(pScene, pMaterial);
		materialData.settings.subMaterialIndex = std::max(-1, subMaterial);
		materialData.settings.physicalizeSetting = Convert(pScene->GetMaterialPhysicalizeSetting(pMaterial));
		materialData.settings.bAutoAssigned = pScene->IsMaterialMarkedForIndexAutoAssignment(pMaterial);

		// TODO: Ignored? XXX
		// materialData.settings.subMaterialName = QtUtil::ToString(m_pMaterialList->model()->GetSubMaterialName(subMaterial));
		// materialData.settings.materialFile = m_pMaterialSettings->GetMaterialName();
	}
}

void ReadMaterialMetaData(FbxTool::CScene* pScene, const std::vector<FbxMetaData::SMaterialMetaData>& materialMetaData)
{
	using namespace Private_MaterialHelpers;

	FbxTool::CScene::CBatchMaterialSignals batchMaterialSignals(*pScene);

	for (auto& materialData : materialMetaData)
	{
		const FbxTool::SMaterial* const pMaterial = materialData.name.empty() ? pScene->GetMaterialByIndex(0) : pScene->GetMaterialByName(materialData.name);
		if (!pMaterial)
		{
			LogPrintf("Skipping unknown material '%s' referenced by meta data", materialData.name.c_str());
			continue;
		}

		pScene->SetMaterialSubMaterialIndex(pMaterial, materialData.settings.subMaterialIndex);
		pScene->SetMaterialPhysicalizeSetting(pMaterial, Convert(materialData.settings.physicalizeSetting));
		pScene->MarkMaterialForIndexAutoAssignment(pMaterial, materialData.settings.bAutoAssigned);
	}
}

std::vector<std::pair<int, QString>> GetSubMaterialList(const string& materialFilePath)
{
	// TODO: What the hell? Just get this from the IMaterial.
	std::vector<std::pair<int, QString>> loadedMaterialNames;

	if (!materialFilePath.empty())
	{
		XmlNodeRef rootNode = gEnv->pSystem->GetXmlUtils()->LoadXmlFromFile(materialFilePath.c_str());
		if (rootNode && strcmp(rootNode->getTag(), "Material") == 0)
		{
			const int numChildren = rootNode->getChildCount();
			for (int i = 0; i < numChildren; ++i)
			{
				const char* const szTag = rootNode->getChild(i)->getTag();
				if (strcmp(szTag, "SubMaterials") == 0)
				{
					const int numSubmaterials = rootNode->getChild(i)->getChildCount();
					for (int j = 0; j < numSubmaterials; ++j)
					{
						XmlNodeRef subMaterialNode = rootNode->getChild(i)->getChild(j);
						const char* szName = subMaterialNode->getAttr("Name");
						loadedMaterialNames.push_back(std::make_pair(j, !szName ? QString() : QtUtil::ToQString(szName)));
					}
				}
			}
		}
	}

	return loadedMaterialNames;
}

void CreateMaterial(CMaterial* pParentMaterial, FbxTool::CScene* pFbxScene, const SubMaterialInitializer& initializer)
{	
	const int sourceMaterialCount = pFbxScene->GetMaterialCount();

	int maxSubMatId = -1;
	for (size_t i = 0; i < sourceMaterialCount; ++i)
	{
		FbxTool::CScene::ConstMaterialHandle pMaterial = pFbxScene->GetMaterialByIndex(i);
		maxSubMatId = std::max(maxSubMatId, pFbxScene->GetMaterialSubMaterialIndex(pMaterial));
	}
	if (maxSubMatId < 0)
	{
		LogPrintf("%s: Cannot create material. All sub-materials are deleted.\n", __FUNCTION__);
		return;
	}

	CMaterialManager* const pMaterialManager = GetIEditor()->GetMaterialManager();
	if (maxSubMatId > 0)
	{
		std::vector<int> initSubMats;
		const int targetMaterialCount = maxSubMatId + 1;
		initSubMats.resize(targetMaterialCount, 0);

		pParentMaterial->SetSubMaterialCount(targetMaterialCount);
		for (int i = 0; i < sourceMaterialCount; ++i)
		{
			FbxTool::CScene::ConstMaterialHandle pMaterial = pFbxScene->GetMaterialByIndex(i);
			const int subMatId = pFbxScene->GetMaterialSubMaterialIndex(pMaterial);
			if (!initSubMats[subMatId])
			{
				CMaterial* pEditorMaterial = pMaterialManager->CreateMaterial(pMaterial->szName, XmlNodeRef(), MTL_FLAG_PURE_CHILD);
				if (initializer)
				{
					initializer(pEditorMaterial, *pMaterial);
				}
				pParentMaterial->SetSubMaterial(subMatId, pEditorMaterial);
				initSubMats[subMatId] = 1;
			}
		}

		for (size_t i = 0; i < initSubMats.size(); ++i)
		{
			if (!initSubMats[i])
			{
				LogPrintf("%s: Unassigned sub-material id %d\n", __FUNCTION__, i);
				CMaterial* pEditorMaterial = pMaterialManager->CreateMaterial("unassigned", XmlNodeRef(), MTL_FLAG_PURE_CHILD);
				pParentMaterial->SetSubMaterial(i, pEditorMaterial);
			}
		}
	}
	else
	{
		if (initializer)
		{
			initializer(pParentMaterial, *pFbxScene->GetMaterialByIndex(0));
		}
	}

	pParentMaterial->Update();
	pParentMaterial->Save();
}

string GetMaterialNameFromFilePath(const string& filePath)
{
	string matFilename = QtUtil::ToString(PathUtil::ToGamePath(QtUtil::ToQString(filePath)));
	PathUtil::RemoveExtension(matFilename);
	return matFilename;
}

const char* GetTextureSemanticFromChannelType(FbxTool::EMaterialChannelType channelType)
{
	switch (channelType)
	{
	case FbxTool::eMaterialChannelType_Diffuse:
		return "Diffuse";
	case FbxTool::eMaterialChannelType_Bump:
		return "Bumpmap";
	case FbxTool::eMaterialChannelType_Specular:
		return "Specular";
	default:
		return nullptr;
	}
}


