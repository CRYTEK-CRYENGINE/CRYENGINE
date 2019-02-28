// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetFoldersView.h"

#include "AssetFoldersModel.h"
#include "AssetThumbnailsGenerator.h"

#include "ProxyModels/DeepFilterProxyModel.h"
#include "QAdvancedTreeView.h"
#include "FilePathUtil.h"
#include "QtUtil.h"

#include <QItemSelectionModel>
#include <QMenu>

namespace Private_AssetFolderView
{

class CFoldersViewProxyModel : public QDeepFilterProxyModel
{
public:
	CFoldersViewProxyModel(bool bHideEngineFolder = false)
		: QDeepFilterProxyModel(QDeepFilterProxyModel::BehaviorFlags(QDeepFilterProxyModel::AcceptIfChildMatches | QDeepFilterProxyModel::AcceptIfParentMatches))
		, m_bHideEngineFolder(bHideEngineFolder)
	{
	}
	bool rowMatchesFilter(int sourceRow, const QModelIndex& sourceParent) const
	{
		if (m_bHideEngineFolder)
		{
			QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
			if (!index.isValid())
			{
				return false;
			}

			QString path = index.data((int)CAssetFoldersModel::Roles::FolderPathRole).toString();
			if (path.startsWith("%"))
			{
				return false;
			}
		}

		return QDeepFilterProxyModel::rowMatchesFilter(sourceRow, sourceParent);
	}
private:
	bool m_bHideEngineFolder;

};

}

CAssetFoldersView::CAssetFoldersView(bool bHideEngineFolder /*= false*/, QWidget* parent /*= nullptr*/)
	: QWidget(parent)
{
	using namespace Private_AssetFolderView;

	QAbstractItemModel* pModel = CAssetFoldersModel::GetInstance();
	CRY_ASSERT(pModel);

	auto deepFilterProxy = new CFoldersViewProxyModel(bHideEngineFolder);
	deepFilterProxy->setSourceModel(pModel);
	deepFilterProxy->setFilterKeyColumn(0);

	m_treeView = new QAdvancedTreeView();
	m_treeView->setModel(deepFilterProxy);
	m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_treeView->setUniformRowHeights(true);
	m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_treeView->setHeaderHidden(true);
	m_treeView->sortByColumn(0, Qt::AscendingOrder);

	connect(m_treeView, &QTreeView::customContextMenuRequested, this, &CAssetFoldersView::OnContextMenu);
	connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, [this]() { OnSelectionChanged(); });
	connect(deepFilterProxy, &QAbstractItemModel::dataChanged, this, &CAssetFoldersView::OnDataChanged);

	auto searchBox = new QSearchBox();
	searchBox->setPlaceholderText(tr("Search Folders"));
	searchBox->SetModel(deepFilterProxy);
	searchBox->EnableContinuousSearch(true);
	searchBox->SetAutoExpandOnSearch(m_treeView);

	auto layout = new QVBoxLayout();
	layout->setMargin(0);
	layout->addWidget(searchBox);
	layout->addWidget(m_treeView);

	setLayout(layout);

	ClearSelection();
}

CAssetFoldersView::~CAssetFoldersView()
{

}

QVariantMap CAssetFoldersView::GetState() const
{
	QVariantMap state;
	state.insert("selected", GetSelectedFolders());
	//Note : not saving the tree view state here as we have no use for it (one column, no sorting)
	return state;
}

void CAssetFoldersView::SetState(const QVariantMap& state)
{
	QVariant selectedVar = state.value("selected");
	if (selectedVar.isValid())
	{
		SelectFolders(selectedVar.toStringList());
	}
}

void CAssetFoldersView::SelectFolder(const QString& folder)
{
	auto index = CAssetFoldersModel::GetInstance()->FindIndexForFolder(folder);
	if (index.isValid())
		SelectFolder(index);
}

void CAssetFoldersView::SelectFolder(const QModelIndex& folderIndex)
{
	CRY_ASSERT(folderIndex.model() == CAssetFoldersModel::GetInstance());
	auto proxyModel = qobject_cast<QAbstractProxyModel*>(m_treeView->model());
	auto proxyIndex = proxyModel->mapFromSource(folderIndex);

	m_treeView->expand(proxyIndex.parent());
	m_treeView->scrollTo(proxyIndex);
	m_treeView->selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void CAssetFoldersView::SelectFolders(const QStringList& folders)
{
	QModelIndexList list;
	list.reserve(folders.count());

	for (auto& folder : folders)
	{
		auto index = CAssetFoldersModel::GetInstance()->FindIndexForFolder(folder);
		if (index.isValid())
			list.append(index);
	}

	SelectFolders(list);
}

void CAssetFoldersView::SelectFolders(const QModelIndexList& foldersIndices)
{
	QItemSelection selection;

	for (auto& index : foldersIndices)
	{
		CRY_ASSERT(index.model() == CAssetFoldersModel::GetInstance());
		auto proxyModel = qobject_cast<QAbstractProxyModel*>(m_treeView->model());
		auto proxyIndex = proxyModel->mapFromSource(index);

		m_treeView->expand(proxyIndex.parent());
		m_treeView->scrollTo(proxyIndex);

		selection.select(proxyIndex, proxyIndex);
	}
	
	m_treeView->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void CAssetFoldersView::ClearSelection()
{
	//select all assets by default
	QModelIndex assetsRootIndex = CAssetFoldersModel::GetInstance()->GetProjectAssetsFolderIndex();
	auto proxyModel = qobject_cast<QAbstractProxyModel*>(m_treeView->model());
	m_treeView->expand(proxyModel->mapFromSource(assetsRootIndex));
	SelectFolder(assetsRootIndex);
}

const QStringList& CAssetFoldersView::GetSelectedFolders() const
{
	return m_selectedFolders;
}

const QAdvancedTreeView* CAssetFoldersView::GetInternalView() const
{
	return m_treeView;
}

void CAssetFoldersView::setSelectionMode(QAbstractItemView::SelectionMode mode)
{
	m_treeView->setSelectionMode(mode);
}

QString CAssetFoldersView::ToFolder(const QModelIndex& index)
{
	if (index.isValid())
		return index.data((int)CAssetFoldersModel::Roles::FolderPathRole).toString();
	else
		return QString();
}

void CAssetFoldersView::OnContextMenu(const QPoint &pos)
{
	QString folder = ToFolder(m_treeView->indexAt(pos));

	if (CAssetFoldersModel::GetInstance()->IsReadOnlyFolder(folder))
		return;

	QMenu* menu = new QMenu();
	auto action = menu->addAction(CryIcon("icons:General/Element_Add.ico"), tr("Create folder"));
	connect(action, &QAction::triggered, [this, folder]() { OnCreateFolder(folder); });

	if (CAssetFoldersModel::GetInstance()->IsEmptyFolder(folder))
	{
		menu->addSeparator();

		action = menu->addAction(CryIcon("icons:General/Element_Remove.ico"), tr("Delete"));
		connect(action, &QAction::triggered, [this, folder]() { OnDeleteFolder(folder); });

		action = menu->addAction(tr("Rename"));
		connect(action, &QAction::triggered, [this, folder]() { OnRenameFolder(folder); });
	}

	menu->addSeparator();

	action = menu->addAction(tr("Open in Explorer"));
	connect(action, &QAction::triggered, [this, folder]() { OnOpenInExplorer(folder); });

	action = menu->addAction(tr("Generate Thumbnails"));
	connect(action, &QAction::triggered, [folder]()
	{
		AsseThumbnailsGenerator::GenerateThumbnailsAsync(QtUtil::ToString(folder));
	});


	menu->exec(QCursor::pos());
}

void CAssetFoldersView::OnCreateFolder(const QString& parentFolder)
{
	QString newFolderPath = CAssetFoldersModel::GetInstance()->CreateFolder(parentFolder);
	OnRenameFolder(newFolderPath);
}

void CAssetFoldersView::OnRenameFolder(const QString& folder)
{
	auto folderIndex = CAssetFoldersModel::GetInstance()->FindIndexForFolder(folder);
	auto proxyModel = qobject_cast<QAbstractProxyModel*>(m_treeView->model());
	auto proxyIndex = proxyModel->mapFromSource(folderIndex);

	m_treeView->expand(proxyIndex.parent());
	m_treeView->scrollTo(proxyIndex);
	m_treeView->selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	m_treeView->edit(proxyIndex);
}

void CAssetFoldersView::OnDeleteFolder(const QString& folder)
{
	CAssetFoldersModel::GetInstance()->DeleteFolder(folder);
	SelectFolder(QString(PathUtil::GetParentDirectory(folder.toStdString().c_str())));
}

void CAssetFoldersView::OnOpenInExplorer(const QString& folder)
{
	CAssetFoldersModel::GetInstance()->OpenFolderWithShell(folder);
}

void CAssetFoldersView::OnDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
	//assumes only one item can be changed at a time
	CRY_ASSERT(topLeft.row() == bottomRight.row());

	if (m_treeView->selectionModel()->isSelected(topLeft))
		OnSelectionChanged();
}

void CAssetFoldersView::OnSelectionChanged()
{
	m_selectedFolders.clear();

	const QModelIndexList selection = m_treeView->selectionModel()->selectedRows();

	if (selection.isEmpty())
	{
		// If no folder is selected, the content of the top-level asset folder should be displayed.
		const QModelIndex assetsRootIndex = CAssetFoldersModel::GetInstance()->GetProjectAssetsFolderIndex();
		m_selectedFolders.push_back(assetsRootIndex.data((int)CAssetFoldersModel::Roles::FolderPathRole).toString());
	}
	else
	{
		m_selectedFolders.reserve(selection.size());
		for (const QModelIndex& index : selection)
		{
			m_selectedFolders.push_back(index.data((int)CAssetFoldersModel::Roles::FolderPathRole).toString());
		}
	}

	signalSelectionChanged(m_selectedFolders);
}

