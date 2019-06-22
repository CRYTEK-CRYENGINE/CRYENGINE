// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileImporterUtils.h"

#include "AssetsManager.h"
#include "MainWindow.h"
#include "PropertiesWidget.h"
#include "FileImporterDialog.h"
#include "FileMonitorMiddleware.h"
#include "Common/IImpl.h"

#include <PathUtils.h>
#include <QtUtil.h>
#include <FileDialogs/SystemFileDialog.h>

#include <QDir>

namespace ACE
{
ExtensionFilterVector g_extensionFilters;
QStringList g_supportedFileTypes;

//////////////////////////////////////////////////////////////////////////
void OpenFileSelectorFromImpl(QString const& targetFolderName, bool const isLocalized)
{
	g_pIImpl->OnFileImporterOpened();
	CSystemFileDialog::RunParams runParams;
	runParams.extensionFilters = g_extensionFilters;
	runParams.title = QObject::tr("Import Audio Files");
	runParams.buttonLabel = QObject::tr("Import");
	std::vector<QString> const importedFiles = CSystemFileDialog::RunImportMultipleFiles(runParams, g_pMainWindow);
	g_pIImpl->OnFileImporterClosed();

	if (!importedFiles.empty())
	{
		FileImportInfos fileInfos;

		for (auto const& filePath : importedFiles)
		{
			QFileInfo const& fileInfo(filePath);

			if (fileInfo.isFile())
			{
				fileInfos.emplace_back(fileInfo, g_supportedFileTypes.contains(fileInfo.suffix(), Qt::CaseInsensitive));
			}
		}

		OpenFileImporter(fileInfos, targetFolderName, isLocalized, EImportTargetType::Middleware, nullptr);
	}
}

//////////////////////////////////////////////////////////////////////////
void OpenFileSelector(EImportTargetType const type, CAsset* const pAsset)
{
	g_pIImpl->OnFileImporterOpened();
	CSystemFileDialog::RunParams runParams;
	runParams.extensionFilters = g_extensionFilters;
	runParams.title = QObject::tr("Import Audio Files");
	runParams.buttonLabel = QObject::tr("Import");
	std::vector<QString> const importedFiles = CSystemFileDialog::RunImportMultipleFiles(runParams, g_pMainWindow);
	g_pIImpl->OnFileImporterClosed();

	if (!importedFiles.empty())
	{
		FileImportInfos fileInfos;

		for (auto const& filePath : importedFiles)
		{
			QFileInfo const& fileInfo(filePath);

			if (fileInfo.isFile())
			{
				fileInfos.emplace_back(fileInfo, g_supportedFileTypes.contains(fileInfo.suffix(), Qt::CaseInsensitive));
			}
		}

		OpenFileImporter(fileInfos, "", false, type, pAsset);
	}
}

//////////////////////////////////////////////////////////////////////////
void OpenFileImporter(
	FileImportInfos const& fileImportInfos,
	QString const& targetFolderName,
	bool const isLocalized,
	EImportTargetType const type,
	CAsset* const pAsset)
{
	if (type != EImportTargetType::Middleware)
	{
		g_importedItemIds.clear();
	}

	g_pFileMonitorMiddleware->Disable();
	FileImportInfos fileInfos = fileImportInfos;

	QString assetsPath = QtUtil::ToQString(PathUtil::GetGameFolder() + "/" + g_implInfo.assetsPath.c_str());
	QString localizedAssetsPath = QtUtil::ToQString(PathUtil::GetGameFolder() + "/" + g_implInfo.localizedAssetsPath.c_str());
	QString const targetFolderPath = (isLocalized ? localizedAssetsPath : assetsPath) + "/" + targetFolderName;

	QDir const targetFolder(targetFolderPath);
	QString const fullTargetPath = targetFolder.absolutePath() + "/";

	for (auto& fileInfo : fileInfos)
	{
		if (fileInfo.isTypeSupported && fileInfo.sourceInfo.isFile())
		{
			QString const targetPath = fullTargetPath + fileInfo.parentFolderName + fileInfo.sourceInfo.fileName();
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

	assetsPath = QDir(assetsPath).absolutePath();
	localizedAssetsPath = QDir(localizedAssetsPath).absolutePath();

	auto const pFileImporterDialog = new CFileImporterDialog(
		fileInfos,
		assetsPath,
		localizedAssetsPath,
		fullTargetPath,
		targetFolderName,
		isLocalized,
		type != EImportTargetType::Middleware,
		g_pMainWindow);

	g_pIImpl->OnFileImporterOpened();

	QObject::connect(pFileImporterDialog, &CFileImporterDialog::destroyed, [&]()
		{
			g_pIImpl->OnFileImporterClosed();
		});

	pFileImporterDialog->exec();

	g_pMainWindow->ReloadMiddlewareData();

	switch (type)
	{
	case EImportTargetType::SystemControls:
		{
			for (auto const id : g_importedItemIds)
			{
				Impl::IItem* const pIItem = g_pIImpl->GetItem(id);

				if (pIItem != nullptr)
				{
					g_assetsManager.CreateAndConnectImplItems(pIItem, pAsset);
				}
			}

			break;
		}
	case EImportTargetType::Connections:
		{
			auto const pTrigger = static_cast<CControl*>(pAsset);
			ControlId lastConnectedId = g_invalidControlId;

			for (auto const id : g_importedItemIds)
			{
				Impl::IItem* const pIItem = g_pIImpl->GetItem(id);

				if (pIItem != nullptr)
				{
					IConnection* pIConnection = pTrigger->GetConnection(id);

					if (pIConnection == nullptr)
					{
						pIConnection = g_pIImpl->CreateConnectionToControl(pTrigger->GetType(), pIItem);

						if (pIConnection != nullptr)
						{
							pTrigger->AddConnection(pIConnection);
							lastConnectedId = id;
						}
					}
				}
			}

			if ((lastConnectedId != g_invalidControlId) && (g_pPropertiesWidget != nullptr))
			{
				g_pPropertiesWidget->OnConnectionAdded(lastConnectedId);
			}

			break;
		}
	default:
		{
			break;
		}
	}
}
} // namespace ACE
