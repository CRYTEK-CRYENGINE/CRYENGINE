// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "BatchFileDialog.h"
#include "QPropertyTree/QPropertyDialog.h"
#include <CrySerialization/StringList.h>
#include <CrySerialization/STL.h>
#include <CrySerialization/IArchive.h>
#include <vector>
#include <CrySystem/File/ICryPak.h>
#include <CryString/StringUtils.h>
#include <QApplication>
#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <CryCore/Platform/CryWindows.h>
#include "FileDialogs/SystemFileDialog.h"

struct SBatchFileItem
{
	bool   selected;
	string path;

	bool operator<(const SBatchFileItem& rhs) const { return path < rhs.path; }

	void Serialize(Serialization::IArchive& ar)
	{
		ar(selected, "selected", "^");
		ar(path, "path", "^<!");
	}
};
typedef std::vector<SBatchFileItem> SBatchFileItems;

struct CBatchFileDialog::SContent
{
	SBatchFileItems items;
	string          listLabel;

	SContent(const char* listLabel)
	{
		this->listLabel = ">60>+";
		this->listLabel += listLabel;
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(items, "items", listLabel.c_str());
	}
};

static bool ReadFile(std::vector<char>* buffer, const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f)
		return false;
	fseek(f, 0, SEEK_END);
	size_t len = (size_t)ftell(f);
	fseek(f, 0, SEEK_SET);

	buffer->resize(len);
	bool result = true;
	if (len)
		result = fread(&(*buffer)[0], 1, len, f) == len;
	fclose(f);
	return result;
}

static void SplitLines(std::vector<string>* lines, const char* start, const char* end)
{
	const char* p = start;
	const char* lineStart = start;
	while (true)
	{
		if (p == end || *p == '\r' || *p == '\n')
		{
			if (p != lineStart)
			{
				string line(lineStart, p);
				bool hasPrintableChars = false;
				for (size_t i = 0; i < line.size(); ++i)
					if (!isspace(line[i]))
						hasPrintableChars = true;
				if (hasPrintableChars)
					lines->push_back(line);
			}
			lineStart = p + 1;
			if (p == end)
				break;
		}
		++p;
	}
}

static string NormalizePath(const char* path)
{
	string result = path;
	result.replace('\\', '/');
	result.MakeLower();

	// strip .phys extensions in case list of .cdf is provided
	string::size_type dotPos = result.rfind('.');
	if (dotPos != string::npos)
	{
		if (stricmp(result.c_str() + dotPos, ".phys"))
			result.erase(dotPos, 5);
	}
	return result;
}

static bool IsEquivalentPath(const char* pathA, const char* pathB)
{
	string normalizedA = NormalizePath(pathA);
	string normalizedB = NormalizePath(pathB);
	return normalizedA == normalizedB;
}

void CBatchFileDialog::OnLoadList()
{
	CSystemFileDialog::RunParams runParams;
	runParams.title = tr("Load file list...");
	runParams.extensionFilters << CExtensionFilter(tr("Text Files"), "txt");

	const QString existingFile = CSystemFileDialog::RunImportFile(runParams, m_dialog);
	if (existingFile.isEmpty())
	{
		return;
	}

	string path = existingFile.toLocal8Bit().data();
	std::vector<char> content;
	ReadFile(&content, path.c_str());

	std::vector<string> lines;
	SplitLines(&lines, &content[0], &content[0] + content.size());

	for (size_t i = 0; i < m_content->items.size(); ++i)
	{
		m_content->items[i].selected = false;
	}
	for (size_t i = 0; i < lines.size(); ++i)
	{
		const char* line = lines[i].c_str();
		for (size_t j = 0; j < m_content->items.size(); ++j)
		{
			const char* itemPath = m_content->items[j].path.c_str();
			if (IsEquivalentPath(line, itemPath))
			{
				m_content->items[j].selected = true;
				break;
			}
		}
	}

	m_dialog->revert();
}

void CBatchFileDialog::OnSelectAll()
{
	for (size_t i = 0; i < m_content->items.size(); ++i)
		m_content->items[i].selected = true;

	m_dialog->revert();
}

void CBatchFileDialog::OnSelectNone()
{
	for (size_t i = 0; i < m_content->items.size(); ++i)
		m_content->items[i].selected = false;

	m_dialog->revert();
}

static void FindFiles(std::vector<string>* filenames, const char* directory, const char* subdirectory, const char* mask)
{
	char path[1024];

	if (subdirectory[0])
	{
		if (!cry_sprintf(path, "%s\\%s\\*.*", directory, subdirectory))
			return;
	}
	else
	{
		if (!cry_sprintf(path, "%s\\*.*", directory))
			return;
	}

	WIN32_FIND_DATAA fd;
	HANDLE hff = FindFirstFileA(path, &fd);
	if (hff != INVALID_HANDLE_VALUE)
	{
		BOOL bIsFind = TRUE;
		while (hff && bIsFind)
		{
			string filename = subdirectory[0] ? (string(subdirectory) + "\\" + fd.cFileName) : string(fd.cFileName);

			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
					FindFiles(filenames, directory, filename, mask);
			}
			else
			{
				if (CryStringUtils::MatchWildcard(fd.cFileName, mask))
					filenames->push_back(filename);
			}
			bIsFind = FindNextFileA(hff, &fd);
		}
		FindClose(hff);
	}
}

bool EDITOR_COMMON_API ShowBatchFileDialog(Serialization::StringList* result, const SBatchFileSettings& settings, QWidget* parent)
{
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	CBatchFileDialog::SContent content(settings.listLabel);
	if (settings.scanExtension[0] != '\0')
	{
		if (settings.useCryPak)
		{
			std::vector<string> files;

			string mask = "*.";
			mask += settings.scanExtension;
			SDirectoryEnumeratorHelper helper;
			helper.ScanDirectoryRecursive("", "", mask.c_str(), files);

			for (int k = 0; k < files.size(); ++k)
			{
				SBatchFileItem item;
				item.selected = true;
				item.path = files[k];
				item.path.replace('\\', '/');
				content.items.push_back(item);
			}

		}
		else
		{
			string gameFolder = gEnv->pCryPak->GetGameFolder();
			const char* modDirectory = gameFolder.c_str();
			int modIndex = 0;
			do
			{
				string modPrefix = string(GetIEditor()->GetMasterCDFolder()).GetString();
				if (!modPrefix.empty() && modPrefix[modPrefix.size() - 1] != '\\')
					modPrefix += "\\";
				modPrefix += modDirectory;
				if (!modPrefix.empty() && modPrefix[modPrefix.size() - 1] != '\\')
					modPrefix += "\\";
				modPrefix.replace('/', '\\');

				std::vector<string> files;
				string mask = "*.";
				mask += settings.scanExtension;
				FindFiles(&files, modPrefix, "", mask.c_str());

				for (int k = 0; k < files.size(); ++k)
				{
					SBatchFileItem item;
					item.selected = true;
					item.path = modPrefix;
					item.path += files[k];
					item.path.replace('/', '\\');
					content.items.push_back(item);
				}

				modDirectory = gEnv->pCryPak->GetMod(modIndex);
				++modIndex;
			}
			while (modDirectory != 0);
		}
	}

	content.items.reserve(content.items.size() + settings.explicitFileList.size());
	for (size_t i = 0; i < settings.explicitFileList.size(); ++i)
	{
		SBatchFileItem item;
		item.path = settings.explicitFileList[i].c_str();
		item.selected = true;
		content.items.push_back(item);
	}

	std::sort(content.items.begin(), content.items.end());
	QApplication::restoreOverrideCursor();

	QPropertyDialog dialog(parent);
	dialog.setSerializer(Serialization::SStruct(content));
	dialog.setWindowTitle(settings.title);
	dialog.setWindowStateFilename(settings.stateFilename);
	dialog.setSizeHint(QSize(settings.defaultWidth, settings.defaultHeight));
	CBatchFileDialog handler;
	handler.m_dialog = &dialog;
	handler.m_content = &content;

	QBoxLayout* topRow = new QBoxLayout(QBoxLayout::LeftToRight);
	QLabel* label = new QLabel(settings.descriptionText);
	QFont font;
	font.setBold(true);
	label->setFont(font);
	topRow->addWidget(label, 1);
	{
		if (settings.allowListLoading)
		{
			QPushButton* loadListButton = new QPushButton("Load List...");
			QObject::connect(loadListButton, SIGNAL(pressed()), &handler, SLOT(OnLoadList()));
			topRow->addWidget(loadListButton);
		}
		QPushButton* selectAllButton = new QPushButton("Select All");
		QObject::connect(selectAllButton, SIGNAL(pressed()), &handler, SLOT(OnSelectAll()));
		topRow->addWidget(selectAllButton);
		QPushButton* selectNoneButton = new QPushButton("Select None");
		QObject::connect(selectNoneButton, SIGNAL(pressed()), &handler, SLOT(OnSelectNone()));
		topRow->addWidget(selectNoneButton);
	}
	dialog.layout()->insertLayout(0, topRow);

	if (parent)
	{
		QPoint center = parent->rect().center();
		dialog.move(max(0, center.x() - dialog.width() / 2),
		            max(0, center.y() - dialog.height() / 2));
	}

	int numFailed = 0;
	int numSaved = 0;
	std::vector<string> failedFiles;
	if (dialog.exec() == QDialog::Accepted)
	{
		result->clear();
		for (size_t i = 0; i < content.items.size(); ++i)
		{
			if (content.items[i].selected)
			{
				const char* path = content.items[i].path.c_str();
				result->push_back(path);
			}
		}
		return true;
	}
	return false;
}

