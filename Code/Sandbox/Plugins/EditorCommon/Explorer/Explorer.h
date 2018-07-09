// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QObject>
#include "Serialization.h"

#include "EditorCommonAPI.h"
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <CryCore/Platform/platform.h>
#include <CryCore/smartptr.h>
#include <CryCore/functor.h>
#include "DependencyManager.h"
#include "UndoStack.h"
#include "EntryList.h"

namespace Explorer
{

struct ActionContext;
class IExplorerEntryProvider;

enum EExplorerEntryType
{
	ENTRY_SUBTREE_ROOT,
	ENTRY_GROUP,
	ENTRY_ASSET,
	ENTRY_LOADING,
	ENTRY_TYPES
};

struct EDITOR_COMMON_API ExplorerEntry : _i_reference_target_t
{
	EExplorerEntryType     type;
	int                    subtree;
	string                 path;
	string                 name;
	bool                   modified;
	uint                   id;
	const char*            icon;

	ExplorerEntry*         parent;
	vector<ExplorerEntry*> children;
	unique_ptr<CUndoStack> history;
	vector<uint>           columnValues;

	ExplorerEntry(int subtree, EExplorerEntryType type, uint id);
	void Serialize(Serialization::IArchive& ar);

	void SetColumnValue(int column, uint value);
	uint GetColumnValue(int column) const;
};

typedef vector<_smart_ptr<ExplorerEntry>>                      ExplorerEntries;
typedef map<string, ExplorerEntry*, stl::less_stricmp<string>> EntriesByPath;
typedef map<uint, ExplorerEntry*>                              EntriesById;

struct FolderSubtree
{
	IExplorerEntryProvider*   provider;
	string                    label;
	_smart_ptr<ExplorerEntry> root;
	ExplorerEntries           entries;
	ExplorerEntries           groups;
	ExplorerEntries           helpers;
	_smart_ptr<ExplorerEntry> loadingEntry;

	EntriesByPath             groupsByPath;
	EntriesByPath             entriesByPath;
	EntriesById               entriesById;
	string                    commonPrefix;

	FolderSubtree(IExplorerEntryProvider* provider_ = 0, const char* label_ = "")
		: provider(provider_), label(label_)
	{}

	void Clear()
	{
		entries.clear();
		groups.clear();
		groupsByPath.clear();
		entriesByPath.clear();
		entriesById.clear();
		commonPrefix.clear();
	}
};

struct ExplorerColumnValue
{
	const char* tooltip;
	const char* icon;
};

struct ExplorerColumn
{
	enum Format
	{
		TEXT,
		INTEGER,
		KILOBYTES,
		ICON
	};

	string                      label;
	bool                        visibleByDefault;
	Format                      format;
	vector<ExplorerColumnValue> values;

	ExplorerColumn() : visibleByDefault(false), format(TEXT) {}
};

struct EntryModifiedEvent;
struct ExplorerAction;
struct ActionOutput;
typedef vector<ExplorerAction> ExplorerActions;
struct ExplorerEntryId;

enum
{
	ENTRY_PART_STATUS_COLUMNS = 1 << 0,
	ENTRY_PART_CONTENT        = 1 << 1
};

struct ExplorerEntryModifyEvent
{
	ExplorerEntry*         entry;
	int                    entryParts;
	bool                   continuousChange;
	vector<ExplorerEntry*> dependentEntries;

	ExplorerEntryModifyEvent()
		: entry()
		, entryParts()
		, continuousChange(false)
	{
	}
};

// ExplorerData aggregates available asset information in a uniform tree
class EDITOR_COMMON_API ExplorerData : public QObject
{
	Q_OBJECT
public:
	explicit ExplorerData(QObject* parent = 0);

	int                           AddProvider(IExplorerEntryProvider* provider, const char* label = "");
	int                           AddColumn(const char* label, ExplorerColumn::Format format, bool visibleByDefault, const ExplorerColumnValue* values = 0, size_t numValues = 0);

	void                          Populate();
	ExplorerEntry*                GetRoot()               { return m_root.get(); }

	int                           GetSubtreeCount() const { return m_subtrees.size(); }
	int                           FindSubtree(const char* label) const;
	const char*                   GetSubtreeLabel(int subtree) const;
	const IExplorerEntryProvider* GetSubtreeProvider(int subtree) const { return m_subtrees[subtree].provider; }

	int                           GetColumnCount() const                { return (int)m_columns.size(); }
	int                           FindColumn(const char* label) const;
	const char*                   GetColumnLabel(int column) const;
	const ExplorerColumn*         GetColumn(int column) const;

	void                          SetEntryTypeIcon(EExplorerEntryType type, const char* icon) { m_entryIcons[type] = icon; }

	bool                          GetSerializerForEntry(Serialization::SStruct* out, ExplorerEntry* entry);
	void                          GetActionsForEntry(ExplorerActions* actions, ExplorerEntry* entry);
	void                          GetCommonActions(ExplorerActions* actions, const ExplorerEntries& entries);
	void                          SetEntryColumn(const ExplorerEntryId& id, int column, uint value, bool notify);

	void                          LoadEntries(const ExplorerEntries& currentEntries);
	void                          CheckIfModified(ExplorerEntry* entry, const char* reason, bool continuousChange) const;
	string                        GetFilePathForEntry(const ExplorerEntry* entry) const;
	bool                          IsEntryAvailableLocally(const ExplorerEntry* entry) const;
	ExplorerEntry*                FindEntryById(const ExplorerEntryId& id) const;
	ExplorerEntry*                FindEntryById(const IExplorerEntryProvider* provider, uint id) const;
	ExplorerEntry*                FindEntryByPath(const IExplorerEntryProvider* provider, const char* path) const;
	void                          FindEntriesByPath(vector<ExplorerEntry*>* entries, const char* path) const;
	void                          GetDependingEntries(vector<ExplorerEntry*>* entries, ExplorerEntry* entry) const;

	const char*                   IconForEntry(const ExplorerEntry* entry) const;

	void                          Revert(ExplorerEntry* entry);
	bool                          SaveEntry(ActionOutput* output, ExplorerEntry*);
	bool                          SaveAll(ActionOutput* output);
	void                          GetUnsavedEntries(ExplorerEntries* unsavedEntries);
	void                          GetSaveFilenames(vector<string>* filenames, const ExplorerEntries& entries) const;
	bool                          HasAtLeastOneUndoAvailable(const ExplorerEntries& explorerEntries);

	void                          UndoInOrder(const ExplorerEntries& entries);
	void                          RedoInOrder(const ExplorerEntries& entries);
	void                          Undo(const ExplorerEntries& entries, int count);
	bool                          CanUndo(const ExplorerEntries& entries) const;
	void                          Redo(const ExplorerEntries& entries);
	bool                          CanRedo(const ExplorerEntries& entries) const;
	void                          GetUndoActions(vector<string>* actionNames, int maxActionCount, const ExplorerEntries& entries) const;

	void                          ActionRevert(ActionContext& x);
	void                          ActionSave(ActionContext& x);
	void                          ActionShowInExplorer(ActionContext& x);

	void                          BeginBatchChange(int subtree) { SignalBeginBatchChange(subtree); }
	void                          EndBatchChange(int subtree)   { SignalEndBatchChange(subtree); }

signals:
	void SignalEntryModified(ExplorerEntryModifyEvent& ev);
	void SignalEntryLoaded(ExplorerEntry* entry);
	void SignalBeginAddEntry(ExplorerEntry* entry);
	void SignalEndAddEntry();
	void SignalBeginRemoveEntry(ExplorerEntry* entry);
	void SignalEndRemoveEntry();
	void SignalEntryImported(ExplorerEntry* entry, ExplorerEntry* oldEntry);
	void SignalRefreshFilter();
	void SignalBeginBatchChange(int subtree);
	void SignalEndBatchChange(int subtree);
	void SignalEntrySavedAs(const char* oldPath, const char* newPath);
	void SignalSelectedEntryClicked(ExplorerEntry* entry);

protected slots:
	void OnProviderSubtreeReset(int subtree);
	void OnProviderSubtreeLoadingFinished(int subtree);
	void OnProviderEntryModified(EntryModifiedEvent&);
	void OnProviderEntryAdded(int subtree, uint id);
	void OnProviderEntryRemoved(int subtree, uint id);
	void OnProviderBeginBatchChange(int subtree);
	void OnProviderEndBatchChange(int subtree);

private:
	void                 EntryModified(ExplorerEntry* entry, bool continuousChange);
	void                 ResetSubtree(FolderSubtree* subtree, int subtreeIndex, const char* text, bool addLoadingEntry);
	void                 CreateGroups(ExplorerEntry* globalRoot, FolderSubtree* subtree, int subtreeIndex);
	void                 UpdateSingleLevelGroups(FolderSubtree* subtree, int subtreeIndex);
	void                 UpdateGroups();
	ExplorerEntry*       CreateGroupsForPath(FolderSubtree* subtree, const char* groupPath, const char* commonPathPrefix);
	void                 RemoveEmptyGroups(FolderSubtree* subtree);
	bool                 UnlinkEntryFromParent(ExplorerEntry* entry);
	void                 RemoveEntry(ExplorerEntry* entry);
	void                 RemoveChildren(ExplorerEntry* entry);
	void                 LinkEntryToParent(ExplorerEntry* parent, ExplorerEntry* entry);
	void                 SetEntryState(ExplorerEntry* entry, const DynArray<char>& state);
	void                 GetEntryState(DynArray<char>* state, ExplorerEntry* entry);
	const FolderSubtree* FindFolderSubtree(const IExplorerEntryProvider* provider) const;

	_smart_ptr<ExplorerEntry>     m_root;

	vector<FolderSubtree>         m_subtrees;
	vector<ExplorerColumn>        m_columns;
	string                        m_entryIcons[ENTRY_TYPES];

	unique_ptr<DependencyManager> m_dependencyManager;

	uint64                        m_undoCounter;
};

enum
{
	ACTION_DISABLED      = 1 << 0,
	ACTION_IMPORTANT     = 1 << 1,
	ACTION_NOT_STACKABLE = 1 << 2
};

struct ActionContext
{
	ActionOutput*   output;
	ExplorerEntries entries;

	QWidget*        window;

	ActionContext()
		: output()
		, window()
	{
	}
};

struct EDITOR_COMMON_API ExplorerAction
{
	const char* icon;
	const char* text;
	const char* description;
	int         flags;

	typedef std::function<void (ActionContext&)> ActionFunction;
	ActionFunction func;

	ExplorerAction()
		: icon("")
		, text("")
		, description("")
		, flags(0)
	{
	}

	ExplorerAction(const char* text, int actionFlags, const ActionFunction& function, const char* icon = "", const char* description = "")
		: text(text)
		, flags(actionFlags)
		, icon(icon)
		, description(description)
		, func(function)
	{
	}

	void Execute(const ExplorerEntries& entries, QWidget* parent0) const;
};

struct EDITOR_COMMON_API ActionOutput
{
	typedef vector<string>          DetailList;
	typedef map<string, DetailList> ErrorToDetails;
	int errorCount;

	ActionOutput() : errorCount(0) {}

	void AddError(const char* error, const char* details)
	{
		errorToDetails[error].push_back(details);
		++errorCount;
	}
	void Show(QWidget* parent) const;

	ErrorToDetails errorToDetails;
};

struct ActionContextAndOutput : ActionContext
{
	ActionOutput _output;

	ActionContextAndOutput()
	{
		output = &_output;
	}
};

class EDITOR_COMMON_API ExplorerActionHandler : public QObject
{
	Q_OBJECT
public:
	ExplorerActionHandler(const ExplorerAction& action)
		: m_action(action)
	{
	}
public slots:
	void OnTriggered()
	{
		if (m_action.func)
			SignalAction(m_action);
	}
signals:
	void SignalAction(const ExplorerAction& action);
private:
	ExplorerAction m_action;
};

}

