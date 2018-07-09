// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SERIALIZESCRIPTTABLEREADER_H__
#define __SERIALIZESCRIPTTABLEREADER_H__

#pragma once

#include <CryNetwork/SimpleSerialize.h>
#include <stack>

class CSerializeScriptTableReaderImpl : public CSimpleSerializeImpl<true, eST_Script>
{
public:
	CSerializeScriptTableReaderImpl(SmartScriptTable tbl);

	template<class T>
	void Value(const char* name, T& value)
	{
		IScriptTable* pTbl = CurTable();
		if (pTbl)
			if (pTbl->HaveValue(name))
				if (!pTbl->GetValue(name, value))
				{
					Failed();
					GameWarning("Failed to read %s", name);
				}
	}

	void Value(const char* name, EntityId& value);

	void Value(const char* name, SNetObjectID& value)
	{
		CRY_ASSERT(false);
	}

	void Value(const char* name, XmlNodeRef& value)
	{
		CRY_ASSERT(false);
	}

	void Value(const char* name, Quat& value);
	void Value(const char* name, ScriptAnyValue& value);
	void Value(const char* name, SSerializeString& value);
	void Value(const char* name, uint16& value);
	void Value(const char* name, uint64& value);
	void Value(const char* name, int16& value);
	void Value(const char* name, int64& value);
	void Value(const char* name, uint8& value);
	void Value(const char* name, int8& value);
	void Value(const char* name, Vec2& value);
	void Value(const char* name, CTimeValue& value);

	template<class T, class P>
	void Value(const char* name, T& value, const P& p)
	{
		Value(name, value);
	}

	bool BeginGroup(const char* szName);
	bool BeginOptionalGroup(const char* szName, bool cond) { return false; }
	void EndGroup();

private:
	template<class T, class U>
	void NumValue(const char* name, U& value);

	int                          m_nSkip;
	std::stack<SmartScriptTable> m_tables;

	IScriptTable* CurTable()
	{
		if (m_nSkip)
			return 0;
		else
			return m_tables.top();
	}
};

#endif
