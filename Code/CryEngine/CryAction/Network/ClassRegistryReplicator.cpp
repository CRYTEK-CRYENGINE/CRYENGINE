// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ClassRegistryReplicator.h"
#include <CryCore/CryCrc32.h>

#if !defined(_RELEASE)
	#define CLASS_DUMP_ENABLED 1
#else
	#define CLASS_DUMP_ENABLED 0
#endif

bool CClassRegistryReplicator::RegisterClassName(const string& name, uint16 id)
{
	if (name.empty())
	{
		GameWarning("Empty class name; ignoring (tried to register at %d)",
		            id);
		return false;
	}

	static const uint16 NotSet = ~uint16(0);
	if (id == NotSet)
	{
		std::map<string, uint16>::iterator iter = m_classNameToId.find(name);
		if (iter != m_classNameToId.end())
			id = iter->second;
		else
			id = m_numClassIds++;
	}

	if (m_numClassIds <= id)
		m_numClassIds = id + 1;
	if (m_classIdToName.size() <= id)
		m_classIdToName.resize(id + 1);

	string overwrote;
	if (!m_classIdToName[id].empty())
	{
		if (m_classIdToName[id] != name)
		{
			GameWarning("Changing index %d of %s due to overwrite by %s", id, m_classIdToName[id].c_str(), name.c_str());
			overwrote = m_classIdToName[id];
			m_classNameToId.erase(name);
		}
		else
		{
			// all is ok... (things match up)
			return true;
		}
	}

	if (m_classNameToId.find(name) != m_classNameToId.end())
	{
		GameWarning("Aliased class name %s detected at id %d and %d",
		            name.c_str(), m_classNameToId[name], id);
		return false;
	}

	m_classIdToName[id] = name;
	m_classNameToId[name] = id;

	if (!overwrote.empty())
	{
		id = NotSet;
		for (std::vector<string>::iterator iter = m_classIdToName.begin(); iter != m_classIdToName.end(); ++iter)
			if (iter->empty())
				id = uint16(iter - m_classIdToName.begin());
		return RegisterClassName(overwrote, id);
	}

	return true;
}

bool CClassRegistryReplicator::ClassIdFromName(uint16& id, const string& name) const
{
	std::map<string, uint16>::const_iterator iter = m_classNameToId.find(name);
	if (iter == m_classNameToId.end())
	{
		id = 0xdead;
		return false;
	}
	id = iter->second;
	return true;
}

bool CClassRegistryReplicator::ClassNameFromId(string& name, uint16 id) const
{
	if (id >= m_classIdToName.size() || m_classIdToName[id].empty())
	{
		name = "<error>";
		return false;
	}
	name = m_classIdToName[id];
	return true;
}

void CClassRegistryReplicator::Reset()
{
	// clear out our class name buffer
	m_numClassIds = 0;
	m_classIdToName.resize(0);
	m_classNameToId.clear();
}

void CClassRegistryReplicator::GetMemoryStatistics(ICrySizer* s) const
{
	s->AddObject(m_classNameToId);
	s->AddObject(m_classIdToName);
}

uint32 CClassRegistryReplicator::GetHash()
{
	CCrc32 crc;
	for (size_t i = 0; i < NumClassIds(); i++)
	{
		string name;
		if (ClassNameFromId(name, static_cast<uint16>(i)))
		{
			crc.AddLowercase(name.c_str(), name.size());
		}
	}
	return crc.Get();
}

void CClassRegistryReplicator::DumpClasses()
{
#if CLASS_DUMP_ENABLED
	CryLogAlways("==========================");
	CryLogAlways("===== CLASS REGISTRY =====");
	CryLogAlways("==========================");
	for (size_t i = 0; i < NumClassIds(); i++)
	{
		string name;
		if (ClassNameFromId(name, static_cast<uint16>(i)))
		{
			CryLogAlways("%" PRISIZE_T ": %s", i, name.c_str());
		}
	}
#endif
}
