// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "Controls/EditorDialog.h"

#include "EditorCommonAPI.h"

#include "FileDialogs/ExtensionFilter.h"
#include "FileSystem/FileSystem_FileFilter.h"

#include <vector>
#include <memory>

class CFilePopupMenu;

class EDITOR_COMMON_API CEngineFileDialog
	: public CEditorDialog
{
	Q_OBJECT

public:
	typedef FileSystem::SFileFilter::FileFilterCallback                             FileFilterCallback;
	typedef std::function<bool (const QString& path)> AcceptFileCallback;

	enum Mode
	{
		SelectDirectory,	// ignore files and select a directory
		OpenFile,			// select one file
		OpenMultipleFiles,	// select multiple files
		SaveFile,			// save (write) a file

		Count
	};

	struct OpenParams
	{
		Mode                  mode;
		QString               title;
		ExtensionFilterVector extensionFilters; // extension filters for files
		QString               initialDir;       // full engine path
		QString               initialFile;      // full engine path
		QString               baseDirectory; // limits selection to files below this directory
		FileFilterCallback    fileFilterFunc;
		bool                  allowCreateFolder;   // show a button to create new folders
		QString               defaultExtension;    // if set, this will be selected as the default extension. If the user doesn't specify an extension however, the first extension from the current extension filter will be used. This is similar to windows behavior
		AcceptFileCallback    acceptFileCallback;
		QString               buttonLabel; // empty is replaced with default

		OpenParams(Mode inMode) : mode(inMode), allowCreateFolder(false) {}
	};
	struct RunParams
	{
		ExtensionFilterVector extensionFilters;
		QString               baseDirectory; // limits selection to files below this directory
		QString               initialDir;    // full system path
		QString               initialFile;   // full system path
		QString               title;         // empty is replaced with default
		QString               buttonLabel;   // empty is replaced with default
	};

public:
	explicit CEngineFileDialog(OpenParams& openParams, QWidget* parent = nullptr);
	~CEngineFileDialog();

	/// \returns vector with engine pathes of all files selected
	std::vector<QString> GetSelectedFiles() const;

public:
	/// \return selected game file path or empty string if aborted. Path will be relative to game folder.
	/// \note initialDir & baseDirectory are relative to the game folder
	static QString RunGameOpen(const RunParams&, QWidget* parent = nullptr);

	/// \return selected game file path or empty string if aborted. Path will be relative to game folder.
	/// \note initialDir & baseDirectory are relative to the game folder
	static QString RunGameSave(const RunParams&, QWidget* parent = nullptr);

	/// \return selected game data directory path or empty string if aborted. Path will be relative to game folder.
	static QString RunGameSelectDirectory(const RunParams& runParams, QWidget* parent = nullptr);

signals:
	void PopupMenuToBePopulated(CFilePopupMenu* menu);

private:
	struct Implementation;
	std::unique_ptr<Implementation> p;
};

