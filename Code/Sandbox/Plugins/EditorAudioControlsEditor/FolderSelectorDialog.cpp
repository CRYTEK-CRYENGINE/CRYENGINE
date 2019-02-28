// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FolderSelectorDialog.h"

#include "FolderSelectorFilterModel.h"
#include "TreeView.h"
#include "CreateFolderDialog.h"

#include <FileSystem/OsFileSystemModels/AdvancedFileSystemModel.h>
#include <QSearchBox.h>

#include <QDialogButtonBox>
#include <QDir>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHeaderView>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CFolderSelectorDialog::CFolderSelectorDialog(QString const& assetFolderPath, QString const& targetPath, QWidget* const pParent)
	: CEditorDialog("AudioFileImportPathSelectorDialog", pParent)
	, m_pFileSystemModel(new CAdvancedFileSystemModel(this))
	, m_pFilterProxyModel(new CFolderSelectorFilterModel(assetFolderPath, this))
	, m_pAddFolderButton(new QPushButton(tr("Add Folder"), this))
	, m_pTreeView(new CTreeView(this, QAdvancedTreeView::Behavior::None))
	, m_pSearchBox(new QSearchBox(this))
	, m_assetFolderPath(assetFolderPath)
	, m_targetPath(QDir::cleanPath(targetPath))
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle("Select Target Folder");

	QDir implFolder = QDir(m_assetFolderPath);
	implFolder.cdUp();
	QString const implFolderPath = implFolder.absolutePath();

	m_pFileSystemModel->setRootPath(m_targetPath);
	m_pFileSystemModel->setFilter(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);

	m_pFilterProxyModel->setSourceModel(m_pFileSystemModel);
	QModelIndex const& targetDirIndex = m_pFileSystemModel->index(m_targetPath);
	QModelIndex const& implFolderIndex = m_pFileSystemModel->index(implFolderPath);
	m_pFilterProxyModel->SetSourceRootIndex(implFolderIndex);

	m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pTreeView->setDragEnabled(false);
	m_pTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
	m_pTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pFilterProxyModel);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
	m_pTreeView->header()->setContextMenuPolicy(Qt::PreventContextMenu);
	m_pTreeView->setRootIndex(m_pFilterProxyModel->mapFromSource(implFolderIndex));

	QModelIndex const& selectedIndex = m_pFilterProxyModel->mapFromSource(targetDirIndex);
	m_pTreeView->setCurrentIndex(selectedIndex);
	m_pTreeView->scrollTo(selectedIndex);

	int const numColumns = m_pFilterProxyModel->columnCount();

	for (int i = 1; i < numColumns; ++i)
	{
		m_pTreeView->hideColumn(i);
	}

	m_pSearchBox->SetModel(m_pFilterProxyModel);
	m_pSearchBox->SetAutoExpandOnSearch(m_pTreeView);
	m_pSearchBox->EnableContinuousSearch(true);

	auto const pDialogButtons = new QDialogButtonBox(this);
	pDialogButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	auto const pLayout = new QVBoxLayout(this);
	setLayout(pLayout);
	pLayout->addWidget(m_pAddFolderButton);
	pLayout->addWidget(m_pSearchBox);
	pLayout->addWidget(m_pTreeView);
	pLayout->addWidget(pDialogButtons);

	QObject::connect(m_pAddFolderButton, &QPushButton::clicked, this, &CFolderSelectorDialog::OpenFolderDialog);
	QObject::connect(m_pTreeView, &CTreeView::customContextMenuRequested, this, &CFolderSelectorDialog::OnContextMenu);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &CFolderSelectorDialog::OnSelectionChanged);
	QObject::connect(m_pTreeView, &CTreeView::doubleClicked, this, &CFolderSelectorDialog::OnItemDoubleClicked);
	QObject::connect(pDialogButtons, &QDialogButtonBox::accepted, this, &CFolderSelectorDialog::OnAccept);
	QObject::connect(pDialogButtons, &QDialogButtonBox::rejected, this, &CFolderSelectorDialog::reject);

	m_pSearchBox->signalOnFiltered.Connect([&]()
		{
			m_pTreeView->scrollTo(m_pTreeView->currentIndex());
	  }, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CFolderSelectorDialog::~CFolderSelectorDialog()
{
	m_pSearchBox->signalOnFiltered.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CFolderSelectorDialog::OnContextMenu(QPoint const& pos)
{
	auto const pContextMenu = new QMenu(this);
	pContextMenu->addAction(tr("Add Folder"), [&]() { OpenFolderDialog(); });
	pContextMenu->exec(QCursor::pos());
}

//////////////////////////////////////////////////////////////////////////
void CFolderSelectorDialog::OpenFolderDialog()
{
	auto const pCreateFolderDialog = new CCreateFolderDialog(this);
	QObject::connect(pCreateFolderDialog, &CCreateFolderDialog::SignalSetFolderName, this, &CFolderSelectorDialog::OnCreateFolder);
	pCreateFolderDialog->exec();
}

//////////////////////////////////////////////////////////////////////////
void CFolderSelectorDialog::OnCreateFolder(QString const& folderName)
{
	// TODO: Add dialog to set folder name. Only allow valid characters.

	QString const targetFolderPath = m_targetPath + "/" + folderName;
	QDir targetFolder(targetFolderPath);

	if (!targetFolder.exists())
	{
		targetFolder.mkpath(targetFolderPath);
	}

	QModelIndex const& targetDirIndex = m_pFileSystemModel->index(targetFolderPath);
	QModelIndex const& selectedIndex = m_pFilterProxyModel->mapFromSource(targetDirIndex);
	m_pTreeView->setCurrentIndex(selectedIndex);
	m_pTreeView->scrollTo(selectedIndex);
}

//////////////////////////////////////////////////////////////////////////
void CFolderSelectorDialog::OnSelectionChanged(QModelIndex const& index)
{
	QModelIndex const currentIndex = m_pFilterProxyModel->mapToSource(index);
	m_targetPath = m_pFileSystemModel->filePath(currentIndex);
}

//////////////////////////////////////////////////////////////////////////
void CFolderSelectorDialog::OnItemDoubleClicked(QModelIndex const& index)
{
	OnSelectionChanged(index);
	OnAccept();
}

//////////////////////////////////////////////////////////////////////////
void CFolderSelectorDialog::OnAccept()
{
	SignalSetTargetPath(m_targetPath + "/");
	accept();
}
} // namespace ACE
