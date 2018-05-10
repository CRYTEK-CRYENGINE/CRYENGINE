// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SERIALIZESCRIPTTABLEWRITER_H__
#define __SERIALIZESCRIPTTABLEWRITER_H__

#pragma once

#include <CryNetwork/SimpleSerialize.h>
#include <stack>

class CSerializeScriptTableWriterImpl : public CSimpleSerializeImpl<false, eST_Script>
{
public:
	CSerializeScriptTableWriterImpl(SmartScriptTable tbl = SmartScriptTable());

	template<class T>
	void Value(const char* name, const T& value)
	{
		IScriptTable* pTbl = CurTable();
		pTbl->SetValue(name, value);
	}

	void Value(const char* name, SNetObjectID value)
	{
		CRY_ASSERT(false);
	}

	void Value(const char* name, XmlNodeRef ref)
	{
		CRY_ASSERT(false);
	}

	void Value(const char* name, Vec3 value);
	void Value(const char* name, Vec2 value);
	void Value(const char* name, const SSerializeString& value);
	void Value(const char* name, int64 value);
	void Value(const char* name, Quat value);
	void Value(const char* name, uint64 value);
	void Value(const char* name, CTimeValue value);

	template<class T, class P>
	void Value(const char* name, T& value, const P& p)
	{
		Value(name, value);
	}

	bool BeginGroup(const char* szName);
	bool BeginOptionalGroup(const char* szName, bool cond) { return false; }
	void EndGroup();

private:
	std::stack<SmartScriptTable> m_tables;
	IScriptTable*    CurTable() { return m_tables.top().GetPtr(); }
	SmartScriptTable ReuseTable(const char* name);

	IScriptSystem* m_pSS;
};

#endif
