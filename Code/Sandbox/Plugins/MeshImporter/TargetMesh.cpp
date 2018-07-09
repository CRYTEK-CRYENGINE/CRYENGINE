// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TargetMesh.h"
#include "QtCommon.h"
#include <IEditor.h>
#include <Cry3DEngine/I3DEngine.h>
#include "Cry3DEngine/CGF/CryHeaders.h"
#include <Cry3DEngine/CGF/CGFContent.h>

#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/STL.h>
#include <CrySerialization/yasli/Enum.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

void CTargetMeshModel::SItemProperties::Serialize(Serialization::IArchive& ar)
{
	ar(pItem->m_name, "name", "!Name");
}

CTargetMeshModel::CTargetMeshModel(QObject* pParent)
	: QAbstractItemModel(pParent)
{}

void CTargetMeshModel::LoadCgf(const char* filename)
{
	beginResetModel();

	m_items.clear();
	m_rootItems.clear();

	if (filename)
	{
		CContentCGF* pCGF = GetIEditor()->Get3DEngine()->CreateChunkfileContent(filename);
		if (GetIEditor()->Get3DEngine()->LoadChunkFileContent(pCGF, filename))
		{
			RebuildInternal(*pCGF);
		}
	}

	endResetModel();
}

void CTargetMeshModel::Clear()
{
	LoadCgf(nullptr);
}

CTargetMeshModel::SItemProperties* CTargetMeshModel::GetProperties(const QModelIndex& index)
{
	SItem* const pItem = (SItem*)index.internalPointer();
	return &pItem->m_properties;
}

const CTargetMeshModel::SItemProperties* CTargetMeshModel::GetProperties(const QModelIndex& index) const
{
	SItem* const pItem = (SItem*)index.internalPointer();
	return &pItem->m_properties;
}

QModelIndex CTargetMeshModel::index(int row, int column, const QModelIndex& parent) const
{
	if (parent.isValid())
	{
		SItem* const pParentItem = (SItem*)parent.internalPointer();
		return createIndex(row, 0, pParentItem->m_children[row]);
	}
	else
	{
		return createIndex(row, 0, m_rootItems[row]);
	}
}

QModelIndex CTargetMeshModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return QModelIndex();
	}

	SItem* const pItem = (SItem*)index.internalPointer();
	SItem* const pParentItem = pItem->m_pParent;
	if (pParentItem)
	{
		return createIndex(pItem->m_siblingIndex, 0, pParentItem);
	}
	else
	{
		return QModelIndex();
	}
}

int CTargetMeshModel::rowCount(const QModelIndex& index) const
{
	if (index.isValid())
	{
		const SItem* const pItem = (SItem*)index.internalPointer();
		return pItem->m_children.size();
	}
	else
	{
		return m_rootItems.size();
	}
}

int CTargetMeshModel::columnCount(const QModelIndex& index) const
{
	return eColumnType_COUNT;
}

QVariant CTargetMeshModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	if (role == Qt::DisplayRole)
	{
		const SItem* const pItem = (SItem*)index.internalPointer();
		return QLatin1String(pItem->m_name);
	}

	if (role == eItemDataRole_YasliSStruct)
	{
		std::unique_ptr<Serialization::SStruct> sstruct(new Serialization::SStruct(*GetProperties(index)));
		return QVariant::fromValue((void*)sstruct.release());
	}

	return QVariant();
}

QVariant CTargetMeshModel::headerData(int column, Qt::Orientation orientation, int role) const
{
	if (column == eColumnType_Name && role == Qt::DisplayRole)
	{
		return tr("Name");
	}
	return QVariant();
}

void CTargetMeshModel::RebuildInternal(CContentCGF& cgf)
{
	// Must avoid re-allocation of vector. Otherwise pointers to items might become invalid.
	m_items.clear();
	m_items.resize(cgf.GetNodeCount());

	for (int i = 0; i < cgf.GetNodeCount(); ++i)
	{
		CNodeCGF* pNode = cgf.GetNode(i);
		m_items[i].m_name = pNode->name;

		if (!pNode->pParent)
			continue;

		for (int j = 0; j < cgf.GetNodeCount(); ++j)
		{
			if (cgf.GetNode(j) == pNode->pParent)
			{
				m_items[j].AddChild(&m_items[i]);
				break;
			}
		}
	}

	m_rootItems.clear();
	for (auto& item : m_items)
	{
		if (!item.m_pParent)
		{
			m_rootItems.push_back(&item);
		}
	}
}

CTargetMeshModel* CTargetMeshView::model()
{
	return m_pModel.get();
}

void CTargetMeshView::reset()
{
	QAdvancedTreeView::reset();
	expandAll();
}

CTargetMeshView::CTargetMeshView(QWidget* pParent)
	: QAdvancedTreeView(QAdvancedTreeView::Behavior(QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset), pParent)
	, m_pModel(new CTargetMeshModel())
{
	setModel(m_pModel.get());
}

