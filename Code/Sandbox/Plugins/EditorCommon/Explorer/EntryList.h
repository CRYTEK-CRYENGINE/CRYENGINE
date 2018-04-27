// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "../Serialization.h"
#include <CryCore/Containers/CryArray.h>
#include <map>
#include <vector>
#include <memory>

namespace Explorer
{

using std::map;
using std::vector;
using std::unique_ptr;

typedef uint           EntryId;
typedef DynArray<char> DataBuffer;

inline bool operator!=(const MemoryOArchive& oa, const DataBuffer& buffer)
{
	if (oa.length() != buffer.size())
		return true;
	return memcmp(oa.buffer(), buffer.data(), buffer.size()) != 0;
}

inline bool operator!=(const DataBuffer& lhv, const DataBuffer& rhv)
{
	if (lhv.size() != rhv.size())
		return true;
	return memcmp(lhv.data(), rhv.data(), lhv.size()) != 0;
}

struct EDITOR_COMMON_API EntryBase
{
	EntryId    id;
	string     name;
	string     path;
	bool       loaded;
	bool       modified;
	bool       dataLostDuringLoading;
	bool       failedToLoad;

	DataBuffer lastContent;
	DataBuffer savedContent;

	EntryBase()
		: id()
		, loaded(false)
		, modified(false)
		, dataLostDuringLoading(false)
		, failedToLoad(false)
	{
	}

	virtual ~EntryBase() {}
	virtual void SerializeContent(Serialization::IArchive& ar) = 0;
	virtual void Reset() = 0;

	void         Serialize(Serialization::IArchive& ar);
	void         StoreSavedContent();
	void         RestoreSavedContent();
	bool         Saved();
	bool         Changed(DataBuffer* previousContent);
	bool         Revert(DataBuffer* previousContent);
	bool         Reverted(DataBuffer* previousContent);
};

class EDITOR_COMMON_API EntryListBase
{
public:
	EntryListBase() : m_nextId(1), m_idProvider(0), m_entries(*(new SEntries)) {}
	~EntryListBase() { delete &m_entries; }

	size_t             Count() const { return m_entries.size(); }
	void               Clear();
	void               SaveAll();
	bool               RemoveById(EntryId id);
	EntryId            RemoveByPath(const char* path);

	void               SetIdProvider(EntryListBase* list) { m_idProvider = list; }

	virtual EntryBase* AddEntry(bool* isNewEntry, const char* path, const char* name, bool resetIfExists) = 0;

	EntryBase*         GetBaseById(EntryId id) const;
	EntryBase*         GetBaseByName(const char* name) const;
	EntryBase*         GetBaseByPath(const char* path) const;
	EntryBase*         GetBaseByIndex(size_t index) const;

protected:
	void    AddEntry(EntryBase* entry);
	EntryId MakeNextId();
	EntryListBase* m_idProvider;

	typedef vector<unique_ptr<EntryBase>>                      SEntries;
	SEntries&     m_entries;
	typedef map<string, EntryBase*, stl::less_stricmp<string>> EntriesByPath;
	EntriesByPath m_entriesByPath;
	EntriesByPath m_entriesByName;
	typedef map<int, EntryBase*> EntriesById;
	EntriesById   m_entriesById;
	EntryId       m_nextId;
};

template<class T>
struct SEntry : public EntryBase
{
	T content;

	void SerializeContent(Serialization::IArchive& ar) override
	{
		content.Serialize(ar);
	}
	void Reset() override
	{
		content.Reset();
	}
};

template<class T>
class CEntryList : public EntryListBase
{
public:
	SEntry<T>* AddEntry(bool* isNewEntry, const char* path, const char* name, bool resetIfExists) override
	{
		SEntry<T>* entry = GetByPath(path);
		if (!entry)
		{
			if (isNewEntry)
				*isNewEntry = true;
			entry = new SEntry<T>;
			entry->path = path;
			if (name)
				entry->name = name;
			EntryListBase::AddEntry(entry);
		}
		else if (isNewEntry)
		{
			if (resetIfExists)
				entry->Reset();
			*isNewEntry = false;
		}
		return entry;
	}

	SEntry<T>* GetById(EntryId id) const         { return static_cast<SEntry<T>*>(GetBaseById(id)); }
	SEntry<T>* GetByIndex(size_t index) const    { return static_cast<SEntry<T>*>(GetBaseByIndex(index));  }
	SEntry<T>* GetByName(const char* name) const { return static_cast<SEntry<T>*>(GetBaseByName(name)); }
	SEntry<T>* GetByPath(const char* path) const { return static_cast<SEntry<T>*>(GetBaseByPath(path)); }
};

}

