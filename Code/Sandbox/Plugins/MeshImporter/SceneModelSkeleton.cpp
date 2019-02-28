// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "SceneModelSkeleton.h"
#include "Scene/SceneElementCommon.h"
#include "Scene/SceneElementSourceNode.h"
#include "Scene/SceneElementTypes.h"

#include <EditorStyleHelper.h>

namespace Private_SceneModelSkeleton
{

bool IsPartialTree(const FbxTool::SNode* pSelf, const std::vector<bool>& nodesInclusion)
{
	bool bPartial = false;

	// TODO: Compute this for all elements in a single pass.
	std::vector<const FbxTool::SNode*> elementStack;
	elementStack.push_back(pSelf);
	while (!elementStack.empty() && !bPartial)
	{
		const FbxTool::SNode* const pElement = elementStack.back();
		elementStack.pop_back();

		if (!nodesInclusion[pElement->id])
		{
			bPartial = true;
		}

		for (int i = 0; i < pElement->numChildren; ++i)
		{
			elementStack.push_back(pElement->ppChildren[i]);
		}
	}

	return bPartial;
}

Qt::CheckState GetCheckStateRole(const FbxTool::SNode* pNode, const std::vector<bool>& nodesInclusion)
{
	if (!nodesInclusion[pNode->id])
	{
		return Qt::Unchecked;
	}
	else if(IsPartialTree(pNode, nodesInclusion))
	{
		return Qt::PartiallyChecked;
	}
	else
	{
		return Qt::Checked;
	}
}

void SetNodeInclusion(const FbxTool::SNode* pNode, std::vector<bool>& nodesInclusion, bool bIncluded)
{
	std::vector<const FbxTool::SNode*> elementStack;
	elementStack.push_back(pNode);

	while (!elementStack.empty())
	{
		const FbxTool::SNode* const pElement = elementStack.back();
		elementStack.pop_back();

		nodesInclusion[pElement->id] = bIncluded;
		
		for (int i = 0; i < pElement->numChildren; ++i)
		{
			elementStack.push_back(pElement->ppChildren[i]);
		}
	}
}

bool IsAncestor(const FbxTool::SNode* pSelf, const FbxTool::SNode* pAncestor)
{
	while (pSelf)
	{
		if (pSelf == pAncestor)
		{
			return true;
		}
		pSelf = pSelf->pParent;
	}
	return false;
}

} // namespace Private_SceneModelSkeleton

// ==================================================
// CNodeSkeleton
// ==================================================

void CSkeleton::SetScene(const FbxTool::CScene* pScene)
{
	using namespace Private_SceneModelSkeleton;

	const size_t maxNodeId = pScene->GetNodeCount() + 1;
	m_nodesInclusion.resize(maxNodeId, 0);

	m_pExportRoot = nullptr;
}

const FbxTool::SNode* CSkeleton::GetExportRoot() const
{
	return m_pExportRoot;
}

const std::vector<bool>& CSkeleton::GetNodesInclusion() const
{
	return m_nodesInclusion;
}

void CSkeleton::SetNodeInclusion(const FbxTool::SNode* pNode, bool bIncluded)
{
	if (m_nodesInclusion[pNode->id] == bIncluded)
	{
		return;  // Nothing changes.
	}

	if (!bIncluded)
	{
		Private_SceneModelSkeleton::SetNodeInclusion(pNode, m_nodesInclusion, false);
		if (pNode == m_pExportRoot)
		{
			m_pExportRoot = nullptr;
		}
	}
	else if (m_pExportRoot && Private_SceneModelSkeleton::IsAncestor(pNode, m_pExportRoot))
	{
		Private_SceneModelSkeleton::SetNodeInclusion(pNode, m_nodesInclusion, true);
	}
	else
	{
		if (m_pExportRoot)
		{
			Private_SceneModelSkeleton::SetNodeInclusion(m_pExportRoot, m_nodesInclusion, false);
		}
		Private_SceneModelSkeleton::SetNodeInclusion(pNode, m_nodesInclusion, true);
		m_pExportRoot = pNode;
	}

	if (m_callback)
	{
		m_callback();
	}
}

void CSkeleton::SetCallback(const Callback& callback)
{
	m_callback = callback;
}

// ==================================================
// CSceneModelSkeleton
// ==================================================

int CSceneModelSkeleton::columnCount(const QModelIndex& index) const
{
	return eColumnType_COUNT;
}

QVariant CSceneModelSkeleton::data(const QModelIndex& index, int role) const
{
	using namespace Private_SceneModelSkeleton;

	CRY_ASSERT(index.isValid());

	if (!GetScene())
	{
		return QVariant();
	}

	CSceneElementCommon* const pSceneElement = GetSceneElementFromModelIndex(index);

	const FbxTool::SNode* const pFbxNode = pSceneElement->GetType() == ESceneElementType::SourceNode ? ((CSceneElementSourceNode*)pSceneElement)->GetNode() : nullptr;

	if (role == Qt::DisplayRole)
	{
		switch (index.column())
		{
		case eColumnType_Name:
			return QtUtil::ToQString(pSceneElement->GetName());
		case eColumnType_SourceNodeAttribute:
			if (pFbxNode && !pFbxNode->namedAttributes.empty())
			{
				return QtUtil::ToQString(pFbxNode->namedAttributes[0]);
			}
			break;
		default:
			break;
		}
	}
	else if (role == Qt::CheckStateRole && pFbxNode && index.column() == eColumnType_Name)
	{
		return GetCheckStateRole(pFbxNode, m_pNodeSkeleton->GetNodesInclusion());
	}
	return CSceneModelCommon::data(index, role);
}

QVariant CSceneModelSkeleton::headerData(int column, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole)
	{
		switch (column)
		{
		case eColumnType_Name:
			return QString("Node name");
		case eColumnType_SourceNodeAttribute:
			return QString("Attribute");
		}
	}

	// Attributes.
	if (role == Attributes::s_getAttributeRole)
	{
		CItemModelAttribute* const pAttribute = GetColumnAttribute(column);
		if (pAttribute)
		{
			return QVariant::fromValue(pAttribute);
		}
	}

	return QVariant();
}

Qt::ItemFlags CSceneModelSkeleton::flags(const QModelIndex& modelIndex) const
{
	return Qt::ItemIsUserCheckable | CSceneModelCommon::flags(modelIndex);
}

bool CSceneModelSkeleton::setData(const QModelIndex& modelIndex, const QVariant& value, int role)
{
	if (!modelIndex.isValid())
	{
		return false;
	}

	if (role == Qt::CheckStateRole)
	{
		CSceneElementCommon* const pSceneElement = GetSceneElementFromModelIndex(modelIndex);

		if (pSceneElement->GetType() != ESceneElementType::SourceNode)
		{
			return false;
		}
		const auto checkState = (Qt::CheckState)value.toUInt();

		const FbxTool::SNode* const pNode = ((CSceneElementSourceNode*)pSceneElement)->GetNode();

		m_pNodeSkeleton->SetNodeInclusion(pNode, checkState == Qt::Checked);

		Reset();

		return true;
	}

	return false;
}

CItemModelAttribute* CSceneModelSkeleton::GetColumnAttribute(int col) const
{
	switch (col)
	{
	case eColumnType_Name:
		return &Attributes::s_nameAttribute;
	case eColumnType_SourceNodeAttribute:
		return GetSourceNodeAttributeAttribute();
	default:
		return nullptr;
	};
}

