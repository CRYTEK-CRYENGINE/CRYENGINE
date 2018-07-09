// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SerializeReaderXMLCPBin.h"
#include <CrySystem/ISystem.h>
#include <CryEntitySystem/IEntitySystem.h>

static const size_t MAX_NODE_STACK_DEPTH = 40;

#define TAG_SCRIPT_VALUE "v"
#define TAG_SCRIPT_TYPE  "t"
#define TAG_SCRIPT_NAME  "n"

CSerializeReaderXMLCPBin::CSerializeReaderXMLCPBin(XMLCPB::CNodeLiveReaderRef nodeRef, XMLCPB::CReaderInterface& binReader)
	: m_nErrors(0)
	, m_binReader(binReader)
{
	//m_curTime = gEnv->pTimer->GetFrameStartTime();
	assert(nodeRef.IsValid());
	m_nodeStack.reserve(MAX_NODE_STACK_DEPTH);
	m_nodeStack.push_back(nodeRef);
}

//////////////////////////////////////////////////////////////////////////

bool CSerializeReaderXMLCPBin::Value(const char* name, ScriptAnyValue& value)
{
	DefaultValue(value); // Set input value to default.
	if (m_nErrors)
		return false;

	XMLCPB::CNodeLiveReaderRef nodeRef = CurNode()->GetChildNode(name);
	if (!nodeRef.IsValid())
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Unable to read attribute %s (invalid type?)", name);
		Failed();
		return false;
	}
	else if (!ReadScript(nodeRef, value))
		Failed();
	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CSerializeReaderXMLCPBin::ReadScript(XMLCPB::CNodeLiveReaderRef nodeRef, ScriptAnyValue& value)
{
	if (m_nErrors)
		return false;
	const char* type;
	nodeRef->ReadAttr(TAG_SCRIPT_TYPE, type);
	bool convertOk = false;
	if (0 == strcmp(type, "nil"))
	{
		value = ScriptAnyValue();
		convertOk = true;
	}
	else if (0 == strcmp(type, "b"))
	{
		bool x = false;
		convertOk = nodeRef->ReadAttr(TAG_SCRIPT_VALUE, x);
		value = ScriptAnyValue(x);
	}
	else if (0 == strcmp(type, "h"))
	{
		ScriptHandle x;

		// We are always reading uint64 for handles. In x86, this is a bit inefficient. But when dealing
		//	with writing in x86 and reading in x64 (or vice versa), this ensures we don't have to guess which DWORD
		//	to cast from when reading (especially for Endian).
		uint64 handle = 0;
		convertOk = nodeRef->ReadAttr(TAG_SCRIPT_VALUE, handle);
		x.n = (UINT_PTR)handle;

		value = ScriptAnyValue(x);
	}
	else if (0 == strcmp(type, "n"))
	{
		float x;
		convertOk = nodeRef->ReadAttr(TAG_SCRIPT_VALUE, x);
		value = ScriptAnyValue(x);
	}
	else if (0 == strcmp(type, "s"))
	{
		convertOk = nodeRef->HaveAttr(TAG_SCRIPT_VALUE);
		const char* pVal = NULL;
		nodeRef->ReadAttr(TAG_SCRIPT_VALUE, pVal);
		value = ScriptAnyValue(pVal);
	}
	else if (0 == strcmp(type, "vec"))
	{
		Vec3 x;
		convertOk = nodeRef->ReadAttr(TAG_SCRIPT_VALUE, x);
		value = ScriptAnyValue(x);
	}
	else if (0 == strcmp(type, "entityId"))
	{
		EntityId id;
		SmartScriptTable tbl;
		if (nodeRef->ReadAttr(TAG_SCRIPT_VALUE, id))
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
				tbl = pEntity->GetScriptTable();
		convertOk = tbl.GetPtr() != 0;
		if (convertOk)
			value = ScriptAnyValue(tbl);
	}
	else if (0 == strcmp(type, "table"))
	{
		if (nodeRef->HaveAttr("count"))
		{
			int arrayCount = 0;
			nodeRef->ReadAttr("count", arrayCount);
			int childCount = nodeRef->GetNumChildren();
			int arrayIndex = 0;
			SmartScriptTable tbl;
			if (value.GetType() == EScriptAnyType::Table && value.GetScriptTable())
				tbl = value.GetScriptTable();
			else
				tbl = SmartScriptTable(gEnv->pScriptSystem);
			int nCount = min(arrayCount, childCount);
			for (int i = 0; i < nCount; i++)
			{
				XMLCPB::CNodeLiveReaderRef childRef = nodeRef->GetChildNode(i);
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
			int childCount = nodeRef->GetNumChildren();
			SmartScriptTable tbl;
			if (value.GetType() == EScriptAnyType::Table && value.GetScriptTable())
				tbl = value.GetScriptTable();
			else
				tbl = SmartScriptTable(gEnv->pScriptSystem);
			for (int i = 0; i < childCount; i++)
			{
				XMLCPB::CNodeLiveReaderRef childRef = nodeRef->GetChildNode(i);
				ScriptAnyValue elemValue;
				const char* elemName = NULL;
				childRef->ReadAttr(TAG_SCRIPT_NAME, elemName);
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
		if (nodeRef->HaveAttr(TAG_SCRIPT_VALUE))
		{
			string val;
			nodeRef->ReadAttrAsString(TAG_SCRIPT_VALUE, val);
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Couldn't convert '%s' to type '%s'", val.c_str(), type);
		}
		else
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "No 'value' tag for value of type '%s'", type);
		return false;
	}
	return true;
}

bool CSerializeReaderXMLCPBin::Value(const char* name, int8& value)
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

bool CSerializeReaderXMLCPBin::Value(const char* name, string& value)
{
	DefaultValue(value); // Set input value to default.
	if (m_nErrors)
		return false;
	if (!CurNode()->HaveAttr(name))
	{
		// the attrs are not saved if they already had the default value
		return false;
	}
	else
	{
		const char* pVal = NULL;
		CurNode()->ReadAttr(name, pVal);
		value = pVal;
	}
	return true;
}

bool CSerializeReaderXMLCPBin::Value(const char* name, CTimeValue& value)
{
	DefaultValue(value); // Set input value to default.
	if (m_nErrors)
		return false;
	XMLCPB::CNodeLiveReaderRef nodeRef = CurNode();
	if (!nodeRef.IsValid())
		return false;

	const char* pVal = NULL;
	if (nodeRef->HaveAttr(name) && nodeRef->ObtainAttr(name).GetBasicDataType() == XMLCPB::DT_STR)
		nodeRef->ReadAttr(name, pVal);

	bool isZero = pVal ? 0 == strcmp("zero", pVal) : false;

	if (isZero)
		value = CTimeValue(0.0f);
	else
	{
		float delta;
		if (!GetAttr(nodeRef, name, delta))
		{
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

void CSerializeReaderXMLCPBin::RecursiveReadIntoXmlNodeRef(XMLCPB::CNodeLiveReaderRef BNode, XmlNodeRef& xmlNode)
{
	for (int i = 0; i < BNode->GetNumAttrs(); ++i)
	{
		XMLCPB::CAttrReader attr = BNode->ObtainAttr(i);
		const char* pVal = NULL;
		attr.Get(pVal);
		xmlNode->setAttr(attr.GetName(), pVal);
	}

	for (int i = 0; i < BNode->GetNumChildren(); ++i)
	{
		XMLCPB::CNodeLiveReaderRef BChild = BNode->GetChildNode(i);
		XmlNodeRef child = xmlNode->createNode(BChild->GetTag());
		xmlNode->addChild(child);
		RecursiveReadIntoXmlNodeRef(BChild, child);
	}
}

bool CSerializeReaderXMLCPBin::Value(const char* name, XmlNodeRef& value)
{
	XMLCPB::CNodeLiveReaderRef BNode = CurNode()->GetChildNode(name);

	if (value)
	{
		value->removeAllAttributes();
		value->removeAllChilds();
	}

	if (!BNode.IsValid())
	{
		value = NULL;
		return false;
	}

	assert(BNode->GetNumChildren() == 1);
	BNode = BNode->GetChildNode(uint32(0));

	if (!value)
		value = GetISystem()->CreateXmlNode(BNode->GetTag());

	RecursiveReadIntoXmlNodeRef(BNode, value);

	return true;
}

bool CSerializeReaderXMLCPBin::ValueByteArray(const char* name, uint8*& rdata, uint32& outSize)
{
	DefaultValue(rdata, outSize);
	if (m_nErrors)
		return false;
	if (!CurNode()->HaveAttr(name))
	{
		// the attrs are not saved if they already had the default value
		return false;
	}
	else
	{
		assert(outSize == 0);
		CurNode()->ReadAttr(name, rdata, outSize);
	}
	return true;
}

void CSerializeReaderXMLCPBin::DefaultValue(uint8*& rdata, uint32& outSize) const
{
	// rdata remains untouched. If the attribute is found in ReadAttr, it'll be realloced to match the new size.
	//	outSize is set to 0, so if no data is found, we return back a 0'd amount read. rdata will still contain its
	//	previous data, to cut down on memory fragmentation for future reads.
	outSize = 0;
}

//////////////////////////////////////////////////////////////////////////

void CSerializeReaderXMLCPBin::BeginGroup(const char* szName)
{
	if (m_nErrors)
	{
		m_nErrors++;
	}
	else
	{
		XMLCPB::CNodeLiveReaderRef node = CurNode()->GetChildNode(szName);
		if (node.IsValid())
		{
			m_nodeStack.push_back(node);
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "!BeginGroup( %s ) not found", szName);
			m_nErrors++;
		}
	}
}

bool CSerializeReaderXMLCPBin::BeginOptionalGroup(const char* szName, bool condition)
{
	if (m_nErrors)
	{
		m_nErrors++;
	}
	XMLCPB::CNodeLiveReaderRef node = CurNode()->GetChildNode(szName);
	if (node.IsValid())
	{
		m_nodeStack.push_back(node);
		return true;
	}
	else
		return false;
}

void CSerializeReaderXMLCPBin::EndGroup()
{
	if (m_nErrors)
		m_nErrors--;
	else
	{
		m_nodeStack.pop_back();
	}
	assert(!m_nodeStack.empty());
}

//////////////////////////////////////////////////////////////////////////
const char* CSerializeReaderXMLCPBin::GetStackInfo() const
{
	static string str;
	str.assign("");
	for (int i = 0; i < (int)m_nodeStack.size(); i++)
	{
		const char* name;
		m_nodeStack[i]->ReadAttr(TAG_SCRIPT_NAME, name);
		if (name && name[0])
			str += name;
		else
			str += m_nodeStack[i]->GetTag();
		if (i != m_nodeStack.size() - 1)
			str += "/";
	}
	return str.c_str();
}
