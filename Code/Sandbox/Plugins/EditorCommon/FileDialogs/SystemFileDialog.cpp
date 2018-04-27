// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "SystemFileDialog.h"

#include <QTranslator>

bool IsWildcardExtension(const QString& ext)
{
	return ext == "*";
}

QStringList GetExtensionFilters(const ExtensionFilterVector& extensionFilters)
{
	QStringList filters;

	if (extensionFilters.isEmpty())
	{
		filters << "All Files (*)";
	}
	else
	{
		foreach(const CExtensionFilter &extensionFilter, extensionFilters)
		{
			QString filter = extensionFilter.GetDescription();
			filter += ' ';
			filter += '(';

			auto extensions = extensionFilter.GetExtensions();
			for(auto extensionIt = extensions.begin(), end = extensions.end(); extensionIt != extensions.end(); ++extensionIt)
			{
				filter += '*';
				filter += '.';
				filter += *extensionIt;

				if (extensionIt + 1 != end)
				{
					filter += ' ';
				}
			}

			filter += ')';

			filters << filter;
		}
	}

	return filters;
}

CSystemFileDialog::CSystemFileDialog(const OpenParams& openParams, QWidget* parent)
	: m_dialog(parent)
{
	m_dialog.setOption(QFileDialog::DontUseCustomDirectoryIcons);

	if (openParams.mode == Mode::OpenMultipleFiles)
	{
		m_dialog.setFileMode(QFileDialog::ExistingFiles);
	}
	else if(openParams.mode == Mode::OpenFile)
	{
		m_dialog.setFileMode(QFileDialog::ExistingFile);
	}
	else if (openParams.mode == Mode::SaveFile)
	{
		m_dialog.setFileMode(QFileDialog::AnyFile);
		m_dialog.setAcceptMode(QFileDialog::AcceptSave);
		m_dialog.setConfirmOverwrite(true);
	}
	else if (openParams.mode == Mode::SelectDirectory)
	{
		m_dialog.setFileMode(QFileDialog::Directory);
	}

	if (openParams.mode != Mode::SelectDirectory)
	{
		m_dialog.setNameFilters(GetExtensionFilters(openParams.extensionFilters));
	}

	m_dialog.setWindowTitle(openParams.title);
	m_dialog.setDirectory(openParams.initialDir);
	m_dialog.selectFile(openParams.initialFile);
}

std::vector<QString> CSystemFileDialog::GetSelectedFiles() const
{
	return m_dialog.selectedFiles().toVector().toStdVector();
}

QString CSystemFileDialog::RunImportFile(const RunParams& runParams, QWidget* parent)
{
	OpenParams openParams(CSystemFileDialog::OpenFile);
	openParams.title = runParams.title.isEmpty() ? "Import File" : runParams.title;
	openParams.initialDir = runParams.initialDir;
	openParams.initialFile = runParams.initialFile;
	openParams.extensionFilters = runParams.extensionFilters;

	CSystemFileDialog dialog(openParams, parent);

	if (!dialog.Execute())
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

std::vector<QString> CSystemFileDialog::RunImportMultipleFiles(const RunParams& runParams, QWidget* parent)
{
	OpenParams openParams(CSystemFileDialog::OpenMultipleFiles);
	openParams.title = runParams.title.isEmpty() ? QObject::tr("Import Files") : runParams.title;
	openParams.initialDir = runParams.initialDir;
	openParams.initialFile = runParams.initialFile;
	openParams.extensionFilters = runParams.extensionFilters;

	CSystemFileDialog dialog(openParams, parent);

	if (!dialog.Execute())
	{
		return std::vector<QString>();
	}

	return dialog.GetSelectedFiles();
}

QString CSystemFileDialog::RunExportFile(const RunParams& runParams, QWidget* parent)
{
	OpenParams openParams(CSystemFileDialog::SaveFile);
	openParams.title = runParams.title.isEmpty() ? "Export File" : runParams.title;
	openParams.initialDir = runParams.initialDir;
	openParams.initialFile = runParams.initialFile;
	openParams.extensionFilters = runParams.extensionFilters;

	CSystemFileDialog dialog(openParams, parent);

	if (!dialog.Execute())
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

QString CSystemFileDialog::RunSelectDirectory(const RunParams& runParams, QWidget* parent)
{
	OpenParams openParams(CSystemFileDialog::SelectDirectory);
	openParams.title = runParams.title.isEmpty() ? "Select Directory" : runParams.title;
	openParams.initialDir = runParams.initialDir;
	openParams.initialFile = runParams.initialFile;

	CSystemFileDialog dialog(openParams, parent);

	if (!dialog.Execute())
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

