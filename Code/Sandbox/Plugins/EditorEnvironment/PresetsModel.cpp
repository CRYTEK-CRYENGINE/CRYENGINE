// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <QStyle>
#include <QIcon>
#include <CryIcon.h>

#include "PresetsModel.h"
#include "QPresetsWidget.h"

CPresetsModel::CPresetsModel(QPresetsWidget& presetsWidget, QObject* pParent)
	: QAbstractItemModel(pParent)
	, m_presetsWidget(presetsWidget)
{
	connect(&presetsWidget, &QPresetsWidget::SignalBeginAddEntryNode, this, &CPresetsModel::OnSignalBeginAddEntryNode);
	connect(&presetsWidget, &QPresetsWidget::SignalEndAddEntryNode, this, &CPresetsModel::OnSignalEndAddEntryNode);
	connect(&presetsWidget, &QPresetsWidget::SignalBeginDeleteEntryNode, this, &CPresetsModel::OnSignalBeginDeleteEntryNode);
	connect(&presetsWidget, &QPresetsWidget::SignalEndDeleteEntryNode, this, &CPresetsModel::OnSignalEndDeleteEntryNode);
	connect(&presetsWidget, &QPresetsWidget::SignalBeginResetModel, this, &CPresetsModel::OnSignalBeginResetModel);
	connect(&presetsWidget, &QPresetsWidget::SignalEndResetModel, this, &CPresetsModel::OnSignalEndResetModel);
	connect(&presetsWidget, &QPresetsWidget::SignalEntryNodeDataChanged, this, &CPresetsModel::OnSignalEntryNodeDataChanged);
}

QModelIndex CPresetsModel::index(int row, int column, const QModelIndex& parent) const
{
	if ((row < 0) || (column < 0))
		return QModelIndex();

	CPresetsModelNode* pNode = GetEntryNode(parent);
	if (pNode == nullptr)
	{
		pNode = m_presetsWidget.GetRoot();
	}
	assert(pNode != nullptr);

	if (size_t(row) < pNode->children.size())
	{
		return createIndex(row, column, pNode->children[row]);
	}

	return QModelIndex();
}

Qt::ItemFlags CPresetsModel::flags(const QModelIndex& index) const
{
	if (index.isValid())
	{
		if (index.column() == eColumn_PakStatus)
		{
			return QAbstractItemModel::flags(index) & ~Qt::ItemIsSelectable;
		}

		CPresetsModelNode* pNode = GetEntryNode(index);

		if (pNode && (pNode->type == CPresetsModelNode::EType_Group))
		{
			return QAbstractItemModel::flags(index) & ~Qt::ItemIsSelectable;
		}
	}

	return QAbstractItemModel::flags(index);
}

int CPresetsModel::rowCount(const QModelIndex& parent) const
{
	CPresetsModelNode* pNode = GetEntryNode(parent);
	if (pNode == nullptr)
	{
		pNode = m_presetsWidget.GetRoot();
	}
	assert(pNode != nullptr);

	return static_cast<int>(pNode->children.size());
}

int CPresetsModel::columnCount(const QModelIndex& parent) const
{
	return eColumn_COUNT;
}

QVariant CPresetsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
	{
		return QString(GetHeaderSectionName(section));
	}
	else if (role == Qt::SizeHintRole && orientation == Qt::Horizontal)
	{
		if (section == eColumn_Name)
		{
			return QSize(130, 20);
		}
		else if (section == eColumn_PakStatus)
		{
			return QSize(20, 20);
		}
	}

	return QVariant();
}

bool CPresetsModel::hasChildren(const QModelIndex& parent) const
{
	CPresetsModelNode* pNode = GetEntryNode(parent);
	if (pNode == nullptr)
	{
		pNode = m_presetsWidget.GetRoot();
	}
	assert(pNode != nullptr);

	return !pNode->children.empty();
}

QVariant CPresetsModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	CPresetsModelNode* pNode = GetEntryNode(index);
	if (pNode == nullptr)
	{
		return QVariant();
	}

	switch (role)
	{
	case Qt::DisplayRole:
		{
			if (index.column() == eColumn_Name)
			{
				if ((pNode->type == CPresetsModelNode::EType_Leaf) && pNode->flags.AreAnyFlagsActive(CPresetsModelNode::EFlags_Modified))
					return QString(QString(pNode->name.c_str()) + QString(" *"));

				return QString(pNode->name.c_str());
			}
		}
		break;
	case Qt::EditRole:
		{
			if (index.column() == eColumn_Name)
			{
				return QString(pNode->name.c_str());
			}
		}
		break;
	case Qt::FontRole:
		{
			if ((index.column() == eColumn_Name))
			{
				if (pNode->type == CPresetsModelNode::EType_Leaf)
				{
					QFont font;
					if (pNode->flags.AreAllFlagsActive(CPresetsModelNode::EFlags_Modified))
						font.setItalic(true);
					return font;
				}
			}
		}
		break;
	case Qt::SizeHintRole:
		{
			if (index.column() == eColumn_Name)
			{
				return QSize(130, 20);
			}
			else if (index.column() == eColumn_PakStatus)
			{
				return QSize(20, 20);
			}
		}
		break;
	case Qt::DecorationRole:
		{
			if (index.column() == eColumn_Name)
			{
				if (pNode->type == CPresetsModelNode::EType_Group)
					return CryIcon("icons:General/Folder.ico");
				else if (pNode->type == CPresetsModelNode::EType_Leaf)
					return CryIcon("icons:General/File.ico");
			}
			else if (index.column() == eColumn_PakStatus)
			{
				if (pNode->type == CPresetsModelNode::EType_Leaf)
				{
					if (pNode->flags.AreAllFlagsActive(CPresetsModelNode::EFlags_InPak | CPresetsModelNode::EFlags_InFolder))
						return CryIcon("icons:General/Pakfile_Folder.ico");
					if (pNode->flags.AreAnyFlagsActive(CPresetsModelNode::EFlags_InPak))
						return CryIcon("icons:General/Pakfile.ico");
					if (pNode->flags.AreAnyFlagsActive(CPresetsModelNode::EFlags_InFolder))
						return CryIcon("icons:General/Folder.ico");

					QStyle* style = m_presetsWidget.style();
					return style->standardIcon(QStyle::SP_MessageBoxCritical);
				}
			}
		}
		break;
	case Qt::ToolTipRole:
		{
			if (index.column() == eColumn_PakStatus && (pNode->type == CPresetsModelNode::EType_Leaf))
			{
				if (pNode->flags.AreAllFlagsActive(CPresetsModelNode::EFlags_InPak | CPresetsModelNode::EFlags_InFolder))
					return QString("In pak and in folder");
				if (pNode->flags.AreAnyFlagsActive(CPresetsModelNode::EFlags_InPak))
					return QString("In pak");
				if (pNode->flags.AreAnyFlagsActive(CPresetsModelNode::EFlags_InFolder))
					return QString("In folder");

				return QString("Not on disk");
			}
		}
		break;
	}

	return QVariant();
}

bool CPresetsModel::setData(const QModelIndex& index, const QVariant& value, int role /* = Qt::EditRole */)
{
	return false;
}

QModelIndex CPresetsModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return QModelIndex();

	CPresetsModelNode* pNode = GetEntryNode(index);
	if (pNode == nullptr)
		return QModelIndex();
	if (pNode == m_presetsWidget.GetRoot())
		return QModelIndex();

	CPresetsModelNode* pParentEntryNode = pNode->pParent;
	if (pParentEntryNode == nullptr)
		return QModelIndex();

	if (pParentEntryNode == m_presetsWidget.GetRoot())
		return QModelIndex();

	return ModelIndexFromNode(pParentEntryNode);
}

const char* CPresetsModel::GetHeaderSectionName(int section) const
{
	if (section == eColumn_Name)
	{
		return "Name";
	}
	else if (section == eColumn_PakStatus)
	{
		return "Pak";
	}

	return "";
}

QModelIndex CPresetsModel::ModelIndexFromNode(CPresetsModelNode* pNode) const
{
	assert(pNode != nullptr);
	assert(pNode->pParent != nullptr);

	for (size_t i = 0, childCount = pNode->pParent->children.size(); i < childCount; ++i)
		if (pNode->pParent->children[i] == pNode)
			return createIndex(i, 0, pNode);

	assert(0);
	return createIndex(0, 0, pNode);
}

CPresetsModelNode* CPresetsModel::GetEntryNode(const QModelIndex& index)
{
	return static_cast<CPresetsModelNode*>(index.internalPointer());
}

void CPresetsModel::OnSignalBeginAddEntryNode(CPresetsModelNode* pEntryNode)
{
	assert(pEntryNode->pParent != nullptr);

	const QModelIndex parentIndex = (pEntryNode->pParent == m_presetsWidget.GetRoot()) ? QModelIndex() : ModelIndexFromNode(pEntryNode->pParent);
	const int newRowIndex = pEntryNode->pParent->children.size();

	beginInsertRows(parentIndex, newRowIndex, newRowIndex);
}

void CPresetsModel::OnSignalEndAddEntryNode(CPresetsModelNode* pEntryNode)
{
	endInsertRows();
}

void CPresetsModel::OnSignalBeginDeleteEntryNode(CPresetsModelNode* pEntryNode)
{
	assert(pEntryNode != nullptr);
	assert(pEntryNode->pParent != nullptr);

	QModelIndex parentIndex = (pEntryNode->pParent == m_presetsWidget.GetRoot()) ? QModelIndex() : ModelIndexFromNode(pEntryNode->pParent);
	QModelIndex index = ModelIndexFromNode(pEntryNode);

	assert(pEntryNode == GetEntryNode(index));

	beginRemoveRows(parentIndex, index.row(), index.row());
}

void CPresetsModel::OnSignalEndDeleteEntryNode()
{
	endRemoveRows();
}

void CPresetsModel::OnSignalBeginResetModel()
{
	beginResetModel();
}

void CPresetsModel::OnSignalEndResetModel()
{
	endResetModel();
}

void CPresetsModel::OnSignalEntryNodeDataChanged(CPresetsModelNode* pEntryNode)
{
	assert(pEntryNode != nullptr);
	QModelIndex index = ModelIndexFromNode(pEntryNode);
	dataChanged(index, index);
}

