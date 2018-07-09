// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Scene.h"
#include "SceneElementSourceNode.h"
#include "SceneElementSkin.h"
#include "SceneElementTypes.h"
#include "SceneModelCommon.h"
#include "SceneView.h"
#include "FbxScene.h"

#include "ProxyModels/AttributeFilterProxyModel.h"

namespace Private_SourceSceneHelpers
{

void AddPseudoChildNodes(CSceneElementSourceNode* pSourceNodeElement)
{
	const FbxTool::SNode* const pNode = pSourceNodeElement->GetNode();
	bool bHasSkin = false;
	for (int i = 0; i < pNode->numMeshes; ++i)
	{
		bHasSkin = bHasSkin || pNode->ppMeshes[i]->bHasSkin;
	}
	if (bHasSkin)
	{
		CSceneElementSkin* const pSkinElement = pSourceNodeElement->GetScene()->NewElement<CSceneElementSkin>();
		CRY_ASSERT(pSkinElement);
		pSkinElement->SetName("Skin");
		pSourceNodeElement->AddChild(pSkinElement);
	}
}

} // namespace Private_SourceSceneHelpers

CSceneElementSourceNode* CreateSceneFromSourceScene(CScene& scene, FbxTool::CScene& sourceScene)
{	
	using namespace Private_SourceSceneHelpers;

	const FbxTool::SNode* const pFbxRootNode = sourceScene.GetRootNode();

	CSceneElementSourceNode* const pRoot = scene.NewElement<CSceneElementSourceNode>();
	CRY_ASSERT(pRoot);
	pRoot->SetName("Scene");
	pRoot->SetSceneAndNode(&sourceScene, pFbxRootNode);

	// Create a tree of scene elements.

	// S, P are used as stacks. Let s = S[i] and p = P[i] for some index i.
	// The following invariant holds:
	// p corresponds to the node s.parent.

	std::vector<CSceneElementSourceNode*> P;
	std::vector<const FbxTool::SNode*> S;

	for (int i = 0; i < pFbxRootNode->numChildren; ++i)
	{
		P.push_back(pRoot);
		S.push_back(pFbxRootNode->ppChildren[i]);
	}

	while (!S.empty())
	{
		CSceneElementSourceNode* const parent = P.back();
		P.pop_back();
		const FbxTool::SNode* pNode = S.back();
		S.pop_back();

		CSceneElementSourceNode* const pElement = scene.NewElement<CSceneElementSourceNode>();
		parent->AddChild(pElement);

		pElement->SetSceneAndNode(&sourceScene, pNode);
		pElement->SetName(pNode->szName);

		AddPseudoChildNodes(pElement);

		for (int i = 0; i < pNode->numChildren; ++i)
		{
			S.push_back(pNode->ppChildren[i]);
			P.push_back(pElement);
		}
	}

	return pRoot;
}

CSceneElementSourceNode* FindSceneElementOfNode(CScene& scene, const FbxTool::SNode* pNode)
{
	for (int i = 0; i < scene.GetElementCount(); ++i)
	{
		CSceneElementCommon* const pElement = scene.GetElementByIndex(i);
		if (pElement->GetType() == ESceneElementType::SourceNode)
		{
			auto pSourceNodeElement = (CSceneElementSourceNode*)pElement;
			if (pSourceNodeElement->GetNode() == pNode)
			{
				return pSourceNodeElement;
			}
		}
	}

	return nullptr;
}

void SelectSceneElementWithNode(CSceneModelCommon* pSceneModel, CSceneViewCommon* pSceneView, const FbxTool::SNode* pNode)
{
	CSceneElementCommon* const pSceneElement = pSceneModel->FindSceneElementOfNode(pNode);
	const QModelIndex modelIndex = pSceneModel->GetModelIndexFromSceneElement(pSceneElement);
	const auto pFilter = (QAttributeFilterProxyModel*)pSceneView->model();
	const QModelIndex proxyIndex = pFilter->mapFromSource(modelIndex);
	if (proxyIndex.isValid())
	{
		pSceneView->selectionModel()->select(proxyIndex, QItemSelectionModel::ClearAndSelect);
		pSceneView->scrollTo(modelIndex);
	}
}

