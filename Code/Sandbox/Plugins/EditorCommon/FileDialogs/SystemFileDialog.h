// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FileDialogs/ExtensionFilter.h"

#include "EditorCommonAPI.h"

#include <vector>
#include <QFileDialog>

class CFilePopupMenu;

class EDITOR_COMMON_API CSystemFileDialog
{
public:
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
		QString               initialDir;       // full system path
		QString               initialFile;      // full system path

		OpenParams(Mode inMode) : mode(inMode) {}
	};
	struct RunParams
	{
		ExtensionFilterVector extensionFilters;
		QString               initialDir;  // full system path
		QString               initialFile; // full system path
		QString               title;       // empty is replaced with default
		QString               buttonLabel; // empty is replaced with default
	};

public:
	explicit CSystemFileDialog(const OpenParams& openParams, QWidget* parent = nullptr);

	int Execute() { return m_dialog.exec(); }

	/// \returns vector with full pathes of all files selected
	std::vector<QString> GetSelectedFiles() const;

public:
	/// \return selected system file path or empty string if aborted
	static QString RunImportFile(const RunParams& runParams, QWidget* parent = nullptr);

	/// \return selected system file paths or empty vector if aborted
	static std::vector<QString> RunImportMultipleFiles(const RunParams& runParams, QWidget* parent = nullptr);

	/// \return selected system file path or empty string if aborted
	static QString RunExportFile(const RunParams& runParams, QWidget* parent = nullptr);

	/// \return selected system directory path or empty string if aborted
	static QString RunSelectDirectory(const RunParams& runParams, QWidget* parent = nullptr);

protected:
	QFileDialog m_dialog;
};

