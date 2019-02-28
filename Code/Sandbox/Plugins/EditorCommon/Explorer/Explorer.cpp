// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Explorer.h"
#include "ExplorerDataProvider.h"
#include "Serialization.h"
#include <CrySerialization/IArchiveHost.h>
#include "IEditor.h"
#include "UndoStack.h"
#include <CryCore/functor.h>
#include "DependencyManager.h"
#include "Controls/QuestionDialog.h"
#include <CrySystem/File/ICryPak.h>

#ifdef WIN32
	#include <Shellapi.h>
#endif

#if 0
	#define TRACE_EXPLORER(fmt, ...) { char buf[1024]; cry_sprintf(buf, fmt "\n", __VA_ARGS__); OutputDebugStringA(buf); }
#else
	#define TRACE_EXPLORER(...)
#endif

namespace Explorer
{

template<class Map>
typename Map::mapped_type find_in_map(Map const& m, typename Map::key_type const& key)
{
	auto it = m.find(key);
	return it != m.end() ? it->second : Map::mapped_type();
}

string GetPath(const char* path)
{
	const char* pathEnd = strrchr(path, '/');
	if (pathEnd == 0)
		return "/";
	return string(path, pathEnd + 1);
}

// ---------------------------------------------------------------------------
bool IExplorerEntryProvider::OwnsEntry(const ExplorerEntry* entry) const
{
	return entry && entry->subtree == m_subtree;
}

bool IExplorerEntryProvider::OwnsAssetEntry(const ExplorerEntry* entry) const
{
	return entry && entry->subtree == m_subtree && entry->type == ENTRY_ASSET;
}

string IExplorerEntryProvider::GetCanonicalPath(const char* path) const
{
	const char* canonicalExtension = CanonicalExtension();
	if (*canonicalExtension)
		return PathUtil::ReplaceExtension(path, canonicalExtension);
	else
		return path;
}

// ---------------------------------------------------------------------------

void ExplorerEntryId::Serialize(Serialization::IArchive& ar)
{
	ar(subtree, "subtree");
	if (ExplorerData* explorerData = ar.context<ExplorerData>())
	{
		string path;
		ExplorerEntry* entry = explorerData->FindEntryById(*this);
		if (entry)
			path = entry->path;
		ar(path, "path");
		if (ar.isInput())
		{
			if (subtree >= 0)
				entry = explorerData->FindEntryByPath(explorerData->GetSubtreeProvider(subtree), path);
			else
				entry = 0;
			if (entry)
				id = entry->id;
			else
				id = 0;
		}
	}
	else
	{
		ar(id, "id");
	}
}

// ---------------------------------------------------------------------------

ExplorerEntry::ExplorerEntry(int subtree, EExplorerEntryType type, uint id)
	: parent(0)
	, type(type)
	, subtree(subtree)
	, modified(false)
	, id(id)
	, icon("")
{
}

void ExplorerEntry::Serialize(Serialization::IArchive& ar)
{
}

void ExplorerEntry::SetColumnValue(int column, uint value)
{
	if (column < 0)
		return;
	if (column >= columnValues.size())
		columnValues.resize(column + 1, 0);
	columnValues[column] = value;
}

uint ExplorerEntry::GetColumnValue(int column) const
{
	if (column < 0)
		return 0;
	if (column >= columnValues.size())
		return 0;
	return columnValues[column];
}

// ---------------------------------------------------------------------------

bool LessCompareExplorerEntryByPath(const ExplorerEntry* a, const ExplorerEntry* b)
{
	if (a->type == b->type)
	{
		return a->path < b->path;
	}
	else
	{
		return int(a->type) < int(b->type);
	}
}

// ---------------------------------------------------------------------------

ExplorerData::ExplorerData(QObject* parent)
	: QObject(parent)
	, m_dependencyManager(new DependencyManager())
	, m_undoCounter(1)
{
	m_root.reset(new ExplorerEntry(-1, ENTRY_GROUP, 0));

	AddColumn("Name", ExplorerColumn::TEXT, true);
}

int ExplorerData::AddProvider(IExplorerEntryProvider* provider, const char* label)
{
	EXPECTED(provider != 0);

	EXPECTED(connect(provider, SIGNAL(SignalSubtreeReset(int)), this, SLOT(OnProviderSubtreeReset(int))));
	EXPECTED(connect(provider, SIGNAL(SignalSubtreeLoadingFinished(int)), this, SLOT(OnProviderSubtreeLoadingFinished(int))));
	EXPECTED(connect(provider, SIGNAL(SignalEntryModified(EntryModifiedEvent &)), this, SLOT(OnProviderEntryModified(EntryModifiedEvent &))));
	EXPECTED(connect(provider, SIGNAL(SignalEntryAdded(int, uint)), this, SLOT(OnProviderEntryAdded(int, uint))));
	EXPECTED(connect(provider, SIGNAL(SignalEntryRemoved(int, uint)), this, SLOT(OnProviderEntryRemoved(int, uint))));
	EXPECTED(connect(provider, &IExplorerEntryProvider::SignalBeginBatchChange, this, &ExplorerData::OnProviderBeginBatchChange));
	EXPECTED(connect(provider, &IExplorerEntryProvider::SignalEndBatchChange, this, &ExplorerData::OnProviderEndBatchChange));
	EXPECTED(connect(provider, &IExplorerEntryProvider::SignalEntrySavedAs, this, &ExplorerData::SignalEntrySavedAs));

	m_subtrees.push_back(FolderSubtree(provider, label));
	provider->SetExplorerData(this, m_subtrees.size() - 1);
	return m_subtrees.size() - 1;
}

int ExplorerData::FindSubtree(const char* label) const
{
	for (size_t i = 0; i < m_subtrees.size(); ++i)
		if (stricmp(m_subtrees[i].label, label) == 0)
			return int(i);
	return -1;
}

int ExplorerData::AddColumn(const char* label, ExplorerColumn::Format format, bool visibleByDefault, const ExplorerColumnValue* values, size_t numValues)
{
	ExplorerColumn column;
	column.label = label;
	column.visibleByDefault = visibleByDefault;
	column.format = format;
	column.values.assign(values, values + numValues);
	m_columns.push_back(column);
	return (int)m_columns.size() - 1;
}

int ExplorerData::FindColumn(const char* label) const
{
	for (size_t i = 0; i < m_columns.size(); ++i)
		if (stricmp(m_columns[i].label, label) == 0)
			return int(i);
	return -1;
}

const char* ExplorerData::GetColumnLabel(int columnIndex) const
{
	const ExplorerColumn* column = GetColumn(columnIndex);
	if (!column)
		return "";
	return column->label.c_str();
}

const ExplorerColumn* ExplorerData::GetColumn(int columnIndex) const
{
	if (columnIndex >= m_columns.size())
		return 0;
	return &m_columns[columnIndex];
}

void ExplorerData::OnProviderEntryModified(EntryModifiedEvent& ev)
{
	ExplorerEntry* entry = FindEntryById(ExplorerEntryId(ev.subtree, ev.id));
	if (entry)
	{
		IExplorerEntryProvider* provider = m_subtrees[entry->subtree].provider;
		if (ev.contentChanged && !ev.continuousChange)
		{
			if (!entry->history)
				entry->history.reset(new CUndoStack());
			entry->history->PushUndo(&ev.previousContent, ev.reason, m_undoCounter);
			++m_undoCounter;
		}
		if (provider && !ev.continuousChange)
			provider->UpdateEntry(entry);

		EntryModified(entry, ev.continuousChange);
		return;
	}
}

void ExplorerData::EntryModified(ExplorerEntry* entry, bool continuousChange)
{
	ExplorerEntryModifyEvent explorerEv;
	explorerEv.entry = entry;
	explorerEv.entryParts = ENTRY_PART_CONTENT;
	explorerEv.continuousChange = continuousChange;

	// TODO: ENTRY_PART_DEPENDENCY
	{
		vector<string> paths;
		IExplorerEntryProvider* provider = m_subtrees[entry->subtree].provider;
		if (provider)
			provider->GetDependencies(&paths, entry->id);
		m_dependencyManager->SetDependencies(entry->path.c_str(), paths);

		vector<string> users;
		m_dependencyManager->FindUsers(&users, entry->path.c_str());
		if (!users.empty())
		{
			for (size_t i = 0; i < users.size(); ++i)
				FindEntriesByPath(&explorerEv.dependentEntries, users[i].c_str());
		}
	}

	SignalEntryModified(explorerEv);
}

void ExplorerData::OnProviderEntryAdded(int subtreeIndex, uint id)
{
	FolderSubtree& subtree = m_subtrees[subtreeIndex];
	IExplorerEntryProvider* provider = subtree.provider;
	if (!provider)
		return;
	_smart_ptr<ExplorerEntry> entry(new ExplorerEntry(subtreeIndex, ENTRY_ASSET, id));
	subtree.entries.push_back(entry);
	subtree.entriesById[id] = entry;

	provider->UpdateEntry(entry);
	subtree.entriesByPath[entry->path] = entry;

	bool singleLevelGroup = false;
	if (singleLevelGroup)
	{
		UpdateSingleLevelGroups(&subtree, subtreeIndex);
	}
	else
	{
		if (strnicmp(entry->path.c_str(), subtree.commonPrefix.c_str(), subtree.commonPrefix.size()) == 0)
		{
			ExplorerEntry* parent = CreateGroupsForPath(&subtree, entry->path.c_str(), subtree.commonPrefix.c_str());
			;
			if (parent)
			{
				LinkEntryToParent(parent, entry);
			}
		}
		else
		{
			CreateGroups(m_root.get(), &subtree, subtreeIndex);
		}
	}
}

void ExplorerData::OnProviderEntryRemoved(int subtree, uint id)
{
	ExplorerEntry* entry = FindEntryById(ExplorerEntryId(subtree, id));
	if (!entry)
		return;

	UnlinkEntryFromParent(entry);
	RemoveEntry(entry);
	RemoveEmptyGroups(&m_subtrees[subtree]);
}

void ExplorerData::OnProviderSubtreeLoadingFinished(int subtreeIndex)
{
	FolderSubtree* subtree = &m_subtrees[subtreeIndex];

	if (subtree->loadingEntry)
	{
		UnlinkEntryFromParent(subtree->loadingEntry);
		subtree->loadingEntry.reset();
	}

	SignalEntryLoaded(subtree->root);
}

void ExplorerData::OnProviderBeginBatchChange(int subtree)
{
	SignalBeginBatchChange(subtree);
}

void ExplorerData::OnProviderEndBatchChange(int subtree)
{
	SignalEndBatchChange(subtree);
}

void ExplorerData::ResetSubtree(FolderSubtree* subtree, int subtreeIndex, const char* text, bool addLoadingEntry)
{
	if (!subtree->root)
	{
		subtree->root.reset(new ExplorerEntry(subtreeIndex, ENTRY_SUBTREE_ROOT, 0));
		LinkEntryToParent(m_root.get(), subtree->root);
	}
	else
		RemoveChildren(subtree->root.get());
	subtree->Clear();
	subtree->root->name = text;

	if (addLoadingEntry)
	{
		subtree->loadingEntry.reset(new ExplorerEntry(subtreeIndex, ENTRY_LOADING, 0));
		subtree->loadingEntry->name = "Loading...";
		LinkEntryToParent(subtree->root, subtree->loadingEntry);
	}
}

void ExplorerData::OnProviderSubtreeReset(int subtreeIndex)
{
	FolderSubtree& subtree = m_subtrees[subtreeIndex];
	IExplorerEntryProvider* provider = subtree.provider;
	ResetSubtree(&subtree, subtreeIndex, subtree.label, provider->HasBackgroundLoading());

	if (!provider)
		return;

	int numEntries = provider->GetEntryCount();
	for (int i = 0; i < numEntries; ++i)
	{
		uint id = provider->GetEntryIdByIndex(i);

		ExplorerEntry* entry = new ExplorerEntry(subtreeIndex, ENTRY_ASSET, id);
		provider->UpdateEntry(entry);

		subtree.entries.push_back(entry);
		subtree.entriesById[entry->id] = entry;
		subtree.entriesByPath[entry->path] = entry;
	}

	CreateGroups(m_root.get(), &subtree, subtreeIndex);

	SignalRefreshFilter();
}

void ExplorerData::Populate()
{
	for (size_t i = 0; i < m_subtrees.size(); ++i)
	{
		OnProviderSubtreeReset(i);
	}
}

void ExplorerData::UpdateSingleLevelGroups(FolderSubtree* subtree, int subtreeIndex)
{
	typedef map<string, _smart_ptr<ExplorerEntry>, stl::less_stricmp<string>> Groups;
	Groups groups;

	for (size_t i = 0; i < subtree->groups.size(); ++i)
	{
		string path = subtree->groups[i]->path.c_str();
		groups[path] = subtree->groups[i];
	}

	for (size_t i = 0; i < subtree->entries.size(); ++i)
	{
		_smart_ptr<ExplorerEntry> entry = subtree->entries[i];

		string path = GetPath(entry->path.c_str());
		_smart_ptr<ExplorerEntry>& group = groups[path];

		if (entry->parent != 0)
			continue;

		if (!group)
		{
			group.reset(new ExplorerEntry(subtreeIndex, ENTRY_GROUP, 0));
			subtree->groups.push_back(group);
			group->path = path;
			group->name = path;
			LinkEntryToParent(subtree->root, group);
		}

		LinkEntryToParent(group, entry);
	}
}

void ExplorerData::RemoveEmptyGroups(FolderSubtree* subtree)
{
	TRACE_EXPLORER("RemoveEmptyGroupBefore before:");
	bool gotNewEmpties = true;
	while (gotNewEmpties)
	{
		gotNewEmpties = false;
		for (int i = (int)subtree->groups.size() - 1; i >= 0; --i)
		{
			ExplorerEntry* group = subtree->groups[i];
			if (group->children.empty())
			{
				if (UnlinkEntryFromParent(group))
					gotNewEmpties = true;

				subtree->groupsByPath.erase(group->path);
				subtree->groups.erase(subtree->groups.begin() + i);
			}
		}
	}
}

static string FindCommonPathPrefix(const FolderSubtree* subtree)
{
	bool firstPath = true;
	string commonPathPrefix;
	for (size_t i = 0; i < subtree->entries.size(); ++i)
	{
		const ExplorerEntry* entry = subtree->entries[i];
		if (entry->path.find('/') == string::npos)
		{
			commonPathPrefix.clear();
			break;
		}

		if (firstPath)
		{
			commonPathPrefix = entry->path;
			string::size_type pos = commonPathPrefix.rfind('/');
			if (pos != string::npos)
				commonPathPrefix.erase(pos + 1, commonPathPrefix.size());
			else
				commonPathPrefix.clear();
			firstPath = false;
			continue;
		}
		else
		{
			if (strnicmp(entry->path.c_str(), commonPathPrefix.c_str(), commonPathPrefix.size()) == 0)
				continue;
		}

		commonPathPrefix.erase(commonPathPrefix.size() - 1);
		while (!commonPathPrefix.empty())
		{
			string::size_type pos = commonPathPrefix.rfind('/');
			if (pos == string::npos)
			{
				commonPathPrefix.clear();
				break;
			}
			else
			{
				commonPathPrefix.erase(pos + 1, commonPathPrefix.size());
				if (strnicmp(entry->path.c_str(), commonPathPrefix.c_str(), commonPathPrefix.size()) == 0)
					break;
				else
					commonPathPrefix.erase(commonPathPrefix.size() - 1);
			}
		}
	}
	return commonPathPrefix;
}

void ExplorerData::CreateGroups(ExplorerEntry* globalRoot, FolderSubtree* subtree, int subtreeIndex)
{
	string commonPathPrefix = FindCommonPathPrefix(subtree);

	if (subtree->commonPrefix != commonPathPrefix)
	{
		// if we change our common path we need to restructure the whole tree
		for (size_t i = 0; i < subtree->entries.size(); ++i)
		{
			ExplorerEntry* entry = subtree->entries[i];
			if (entry->parent)
				UnlinkEntryFromParent(entry);
		}
		RemoveEmptyGroups(subtree);
		EXPECTED(subtree->groups.empty());

		subtree->groups.clear();
		subtree->groupsByPath.clear();
		for (size_t i = 0; i < subtree->entries.size(); ++i)
			if (subtree->entries[i])
				subtree->entries[i]->parent = 0;
	}

	if (subtree->entries.empty())
		return;

	for (size_t i = 0; i < subtree->entries.size(); ++i)
	{
		ExplorerEntry* entry = subtree->entries[i];
		ExplorerEntry* group = CreateGroupsForPath(subtree, entry->path.c_str(), commonPathPrefix.c_str());
		if (group)
		{
			LinkEntryToParent(group, entry);
		}
		else
		{
			entry->parent = 0;
		}
	}
	subtree->commonPrefix = commonPathPrefix;
	subtree->root->name += " (";
	subtree->root->name += commonPathPrefix;
	subtree->root->name += ")";
}

ExplorerEntry* ExplorerData::CreateGroupsForPath(FolderSubtree* subtree, const char* animationPath, const char* commonPathPrefix)
{
	ExplorerEntry* root = subtree->root.get();
	if (!root)
		return 0;
	const char* p = animationPath;
	if (strnicmp(p, commonPathPrefix, strlen(commonPathPrefix)) != 0)
		return root;
	p += strlen(commonPathPrefix);
	if (*p == '\0')
		return 0;

	ExplorerEntry* currentGroup = root;
	while (true)
	{
		string name;
		string groupPath;

		const char* folderName = p;
		const char* folderNameEnd = strchr(p, '/');
		if (folderNameEnd != 0)
		{
			p = folderNameEnd + 1;

			name.assign(folderName, folderNameEnd);
			groupPath.assign(animationPath, folderNameEnd);
		}
		else
		{
			break;
		}

		EntriesByPath::iterator it = subtree->groupsByPath.find(groupPath);
		if (it == subtree->groupsByPath.end())
		{
			ExplorerEntry* newGroup = new ExplorerEntry(currentGroup->subtree, ENTRY_GROUP, 0);
			newGroup->name = name;
			newGroup->path = groupPath;

			subtree->groups.push_back(newGroup);
			subtree->groupsByPath[groupPath] = newGroup;

			LinkEntryToParent(currentGroup, newGroup);

			currentGroup = newGroup;
		}
		else
		{
			currentGroup = it->second;
		}
	}
	return currentGroup;
}

void ExplorerData::LoadEntries(const ExplorerEntries& currentEntries)
{
	for (size_t i = 0; i < currentEntries.size(); ++i)
	{
		ExplorerEntry* entry = currentEntries[i].get();
		if (!entry)
			continue;
		IExplorerEntryProvider* provider = m_subtrees[entry->subtree].provider;
		if (provider)
		{
			provider->LoadOrGetChangedEntry(entry->id);
			vector<string> paths;
			provider->GetDependencies(&paths, entry->id);
			m_dependencyManager->SetDependencies(entry->path.c_str(), paths);
		}
	}
}

void ExplorerData::CheckIfModified(ExplorerEntry* entry, const char* reason, bool continuousChange) const
{
	if (!entry)
		return;

	IExplorerEntryProvider* provider = m_subtrees[entry->subtree].provider;
	if (provider)
		provider->CheckIfModified(entry->id, reason, continuousChange); // can emit SignalProviderEntryModified
}

bool ExplorerData::SaveEntry(ActionOutput* output, ExplorerEntry* entry)
{
	if (!entry)
		return false;

	IExplorerEntryProvider* provider = m_subtrees[entry->subtree].provider;
	if (!provider)
		return false;

	return provider->SaveEntry(output, entry->id);
}

bool ExplorerData::SaveAll(ActionOutput* output)
{
	bool failed = false;
	for (size_t i = 0; i < m_subtrees.size(); ++i)
	{
		IExplorerEntryProvider* provider = m_subtrees[i].provider;
		if (!provider)
			continue;
		if (!provider->SaveAll(output))
			failed = true;
	}
	return !failed;
}

void ExplorerData::Revert(ExplorerEntry* entry)
{
	if (!entry)
		return;

	IExplorerEntryProvider* provider = m_subtrees[entry->subtree].provider;
	if (!provider)
		return;

	provider->RevertEntry(entry->id);
}

bool ExplorerData::GetSerializerForEntry(Serialization::SStruct* out, ExplorerEntry* entry)
{
	if (!entry)
		return false;

	IExplorerEntryProvider* provider = m_subtrees[entry->subtree].provider;
	if (!provider)
		return false;

	if (entry->type == ENTRY_GROUP)
		return false;

	return provider->GetEntrySerializer(out, entry->id);
}

void ExplorerData::GetActionsForEntry(vector<ExplorerAction>* actions, ExplorerEntry* entry)
{
	IExplorerEntryProvider* provider = m_subtrees[entry->subtree].provider;
	if (provider)
	{
		provider->GetEntryActions(actions, entry->id, this);
	}
}

static bool IsSameAction(const ExplorerAction& a, const ExplorerAction& b)
{
	return strcmp(a.text, b.text) == 0 && strcmp(a.icon, b.icon) == 0;
}

static const ExplorerAction* FindActionIn(const ExplorerAction& action, const ExplorerActions& actions)
{
	for (size_t i = 0; i < actions.size(); ++i)
	{
		if (IsSameAction(actions[i], action))
			return &actions[i];
	}
	return 0;
}

static void FilterCommonActions(ExplorerActions* out, const ExplorerActions& in)
{
	for (int i = 0; i < (int)out->size(); ++i)
	{
		ExplorerAction& action = (*out)[i];
		if (!action.func)
			continue; // keep separators
		const ExplorerAction* inAction = FindActionIn(action, in);
		if (!inAction || (action.flags & ACTION_NOT_STACKABLE) != 0 || (inAction->flags & ACTION_NOT_STACKABLE) != 0)
		{
			out->erase(out->begin() + i);
			--i;
			continue;
		}
		else
		{
			if ((inAction->flags & ACTION_DISABLED) == 0)
				action.flags &= ~ACTION_DISABLED;
		}
	}

	bool lastSeparator = true;
	for (int i = 0; i < (int)out->size(); ++i)
	{
		ExplorerAction& action = (*out)[i];
		if (!action.func)
		{
			if (lastSeparator)
			{
				out->erase(out->begin() + i);
				--i;
				lastSeparator = i < 0;
				continue;
			}
			else
			{
				lastSeparator = true;
			}
		}
		else
			lastSeparator = false;
	}

	if (!out->empty() && !out->back().func)
		out->pop_back();
}

void ExplorerData::GetCommonActions(ExplorerActions* actions, const ExplorerEntries& entries)
{
	actions->clear();
	if (entries.empty())
		return;
	GetActionsForEntry(actions, entries[0]);

	ExplorerActions temp;
	for (size_t i = 1; i < entries.size(); ++i)
	{
		temp.clear();
		ExplorerEntry* e = entries[i];
		GetActionsForEntry(&temp, e);

		FilterCommonActions(actions, temp);
	}
}

void ExplorerData::SetEntryColumn(const ExplorerEntryId& id, int column, uint value, bool notify)
{
	ExplorerEntry* entry = FindEntryById(id);
	if (entry)
	{
		entry->SetColumnValue(column, value);
		if (notify)
		{
			ExplorerEntryModifyEvent ev;
			ev.entry = entry;
			ev.entryParts = ENTRY_PART_STATUS_COLUMNS;
			ev.continuousChange = false;
			SignalEntryModified(ev);
		}
	}
}

void ExplorerData::ActionSave(ActionContext& x)
{
	for (size_t i = 0; i < x.entries.size(); ++i)
	{
		ExplorerEntry* entry = x.entries[i];
		int errorCount = x.output ? x.output->errorCount : 0;
		if (!SaveEntry(x.output, entry))
		{
			if (x.output)
			{
				if (errorCount == x.output->errorCount)
					x.output->AddError("Failed to save entries", entry->path.c_str());
			}
		}
	}
}

void ExplorerData::ActionRevert(ActionContext& x)
{
	for (size_t i = 0; i < x.entries.size(); ++i)
		Revert(x.entries[i]);
}

void ExplorerData::ActionShowInExplorer(ActionContext& x)
{
	ExplorerEntry* entry = x.entries.front();
	string fullPath = GetFilePathForEntry(entry);
	string commandLine = "/n,/select,\"";
	commandLine += fullPath;
	commandLine += "\"";
#ifdef WIN32
	ShellExecute(0, "open", "explorer", commandLine, 0, SW_SHOW);
#endif
}

void ExplorerData::RemoveEntry(ExplorerEntry* entry)
{
	FolderSubtree& subtree = m_subtrees[entry->subtree];

	for (size_t i = 0; i < subtree.entries.size(); ++i)
	{
		if (subtree.entries[i] == entry)
		{
			subtree.entriesById.erase(entry->id);
			subtree.entriesByPath.erase(entry->path);
			subtree.entries.erase(subtree.entries.begin() + i);
		}
	}
}

void ExplorerData::RemoveChildren(ExplorerEntry* entry)
{
	int count = entry->children.size();
	for (int i = count - 1; i >= 0; --i)
	{
		RemoveChildren(entry->children[i]);

		SignalBeginRemoveEntry(entry->children[i]);
		entry->children[i]->parent = 0;
		entry->children.erase(entry->children.begin() + i);
		SignalEndRemoveEntry();
	}
}

void ExplorerData::LinkEntryToParent(ExplorerEntry* parent, ExplorerEntry* child)
{
	TRACE_EXPLORER("0x%p Adding '%s->%s'", (void*)child, parent->name, child->name);
	CRY_ASSERT(child->parent == 0);
	child->parent = parent;
	SignalBeginAddEntry(child);
	EXPECTED(stl::push_back_unique(parent->children, child));
	SignalEndAddEntry();
}

bool ExplorerData::UnlinkEntryFromParent(ExplorerEntry* entry)
{
	TRACE_EXPLORER("0x%p Removing '%s' from '%s'", (void*)entry, entry->name.c_str(), entry->parent->name.c_str());
	ExplorerEntry* parent = entry->parent;
	if (!EXPECTED(parent != 0))
		return false;

	bool gotAtLeastOneEmpty = false;
	for (size_t j = 0; j < parent->children.size(); ++j)
	{
		if (parent->children[j] == entry)
		{
			SignalBeginRemoveEntry(entry);
			parent->children.erase(parent->children.begin() + j);
			if (parent->children.empty())
				gotAtLeastOneEmpty = true;
			entry->parent = 0;
			--j;
			SignalEndRemoveEntry();
		}
	}
	return gotAtLeastOneEmpty;
}

string ExplorerData::GetFilePathForEntry(const ExplorerEntry* entry) const
{
	string path = entry->path;
	if (stricmp(PathUtil::GetExt(path.c_str()), "caf") == 0)
	{
		string pathICaf = PathUtil::ReplaceExtension(path, "i_caf");
		if (gEnv->pCryPak->IsFileExist(pathICaf.c_str(), ICryPak::eFileLocation_Any))
			path = pathICaf;
	}

	char fullPathBuffer[ICryPak::g_nMaxPath];
	if (!gEnv->pCryPak->IsFileExist(path.c_str(), ICryPak::eFileLocation_OnDisk))
	{
		FILE* f = gEnv->pCryPak->FOpen(path.c_str(), "rb");
		if (f)
		{
			path = gEnv->pCryPak->GetFileArchivePath(f);
			gEnv->pCryPak->FClose(f);
		}
	}

	string fullPath = gEnv->pCryPak->AdjustFileName(path.c_str(), fullPathBuffer, ICryPak::FLAGS_CHECK_MOD_PATHS | ICryPak::FLAGS_NEVER_IN_PAK);
	if (GetFileAttributes(fullPath.c_str()) == INVALID_FILE_ATTRIBUTES)
		return string();

	return fullPath;
}

void ExplorerData::GetDependingEntries(vector<ExplorerEntry*>* entries, ExplorerEntry* entry) const
{
	vector<string> users;
	m_dependencyManager->FindDepending(&users, entry->path.c_str());

	for (size_t i = 0; i < users.size(); ++i)
		FindEntriesByPath(&*entries, users[i].c_str());
}

const FolderSubtree* ExplorerData::FindFolderSubtree(const IExplorerEntryProvider* provider) const
{
	if (provider)
	{
		for (int subtree = 0; subtree < m_subtrees.size(); ++subtree)
		{
			const FolderSubtree& f = m_subtrees[subtree];
			if (f.provider == provider)
				return &f;
		}
	}
	return 0;
}

ExplorerEntry* ExplorerData::FindEntryById(const IExplorerEntryProvider* provider, uint id) const
{
	const FolderSubtree* folderSubtree = FindFolderSubtree(provider);
	if (!folderSubtree)
		return 0;
	return find_in_map(folderSubtree->entriesById, id);
}

ExplorerEntry* ExplorerData::FindEntryById(const ExplorerEntryId& id) const
{
	if ((size_t)id.subtree >= m_subtrees.size())
		return 0;
	const FolderSubtree& folderSubtree = m_subtrees[id.subtree];
	return find_in_map(folderSubtree.entriesById, id.id);
}

ExplorerEntry* ExplorerData::FindEntryByPath(const IExplorerEntryProvider* provider, const char* path) const
{
	const FolderSubtree* folder = FindFolderSubtree(provider);
	if (!folder)
		return 0;
	return find_in_map(folder->entriesByPath, path);
}

void ExplorerData::FindEntriesByPath(vector<ExplorerEntry*>* entries, const char* path) const
{
	for (int subtree = 0; subtree < m_subtrees.size(); ++subtree)
	{
		const FolderSubtree& f = m_subtrees[subtree];
		EntriesByPath::const_iterator it = f.entriesByPath.find(path);
		if (it != f.entriesByPath.end())
			entries->push_back(it->second);
	}
}

const char* ExplorerData::IconForEntry(const ExplorerEntry* entry) const
{
	if (!entry)
		return "";
	if (entry->icon && entry->icon[0])
		return entry->icon;
	return m_entryIcons[entry->type];
}

void ExplorerData::Undo(const ExplorerEntries& entries, int count)
{
	for (size_t i = 0; i < entries.size(); ++i)
	{
		ExplorerEntry* entry = entries[i];
		if (!entry->history)
			continue;
		DynArray<char> currentState;
		GetEntryState(&currentState, entry);
		DynArray<char> newState;
		if (entry->history->Undo(&newState, currentState, count, ++m_undoCounter))
		{
			SetEntryState(entry, newState);
		}

		EntryModified(entry, false);
	}
}

void ExplorerData::UndoInOrder(const ExplorerEntries& entries)
{
	int index = -1;
	uint64 newestUndo = 0;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		ExplorerEntry* entry = entries[i];
		if (!entry->history)
			continue;
		uint64 lastUndo = entry->history->NewestUndoIndex();
		if (lastUndo > newestUndo)
		{
			index = i;
			newestUndo = lastUndo;
		}
	}

	if (index >= 0)
	{
		ExplorerEntries singleEntry(1, entries[index]);
		Undo(singleEntry, 1);
	}
}

void ExplorerData::RedoInOrder(const ExplorerEntries& entries)
{
	int index = -1;
	uint64 newestRedo = 0;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		ExplorerEntry* entry = entries[i];
		if (!entry->history)
			continue;
		uint64 lastRedo = entry->history->NewestRedoIndex();
		if (lastRedo > newestRedo)
		{
			index = i;
			newestRedo = lastRedo;
		}
	}

	if (index >= 0)
	{
		ExplorerEntries singleEntry(1, entries[index]);
		Redo(singleEntry);
	}
}

bool ExplorerData::CanUndo(const ExplorerEntries& entries) const
{
	if (entries.empty())
		return false;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		ExplorerEntry* entry = entries[i];
		if (entry->history && entry->history->HasUndo())
			return true;
	}
	return false;
}

void ExplorerData::Redo(const ExplorerEntries& entries)
{
	for (size_t i = 0; i < entries.size(); ++i)
	{
		ExplorerEntry* entry = entries[i];
		if (!entry->history)
			continue;

		DynArray<char> currentState;
		GetEntryState(&currentState, entry);
		DynArray<char> newState;
		if (entry->history->Redo(&newState, currentState, ++m_undoCounter))
			SetEntryState(entry, newState);

		EntryModified(entry, false);
	}
}

bool ExplorerData::CanRedo(const ExplorerEntries& entries) const
{
	if (entries.empty())
		return false;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		ExplorerEntry* entry = entries[i];
		if (entry->history && entry->history->HasRedo())
			return true;
	}
	return false;
}

void ExplorerData::GetUndoActions(vector<string>* actionNames, int maxActionCount, const ExplorerEntries& entries) const
{
	actionNames->clear();
	if (entries.size() > 1)
	{
		if (CanUndo(entries))
		{
			actionNames->push_back("[Change in multiple entries]");
		}
	}
	else if (entries.size() == 1)
	{
		if (entries[0]->history)
		{
			entries[0]->history->GetUndoActions(actionNames, maxActionCount);
		}
	}
}

void ExplorerData::SetEntryState(ExplorerEntry* entry, const DynArray<char>& state)
{
	if (state.empty())
		return;

	IExplorerEntryProvider* provider = m_subtrees[entry->subtree].provider;

	Serialization::SStruct ser;
	if (!provider->GetEntrySerializer(&ser, entry->id))
		return;

	gEnv->pSystem->GetArchiveHost()->LoadBinaryBuffer(ser, state.data(), state.size());

	provider->CheckIfModified(entry->id, 0, false);
}

void ExplorerData::GetEntryState(DynArray<char>* state, ExplorerEntry* entry)
{
	state->clear();
	IExplorerEntryProvider* provider = m_subtrees[entry->subtree].provider;

	Serialization::SStruct ser;
	if (!provider->GetEntrySerializer(&ser, entry->id))
		return;

	// TODO: change vector<char> to DynArray
	DynArray<char> temp;
	gEnv->pSystem->GetArchiveHost()->SaveBinaryBuffer(temp, ser);
	state->assign(temp.begin(), temp.end());
}

void ExplorerData::GetUnsavedEntries(ExplorerEntries* unsavedEntries)
{
	for (int subtreeIndex = 0; subtreeIndex < m_subtrees.size(); ++subtreeIndex)
	{
		FolderSubtree& subtree = m_subtrees[subtreeIndex];
		for (size_t entryIndex = 0; entryIndex < subtree.entries.size(); ++entryIndex)
		{
			ExplorerEntry* entry = subtree.entries[entryIndex];
			if (!entry)
				continue;
			if (entry->modified)
				unsavedEntries->push_back(entry);
		}
	}
}

void ExplorerData::GetSaveFilenames(vector<string>* filenames, const ExplorerEntries& entries) const
{
	filenames->resize(entries.size());

	for (size_t i = 0; i < entries.size(); ++i)
	{
		ExplorerEntry* entry = entries[i];
		if (!entry)
			continue;

		IExplorerEntryProvider* provider = m_subtrees[entry->subtree].provider;
		if (!provider)
			continue;

		(*filenames)[i] = provider->GetSaveFilename(entry->id);
	}
}

void ExplorerAction::Execute(const ExplorerEntries& entries, QWidget* parent) const
{
	if (!EXPECTED(func))
		return;

	ActionContextAndOutput x;
	x.window = parent;
	x.entries = entries;

	func(x);

	if (!x.output->errorToDetails.empty())
	{
		x.output->Show(parent);
	}
}

void ActionOutput::Show(QWidget* parent) const
{
	string message;
	ActionOutput::ErrorToDetails::const_iterator it;
	for (it = errorToDetails.begin(); it != errorToDetails.end(); ++it)
	{
		message += it->first;
		message += ":\n";
		const ActionOutput::DetailList& details = it->second;
		for (size_t i = 0; i < details.size(); ++i)
		{
			message += "    ";
			message += details[i];
			message += "\n";
		}

		CQuestionDialog::SWarning(QObject::tr("Warning"), QObject::tr(QString::fromLocal8Bit(message).toStdString().c_str()));
	}
}

}

