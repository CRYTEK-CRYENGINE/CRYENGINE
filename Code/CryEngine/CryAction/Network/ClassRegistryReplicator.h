// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CLASSREGISTRYREPLICATOR_H__
#define __CLASSREGISTRYREPLICATOR_H__

#pragma once

class CClassRegistryReplicator
{
public:
	CClassRegistryReplicator() { Reset(); }

	bool   RegisterClassName(const string& name, uint16 id);
	bool   ClassNameFromId(string& name, uint16 id) const;
	bool   ClassIdFromName(uint16& id, const string& name) const;
	size_t NumClassIds() { return m_classIdToName.size(); }
	void   Reset();
	uint32 GetHash();
	void   DumpClasses();

	void   GetMemoryStatistics(ICrySizer* s) const;

private:
	uint16                   m_numClassIds;
	std::vector<string>      m_classIdToName;
	std::map<string, uint16> m_classNameToId;
};

#endif
