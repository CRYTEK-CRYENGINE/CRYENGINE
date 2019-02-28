// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QAdvancedTreeView.h"

#include <QItemSelectionModel>
#include <QAbstractProxyModel>
#include <QHeaderView>
#include <QAction>
#include <QMenu>
#include <QDrag>
#include <QEvent>
#include <QHoverEvent>

#include "QtUtil.h"
#include "QAdvancedItemDelegate.h"
#include "DragDrop.h"
#include "EditorStyleHelper.h"

namespace Private_QAdvancedTreeView
{

// A value returned for Qt::Decoration role can be one of the following types: QColor, QIcon, QPixmap.
QPixmap GetPixmapFromDecoration(QVariant v, int width, int height)
{
	if (v.canConvert<QIcon>())
	{
		return v.value<QIcon>().pixmap(width, height);
	}
	else if (v.canConvert<QPixmap>())
	{
		return v.value<QPixmap>();
	}
	else
	{
		// TODO: return something meaningful for QColor.
		return QPixmap();
	}
}

QPixmap CreateDefaultPixmap(QModelIndexList indices)
{
	const int canvasSize = 2048;
	const int iconSize = 64;
	const int cellSpacing = 2;
	const int spacing = 2;

	QPixmap canvas(canvasSize, canvasSize);
	int yOff = 0;
	int width = 0;

	EditorStyleHelper* pStyleHelper = GetStyleHelper();
	QPainter canvasPainter(&canvas);
	canvasPainter.fillRect(canvas.rect(), pStyleHelper->toolTipBaseColor());
	canvasPainter.setBrush(QBrush(QColor(255, 255, 255)));
	canvasPainter.setPen(QPen(pStyleHelper->toolTipTextColor()));
	auto fm = canvasPainter.fontMetrics();

	for (QModelIndex index : indices)
	{
		QString name = index.data(Qt::DisplayRole).toString();
		QPixmap icon = GetPixmapFromDecoration(index.data(Qt::DecorationRole), fm.height(), fm.height());

		// Text and icon are aligned to bottom of cell.
		const int cellHeight = std::max(icon.height(), fm.height());
		const int textX = pStyleHelper->toolTipMarginLeft() + icon.width() + spacing;
		const int posY = pStyleHelper->toolTipMarginTop() + yOff + cellHeight - icon.height();

		canvasPainter.drawPixmap(pStyleHelper->toolTipMarginLeft(), posY, icon, 0, 0, icon.width(), icon.height());
		canvasPainter.drawText(textX, posY + fm.height(), name);

		const int cellWidth = textX + fm.width(name) + pStyleHelper->toolTipMarginRight();

		width = std::max(width, cellWidth);
		yOff += cellHeight + cellSpacing;
	}

	int height = pStyleHelper->toolTipMarginTop() + pStyleHelper->toolTipMarginBottom() + yOff - cellSpacing;

	QPixmap clipped(std::min(width, canvas.width()), std::min(height, canvas.height()));
	QPainter clippedPainter(&clipped);
	clippedPainter.drawPixmap(0, 0, canvas);
	return clipped;
}

} // namespace Private_QAdvancedTreeView

QAdvancedTreeView::QAdvancedTreeView(BehaviorFlags flags /*= PreserveExpanded | PreserveSelection*/, QWidget* parent /*= nullptr*/)
	: QTreeView(parent)
	, m_behavior(flags)
	, m_attributeRole(Attributes::s_getAttributeRole)
	, m_attributeMenuPathRole(Attributes::s_attributeMenuPathRole)
	, m_layoutVersion(1)
{
	if (m_behavior & PreserveExpandedAfterReset)
	{
		connect(this, &QAdvancedTreeView::expanded, this, &QAdvancedTreeView::OnExpanded);
		connect(this, &QAdvancedTreeView::collapsed, this, &QAdvancedTreeView::OnCollapsed);
	}

	ConnectDefaultHeaderContextMenu();

	// Making QAdvancedItemDelegate default so we can support custom tinting icons on selection
	setItemDelegate(new QAdvancedItemDelegate(this));

	//defaults for advanced tree view differ from base class
	setSortingEnabled(true);

	//Make sure we can capture mouse movement
	setMouseTracking(true);

	header()->setStretchLastSection(true);
}

QAdvancedTreeView::~QAdvancedTreeView()
{

}

void QAdvancedTreeView::setModel(QAbstractItemModel* model)
{
	QAbstractItemModel* old = QTreeView::model();

	if (old != model)
	{
		if (old)
		{
			disconnect(old, &QAbstractItemModel::modelAboutToBeReset, this, &QAdvancedTreeView::OnModelAboutToBeReset);
			disconnect(old, &QAbstractItemModel::modelReset, this, &QAdvancedTreeView::OnModelReset);
			disconnect(old, &QAbstractItemModel::columnsInserted, this, &QAdvancedTreeView::OnColumnsInserted);
			disconnect(old, &QAbstractItemModel::columnsMoved, this, &QAdvancedTreeView::OnColumnsMoved);
			disconnect(old, &QAbstractItemModel::columnsRemoved, this, &QAdvancedTreeView::OnColumnsRemoved);
		}

		reset();
		QTreeView::setModel(model);

		if (model)
		{
			connect(model, &QAbstractItemModel::modelAboutToBeReset, this, &QAdvancedTreeView::OnModelAboutToBeReset);
			connect(model, &QAbstractItemModel::modelReset, this, &QAdvancedTreeView::OnModelReset);
			connect(model, &QAbstractItemModel::columnsInserted, this, &QAdvancedTreeView::OnColumnsInserted);
			connect(model, &QAbstractItemModel::columnsMoved, this, &QAdvancedTreeView::OnColumnsMoved);
			connect(model, &QAbstractItemModel::columnsRemoved, this, &QAdvancedTreeView::OnColumnsRemoved);

			UpdateColumnVisibility(true);
		}
	}
}

void QAdvancedTreeView::reset()
{
	m_expanded.clear();
	QTreeView::reset();
}

void QAdvancedTreeView::ConnectDefaultHeaderContextMenu()
{
	header()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(header(), &QHeaderView::customContextMenuRequested, this, &QAdvancedTreeView::OnContextMenu);
}

void QAdvancedTreeView::DisconnectDefaultHeaderContextMenu()
{
	disconnect(header(), &QHeaderView::customContextMenuRequested, this, &QAdvancedTreeView::OnContextMenu);
}

void QAdvancedTreeView::SetAttributeRole(int role /*= Attributes::s_defaultRole*/, int menuPathRole /*= Attributes::s_attributeMenuPathRole*/)
{
	CRY_ASSERT(m_behavior & UseItemModelAttribute);
	m_attributeRole = role;
	m_attributeMenuPathRole = menuPathRole;
}

void QAdvancedTreeView::OnExpanded(const QModelIndex& index)
{
	if (model()->hasChildren(index))
		m_expanded.insert(QPersistentModelIndex(index));
}

void QAdvancedTreeView::OnCollapsed(const QModelIndex& index)
{
	m_expanded.remove(QPersistentModelIndex(index));
}

void QAdvancedTreeView::OnContextMenu(const QPoint& point)
{
	QAbstractItemModel* pModel = model();
	if (pModel)
	{
		QMenu menu;
		int columnCount = pModel->columnCount();
		QAction* separator = nullptr;

		if (m_behavior & UseItemModelAttribute)
		{
			const bool isDevMode = GetIEditor()->IsDevModeEnabled();

			for (int i = 0; i < columnCount; ++i)
			{
				//Only display sections with attributes
				QVariant value = pModel->headerData(i, Qt::Horizontal, m_attributeRole);
				CItemModelAttribute* pAttribute = value.value<CItemModelAttribute*>();

				//In dev mode, we show all attributes even the always hidden ones
				if (pAttribute && (isDevMode || pAttribute->GetVisibility() != CItemModelAttribute::AlwaysHidden))
				{
					QVariant menuPath = pModel->headerData(i, Qt::Horizontal, m_attributeMenuPathRole);
					QString actionPath;
					if (menuPath.isValid())
					{
						actionPath += menuPath.toString();
						if (!actionPath.endsWith('/'))
							actionPath += '/';
					}

					QAction* pAction = nullptr;

					if (actionPath.length() > 1) //Is in submenu
					{
						if (!separator)
						{
							separator = menu.addSeparator();
						}

						pAction = QtUtil::AddActionFromPath(actionPath + pAttribute->GetName(), &menu);
					}
					else
					{
						pAction = new QAction(pAttribute->GetName(), &menu);
						menu.insertAction(separator, pAction);
					}

					pAction->setCheckable(true);
					pAction->setChecked(IsColumnVisible(i));
					connect(pAction, &QAction::toggled, [=](bool bChecked)
					{
						int column = pAction->data().toInt();
						SetColumnVisible(i, bChecked);
					});

					pAction->setData(QVariant(i));
				}
			}
		}
		else
		{
			for (int i = 0; i < columnCount; ++i)
			{
				QAction* pAction = menu.addAction(pModel->headerData(i, Qt::Horizontal).toString());
				pAction->setCheckable(true);
				pAction->setChecked(IsColumnVisible(i));

				connect(pAction, &QAction::toggled, [=](bool bChecked)
				{
					int column = pAction->data().toInt();
					SetColumnVisible(i, bChecked);
				});

				pAction->setData(QVariant(i));
			}
		}
		menu.exec(QCursor::pos());
	}
}

void QAdvancedTreeView::OnModelAboutToBeReset()
{
	BackupExpanded();
	BackupSelection();
}

void QAdvancedTreeView::OnModelReset()
{
	RestoreExpanded();
	RestoreSelection();
	UpdateColumnVisibility();
}

void QAdvancedTreeView::OnColumnsInserted(const QModelIndex& parent, int first, int last)
{
	m_columnVisible.insert(m_columnVisible.begin() + first, last - first + 1, std::pair<QVariant, bool>(QVariant(), true));
	UpdateColumnVisibility();
}

void QAdvancedTreeView::OnColumnsRemoved(const QModelIndex& parent, int first, int last)
{
	m_columnVisible.erase(m_columnVisible.begin() + first, m_columnVisible.begin() + last + 1);
	UpdateColumnVisibility();
}

void QAdvancedTreeView::OnColumnsMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination, int column)
{
	const int count = end - start + 1;
	std::vector<std::pair<QVariant, bool>> moving;
	moving.reserve(count);
	for (int i = start; i <= end; i++)
	{
		moving[i - start] = m_columnVisible[i];
	}

	m_columnVisible.erase(m_columnVisible.begin() + start, m_columnVisible.begin() + end + 1);
	m_columnVisible.insert(m_columnVisible.begin() + column, moving.begin(), moving.end());

	UpdateColumnVisibility();
}

void QAdvancedTreeView::OnHeaderSectionCountChanged(int oldCount, int newCount)
{
	UpdateColumnVisibility();
}

void QAdvancedTreeView::BackupExpanded()
{
	if (!(m_behavior & PreserveExpandedAfterReset))
		return;

	m_expandedBackup.clear();

	for (QPersistentModelIndex index : m_expanded)
	{
		if (isExpanded(index))
		{
			m_expandedBackup.push_back(CAdvancedPersistentModelIndex(ToOriginal(index, model())));
		}
	}
}

void QAdvancedTreeView::RestoreExpanded()
{
	if (!(m_behavior & PreserveExpandedAfterReset))
		return;

	m_expanded.clear();

	for (CAdvancedPersistentModelIndex persistentIndex : m_expandedBackup)
	{
		QModelIndex index = persistentIndex.toIndex();
		if (index.isValid())
		{
			setExpanded(FromOriginal(index, model()), true);
		}
	}

	m_expandedBackup.clear();
}

void QAdvancedTreeView::BackupSelection()
{
	if (!(m_behavior & PreserveSelectionAfterReset))
		return;

	BackupSelection(m_selectedBackup, model(), selectionModel());
}

void QAdvancedTreeView::BackupSelection(std::vector<CAdvancedPersistentModelIndex>& selectedBackupStorage, QAbstractItemModel* viewModel, QItemSelectionModel* viewSelectionModel)
{
	selectedBackupStorage.clear();

	QModelIndexList list = viewSelectionModel->selectedRows();
	for (QModelIndex index : list)
	{
		selectedBackupStorage.push_back(CAdvancedPersistentModelIndex(ToOriginal(index, viewModel)));
	}

	//push the current index last
	selectedBackupStorage.push_back(CAdvancedPersistentModelIndex(ToOriginal(viewSelectionModel->currentIndex(), viewModel)));
}

void QAdvancedTreeView::RestoreSelection()
{
	if (!(m_behavior & PreserveSelectionAfterReset))
		return;

	RestoreSelection(m_selectedBackup, model(), selectionModel());
}

void QAdvancedTreeView::RestoreSelection(std::vector<CAdvancedPersistentModelIndex>& selectedBackupStorage, QAbstractItemModel* viewModel, QItemSelectionModel* viewSelectionModel)
{
	if (selectedBackupStorage.size() == 0)
		return;

	//current index is last
	QModelIndex current = FromOriginal(selectedBackupStorage[selectedBackupStorage.size() - 1].toIndex(), viewModel);
	selectedBackupStorage.pop_back();

	for (CAdvancedPersistentModelIndex persistentIndex : selectedBackupStorage)
	{
		QModelIndex index = persistentIndex.toIndex();
		if (index.isValid())
		{
			//Right now only supporting rows.
			//Passing all the indexes and merging selection didn't work:
			//only one cell ended up selected even though the selection contained the indices.

			QModelIndex localIndex = FromOriginal(index, viewModel);
			viewSelectionModel->select(localIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
		}
	}

	if (current.isValid())
		viewSelectionModel->setCurrentIndex(current, QItemSelectionModel::Select | QItemSelectionModel::Rows);

	selectedBackupStorage.clear();
}

void QAdvancedTreeView::UpdateColumnVisibility(bool resetVisibility /*= false*/)
{
	auto pModel = model();
	const int columnCount = pModel->columnCount();

	m_columnVisible.resize(columnCount, std::pair<QVariant, bool>(QVariant(), true));
	if (m_behavior & UseItemModelAttribute)
	{
		for (int i = 0; i < columnCount; ++i)
		{
			//Only display sections with attributes
			QVariant value = pModel->headerData(i, Qt::Horizontal, m_attributeRole);
			CItemModelAttribute* pAttribute = value.value<CItemModelAttribute*>();
			if (pAttribute)
			{
				bool wasVisible = m_columnVisible[i].first == value ? m_columnVisible[i].second : true;
				m_columnVisible[i].first = value;
				bool isVisibleNow = resetVisibility ? pAttribute->GetVisibility() == CItemModelAttribute::Visible : wasVisible;
				SetColumnVisible(i, isVisibleNow);
				auto pDelegate = GetAdvancedDelegate();
				if (isVisibleNow && pDelegate && pDelegate->TestColumnBehavior(i, QAdvancedItemDelegate::Behavior::OverrideCheckIcon))
				{
					header()->setSectionResizeMode(i, QHeaderView::Fixed);
				}
			}
			else
			{
				SetColumnVisible(i, false);
			}
		}
	}
	else
	{
		for (int i = 0; i < columnCount; ++i)
		{
			QVariant value = pModel->headerData(i, Qt::Horizontal, Qt::DisplayRole);
			bool wasVisible = m_columnVisible[i].first == value ? m_columnVisible[i].second : true;
			m_columnVisible[i].first = value;
			SetColumnVisible(i, wasVisible);
		}
	}
}

int QAdvancedTreeView::GetColumnCount() const
{
	return header()->count();
}

bool QAdvancedTreeView::IsColumnVisible(int i) const
{
	return m_columnVisible[i].second; // !header()->isSectionHidden(i);
}

void QAdvancedTreeView::SetColumnVisible(int i, bool visible)
{
	header()->setSectionHidden(i, !visible);
	m_columnVisible[i].second = visible;
	signalVisibleSectionCountChanged();
}

void QAdvancedTreeView::TriggerRefreshHeaderColumns()
{
	OnHeaderSectionCountChanged(0, header()->count());
}

QVariantMap QAdvancedTreeView::GetState() const
{
	QVariantMap state;

	QVariantList columnOrder;

	QVariantMap columnStateMapCopy(m_columnStateMap);

	QHeaderView *pHeader = header();
	auto itemModel = pHeader->model();
	
	const int count = GetColumnCount();
	for (int i = 0; i < count; i++)
	{
		QString name;
		if (m_behavior & UseItemModelAttribute)
		{
			QVariant attributeVariant = itemModel->headerData(i, Qt::Horizontal, m_attributeRole);
			CItemModelAttribute* pAttribute = attributeVariant.value<CItemModelAttribute*>();
			name = pAttribute->GetName();
		}

		if (~m_behavior & UseItemModelAttribute || name.isEmpty())
			name = itemModel->headerData(i, Qt::Horizontal).toString();

		QVariantMap currentColumnData;
		currentColumnData["visible"] = IsColumnVisible(i);
		currentColumnData["width"] = pHeader->sectionSize(i);
		currentColumnData["data"] = name;
		currentColumnData["visualIndex"] = pHeader->visualIndex(i);
		columnStateMapCopy[name] = currentColumnData;
		columnOrder.push_back(name);
	}

	QVariantMap headerSortingOrder;

	int sortedSection = header()->sortIndicatorSection();
	int sortOrder = header()->sortIndicatorOrder();
	headerSortingOrder["sortedSection"] = sortedSection;
	headerSortingOrder["sortOrder"] = sortOrder;

	state.insert("version", m_layoutVersion);
	state.insert("columnOrder", columnOrder);
	state.insert("columnData", columnStateMapCopy);
	state.insert("headerSortingOrder", headerSortingOrder);

	return state;
}

void QAdvancedTreeView::SetState(const QVariantMap& state)
{
	QHeaderView *pHeader = header();
	auto itemModel = header()->model();

	QVariant columnDataVar = state.value("columnData");
	if (columnDataVar.isValid() && columnDataVar.type() == QVariant::Map)
	{
		const int version = state["version"].toInt();
		m_columnStateMap = columnDataVar.value<QVariantMap>();
		const int columnCount = GetColumnCount();
		for (int i = 0; i < columnCount; i++)
		{
			QVariant attributeVariant;
			if (m_behavior & UseItemModelAttribute)
			{
				attributeVariant = model()->headerData(i, Qt::Horizontal, m_attributeRole);
			}
			CItemModelAttribute* pAttribute = attributeVariant.value<CItemModelAttribute*>();
			QString columnName;
			if (pAttribute)
			{
				columnName = pAttribute->GetName();
			}

			if (version < 1 || columnName.isEmpty())
			{
				columnName = itemModel->headerData(i, Qt::Horizontal).toString();
			}
			bool columnVisibilityWasSet = false;
			for (auto columnVar : m_columnStateMap.values())
			{
				if (columnVar.isValid() && columnVar.type() == QVariant::Map)
				{
					QVariantMap columnMap = columnVar.value<QVariantMap>();
					if (columnMap.contains("data"))
					{
						if (columnMap["data"] == columnName)
						{
							if (columnMap.contains("visible"))
							{
								if (pAttribute)
								{
									SetColumnVisible(i, columnMap["visible"].toBool() && pAttribute->GetVisibility() != CItemModelAttribute::AlwaysHidden);
								}
								else
								{
									SetColumnVisible(i, columnMap["visible"].toBool());
								}
								columnVisibilityWasSet = true;
							}

							if (columnMap.contains("width"))
							{
								if (columnMap["width"].toInt() > 0)
								{
									header()->resizeSection(i, columnMap["width"].toInt());
								}
							}

							if (columnMap.contains("visualIndex"))
							{
								if (columnMap["visualIndex"].toInt() > 0)
								{
									header()->moveSection(header()->visualIndex(i), columnMap["visualIndex"].toInt());
								}
							}
							break; // break out of the for loop as the column was already found
						}
					}
				}
			}
			if (!columnVisibilityWasSet &&
				pAttribute &&
				pAttribute->GetVisibility() == CItemModelAttribute::AlwaysHidden)
			{
				SetColumnVisible(i, false);
			}
		}
	}

	// TODO: Sorting columns is actually going to need a different system
	// both for keeping columns between models and for merging order changes

	QVariant sortingVar = state.value("headerSortingOrder");
	if (sortingVar.isValid() && sortingVar.type() == QVariant::Map)
	{	
		QVariantMap sortingVarMap = sortingVar.toMap();
		QVariant sortedSectionVar = sortingVarMap["sortedSection"];
		QVariant sortOrderVar = sortingVarMap["sortOrder"];

		if (sortedSectionVar.isValid() && sortOrderVar.isValid())
		{
			auto h = header();
			header()->setSortIndicator(sortedSectionVar.toInt(), static_cast<Qt::SortOrder>(sortOrderVar.toInt()));
		}
	}
}

QAdvancedItemDelegate* QAdvancedTreeView::GetAdvancedDelegate()
{
	return qobject_cast<QAdvancedItemDelegate*>(itemDelegate());
}

void QAdvancedTreeView::startDrag(Qt::DropActions supportedActions)
{
	using namespace Private_QAdvancedTreeView;

	//TODO : Figure out and document why this code is needed at all, this is not obvious.
	auto delegate = GetAdvancedDelegate();
	if (!delegate || !delegate->IsDragChecking())
	{
		if (model() && selectionModel())
		{
			const QModelIndexList selection = selectionModel()->selectedIndexes();
			QMimeData* const pMimeData = model()->mimeData(selection);

			// If the selection spans multiple columns of the same row, we only show the name and
			// icon of the column that stores the name.
			// We find this column by looking for the item attribute s_nameAttribute.
			// If the model does not provide this attribute, we assume that the name is stored in
			// the first column.
			int nameCol = 0;
			for (int i = 0; i < model()->columnCount(); ++i)
			{
				QVariant value = model()->headerData(i, Qt::Horizontal, Attributes::s_getAttributeRole);
				const CItemModelAttribute* const pAttribute = value.value<CItemModelAttribute*>();
				if (pAttribute == &Attributes::s_nameAttribute)
				{
					nameCol = i;
				}
			}
			QModelIndexList names;
			for (QModelIndex index : selection)
			{
				names.push_back(index.sibling(index.row(), nameCol));
			}
			names.erase(std::unique(names.begin(), names.end()), names.end());

			const QPixmap pixmap = CreateDefaultPixmap(names);
			CDragDropData::StartDrag(this, supportedActions, pMimeData, &pixmap);
		}
	}
}

void QAdvancedTreeView::drawBranches(QPainter *painter, const QRect &rect,
	const QModelIndex &index) const

{
	QTreeView::drawBranches(painter, rect, index);
	const bool reverse = isRightToLeft();
	const int indent = indentation();
	const int outer = rootIsDecorated() ? 0 : 1;
	const int item = index.row();
	int level = index.column();

	QRect primitive(reverse ? rect.left() : rect.right() + 1, rect.top(), indent, rect.height());
	int branchAreaLeft = rect.left();

	QModelIndex parent = index.parent();
	QModelIndex current = parent;
	QModelIndex ancestor = current.parent();

	QStyleOptionViewItem opt = viewOptions();
	QStyle::State extraFlags = QStyle::State_None;
	if (isEnabled())
		extraFlags |= QStyle::State_Enabled;
	if (window()->isActiveWindow())
		extraFlags |= QStyle::State_Active;
	QPoint oldBO = painter->brushOrigin();
	if (verticalScrollMode() == QAbstractItemView::ScrollPerPixel)
		painter->setBrushOrigin(QPoint(0, verticalOffset()));

	if (alternatingRowColors()) 
	{
		if (currentIndex().row() & 1) 
		{
			opt.features |= QStyleOptionViewItem::Alternate;
		}
		else {
			opt.features &= ~QStyleOptionViewItem::Alternate;
		}
	}

	if (selectionModel()->isSelected(index))
		extraFlags |= QStyle::State_Selected;


	if (level >= outer) 
	{
		// start with the innermost branch
		primitive.moveLeft(reverse ? primitive.left() : primitive.left() - indent);
		opt.rect = primitive;

		bool expanded = false;
		if (m_expanded.contains(QPersistentModelIndex(index)))
			expanded = true;
		expanded = QTreeView::isExpanded(index);
		const bool children = model()->hasChildren(index);
		bool moreSiblings = current.sibling(current.row(), current.column()).isValid(); //Makes sense with own row/coll?

		opt.state = QStyle::State_Item | extraFlags
			| (moreSiblings ? QStyle::State_Sibling : QStyle::State_None)
			| (children ? QStyle::State_Children : QStyle::State_None)
			| (expanded ? QStyle::State_Open : QStyle::State_None);
		if (m_hoveredOver.isValid() && index.row() == m_hoveredOver.row() && index.parent() == m_hoveredOver.parent())
		{
			opt.state |= QStyle::State_MouseOver;
		}
		else
			opt.state &= ~QStyle::State_MouseOver;
		drawBranchIndicator(painter, &opt);
	}
	painter->setBrushOrigin(oldBO);
}


void QAdvancedTreeView::drawBranchIndicator(QPainter *painter, const QStyleOptionViewItem *opt) const
{
	//Make sure the icon is placed in the middle
	QRect rect = opt->rect;
	int heightDiff = (rect.height()) /2;

	rect.moveCenter(QPoint(rect.center().x(), rect.center().y() + heightDiff / 2));
	rect.setHeight(m_BranchIconSize);
	rect.setWidth(m_BranchIconSize);
	
	CryIcon icon;
	if (opt->state & QStyle::State_Children)
	{
		icon = CryIcon("icons:General/Pointer_Right.ico");
		if (opt->state & QStyle::State_Open)
		{
			icon = CryIcon("icons:General/Pointer_Down_Expanded.ico");
		}
	}

	else
	{
		//Don't draw an Icon
		return;
	}
	if (opt->state & QStyle::State_MouseOver || opt->state & QStyle::State_Selected)
	{
		QPixmap pixmap = icon.pixmap(rect.size());
		QtUtil::DrawStatePixmap(painter, rect, pixmap, opt->state);
	}
	else
	{
		icon.paint(painter, rect);
	}
}

void QAdvancedTreeView::mousePressEvent(QMouseEvent* pEvent)
{
	if (!hasAutoScroll())
	{
		QTreeView::mousePressEvent(pEvent);
		return;
	}

	// disable autoScroll for mouse press because otherwise
	// the treeview content slides away if a scrollbar
	// is used to handle the treeview content
	setAutoScroll(false);
	QTreeView::mousePressEvent(pEvent);
	setAutoScroll(true);
}

QModelIndex QAdvancedTreeView::ToOriginal(const QModelIndex& index, QAbstractItemModel* viewModel)
{
	QAbstractItemModel* model = viewModel;
	QModelIndex original = index;

	while (QAbstractProxyModel* proxy = qobject_cast<QAbstractProxyModel*>(model))
	{
		original = proxy->mapToSource(original);
		model = proxy->sourceModel();
	}

	return original;
}

QModelIndex QAdvancedTreeView::FromOriginal(const QModelIndex& index, QAbstractItemModel* viewModel)
{
	std::vector<QAbstractProxyModel*> proxys;

	QAbstractItemModel* model = viewModel;
	while (QAbstractProxyModel* proxy = qobject_cast<QAbstractProxyModel*>(model))
	{
		proxys.push_back(proxy);
		model = proxy->sourceModel();
	}

	QModelIndex localIndex = index;
	for (auto it = proxys.rbegin(); it != proxys.rend(); ++it)
	{
		localIndex = (*it)->mapFromSource(localIndex);
	}

	return localIndex;
}

bool QAdvancedTreeView::viewportEvent(QEvent *event)
{
	switch (event->type()) {
	case QEvent::HoverEnter:
	case QEvent::HoverLeave:
	case QEvent::HoverMove: {
		QHoverEvent *he = static_cast<QHoverEvent*>(event);
		QPersistentModelIndex oldHoveredOver = m_hoveredOver;
		m_hoveredOver = indexAt(he->pos());
		if (m_hoveredOver != oldHoveredOver || m_hoveredOver.row() != oldHoveredOver.row()) 
		{
			QRect rect = visualRect(oldHoveredOver);
			rect.setX(0);
			rect.setWidth(viewport()->width());
			viewport()->update(rect);
		}
		break; }
	default:
		break;
	}
	return QAbstractItemView::viewportEvent(event);
}
//////////////////////////////////////////////////////////////////////////

CAdvancedPersistentModelIndex::CAdvancedPersistentModelIndex(const QModelIndex& index)
	: m_column(index.column())
	, m_model(index.model())
	, m_persistentIndex(index)
{
	QModelIndex localIndex = index;
	while (localIndex.isValid())
	{
		m_identifiers.push_front(localIndex.internalId());
		localIndex = localIndex.parent();
	}
}

QModelIndex CAdvancedPersistentModelIndex::toIndex() const
{
	if (m_persistentIndex.isValid())
	{
		return m_persistentIndex;
	}

	QModelIndex index;

	std::deque<quintptr> identifiers = m_identifiers;

	while (!identifiers.empty())
	{
		const int rowCount = m_model->rowCount(index);
		bool bFound = false;

		for (int i = 0; i < rowCount; i++)
		{
			QModelIndex rowIndex = m_model->index(i, m_column, index);
			if (rowIndex.internalId() == identifiers.front())
			{
				index = rowIndex;
				identifiers.pop_front();
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			return QModelIndex();
		}

	}

	return index;
}
