// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SerializeScriptTableReader.h"

CSerializeScriptTableReaderImpl::CSerializeScriptTableReaderImpl(SmartScriptTable tbl)
{
	m_nSkip = 0;
	m_tables.push(tbl);
}

template<class T, class U>
void CSerializeScriptTableReaderImpl::NumValue(const char* name, U& value)
{
	T temp;
	IScriptTable* pTbl = CurTable();
	if (pTbl)
		if (pTbl->HaveValue(name))
			if (!pTbl->GetValue(name, temp))
			{
				Failed();
				GameWarning("Failed to read %s", name);
			}
			else
				value = temp;
}

void CSerializeScriptTableReaderImpl::Value(const char* name, EntityId& value)
{
	// values of type entity id might be an unsigned int, or might be a script handle
	IScriptTable* pTbl = CurTable();
	if (pTbl)
		if (pTbl->HaveValue(name))
			if (!pTbl->GetValue(name, value))
			{
				ScriptHandle temp;
				if (!pTbl->GetValue(name, temp))
				{
					Failed();
					GameWarning("Failed to read %s", name);
				}
				else
					value = (EntityId)temp.n;
			}
}

void CSerializeScriptTableReaderImpl::Value(const char* name, CTimeValue& value)
{
	NumValue<float>(name, value);
}

void CSerializeScriptTableReaderImpl::Value(const char* name, Vec2& value)
{
	SmartScriptTable temp;
	IScriptTable* pTbl = CurTable();
	Vec2 tempValue;
	if (pTbl)
		if (pTbl->HaveValue(name))
			if (!pTbl->GetValue(name, temp) || !temp->GetValue("x", tempValue.x) || !temp->GetValue("y", tempValue.y))
			{
				Failed();
				GameWarning("Failed to read %s", name);
			}
			else
				value = tempValue;
}

void CSerializeScriptTableReaderImpl::Value(const char* name, Quat& value)
{
	SmartScriptTable temp;
	IScriptTable* pTbl = CurTable();
	Quat tempValue;
	if (pTbl)
		if (pTbl->HaveValue(name))
			if (!pTbl->GetValue(name, temp) || !temp->GetValue("x", tempValue.v.x) || !temp->GetValue("y", tempValue.v.y) || !temp->GetValue("z", tempValue.v.z) || !temp->GetValue("w", tempValue.w))
			{
				Failed();
				GameWarning("Failed to read %s", name);
			}
			else
				value = tempValue;
}

void CSerializeScriptTableReaderImpl::Value(const char* name, ScriptAnyValue& value)
{
	ScriptAnyValue temp;
	IScriptTable* pTbl = CurTable();
	if (pTbl)
		if (pTbl->GetValueAny(name, temp))
			if (temp.GetType() != EScriptAnyType::Nil)
				value = temp;
}

void CSerializeScriptTableReaderImpl::Value(const char* name, uint8& value)
{
	NumValue<int>(name, value);
}

void CSerializeScriptTableReaderImpl::Value(const char* name, int8& value)
{
	NumValue<int>(name, value);
}

void CSerializeScriptTableReaderImpl::Value(const char* name, uint16& value)
{
	NumValue<int>(name, value);
}

void CSerializeScriptTableReaderImpl::Value(const char* name, int16& value)
{
	NumValue<int>(name, value);
}

void CSerializeScriptTableReaderImpl::Value(const char* name, uint64& value)
{
	NumValue<int>(name, value);
}

void CSerializeScriptTableReaderImpl::Value(const char* name, int64& value)
{
	NumValue<int>(name, value);
}

void CSerializeScriptTableReaderImpl::Value(const char* name, SSerializeString& str)
{
	const char* tempstr = str.c_str();
	NumValue<const char*>(name, tempstr);
}

bool CSerializeScriptTableReaderImpl::BeginGroup(const char* szName)
{
	IScriptTable* pTbl = CurTable();
	if (!pTbl)
		m_nSkip++;
	else
	{
		ScriptAnyValue val;
		pTbl->GetValueAny(szName, val);
		switch (val.GetType())
		{
		case EScriptAnyType::Table:
			m_tables.push(val.GetScriptTable());
			break;
		default:
			GameWarning("Expected %s to be a table, but it wasn't", szName);
			Failed();
		case EScriptAnyType::Nil:
			m_nSkip++;
			break;
		}
	}
	return true;
}

void CSerializeScriptTableReaderImpl::EndGroup()
{
	if (m_nSkip)
		--m_nSkip;
	else
		m_tables.pop();
}
