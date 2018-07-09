// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Config.h"

#if USE_ARITHSTREAM         // Re-visit. Really this should be used in bit stream version - would definately help bandwidth
	#include "StringTable.h"
	#include <CryCore/StlUtils.h>
	#include "ArithModel.h"

static const uint32 LOG2_NUM_STRING_IDS = 16;
static const uint32 NUM_STRING_IDS = 1 << LOG2_NUM_STRING_IDS;

class CStringTableRep
{
public:
	static CStringTableRepPtr Create()                         { return new CStringTableRep(); }
	static CStringTableRepPtr Create(const CStringTableRep& r) { return new CStringTableRep(r); }

	void                      AddRef()                         { ++m_nRefs; }
	void                      Release()                        { if (--m_nRefs <= 0) DeleteThis(); }

	bool                      IsUnique()                       { return m_nRefs == 1; }

	typedef std::unordered_map<string, uint32, stl::hash_strcmp<string>, stl::hash_strcmp<string>> StrToIdMap;
	typedef std::vector<string>                                                                    StrVec;
	StrToIdMap m_strToId;
	StrVec     m_strings;

private:
	CStringTableRep() : m_nRefs(0) {}
	CStringTableRep(const CStringTableRep& r) : m_nRefs(0), m_strToId(r.m_strToId), m_strings(r.m_strings) {}
	~CStringTableRep() {}

	void DeleteThis() { delete this; }

	int m_nRefs;
};

CStringTable::CStringTable() : m_pRep(CStringTableRep::Create())
{
	string emptyStr;
	m_pRep->m_strToId[emptyStr] = 0;
	m_pRep->m_strings.push_back(emptyStr);
}

CStringTable::~CStringTable()
{
}

CStringTable::CStringTable(const CStringTable& s) : m_pRep(s.m_pRep)
{
}

CStringTable& CStringTable::operator=(const CStringTable& s)
{
	m_pRep = s.m_pRep;
	return *this;
}

void CStringTable::WriteString(CCommOutputStream& out, const string& s, CArithModel* pModel)
{
	CStringTableRep::StrToIdMap::iterator it = m_pRep->m_strToId.find(s);
	bool newString = false;
	if (it == m_pRep->m_strToId.end())
	{
		if (!m_pRep->IsUnique())
			m_pRep = CStringTableRep::Create(*m_pRep);
		NET_ASSERT(m_pRep->IsUnique());
		it = m_pRep->m_strToId.insert(std::make_pair(s, m_pRep->m_strings.size())).first;
		m_pRep->m_strings.push_back(s);
		newString = true;
	}
	NET_ASSERT(it->second < NUM_STRING_IDS);
	out.WriteBits(it->second, LOG2_NUM_STRING_IDS);
	if (newString)
	{
		pModel->WriteString(out, s);
	}
}

void CStringTable::ReadString(CCommInputStream& in, string& s, CArithModel* pModel)
{
	uint32 id = in.ReadBits(LOG2_NUM_STRING_IDS);
	if (id >= m_pRep->m_strings.size() || (id && m_pRep->m_strings[id].empty()))
	{
		if (!m_pRep->IsUnique())
			m_pRep = CStringTableRep::Create(*m_pRep);
		NET_ASSERT(m_pRep->IsUnique());
		pModel->ReadString(in, s);
		m_pRep->m_strToId[s] = id;
		if (id >= m_pRep->m_strings.size())
			m_pRep->m_strings.resize(id + 1);
		m_pRep->m_strings[id] = s;
	}
	else
	{
		s = m_pRep->m_strings[id];
	}
}

#endif
