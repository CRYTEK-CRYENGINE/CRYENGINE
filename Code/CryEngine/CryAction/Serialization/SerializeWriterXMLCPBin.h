// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SERIALIZEWRITERXMLCPBIN_H__
#define __SERIALIZEWRITERXMLCPBIN_H__

#pragma once

#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/IValidator.h>
#include <CryNetwork/SimpleSerialize.h>
#include "XMLCPBin/Writer/XMLCPB_WriterInterface.h"

class CSerializeWriterXMLCPBin : public CSimpleSerializeImpl<false, eST_SaveGame>
{
public:
	CSerializeWriterXMLCPBin(const XMLCPB::CNodeLiveWriterRef& nodeRef, XMLCPB::CWriterInterface& binWriter);
	~CSerializeWriterXMLCPBin();

	template<class T_Value>
	bool Value(const char* name, T_Value& value)
	{
		AddValue(name, value);
		return true;
	}

	template<class T_Value, class T_Policy>
	bool Value(const char* name, T_Value& value, const T_Policy& policy)
	{
		return Value(name, value);
	}

	bool Value(const char* name, CTimeValue value);
	bool Value(const char* name, ScriptAnyValue& value);
	bool Value(const char* name, XmlNodeRef& value);
	bool ValueByteArray(const char* name, const uint8* data, uint32 len);

	void BeginGroup(const char* szName);
	bool BeginOptionalGroup(const char* szName, bool condition);
	void EndGroup();

private:
	CTimeValue                              m_curTime;
	std::vector<XMLCPB::CNodeLiveWriterRef> m_nodeStack;  // TODO: look to get rid of this. it should be useless, because can access all necesary data from the XMLCPBin object
	std::vector<IScriptTable*>              m_savedTables;
	std::vector<const char*>                m_luaSaveStack;
	XMLCPB::CWriterInterface&               m_binWriter;

	void RecursiveAddXmlNodeRef(XMLCPB::CNodeLiveWriterRef BNode, XmlNodeRef xmlNode);

	//////////////////////////////////////////////////////////////////////////
	ILINE XMLCPB::CNodeLiveWriterRef& CurNode()
	{
		assert(!m_nodeStack.empty());
		if (m_nodeStack.empty())
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CSerializeWriterXMLCPBin: !Trying to access a node from the nodeStack, but the stack is empty. Savegame will be corrupted");
			static XMLCPB::CNodeLiveWriterRef temp = m_binWriter.GetRoot()->AddChildNode("Error");
			return temp;
		}
		return m_nodeStack.back();
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	void AddValue(const char* name, const T& value)
	{
		XMLCPB::CNodeLiveWriterRef& curNode = CurNode();

#ifndef _RELEASE
		if (GetISystem()->IsDevMode() && curNode.IsValid())
		{
			if (curNode->HaveAttr(name))
			{
				assert(0);
				CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "!Duplicate tag Value( \"%s\" ) in Group %s", name, GetStackInfo());
			}
		}
#endif

		if (!IsDefaultValue(value))
		{
			curNode->AddAttr(name, value);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void AddValue(const char* name, const SSerializeString& value)
	{
		AddValue(name, value.c_str());
	}

	void AddValue(const char* name, const SNetObjectID& value)
	{
		assert(false);
	}

	//////////////////////////////////////////////////////////////////////////
	template<class T>
	void AddTypedValue(const char* name, const T& value, const char* type)
	{
		assert(false);    // not needed for savegames, apparently
		//		if (!IsDefaultValue(value))
		//		{
		//			XMLCPB::CNodeLiveWriterRef newNode = CreateNodeNamed(name);
		//			newNode->AddAttr("v",value);
		//			newNode->AddAttr("t",type);
		//		}
	}

	void WriteTable(XMLCPB::CNodeLiveWriterRef addTo, SmartScriptTable tbl, bool bCheckEntityOnScript);
	void ScriptValue(XMLCPB::CNodeLiveWriterRef addTo, const char* tag, const char* name, const ScriptAnyValue& value, bool bCheckEntityOnScript);

	// Used for printing current stack info for warnings.
	const char* GetStackInfo() const;
	const char* GetLuaStackInfo() const;

	static bool ShouldSkipValue(const char* name, const ScriptAnyValue& value);
	static bool IsVector(SmartScriptTable tbl);
	bool        IsEntity(SmartScriptTable tbl, EntityId& entityId);

	//////////////////////////////////////////////////////////////////////////
	// Check For Defaults.
	//////////////////////////////////////////////////////////////////////////
	bool IsDefaultValue(bool v) const                        { return v == false; };
	bool IsDefaultValue(float v) const                       { return v == 0; };
	bool IsDefaultValue(int8 v) const                        { return v == 0; };
	bool IsDefaultValue(uint8 v) const                       { return v == 0; };
	bool IsDefaultValue(int16 v) const                       { return v == 0; };
	bool IsDefaultValue(uint16 v) const                      { return v == 0; };
	bool IsDefaultValue(int32 v) const                       { return v == 0; };
	bool IsDefaultValue(uint32 v) const                      { return v == 0; };
	bool IsDefaultValue(int64 v) const                       { return v == 0; };
	bool IsDefaultValue(uint64 v) const                      { return v == 0; };
	bool IsDefaultValue(const Vec2& v) const                 { return v.x == 0 && v.y == 0; };
	bool IsDefaultValue(const Vec3& v) const                 { return v.x == 0 && v.y == 0 && v.z == 0; };
	bool IsDefaultValue(const Ang3& v) const                 { return v.x == 0 && v.y == 0 && v.z == 0; };
	bool IsDefaultValue(const Quat& v) const                 { return v.w == 1.0f && v.v.x == 0 && v.v.y == 0 && v.v.z == 0; };
	bool IsDefaultValue(const ScriptAnyValue& v) const       { return false; };
	bool IsDefaultValue(const CTimeValue& v) const           { return v.GetValue() == 0; };
	bool IsDefaultValue(const char* str) const               { return !str || !*str; };
	bool IsDefaultValue(const string& str) const             { return str.empty(); };
	bool IsDefaultValue(const SSerializeString& str) const   { return str.empty(); };
	bool IsDefaultValue(const uint8* data, uint32 len) const { return (len <= 0); };
	//////////////////////////////////////////////////////////////////////////

};

#endif
