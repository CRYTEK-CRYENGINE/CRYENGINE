// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// TODO pavloi 2016.04.06: hack - somehow Explorer/EntryList.h includes Serialization.h not from EditorCommon, but from CryAction.
// Until I figure out, how to fix it, there is a piece from proper Serialization.h
#include <CrySerialization/yasli/BinArchive.h>
typedef yasli::BinOArchive MemoryOArchive;
typedef yasli::BinIArchive MemoryIArchive;

#include <Explorer/ExplorerDataProvider.h>
#include <Explorer/EntryList.h>

namespace Explorer
{
struct ActionContext;
} // namespace Explorer

class CUqsQueryDocument;
class CUqsEditorContext;

struct SUqsQueryEntry
{
public:
	SUqsQueryEntry();
	~SUqsQueryEntry();

	bool               Load(CUqsEditorContext& editorContext);
	bool               Save();
	void               Delete();
	void               Reset();

	void               Serialize(Serialization::IArchive& ar);

	CUqsQueryDocument* GetDocument() const { return m_pDoc.get(); }
	void               SetDocument(std::unique_ptr<CUqsQueryDocument>&& pDocument);

	string                             m_queryName;
	std::unique_ptr<CUqsQueryDocument> m_pDoc;
};

class CQueryListProvider : public Explorer::IExplorerEntryProvider
{
	Q_OBJECT
public:
	explicit CQueryListProvider(CUqsEditorContext& editorContext);
	~CQueryListProvider();
	void                              Populate();

	Explorer::SEntry<SUqsQueryEntry>* CreateNewQuery(const char* szQueryName, stack_string& outErrorMessage);

	Explorer::SEntry<SUqsQueryEntry>* GetEntryById(uint id) const;

public:
	// Explorer::IExplorerEntryProvider

	virtual void   UpdateEntry(Explorer::ExplorerEntry* pExplorerEntry) override;
	virtual bool   GetEntrySerializer(Serialization::SStruct* out, uint id) const override;
	virtual void   GetEntryActions(std::vector<Explorer::ExplorerAction>* pActions, uint id, Explorer::ExplorerData* pExplorerData) override;
	virtual int    GetEntryCount() const override;
	virtual uint   GetEntryIdByIndex(int index) const override;
	virtual bool   LoadOrGetChangedEntry(uint id) override;
	virtual void   CheckIfModified(uint id, const char* szReason, bool bContinuousChange) override;
	virtual bool   HasBackgroundLoading() const override;
	virtual bool   SaveEntry(Explorer::ActionOutput* output, uint id) override;
	virtual string GetSaveFilename(uint id) override;
	virtual void   RevertEntry(uint id) override;
	virtual bool   SaveAll(Explorer::ActionOutput* output) override;
	virtual void   SetExplorerData(Explorer::ExplorerData* explorerData, int subtree) override;
	//virtual const char* CanonicalExtension() const { return ""; }
	//virtual void GetDependencies(vector<string>* paths, uint id) {}

	// ~Explorer::IExplorerEntryProvider

signals:
	void DocumentAboutToBeRemoved(CUqsQueryDocument* pDocument);

private:

	void ActionDeleteQuery(Explorer::ActionContext& x);

	void RemoveAllNeverSavedQueries();
	void DeleteQueryByEntryId(uint id);

	void UpdateQueryEntryPakState(const Explorer::SEntry<SUqsQueryEntry>& entry);

private:

	Explorer::CEntryList<SUqsQueryEntry>     m_queries;
	CUqsEditorContext&                       m_editorContext;
};
