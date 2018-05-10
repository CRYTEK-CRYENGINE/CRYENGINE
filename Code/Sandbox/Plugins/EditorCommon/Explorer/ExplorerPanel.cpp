// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ExplorerPanel.h"

#include <QApplication>
#include <QAdvancedTreeView.h>
#include <QBoxLayout>
#include <QClipboard>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QDockWidget>
#include <QHeaderView>
#include <QEvent>
#include <QFocusEvent>
#include "QSearchBox.h"
#include "Explorer.h"
#include "ExplorerModel.h"
#include "ExplorerFileList.h"

#include <CryCore/Platform/CryWindows.h>
#include <shellapi.h>

#include "QAdvancedItemDelegate.h"
#include "CryIcon.h"

//////////////////////////////////////////////////////////////////////////
// Serialization method for a tree view
//////////////////////////////////////////////////////////////////////////


QString GetIndexPath(QAbstractItemModel* model, const QModelIndex& index)
{
	QString path;

	QModelIndex cur = index;
	while (cur.isValid() && cur != QModelIndex())
	{
		if (!path.isEmpty())
			path = QString("|") + path;
		path = model->data(cur).toString() + path;
		cur = model->parent(cur);
	}
	return path;
}

QModelIndex FindIndexChildByText(QAbstractItemModel* model, const QModelIndex& parent, const QString& text)
{
	int rowCount = model->rowCount(parent);
	for (int i = 0; i < rowCount; ++i)
	{
		QModelIndex child = model->index(i, 0, parent);
		QString childText = model->data(child).toString();
		if (childText == text)
			return child;
	}
	return QModelIndex();
}

QModelIndex GetIndexByPath(QAbstractItemModel* model, const QString& path)
{
	QStringList items = path.split('|');
	QModelIndex cur = QModelIndex();
	for (int i = 0; i < items.size(); ++i)
	{
		cur = FindIndexChildByText(model, cur, items[i]);
		if (!cur.isValid())
			return QModelIndex();
	}
	return cur;
}

std::vector<QString> GetIndexPaths(QAbstractItemModel* model, const QModelIndexList& indices)
{
	std::vector<QString> result;
	for (int i = 0; i < indices.size(); ++i)
	{
		QString path = GetIndexPath(model, indices[i]);
		if (!path.isEmpty())
			result.push_back(path);
	}
	return result;
}

QModelIndexList GetIndicesByPath(QAbstractItemModel* model, const std::vector<QString>& paths)
{
	QModelIndexList result;
	for (int i = 0; i < paths.size(); ++i)
	{
		QModelIndex index = GetIndexByPath(model, paths[i]);
		if (index.isValid())
			result.push_back(index);
	}
	return result;
}

struct QTreeViewStateSerializer
{
	QTreeView* treeView;
	QTreeViewStateSerializer(QTreeView* treeView) : treeView(treeView) {}

	void Serialize(Serialization::IArchive& ar)
	{
		QAbstractItemModel* model = treeView->model();
		if (!model)
			return;

		std::vector<QString> expandedItems;
		if (ar.isOutput())
		{

			std::vector<QModelIndex> stack;
			stack.push_back(QModelIndex());
			while (!stack.empty())
			{
				QModelIndex index = stack.back();
				stack.pop_back();

				int rowCount = model->rowCount(index);
				for (int i = 0; i < rowCount; ++i)
				{
					QModelIndex child = model->index(i, 0, index);
					if (treeView->isExpanded(child))
					{
						stack.push_back(child);
						expandedItems.push_back(GetIndexPath(model, child));
					}
				}
			}
		}
		ar(expandedItems, "expandedItems");
		if (ar.isInput())
		{
			treeView->collapseAll();
			for (size_t i = 0; i < expandedItems.size(); ++i)
			{
				QModelIndex index = GetIndexByPath(model, expandedItems[i]);
				if (index.isValid())
					treeView->expand(index);
			}
		}

		std::vector<QString> selectedItems;
		if (ar.isOutput())
			selectedItems = GetIndexPaths(model, treeView->selectionModel()->selectedIndexes());
		ar(selectedItems, "selectedItems");
		if (ar.isInput())
		{
			QModelIndexList indices = GetIndicesByPath(model, selectedItems);
			if (!indices.empty())
			{
				treeView->selectionModel()->select(QModelIndex(), QItemSelectionModel::ClearAndSelect);
				for (int i = 0; i < indices.size(); ++i)
					treeView->selectionModel()->select(indices[i], QItemSelectionModel::Select);
			}
		}

		QString currentItem;
		if (ar.isOutput())
			currentItem = GetIndexPath(model, treeView->selectionModel()->currentIndex());
		ar(currentItem, "currentItem");
		if (ar.isInput())
		{
			QModelIndex currentIndex = GetIndexByPath(model, currentItem);
			treeView->scrollTo(currentIndex, QAbstractItemView::PositionAtCenter);
			treeView->selectionModel()->setCurrentIndex(currentIndex, QItemSelectionModel::Current);
		}

		std::vector<int> sectionsHidden;
		std::vector<int> sectionsVisible;
		if (ar.isOutput())
		{
			for (int i = 0; i < treeView->model()->columnCount(); ++i)
				if (treeView->header()->isSectionHidden(i))
					sectionsHidden.push_back(i);
				else
					sectionsVisible.push_back(i);
		}
		ar(sectionsHidden, "sectionsHidden");
		ar(sectionsVisible, "sectionsVisible");
		if (ar.isInput())
		{
			int columnCount = treeView->model()->columnCount();
			for (int i = 0; i < sectionsHidden.size(); ++i)
			{
				int section = sectionsHidden[i];
				if (section >= 0 && section < columnCount)
					treeView->header()->hideSection(section);
			}
			for (int i = 0; i < sectionsVisible.size(); ++i)
			{
				int section = sectionsVisible[i];
				if (section >= 0 && section < columnCount)
					treeView->header()->showSection(section);
			}
		}
	}
};

bool Serialize(Serialization::IArchive& ar, QTreeView* treeView, const char* name, const char* label)
{
	return ar(QTreeViewStateSerializer(treeView), name, label);
}



//////////////////////////////////////////////////////////////////////////

namespace Explorer
{

ExplorerTreeView::ExplorerTreeView()
	: m_treeSelectionChanged(false)
	, m_dragging(false)
{
	// Making QAdvancedItemDelegate default so we can support custom tinting icons on selection
	setItemDelegate(new QAdvancedItemDelegate(this));
}

void ExplorerTreeView::setModel(QAbstractItemModel* model)
{
	if (m_selectionHandler)
	{
		QObject::disconnect(m_selectionHandler);
		m_selectionHandler = QMetaObject::Connection();
	}

	QAdvancedTreeView::setModel(model);

	m_selectionHandler = connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
	                             [this](const QItemSelection&, const QItemSelection&)
		{
			m_treeSelectionChanged = true;
	  }
	                             );
}

void ExplorerTreeView::mousePressEvent(QMouseEvent* ev)
{
	if (ev->button() == Qt::LeftButton)
	{
		m_dragging = false;
		m_treeSelectionChanged = false;
		QAdvancedTreeView::mousePressEvent(ev);
		if (!m_treeSelectionChanged && selectionModel())
		{
			QModelIndex currentIndex = selectionModel()->currentIndex();
			if (currentIndex.isValid())
				SignalClickOnSelectedItem(currentIndex);
		}
	}
	else
		QAdvancedTreeView::mousePressEvent(ev);
}

void ExplorerTreeView::mouseMoveEvent(QMouseEvent* event)
{
	if (!m_dragging)
	{
		setState(QAbstractItemView::DraggingState);
		m_dragging = true;
	}
	QAdvancedTreeView::mouseMoveEvent(event);
}

void ExplorerTreeView::startDrag(Qt::DropActions supportedActions)
{
	QModelIndex currentIndex = selectionModel()->currentIndex();
	if (currentIndex.isValid())
		SignalStartDrag(currentIndex);
}

// ---------------------------------------------------------------------------

static QModelIndex FindIndexByEntry(QAdvancedTreeView* treeView, QSortFilterProxyModel* filterModel, ExplorerModel* model, ExplorerEntry* entry)
{
	if (model->GetRootIndex() > 0 && model->GetActiveRoot()->subtree != entry->subtree)
		return QModelIndex();
	QModelIndex sourceIndex = model->ModelIndexFromEntry(entry, 0);
	QModelIndex index;
	if (treeView->model() == filterModel)
		index = filterModel->mapFromSource(sourceIndex);
	else
		index = sourceIndex;
	return index;
}

// ---------------------------------------------------------------------------

struct ColumnFilterOption
{
	cstr   name;
	string label;
	int    column;

	struct Option
	{
		string name, label;
		bool   allow;

		Option(string name, string label)
			: name(name), label(label), allow(true) {}
		void Serialize(IArchive& ar)
		{
			ar(allow, name, label);
		}
	};
	vector<Option> options;

	ColumnFilterOption(int column, const ExplorerColumn* columnData)
		: column(column)
		, name(columnData->label)
		, label(string(name) + ": ")
	{
		if (columnData->values.size())
		{
			for (auto const& column : columnData->values)
			{
				cstr label = column.tooltip;
				options.push_back(Option(Serialization::LabelToName(label), string("^") + label));
			}
		}
		else
		{
			options.push_back(Option("no", "^No"));
			options.push_back(Option("yes", "^Yes"));
		}
	}

	bool Allow(const ExplorerEntry* entry) const
	{
		if (column < 0)
			return true;
		int val = entry->GetColumnValue(column);
		return val < 0 || val >= options.size() || options[val].allow;
	}

	void Serialize(Serialization::IArchive& ar)
	{
		if (options.size() && ar.openBlock(name, label))
		{
			for (int i = 0; i < options.size(); ++i)
				options[i].Serialize(ar);
			ar.closeBlock();
		}
	}
};

struct FilterOptions
{
	vector<ColumnFilterOption> options;

	FilterOptions()
	{
	}

	FilterOptions(const ExplorerData& explorerData)
	{
		for (int col = 0, count = explorerData.GetColumnCount(); col < count; col++)
		{
			const ExplorerColumn* columnData = explorerData.GetColumn(col);
			if (columnData->format == ExplorerColumn::ICON)
			{
				options.push_back(ColumnFilterOption(col, columnData));
			}
		}
	}

	bool Allow(const ExplorerEntry* entry) const
	{
		if (entry->type == ENTRY_ASSET)
		{
			for (auto const& filter : options)
			{
				if (!filter.Allow(entry))
					return false;
			}
		}
		else if (entry->type == ENTRY_GROUP)
			return false;
		return true;
	}

	void Serialize(Serialization::IArchive& ar)
	{
		for (auto& filter : options)
			filter.Serialize(ar);
	}
};

class ExplorerFilterProxyModel : public QSortFilterProxyModel
{
public:
	ExplorerFilterProxyModel(QObject* parent)
		: QSortFilterProxyModel(parent)
	{
	}

	void setFilterOptions(const FilterOptions& options)
	{
		m_filterOptions = options;
		m_acceptCache.clear();
		invalidate();
	}

	void invalidate()
	{
		QSortFilterProxyModel::invalidate();
		m_acceptCache.clear();
	}

	void setFilterTokenizedString(const QString& pattern)
	{
		m_filterTokens = pattern.split(' ', QString::SkipEmptyParts);
		invalidateFilter();
	}

	bool matchFilter(int source_row, const QModelIndex& source_parent) const
	{
		const QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
		const ExplorerEntry* const entry = ExplorerModel::GetEntry(index);
		if (entry && entry->type == ENTRY_LOADING)
		{
			return true;
		}

		const QVariant data = sourceModel()->data(index, Qt::DisplayRole);
		const QString label(data.toString());
		const QString path(QString::fromLocal8Bit(entry->path.c_str()));

		for (const auto& token : m_filterTokens)
		{
			if (label.indexOf(token, 0, Qt::CaseInsensitive) == -1 && path.indexOf(token, 0, Qt::CaseInsensitive) == -1)
			{
				return false;
			}
		}

		return m_filterOptions.Allow(entry);
	}

	virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
	{
		if (left.column() == right.column())
		{
			QVariant valueLeft = sourceModel()->data(left, Qt::UserRole);
			QVariant valueRight = sourceModel()->data(right, Qt::UserRole);
			if (valueLeft.type() == valueRight.type())
			{
				switch (valueLeft.type())
				{
				case QVariant::Int:
					return valueLeft.toInt() < valueRight.toInt();
				case QVariant::String:
					return valueLeft.toString() < valueRight.toString();
				case QVariant::Double:
					return valueLeft.toDouble() < valueRight.toDouble();
				default:
					break; // fall back to default comparison
				}
			}
		}

		return QSortFilterProxyModel::lessThan(left, right);
	}

	virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override
	{
		if (matchFilter(source_row, source_parent))
			return true;

		if (hasAcceptedChildrenCached(source_row, source_parent))
			return true;

		return false;
	}

private:

	bool hasAcceptedChildrenCached(int source_row, const QModelIndex& source_parent) const
	{
		std::pair<QModelIndex, int> indexId = std::make_pair(source_parent, source_row);
		TAcceptCache::iterator it = m_acceptCache.find(indexId);
		if (it == m_acceptCache.end())
		{
			bool result = hasAcceptedChildren(source_row, source_parent);
			m_acceptCache[indexId] = result;
			return result;
		}
		else
			return it->second;
	}

	bool hasAcceptedChildren(int source_row, const QModelIndex& source_parent) const
	{
		QModelIndex item = sourceModel()->index(source_row, 0, source_parent);
		if (!item.isValid())
			return false;

		int childCount = sourceModel()->rowCount(item);
		for (int i = 0; i < childCount; ++i)
		{
			if (filterAcceptsRow(i, item))
				return true;
		}

		return false;
	}

	FilterOptions        m_filterOptions;
	QStringList          m_filterTokens;

	typedef map<std::pair<QModelIndex, int>, bool> TAcceptCache;
	mutable TAcceptCache m_acceptCache;
};

// ---------------------------------------------------------------------------
ExplorerPanel::ExplorerPanel(QWidget* parent, ExplorerData* explorerData)
	: QWidget(parent)
	, m_explorerData(explorerData)
	, m_filterModel(0)
	, m_explorerRootIndex(0)
	, m_dockWidget(0)
	, m_model(0)
	, m_searchBox(0)
	, m_filterMode(false)
	, m_batchChangesRunning(0)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	QBoxLayout* layout = new QBoxLayout(QBoxLayout::TopToBottom, this);
	layout->setMargin(0);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(2);
	setLayout(layout);
	{
		QBoxLayout* hlayout = new QBoxLayout(QBoxLayout::LeftToRight);
		hlayout->setSpacing(4);

		m_searchBox = new QSearchBox();
		EXPECTED(connect(m_searchBox, SIGNAL(textChanged(const QString &)), this, SLOT(OnFilterTextChanged(const QString &))));
		hlayout->addWidget(m_searchBox, 1);

		if (m_explorerData->GetSubtreeCount() == 1)
		{
			// set single subtree as root
			m_explorerRootIndex = 1;
		}
		else
		{
			// create root menu
			m_rootButton = new QPushButton("");
			EXPECTED(connect(m_rootButton, SIGNAL(pressed()), this, SLOT(OnRootButtonPressed())));
			m_rootMenu = new QMenu(this);

			m_rootButton->setMenu(m_rootMenu);
			UpdateRootMenu();
			hlayout->addWidget(m_rootButton, 0);
		}

		m_filterButton = new QToolButton();
		m_filterButton->setIcon(CryIcon("icons:General/Options.ico"));
		m_filterButton->setCheckable(true);
		m_filterButton->setChecked(m_filterMode);
		m_filterButton->setAutoRaise(true);
		m_filterButton->setToolTip("Enable Filter Options");
		EXPECTED(connect(m_filterButton, SIGNAL(toggled(bool)), this, SLOT(OnFilterButtonToggled(bool))));
		hlayout->addWidget(m_filterButton, 0);

		layout->addLayout(hlayout);
	}

	m_filterOptions.reset(new FilterOptions(*explorerData));

	m_filterOptionsTree = new QPropertyTree(this);
	PropertyTreeStyle treeStyle(QPropertyTree::defaultTreeStyle());
	treeStyle.propertySplitter = false;
	treeStyle.groupRectangle = false;
	m_filterOptionsTree->setTreeStyle(treeStyle);
	m_filterOptionsTree->setCompact(true);
	m_filterOptionsTree->setSizeToContent(true);
	m_filterOptionsTree->setHideSelection(true);
	m_filterOptionsTree->attach(Serialization::SStruct(*m_filterOptions));
	EXPECTED(connect(m_filterOptionsTree, SIGNAL(signalChanged()), this, SLOT(OnFilterOptionsChanged())));
	layout->addWidget(m_filterOptionsTree);
	m_filterOptionsTree->hide();

	m_treeView = new ExplorerTreeView();

	{
		m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
		m_treeView->setIndentation(12);
		m_treeView->setSortingEnabled(true);
		m_treeView->header()->setContextMenuPolicy(Qt::CustomContextMenu);
		m_treeView->installEventFilter(this);

		m_treeView->DisconnectDefaultHeaderContextMenu();

		// connect our context menu for header
		EXPECTED(connect(m_treeView->header(), SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(OnHeaderContextMenu(const QPoint &))));

		EXPECTED(connect(m_treeView, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(OnContextMenu(const QPoint &))));
		EXPECTED(connect(m_treeView, SIGNAL(activated(const QModelIndex &)), this, SLOT(OnActivated(const QModelIndex &))));
		EXPECTED(connect(m_treeView, &ExplorerTreeView::SignalClickOnSelectedItem, this, [this](const QModelIndex& i) { OnClickedSelectedItem(i); }));
		EXPECTED(connect(m_treeView, &ExplorerTreeView::SignalStartDrag, this, [this](const QModelIndex& i) { OnStartDrag(i); }));

		m_model = new ExplorerModel(explorerData, this);
		m_model->SetRootByIndex(m_explorerRootIndex);

		layout->addWidget(m_treeView);
		OnFilterTextChanged(QString());
		m_searchBox->SetAutoExpandOnSearch(m_treeView);
		m_searchBox->EnableContinuousSearch(true);
	}

	EXPECTED(connect(explorerData, SIGNAL(SignalEntryImported(ExplorerEntry*, ExplorerEntry*)), this, SLOT(OnEntryImported(ExplorerEntry*, ExplorerEntry*))));
	EXPECTED(connect(explorerData, SIGNAL(SignalEntryLoaded(ExplorerEntry*)), this, SLOT(OnEntryLoaded(ExplorerEntry*))));
	EXPECTED(connect(explorerData, SIGNAL(SignalRefreshFilter()), this, SLOT(OnRefreshFilter())));
	EXPECTED(connect(explorerData, &ExplorerData::SignalBeginBatchChange, this, &ExplorerPanel::OnExplorerBeginBatchChange));
	EXPECTED(connect(explorerData, &ExplorerData::SignalEndBatchChange, this, &ExplorerPanel::OnExplorerEndBatchChange));
	EXPECTED(connect(explorerData, &ExplorerData::SignalEntryModified, this, &ExplorerPanel::OnExplorerEntryModified));
}

ExplorerPanel::~ExplorerPanel()
{
	m_treeView->setModel(0);
}

void ExplorerPanel::SetDockWidget(QDockWidget* dockWidget)
{
	m_dockWidget = dockWidget;
	UpdateRootMenu();
}

void ExplorerPanel::SetRootIndex(int rootIndex)
{
	m_explorerRootIndex = rootIndex;
	UpdateRootMenu();
}

QModelIndex ExplorerPanel::FindIndexByEntry(ExplorerEntry* entry) const
{
	if (m_model->GetRootIndex() > 0 && m_model->GetActiveRoot()->subtree != entry->subtree)
		return QModelIndex();
	QModelIndex index = m_model->ModelIndexFromEntry(entry, 0);
	if (m_treeView->model() == m_filterModel)
		index = m_filterModel->mapFromSource(index);
	return index;
}

void ExplorerPanel::SetSelectedEntries(const ExplorerEntries& selectedEntries)
{
	QModelIndex singleIndex;
	QItemSelection newSelection;
	for (size_t i = 0; i < selectedEntries.size(); ++i)
	{
		QModelIndex index = FindIndexByEntry(selectedEntries[i]);
		if (!index.isValid())
			continue;

		if (singleIndex.isValid())
			singleIndex = QModelIndex();
		else
			singleIndex = index;
		newSelection.append(QItemSelectionRange(index));
	}

	m_treeView->selectionModel()->select(newSelection, QItemSelectionModel::ClearAndSelect);
	if (singleIndex.isValid())
		m_treeView->scrollTo(singleIndex);

	m_explorerData->LoadEntries(selectedEntries);
}

bool ExplorerPanel::IsEntrySelected(ExplorerEntry* entry) const
{
	if (!entry)
		return false;
	QModelIndex index = FindIndexByEntry(entry);
	return m_treeView->selectionModel()->isSelected(index);
}

void ExplorerPanel::OnRootButtonPressed()
{
	UpdateRootMenu();
}

void ExplorerPanel::OnFilterButtonToggled(bool filterMode)
{
	m_filterMode = filterMode;
	OnFilterOptionsChanged();
	m_filterOptionsTree->setVisible(m_filterMode);
}

void ExplorerPanel::OnFilterOptionsChanged()
{
	m_filterModel->setFilterOptions(m_filterMode ? *m_filterOptions : FilterOptions());
}

void ExplorerPanel::SetExpanded(const QModelIndex& index, bool bExpand, uint depth)
{
	m_treeView->setExpanded(index, bExpand);
	if (depth--)
	{
		if (!m_treeView->model())
			return;

		int numChildren = m_treeView->model()->rowCount(index);
		for (int i = 0; i < numChildren; ++i)
		{
			QModelIndex child = m_treeView->model()->index(i, 0, index);
			SetExpanded(child, bExpand, depth);
		}
	}
}

void ExplorerPanel::OnRootSelected(bool)
{
	int newRoot = 0;
	QAction* action = qobject_cast<QAction*>(sender());
	if (action)
		newRoot = action->data().toInt();

	m_explorerRootIndex = newRoot;
	m_model->SetRootByIndex(newRoot);
	if (m_filterModel)
		m_filterModel->invalidate();
	UpdateRootMenu();
}

void ExplorerPanel::UpdateRootMenu()
{
	if (!m_rootMenu)
		return;

	const auto pushRootMenuAction = [this](QString actionLabel) -> void
	{
		const int entryIndex = int32(m_rootMenuActions.size());

		QAction* const pAction = new QAction(std::move(actionLabel), m_rootMenu);
		pAction->setCheckable(true);
		pAction->setChecked(entryIndex == m_explorerRootIndex);
		pAction->setData(QVariant(entryIndex));
		EXPECTED(connect(pAction, &QAction::triggered, this, &ExplorerPanel::OnRootSelected));

		m_rootMenuActions.push_back(pAction);
	};

	m_rootMenuActions.clear();
	pushRootMenuAction("All Types");
	for (ExplorerEntry* const entry : m_explorerData->GetRoot()->children)
	{
		QString label = QString(entry->name.c_str()).remove(QRegExp(" \\(.*"));
		pushRootMenuAction(std::move(label));
	}
	std::sort(m_rootMenuActions.begin() + 1, m_rootMenuActions.end(), [](const QAction* lhs, const QAction* rhs) { return lhs->text() < rhs->text(); });

	m_rootMenu->clear();
	m_rootMenu->addAction(m_rootMenuActions[0]);
	m_rootMenu->addSeparator();
	std::for_each(m_rootMenuActions.begin() + 1, m_rootMenuActions.end(), [this](QAction* pAction)
	{ 
		m_rootMenu->addAction(pAction); 
	});

	const QAction* pActiveAction = *std::find_if(m_rootMenuActions.begin(), m_rootMenuActions.end(), [](const QAction* action) { return action->isChecked(); });
	m_rootButton->setText(pActiveAction->text());
	if (m_dockWidget)
	{
		if (m_explorerRootIndex == 0)
		{
			m_dockWidget->setWindowTitle("Assets");
		}
		else
		{
			m_dockWidget->setWindowTitle(QString("Assets: ") + pActiveAction->text());
		}
	}
}

void ExplorerPanel::OnFilterTextChanged(const QString& str)
{
	bool modelChanged = m_treeView->model() != m_filterModel || m_filterModel == 0;
	if (modelChanged)
	{
		if (!m_filterModel)
		{
			m_filterModel = new ExplorerFilterProxyModel(this);
			m_filterModel->setSourceModel(m_model);
			m_filterModel->setDynamicSortFilter(false);
		}

		SetTreeViewModel(m_filterModel);
		m_searchBox->SetSearchFunction(m_filterModel, &ExplorerFilterProxyModel::setFilterTokenizedString);
	}

	if (m_filterModel)
		m_filterModel->invalidate();
}

void ExplorerPanel::OnTreeSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	SignalSelectionChanged();
}

void ExplorerPanel::GetSelectedEntries(ExplorerEntries* entries, BMerge merge) const
{
	if (!merge)
		entries->clear();

	if (!m_treeView->selectionModel())
		return;

	QItemSelection selection = m_treeView->selectionModel()->selection();
	QItemSelection selectedOriginal;
	if (m_treeView->model() == m_filterModel)
		selectedOriginal = m_filterModel->mapSelectionToSource(selection);
	else
		selectedOriginal = selection;

	const QModelIndexList& indices = selectedOriginal.indexes();
	size_t numIndices = indices.size();
	for (size_t i = 0; i < numIndices; ++i)
	{
		const QModelIndex& index = indices[i];
		ExplorerEntry* entry = (ExplorerEntry*)index.internalPointer();
		entries->push_back(entry);
	}

	// as we may select cells in multiple columns we may have duplicated entries at this point
	std::sort(entries->begin(), entries->end());
	entries->erase(std::unique(entries->begin(), entries->end()), entries->end());
}

ExplorerEntry* ExplorerPanel::GetCurrentEntry() const
{
	return GetEntryByIndex(m_treeView->currentIndex());
}

void ExplorerPanel::SetCurrentEntry(ExplorerEntry* entry)
{
	QModelIndex index = FindIndexByEntry(entry);
	if (!index.isValid())
		return;

	m_treeView->setCurrentIndex(index);
	m_treeView->scrollTo(index);
}

void ExplorerPanel::OnHeaderContextMenu(const QPoint& pos)
{
	QMenu menu;

	int columnCount = m_explorerData->GetColumnCount();
	for (int i = 1; i < columnCount; ++i)
	{
		QAction* action = menu.addAction(m_explorerData->GetColumnLabel(i), this, SLOT(OnHeaderColumnToggle()));
		action->setCheckable(true);
		action->setChecked(!m_treeView->header()->isSectionHidden(i));
		action->setData(QVariant(i));
	}

	menu.exec(QCursor::pos());
}

void ExplorerPanel::OnHeaderColumnToggle()
{
	QObject* senderPtr = QObject::sender();

	if (QAction* action = qobject_cast<QAction*>(senderPtr))
	{
		int column = action->data().toInt();
		m_treeView->header()->setSectionHidden(column, !m_treeView->header()->isSectionHidden(column));
	}

	int numVisibleSections = 0;
	for (int i = 0; i < m_treeView->model()->columnCount(); ++i)
		if (!m_treeView->header()->isSectionHidden(i))
			++numVisibleSections;

	if (numVisibleSections == 0)
		m_treeView->header()->setSectionHidden(0, false);
}

void ExplorerPanel::OnContextMenu(const QPoint& pos)
{
	QMenu menu;
	menu.addAction("Copy Name", this, SLOT(OnMenuCopyName()));
	menu.addAction("Copy Path", this, SLOT(OnMenuCopyPath()), QKeySequence("Ctrl+C"));
	menu.addAction("Paste Selection", this, SLOT(OnMenuPasteSelection()), QKeySequence("Ctrl+V"));
	menu.addAction("Expand All", this, SLOT(OnMenuExpandAll()));
	menu.addAction("Collapse All", this, SLOT(OnMenuCollapseAll()));
	menu.addSeparator();

	ExplorerEntries entries;
	ExplorerActions actions;
	GetSelectedEntries(&entries);
	m_explorerData->GetCommonActions(&actions, entries);

	for (size_t i = 0; i < actions.size(); ++i)
	{
		const ExplorerAction& action = actions[i];
		if (!action.func)
		{
			menu.addSeparator();
			continue;
		}

		ExplorerActionHandler* handler(new ExplorerActionHandler(action));
		EXPECTED(connect(handler, SIGNAL(SignalAction(const ExplorerAction &)), this, SLOT(OnExplorerAction(const ExplorerAction &))));

		QAction* qaction = new QAction(CryIcon(QString(action.icon)), action.text, &menu);
		qaction->setPriority(((action.flags & ACTION_IMPORTANT) != 0) ? QAction::NormalPriority : QAction::LowPriority);
		qaction->setToolTip(action.description);
		qaction->setEnabled((action.flags & ACTION_DISABLED) == 0);
		if (action.flags & ACTION_IMPORTANT)
		{
			QFont boldFont;
			boldFont.setBold(true);
			qaction->setFont(boldFont);
		}
		connect(qaction, SIGNAL(triggered()), handler, SLOT(OnTriggered()));

		menu.addAction(qaction);
	}

	menu.exec(QCursor::pos());
}

void ExplorerPanel::OnMenuCopyName()
{
	ExplorerEntries entries;
	GetSelectedEntries(&entries);

	QString str;

	if (entries.size() == 1)
		str = QString::fromLocal8Bit(entries[0]->name.c_str());
	else
	{
		for (size_t i = 0; i < entries.size(); ++i)
		{
			str += QString::fromLocal8Bit(entries[i]->name.c_str());
			str += "\n";
		}
	}

	QApplication::clipboard()->setText(str);
}

void ExplorerPanel::OnMenuCopyPath()
{
	ExplorerEntries entries;
	GetSelectedEntries(&entries);

	QString str;

	if (entries.size() == 1)
		str = QString::fromLocal8Bit(entries[0]->path.c_str());
	else
	{
		for (size_t i = 0; i < entries.size(); ++i)
		{
			str += QString::fromLocal8Bit(entries[i]->path.c_str());
			str += "\n";
		}
	}

	QApplication::clipboard()->setText(str);
}

void ExplorerPanel::OnMenuPasteSelection()
{
	ExplorerEntries entries;

	QString str = QApplication::clipboard()->text();
	QStringList items = str.split("\n", QString::SkipEmptyParts);
	int subtreeCount = m_explorerData->GetSubtreeCount();
	for (int subtree = 0; subtree < subtreeCount; ++subtree)
	{
		const IExplorerEntryProvider* provider = m_explorerData->GetSubtreeProvider(subtree);
		for (size_t i = 0; i < items.size(); ++i)
		{
			ExplorerEntry* entry = m_explorerData->FindEntryByPath(provider, items[i].toLocal8Bit().data());
			if (entry)
				entries.push_back(entry);
		}
	}

	SetSelectedEntries(entries);
	if (!entries.empty())
	{
		QModelIndex index = FindIndexByEntry(entries[0]);
		if (index.isValid())
		{
			m_treeView->selectionModel()->select(index, QItemSelectionModel::Current);
			m_treeView->scrollTo(index);
		}
	}
}

void ExplorerPanel::OnMenuExpandAll()
{
	QItemSelection selection = m_treeView->selectionModel()->selection();
	const QModelIndexList& indices = selection.indexes();
	for (auto index : indices)
	{
		SetExpanded(index, true, ~0);
	}
}

void ExplorerPanel::OnMenuCollapseAll()
{
	QItemSelection selection = m_treeView->selectionModel()->selection();
	const QModelIndexList& indices = selection.indexes();
	for (auto index : indices)
	{
		SetExpanded(index, false, ~0);
	}
}

void ExplorerPanel::OnExplorerAction(const ExplorerAction& action)
{
	if (!action.func)
		return;

	ActionContextAndOutput x;
	x.window = this;

	GetSelectedEntries(&x.entries);

	action.func(x);

	if (!x.output->errorToDetails.empty())
	{
		x.output->Show(this);
	}
}

ExplorerEntry* ExplorerPanel::GetEntryByIndex(const QModelIndex& index) const
{
	QModelIndex realIndex;
	if (m_treeView->model() == m_filterModel)
		realIndex = m_filterModel->mapToSource(index);
	else
		realIndex = index;

	return ((ExplorerEntry*)realIndex.internalPointer());
}

ExplorerEntry* ExplorerPanel::GetEntryByPath(const char* szPath) const
{
	// TODO: This is super inefficient...
	for (int row = 0; row < m_filterModel->rowCount(); ++row)
	{
		for (int column = 0; column < m_filterModel->columnCount(); ++column)
		{
			const QModelIndex index = m_filterModel->index(row, column);
			ExplorerEntry* const pEntry = GetEntryByIndex(index);
			if (pEntry->path == szPath)
			{
				return pEntry;
			}
		}
	}

	return nullptr;
}

void ExplorerPanel::OnClickedSelectedItem(const QModelIndex& index)
{
	QModelIndex realIndex;
	if (m_treeView->model() == m_filterModel)
		realIndex = m_filterModel->mapToSource(index);
	else
		realIndex = index;

	ExplorerEntry* entry = ((ExplorerEntry*)realIndex.internalPointer());
	if (entry)
	{
		m_explorerData->SignalSelectedEntryClicked(entry);
	}
}

void ExplorerPanel::OnStartDrag(const QModelIndex& index)
{
	QModelIndex realIndex;
	if (m_treeView->model() == m_filterModel)
		realIndex = m_filterModel->mapToSource(index);
	else
		realIndex = index;
	ExplorerEntry* entry = ((ExplorerEntry*)realIndex.internalPointer());
	if (entry)
		SignalStartDrag(entry);
}

void ExplorerPanel::OnActivated(const QModelIndex& index)
{
	SignalActivated(GetEntryByIndex(index));
}

void ExplorerPanel::OnEntryImported(ExplorerEntry* entry, ExplorerEntry* oldEntry)
{
	// this invalidation is hack which is needed because our model implementation
	// doesn't propogate updates from the children of filtered out items
	if (m_filterModel)
		m_filterModel->invalidate();

	QModelIndex newIndex = FindIndexByEntry(entry);
	if (newIndex.isValid())
	{
		m_treeView->scrollTo(newIndex);
		m_treeView->selectionModel()->select(newIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
	}
}

void ExplorerPanel::OnAssetLoaded()
{
	// hack
	m_filterModel->invalidate();
}

void ExplorerPanel::OnRefreshFilter()
{
	OnFilterTextChanged(m_searchBox->text());
}

void ExplorerPanel::OnEntryLoaded(ExplorerEntry* entry)
{
	// this invalidation is hack which is needed because our model implementation
	// doesn't propogate updates from the children of filtered out items
	if (m_filterModel)
		m_filterModel->invalidate();

	QModelIndex newIndex = FindIndexByEntry(entry);
	if (newIndex.isValid())
	{
		SetExpanded(newIndex, true, m_searchBox->text().isEmpty() ? 1 : ~0);
	}
}

void ExplorerPanel::SetTreeViewModel(QAbstractItemModel* model)
{
	if (m_treeView->model() != model)
	{
		m_treeView->setModel(model);
		connect(m_treeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(OnTreeSelectionChanged(const QItemSelection &, const QItemSelection &)));
		m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
		m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
		m_treeView->setDragEnabled(true);
		m_treeView->viewport()->setAcceptDrops(false);
		m_treeView->setDropIndicatorShown(true);
		m_treeView->setDragDropMode(QAbstractItemView::DragOnly);

		m_treeView->header()->setStretchLastSection(true);

		for (int i = 1; i < m_explorerData->GetColumnCount(); ++i)
		{
			const ExplorerColumn* column = m_explorerData->GetColumn(i);
			if (column)
			{
				if (column->format == ExplorerColumn::TEXT)
					m_treeView->header()->setSectionResizeMode(i, QHeaderView::Interactive);
				else
					m_treeView->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
				if (!column->visibleByDefault)
					m_treeView->header()->hideSection(i);
			}
		}
		m_treeView->header()->setSortIndicator(0, Qt::AscendingOrder);

		for (int i = m_explorerData->GetColumnCount(); i < model->columnCount(); ++i)
			m_treeView->header()->hideSection(i);
	}
}

void ExplorerPanel::OnExplorerEndReset()
{
	m_treeView->expandToDepth(1);
}

void ExplorerPanel::Serialize(Serialization::IArchive& ar)
{
	ar(m_explorerRootIndex, "explorerRootIndex");
	ar(m_filterMode, "filterMode");
	ar(*m_filterOptions, "filterOptions");
	if (ar.isInput() && m_model)
	{
		m_model->SetRootByIndex(m_explorerRootIndex);
		if (m_filterModel)
			m_filterModel->invalidate();
	}

	QString text = m_searchBox->text();
	ar(text, "filter");
	if (ar.isInput())
		m_searchBox->setText(text);

	if (ar.isInput() && m_treeView && m_treeView->model())
	{
		for (int i = 0; i < m_treeView->model()->columnCount(); ++i)
		{
			const ExplorerColumn* column = m_explorerData->GetColumn(i);
			if (column)
				m_treeView->header()->setSectionHidden(i, !column->visibleByDefault);
		}
	}
	ar(static_cast<QTreeView*>(m_treeView), "treeView");
	if (ar.isInput())
		m_treeView->header()->showSection(0);

	if (ar.isInput())
	{
		UpdateRootMenu();
		m_filterOptionsTree->revert();
		m_filterButton->setChecked(m_filterMode);
		m_filterModel->setFilterOptions(m_filterMode ? *m_filterOptions : FilterOptions());
	}
}

bool ExplorerPanel::eventFilter(QObject* sender, QEvent* ev)
{
	if (sender == m_treeView)
	{
		if(ev->type() == QEvent::KeyPress)
		{
			QKeyEvent* kev = static_cast<QKeyEvent*>(ev);
			int key = kev->key() | kev->modifiers();
			if (key == (Qt::Key_C | Qt::CTRL))
			{
				OnMenuCopyPath();
				return true;
			}
			else if (key == (Qt::Key_V | Qt::CTRL))
			{
				OnMenuPasteSelection();
				return true;
			}
			/* TODO
			   else if (key == Qt::Key_Space ||
			   key == Qt::Key_Comma ||
			   key == Qt::Key_Period)
			   {
			   m_mainWindow->GetPlaybackPanel()->HandleKeyEvent(key);
			   return true;
			   }
			 */
		}
		else if (ev->type() == QEvent::FocusIn)
		{
			SignalFocusIn();
		}
	}
	return QWidget::eventFilter(sender, ev);
}

void ExplorerPanel::OnExplorerBeginBatchChange(int subtree)
{
	++m_batchChangesRunning;
}

void ExplorerPanel::OnExplorerEndBatchChange(int subtree)
{
	--m_batchChangesRunning;
	if (m_batchChangesRunning == 0 && m_filterModel)
		m_filterModel->invalidate();
}

void ExplorerPanel::OnExplorerEntryModified(ExplorerEntryModifyEvent& ev)
{
	if (m_filterModel)
	{
		if (m_batchChangesRunning == 0)
			m_filterModel->invalidate();
	}
}

}

