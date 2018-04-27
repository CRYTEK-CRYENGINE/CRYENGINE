// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MiddlewareDataWidget.h"

#include "AudioControlsEditorPlugin.h"
#include "MiddlewareFilterProxyModel.h"
#include "ImplementationManager.h"
#include "AssetIcons.h"
#include "TreeView.h"
#include "FileImporterDialog.h"

#include <ModelUtils.h>
#include <IItemModel.h>
#include <IItem.h>
#include <FilePathUtil.h>
#include <CrySystem/File/CryFile.h>
#include <QFilteringPanel.h>
#include <QSearchBox.h>
#include <QtUtil.h>
#include <FileDialogs/SystemFileDialog.h>

#include <QDir>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CMiddlewareDataWidget::CMiddlewareDataWidget(QWidget* const pParent)
	: QWidget(pParent)
	, m_pMiddlewareFilterProxyModel(new CMiddlewareFilterProxyModel(this))
	, m_pImplItemModel(nullptr)
	, m_pImportButton(new QPushButton(tr("Import Files"), this))
	, m_pTreeView(new CTreeView(this))
	, m_nameColumn(0)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	if (g_pIImpl != nullptr)
	{
		m_pImplItemModel = g_pIImpl->GetItemModel();
		CRY_ASSERT_MESSAGE(m_pImplItemModel != nullptr, "Impl itemmodel is null pointer.");
		m_nameColumn = m_pImplItemModel->GetNameColumn();
		m_pImportButton->setVisible(g_pIImpl->IsFileImportAllowed());

		m_pImplItemModel->SignalDroppedFiles.Connect([&](FileImportInfos const& fileImportInfos, QString const& targetFolderName)
			{
				OpenFileImporter(fileImportInfos, targetFolderName);
		  }, reinterpret_cast<uintptr_t>(this));
	}
	else
	{
		setEnabled(false);
		m_pImportButton->setVisible(false);
	}

	m_pMiddlewareFilterProxyModel->setSourceModel(m_pImplItemModel);
	m_pMiddlewareFilterProxyModel->setFilterKeyColumn(m_nameColumn);

	m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pTreeView->setDragEnabled(true);
	SetDragDropMode();
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pTreeView->setTreePosition(m_nameColumn);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pMiddlewareFilterProxyModel);
	m_pTreeView->sortByColumn(m_nameColumn, Qt::AscendingOrder);
	m_pTreeView->header()->setMinimumSectionSize(25);
	SetColumnResizeModes();
	m_pTreeView->SetNameColumn(m_nameColumn);
	m_pTreeView->SetNameRole(static_cast<int>(ModelUtils::ERoles::Name));
	m_pTreeView->TriggerRefreshHeaderColumns();

	m_pFilteringPanel = new QFilteringPanel("ACEMiddlewareData", m_pMiddlewareFilterProxyModel, this);
	m_pFilteringPanel->SetContent(m_pTreeView);
	m_pFilteringPanel->GetSearchBox()->SetAutoExpandOnSearch(m_pTreeView);

	auto const pMainLayout = new QVBoxLayout(this);
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(m_pImportButton);
	pMainLayout->addWidget(m_pFilteringPanel);

	QObject::connect(m_pImportButton, &QPushButton::clicked, this, &CMiddlewareDataWidget::OnImportFiles);
	QObject::connect(m_pTreeView, &CTreeView::customContextMenuRequested, this, &CMiddlewareDataWidget::OnContextMenu);

	g_assetsManager.SignalConnectionAdded.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  m_pMiddlewareFilterProxyModel->invalidate();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalConnectionRemoved.Connect([this]()
		{
			if (!g_assetsManager.IsLoading())
			{
			  m_pMiddlewareFilterProxyModel->invalidate();
			}
	  }, reinterpret_cast<uintptr_t>(this));

	g_implementationManager.SignalImplementationAboutToChange.Connect([this]()
		{
			if (g_pIImpl != nullptr)
			{
			  ClearFilters();
			  m_pMiddlewareFilterProxyModel->ClearFilters();
			  m_pImplItemModel->SignalDroppedFiles.DisconnectById(reinterpret_cast<uintptr_t>(this));
			  m_pMiddlewareFilterProxyModel->setSourceModel(nullptr);
			  m_pTreeView->setModel(nullptr);
			  m_pImplItemModel = nullptr;
			  m_nameColumn = 0;
			}

	  }, reinterpret_cast<uintptr_t>(this));

	g_implementationManager.SignalImplementationChanged.Connect([this]()
		{
			if (g_pIImpl != nullptr)
			{
			  m_pImplItemModel = g_pIImpl->GetItemModel();
			  CRY_ASSERT_MESSAGE(m_pImplItemModel != nullptr, "ImplItemModel is null pointer.");
			  m_nameColumn = m_pImplItemModel->GetNameColumn();
			  m_pImportButton->setVisible(g_pIImpl->IsFileImportAllowed());
			  m_pMiddlewareFilterProxyModel->setSourceModel(m_pImplItemModel);
			  m_pMiddlewareFilterProxyModel->setFilterKeyColumn(m_nameColumn);
			  m_pTreeView->setModel(m_pMiddlewareFilterProxyModel);
			  m_pTreeView->setTreePosition(m_nameColumn);
			  m_pTreeView->sortByColumn(m_nameColumn, Qt::AscendingOrder);
			  m_pTreeView->SetNameColumn(m_nameColumn);
			  SetColumnResizeModes();
			  setEnabled(true);

			  m_pImplItemModel->SignalDroppedFiles.Connect([&](FileImportInfos const& fileImportInfos, QString const& targetFolderName)
				{
					OpenFileImporter(fileImportInfos, targetFolderName);
			  }, reinterpret_cast<uintptr_t>(this));
			}
			else
			{
			  setEnabled(false);
			  m_pImportButton->setVisible(false);
			}

			SetDragDropMode();
	  }, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CMiddlewareDataWidget::~CMiddlewareDataWidget()
{
	g_assetsManager.SignalConnectionAdded.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalConnectionRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implementationManager.SignalImplementationAboutToChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implementationManager.SignalImplementationChanged.DisconnectById(reinterpret_cast<uintptr_t>(this));

	if (m_pImplItemModel != nullptr)
	{
		m_pImplItemModel->SignalDroppedFiles.DisconnectById(reinterpret_cast<uintptr_t>(this));
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::SetDragDropMode()
{
	if (g_pIImpl != nullptr)
	{
		m_pTreeView->setDragDropMode(g_pIImpl->IsFileImportAllowed() ? QAbstractItemView::DragDrop : QAbstractItemView::DragOnly);
	}
	else
	{
		m_pTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::SetColumnResizeModes()
{
	if (g_pIImpl != nullptr)
	{
		m_pTreeView->header()->resizeSections(QHeaderView::Interactive);

		for (auto const& columns : m_pImplItemModel->GetColumnResizeModes())
		{
			m_pTreeView->header()->setSectionResizeMode(columns.first, columns.second);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::OnContextMenu(QPoint const& pos)
{
	auto const pContextMenu = new QMenu(this);
	auto const& selection = m_pTreeView->selectionModel()->selectedRows(m_nameColumn);

	if (!selection.isEmpty())
	{
		if (selection.count() == 1)
		{
			ControlId const itemId = selection[0].data(static_cast<int>(ModelUtils::ERoles::Id)).toInt();
			Impl::IItem const* const pIItem = g_pIImpl->GetItem(itemId);

			if ((pIItem != nullptr) && ((pIItem->GetFlags() & EItemFlags::IsConnected) != 0))
			{
				auto const pConnectionsMenu = new QMenu(pContextMenu);
				auto const& controls = g_assetsManager.GetControls();
				int count = 0;

				for (auto const pControl : controls)
				{
					if (pControl->GetConnection(pIItem) != nullptr)
					{
						pConnectionsMenu->addAction(GetAssetIcon(pControl->GetType()), tr(pControl->GetName()), [=]()
							{
								SignalSelectConnectedSystemControl(*pControl, pIItem->GetId());
						  });

						++count;
					}
				}

				if (count > 0)
				{
					pConnectionsMenu->setTitle(tr("Connections (" + ToString(count) + ")"));
					pContextMenu->addMenu(pConnectionsMenu);
					pContextMenu->addSeparator();
				}
			}

			if ((pIItem != nullptr) && ((pIItem->GetPakStatus() & EPakStatus::OnDisk) != 0) && !pIItem->GetFilePath().IsEmpty())
			{
				pContextMenu->addAction(tr("Show in File Explorer"), [&]()
					{
						QtUtil::OpenInExplorer((PathUtil::GetGameFolder() + "/" + pIItem->GetFilePath()).c_str());
				  });

				pContextMenu->addSeparator();
			}
		}

		pContextMenu->addAction(tr("Expand Selection"), [&]() { m_pTreeView->ExpandSelection(m_pTreeView->GetSelectedIndexes()); });
		pContextMenu->addAction(tr("Collapse Selection"), [&]() { m_pTreeView->CollapseSelection(m_pTreeView->GetSelectedIndexes()); });
		pContextMenu->addSeparator();
	}

	pContextMenu->addAction(tr("Expand All"), [&]() { m_pTreeView->expandAll(); });
	pContextMenu->addAction(tr("Collapse All"), [&]() { m_pTreeView->collapseAll(); });

	if (g_pIImpl->IsFileImportAllowed())
	{
		pContextMenu->addSeparator();
		pContextMenu->addAction(tr("Import Files"), [&]() { OnImportFiles(); });
	}

	pContextMenu->exec(QCursor::pos());
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::OpenFileImporter(FileImportInfos const& fileImportInfos, QString const& targetFolderName)
{
	FileImportInfos fileInfos = fileImportInfos;

	QString const assetFolderPath = QtUtil::ToQString(PathUtil::GetGameFolder() + "/" + g_assetsManager.GetAssetFolderPath());
	QString const targetFolderPath = assetFolderPath + targetFolderName;

	QDir const targetFolder(targetFolderPath);
	QString const fullTargetPath = targetFolder.absolutePath() + "/";

	for (auto& fileInfo : fileInfos)
	{
		if (fileInfo.isTypeSupported && fileInfo.sourceInfo.isFile())
		{
			QString const targetPath = fullTargetPath + fileInfo.parentFolderName + fileInfo.sourceInfo.fileName();
			QFileInfo const targetFile(targetPath);

			fileInfo.targetInfo = targetFile;
			fileInfo.actionType = (targetFile.isFile() ? SFileImportInfo::EActionType::Replace : SFileImportInfo::EActionType::New);
		}
	}

	auto const pFileImporterDialog = new CFileImporterDialog(fileInfos, QDir(assetFolderPath).absolutePath(), fullTargetPath, this);
	m_pTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
	QObject::connect(pFileImporterDialog, &CFileImporterDialog::SignalImporterClosed, this, &CMiddlewareDataWidget::SetDragDropMode);
	pFileImporterDialog->exec();
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::OnImportFiles()
{
	CSystemFileDialog::RunParams runParams;
	runParams.extensionFilters = m_pImplItemModel->GetExtensionFilters();
	runParams.title = tr("Import Audio Files");
	runParams.buttonLabel = tr("Import");
	std::vector<QString> const importedFiles = CSystemFileDialog::RunImportMultipleFiles(runParams, this);

	if (!importedFiles.empty())
	{
		FileImportInfos fileInfos;
		auto const& supportedFileTypes = m_pImplItemModel->GetSupportedFileTypes();

		for (auto const& filePath : importedFiles)
		{
			QFileInfo const& fileInfo(filePath);

			if (fileInfo.isFile())
			{
				fileInfos.emplace_back(fileInfo, supportedFileTypes.contains(fileInfo.suffix(), Qt::CaseInsensitive));
			}
		}

		QString targetFolderName = "";

		if (!m_pTreeView->selectionModel()->selectedRows(m_nameColumn).isEmpty())
		{
			targetFolderName = m_pImplItemModel->GetTargetFolderName(m_pMiddlewareFilterProxyModel->mapToSource(m_pTreeView->currentIndex()));
		}

		OpenFileImporter(fileInfos, targetFolderName);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::SelectConnectedImplItem(ControlId const itemId)
{
	ClearFilters();
	auto const& matches = m_pMiddlewareFilterProxyModel->match(m_pMiddlewareFilterProxyModel->index(0, 0, QModelIndex()), static_cast<int>(ModelUtils::ERoles::Id), itemId, 1, Qt::MatchRecursive);

	if (!matches.isEmpty())
	{
		m_pTreeView->setFocus();
		m_pTreeView->selectionModel()->setCurrentIndex(matches.first(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::ClearFilters()
{
	m_pFilteringPanel->GetSearchBox()->clear();
	m_pFilteringPanel->Clear();
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::Reset()
{
	if (g_pIImpl != nullptr)
	{
		ClearFilters();
		m_pMiddlewareFilterProxyModel->invalidate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::BackupTreeViewStates()
{
	m_pTreeView->BackupExpanded();
	m_pTreeView->BackupSelection();
}

//////////////////////////////////////////////////////////////////////////
void CMiddlewareDataWidget::RestoreTreeViewStates()
{
	m_pTreeView->RestoreExpanded();
	m_pTreeView->RestoreSelection();
}
} // namespace ACE