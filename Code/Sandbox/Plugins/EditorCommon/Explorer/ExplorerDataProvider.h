// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QObject>
#include <vector>
#include "EditorCommonAPI.h"
#include "Serialization.h"

namespace Explorer
{

using std::vector;
struct ExplorerEntry;
struct ExplorerAction;
struct ActionOutput;
class ExplorerData;

struct EDITOR_COMMON_API ExplorerEntryId
{
	int  subtree;
	uint id;

	ExplorerEntryId(int subtree, uint id) : subtree(subtree), id(id)  {}
	ExplorerEntryId() : subtree(-1), id(0)  {}

	bool operator==(const ExplorerEntryId& rhs) const { return subtree == rhs.subtree && id == rhs.id; }

	void Serialize(Serialization::IArchive& ar);
};

struct EntryModifiedEvent
{
	int            subtree;
	uint           id;
	const char*    reason;
	bool           contentChanged;
	bool           continuousChange;
	DynArray<char> previousContent;

	EntryModifiedEvent()
		: subtree(-1)
		, id(0)
		, reason("")
		, contentChanged(false)
		, continuousChange(false)
	{
	}
};

class EDITOR_COMMON_API IExplorerEntryProvider : public QObject
{
	Q_OBJECT

public:
	IExplorerEntryProvider()
		: m_subtree(-1) {}

	virtual void        UpdateEntry(ExplorerEntry* entry) = 0;
	virtual bool        GetEntrySerializer(Serialization::SStruct* out, uint id) const = 0;
	virtual void        GetEntryActions(vector<ExplorerAction>* actions, uint id, ExplorerData* explorerData) = 0;

	virtual int         GetEntryCount() const = 0;
	virtual uint        GetEntryIdByIndex(int index) const = 0;
	virtual bool        LoadOrGetChangedEntry(uint id) { return true; }
	virtual void        CheckIfModified(uint id, const char* reason, bool continuous) = 0;
	virtual bool        HasBackgroundLoading() const   { return false; }

	virtual bool        SaveEntry(ActionOutput* output, uint id) = 0;
	virtual string      GetSaveFilename(uint id) = 0;
	virtual void        RevertEntry(uint id) = 0;
	virtual bool        SaveAll(ActionOutput* output) = 0;
	virtual void        SetExplorerData(ExplorerData* explorerData, int subtree) {}
	virtual const char* CanonicalExtension() const                               { return ""; }
	virtual void        GetDependencies(vector<string>* paths, uint id)          {}

	void                SetSubtree(int subtree)                                  { m_subtree = subtree; }
	bool                OwnsEntry(const ExplorerEntry* entry) const;
	bool                OwnsAssetEntry(const ExplorerEntry* entry) const;

	string              GetCanonicalPath(const char* path) const;

signals:
	void SignalSubtreeReset(int subtree);
	void SignalEntryModified(EntryModifiedEvent& ev);
	void SignalEntryAdded(int subtree, uint id);
	void SignalEntryRemoved(int subtree, uint id);
	void SignalEntrySavedAs(const char* oldName, const char* newName);
	void SignalEntry(int subtree, uint id);
	void SignalSubtreeLoadingFinished(int subtree);
	void SignalBeginBatchChange(int subtree);
	void SignalEndBatchChange(int subtree);

protected:
	int m_subtree;
};

}

