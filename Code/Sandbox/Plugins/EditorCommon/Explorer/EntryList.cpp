// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CrySystem/File/ICryPak.h>
#include <CryString/CryPath.h>
#include <Serialization/Decorators/INavigationProvider.h>
#include <CrySerialization/IArchiveHost.h>

#include "EntryList.h"

namespace Explorer
{

void EntryBase::Serialize(Serialization::IArchive& ar)
{
	Serialization::SContext entryContext(ar, this);
	Serialization::SNavigationContext nav;
	nav.path = path;
	Serialization::SContext navContext(ar, &nav);

	SerializeContent(ar);
}

void EntryBase::StoreSavedContent()
{
	Serialization::SaveBinaryBuffer(savedContent, *this);
}

void EntryBase::RestoreSavedContent()
{
	MemoryIArchive ia;
	ia.open(savedContent.data(), savedContent.size());
	Serialize(ia);
}

bool EntryBase::Saved()
{
	StoreSavedContent();
	lastContent = savedContent;
	bool wasModified = modified;
	modified = false;
	return wasModified;
}

bool EntryBase::Changed(DataBuffer* previousContent)
{
	MemoryOArchive content;
	Serialize(content);

	if (content != lastContent)
	{
		previousContent->clear();
		previousContent->swap(lastContent);
		lastContent.assign(content.buffer(), content.buffer() + content.length());
		modified = content != savedContent;
		return true;
	}
	return false;
}

bool EntryBase::Reverted(DataBuffer* previousContent)
{
	bool wasModified = modified;
	if (savedContent != lastContent)
	{
		wasModified = true;
		if (previousContent)
		{
			previousContent->clear();
			previousContent->swap(lastContent);
		}
		lastContent = savedContent;
	}
	modified = false;
	return wasModified;
}

bool EntryBase::Revert(DataBuffer* previousContent)
{
	MemoryOArchive content;
	Serialize(content);
	if (content != savedContent)
	{
		if (previousContent)
		{
			previousContent->assign(content.buffer(), content.buffer() + content.length());
		}
		RestoreSavedContent();
		lastContent = savedContent;
		modified = false;
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------

void EntryListBase::Clear()
{
	m_entries.clear();
	m_entriesById.clear();
	m_entriesByPath.clear();
	m_entriesByName.clear();
}

void EntryListBase::AddEntry(EntryBase* entry)
{
	entry->id = MakeNextId();
	if (entry->name.empty())
		entry->name = PathUtil::GetFile(entry->path);

	m_entries.push_back(unique_ptr<EntryBase>(entry));
	m_entriesById[entry->id] = entry;
	m_entriesByPath[entry->path] = entry;
	m_entriesByName[entry->name] = entry;
}

EntryBase* EntryListBase::GetBaseByIndex(size_t index) const
{
	if (index >= m_entries.size())
		return 0;
	return m_entries[index].get();
}

EntryBase* EntryListBase::GetBaseById(EntryId id) const
{
	EntriesById::const_iterator it = m_entriesById.find(id);
	if (it == m_entriesById.end())
		return 0;
	return it->second;
}

EntryBase* EntryListBase::GetBaseByName(const char* name) const
{
	EntriesByPath::const_iterator it = m_entriesByName.find(name);
	if (it == m_entriesByName.end())
		return 0;
	return it->second;
}

EntryBase* EntryListBase::GetBaseByPath(const char* path) const
{
	EntriesByPath::const_iterator it = m_entriesByPath.find(path);
	if (it == m_entriesByPath.end())
		return 0;
	return it->second;
}

EntryId EntryListBase::MakeNextId()
{
	if (m_idProvider)
		return m_idProvider->MakeNextId();
	else
		return m_nextId++;
}

bool EntryListBase::RemoveById(EntryId id)
{
	EntryBase* entry = GetBaseById(id);
	if (!entry)
		return false;

	m_entriesById.erase(entry->id);
	m_entriesByPath.erase(entry->path);
	m_entriesByName.erase(entry->name);
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		if (m_entries[i].get() == entry)
		{
			m_entries.erase(m_entries.begin() + i);
			--i;
		}
	}
	return true;
}

EntryId EntryListBase::RemoveByPath(const char* path)
{
	EntryId id = 0;

	EntryBase* entry = GetBaseByPath(path);
	if (!entry)
		return false;

	m_entriesById.erase(entry->id);
	m_entriesByPath.erase(entry->path);
	m_entriesByName.erase(entry->name);
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		if (m_entries[i].get() == entry)
		{
			id = entry->id;
			m_entries.erase(m_entries.begin() + i);
			--i;
		}
	}
	return id;
}

}

