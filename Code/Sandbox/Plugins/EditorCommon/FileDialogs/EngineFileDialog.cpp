// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "EngineFileDialog.h"

#include "FileSystem/FileSystem_Enumerator.h"
#include "FileSystem/FileTreeModel.h"
#include "FileSystem/FileListModel.h"
#include "FileSystem/FileSortProxyModel.h"

#include "FileDialogs/FileNameLineEdit.h"
#include "FileDialogs/Internal/SelectExtensionDialog.h"
#include "FileDialogs/Internal/FilePreviewContainer.h"

#include "QSearchBox.h"
#include "Controls/QuestionDialog.h"
#include "Controls/QMenuComboBox.h"

#include "FilePopupMenu.h"
#include "CryIcon.h"
#include "QAdvancedItemDelegate.h"
#include "FilePathUtil.h"
#include "EditorStyleHelper.h"

#include <CrySystem/File/ICryPak.h>

#include <QScopedPointer>
#include <QStandardItemModel>
#include <QItemSelectionModel>
#include <QFileInfo>
#include <QButtonGroup>
#include <QTableView>
#include <QListView>
#include <QAdvancedTreeView.h>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QSplitter>
#include <QHeaderView>
#include <QLineEdit>
#include <QDir>
#include <QKeyEvent>
#include <QInputDialog>

#include <stack>

struct CEngineFileDialog::Implementation
{
	Implementation(CEngineFileDialog* pDialog, OpenParams& openParams)
		: m_dialog(pDialog)
		, m_acceptFileCallback(openParams.acceptFileCallback ? openParams.acceptFileCallback : [](const QString&){ return true; })
		, m_mode(openParams.mode)
		, m_defaultExtension(openParams.defaultExtension)
		, m_allowAnyFile(false)
	{
		ValidateParams(openParams);
		m_models.Setup(openParams.fileFilterFunc, openParams.baseDirectory, openParams.extensionFilters, openParams.mode, pDialog);
		m_ui.Setup(m_models, openParams.mode, openParams.allowCreateFolder, openParams.buttonLabel, pDialog); // Creates the views
		SetupConnections();// Sets up models of the views and creates connections
		SetupExtensionFilters(openParams.extensionFilters);

		//Start in the project folder in the worst case scenario
		SetupCurrentFileDirectory(openParams.initialFile, openParams.initialDir);

		if (m_ui.pPreviewContainer && m_ui.pPreviewContainer->GetToggleButton())
			m_ui.pPreviewContainer->GetToggleButton()->setChecked(true);

		AddProperties();
	}

	void ValidateParams(OpenParams& openParams)
	{
		//ensure all the paths are game paths
		openParams.baseDirectory = PathUtil::ToGamePath(openParams.baseDirectory);
		openParams.initialDir = PathUtil::ToGamePath(openParams.initialDir);
		openParams.initialFile = PathUtil::ToGamePath(openParams.initialFile);
	}

	struct Models
	{
		QVector<FileSystem::SFileFilter::FileFilterCallback> defaultFileFilterCallbacks;
		FileSystem::SFileFilter                              fileFilter;
		CFileTreeModel*      fileTree;
		CFileSortProxyModel* fileSortProxy;
		CFileTreeModel*      directories;
		CFileSortProxyModel* directorySortProxy;
		bool                 m_selectDirectory;
		FileSystem::CEnumerator*  m_enumerator;

		CFileListModel*		pHistoryModel;
		CFileListModel*		pFavoritesModel;

		Models()
			: fileTree(nullptr)
			, fileSortProxy(nullptr)
			, directories(nullptr)
			, directorySortProxy(nullptr)
			, m_selectDirectory(false)
			, m_enumerator(GetIEditor()->GetFileSystemEnumerator())
		{}

		void Setup(const FileFilterCallback& fileFilterFunc, const QString& baseDirectory, const ExtensionFilterVector& extensionFilters, CEngineFileDialog::Mode mode, QObject* parent)
		{
			m_selectDirectory = (mode == CEngineFileDialog::SelectDirectory);
			if (!m_selectDirectory && !extensionFilters.empty())
			{
				fileFilter.fileExtensions = extensionFilters.front().GetExtensions().toSet();
			}
			auto directoryOnlyFilter = [](const FileSystem::SnapshotPtr&, const FileSystem::FilePtr&)
			{
				return false;
			};

			if (m_selectDirectory)
			{
				defaultFileFilterCallbacks << directoryOnlyFilter;
			}
			if (fileFilterFunc)
			{
				defaultFileFilterCallbacks << fileFilterFunc;
			}
			fileFilter.fileFilterCallbacks = defaultFileFilterCallbacks;
			//TODO : restore base dir functionnality
			/*if (!baseDirectory.isEmpty())
			{
				fileFilter.directories << baseDirectory;
			}*/

			fileTree = new CFileTreeModel(*m_enumerator, fileFilter, parent);

			fileSortProxy = new CFileSortProxyModel(parent);
			fileSortProxy->setSourceModel(fileTree);

			if (!m_selectDirectory)
			{
				auto directoryFilter = fileFilter;
				directoryFilter.fileFilterCallbacks << directoryOnlyFilter;
				directories = new CFileTreeModel(*m_enumerator, directoryFilter, parent);

				directorySortProxy = new CFileSortProxyModel(parent);
				directorySortProxy->setSourceModel(directories);
			}
			else
			{
				directories = fileTree;
				directorySortProxy = fileSortProxy;
			}

			pHistoryModel = new CFileListModel(*m_enumerator);
			pFavoritesModel = new CFileListModel(*m_enumerator);
		}

		bool IsFileIndexDirectory(const QModelIndex& fileSortIndex) const
		{
			auto fileIndex = fileSortProxy->mapToSource(fileSortIndex);
			return fileTree->IsDirectory(fileIndex);
		}

		QString GetPathFromFileIndex(const QModelIndex& fileSortIndex) const
		{
			auto fileIndex = fileSortProxy->mapToSource(fileSortIndex);
			auto path = fileTree->GetPathFromIndex(fileIndex);
			return path;
		}

		QString GetFileNameFromFileIndex(const QModelIndex& fileSortIndex) const
		{
			auto fileIndex = fileSortProxy->mapToSource(fileSortIndex);
			auto name = fileTree->GetFileNameFromIndex(fileIndex);
			return name;
		}

		QModelIndex GetDirectoryIndexByPath(const QString& path) const
		{
			auto directoryIndex = directories->GetIndexByPath(path);
			auto directorySortIndex = directorySortProxy->mapFromSource(directoryIndex);
			return directorySortIndex;
		}

		QString GetPathFromDirectoryIndex(const QModelIndex& directorySortIndex) const
		{
			auto directoryIndex = directorySortProxy->mapToSource(directorySortIndex);
			auto path = directories->GetPathFromIndex(directoryIndex);
			return path;
		}

		QModelIndex ConvertFileToDirectoryIndex(const QModelIndex& fileSortIndex) const
		{
			auto fileIndex = fileSortProxy->mapToSource(fileSortIndex);
			auto filePath = fileTree->GetPathFromIndex(fileIndex);
			auto directorySortIndex = GetDirectoryIndexByPath(filePath);
			return directorySortIndex;
		}

		QModelIndex ConvertDirectoryToFileIndex(const QModelIndex& directorySortIndex) const
		{
			auto directoryPath = GetPathFromDirectoryIndex(directorySortIndex);
			auto fileIndex = fileTree->GetIndexByPath(directoryPath);
			auto sortIndex = fileSortProxy->mapFromSource(fileIndex);
			return sortIndex;
		}

		void SetFilterExtensions(const QStringList& extensions)
		{
			fileFilter.fileExtensions = extensions.toSet();
			UpdateFilter();
		}

		void UpdateFilter()
		{
			fileTree->SetFilter(fileFilter);
			if (!m_selectDirectory)
			{
				FileSystem::SFileFilter directoryFilter = fileFilter;
				directoryFilter.fileFilterCallbacks.push_back([](const FileSystem::SnapshotPtr&, const FileSystem::FilePtr&)
				{
					return false;
				});
				directories->SetFilter(directoryFilter);
			}
		}
	};

	void OpenDirectoryIndex(const QModelIndex& directoryIndex)
	{
		ExpandToDirectory(directoryIndex);
		m_ui.pDirectoryTreeView->setCurrentIndex(directoryIndex);
	}

	void OpenFavoritesIndex(const QModelIndex& favoritesIndex)
	{
		auto path = m_models.pFavoritesModel->GetPathFromIndex(favoritesIndex);
		auto fileIndex = m_models.fileTree->GetIndexByPath(path);
		if (fileIndex.isValid())
		{
			if (m_models.fileTree->IsDirectory(fileIndex))
			{
				const auto directoryIndex = m_models.GetDirectoryIndexByPath(path);
				OpenDirectoryIndex(directoryIndex);
				m_ui.pFilenameLineEdit->Clear();
			}
			else
			{
				const auto directoryIndex = m_models.GetDirectoryIndexByPath(m_models.fileTree->GetPathFromIndex(fileIndex.parent()));
				OpenDirectoryIndex(directoryIndex);

				//select in file view
				const auto fileSortIndex = m_models.fileSortProxy->mapFromSource(fileIndex);
				m_ui.pFileDetailsView->selectionModel()->select(fileSortIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
			}
		}
	}

	void OpenFileIndex(const QModelIndex& fileIndex)
	{
		const auto directoryIndex = m_models.ConvertFileToDirectoryIndex(fileIndex);
		if (directoryIndex.isValid())
		{
			OpenDirectoryIndex(directoryIndex);
		}
		else if (!IsSelectingDirectory() && !fileIndex.model()->hasChildren(fileIndex))
		{
			AcceptFilesFromInput();
		}
	}

	bool IsSelectingDirectory() const
	{
		return m_mode == CEngineFileDialog::SelectDirectory;
	}

	bool IsOpeningMode() const
	{
		return m_mode == CEngineFileDialog::OpenFile || m_mode == CEngineFileDialog::OpenMultipleFiles;
	}

	bool IsAsteriskFilterCurrentlySelected()
	{
		return  GetCurrentFileExtensions().first().contains("*");
	}

	void AcceptFilesFromInput()
	{
		m_selectedFiles.clear();
		auto selectedDirectoryIndex = m_ui.pDirectoryTreeView->currentIndex();
		auto selectedDirectoryPath = m_models.GetPathFromDirectoryIndex(selectedDirectoryIndex);
		auto selectedDirectoryDir = QDir(selectedDirectoryPath.isEmpty() ? "/" : selectedDirectoryPath);

		auto filenames = m_ui.pFilenameLineEdit->GetFileNames();
		if (filenames.isEmpty() && IsSelectingDirectory())
		{
			filenames << selectedDirectoryPath;
		}

		if (filenames.isEmpty())
			return;

		const bool enforceExtension = !IsSelectingDirectory() && !IsAsteriskFilterCurrentlySelected(); // enforce files to have an extension

		for (auto& filename : filenames)
		{
			if (enforceExtension)
			{
				auto validName = EnforceValidExtension(filename, selectedDirectoryPath);
				if (validName.isEmpty())
				{
					m_ui.pFilenameLineEdit->SetFileNames(filenames);
					return; // cancelled
				}
				filename = validName;
			}
			auto absolutePath = selectedDirectoryDir.absoluteFilePath(filename);
			if (!absolutePath.isEmpty() && absolutePath.startsWith("/"))
			{
				absolutePath = absolutePath.mid(1); // remove slash in beginning
			}
			if (!m_acceptFileCallback(absolutePath))
			{
				return; // input not accepted
			}
			string gamePath = PathUtil::MakeGamePath(string(absolutePath.toUtf8().constData()));
			m_selectedFiles.push_back(gamePath.c_str());
		}

		//unique entry in history
		//must make sure we do not add the root to history
		if(!selectedDirectoryPath.isEmpty() && !m_history.contains(selectedDirectoryPath))
			m_history.push_back(selectedDirectoryPath);

		m_dialog->accept();
	}

	void ExpandToDirectory(const QModelIndex& directoryIndex)
	{
		if (directoryIndex == m_ui.pDirectoryTreeView->rootIndex())
		{
			return;
		}
		auto expandIndex = directoryIndex.parent();
		while (expandIndex.isValid() && expandIndex != m_ui.pDirectoryTreeView->rootIndex())
		{
			m_ui.pDirectoryTreeView->expand(expandIndex);
			expandIndex = expandIndex.parent();
		}
	}

	void OnDirectoryChanged(const QModelIndex& newIndex, const QModelIndex& oldIndex)
	{
		auto fileIndex = m_models.ConvertDirectoryToFileIndex(newIndex);
		m_ui.pFileDetailsView->setRootIndex(fileIndex);
		m_ui.pFileDetailsView->clearSelection();

		m_navigationHistory.Store(oldIndex);

		auto indexPath = m_models.GetPathFromDirectoryIndex(newIndex);
		ResetLookInComboBox(indexPath);

		m_ui.pCreateFolderButton->setEnabled(newIndex.isValid());
	}

	void OnFileSelectionChanged(const QModelIndexList& currentSelection)
	{
		QModelIndexList fileSelection;
		std::remove_copy_if(currentSelection.cbegin(), currentSelection.cend(), std::back_inserter(fileSelection), [&](const QModelIndex& idx)
		{
			return !IsSelectingDirectory() && m_models.IsFileIndexDirectory(idx);
		});

		//do not clear the input field when selection is not relevant (ex: selected directory during file select dialog)
		if (fileSelection.isEmpty())
			return;

		auto fileNames = GetSelectionFileNames(fileSelection);
		m_ui.pFilenameLineEdit->SetFileNames(fileNames);

		UpdatePreviewWidgetFromCurrentSelection();
	}

	QStringList GetSelectionFileNames(const QModelIndexList& selection)
	{
		QStringList result;
		result.reserve(selection.size());
		for (const auto& fileIndex : selection)
		{
			auto fileName = m_models.GetFileNameFromFileIndex(fileIndex);
			result << fileName;
		}
		return result;
	}

	void SetupConnections()
	{
		m_ui.pLookInComboBox->signalCurrentIndexChanged.Connect([this](int index)
		{
			auto path = m_ui.pLookInComboBox->GetItem(index);
			auto directoryIndex = m_models.GetDirectoryIndexByPath(path);
			OpenDirectoryIndex(directoryIndex);
		});

		connect(m_ui.pBackButton, &QPushButton::clicked, [this]
		{
			auto directoryIndex = m_navigationHistory.GetLast();
			m_navigationHistory.bRememberInHistory = false;
			OpenDirectoryIndex(directoryIndex);
		});

		connect(m_ui.pUpButton, &QPushButton::clicked, [this]
		{
			auto currentDirectoryIndex = m_ui.pDirectoryTreeView->currentIndex();
			auto parentDirectoryIndex = currentDirectoryIndex.parent();
			OpenDirectoryIndex(parentDirectoryIndex);
		});

		connect(m_ui.pCreateFolderButton, &QAbstractButton::clicked, [this]
		{
			auto currentDirectoryIndex = m_ui.pDirectoryTreeView->currentIndex();
			auto currentDirectoryPath = m_models.GetPathFromDirectoryIndex(currentDirectoryIndex);
			if (!currentDirectoryPath.isEmpty() && currentDirectoryPath[0] == '/')
			{
			  currentDirectoryPath = currentDirectoryPath.mid(1);
			}
			bool ok = false;
			auto folderName = QInputDialog::getText(m_dialog, tr("Create Folder"), tr("Directory Name:"), QLineEdit::Normal, tr("New Folder"), &ok);
			if (!ok || folderName.isEmpty() || folderName.startsWith('/') || folderName.contains(".."))
			{
			  return; // canceled
			}

			auto enumerator = GetIEditor()->GetFileSystemEnumerator();

			auto currentDir = QDir(enumerator->GetProjectRootPath());
			bool success = currentDir.mkpath(currentDirectoryPath.isEmpty() ? folderName : currentDirectoryPath + '/' + folderName);
			if (!success)
			{
			  CQuestionDialog::SCritical(tr("Error"), tr("Failed to create folder"));
			  return; // failed to create
			}
		});

		connect(m_ui.pDirectoryTreeView->selectionModel(), &QItemSelectionModel::currentChanged, [this](const QModelIndex& newIndex, const QModelIndex& oldIndex)
		{
			OnDirectoryChanged(newIndex, oldIndex);
			m_ui.pBackButton->setEnabled(m_navigationHistory.isFilled());
			m_ui.pUpButton->setEnabled(newIndex.isValid());
		});

		connect(m_ui.pHistoryView->selectionModel(), &QItemSelectionModel::selectionChanged, [this]
		{
			auto selection = m_ui.pHistoryView->selectionModel()->selectedRows();
			if (selection.size() == 1)
			{
				const auto path = m_models.pHistoryModel->GetPathFromIndex(selection.first());
				const auto directoryIndex = m_models.GetDirectoryIndexByPath(path);
				if (directoryIndex.isValid())
				{
					OpenDirectoryIndex(directoryIndex);
				}
			}
		});

		auto pConn = std::make_shared<QMetaObject::Connection>();
		*pConn = connect(
		  m_ui.pDirectoryTreeView->model(),
		  &QAbstractItemModel::layoutChanged,
		  [this, pConn]()
		{
			m_ui.pDirectoryTreeView->scrollTo(m_ui.pDirectoryTreeView->currentIndex(), QTreeView::PositionAtCenter);

			// We only want to center the view to the initially set current index.
			QObject::disconnect(*pConn);
		});

		connect(m_ui.pOpenButton, &QPushButton::clicked, [this]
		{
			AcceptFilesFromInput();
		});

		connect(m_ui.pCancelButton, &QPushButton::clicked, m_dialog, &CEngineFileDialog::reject);

		m_ui.pFileDetailsView->SetImplementation(this);

		//double click selects file
		connect(m_ui.pFileDetailsView, &QTreeView::activated, [this](const QModelIndex& fileIndex)
		{
			OpenFileIndex(fileIndex);
		});

		connect(m_ui.pFileDetailsView->selectionModel(), &QItemSelectionModel::selectionChanged, [this]
		{
			auto selectedRows = m_ui.pFileDetailsView->selectionModel()->selectedRows();
			OnFileSelectionChanged(selectedRows);
		});

		connect(m_ui.pFileDetailsView, &QTreeView::customContextMenuRequested, [this]
		{
			auto currentSelection = m_ui.pFileDetailsView->selectionModel()->currentIndex();
			SpawnContextMenu(currentSelection, m_ui.pFileDetailsView);
		});

		connect(m_ui.pDirectoryTreeView, &QTreeView::customContextMenuRequested, [this]
		{
			auto currentSelection = m_ui.pDirectoryTreeView->selectionModel()->currentIndex();
			SpawnContextMenu(m_models.ConvertDirectoryToFileIndex(currentSelection), m_ui.pDirectoryTreeView);
		});

		connect(m_ui.pPreviewContainer, &CFilePreviewContainer::Enabled, [this]
		{
			UpdatePreviewWidgetFromCurrentSelection();
		});

		//favorites view
		connect(m_ui.pFavoritesView, &QTreeView::activated, [this](const QModelIndex& favIndex)
		{
			const auto path = m_models.pFavoritesModel->GetPathFromIndex(favIndex);
			const auto fileIndex = m_models.fileTree->GetIndexByPath(path);
			if (fileIndex.isValid())
			{
				if (m_models.fileTree->IsDirectory(fileIndex))
				{
					const auto directoryIndex = m_models.GetDirectoryIndexByPath(path);
					if (m_ui.pDirectoryTreeView->currentIndex() == directoryIndex) //do not do anything if directory is already selected
						return;
				}

				//This also works for directories because if the line edit text is empty it will look at the selected directory in the directory tree and use that
				OpenFavoritesIndex(favIndex);
				AcceptFilesFromInput();
			}
		});

		connect(m_ui.pFavoritesView->selectionModel(), &QItemSelectionModel::selectionChanged, [this]
		{
			auto selection = m_ui.pFavoritesView->selectionModel()->selectedRows();
			if(selection.size() == 1)
				OpenFavoritesIndex(selection.first());
		});

		connect(m_ui.pFavoritesView, &QTreeView::customContextMenuRequested, [this](const QPoint &pos)
		{
			auto currentSelection = m_ui.pFavoritesView->selectionModel()->currentIndex();

			auto menu = new QMenu();
			auto action = menu->addAction(tr("Remove From Favorites"));
			connect(action, &QAction::triggered, [&]() {
				m_models.pFavoritesModel->RemoveEntry(currentSelection);
			});

			menu->exec(QCursor::pos());
		});
	}

	struct History
	{
		History()
			: bRememberInHistory(true)
		{}

		void Store(const QModelIndex& index)
		{
			if (bRememberInHistory)
			{
				paths.push(index);
			}
			bRememberInHistory = true;
		}

		QModelIndex GetLast()
		{
			if (isFilled())
			{
				QModelIndex index = paths.top();
				paths.pop();
				return index;
			}
			return QModelIndex();
		}

		bool isFilled() const
		{
			while (!paths.empty())
			{
				return true;
			}
			return false;
		}

		mutable std::stack<QPersistentModelIndex> paths;
		bool bRememberInHistory;
	};

	void ResetLookInComboBox(const QString& currentPath)
	{
		auto rootPath = QStringLiteral("/");
		auto pLookInComboBox = m_ui.pLookInComboBox;
		pLookInComboBox->Clear();
		auto path = currentPath;
		while (true)
		{
			pLookInComboBox->AddItem(path);
			if (path == rootPath || path.isEmpty())
			{
				return;
			}
			path = QFileInfo(path).path();
		}
	}

	bool IsWildcardExtension(const QString& ext) const
	{
		//internally extensions are always stored without the *. prefix
		return ext == "*";
	}

	QSet<QString> FindPossibleExtensionsForFilename(const QString& filename, const QString& dirPath) const
	{
		QString filenameFilter = filename;

		auto index = filename.lastIndexOf('.');
		if (index > 0)
			filenameFilter = filename.left(index + 1);
		else
			filenameFilter.append('.');

		auto snapshot = GetIEditor()->GetFileSystemEnumerator()->GetCurrentSnapshot();

		FileSystem::SFileFilter filter;
		filter.directories.append(dirPath);
		filter.recursiveSubdirectories = false;
		filter.fileFilterCallbacks.push_back([&](const FileSystem::SnapshotPtr&, const FileSystem::FilePtr& filePtr)
		{
			return filePtr->keyName.startsWith(filenameFilter);
		});

		auto files = snapshot->FindFiles(filter);

		QSet<QString> possibleExts;

		//known issue, extension here is the last part of the extension and will not handle double extensions well
		for (auto& file : files)
		{
			possibleExts.insert(file->extension);
		}

		return possibleExts;
	}

	bool IsMatchingFilters(const QString& enginePath)
	{
		//TODO : this needs to handle base directory

		auto snapshotPtr = m_models.m_enumerator->GetCurrentSnapshot();
		auto dirPtr = snapshotPtr->GetDirectoryByEnginePath(enginePath);
		
		if (m_allowAnyFile)
			return true;
		else if (dirPtr)
			return true;
		else if (IsSelectingDirectory())
			return false; //because dirPtr == nullptr
		else
		{
			foreach(const auto & extension, m_validExtensions)
			{
				if (!IsWildcardExtension(extension) && enginePath.endsWith(L'.' + extension, Qt::CaseInsensitive))
				{
					auto filePtr = snapshotPtr->GetFileByEnginePath(enginePath);
					//matches extension, now check if matches callbacks
					for (auto& callback : m_models.defaultFileFilterCallbacks)
					{
						if (!callback(snapshotPtr, filePtr))
							return false;
					}
					return true;
				}
			}
			return false;
		}
		
	}

	QString EnforceValidExtension(const QString& filename, const QString& dirPath) const
	{
		if (filename.contains('.'))
		{
			foreach(const auto & extension, m_validExtensions)
			{
				if (IsWildcardExtension(extension) || filename.endsWith(L'.' + extension, Qt::CaseInsensitive))
				{
					return filename;
				}
			}
		}

		if (IsOpeningMode())
		{
			//Choose from all the existing files on disk that match the filename and have valid extension
			auto possibleExts = FindPossibleExtensionsForFilename(filename, dirPath);
			possibleExts = possibleExts.intersect(m_validExtensions);
			if (possibleExts.isEmpty())
			{
				return QString(); // not file to open with valid extension and corresponding filename
			}

			QScopedPointer<CSelectExtensionDialog, QScopedPointerDeleteLater> selectDialog(new CSelectExtensionDialog(m_dialog));
			selectDialog->SetFilename(filename);
			selectDialog->SetExtensions(possibleExts.toList());

			if (!selectDialog->exec())
			{
				return QString(); // aborted
			}
			return selectDialog->GetFilenameWithExtension();
		}
		else //saving mode, must enforce extension
		{
			QScopedPointer<CSelectExtensionDialog, QScopedPointerDeleteLater> selectDialog(new CSelectExtensionDialog(m_dialog));
			selectDialog->SetFilename(filename);
			selectDialog->SetExtensions(m_validExtensions.toList());

			if (selectDialog->IsValid())//This will be invalid if m_validExtensions only contains wildcards
			{
				auto currentExts = GetCurrentFileExtensions();

				if (!currentExts.isEmpty())//security as we have been having many crashes here
				{
					if (!m_defaultExtension.isEmpty() && currentExts.contains(m_defaultExtension))
						selectDialog->SetPreferred(m_defaultExtension);
					else if (!IsWildcardExtension(currentExts.first()))
						selectDialog->SetPreferred(currentExts.first());
				}
			}

			if (!selectDialog->exec())
			{
				return QString(); // aborted
			}
			return selectDialog->GetFilenameWithExtension();
		}
	}

	void SetupExtensionFilters(const ExtensionFilterVector& extensionFilters)
	{
		if (IsSelectingDirectory())
			return;

		CRY_ASSERT(m_defaultExtension.isEmpty() || !IsWildcardExtension(m_defaultExtension));//default ext must be a valid extension

		if (extensionFilters.isEmpty())
		{
			QStringList validExtensions;
			validExtensions.append(QStringLiteral("*"));
			m_ui.pFiletypeComboBox->AddItem(tr("All Files"), validExtensions);

			m_validExtensions.insert(m_defaultExtension);
			m_allowAnyFile = true;
		}
		else
		{
			foreach(const CExtensionFilter &extensionFilter, extensionFilters)
			{
				auto description = extensionFilter.GetDescription();
				auto extensions = extensionFilter.GetExtensions();
				CRY_ASSERT(extensions.count() >= 1); // filter must not be empty
				m_ui.pFiletypeComboBox->AddItem(description, extensions);
				m_validExtensions.unite(extensions.toSet());

				if (extensions.contains("*"))
					m_allowAnyFile = true;
			}

			CRY_ASSERT(m_defaultExtension.isEmpty() || m_validExtensions.contains(m_defaultExtension));
		}

		m_ui.pFiletypeComboBox->setEnabled(extensionFilters.size() > 1);

		// Preselect the default extension if provided.
		if (!m_defaultExtension.isEmpty())
		{
			m_ui.pFiletypeComboBox->SetCheckedByData(m_defaultExtension);
		}
	}

	QStringList GetCurrentFileExtensions() const
	{
		auto menuComboBox = m_ui.pFiletypeComboBox;
		int index = menuComboBox->GetCheckedItem();
		return menuComboBox->GetData(index).toStringList();
	}

	void SetupCurrentFileDirectory(const QString& initialFile, const QString& initialDir)
	{
		auto realInitialFile = initialFile;
		auto realInitialDir = initialDir;
		if (!realInitialFile.isEmpty())
		{
			realInitialDir = QFileInfo(realInitialFile).path();
		}
		auto initialDirectoryIndex = m_models.directories->GetIndexByPath(realInitialDir);
		auto initialDirectorySortIndex = m_models.directorySortProxy->mapFromSource(initialDirectoryIndex);
		if (!initialDirectoryIndex.isValid())
		{
			initialDirectorySortIndex = m_ui.pDirectoryTreeView->rootIndex();
		}
		OpenDirectoryIndex(initialDirectorySortIndex);

		auto initialFileIndex = m_models.fileTree->GetIndexByPath(realInitialFile);
		if (initialFileIndex.isValid())
		{
			auto proxyIndex = m_models.fileSortProxy->mapFromSource(initialFileIndex);
			m_ui.pFileDetailsView->setCurrentIndex(proxyIndex);
		}
		else if (!realInitialFile.isEmpty())
		{
			m_ui.pFilenameLineEdit->SetFileNames(QStringList() << QFileInfo(realInitialFile).fileName());
		}
	}

	void SpawnContextMenu(const QModelIndex& selectedIndex, QWidget* spawnParent) const
	{
		if (!selectedIndex.isValid())
		{
			return;
		}

		auto filePath = m_models.GetPathFromFileIndex(selectedIndex);
		QScopedPointer<CFilePopupMenu, QScopedPointerDeleteLater> popupMenu(new CFilePopupMenu(QFileInfo(filePath), spawnParent));

		popupMenu->addSeparator();
		popupMenu->addAction(new CFilePopupMenu::SFilePopupMenuAction("Add to favorites", nullptr, [&]() {
			m_models.pFavoritesModel->AddEntry(filePath);
		}));

		m_dialog->PopupMenuToBePopulated(popupMenu.data());
		popupMenu->exec(QCursor::pos());
	}

	bool HandleFileDetailsKeyPressEvent(QKeyEvent* event)
	{
		switch (event->key())
		{
		case Qt::Key_Backspace:
			m_ui.pUpButton->click();
			return true;

		case Qt::Key_Left:
			if (event->modifiers() != Qt::AltModifier)
			{
				return false;
			}
			m_ui.pBackButton->click();
			return true;

		default:
			return false;
		}
	}

	void UpdatePreviewWidgetFromCurrentSelection()
	{
		auto selectionModel = m_ui.pFileDetailsView->selectionModel();
		auto selectedRows = selectionModel->selectedRows();
		auto it = std::find_if(selectedRows.cbegin(), selectedRows.cend(), [&](const QModelIndex& idx)
		{
			if (m_models.IsFileIndexDirectory(idx))
			{
			  return false; // directories cannot be viewed
			}
			const auto fileName = m_models.GetFileNameFromFileIndex(idx);
			return CFilePreviewContainer::IsPreviewablePath(fileName);
		});

		QString filePath;
		if (it != selectedRows.cend())
		{
			filePath = m_models.GetPathFromFileIndex(*it);
			if (!filePath.isEmpty() && filePath.startsWith("/"))
			{
				filePath.remove(0, 1);
			}
		}
		m_ui.pPreviewContainer->SetPath(filePath);
	}

	struct Ui
	{
		class CFileTreeView : public QAdvancedTreeView
		{
		public:
			CFileTreeView(QWidget* parent)
				: QAdvancedTreeView(QAdvancedTreeView::Behavior(QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset), parent)
				, m_impl(nullptr)
			{
				setDropIndicatorShown(false);
			}

			void SetImplementation(Implementation* impl)
			{
				m_impl = impl;
			}

			// QWidget interface
		protected:
			virtual void keyPressEvent(QKeyEvent* e) override
			{
				if (!m_impl || !m_impl->HandleFileDetailsKeyPressEvent(e))
					QAdvancedTreeView::keyPressEvent(e);
				e->accept();
			}

		private:
			Implementation* m_impl;
		};

		void Setup(Models& models, CEngineFileDialog::Mode mode, bool allowCreateFolder, const QString& buttonLabel, QWidget* parent)
		{
			auto selectionMode = (mode == CEngineFileDialog::OpenMultipleFiles) ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection;

			switch (mode)
			{
			case CEngineFileDialog::SelectDirectory:
				parent->setWindowTitle(tr("Select Directory"));
				break;
			case CEngineFileDialog::OpenFile:
				parent->setWindowTitle(tr("Select File"));
				break;
			case CEngineFileDialog::OpenMultipleFiles:
				parent->setWindowTitle(tr("Select Files"));
				break;
			case CEngineFileDialog::SaveFile:
				parent->setWindowTitle(tr("Save File"));
				break;
			default:
				CRY_ASSERT(0);// bad value
				break;
			}

			const bool selectDirectory = (mode == CEngineFileDialog::SelectDirectory);

			auto backButton = new QToolButton(parent);
			backButton->setEnabled(false);
			backButton->setText(tr("Back"));
			backButton->setIcon(CryIcon(":/icons/General/Arrow_Left.ico"));

			auto upButton = new QToolButton(parent);
			upButton->setText(tr("Up"));
			upButton->setIcon(CryIcon(":/icons/General/Arrow_Up.ico"));

			auto lookInComboBox = new QMenuComboBox(parent);
			QSizePolicy lookInSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			lookInSizePolicy.setHorizontalStretch(3);
			lookInComboBox->setSizePolicy(lookInSizePolicy);

			auto searchBox = new QSearchBox(parent);
			auto searchFunction = [&models](const QString& text)
			{
				bool isSearchFilter = (!text.isEmpty());
				models.fileFilter.fileFilterCallbacks = models.defaultFileFilterCallbacks;
				if (isSearchFilter)
				{
					models.fileFilter.fileFilterCallbacks <<[text](const FileSystem::SnapshotPtr&, const FileSystem::FilePtr& file)
						  {
						  return file->keyName.contains(text, Qt::CaseInsensitive);
					  };
				}
				models.UpdateFilter();
			};
			searchBox->SetSearchFunction(std::function<void(const QString&)>(searchFunction));
			searchBox->EnableContinuousSearch(true);
			searchBox->setMinimumWidth(150);
			QSizePolicy searchBoxSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			searchBoxSizePolicy.setHorizontalStretch(1);
			searchBox->setSizePolicy(searchBoxSizePolicy);

			auto previewContainer = new CFilePreviewContainer(parent);
			auto showPreviewButton = previewContainer->GetToggleButton();
			if (selectDirectory)
			{
				showPreviewButton->hide(); // for directories it makes not sense to preview
			}

			auto navigationLayout = new QHBoxLayout();
			navigationLayout->setSpacing(5);
			navigationLayout->addWidget(backButton);
			navigationLayout->addWidget(upButton);
			navigationLayout->addWidget(lookInComboBox);
			navigationLayout->addWidget(searchBox);
			navigationLayout->addWidget(showPreviewButton);

			auto createFolderButton = new QToolButton(parent);
			if (allowCreateFolder)
			{
				createFolderButton->setText(tr("Create Folder"));
				createFolderButton->setIcon(CryIcon(":/icons/General/Folder_Add.ico"));
				createFolderButton->setEnabled(true);
				navigationLayout->addWidget(createFolderButton);
			}
			else
			{
				createFolderButton->hide();
			}

			auto directoryTreeView = new QAdvancedTreeView(QAdvancedTreeView::Behavior(QAdvancedTreeView::PreserveExpandedAfterReset | QAdvancedTreeView::PreserveSelectionAfterReset), parent);
			// Use QAdvancedItemDelegate so we can support custom tinting icons on selection
			directoryTreeView->setItemDelegate(new QAdvancedItemDelegate(directoryTreeView));
			directoryTreeView->setUniformRowHeights(true);
			directoryTreeView->setHeaderHidden(true);
			directoryTreeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
			directoryTreeView->setSortingEnabled(true);
			directoryTreeView->sortByColumn(0, Qt::AscendingOrder);
			directoryTreeView->setModel(models.directorySortProxy);
			directoryTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
			auto directoryColumnCount = models.directorySortProxy->columnCount();
			for (int column = 1; column < directoryColumnCount; ++column)
			{
				directoryTreeView->setColumnHidden(column, true); // hide all but first column
			}

			auto fileDetailsView = new CFileTreeView(parent);
			// Use QAdvancedItemDelegate so we can support custom tinting icons on selection
			fileDetailsView->setItemDelegate(new QAdvancedItemDelegate(fileDetailsView));
			fileDetailsView->setSortingEnabled(true);
			fileDetailsView->setUniformRowHeights(true);
			fileDetailsView->setAllColumnsShowFocus(true);
			fileDetailsView->setRootIsDecorated(false);
			fileDetailsView->setItemsExpandable(false);
			fileDetailsView->setDragEnabled(true);
			fileDetailsView->setDragDropMode(QAbstractItemView::DragOnly);
			fileDetailsView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
			fileDetailsView->setSelectionBehavior(QAbstractItemView::SelectRows);
			fileDetailsView->setSelectionMode(selectionMode);
			fileDetailsView->setContextMenuPolicy(Qt::CustomContextMenu);
			fileDetailsView->sortByColumn(0, Qt::AscendingOrder);
			fileDetailsView->setModel(models.fileSortProxy);

			auto fontMetrics = parent->fontMetrics();
			auto fileHeader = fileDetailsView->header();
			fileHeader->setStretchLastSection(false);
			fileHeader->resizeSection(0, fontMetrics.width(QStringLiteral("wwwwwwwwwwwwwwwwwwwwwwwwww")));
			fileHeader->resizeSection(1, fontMetrics.width(QStringLiteral("88-88-8888 88-88PM")));
			fileHeader->resizeSection(2, fontMetrics.width(QStringLiteral("Unknown Directory")));
			fileHeader->resizeSection(3, fontMetrics.width(QStringLiteral("888.88 WYWES")));
			fileHeader->resizeSection(4, fontMetrics.width(QStringLiteral("wwwwwwwwwwwwwwwwwwwwwwwwww (wwwwwwwwwwwwwwwwwwwwwwwwww)")));
			if (selectDirectory)
			{
				fileDetailsView->setColumnHidden(1, true);
				fileDetailsView->setColumnHidden(2, true);
				fileDetailsView->setColumnHidden(3, true);
			}

			auto enumerator = GetIEditor()->GetFileSystemEnumerator();
			//history
			pHistoryView = new CFileTreeView(parent);
			pHistoryView->setUniformRowHeights(true);
			pHistoryView->setModel(models.pHistoryModel);
			pHistoryView->setHeaderHidden(true);
			pHistoryView->setRootIndex(QModelIndex());
			pHistoryView->setEditTriggers(QAbstractItemView::NoEditTriggers);
			pHistoryView->setColumnHidden(1, true);
			pHistoryView->setColumnHidden(2, true);
			pHistoryView->setColumnHidden(3, true);


			//favorites
			pFavoritesView = new CFileTreeView(parent);
			pFavoritesView->setUniformRowHeights(true);
			pFavoritesView->setModel(models.pFavoritesModel);
			pFavoritesView->setHeaderHidden(true);
			pFavoritesView->setRootIndex(QModelIndex());
			pFavoritesView->setColumnHidden(1, true);
			pFavoritesView->setColumnHidden(2, true);
			pFavoritesView->setColumnHidden(3, true);
			pFavoritesView->setEditTriggers(QAbstractItemView::NoEditTriggers);
			pFavoritesView->setDragEnabled(true);
			pFavoritesView->setDragDropMode(QAbstractItemView::DragDrop);
			pFavoritesView->setContextMenuPolicy(Qt::CustomContextMenu);

			//left panel
			CryIconColorMap colorMap = CryIconColorMap();
			colorMap[QIcon::Normal] = GetStyleHelper()->contrastingIconTint();
			colorMap[QIcon::Selected] = GetStyleHelper()->contrastingIconTint();

			auto tab = new QTabWidget();
			tab->addTab(directoryTreeView, "");
			tab->setTabToolTip(0, "Directories");
			tab->setTabIcon(0, CryIcon("icons:General/Folder.ico", colorMap));

			tab->addTab(pHistoryView, "");
			tab->setTabToolTip(1, "History");
			tab->setTabIcon(1, CryIcon("icons:General/History_Undo_List.ico", colorMap));

			tab->addTab(pFavoritesView, "");
			tab->setTabToolTip(2, "Favorites");
			tab->setTabIcon(2, CryIcon("icons:General/Favorites_Full.ico", colorMap));

			auto splitter = new QSplitter(parent);
			splitter->addWidget(tab);
			splitter->addWidget(fileDetailsView);
			splitter->addWidget(previewContainer);

			splitter->setStretchFactor(0, 2);
			splitter->setStretchFactor(1, 3);
			splitter->setStretchFactor(2, 3);

			auto filenameLabel = new QLabel(parent);
			filenameLabel->setText(selectDirectory ? tr("Directory:") : tr("Filename:"));
			auto filenameLineEdit = new CFileNameLineEdit(parent);

			auto filetypeComboBox = new QMenuComboBox(parent);
			filetypeComboBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

			auto selectionLayout = new QHBoxLayout();
			selectionLayout->setSpacing(5);
			selectionLayout->addItem(new QSpacerItem(100, 20, QSizePolicy::Preferred, QSizePolicy::Minimum));
			selectionLayout->addWidget(filenameLabel);
			selectionLayout->addWidget(filenameLineEdit);
			if (!selectDirectory)
			{
				selectionLayout->addWidget(filetypeComboBox);
			}
			else
			{
				filetypeComboBox->hide();
			}

			auto openButton = new QPushButton(parent);
			openButton->setText(buttonLabel.isEmpty() ? (selectDirectory ? tr("Select") : tr("Open")) : buttonLabel);
			openButton->setDefault(selectDirectory ? false : true);
			openButton->setEnabled(selectDirectory ? true : false);

			auto cancelButton = new QPushButton(parent);
			cancelButton->setText(tr("Cancel"));

			auto actionLayout = new QHBoxLayout();
			actionLayout->setSpacing(5);
			actionLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
			actionLayout->addWidget(openButton);
			actionLayout->addWidget(cancelButton);

			auto verticalLayout = new QVBoxLayout(parent);
			verticalLayout->setSpacing(5);
			verticalLayout->addLayout(navigationLayout);
			verticalLayout->addWidget(splitter);
			verticalLayout->addLayout(selectionLayout);
			verticalLayout->addLayout(actionLayout);

			filetypeComboBox->signalCurrentIndexChanged.Connect([&models, filenameLineEdit, filetypeComboBox](int index)
			{
				auto extensions = filetypeComboBox->GetData(index).toStringList();
				extensions.removeAll("*");

				models.SetFilterExtensions(extensions);

				if (extensions.count() == 1)
					filenameLineEdit->SetExtensionForFiles(extensions.first());
			});

			connect(filenameLineEdit, &QLineEdit::textChanged, [=](const QString& newText)
			{
				openButton->setEnabled(selectDirectory || !newText.isEmpty());
			});

			this->pLookInComboBox = lookInComboBox;
			this->pFiletypeComboBox = filetypeComboBox;
			this->pSearchBox = searchBox;
			this->pUpButton = upButton;
			this->pBackButton = backButton;
			this->pCreateFolderButton = createFolderButton;
			this->pOpenButton = openButton;
			this->pCancelButton = cancelButton;
			this->pSplitter = splitter;
			this->pDirectoryTreeView = directoryTreeView;
			this->pFileDetailsView = fileDetailsView;
			this->pPreviewContainer = previewContainer;
			this->pFilenameLineEdit = filenameLineEdit;
		}

		QMenuComboBox*         pLookInComboBox;
		QMenuComboBox*         pFiletypeComboBox;
		QSearchBox*            pSearchBox;

		QAbstractButton*       pUpButton;
		QAbstractButton*       pBackButton;
		QAbstractButton*       pCreateFolderButton;
		QAbstractButton*       pOpenButton;
		QAbstractButton*       pCancelButton;

		QSplitter*             pSplitter;
		QAdvancedTreeView*     pDirectoryTreeView;
		CFileTreeView*         pFileDetailsView;
		CFilePreviewContainer* pPreviewContainer;
		CFileTreeView*		   pFavoritesView;
		CFileTreeView*		   pHistoryView;

		CFileNameLineEdit*     pFilenameLineEdit;
	};

	void AddProperties()
	{
		static const QString splitterProp = QStringLiteral("splitter");

		m_ui.pPreviewContainer->AddPersonalizedPropertyTo(m_dialog);

		m_dialog->AddPersonalizedSharedProperty(splitterProp, [this]
		{
			auto byteArray = m_ui.pSplitter->saveState();
			return QVariant(QString::fromUtf8(byteArray.toBase64()));
		}, [this](const QVariant& variant)
		{
			if (!variant.canConvert<QString>())
			{
			  return;
			}
			auto text = variant.toString();
			auto state = QByteArray::fromBase64(text.toUtf8());
			m_ui.pSplitter->restoreState(state);
		});

		m_dialog->AddPersonalizedProjectProperty("history", [this]()
		{
			//keep maximum 30 items in the history
			return QVariant(m_history.mid(0, 30));
		}, [this](const QVariant& variant)
		{
			//TODO : handle base dir here to only show valid history entries
			m_history = variant.toStringList();
			m_models.pHistoryModel->AddEntries(m_history);
		});

		m_dialog->AddPersonalizedProjectProperty("favorites", [this]()
		{
			return QVariant(m_favorites);
		}, [this](const QVariant& variant)
		{
			m_favorites = variant.toStringList();

			//only show favorites that match the filter
			for (auto& favoriteEntry : m_favorites)
			{
				if (IsMatchingFilters(favoriteEntry))
					m_models.pFavoritesModel->AddEntry(favoriteEntry);
			}
		});

		//favorites model
		//here we are trying to maintain the list of unfiltered favorites and update it based on what the user does with the filtered favorites that are displayed
		//added here to guarantee these were added after the models are populated
		connect(m_models.pFavoritesModel, &QAbstractItemModel::rowsInserted, [this](const QModelIndex &parent, int first, int last)
		{
			int insertionIndex = 0;
			if (first > 0)
			{
				QString path = m_models.pFavoritesModel->GetEntry(first - 1);
				insertionIndex = m_favorites.indexOf(path);
				CRY_ASSERT(insertionIndex >= 0);
				insertionIndex++;
			}

			for (int i = first; i <= last; i++)
			{
				QString path = m_models.pFavoritesModel->GetEntry(i);
				CRY_ASSERT(!path.isEmpty());
				if (!m_favorites.contains(path))
				{
					m_favorites.insert(insertionIndex, path);
					insertionIndex++;
				}
			}
		});

		connect(m_models.pFavoritesModel, &QAbstractItemModel::rowsAboutToBeRemoved, [this](const QModelIndex &parent, int first, int last)
		{
			for (int i = first; i <= last; i++)
			{
				QString path = m_models.pFavoritesModel->GetEntry(i);
				m_favorites.removeOne(path);
			}
		});

		connect(m_models.pFavoritesModel, &QAbstractItemModel::rowsAboutToBeMoved, [this](const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row)
		{
			int insertionIndex = 0;
			if (row > 0)
			{
				QString path = m_models.pFavoritesModel->GetEntry(row - 1);
				insertionIndex = m_favorites.indexOf(path);
				CRY_ASSERT(insertionIndex >= 0);
				insertionIndex++;
			}

			for (int i = start; i <= end; i++)
			{
				QString path = m_models.pFavoritesModel->GetEntry(i);
				CRY_ASSERT(!path.isEmpty());
				m_favorites.move(m_favorites.indexOf(path), insertionIndex);
				insertionIndex++;
			}
		});
	}

	CEngineFileDialog*   m_dialog;
	QSet<QString>        m_validExtensions;
	QString              m_defaultExtension;
	Models               m_models;
	Ui                   m_ui;
	History              m_navigationHistory;
	AcceptFileCallback   m_acceptFileCallback;
	CEngineFileDialog::Mode m_mode;
	std::vector<QString> m_selectedFiles;
	bool				 m_allowAnyFile;
	QStringList			 m_history;
	QStringList			 m_favorites;
};

CEngineFileDialog::CEngineFileDialog(OpenParams& openParams, QWidget* parent)
	: CEditorDialog(QStringLiteral("EngineFileDialog"), parent)
	, p(new Implementation(this, openParams))
{
	if (!openParams.title.isEmpty())
		setWindowTitle(openParams.title);
}

CEngineFileDialog::~CEngineFileDialog()
{
	/* automatic unique_ptr destruction */
}

std::vector<QString> CEngineFileDialog::GetSelectedFiles() const
{
	return p->m_selectedFiles;
}

QString CEngineFileDialog::RunGameOpen(const RunParams& runParams, QWidget* parent)
{
	//////////////////////////////////////////////////////////////////////////
	OpenParams openParams(CEngineFileDialog::OpenFile);
	openParams.initialDir = runParams.initialDir;
	openParams.extensionFilters = runParams.extensionFilters;
	//openParams.baseDirectory = runParams.baseDirectory;
	openParams.buttonLabel = runParams.buttonLabel.isEmpty() ? tr("Open") : runParams.buttonLabel;

	if (!runParams.initialFile.isEmpty())
		openParams.initialFile = runParams.initialFile;

	openParams.acceptFileCallback = [](const QString& path)
	{
		auto snapshot = GetIEditor()->GetFileSystemEnumerator()->GetCurrentSnapshot();
		auto filePtr = snapshot->GetFileByEnginePath(path);
		if (!filePtr)
		{
			CQuestionDialog::SCritical(tr("Error"), tr("File %1 does not exist!").arg(path));
			return false; // failed to find file
		}
		return true;
	};

	auto title = runParams.title.isEmpty() ? tr("Open File") : runParams.title;

	CEngineFileDialog dialog(openParams, parent);
	dialog.setWindowTitle(title);

	if (!dialog.exec())
	{
		return QString();
	}

	auto selectedFiles = dialog.GetSelectedFiles();
	if (selectedFiles.size() != 1)
	{
		return QString();
	}
	return selectedFiles.front();
}

QString CEngineFileDialog::RunGameSave(const RunParams& runParams, QWidget* parent)
{
	OpenParams openParams(CEngineFileDialog::SaveFile);
	openParams.title = runParams.title.isEmpty() ? tr("Save File") : runParams.title;
	openParams.initialDir = runParams.initialDir;
	openParams.extensionFilters = runParams.extensionFilters;
	//openParams.baseDirectory = runParams.baseDirectory;
	openParams.allowCreateFolder = true;
	openParams.buttonLabel = runParams.buttonLabel.isEmpty() ? tr("Save") : runParams.buttonLabel;

	if (!runParams.initialFile.isEmpty())
		openParams.initialFile = runParams.initialFile;

	openParams.acceptFileCallback = [](const QString& path)
	{
		auto snapshot = GetIEditor()->GetFileSystemEnumerator()->GetCurrentSnapshot();
		auto filePtr = snapshot->GetFileByEnginePath(path);
		if (filePtr)
		{
			auto result = CQuestionDialog::SQuestion(tr("Overwrite?"), tr("File %1 already exists! Do you want to overwrite?").arg(path));
			return (result == QDialogButtonBox::StandardButton::Yes);
		}
		return true;
	};

	CEngineFileDialog dialog(openParams, parent);

	if (!dialog.exec())
	{
		return QString();
	}

	auto selectedFiles = dialog.GetSelectedFiles();
	if (selectedFiles.size() != 1)
	{
		return QString();
	}
	
	return selectedFiles.front();
}

QString CEngineFileDialog::RunGameSelectDirectory(const RunParams& runParams, QWidget* parent)
{
	OpenParams openParams(CEngineFileDialog::SelectDirectory);
	openParams.title = runParams.title.isEmpty() ? tr("Select Directory") : runParams.title;
	openParams.initialDir = runParams.initialDir;
	//openParams.baseDirectory = runParams.baseDirectory;
	openParams.buttonLabel = runParams.buttonLabel.isEmpty() ? tr("Select") : runParams.buttonLabel;
	openParams.allowCreateFolder = true;

	if (!runParams.initialFile.isEmpty())
		openParams.initialFile = runParams.initialFile;

	openParams.acceptFileCallback = [](const QString& path)
	{
		auto snapshot = GetIEditor()->GetFileSystemEnumerator()->GetCurrentSnapshot();
		auto dirPtr = snapshot->GetDirectoryByEnginePath(path);
		if (!dirPtr)
		{
			CQuestionDialog::SCritical(tr("Error"), tr("Directory %1 does not exist!").arg(path));
			return false; // failed to find file
		}
		return true;
	};

	CEngineFileDialog dialog(openParams, parent);

	if (!dialog.exec())
	{
		return QString();
	}

	auto selectedFiles = dialog.GetSelectedFiles();
	if (selectedFiles.size() != 1)
	{
		return QString();
	}
	return selectedFiles.front();
}

