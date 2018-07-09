// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneUserData.h"

bool HasDefaultUserData(const FbxTool::CScene* pScene, const FbxTool::SNode* pNode)
{
	return
		pScene->GetNodeExportSetting(pNode) == GetDefaultNodeExportSetting(pNode) &&
		pScene->GetNodeLod(pNode) == 0 &&
		pScene->GetUserDefinedProperties(pNode).empty() &&
		pScene->GetPhysicalProperties(pNode) == CNodeProperties() &&
		pScene->GetAutoLodProperties(pNode) == CAutoLodSettings::sNodeParam();
}

FbxTool::ENodeExportSetting GetDefaultNodeExportSetting(const FbxTool::SNode* pNode)
{
	static const FbxTool::ENodeExportSetting kDefaultSceneExportSetting = FbxTool::eNodeExportSetting_Include;

	if (!pNode->pParent)
	{
		return kDefaultSceneExportSetting;
	}

	const FbxTool::SNode* pParentNode = pNode->pParent;
	if (pParentNode && pParentNode->bVisible != pNode->bVisible)
	{
		// If visibility of the node changes in the FBX file compared to parent,
		// initialize the export-setting based on that.
		FbxTool::ENodeExportSetting exportSetting = pNode->bVisible
			? FbxTool::eNodeExportSetting_Include
			: FbxTool::eNodeExportSetting_Exclude;
		return exportSetting;
	}

	return FbxTool::eNodeExportSetting_Include;
}

