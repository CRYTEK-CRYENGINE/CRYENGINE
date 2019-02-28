// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileImporterDialog.h"

#include "FileImporterModel.h"
#include "TreeView.h"
#include "FolderSelectorDialog.h"

#include <ProxyModels/AttributeFilterProxyModel.h>
#include <QtUtil.h>
#include <CrySystem/ISystem.h>
#include <CryIcon.h>

#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CFileImporterDialog::CFileImporterDialog(FileImportInfos const& fileInfos, QString const& assetFolderPath, QString const& targetPath, QWidget* const pParent)
	: CEditorDialog("AudioFileImporterDialog", pParent)
	, m_fileImportInfos(fileInfos)
	, m_assetFolderPath(assetFolderPath)
	, m_targetPath(targetPath)
	, m_pTargetDirLineEdit(new QLineEdit(this))
	, m_pFileImporterModel(new CFileImporterModel(m_fileImportInfos, assetFolderPath, targetPath, this))
	, m_pAttributeFilterProxyModel(new QAttributeFilterProxyModel(QAttributeFilterProxyModel::BaseBehavior, this))
	, m_pTreeView(new CTreeView(this, QAdvancedTreeView::BehaviorFlags(QAdvancedTreeView::Behavior::PreserveSelectionAfterReset | QAdvancedTreeView::Behavior::UseItemModelAttribute)))
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(tr("Audio File Importer"));

	auto const pTargetDirLabel = new QLabel(tr("Target folder:"));
	m_pTargetDirLineEdit->setText(m_targetPath);
	m_pTargetDirLineEdit->setToolTip(m_targetPath);
	m_pTargetDirLineEdit->setReadOnly(true);

	auto const pBrowseButton = new QToolButton(this);
	pBrowseButton->setIcon(CryIcon("icons:General/Folder.ico"));
	pBrowseButton->setToolTip(tr("Select target folder"));
	pBrowseButton->setIconSize(QSize(16, 16));

	auto const pSetAllLabel = new QLabel(tr("Set all to:"));

	auto const pImportButton = new QPushButton(tr("Import"));
	pImportButton->setToolTip(tr(R"(Sets action type for all supported files to "new" if target file does not exist. Otherwise sets it to "replace".)"));

	auto const pIgnoreButton = new QPushButton(tr("Ignore"));
	pIgnoreButton->setToolTip(tr(R"(Sets action type for all supported files to "ignore".)"));

	auto const pToolbarLayout = new QHBoxLayout();
	pToolbarLayout->addWidget(pTargetDirLabel);
	pToolbarLayout->addWidget(m_pTargetDirLineEdit);
	pToolbarLayout->addWidget(pBrowseButton);
	pToolbarLayout->addSpacing(25);
	pToolbarLayout->addWidget(pSetAllLabel);
	pToolbarLayout->addWidget(pImportButton);
	pToolbarLayout->addWidget(pIgnoreButton);

	m_pAttributeFilterProxyModel->setSourceModel(m_pFileImporterModel);
	m_pAttributeFilterProxyModel->setFilterKeyColumn(static_cast<int>(CFileImporterModel::EColumns::Source));

	m_pTreeView->setEditTriggers(QAbstractItemView::CurrentChanged | QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked | QAbstractItemView::EditKeyPressed);
	m_pTreeView->setDragEnabled(false);
	m_pTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pAttributeFilterProxyModel);
	m_pTreeView->setItemsExpandable(false);
	m_pTreeView->setRootIsDecorated(false);
	m_pTreeView->sortByColumn(static_cast<int>(CFileImporterModel::EColumns::Source), Qt::AscendingOrder);
	m_pTreeView->header()->setContextMenuPolicy(Qt::PreventContextMenu);
	m_pTreeView->header()->setMinimumSectionSize(25);
	m_pTreeView->header()->setStretchLastSection(false);
	m_pTreeView->header()->resizeSection(static_cast<int>(CFileImporterModel::EColumns::Source), 300);
	m_pTreeView->header()->setSectionResizeMode(static_cast<int>(CFileImporterModel::EColumns::Target), QHeaderView::Stretch);
	m_pTreeView->header()->setSectionResizeMode(static_cast<int>(CFileImporterModel::EColumns::Import), QHeaderView::ResizeToContents);
	m_pTreeView->setTextElideMode(Qt::ElideMiddle);
	m_pTreeView->TriggerRefreshHeaderColumns();

	auto const pDialogButtons = new QDialogButtonBox(this);
	pDialogButtons->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	auto const pMainLayout = new QVBoxLayout(this);
	pMainLayout->addLayout(pToolbarLayout);
	pMainLayout->addWidget(m_pTreeView);
	pMainLayout->addWidget(pDialogButtons);
	setLayout(pMainLayout);

	QDir targetFolder(m_targetPath);

	if (!targetFolder.exists())
	{
		targetFolder.mkpath(m_targetPath);
	}

	QObject::connect(pBrowseButton, &QToolButton::clicked, this, &CFileImporterDialog::OnCreateFolderSelector);

	QObject::connect(m_pFileImporterModel, &CFileImporterModel::SignalActionChanged, this, &CFileImporterDialog::OnActionChanged);

	QObject::connect(pImportButton, &QPushButton::clicked, this, &CFileImporterDialog::OnSetImportAll);
	QObject::connect(pIgnoreButton, &QPushButton::clicked, this, &CFileImporterDialog::OnSetIgnoreAll);

	QObject::connect(pDialogButtons, &QDialogButtonBox::accepted, this, &CFileImporterDialog::OnApplyImport);
	QObject::connect(pDialogButtons, &QDialogButtonBox::rejected, this, &CFileImporterDialog::reject);
}

//////////////////////////////////////////////////////////////////////////
QSize CFileImporterDialog::sizeHint() const
{
	return QSize(800, 500);
}

//////////////////////////////////////////////////////////////////////////
void CFileImporterDialog::OnCreateFolderSelector()
{
	auto const pFolderSelectorDialog = new CFolderSelectorDialog(m_assetFolderPath, m_targetPath, this);
	QObject::connect(pFolderSelectorDialog, &CFolderSelectorDialog::SignalSetTargetPath, this, &CFileImporterDialog::OnTargetPathChanged);
	pFolderSelectorDialog->exec();
}

//////////////////////////////////////////////////////////////////////////
void CFileImporterDialog::OnTargetPathChanged(QString const& targetPath)
{
	if (m_targetPath.compare(targetPath, Qt::CaseInsensitive) != 0)
	{
		m_targetPath = targetPath;
		m_pTargetDirLineEdit->setText(m_targetPath);
		m_pTargetDirLineEdit->setToolTip(m_targetPath);

		for (auto& fileInfo : m_fileImportInfos)
		{
			if (fileInfo.isTypeSupported && fileInfo.sourceInfo.isFile())
			{
				QString targetPath = m_targetPath + fileInfo.parentFolderName + fileInfo.sourceInfo.fileName();
				QFileInfo const targetFile(targetPath);

				fileInfo.targetInfo = targetFile;

				if (fileInfo.sourceInfo == fileInfo.targetInfo)
				{
					fileInfo.actionType = SFileImportInfo::EActionType::SameFile;
				}
				else
				{
					fileInfo.actionType = (targetFile.isFile() ? SFileImportInfo::EActionType::Replace : SFileImportInfo::EActionType::New);
				}
			}
		}

		m_pFileImporterModel->Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileImporterDialog::OnActionChanged(Qt::CheckState const isChecked)
{
	auto const& selection = m_pTreeView->selectionModel()->selectedRows(static_cast<int>(CFileImporterModel::EColumns::Import));

	if (selection.size() > 1)
	{
		for (auto const& index : selection)
		{
			SFileImportInfo& fileInfo = m_pFileImporterModel->ItemFromIndex(m_pAttributeFilterProxyModel->mapToSource(index));

			if (fileInfo.isTypeSupported && (fileInfo.actionType != SFileImportInfo::EActionType::SameFile))
			{
				if (isChecked == Qt::Checked)
				{
					fileInfo.actionType = fileInfo.targetInfo.isFile() ? SFileImportInfo::EActionType::Replace : SFileImportInfo::EActionType::New;
				}
				else
				{
					fileInfo.actionType = SFileImportInfo::EActionType::Ignore;
				}
			}
		}

		m_pFileImporterModel->Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileImporterDialog::OnSetImportAll()
{
	for (auto& fileInfo : m_fileImportInfos)
	{
		if (fileInfo.isTypeSupported && (fileInfo.actionType != SFileImportInfo::EActionType::SameFile))
		{
			if (fileInfo.targetInfo.isFile())
			{
				fileInfo.actionType = SFileImportInfo::EActionType::Replace;
			}
			else
			{
				fileInfo.actionType = SFileImportInfo::EActionType::New;
			}
		}
	}

	m_pFileImporterModel->Reset();
}

//////////////////////////////////////////////////////////////////////////
void CFileImporterDialog::OnSetIgnoreAll()
{
	for (auto& fileInfo : m_fileImportInfos)
	{
		if (fileInfo.isTypeSupported && (fileInfo.actionType != SFileImportInfo::EActionType::SameFile))
		{
			fileInfo.actionType = SFileImportInfo::EActionType::Ignore;
		}
	}

	m_pFileImporterModel->Reset();
}

//////////////////////////////////////////////////////////////////////////
void CFileImporterDialog::OnApplyImport()
{
	for (auto const& fileInfo : m_fileImportInfos)
	{
		if ((fileInfo.actionType == SFileImportInfo::EActionType::New) || (fileInfo.actionType == SFileImportInfo::EActionType::Replace))
		{
			QFile sourceFile(fileInfo.sourceInfo.absoluteFilePath());

			if (sourceFile.exists())
			{
				QString const targetFolderPath = m_targetPath + fileInfo.parentFolderName;
				QDir targetFolder(targetFolderPath);

				if (!targetFolder.exists())
				{
					targetFolder.mkpath(targetFolderPath);
				}

				QString const fullTargetPath = fileInfo.targetInfo.absoluteFilePath();

				if (fileInfo.actionType == SFileImportInfo::EActionType::Replace)
				{
					if (QFile(fullTargetPath).exists())
					{
						char const* const szTargetPath = QtUtil::ToString(fullTargetPath).c_str();
						DWORD const fileAttributes = GetFileAttributesA(szTargetPath);

						if ((fileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
						{
							SetFileAttributesA(szTargetPath, FILE_ATTRIBUTE_NORMAL);
						}

						if ((fileAttributes == INVALID_FILE_ATTRIBUTES) || !DeleteFile(szTargetPath))
						{
							CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR,
							           "[Audio Controls Editor] Failed to delete file %s",
							           QtUtil::ToString(fullTargetPath));
						}
					}
				}

				if (!sourceFile.copy(fullTargetPath))
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,
					           "[Audio Controls Editor] Could not copy %s to %s",
					           QtUtil::ToString(fileInfo.sourceInfo.absoluteFilePath()),
					           QtUtil::ToString(fullTargetPath));
				}
			}
		}
	}

	accept();
}
} // namespace ACE
