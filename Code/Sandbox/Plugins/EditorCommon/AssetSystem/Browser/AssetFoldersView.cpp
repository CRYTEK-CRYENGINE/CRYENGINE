// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetFoldersView.h"

#include "AssetFoldersModel.h"
#include "AssetThumbnailsGenerator.h"
#include "FilteredFolders.h"

#include "DragDrop.h"
#include "PathUtils.h"
#include "ProxyModels/DeepFilterProxyModel.h"
#include "QAdvancedTreeView.h"
#include "QSearchBox.h"
#include "QtUtil.h"

#include <QItemSelectionModel>
#include <QMenu>
#include <QApplication>
#include <QBoxLayout>

class CAssetFoldersView::CFoldersViewProxyModel : public QDeepFilterProxyModel
{
public:
	CFoldersViewProxyModel(bool bHideEngineFolder = false)
		: QDeepFilterProxyModel(QDeepFilterProxyModel::BehaviorFlags(QDeepFilterProxyModel::AcceptIfChildMatches | QDeepFilterProxyModel::AcceptIfParentMatches))
		, m_bHideEngineFolder(bHideEngineFolder)
		, m_pFilteredFolders(nullptr)
	{
	}
	virtual ~CFoldersViewProxyModel() override
	{
		// unsubscribe
		SetFilteredFolders(nullptr);
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

		if (m_pFilteredFolders && !m_pFilteredFolders->IsEmpty())
		{
			QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
			if (index.isValid())
			{
				QString path = index.data((int)CAssetFoldersModel::Roles::FolderPathRole).toString();
				if (!m_pFilteredFolders->Contains(path) && !CAssetFoldersModel::GetInstance()->IsEmptyFolder(path))
				{
					return false;
				}
			}
		}

		return QDeepFilterProxyModel::rowMatchesFilter(sourceRow, sourceParent);
	}

	virtual bool canDropMimeData(const QMimeData* pMimeData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override
	{
		if (QDeepFilterProxyModel::canDropMimeData(pMimeData, action, row, column, parent))
		{
			return true;
		}

		CDragDropData::ClearDragTooltip(qApp->widgetAt(QCursor::pos()));
		return false;
	}

	void SetFilteredFolders(CFilteredFolders* pFilteredFolders)
	{
		if (m_pFilteredFolders)
		{
			m_pFilteredFolders->signalInvalidate.DisconnectObject(this);
		}

		m_pFilteredFolders = pFilteredFolders;

		if (m_pFilteredFolders)
		{
			pFilteredFolders->signalInvalidate.Connect(this, &CFoldersViewProxyModel::invalidate);
			invalidate();
		}
	}

private:
	bool              m_bHideEngineFolder;
	CFilteredFolders* m_pFilteredFolders;
};

CAssetFoldersView::CAssetFoldersView(bool bHideEngineFolder /*= false*/, QWidget* parent /*= nullptr*/)
	: QWidget(parent)
{
	QAbstractItemModel* pModel = CAssetFoldersModel::GetInstance();
	CRY_ASSERT(pModel);

	m_pProxyModel.reset(new CFoldersViewProxyModel(bHideEngineFolder));
	m_pProxyModel->setSourceModel(pModel);
	m_pProxyModel->setFilterKeyColumn(0);

	m_treeView = new QAdvancedTreeView();
	m_treeView->setModel(m_pProxyModel.get());
	m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_treeView->setUniformRowHeights(true);
	m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_treeView->setHeaderHidden(true);
	m_treeView->sortByColumn(0, Qt::AscendingOrder);
	m_treeView->setDragDropMode(QAbstractItemView::DragDrop);

	connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged, [this]() { OnSelectionChanged(); });
	connect(m_pProxyModel.get(), &QAbstractItemModel::dataChanged, this, &CAssetFoldersView::OnDataChanged);

	QWidget* pSearchBoxContainer = new QWidget();
	pSearchBoxContainer->setObjectName("SearchBoxContainer");

	QHBoxLayout* pSearchBoxLayout = new QHBoxLayout();
	pSearchBoxLayout->setAlignment(Qt::AlignTop);
	pSearchBoxLayout->setMargin(0);
	pSearchBoxLayout->setSpacing(0);

	QSearchBox* pSearchBox = new QSearchBox();
	pSearchBox->setPlaceholderText(tr("Search Folders"));
	pSearchBox->SetModel(m_pProxyModel.get());
	pSearchBox->EnableContinuousSearch(true);
	pSearchBox->SetAutoExpandOnSearch(m_treeView);

	pSearchBoxLayout->addWidget(pSearchBox);
	pSearchBoxContainer->setLayout(pSearchBoxLayout);

	QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->setMargin(0);
	pLayout->setSpacing(0);
	pLayout->addWidget(pSearchBoxContainer);
	pLayout->addWidget(m_treeView);

	setLayout(pLayout);

	ClearSelection();
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

void CAssetFoldersView::RenameFolder(const QModelIndex& folderIndex)
{
	CRY_ASSERT(folderIndex.model() == CAssetFoldersModel::GetInstance());

	const QAbstractProxyModel* const pProxyModel = qobject_cast<QAbstractProxyModel*>(m_treeView->model());
	const QModelIndex proxyIndex = pProxyModel->mapFromSource(folderIndex);

	m_treeView->expand(proxyIndex.parent());
	m_treeView->scrollTo(proxyIndex);
	m_treeView->selectionModel()->setCurrentIndex(proxyIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	m_treeView->edit(proxyIndex);
}

void CAssetFoldersView::RenameFolder(const QString& folder)
{
	const QModelIndex folderIndex = CAssetFoldersModel::GetInstance()->FindIndexForFolder(folder);
	if (!folderIndex.isValid())
	{
		return;
	}

	RenameFolder(folderIndex);
}

void CAssetFoldersView::SetFilteredFolders(CFilteredFolders* pFilteredFolders)
{
	static_cast<CFoldersViewProxyModel*>(m_pProxyModel.get())->SetFilteredFolders(pFilteredFolders);
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

void CAssetFoldersView::OnCreateFolder(const QString& parentFolder)
{
	QString newFolderPath = CAssetFoldersModel::GetInstance()->CreateFolder(parentFolder);
	RenameFolder(newFolderPath);
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
