// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DirectorySelectorDialog.h"

#include "DirectorySelectorFilterModel.h"
#include "TreeView.h"

#include <FileSystem/OsFileSystemModels/AdvancedFileSystemModel.h>
#include <QSearchBox.h>

#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHeaderView>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CDirectorySelectorDialog::CDirectorySelectorDialog(QString const& assetFolderPath, QString const& targetPath, QWidget* const pParent)
	: CEditorDialog("AudioFileImportPathSelectorDialog", pParent)
	, m_pFileSystemModel(new CAdvancedFileSystemModel(this))
	, m_pFilterProxyModel(new CDirectorySelectorFilterModel(assetFolderPath, this))
	, m_pTreeView(new CTreeView(this, QAdvancedTreeView::Behavior::None))
	, m_pSearchBox(new QSearchBox(this))
	, m_assetFolderPath(assetFolderPath)
	, m_targetPath(QDir::cleanPath(targetPath))
{
	setWindowTitle("Select Target Directory");

	QDir implFolder = QDir(m_assetFolderPath);
	implFolder.cdUp();
	QString const implFolderPath = implFolder.absolutePath();

	m_pFileSystemModel->setRootPath(m_targetPath);
	m_pFileSystemModel->setFilter(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);

	m_pFilterProxyModel->setSourceModel(m_pFileSystemModel);
	QModelIndex const targetDirIndex = m_pFileSystemModel->index(m_targetPath);
	QModelIndex const implFolderIndex = m_pFileSystemModel->index(implFolderPath);
	m_pFilterProxyModel->SetSourceRootIndex(implFolderIndex);

	m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pTreeView->setDragEnabled(false);
	m_pTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
	m_pTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setContextMenuPolicy(Qt::PreventContextMenu);
	m_pTreeView->setModel(m_pFilterProxyModel);
	m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
	m_pTreeView->header()->setContextMenuPolicy(Qt::PreventContextMenu);
	m_pTreeView->setRootIndex(m_pFilterProxyModel->mapFromSource(implFolderIndex));

	QModelIndex const selectedIndex = m_pFilterProxyModel->mapFromSource(targetDirIndex);
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

	auto const pWindowLayout = new QVBoxLayout(this);
	setLayout(pWindowLayout);
	pWindowLayout->addWidget(m_pSearchBox);
	pWindowLayout->addWidget(m_pTreeView);
	pWindowLayout->addWidget(pDialogButtons);
	pDialogButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &CDirectorySelectorDialog::OnSelectionChanged);
	QObject::connect(m_pTreeView, &CTreeView::doubleClicked, this, &CDirectorySelectorDialog::OnItemDoubleClicked);
	QObject::connect(pDialogButtons, &QDialogButtonBox::accepted, this, &CDirectorySelectorDialog::OnAccept);
	QObject::connect(pDialogButtons, &QDialogButtonBox::rejected, this, &CDirectorySelectorDialog::reject);

	m_pSearchBox->signalOnFiltered.Connect([&]()
		{
			m_pTreeView->scrollTo(m_pTreeView->currentIndex());
	  }, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CDirectorySelectorDialog::~CDirectorySelectorDialog()
{
	m_pSearchBox->signalOnFiltered.DisconnectById(reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
void CDirectorySelectorDialog::OnSelectionChanged(QModelIndex const& index)
{
	QModelIndex const currentIndex = m_pFilterProxyModel->mapToSource(index);
	m_targetPath = m_pFileSystemModel->filePath(currentIndex);
}

//////////////////////////////////////////////////////////////////////////
void CDirectorySelectorDialog::OnItemDoubleClicked(QModelIndex const& index)
{
	OnSelectionChanged(index);
	OnAccept();
}

//////////////////////////////////////////////////////////////////////////
void CDirectorySelectorDialog::OnAccept()
{
	SignalSetTargetPath(m_targetPath + "/");
	accept();
}
} // namespace ACE
