// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SerializeScriptTableWriter.h"

CSerializeScriptTableWriterImpl::CSerializeScriptTableWriterImpl(SmartScriptTable tbl)
{
	m_pSS = gEnv->pScriptSystem;
	if (!tbl)
		tbl = SmartScriptTable(m_pSS);
	m_tables.push(tbl);
}

void CSerializeScriptTableWriterImpl::Value(const char* name, Vec3 value)
{
	SmartScriptTable newTable = ReuseTable(name);
	newTable->SetValue("x", value.x);
	newTable->SetValue("y", value.y);
	newTable->SetValue("z", value.z);
}

void CSerializeScriptTableWriterImpl::Value(const char* name, Vec2 value)
{
	SmartScriptTable newTable = ReuseTable(name);
	newTable->SetValue("x", value.x);
	newTable->SetValue("y", value.y);
}

void CSerializeScriptTableWriterImpl::Value(const char* name, const SSerializeString& value)
{
	IScriptTable* pTbl = CurTable();
	pTbl->SetValue(name, value.c_str());
}

void CSerializeScriptTableWriterImpl::Value(const char* name, int64 value)
{
	// ignored, silently
}

void CSerializeScriptTableWriterImpl::Value(const char* name, Quat value)
{
	SmartScriptTable newTable = ReuseTable(name);
	newTable->SetValue("w", value.w);
	newTable->SetValue("x", value.v.x);
	newTable->SetValue("y", value.v.y);
	newTable->SetValue("z", value.v.z);
}

void CSerializeScriptTableWriterImpl::Value(const char* name, uint64 value)
{
	// ignored, silently
}

void CSerializeScriptTableWriterImpl::Value(const char* name, CTimeValue value)
{
	Value(name, value.GetSeconds());
}

bool CSerializeScriptTableWriterImpl::BeginGroup(const char* szName)
{
	SmartScriptTable newTable = ReuseTable(szName);
	m_tables.push(newTable);
	return true;
}

void CSerializeScriptTableWriterImpl::EndGroup()
{
	m_tables.pop();
}

SmartScriptTable CSerializeScriptTableWriterImpl::ReuseTable(const char* szName)
{
	IScriptTable* pTbl = CurTable();
	ScriptAnyValue curVal;
	pTbl->GetValueAny(szName, curVal);
	SmartScriptTable newTable;
	if (curVal.GetType() == EScriptAnyType::Table)
	{
		newTable = curVal.GetScriptTable();
	}
	else
	{
		newTable = SmartScriptTable(m_pSS);
		pTbl->SetValue(szName, newTable);
	}
	return newTable;
}
