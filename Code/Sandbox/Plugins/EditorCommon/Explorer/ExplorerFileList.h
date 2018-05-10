// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ExplorerDataProvider.h"
#include "EntryList.h"
#include <CrySystem/File/IFileChangeMonitor.h>
#include <CryCore/smartptr.h>

namespace Explorer
{

struct ActionContext;

struct IEntryLoader : _i_reference_target_t
{
	virtual ~IEntryLoader() {}
	virtual bool Load(EntryBase* entry, const char* filename) = 0;
	virtual bool Save(EntryBase* entry, const char* filename) = 0;
};

struct IEntryDependencyExtractor : _i_reference_target_t
{
	virtual ~IEntryDependencyExtractor() {}
	virtual void Extract(vector<string>* paths, const EntryBase* entry) = 0;
};

struct EDITOR_COMMON_API JSONLoader : IEntryLoader
{
	bool Load(EntryBase* entry, const char* filename) override;
	bool Save(EntryBase* entry, const char* filename) override;
};

template<class TSelf>
struct SelfLoader : IEntryLoader
{
	bool Load(EntryBase* entry, const char* filename) override
	{
		return ((SEntry<TSelf>*)entry)->content.Load();
	}
	bool Save(EntryBase* entry, const char* filename) override
	{
		return ((SEntry<TSelf>*)entry)->content.Save();
	}
};

template<class T>
struct CEntryLoader : IEntryLoader
{
	bool Load(EntryBase* entry, const char* filename) override
	{
		return ((SEntry<T>*)entry)->content.Load(filename);
	}
	bool Save(EntryBase* entry, const char* filename) override
	{
		return ((SEntry<T>*)entry)->content.Save(filename);
	}
};

struct ScanLoadedFile;

enum FormatUsage
{
	FORMAT_LOAD = 1 << 0,
	FORMAT_SAVE = 1 << 1,
	FORMAT_LIST = 1 << 2,
	FORMAT_MAIN = 1 << 3
};

struct EntryFormat
{
	string                   extension;
	string                   path;
	int                      usage;
	_smart_ptr<IEntryLoader> loader;

	string                   MakeFilename(const char* entryPath) const;

	EntryFormat()
		: usage()
	{
	}
};
typedef vector<EntryFormat> EntryFormats;

struct EntryType
{
	EntryListBase*                        entryList;
	bool                                  fileListenerRegistered;
	_smart_ptr<IEntryDependencyExtractor> dependencyExtractor;
	EntryFormats                          formats;
	EntryType() : entryList(nullptr), fileListenerRegistered(false) {}

	EntryType& AddFormat(const char* extension, IEntryLoader* loader, int usage = FORMAT_LIST | FORMAT_LOAD | FORMAT_SAVE)
	{
		EntryFormat format;
		format.extension = extension;
		format.usage = usage;
		format.loader.reset(loader);
		formats.push_back(format);
		return *this;
	}
	EntryType& AddFile(const char* path)
	{
		EntryFormat format;
		format.path = path;
		format.usage = FORMAT_LIST | FORMAT_LOAD | FORMAT_SAVE;
		formats.push_back(format);
		return *this;
	}
};

class EDITOR_COMMON_API ExplorerFileList : public IExplorerEntryProvider, public IFileChangeListener
{
	Q_OBJECT
public:
	ExplorerFileList() : m_explorerData(0), m_addPakColumn(false), m_explorerPakColumn(-1), m_dataIcon("") {}
	~ExplorerFileList();

	void                         AddPakColumn()                { m_addPakColumn = true; }
	void                         SetDataIcon(const char* icon) { m_dataIcon = icon; }

	template<class T> EntryType& AddEntryType(IEntryDependencyExtractor* extractor = 0)
	{
		return AddEntryType(new CEntryList<T>(), extractor);
	}
	template<class T> SEntry<T>* AddSingleFile(const char* path, const char* label, IEntryLoader* loader)
	{
		EntryType* f = AddSingleFileEntryType(new CEntryList<T>, path, label, loader);
		return ((CEntryList<T>*)f->entryList)->AddEntry(0, path, label, false);
	}

	ExplorerEntryId              AddEntry(const char* filename, bool resetIfExists);
	bool                         AddAndSaveEntry(const char* filename);
	EntryBase*                   GetEntryBaseByPath(const char* path);
	EntryBase*                   GetEntryBaseById(uint id);
	template<class T> SEntry<T>* GetEntryByPath(const char* path) { return static_cast<SEntry<T>*>(GetEntryBaseByPath(path)); }
	template<class T> SEntry<T>* GetEntryById(uint id)            { return static_cast<SEntry<T>*>(GetEntryBaseById(id)); }
	void                         Populate();

	// IExplorerDataProvider:
	int         GetEntryCount() const override;
	uint        GetEntryIdByIndex(int index) const override;
	bool        GetEntrySerializer(Serialization::SStruct* out, uint id) const override;
	void        UpdateEntry(ExplorerEntry* entry) override;
	void        RevertEntry(uint id) override;
	string      GetSaveFilename(uint id) override;

	bool        SaveEntry(ActionOutput* output, uint id) override;
	bool        SaveAll(ActionOutput* output) override;
	void        SetExplorerData(ExplorerData* explorerData, int subtree) override;
	void        CheckIfModified(uint id, const char* reason, bool continuousChange) override;
	void        GetEntryActions(vector<ExplorerAction>* actions, uint id, ExplorerData* explorerData) override;
	bool        HasBackgroundLoading() const override;
	bool        LoadOrGetChangedEntry(uint id) override;
	const char* CanonicalExtension() const override;
	void        GetDependencies(vector<string>* paths, uint id) override;
	// ^^^

	void       OnFileChange(const char* filename, EChangeType eType) override;

	static int GetFilePakState(const char* path);

public slots:
	void             OnBackgroundFileLoaded(const ScanLoadedFile&);
	void             OnBackgroundLoadingFinished();
signals:
	void             SignalEntryDeleted(const char* path);
private:
	string           GetSaveFileExtension(const char* entryPath) const;
	void             ActionSaveAs(ActionContext& x);
	void             ActionDelete(ActionContext& x);
	bool             CanBeSaved(const char* path) const;

	EntryBase*       GetEntry(uint id, const EntryType** format = 0) const;
	EntryType*       GetEntryTypeByExtension(const char* ext);
	const EntryType* GetEntryTypeByExtension(const char* ext) const;
	EntryType*       GetEntryTypeByPath(const char* singleFile);
	const EntryType* GetEntryTypeByPath(const char* singleFile) const;

	EntryType& AddEntryType(EntryListBase* list, IEntryDependencyExtractor* extractor);
	EntryType* AddSingleFileEntryType(EntryListBase* list, const char* path, const char* label, IEntryLoader* loader);

	void       UpdateEntryPakState(const ExplorerEntryId& id, int pakState);
	void       UpdateEntryPakState(const ExplorerEntryId& id, const char* filename, const EntryType* format = 0);

	vector<EntryType> m_entryTypes;
	ExplorerData*     m_explorerData;
	bool              m_addPakColumn;
	int               m_explorerPakColumn;
	const char*       m_dataIcon;
};

}

