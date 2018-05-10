// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SerializeXMLReader.h"
#include <CrySystem/ISystem.h>
#include <CryEntitySystem/IEntitySystem.h>

#define TAG_SCRIPT_VALUE "v"
#define TAG_SCRIPT_TYPE  "t"
#define TAG_SCRIPT_NAME  "n"

//#define LOG_SERIALIZE_STACK(tag,szName) CryLogAlways( "<%s> %s/%s",tag,GetStackInfo(),szName );
#define LOG_SERIALIZE_STACK(tag, szName)

CSerializeXMLReaderImpl::CSerializeXMLReaderImpl(const XmlNodeRef& nodeRef) : m_nErrors(0)
{
	//m_curTime = gEnv->pTimer->GetFrameStartTime();
	assert(!!nodeRef);
	m_nodeStack.push_back(CParseState());
	m_nodeStack.back().Init(nodeRef);
}

bool CSerializeXMLReaderImpl::Value(const char* name, ScriptAnyValue& value)
{
	DefaultValue(value); // Set input value to default.
	if (m_nErrors)
		return false;

	XmlStrCmpFunc pPrevCmpFunc = g_pXmlStrCmp;
	g_pXmlStrCmp = &strcmp; // Do case-sensitive compare

	XmlNodeRef nodeRef = NextOf(name);
	if (!nodeRef)
	{
		g_pXmlStrCmp = pPrevCmpFunc;
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Unable to read attribute %s (invalid type?)", name);
		Failed();
		return false;
	}
	else if (!ReadScript(nodeRef, value))
		Failed();
	g_pXmlStrCmp = pPrevCmpFunc;
	return true;
}

bool CSerializeXMLReaderImpl::ReadScript(XmlNodeRef nodeRef, ScriptAnyValue& value)
{
	if (m_nErrors)
		return false;
	const char* type = nodeRef->getAttr(TAG_SCRIPT_TYPE);
	bool convertOk = false;
	if (0 == strcmp(type, "nil"))
	{
		value = ScriptAnyValue();
		convertOk = true;
	}
	else if (0 == strcmp(type, "b"))
	{
		bool x = false;
		convertOk = nodeRef->getAttr(TAG_SCRIPT_VALUE, x);
		value = ScriptAnyValue(x);
	}
	else if (0 == strcmp(type, "h"))
	{
		ScriptHandle x;

		// We are always reading uint64 for handles. In x86, this is a bit inefficient. But when dealing
		//	with writing in x86 and reading in x64 (or vice versa), this ensures we don't have to guess which DWORD
		//	to cast from when reading (especially for Endian).
		uint64 handle = 0;
		convertOk = nodeRef->getAttr(TAG_SCRIPT_VALUE, handle);
		x.n = (UINT_PTR)handle;

		value = ScriptAnyValue(x);
	}
	else if (0 == strcmp(type, "n"))
	{
		float x;
		convertOk = nodeRef->getAttr(TAG_SCRIPT_VALUE, x);
		value = ScriptAnyValue(x);
	}
	else if (0 == strcmp(type, "s"))
	{
		convertOk = nodeRef->haveAttr(TAG_SCRIPT_VALUE);
		value = ScriptAnyValue(nodeRef->getAttr(TAG_SCRIPT_VALUE));
	}
	else if (0 == strcmp(type, "vec"))
	{
		Vec3 x;
		convertOk = nodeRef->getAttr(TAG_SCRIPT_VALUE, x);
		value = ScriptAnyValue(x);
	}
	else if (0 == strcmp(type, "entityId"))
	{
		EntityId id;
		SmartScriptTable tbl;
		if (nodeRef->getAttr(TAG_SCRIPT_VALUE, id))
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
				tbl = pEntity->GetScriptTable();
		convertOk = tbl.GetPtr() != 0;
		if (convertOk)
			value = ScriptAnyValue(tbl);
	}
	else if (0 == strcmp(type, "table"))
	{
		int arrayCount = 0;
		if (nodeRef->haveAttr("count"))
		{
			nodeRef->getAttr("count", arrayCount);
			int childCount = nodeRef->getChildCount();
			int arrayIndex = 0;
			SmartScriptTable tbl;
			if (value.GetType() == EScriptAnyType::Table && value.GetScriptTable())
				tbl = value.GetScriptTable();
			else
				tbl = SmartScriptTable(gEnv->pScriptSystem);
			int nCount = min(arrayCount, childCount);
			for (int i = 0; i < nCount; i++)
			{
				XmlNodeRef childRef = nodeRef->getChild(i);
				ScriptAnyValue elemValue;
				++arrayIndex;
				tbl->GetAtAny(arrayIndex, elemValue);
				if (!ReadScript(childRef, elemValue))
					return false;
				tbl->SetAtAny(arrayIndex, elemValue);
			}
			while (arrayIndex <= arrayCount)
				tbl->SetNullAt(++arrayIndex);
			value = SmartScriptTable(tbl);
			convertOk = true;
		}
		else
		{
			int childCount = nodeRef->getChildCount();
			SmartScriptTable tbl;
			if (value.GetType() == EScriptAnyType::Table && value.GetScriptTable())
				tbl = value.GetScriptTable();
			else
				tbl = SmartScriptTable(gEnv->pScriptSystem);
			for (int i = 0; i < childCount; i++)
			{
				XmlNodeRef childRef = nodeRef->getChild(i);
				ScriptAnyValue elemValue;
				const char* elemName = childRef->getAttr(TAG_SCRIPT_NAME);
				tbl->GetValueAny(elemName, elemValue);
				if (!ReadScript(childRef, elemValue))
					return false;
				tbl->SetValueAny(elemName, elemValue);
			}
			value = SmartScriptTable(tbl);
			convertOk = true;
		}
	}
	else
	{
		//CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"Unknown script type '%s' for value '%s'", type, value);
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Unknown script type '%s'", type);
		return false;
	}
	if (!convertOk)
	{
		if (nodeRef->haveAttr(TAG_SCRIPT_VALUE))
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Couldn't convert '%s' to type '%s'", nodeRef->getAttr(TAG_SCRIPT_VALUE), type);
		else
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "No 'value' tag for value of type '%s'", type);
		return false;
	}
	return true;
}

bool CSerializeXMLReaderImpl::Value(const char* name, int8& value)
{
	DefaultValue(value); // Set input value to default.
	if (m_nErrors)
		return false;
	int temp;
	bool bResult = Value(name, temp);
	if (temp < -128 || temp > 127)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Attribute %s is out of range (%d)", name, temp);
		Failed();
		bResult = false;
	}
	else
		value = temp;
	return bResult;
}

bool CSerializeXMLReaderImpl::Value(const char* name, string& value)
{
	DefaultValue(value); // Set input value to default.
	if (m_nErrors)
		return false;
	if (!CurNode()->haveAttr(name))
	{
		//CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"No such attribute %s (invalid type?)", name);
		//Failed();
		return false;
	}
	else
		value = CurNode()->getAttr(name);
	return true;
}

bool CSerializeXMLReaderImpl::Value(const char* name, CTimeValue& value)
{
	DefaultValue(value); // Set input value to default.
	if (m_nErrors)
		return false;
	XmlNodeRef nodeRef = CurNode();
	if (!nodeRef)
		return false;
	if (0 == strcmp("zero", nodeRef->getAttr(name)))
		value = CTimeValue(0.0f);
	else
	{
		float delta;
		if (!GetAttr(nodeRef, name, delta))
		{
			//CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"Failed to read time value %s", name);
			//Failed();
			value = gEnv->pTimer->GetFrameStartTime(); // in case we don't find the node, it was assumed to be the default value (0.0)
			// 0.0 means current time, whereas "zero" really means CTimeValue(0.0), see above
			return false;
		}
		else
		{
			value = CTimeValue(gEnv->pTimer->GetFrameStartTime() + delta);
		}
	}
	return true;
}

bool CSerializeXMLReaderImpl::Value(const char* name, XmlNodeRef& value)
{
	DefaultValue(value); // Set input value to default.
	if (m_nErrors)
		return false;

	if (BeginOptionalGroup(name, true))
	{
		value = CurNode()->getChild(0);
		EndGroup();
	}

	return true;
}

void CSerializeXMLReaderImpl::BeginGroup(const char* szName)
{
	if (m_nErrors)
	{
		//CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"BeginGroup %s called on non-existant group", szName);
		m_nErrors++;
	}
	else if (XmlNodeRef node = NextOf(szName))
	{
		m_nodeStack.push_back(CParseState());
		m_nodeStack.back().Init(node);
		LOG_SERIALIZE_STACK("BeginGroup:ok", szName);
	}
	else
	{
		LOG_SERIALIZE_STACK("BeginGroup:fail", szName);
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "!BeginGroup( %s ) not found", szName);
		m_nErrors++;
	}
}

bool CSerializeXMLReaderImpl::BeginOptionalGroup(const char* szName, bool condition)
{
	if (m_nErrors)
	{
		//CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"BeginOptionalGroup %s called on non-existant group", szName);
		m_nErrors++;
	}
	else if (XmlNodeRef node = NextOf(szName))
	{
		m_nodeStack.push_back(CParseState());
		m_nodeStack.back().Init(node);
		LOG_SERIALIZE_STACK("BeginOptionalGroup:ok", szName);
		return true;
	}
	LOG_SERIALIZE_STACK("BeginOptionalGroup:fail", szName);
	return false;
}

void CSerializeXMLReaderImpl::EndGroup()
{
	if (m_nErrors)
		m_nErrors--;
	else
	{
		LOG_SERIALIZE_STACK("EndGroup", "");
		m_nodeStack.pop_back();
	}
	assert(!m_nodeStack.empty());
}

//////////////////////////////////////////////////////////////////////////
const char* CSerializeXMLReaderImpl::GetStackInfo() const
{
	static string str;
	str.assign("");
	for (int i = 0; i < (int)m_nodeStack.size(); i++)
	{
		const char* name = m_nodeStack[i].m_node->getAttr(TAG_SCRIPT_NAME);
		if (name && name[0])
			str += name;
		else
			str += m_nodeStack[i].m_node->getTag();
		if (i != m_nodeStack.size() - 1)
			str += "/";
	}
	return str.c_str();
}

void CSerializeXMLReaderImpl::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddContainer(m_nodeStack);
}
