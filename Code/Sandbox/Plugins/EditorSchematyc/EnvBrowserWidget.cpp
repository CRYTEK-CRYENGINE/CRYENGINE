// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Clean up includes!!!

#include "StdAfx.h"
#include "EnvBrowserWidget.h"

#include <CryIcon.h>
#include <ISourceControl.h>
#include <QBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton.h>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QWidgetAction>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/Elements/IEnvClass.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/Script/Elements/IScriptFunction.h>
#include <CrySchematyc/Script/Elements/IScriptModule.h>
#include <CrySchematyc/SerializationUtils/ISerializationContext.h>
#include <CrySchematyc/SerializationUtils/IValidatorArchive.h>
#include <CrySchematyc/Utils/StackString.h>
#include <Serialization/Qt.h>

#include "PluginUtils.h"

namespace Schematyc
{
namespace
{
const char* GetElementIcon(EEnvElementType elementType)
{
	return "";
}
}

CEnvBrowserItem::CEnvBrowserItem(const CryGUID& guid, const char* szName, const char* szIcon)
	: m_guid(guid)
	, m_name(szName)
	, m_iconName(szIcon)
	, m_pParent(nullptr)
{}

CryGUID CEnvBrowserItem::GetGUID() const
{
	return m_guid;
}

const char* CEnvBrowserItem::GetName() const
{
	return m_name.c_str();
}

const char* CEnvBrowserItem::GetIcon() const
{
	return m_iconName.c_str();
}

CEnvBrowserItem* CEnvBrowserItem::GetParent()
{
	return m_pParent;
}

void CEnvBrowserItem::AddChild(const CEnvBrowserItemPtr& pChild)
{
	m_children.push_back(pChild);
	pChild->m_pParent = this;
}

void CEnvBrowserItem::RemoveChild(CEnvBrowserItem* pChild)
{
	auto matchItemPtr = [&pChild](const CEnvBrowserItemPtr& pItem) -> bool
	{
		return pItem.get() == pChild;
	};
	m_children.erase(std::remove_if(m_children.begin(), m_children.end(), matchItemPtr));
}

int CEnvBrowserItem::GetChildCount() const
{
	return static_cast<int>(m_children.size());
}

int CEnvBrowserItem::GetChildIdx(CEnvBrowserItem* pChild)
{
	for (int childIdx = 0, childCount = m_children.size(); childIdx < childCount; ++childIdx)
	{
		if (m_children[childIdx].get() == pChild)
		{
			return childIdx;
		}
	}
	return -1;
}

CEnvBrowserItem* CEnvBrowserItem::GetChildByIdx(int childIdx)
{
	return childIdx < m_children.size() ? m_children[childIdx].get() : nullptr;
}

class CEnvBrowserFilter : public QSortFilterProxyModel
{
public:

	inline CEnvBrowserFilter(QObject* pParent, CEnvBrowserModel& model)
		: QSortFilterProxyModel(pParent)
		, m_model(model)
		, m_pFocusItem(nullptr)
	{}

	inline void SetFilterText(const char* szFilterText)
	{
		m_filterText = szFilterText;
	}

	inline void SetFocus(const QModelIndex& index)
	{
		m_pFocusItem = m_model.ItemFromIndex(index);
	}

	inline QModelIndex GetFocus() const
	{
		return m_model.ItemToIndex(m_pFocusItem);
	}

	// QSortFilterProxyModel

	virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override
	{
		QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
		CEnvBrowserItem* pItem = m_model.ItemFromIndex(index);
		if (pItem)
		{
			return FilterItemByFocus(pItem) && FilterItemByTextRecursive(pItem);
		}
		return false;
	}

	virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
	{
		return QSortFilterProxyModel::lessThan(left, right);
	}

	// ~QSortFilterProxyModel

private:

	inline bool FilterItemByFocus(CEnvBrowserItem* pItem) const
	{
		return !m_pFocusItem || (pItem == m_pFocusItem) || (pItem->GetParent() != m_pFocusItem->GetParent());
	}

	inline bool FilterItemByTextRecursive(CEnvBrowserItem* pItem) const   // #SchematycTODO : Keep an eye on performance of this test!
	{
		if (StringUtils::Filter(pItem->GetName(), m_filterText.c_str()))
		{
			return true;
		}
		for (int childIdx = 0, childCount = pItem->GetChildCount(); childIdx < childCount; ++childIdx)
		{
			if (FilterItemByTextRecursive(pItem->GetChildByIdx(childIdx)))
			{
				return true;
			}
		}
		return false;
	}

private:

	CEnvBrowserModel& m_model;
	string            m_filterText;
	CEnvBrowserItem*  m_pFocusItem;
};

CEnvBrowserModel::CEnvBrowserModel(QObject* pParent)
	: QAbstractItemModel(pParent)
{
	Populate();
}

QModelIndex CEnvBrowserModel::index(int row, int column, const QModelIndex& parent) const
{
	CEnvBrowserItem* pParentItem = ItemFromIndex(parent);
	if (pParentItem)
	{
		CEnvBrowserItem* pChildItem = pParentItem->GetChildByIdx(row);
		return pChildItem ? QAbstractItemModel::createIndex(row, column, pChildItem) : QModelIndex();
	}
	else if (row < m_items.size())
	{
		return QAbstractItemModel::createIndex(row, column, m_items[row].get());
	}
	return QModelIndex();
}

QModelIndex CEnvBrowserModel::parent(const QModelIndex& index) const
{
	CEnvBrowserItem* pItem = ItemFromIndex(index);
	if (pItem)
	{
		CEnvBrowserItem* pParentItem = pItem->GetParent();
		if (pParentItem)
		{
			return ItemToIndex(pParentItem, index.column());
		}
	}
	return QModelIndex();
}

int CEnvBrowserModel::rowCount(const QModelIndex& parent) const
{
	CEnvBrowserItem* pParentItem = ItemFromIndex(parent);
	if (pParentItem)
	{
		return pParentItem->GetChildCount();
	}
	else
	{
		return m_items.size();
	}
}

int CEnvBrowserModel::columnCount(const QModelIndex& parent) const
{
	return EEnvBrowserColumn::Count;
}

bool CEnvBrowserModel::hasChildren(const QModelIndex& parent) const
{
	CEnvBrowserItem* pParentItem = ItemFromIndex(parent);
	if (pParentItem)
	{
		return pParentItem->GetChildCount() > 0;
	}
	else
	{
		return true;
	}
}

QVariant CEnvBrowserModel::data(const QModelIndex& index, int role) const
{
	CEnvBrowserItem* pItem = ItemFromIndex(index);
	if (pItem)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			{
				switch (index.column())
				{
				case EEnvBrowserColumn::Name:
					{
						return QString(pItem->GetName());
					}
				}
				break;
			}
		case Qt::DecorationRole:
			{
				switch (index.column())
				{
				case EEnvBrowserColumn::Name:
					{
						return CryIcon(pItem->GetIcon());
					}
				}
				break;
			}
		}
	}
	return QVariant();
}

bool CEnvBrowserModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	return false;
}

QVariant CEnvBrowserModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			{
				switch (section)
				{
				case EEnvBrowserColumn::Name:
					{
						return QString("Name");
					}
				}
				break;
			}
		}
	}
	return QVariant();
}

Qt::ItemFlags CEnvBrowserModel::flags(const QModelIndex& index) const
{
	return QAbstractItemModel::flags(index);
}

QModelIndex CEnvBrowserModel::ItemToIndex(CEnvBrowserItem* pItem, int column) const
{
	if (pItem)
	{
		CEnvBrowserItem* pParentItem = pItem->GetParent();
		if (pParentItem)
		{
			return QAbstractItemModel::createIndex(pParentItem->GetChildIdx(pItem), column, pItem);
		}
		else
		{
			return QAbstractItemModel::createIndex(0, column, pItem);
		}
	}
	return QModelIndex();
}

CEnvBrowserItem* CEnvBrowserModel::ItemFromIndex(const QModelIndex& index) const
{
	return static_cast<CEnvBrowserItem*>(index.internalPointer());
}

CEnvBrowserItem* CEnvBrowserModel::ItemFromGUID(const CryGUID& guid) const
{
	ItemsByGUID::const_iterator itItem = m_itemsByGUID.find(guid);
	return itItem != m_itemsByGUID.end() ? itItem->second : nullptr;
}

void CEnvBrowserModel::Populate()
{
	auto visitEnvElement = [this](const IEnvElement& envElement) -> EVisitStatus
	{
		// #SchematycTODO : Would be better if we could filter deprecated/non-deprecated elements and use an icon rather than text to indicate status!!!

		CStackString name = envElement.GetName();
		if (envElement.GetFlags().Check(EEnvElementFlags::Deprecated))
		{
			name.append(" [DEPRECATED]");
		}

		const CryGUID guid = envElement.GetGUID();
		CEnvBrowserItemPtr pItem = std::make_shared<CEnvBrowserItem>(guid, name.c_str(), GetElementIcon(envElement.GetType()));
		const IEnvElement* pParentEnvElement = envElement.GetParent();
		if (pParentEnvElement && (pParentEnvElement->GetType() != EEnvElementType::Root))
		{
			CEnvBrowserItem* pParentItem = ItemFromGUID(pParentEnvElement->GetGUID());
			if (pParentItem)
			{
				pParentItem->AddChild(pItem);
			}
			else
			{
				SCHEMATYC_EDITOR_CRITICAL_ERROR("Failed to find parent for environment element: name = %s", pItem->GetName());
			}
		}
		else
		{
			m_items.push_back(pItem);
		}
		m_itemsByGUID.insert(ItemsByGUID::value_type(guid, pItem.get()));
		return EVisitStatus::Recurse;
	};
	gEnv->pSchematyc->GetEnvRegistry().GetRoot().VisitChildren(visitEnvElement);
}

CEnvBrowserWidget::CEnvBrowserWidget(QWidget* pParent)
	: QWidget(pParent)
{
	m_pMainLayout = new QBoxLayout(QBoxLayout::TopToBottom);
	m_pFilterLayout = new QBoxLayout(QBoxLayout::LeftToRight);
	m_pSearchFilter = new QLineEdit(this);
	m_pTreeView = new QAdvancedTreeView(QAdvancedTreeView::Behavior(QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset), this);
	m_pModel = new CEnvBrowserModel(this);
	m_pFilter = new CEnvBrowserFilter(this, *m_pModel);

	m_pSearchFilter->setPlaceholderText("Search");   // #SchematycTODO : Add drop down history to search filter?

	m_pFilter->setDynamicSortFilter(true);
	m_pFilter->setSourceModel(m_pModel);

	m_pTreeView->setModel(m_pFilter);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setEditTriggers(QAbstractItemView::SelectedClicked);
	m_pTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pTreeView->setExpandsOnDoubleClick(false);

	connect(m_pSearchFilter, SIGNAL(textChanged(const QString &)), this, SLOT(OnSearchFilterChanged(const QString &)));
	connect(m_pTreeView, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(OnTreeViewCustomContextMenuRequested(const QPoint &)));
}

CEnvBrowserWidget::~CEnvBrowserWidget()
{
	m_pTreeView->setModel(nullptr);
}

void CEnvBrowserWidget::InitLayout()
{
	QWidget::setLayout(m_pMainLayout);
	m_pMainLayout->setSpacing(1);
	m_pMainLayout->setMargin(1);
	m_pMainLayout->addLayout(m_pFilterLayout, 0);
	m_pMainLayout->addWidget(m_pTreeView, 1);

	m_pFilterLayout->setSpacing(2);
	m_pFilterLayout->setMargin(4);
	m_pFilterLayout->addWidget(m_pSearchFilter, 0);
}

void CEnvBrowserWidget::OnSearchFilterChanged(const QString& text)
{
	if (m_pSearchFilter)
	{
		m_pFilter->SetFilterText(text.toStdString().c_str());
		m_pFilter->invalidate();
		ExpandAll();
	}
}

void CEnvBrowserWidget::OnTreeViewCustomContextMenuRequested(const QPoint& position) {}

void CEnvBrowserWidget::ExpandAll()
{
	m_pTreeView->expandAll();
}
} // Schematyc

