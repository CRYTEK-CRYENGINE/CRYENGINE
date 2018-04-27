// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <vector>

class CMaterial;

class QString;

namespace MeshImporter
{
class CSceneManager;
struct SDisplayScene;
}
namespace FbxTool {
class CScene;
}
namespace FbxMetaData {
struct SMaterialMetaData;
}
class CTextureManager;

void                                 AutoAssignSubMaterialIDs(const std::vector<std::pair<int, QString>>& knownMaterials, FbxTool::CScene* pScene);
void                                 WriteMaterialMetaData(const FbxTool::CScene* pScene, std::vector<FbxMetaData::SMaterialMetaData>& materialMetaData);
void                                 ReadMaterialMetaData(FbxTool::CScene* pScene, const std::vector<FbxMetaData::SMaterialMetaData>& materialMetaData);
std::vector<std::pair<int, QString>> GetSubMaterialList(const string& materialFilePath);

typedef std::function<void(CMaterial*, const FbxTool::SMaterial&)> SubMaterialInitializer;

void CreateMaterial(CMaterial* pParentMaterial, FbxTool::CScene* pFbxScene, const SubMaterialInitializer& initializer = SubMaterialInitializer());

string GetMaterialNameFromFilePath(const string& filePath);

const char* GetTextureSemanticFromChannelType(FbxTool::EMaterialChannelType channelType);

