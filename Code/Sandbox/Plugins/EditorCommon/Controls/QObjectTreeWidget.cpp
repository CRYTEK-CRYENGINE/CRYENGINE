// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QObjectTreeWidget.h"

#include <QBoxLayout>
#include <QTreeView>
#include <QStandardItemModel>
#include <QDrag>
#include <QModelIndex>
#include <QDragEnterEvent>
#include <QHeaderView>
#include <QStyleOption>
#include <QPainter>
#include <QSplitter>

#include "ProxyModels/DeepFilterProxyModel.h"
#include "QSearchBox.h"
#include "DragDrop.h"
#include "QtUtil.h"

QObjectTreeView::QObjectTreeView(QWidget* pParent /*= nullptr*/)
	: QAdvancedTreeView(BehaviorFlags(PreserveExpandedAfterReset | PreserveSelectionAfterReset), pParent)
{
}

void QObjectTreeView::startDrag(Qt::DropActions supportedActions)
{
	QModelIndexList indexes = selectionModel()->selectedIndexes();
	if (m_indexFilterFn)
		m_indexFilterFn(indexes);

	// If there are no valid indices to drag then just return
	if (indexes.empty())
		return;

	QDrag* pDrag = new QDrag(this);
	CDragDropData* pMimeData = new CDragDropData();

	QStringList engineFP;
	QSet<qint64> idsUsed;
	for (QModelIndex index : indexes)
	{
		if (!idsUsed.contains(index.internalId()))
		{
			engineFP << index.data(Roles::Id).toString();
			idsUsed.insert(index.internalId());
		}
	}

	QByteArray byteArray;
	QDataStream stream(&byteArray, QIODevice::ReadWrite);
	stream << engineFP;

	pMimeData->SetCustomData("EngineFilePaths", byteArray);
	pDrag->setMimeData(pMimeData);
	pDrag->exec(supportedActions);
}

QObjectTreeWidget::QObjectTreeWidget(QWidget* pParent /*= nullptr*/, const char* szRegExp /* = "[/\\\\.]"*/)
	: QWidget(pParent)
	, m_regExp(szRegExp)
	, m_bIsDragTracked(false)
{
	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->setSpacing(0);
	pMainLayout->setMargin(0);

	m_pModel = new QStandardItemModel();
	m_pModel->setHorizontalHeaderLabels(QStringList("Name"));
	QDeepFilterProxyModel::BehaviorFlags behavior = QDeepFilterProxyModel::AcceptIfChildMatches | QDeepFilterProxyModel::AcceptIfParentMatches;
	m_pProxy = new QDeepFilterProxyModel(behavior);
	m_pProxy->setSourceModel(m_pModel);
	m_pProxy->setSortRole(QObjectTreeView::Sort);

	m_pTreeView = new QObjectTreeView(this);
	m_pTreeView->setDragDropMode(QAbstractItemView::DragOnly);

	m_pTreeView->setModel(m_pProxy);
	m_pTreeView->setSortingEnabled(true);
	m_pTreeView->header()->setStretchLastSection(true);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);

	// if only one widget is inside the splitter it will span the entire splitter size
	m_pSplitter = new QSplitter(this);
	m_pSplitter->setOrientation(Qt::Vertical);
	m_pSplitter->addWidget(m_pTreeView);

	QSearchBox* pSearchBox = new QSearchBox();
	pSearchBox->SetModel(m_pProxy);
	pSearchBox->EnableContinuousSearch(true);

	auto searchFunction = [=](const QString& text)
	{
		m_pProxy->setFilterFixedString(text);

		if (!text.isEmpty())
		{
			m_pTreeView->expandAll();
		}
	};

	pSearchBox->SetSearchFunction(std::function<void(const QString&)>(searchFunction));

	m_pToolLayout = new QHBoxLayout();
	m_pToolLayout->setSpacing(1);
	m_pToolLayout->setContentsMargins(0, 0, 0, 0);
	m_pToolLayout->addWidget(pSearchBox);

	connect(m_pTreeView, &QAbstractItemView::clicked, [=](const QModelIndex& index)
	{
		QVariant typeVariant = index.data(QObjectTreeView::NodeType);
		if (!typeVariant.isValid())
		{
		  return;
		}

		const auto nodeType = typeVariant.value<NodeType>();
		if (nodeType == Entry)
		{
		  signalOnClickFile(index.data(QObjectTreeView::Id).toString().toLocal8Bit());
		}
	});

	connect(m_pTreeView, &QAbstractItemView::doubleClicked, [=](const QModelIndex& index)
	{
		QVariant typeVariant = index.data(QObjectTreeView::NodeType);
		if (!typeVariant.isValid())
		{
		  return;
		}

		const auto nodeType = typeVariant.value<NodeType>();
		if (nodeType == Entry)
		{
		  signalOnDoubleClickFile(index.data(QObjectTreeView::Id).toString().toLocal8Bit());
		}
	});

	m_pTreeView->SetIndexFilter([&](QModelIndexList& indexes)
	{
		for (int i = indexes.count() - 1; i >= 0; --i)
		{
		  QVariant var = indexes.at(i).data(QObjectTreeView::Roles::Id);
		  if (var.isValid())
		  {
		    QString guid = var.toString();
		    if (guid.isEmpty())
					indexes.removeAt(i);
		  }
		  else
				indexes.removeAt(i);
		}
	});

	pMainLayout->addLayout(m_pToolLayout);
	pMainLayout->addWidget(m_pSplitter);

	setLayout(pMainLayout);
	setAcceptDrops(true);
}

QStringList QObjectTreeWidget::GetSelectedIds() const
{
	QModelIndexList indexes = m_pTreeView->selectionModel()->selectedIndexes();

	QStringList selectedIds;
	for (QModelIndex index : indexes)
	{
		selectedIds << index.data(QObjectTreeView::Id).toString();
	}
	return selectedIds;
}

bool QObjectTreeWidget::PathExists(const char* path)
{
	QStandardItem* pResult = nullptr;
	QString fullPath(path);
	QString itemName = fullPath.mid(fullPath.lastIndexOf(m_regExp) + 1);
	QStringList itemPaths = fullPath.split(m_regExp);

	for (auto i = 0; i < itemPaths.size(); ++i)
	{
		pResult = Find(itemPaths[i], pResult);
		if (!pResult)
			return false;
	}

	return true;
}

void QObjectTreeWidget::AddEntry(const char* path, const char* id, const char* sortStr/* = ""*/)
{
	QStandardItem* pParent = nullptr;
	QString fullPath(path);
	QString itemName = fullPath.mid(fullPath.lastIndexOf(m_regExp) + 1);
	QStringList itemPaths = fullPath.split(m_regExp);

	for (auto i = 0; i < itemPaths.size() - 1; ++i)
		pParent = FindOrCreate(itemPaths[i], pParent);

	QStandardItem* pStandardItem = new QStandardItem(itemName);
	pStandardItem->setEditable(false);
	pStandardItem->setData(QString(id), QObjectTreeView::Id);
	pStandardItem->setData(QVariant::fromValue(Entry), QObjectTreeView::NodeType);
	pStandardItem->setData(strcmp(sortStr, "") != 0 ? sortStr : itemName, QObjectTreeView::Sort);

	if (pParent)
		pParent->appendRow(pStandardItem);
	else
		m_pModel->appendRow(pStandardItem);
}

void QObjectTreeWidget::RemoveEntry(const char* path, const char* id)
{
	if (id)
	{
		TakeItemForId(id);
	}
	else
	{
		QStandardItem* pParent = nullptr;
		QString fullPath(path);
		QString itemName = fullPath.mid(fullPath.lastIndexOf(m_regExp) + 1);
		QStringList itemPaths = fullPath.split(m_regExp);

		for (auto i = 0; i < itemPaths.size() - 1; ++i)
		{
			pParent = Find(itemPaths[i], pParent);
			if (!pParent)
			{
				return;
			}
		}
		auto row = -1;
		const auto parentIdx = m_pModel->indexFromItem(pParent);
		for (auto i = 0; i < m_pModel->rowCount(parentIdx); ++i)
		{
			QModelIndex childIdx = m_pModel->index(i, 0, parentIdx);
			QStandardItem* pItem = m_pModel->itemFromIndex(childIdx);

			if (pItem->text() == itemPaths.last())
			{
				row = i;
				break;
			}
		}
		if (row == -1)
		{
			return;
		}

		const auto parentIndex = m_pModel->indexFromItem(pParent);
		m_pModel->removeRow(row, parentIndex);
	}
}

void QObjectTreeWidget::ChangeEntry(const char* path, const char* id, const char* sortStr/* = ""*/)
{
	RemoveEntry(path, id);
	AddEntry(path, id, sortStr);
}

void QObjectTreeWidget::Clear()
{
	m_pModel->clear();
	m_pModel->setHorizontalHeaderLabels(QStringList("Name"));
}

void QObjectTreeWidget::ExpandAll()
{
	m_pTreeView->expandAll();
}

void QObjectTreeWidget::CollapseAll()
{
	m_pTreeView->collapseAll();
}

void QObjectTreeWidget::dragEnterEvent(QDragEnterEvent* pEvent)
{
	if (pEvent->source() == m_pTreeView)
	{
		QDrag* dragObject = m_pTreeView->findChild<QDrag*>();
		signalOnDragStarted(pEvent, dragObject);

		// We need to connect to the QDrag's destroy to get notified when the drag actually ends
		// this should abort the object create tool
		if (!m_bIsDragTracked)
		{
			m_bIsDragTracked = true;
			connect(dragObject, &QObject::destroyed, [this]()
			{
				singalOnDragEnded();
				m_bIsDragTracked = false;
			});
		}
	}
}

void QObjectTreeWidget::paintEvent(QPaintEvent* pEvent)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

QStandardItem* QObjectTreeWidget::TakeItemForId(const QString& id, const QModelIndex& parentIdx /* = QModelIndex()*/)
{
	for (auto i = 0; i < m_pModel->rowCount(parentIdx); ++i)
	{
		QModelIndex childIdx = m_pModel->index(i, 0, parentIdx);
		QStandardItem* pItem = m_pModel->itemFromIndex(childIdx);

		if (!pItem)
			continue;

		QString name = pItem->data(Qt::DisplayRole).toString();
		QVariant idVar = pItem->data(QObjectTreeView::Id);
		if (idVar.isValid())
		{
			QString itemId = idVar.toString();
			if (itemId == id)
			{
				m_pModel->removeRows(i, 1, parentIdx);
				return pItem;
			}
		}
		if (pItem->hasChildren())
		{
			QStandardItem* pResult = TakeItemForId(id, pItem->index());
			if (pResult)
				return pResult;
		}
	}

	return nullptr;
}

QStandardItem* QObjectTreeWidget::Find(const QString& itemName, QStandardItem* pParent /* = nullptr*/)
{
	// If slowdowns occur we could cache results here
	QList<QStandardItem*> results = m_pModel->findItems(itemName, pParent ? Qt::MatchRecursive : Qt::MatchExactly);

	if (results.size())
	{
		for (QStandardItem* pResult : results)
		{
			if (pResult->parent() == pParent)
				return pResult;
		}
	}

	return nullptr;
}

QStandardItem* QObjectTreeWidget::FindOrCreate(const QString& itemName, QStandardItem* pParent /* = nullptr*/)
{
	auto findResult = Find(itemName, pParent);
	if (findResult)
	{
		return findResult;
	}

	QStandardItem* pResult = new QStandardItem(itemName);
	pResult->setData(QVariant::fromValue(Group), QObjectTreeView::NodeType);
	pResult->setData(itemName, QObjectTreeView::Sort);
	pResult->setEditable(false);

	if (pParent)
		pParent->appendRow(pResult);
	else
		m_pModel->appendRow(pResult);

	return pResult;
}

