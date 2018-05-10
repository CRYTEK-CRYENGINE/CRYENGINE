// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryListProvider.h"

#include <Explorer/Explorer.h>

#include "Document.h"

//////////////////////////////////////////////////////////////////////////
// CUqsQueryEntry
//////////////////////////////////////////////////////////////////////////

SUqsQueryEntry::SUqsQueryEntry() {}
SUqsQueryEntry::~SUqsQueryEntry() {}

bool SUqsQueryEntry::Load(CUqsEditorContext& editorContext)
{
	if (!m_pDoc)
	{
		m_pDoc.reset(new CUqsQueryDocument(editorContext));
	}

	if (m_pDoc)
	{
		return m_pDoc->Load(m_queryName);
	}

	return false;
}

bool SUqsQueryEntry::Save()
{
	if (!m_pDoc)
	{
		return false;
	}

	return m_pDoc->Save();
}

void SUqsQueryEntry::Delete()
{
	if (m_pDoc)
	{
		m_pDoc->Delete();
		m_pDoc.reset();
	}
}

void SUqsQueryEntry::Reset()
{
	CRY_ASSERT_MESSAGE(false, "not implmented - reset is normally not called, but might be called in SaveAs");
}

void SUqsQueryEntry::Serialize(Serialization::IArchive& ar)
{
	if (m_pDoc)
	{
		ar(*m_pDoc, "doc", "Doc");
	}
}

void SUqsQueryEntry::SetDocument(std::unique_ptr<CUqsQueryDocument>&& pDocument)
{
	m_pDoc = std::move(pDocument);
	if (m_pDoc)
	{
		m_queryName = m_pDoc->GetName();
	}
	else
	{
		m_queryName.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
// CQueryListProvider
//////////////////////////////////////////////////////////////////////////

CQueryListProvider::CQueryListProvider(CUqsEditorContext& editorContext)
	: m_queries()
	, m_editorContext(editorContext)
{}

CQueryListProvider::~CQueryListProvider()
{
	RemoveAllNeverSavedQueries();
}

struct SListQueriesVisitor : public UQS::DataSource::IEditorLibraryProvider::IListQueriesVisitor
{
	SListQueriesVisitor(Explorer::CEntryList<SUqsQueryEntry>& entries)
		: m_entries(entries)
	{}

	void Visit(const char* szQueryName, const char* szQueryFilePath)
	{
		string lastPartOfName = PathUtil::GetFile(szQueryName);

		Explorer::SEntry<SUqsQueryEntry>* pEntry = m_entries.AddEntry(nullptr, szQueryName, lastPartOfName, false);
		pEntry->content.m_queryName = szQueryName;
	}

	Explorer::CEntryList<SUqsQueryEntry>& m_entries;
};

void CQueryListProvider::Populate()
{
	m_queries.Clear();

	if (UQS::Core::IHub* pHub = UQS::Core::IHubPlugin::GetHubPtr())
	{
		if (UQS::DataSource::IEditorLibraryProvider* pProvider = pHub->GetEditorLibraryProvider())
		{
			SListQueriesVisitor visitor(m_queries);
			pProvider->GetQueriesList(visitor);
		}
	}
}

Explorer::SEntry<SUqsQueryEntry>* CQueryListProvider::CreateNewQuery(const char* szQueryName, stack_string& outErrorMessage)
{
	if (m_queries.GetByPath(szQueryName))
	{
		outErrorMessage = "Query with such name is already loaded in editor.";
		return nullptr;
	}

	std::unique_ptr<CUqsQueryDocument> pDocument(new CUqsQueryDocument(m_editorContext));
	if (!pDocument->CreateNew(szQueryName, outErrorMessage))
	{
		return nullptr;
	}

	const string& actualQueryName = pDocument->GetName();
	string lastPartOfName = PathUtil::GetFile(actualQueryName);

	bool bIsNewEntry = true;
	Explorer::SEntry<SUqsQueryEntry>* pEntry = m_queries.AddEntry(&bIsNewEntry, actualQueryName, lastPartOfName, false);

	if (!(pEntry && bIsNewEntry))
	{
		pDocument->Delete();
		outErrorMessage = "Editor was unable to add created query into the list of queries.";
		return nullptr;
	}

	pEntry->content.SetDocument(std::move(pDocument));
	pEntry->loaded = true;
	pEntry->failedToLoad = false;
	pEntry->modified = true;
	SignalEntryAdded(m_subtree, pEntry->id);

	return pEntry;
}

Explorer::SEntry<SUqsQueryEntry>* CQueryListProvider::GetEntryById(uint id) const
{
	return m_queries.GetById(id);
}

void CQueryListProvider::UpdateEntry(Explorer::ExplorerEntry* pExplorerEntry)
{
	if (Explorer::SEntry<SUqsQueryEntry>* pEntry = GetEntryById(pExplorerEntry->id))
	{
		pExplorerEntry->name = pEntry->name;
		pExplorerEntry->path = pEntry->path;
		pExplorerEntry->modified = pEntry->modified;
		//pExplorerEntry->icon =
	}
}

bool CQueryListProvider::GetEntrySerializer(Serialization::SStruct* out, uint id) const
{
	// TODO pavloi 2016.04.07: same as ExplorerFileList::CheckIfModified and AnimationList::CheckIfModified
	if (Explorer::SEntry<SUqsQueryEntry>* pEntry = GetEntryById(id))
	{
		*out = Serialization::SStruct(*pEntry);
		return true;
	}
	return false;
}

void CQueryListProvider::GetEntryActions(std::vector<Explorer::ExplorerAction>* pActions, uint id, Explorer::ExplorerData* pExplorerData)
{
	if (Explorer::SEntry<SUqsQueryEntry>* pEntry = GetEntryById(id))
	{
		pActions->push_back(Explorer::ExplorerAction("Revert", 0,
		                                             [=](Explorer::ActionContext& x) { pExplorerData->ActionRevert(x); },
		                                             "icons:General/History_Undo.ico",
		                                             "Reload query content from the disk, undoing all changes since last save."));

		pActions->push_back(Explorer::ExplorerAction("Save", 0,
		                                             [=](Explorer::ActionContext& x) { pExplorerData->ActionSave(x); },
		                                             "icons:General/File_Save.ico"));

		pActions->push_back(Explorer::ExplorerAction("Delete", 0,
		                                             [=](Explorer::ActionContext& x) { ActionDeleteQuery(x); },
		                                             "icons:General/Element_Remove.ico",
		                                             "Remove query"));

		pActions->push_back(Explorer::ExplorerAction("Show in Explorer", Explorer::ACTION_NOT_STACKABLE,
		                                             [=](Explorer::ActionContext& x) { pExplorerData->ActionShowInExplorer(x); },
		                                             "icons:General/Show_In_Explorer.ico",
		                                             "Locates file in Windows Explorer."));
	}
}

int CQueryListProvider::GetEntryCount() const
{
	return (int)m_queries.Count();
}

uint CQueryListProvider::GetEntryIdByIndex(int index) const
{
	// TODO pavloi 2016.04.07: same as AnimationList::CheckIfModified
	if (Explorer::SEntry<SUqsQueryEntry>* pEntry = m_queries.GetByIndex(index))
		return pEntry->id;
	return 0;
}

bool CQueryListProvider::LoadOrGetChangedEntry(uint id)
{
	if (Explorer::SEntry<SUqsQueryEntry>* pEntry = GetEntryById(id))
	{
		if (!pEntry->loaded)
		{
			pEntry->loaded = true;
			pEntry->failedToLoad = !pEntry->content.Load(m_editorContext);

			pEntry->StoreSavedContent();
			pEntry->lastContent = pEntry->savedContent;
		}
		return true;
	}
	return false;
}

void CQueryListProvider::CheckIfModified(uint id, const char* szReason, bool bContinuousChange)
{
	// TODO pavloi 2016.04.07: same as ExplorerFileList::CheckIfModified and AnimationList::CheckIfModified
	if (Explorer::SEntry<SUqsQueryEntry>* pEntry = GetEntryById(id))
	{
		Explorer::EntryModifiedEvent ev;
		ev.continuousChange = bContinuousChange;
		if (bContinuousChange || pEntry->Changed(&ev.previousContent))
		{
			ev.subtree = m_subtree;
			ev.id = pEntry->id;
			if (szReason)
			{
				ev.reason = szReason;
				ev.contentChanged = true;
			}
			SignalEntryModified(ev);
		}
	}
}

bool CQueryListProvider::HasBackgroundLoading() const { return false; }

bool CQueryListProvider::SaveEntry(Explorer::ActionOutput* pOutput, uint id)
{
	if (Explorer::SEntry<SUqsQueryEntry>* pEntry = GetEntryById(id))
	{
		if (!pEntry->loaded)
		{
			CryLogAlways("Query is not loaded to be saved");
			return false;
		}

		if (!pEntry->content.Save())
		{
			if (pOutput)
			{
				pOutput->AddError("Failed to save file", pEntry->path.c_str());
			}
			return false;
		}

		if (pEntry->Saved())
		{
			Explorer::EntryModifiedEvent ev;
			ev.subtree = m_subtree;
			ev.id = id;
			SignalEntryModified(ev);
		}

		UpdateQueryEntryPakState(*pEntry);
		return true;
	}
	return false;
}

string CQueryListProvider::GetSaveFilename(uint id)
{
	if (Explorer::SEntry<SUqsQueryEntry>* pEntry = GetEntryById(id))
	{
		return pEntry->path;
	}
	return string();
}

void CQueryListProvider::RevertEntry(uint id)
{
	// TODO pavloi 2016.04.07: same as ExplorerFileList::CheckIfModified and AnimationList::CheckIfModified
	if (Explorer::SEntry<SUqsQueryEntry>* pEntry = GetEntryById(id))
	{
		pEntry->failedToLoad = !pEntry->content.Load(m_editorContext);
		pEntry->StoreSavedContent();

		Explorer::EntryModifiedEvent ev;
		if (pEntry->Reverted(&ev.previousContent))
		{
			using namespace Explorer; // TODO pavloi 2016.04.07: for opertor!= DataBuffer

			ev.subtree = m_subtree;
			ev.id = pEntry->id;
			ev.contentChanged = ev.previousContent != pEntry->lastContent;
			ev.reason = "Revert";
			SignalEntryModified(ev);
		}

		UpdateQueryEntryPakState(*pEntry);
	}
}

bool CQueryListProvider::SaveAll(Explorer::ActionOutput* output)
{
	// TODO pavloi 2016.04.07: same as ExplorerFileList::CheckIfModified and AnimationList::CheckIfModified
	bool bFailed = false;
	for (size_t i = 0; i < m_queries.Count(); ++i)
	{
		Explorer::SEntry<SUqsQueryEntry>* pEntry = m_queries.GetByIndex(i);
		if (pEntry->modified)
		{
			if (!SaveEntry(output, pEntry->id))
			{
				bFailed = true;
				// TODO pavloi 2016.04.07: use output->AddError
			}
		}
	}
	return !bFailed;
}

void CQueryListProvider::ActionDeleteQuery(Explorer::ActionContext& x)
{
	for (auto& entry : x.entries)
	{
		DeleteQueryByEntryId(entry->id);
	}
}

void CQueryListProvider::RemoveAllNeverSavedQueries()
{
	const size_t queriesCount = m_queries.Count();
	for (size_t idx = 0; idx < queriesCount; ++idx)
	{
		if (Explorer::SEntry<SUqsQueryEntry>* pEntry = m_queries.GetByIndex(idx))
		{
			if (CUqsQueryDocument* pDoc = pEntry->content.GetDocument())
			{
				if (pDoc->IsNeverSaved())
				{
					DocumentAboutToBeRemoved(pDoc);
					pDoc->Delete();
					// TODO pavloi 2016.07.01: no signal about deletion, fine for now
				}
			}
		}
	}
}

void CQueryListProvider::DeleteQueryByEntryId(uint id)
{
	if (Explorer::SEntry<SUqsQueryEntry>* pEntry = m_queries.GetById(id))
	{
		if (pEntry->content.GetDocument())
		{
			DocumentAboutToBeRemoved(pEntry->content.GetDocument());
		}

		pEntry->content.Delete();

		if (m_queries.RemoveById(id))
		{
			SignalEntryRemoved(m_subtree, id);
		}
	}
}

void CQueryListProvider::SetExplorerData(Explorer::ExplorerData* explorerData, int subtree)
{
	m_subtree = subtree;
	// TODO pavloi 2016.04.07: add columns
}

void CQueryListProvider::UpdateQueryEntryPakState(const Explorer::SEntry<SUqsQueryEntry>& entry)
{
	// TODO pavloi 2016.04.08: add support for pak state

	//int pakState = GetAnimationPakState(entry->path.c_str());
	//m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, id), m_explorerColumnPak, pakState, true);
	//m_system->explorerData->SetEntryColumn(ExplorerEntryId(m_subtree, id), m_explorerColumnNew, IsNewAnimation(id), true);
}
