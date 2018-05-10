// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ExplorerFileList.h"
#include <IBackgroundTaskManager.h>
#include <IEditor.h>
#include <QFileDialog>
#include "ScanAndLoadFilesTask.h"
#include "Explorer.h"
#include "Serialization.h"
#include "BatchFileDialog.h"
#include "Expected.h"
#include "FileDialogs/EngineFileDialog.h"
#include "Controls/QuestionDialog.h"
#include <CrySystem/File/ICryPak.h>

namespace Explorer
{

string EntryFormat::MakeFilename(const char* entryPath) const
{
	return path.empty() ? PathUtil::ReplaceExtension(entryPath, extension) : path;
}

static bool LoadFile(vector<char>* buf, const char* filename)
{
	FILE* f = gEnv->pCryPak->FOpen(filename, "rb");
	if (!f)
		return false;

	gEnv->pCryPak->FSeek(f, 0, SEEK_END);
	size_t size = gEnv->pCryPak->FTell(f);
	gEnv->pCryPak->FSeek(f, 0, SEEK_SET);

	buf->resize(size);
	bool result = gEnv->pCryPak->FRead(&(*buf)[0], size, f) == size;
	gEnv->pCryPak->FClose(f);
	return result;
}

bool JSONLoader::Load(EntryBase* entry, const char* filename)
{
	string filePath = string(gEnv->pCryPak->GetGameFolder()) + "\\" + filename;
	filePath.replace('/', '\\');

	vector<char> content;
	if (!LoadFile(&content, filePath.c_str()))
		return false;

	return Serialization::LoadJsonBuffer(*entry, content.data(), content.size());
}

bool JSONLoader::Save(EntryBase* entry, const char* filename)
{
	string filePath = string(gEnv->pCryPak->GetGameFolder()) + "\\" + filename;
	filePath.replace('/', '\\');
	return Serialization::SaveJsonFile(filePath.c_str(), *entry);
}

// ---------------------------------------------------------------------------

ExplorerFileList::~ExplorerFileList()
{
	for (size_t i = 0; i < m_entryTypes.size(); ++i)
	{
		delete m_entryTypes[i].entryList;
		m_entryTypes[i].entryList = 0;
	}

	GetIEditor()->GetFileMonitor()->UnregisterListener(this);
}

EntryType& ExplorerFileList::AddEntryType(EntryListBase* list, IEntryDependencyExtractor* extractor)
{
	EntryType type;
	type.entryList = list;
	type.dependencyExtractor.reset(extractor);

	m_entryTypes.push_back(type);

	if (m_entryTypes.size() > 1)
		m_entryTypes.back().entryList->SetIdProvider(m_entryTypes[0].entryList);

	return m_entryTypes.back();
}

EntryType* ExplorerFileList::AddSingleFileEntryType(EntryListBase* list, const char* path, const char* label, IEntryLoader* loader)
{
	EntryType entryType;
	entryType.entryList = list;

	EntryFormat format;
	format.path = path;
	format.loader = loader;
	format.usage = FORMAT_LOAD | FORMAT_SAVE | FORMAT_LIST;
	entryType.formats.push_back(format);

	m_entryTypes.push_back(entryType);

	if (m_entryTypes.size() > 1)
		m_entryTypes.back().entryList->SetIdProvider(m_entryTypes[0].entryList);

	return &m_entryTypes.back();
}

void ExplorerFileList::Populate()
{
	for (size_t i = 0; i < m_entryTypes.size(); ++i)
	{
		EntryType& entryType = m_entryTypes[i];

		bool singleFileFormat = false;
		if (!entryType.fileListenerRegistered)
		{
			for (size_t j = 0; j < entryType.formats.size(); ++j)
			{
				const EntryFormat& format = entryType.formats[j];
				if (!format.path.empty())
				{
					GetIEditor()->GetFileMonitor()->RegisterListener(this, format.path.c_str());
					singleFileFormat = true;
				}
				else
				{
					GetIEditor()->GetFileMonitor()->RegisterListener(this, "", format.extension);
				}
			}
			entryType.fileListenerRegistered = true;
		}

		if (singleFileFormat)
			continue;

		entryType.entryList->Clear();
		SignalSubtreeReset(m_subtree);

		string firstExtension;
		SLookupRule r;
		for (size_t j = 0; j < entryType.formats.size(); ++j)
		{
			const EntryFormat& format = entryType.formats[j];
			if (!format.path.empty())
				continue;
			if ((format.usage & FORMAT_LIST) == 0)
				continue;
			r.masks.push_back(string("*.") + format.extension);
			if (firstExtension.empty())
				firstExtension = format.extension;
		}
		string description = firstExtension.empty() ? string() : (firstExtension + " scan");
		SScanAndLoadFilesTask* task = new SScanAndLoadFilesTask(r, description.c_str());
		EXPECTED(connect(task, SIGNAL(SignalFileLoaded(const ScanLoadedFile &)), this, SLOT(OnBackgroundFileLoaded(const ScanLoadedFile &))));
		EXPECTED(connect(task, SIGNAL(SignalLoadingFinished()), this, SLOT(OnBackgroundLoadingFinished())));
		GetIEditor()->GetBackgroundTaskManager()->AddTask(task, eTaskPriority_FileUpdate, eTaskThreadMask_IO);
	}
}

EntryType* ExplorerFileList::GetEntryTypeByExtension(const char* ext)
{
	for (size_t i = 0; i < m_entryTypes.size(); ++i)
		for (size_t j = 0; j < m_entryTypes[i].formats.size(); ++j)
			if (stricmp(m_entryTypes[i].formats[j].extension, ext) == 0)
				return &m_entryTypes[i];
	return 0;
}

const EntryType* ExplorerFileList::GetEntryTypeByExtension(const char* extension) const
{
	return const_cast<ExplorerFileList*>(this)->GetEntryTypeByExtension(extension);
}

EntryType* ExplorerFileList::GetEntryTypeByPath(const char* singleFilePath)
{
	for (size_t i = 0; i < m_entryTypes.size(); ++i)
		for (size_t j = 0; j < m_entryTypes[i].formats.size(); ++j)
			if (stricmp(m_entryTypes[i].formats[j].path.c_str(), singleFilePath) == 0)
				return &m_entryTypes[i];
	return 0;
}

const EntryType* ExplorerFileList::GetEntryTypeByPath(const char* path) const
{
	return const_cast<ExplorerFileList*>(this)->GetEntryTypeByPath(path);
}

bool ExplorerFileList::AddAndSaveEntry(const char* filename)
{
	uint id = AddEntry(filename, true).id;
	if (!id)
		return false;
	return SaveEntry(0, id);
}

void ExplorerFileList::SetExplorerData(ExplorerData* explorerData, int subtree)
{
	m_explorerData = explorerData;
	m_subtree = subtree;
	m_explorerPakColumn = m_explorerData->FindColumn("Pak");
	if (m_explorerPakColumn < 0 && m_addPakColumn)
	{
		// Matches the order of ICryPak::EFileSearchLocation
		const ExplorerColumnValue pakValues[] = {
			{ "In folder",       "icons:General/Folder.ico"         },
			{ "In pak",          "icons:General/Pakfile.ico"        },
			{ "In folder & pak", "icons:General/Pakfile_Folder.ico" },
		};
		int pakValueCount = sizeof(pakValues) / sizeof(pakValues[0]);
		m_explorerPakColumn = m_explorerData->AddColumn("Pak", ExplorerColumn::ICON, false, pakValues, pakValueCount);
	}
}

ExplorerEntryId ExplorerFileList::AddEntry(const char* filename, bool resetIfExists)
{
	const char* ext = PathUtil::GetExt(filename);
	const EntryType* format = GetEntryTypeByExtension(ext);
	if (!format)
		return ExplorerEntryId();

	bool newEntry;
	EntryBase* entry = format->entryList->AddEntry(&newEntry, filename, 0, resetIfExists);

	if (newEntry)
		SignalEntryAdded(m_subtree, entry->id);
	else
	{
		EntryModifiedEvent ev;
		ev.subtree = m_subtree;
		ev.id = entry->id;
		SignalEntryModified(ev);
	}
	return ExplorerEntryId(m_subtree, entry->id);
}

EntryBase* ExplorerFileList::GetEntryBaseByPath(const char* path)
{
	for (size_t i = 0; i < m_entryTypes.size(); ++i)
	{
		EntryBase* entry = m_entryTypes[i].entryList->GetBaseByPath(path);
		if (entry)
			return entry;
	}
	return 0;
}

EntryBase* ExplorerFileList::GetEntryBaseById(uint id)
{
	for (size_t i = 0; i < m_entryTypes.size(); ++i)
	{
		EntryBase* entry = m_entryTypes[i].entryList->GetBaseById(id);
		if (entry)
			return entry;
	}
	return 0;
}

void ExplorerFileList::OnBackgroundLoadingFinished()
{
	SignalSubtreeLoadingFinished(m_subtree);
}

void ExplorerFileList::OnBackgroundFileLoaded(const ScanLoadedFile& file)
{
	string path = GetCanonicalPath(file.scannedFile.c_str());
	ExplorerEntryId entryId = AddEntry(path.c_str(), false);

	UpdateEntryPakState(entryId, file.pakState);
}

int ExplorerFileList::GetEntryCount() const
{
	int result = 0;
	for (size_t i = 0; i < m_entryTypes.size(); ++i)
		result += m_entryTypes[i].entryList->Count();
	return result;
}

uint ExplorerFileList::GetEntryIdByIndex(int index) const
{
	int startRange = 0;
	for (size_t i = 0; i < m_entryTypes.size(); ++i)
	{
		int size = m_entryTypes[i].entryList->Count();
		if (index >= startRange && index < startRange + size)
		{
			if (EntryBase* entry = m_entryTypes[i].entryList->GetBaseByIndex(index - startRange))
				return entry->id;
			else
				return 0;
		}
		startRange += size;
	}

	return 0;
}

bool ExplorerFileList::SaveAll(ActionOutput* output)
{
	bool failed = false;
	for (size_t j = 0; j < m_entryTypes.size(); ++j)
	{
		EntryType& format = m_entryTypes[j];
		for (size_t i = 0; i < format.entryList->Count(); ++i)
		{
			if (EntryBase* entry = format.entryList->GetBaseByIndex(i))
			{
				if (!entry->modified)
					continue;
				int errorCount = output->errorCount;
				if (!SaveEntry(output, entry->id))
				{
					if (errorCount == output->errorCount)
						output->AddError("Failed to save file", entry->path.c_str());
					failed = true;
				}
			}
		}
	}
	return !failed;
}

static bool SaveEntryOfType(ActionOutput* output, EntryBase* entry, const EntryType* entryType)
{
	for (size_t i = 0; i < entryType->formats.size(); ++i)
	{
		const EntryFormat& format = entryType->formats[i];
		if (!format.loader.get())
			continue;
		if (!(format.usage & FORMAT_SAVE))
			continue;
		string filename = format.MakeFilename(entry->path.c_str());
		if (!format.loader->Save(entry, filename.c_str()))
		{
			if (output)
				output->AddError("Failed to save file", filename.c_str());
			return false;
		}
	}
	return true;
}

string ExplorerFileList::GetSaveFileExtension(const char* entryPath) const
{
	string extension = PathUtil::GetExt(entryPath);
	const EntryType* entryType = GetEntryTypeByExtension(extension.c_str());
	if (!entryType)
		return string();

	for (size_t j = 0; j < entryType->formats.size(); ++j)
	{
		const EntryFormat& format = entryType->formats[j];
		if (format.usage & FORMAT_SAVE && !format.extension.empty())
			return format.extension;
	}

	return string();
}

bool ExplorerFileList::CanBeSaved(const char* entryPath) const
{
	string extension = PathUtil::GetExt(entryPath);
	if (const EntryType* entryType = GetEntryTypeByExtension(extension.c_str()))
	{
		for (size_t j = 0; j < entryType->formats.size(); ++j)
		{
			const EntryFormat& format = entryType->formats[j];
			if (format.usage & FORMAT_SAVE)
			{
				if (!format.extension.empty())
					return true;
			}
		}
	}

	if (const EntryType* entryType = GetEntryTypeByPath(entryPath))
	{
		for (size_t j = 0; j < entryType->formats.size(); ++j)
		{
			const EntryFormat& format = entryType->formats[j];
			if (stricmp(format.path.c_str(), entryPath) == 0 && format.usage & FORMAT_SAVE)
				return true;
		}
	}

	return false;
}

bool ExplorerFileList::SaveEntry(ActionOutput* output, uint id)
{
	const EntryType* entryType;
	if (EntryBase* entry = GetEntry(id, &entryType))
	{
		if (!entry->loaded)
			LoadOrGetChangedEntry(id);

		if (SaveEntryOfType(output, entry, entryType))
		{
			if (entry->Saved())
			{
				EntryModifiedEvent ev;
				ev.subtree = m_subtree;
				ev.id = id;
				SignalEntryModified(ev);
			}

			UpdateEntryPakState(ExplorerEntryId(m_subtree, id), entry->path, entryType);
			return true;
		}
		else
		{
			return false;
		}
	}

	if (output)
		output->AddError("Saving nonexistent entry", "");
	return false;
}

string ExplorerFileList::GetSaveFilename(uint id)
{
	EntryBase* entry = GetEntry(id);
	if (!entry)
		return string();

	return entry->path;
}

void ExplorerFileList::RevertEntry(uint id)
{
	const EntryType* entryType;
	EntryBase* entry = GetEntry(id, &entryType);
	if (entry)
	{
		bool listedFileExists = false;
		for (size_t i = 0; i < entryType->formats.size(); ++i)
		{
			const EntryFormat& format = entryType->formats[i];
			if ((format.usage & FORMAT_LIST) == 0)
				continue;

			string filename = format.MakeFilename(entry->path.c_str());
			if (gEnv->pCryPak->IsFileExist(filename))
				listedFileExists = true;
		}

		if (!listedFileExists)
		{
			if (entryType->entryList->RemoveById(id))
				SignalEntryRemoved(m_subtree, id);
			return;
		}

		entry->failedToLoad = false;
		for (size_t i = 0; i < entryType->formats.size(); ++i)
		{
			const EntryFormat& format = entryType->formats[i];

			string filename = format.MakeFilename(entry->path.c_str());
			bool fileExists = gEnv->pCryPak->IsFileExist(entry->path.c_str());
			if (!format.loader)
				continue;
			if (!format.loader->Load(entry, filename.c_str()))
				entry->failedToLoad = true;
		}
		entry->RestoreSavedContent();

		EntryModifiedEvent ev;
		if (entry->Reverted(&ev.previousContent))
		{
			ev.subtree = m_subtree;
			ev.id = entry->id;
			ev.reason = "Revert";
			ev.contentChanged = ev.previousContent != entry->lastContent;
			SignalEntryModified(ev);
		}

		UpdateEntryPakState(ExplorerEntryId(m_subtree, id), entry->path, entryType);
	}
}

EntryBase* ExplorerFileList::GetEntry(uint id, const EntryType** outFormat) const
{
	for (size_t i = 0; i < m_entryTypes.size(); ++i)
	{
		const EntryType& format = m_entryTypes[i];

		if (EntryBase* entry = format.entryList->GetBaseById(id))
		{
			if (outFormat)
				*outFormat = &format;
			return entry;
		}
	}
	return 0;
}

void ExplorerFileList::CheckIfModified(uint id, const char* reason, bool continuousChange)
{
	const EntryType* format;
	EntryBase* entry = GetEntry(id, &format);
	if (!entry)
		return;
	EntryModifiedEvent ev;
	ev.continuousChange = continuousChange;
	if (continuousChange || entry->Changed(&ev.previousContent))
	{
		ev.subtree = m_subtree;
		ev.id = entry->id;
		if (reason)
		{
			ev.reason = reason;
			ev.contentChanged = true;
		}
		SignalEntryModified(ev);
	}
}

void ExplorerFileList::GetEntryActions(vector<ExplorerAction>* actions, uint id, ExplorerData* explorerData)
{
	if (EntryBase* entry = GetEntry(id))
	{
		bool canBeSaved = CanBeSaved(entry->path.c_str());
		actions->push_back(ExplorerAction("Revert", 0, [=](ActionContext& x) { explorerData->ActionRevert(x); }, "icons:General/History_Undo.ico"));
		if (canBeSaved)
			actions->push_back(ExplorerAction("Save", 0, [=](ActionContext& x) { explorerData->ActionSave(x); }, "icons:General/File_Save.ico"));
		if (canBeSaved && !GetSaveFileExtension(entry->path).empty())
			actions->push_back(ExplorerAction("Save As...", ACTION_NOT_STACKABLE, [&](ActionContext& x) { ActionSaveAs(x); }, "", ""));
		actions->push_back(ExplorerAction());
		if (canBeSaved)
			actions->push_back(ExplorerAction("Delete", 0, [=](ActionContext& x) { ActionDelete(x); }));
		actions->push_back(ExplorerAction());
		actions->push_back(ExplorerAction("Show in Explorer", ACTION_NOT_STACKABLE, [=](ActionContext& x) { explorerData->ActionShowInExplorer(x); }, "icons:General/Show_In_Explorer.ico"));
	}
}

void ExplorerFileList::UpdateEntry(ExplorerEntry* explorerEntry)
{
	if (EntryBase* entry = GetEntry(explorerEntry->id))
	{
		explorerEntry->name = entry->name;
		explorerEntry->path = entry->path;
		explorerEntry->modified = entry->modified;
		if (explorerEntry->type == ENTRY_ASSET)
			explorerEntry->icon = m_dataIcon;
	}
}

bool ExplorerFileList::GetEntrySerializer(Serialization::SStruct* out, uint id) const
{
	if (EntryBase* entry = GetEntry(id))
	{
		*out = Serialization::SStruct(*entry);
		return true;
	}
	return false;
}

static void LoadEntryOfType(EntryBase* entry, const EntryType* entryType)
{
	entry->loaded = true;
	entry->failedToLoad = false;
	for (size_t i = 0; i < entryType->formats.size(); ++i)
	{
		const EntryFormat& format = entryType->formats[i];
		if (!(format.usage & FORMAT_LOAD))
			continue;
		if (!format.loader)
			continue;
		string filename = format.MakeFilename(entry->path.c_str());
		if (!format.loader->Load(entry, filename.c_str()))
			entry->failedToLoad = true;
	}
}

static bool ListedFilesExist(const EntryBase* entry, const EntryType* entryType)
{
	for (size_t i = 0; i < entryType->formats.size(); ++i)
	{
		const EntryFormat& format = entryType->formats[i];
		if (!(format.usage & FORMAT_LIST))
			continue;
		string filename = format.MakeFilename(entry->path.c_str());
		if (gEnv->pCryPak->IsFileExist(filename.c_str()))
			return true;
	}
	return false;
}

void ExplorerFileList::OnFileChange(const char* filename, EChangeType eType)
{
	EntryType* entryType = GetEntryTypeByPath(filename);
	const string originalExt = PathUtil::GetExt(filename);
	if (!entryType)
		entryType = GetEntryTypeByExtension(originalExt);

	if (!entryType)
		return;

	bool fileExists = gEnv->pCryPak->IsFileExist(filename, ICryPak::eFileLocation_Any);

	string listedPath = GetCanonicalPath(filename);
	listedPath.replace('\\', '/');
	if (fileExists)
	{

		ExplorerEntryId entryId;

		EntryBase* entry = entryType->entryList->GetBaseByPath(listedPath.c_str());
		if (!entry)
		{

			entryId = AddEntry(listedPath.c_str(), false);

			entry = entryType->entryList->GetBaseByPath(listedPath.c_str());
		}

		if (entry)
		{
			entryId = ExplorerEntryId(m_subtree, entry->id);

			LoadEntryOfType(entry, entryType);
			entry->StoreSavedContent();
			entry->lastContent = entry->savedContent;

			EntryModifiedEvent ev;
			if (entry->Changed(&ev.previousContent))
			{
				ev.subtree = m_subtree;
				ev.id = entry->id;
				ev.reason = "Reload";
				ev.contentChanged = true;
				SignalEntryModified(ev);
			}

			UpdateEntryPakState(entryId, listedPath, entryType);
		}

	}
	else
	{
		EntryBase* entry = entryType->entryList->GetBaseByPath(listedPath.c_str());
		if (entry && !entry->modified && !ListedFilesExist(entry, entryType))
		{
			uint id = entry->id;
			if (entryType->entryList->RemoveById(id))
				SignalEntryRemoved(m_subtree, id);
		}
	}
}

bool ExplorerFileList::HasBackgroundLoading() const
{
	for (size_t i = 0; i < m_entryTypes.size(); ++i)
		for (size_t j = 0; j < m_entryTypes[i].formats.size(); ++j)
			if (m_entryTypes[i].formats[j].path.empty())
				return true;
	return false;
}

bool ExplorerFileList::LoadOrGetChangedEntry(uint id)
{
	const EntryType* entryType = 0;
	if (EntryBase* entry = GetEntry(id, &entryType))
	{
		if (!entry->loaded)
		{
			LoadEntryOfType(entry, entryType);
			entry->StoreSavedContent();
			entry->lastContent = entry->savedContent;
		}
		return true;
	}
	return false;
}

const char* ExplorerFileList::CanonicalExtension() const
{
	const EntryType& entryType = m_entryTypes[0];
	for (size_t i = 0; i < entryType.formats.size(); ++i)
	{
		const EntryFormat& format = entryType.formats[i];
		if (format.usage & FORMAT_MAIN)
			return format.extension.c_str();
	}
	return "";
}

void ExplorerFileList::UpdateEntryPakState(const ExplorerEntryId& id, int state)
{
	if (m_explorerData && m_explorerPakColumn >= 0)
		// Column value is 1 less than enum, skipping "None" state
		m_explorerData->SetEntryColumn(id, m_explorerPakColumn, state - 1, true);
}

void ExplorerFileList::UpdateEntryPakState(const ExplorerEntryId& id, const char* filename, const EntryType* entryType)
{
	if (m_explorerData && m_explorerPakColumn >= 0)
	{
		int pakState = 0;
		if (entryType)
		{
			for (size_t i = 0; i < entryType->formats.size(); ++i)
			{
				const char* extension = entryType->formats[i].extension;
				if (!extension[0])
					continue;

				string assetFile = PathUtil::ReplaceExtension(filename, extension);
				pakState |= GetFilePakState(assetFile);
			}
		}
		else
			pakState |= GetFilePakState(filename);

		m_explorerData->SetEntryColumn(id, m_explorerPakColumn, pakState - 1, true);
	}
}

void ExplorerFileList::GetDependencies(vector<string>* paths, uint id)
{
	const EntryType* format = 0;
	EntryBase* entry = GetEntry(id, &format);
	if (!entry)
		return;
	if (format->dependencyExtractor)
		format->dependencyExtractor->Extract(paths, entry);
}

void ExplorerFileList::ActionSaveAs(ActionContext& x)
{
	if (x.entries.size() != 1)
	{
		return;
	}
	ExplorerEntry* entry = x.entries[0];
	string saveExtension = GetSaveFileExtension(entry->path.c_str());
	if (saveExtension.empty())
	{
		return;
	}
	CEngineFileDialog::RunParams runParams;
	runParams.title = tr("Save As...");
	runParams.initialFile = QString::fromLocal8Bit(entry->path.c_str());
	runParams.extensionFilters << CExtensionFilter(QStringLiteral(".%1 Files").arg(QString::fromLocal8Bit(saveExtension.c_str()).toUpper()), saveExtension.c_str());
	QString fileName = CEngineFileDialog::RunGameSave(runParams, x.window);

	string filename = fileName.toLocal8Bit().data();
	if (filename.empty())
		return;

	Serialization::SStruct existingSerializer;
	GetEntrySerializer(&existingSerializer, entry->id);

	MemoryOArchive oa;
	if (!oa(existingSerializer) || !oa.length())
		return;

	// Note that AddEntry could reset content of entry pointed by
	// "entry" pointer.
	uint newEntryId = AddEntry(filename.c_str(), true).id;

	Serialization::SStruct newSerializer;
	GetEntrySerializer(&newSerializer, newEntryId);

	MemoryIArchive ia;
	if (!ia.open(oa.buffer(), oa.length()) || !ia(newSerializer))
		return;

	EntryBase* newEntry = GetEntry(newEntryId, 0);
	newEntry->loaded = true;

	if (!SaveEntry(x.output, newEntryId))
		return;

	SignalEntrySavedAs(entry->path.c_str(), filename.c_str());
}

void ExplorerFileList::ActionDelete(ActionContext& x)
{
	vector<string> gamePaths;
	for (size_t i = 0; i < x.entries.size(); ++i)
		gamePaths.push_back(x.entries[i]->path);

	vector<string> realPaths;
	vector<int> gamePathByRealPath;

	string gameFolder = gEnv->pCryPak->GetGameFolder();
	for (size_t i = 0; i < gamePaths.size(); ++i)
	{
		const string& gamePath = gamePaths[i];
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

			string path = modPrefix + gamePath;
			path.replace('/', '\\');

			if (QFile::exists(QString::fromLocal8Bit(path.c_str())))
			{
				realPaths.push_back(path);
				gamePathByRealPath.push_back(i);
			}

			modDirectory = gEnv->pCryPak->GetMod(modIndex);
			++modIndex;
		}
		while (modDirectory != 0);
	}

	if (realPaths.empty())
	{
		CQuestionDialog::SWarning(tr("Warning"), tr("Selected assets are stored in paks and can not be removed."));
		return;
	}

	SBatchFileSettings settings;
	settings.title = "Delete Assets";
	settings.descriptionText = "Are you sure you want to delete selected assets?";
	settings.explicitFileList.resize(realPaths.size());
	for (size_t i = 0; i < realPaths.size(); ++i)
		settings.explicitFileList[i] = realPaths[i];
	settings.scanExtension = "";

	Serialization::StringList filenamesToRemove;
	if (ShowBatchFileDialog(&filenamesToRemove, settings, x.window))
	{
		for (size_t i = 0; i < filenamesToRemove.size(); ++i)
		{
			if (QFile::remove(QString::fromLocal8Bit(filenamesToRemove[i])))
			{
				SignalEntryDeleted(gamePaths[gamePathByRealPath[i]].c_str());
			}
			else
			{
				x.output->AddError("Failed to delete file", filenamesToRemove[i].c_str());
			}
		}
	}
}

int ExplorerFileList::GetFilePakState(const char* path)
{
	int result = 0;
	if (gEnv->pCryPak->IsFileExist(path, ICryPak::eFileLocation_OnDisk))
		result |= ICryPak::eFileLocation_OnDisk;
	if (gEnv->pCryPak->IsFileExist(path, ICryPak::eFileLocation_InPak))
		result |= ICryPak::eFileLocation_InPak;
	return result;
}

}

