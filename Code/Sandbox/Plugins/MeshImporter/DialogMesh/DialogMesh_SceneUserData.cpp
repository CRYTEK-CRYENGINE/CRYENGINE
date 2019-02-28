// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "DialogMesh_SceneUserData.h"
#include "FbxScene.h"

namespace DialogMesh
{

void CSceneUserData::Init(FbxTool::CScene* pFbxScene)
{
	m_pSelectedSkin = nullptr;
}

bool CSceneUserData::SelectSkin(const FbxTool::SMesh* pMesh)
{
	if (!pMesh || !pMesh->bHasSkin)
	{
		return false;
	}

	if (pMesh != m_pSelectedSkin)
	{
		m_pSelectedSkin = pMesh;
		sigSelectedSkinChanged();
	}

	return true;
}

bool CSceneUserData::SelectAnySkin(const FbxTool::SNode* pNode)
{
	if (!pNode)
	{
		return false;
	}

	for (int i = 0; i < pNode->numMeshes; ++i)
	{
		if (SelectSkin(pNode->ppMeshes[i]))
		{
			return true;
		}
	}

	return false;
}

void CSceneUserData::DeselectSkin()
{
	if (m_pSelectedSkin)
	{
		m_pSelectedSkin = nullptr;
		sigSelectedSkinChanged();
	}
}

const FbxTool::SMesh* CSceneUserData::GetSelectedSkin() const
{
	return m_pSelectedSkin;
}

} // namespace DialogMesh

