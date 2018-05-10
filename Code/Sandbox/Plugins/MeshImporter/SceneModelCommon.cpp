// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneModelCommon.h"
#include "FbxScene.h"
#include "Scene/SceneElementCommon.h"
#include "Scene/SceneElementSourceNode.h"
#include "Scene/Scene.h"
#include "Scene/SourceSceneHelper.h"
#include "ImporterUtil.h"

// ==================================================
// CSceneModelCommon::ISceneBuilder
// ==================================================

struct CSceneModelCommon::ISceneBuilder
{
	virtual ~ISceneBuilder() {}

	virtual QModelIndex index(const CSceneModelCommon* pModel, int row, int column, const QModelIndex& parent) const = 0;
	virtual QModelIndex parent(const CSceneModelCommon* pModel, const QModelIndex& index) const = 0;
	virtual int         rowCount(const CSceneModelCommon* pModel, const QModelIndex& index) const = 0;
};

// ==================================================
// CSceneModelWithPseudoRoot
// ==================================================

struct CSceneModelCommon::CSceneModelWithPseudoRoot : CSceneModelCommon::ISceneBuilder
{
	CSceneModelWithPseudoRoot() {}

	// QAbstractItemModel implementation.
	virtual QModelIndex index(const CSceneModelCommon* pModel, int row, int col, const QModelIndex& parentIdx) const override
	{
		if (pModel->IsSceneEmpty())
		{
			return QModelIndex();
		}

		if (parentIdx.isValid())
		{
			return pModel->createIndex(row, col, pModel->GetSceneElementFromModelIndex(parentIdx)->GetChild(row));
		}
		else
		{
			return pModel->createIndex(row, col, const_cast<CSceneElementCommon*>(pModel->GetRoot()));
		}
	}

	virtual QModelIndex parent(const CSceneModelCommon* pModel, const QModelIndex& index) const override
	{
		if (index.isValid())
		{
			CSceneElementCommon* const pSelf = pModel->GetSceneElementFromModelIndex(index);
			CSceneElementCommon* const pParent = pSelf->GetParent();
			if (pParent)
			{
				if (pParent->GetParent())
				{
					return pModel->createIndex(pParent->GetSiblingIndex(), 0, pParent);
				}
				else
				{
					return pModel->createIndex(0, 0, pParent);
				}
			}
		}
		return QModelIndex();
	}

	virtual int rowCount(const CSceneModelCommon* pModel, const QModelIndex& index) const override
	{
		if (pModel->IsSceneEmpty())
		{
			return 0;
		}

		if (index.isValid())
		{
			return pModel->GetSceneElementFromModelIndex(index)->GetNumChildren();
		}
		else
		{
			return 1;
		}
	}
};

// ==================================================
// CSceneModelWithoutPseudoRoot
// ==================================================

struct CSceneModelCommon::CSceneModelWithoutPseudoRoot : CSceneModelCommon::ISceneBuilder
{
	CSceneModelWithoutPseudoRoot() {}

	// QAbstractItemModel implementation.
	virtual QModelIndex index(const CSceneModelCommon* pModel, int row, int col, const QModelIndex& parentIdx) const override
	{
		if (pModel->IsSceneEmpty())
		{
			return QModelIndex();
		}

		if (parentIdx.isValid())
		{
			return pModel->createIndex(row, col, pModel->GetSceneElementFromModelIndex(parentIdx)->GetChild(row));
		}
		else if (row >= 0 && row < pModel->GetRoot()->GetNumChildren())
		{
			return pModel->createIndex(row, col, pModel->GetRoot()->GetChild(row));
		}

		return QModelIndex();
	}

	virtual QModelIndex parent(const CSceneModelCommon* pModel, const QModelIndex& index) const override
	{
		if (index.isValid())
		{
			CSceneElementCommon* const pSelf = pModel->GetSceneElementFromModelIndex(index);
			CSceneElementCommon* const pParent = pSelf->GetParent();
			CRY_ASSERT(pParent);
			if (pParent != pModel->GetRoot())
			{
				return pModel->createIndex(pParent->GetSiblingIndex(), 0, pParent);
			}
		}
		return QModelIndex();
	}

	virtual int rowCount(const CSceneModelCommon* pModel, const QModelIndex& index) const override
	{
		if (pModel->IsSceneEmpty())
		{
			return 0;
		}

		if (index.isValid())
		{
			return pModel->GetSceneElementFromModelIndex(index)->GetNumChildren();
		}
		else
		{
			return pModel->GetRoot()->GetNumChildren();
		}
	}
};

// ==================================================
// CSceneModelCommon
// ==================================================

//! Same as CItemModelAttributeEnumFunc, but does not cache entries; the populate method is invoked every time.
//! Use this if the enumeration is dynamic.
class CItemModelAttributeEnumFuncDynamic : public CItemModelAttributeEnum
{
public:
	CItemModelAttributeEnumFuncDynamic(const char* szName, std::function<QStringList()> populateFunc, Visibility visibility = CItemModelAttribute::Visible, bool filterable = true, QVariant defaultFilterValue = QVariant())
		: CItemModelAttributeEnum(szName, QStringList(), visibility, filterable, defaultFilterValue)
		, m_populateFunc(populateFunc)
	{}

	virtual QStringList GetEnumEntries() const override
	{
		const_cast<CItemModelAttributeEnumFuncDynamic*>(this)->Populate();
		return m_enumEntries;
	}

private:
	void Populate()
	{
		m_enumEntries = m_populateFunc();
	}

private:
	std::function<QStringList()> m_populateFunc;
};

static QStringList GetSourceAttributes(const FbxTool::CScene* pFbxScene)
{
	std::vector<string> attribs;
	for (int i = 0, N = pFbxScene->GetNodeCount(); i < N; ++i)
	{
		const FbxTool::SNode* pFbxNode = pFbxScene->GetNodeByIndex(i);
		if (!pFbxNode->namedAttributes.empty())
		{
			stl::push_back_unique(attribs, pFbxNode->namedAttributes[0]);
		}
	}
	return ToStringList(attribs);
}

CSceneModelCommon::CSceneModelCommon(QObject* pParent /* = nullptr */)
	: QAbstractItemModel(pParent)
	, m_pScene(new CScene())
	, m_pFbxScene(nullptr)
	, m_pRoot(nullptr)
{
	m_pSceneBuilder.reset(new CSceneModelWithPseudoRoot());

	auto populateSourceAttribute = [this]()
	{
		return m_pFbxScene ? GetSourceAttributes(m_pFbxScene) : QStringList();
	};
	m_pSourceNodeAttributeAttribute.reset(new CItemModelAttributeEnumFuncDynamic("Source attribute", populateSourceAttribute, CItemModelAttribute::StartHidden));
}

CItemModelAttribute* CSceneModelCommon::GetSourceNodeAttributeAttribute() const
{
	return m_pSourceNodeAttributeAttribute.get();
}

CSceneModelCommon::~CSceneModelCommon()
{}

void CSceneModelCommon::SetPseudoRootVisible(bool bVisible)
{
	if (bVisible)
	{
		m_pSceneBuilder.reset(new CSceneModelWithPseudoRoot());
	}
	else
	{
		m_pSceneBuilder.reset(new CSceneModelWithoutPseudoRoot());
	}
}

void CSceneModelCommon::SetScene(FbxTool::CScene* pScene)
{
	ClearSceneWithoutReset();

	m_pFbxScene = pScene;

	const FbxTool::SNode* const pFbxRootNode = m_pFbxScene->GetRootNode();

	m_pRoot = CreateSceneFromSourceScene(*m_pScene, *m_pFbxScene);

	beginResetModel();
	endResetModel();

	m_pScene->signalHierarchyChanged.Connect(this, &CSceneModelCommon::Reset);
}

void CSceneModelCommon::ClearScene()
{
	m_pScene->signalHierarchyChanged.DisconnectObject(this);

	beginResetModel();
	ClearSceneWithoutReset();
	endResetModel();
}

void CSceneModelCommon::Reset()
{
	beginResetModel();
	endResetModel();
}

QVariant CSceneModelCommon::data(const QModelIndex& index, int role) const
{
	if (index.isValid() && role == eRole_InternalPointerRole)
	{
		return QVariant((intptr_t)GetSceneElementFromModelIndex(index));
	}
	return QVariant();
}

QModelIndex CSceneModelCommon::index(int row, int column, const QModelIndex& parent) const
{
	return m_pSceneBuilder->index(this, row, column, parent);
}

QModelIndex CSceneModelCommon::parent(const QModelIndex& index) const
{
	return m_pSceneBuilder->parent(this, index);
}

int CSceneModelCommon::rowCount(const QModelIndex& index) const
{
	return m_pSceneBuilder->rowCount(this, index);
}

const CSceneElementCommon* CSceneModelCommon::GetRoot() const
{
	return m_pRoot;
}

CSceneElementSourceNode* CSceneModelCommon::FindSceneElementOfNode(const FbxTool::SNode* pNode)
{
	return ::FindSceneElementOfNode(*m_pScene, pNode);
}

CSceneElementCommon* CSceneModelCommon::GetSceneElementFromModelIndex(const QModelIndex& index) const
{
	assert(index.isValid());
	return static_cast<CSceneElementCommon*>(index.internalPointer());
}

QModelIndex CSceneModelCommon::GetModelIndexFromSceneElement(CSceneElementCommon* pElement) const
{
	assert(pElement);
	return createIndex(pElement->GetSiblingIndex(), 0, pElement);
}

int CSceneModelCommon::GetElementCount() const
{
	return (int)m_pScene->GetElementCount();
}

CSceneElementCommon* CSceneModelCommon::GetElementById(int id)
{
	return m_pScene->GetElementByIndex(id);
}

CSceneElementSourceNode* CSceneModelCommon::GetRootElement()
{
	return m_pRoot;
}

bool CSceneModelCommon::IsSceneEmpty() const
{
	return !m_pRoot;
}

CScene* CSceneModelCommon::GetModelScene()
{
	return m_pScene.get();
}

const FbxTool::CScene* CSceneModelCommon::GetScene() const
{
	return m_pFbxScene;
}

FbxTool::CScene* CSceneModelCommon::GetScene()
{
	return m_pFbxScene;
}

void CSceneModelCommon::ClearSceneWithoutReset()
{
	m_pScene->Clear();
	m_pRoot = nullptr;
	m_pFbxScene = nullptr;
}

