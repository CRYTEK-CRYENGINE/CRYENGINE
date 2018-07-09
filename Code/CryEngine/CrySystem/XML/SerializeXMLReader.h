// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SERIALIZEXMLREADER_H__
#define __SERIALIZEXMLREADER_H__

#pragma once

#include <CryNetwork/SimpleSerialize.h>
#include <stack>
#include <CrySystem/XML/IXml.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/IValidator.h>
#include <CrySystem/ISystem.h>
#include "xml.h"

class CSerializeXMLReaderImpl : public CSimpleSerializeImpl<true, eST_SaveGame>
{
public:
	CSerializeXMLReaderImpl(const XmlNodeRef& nodeRef);

	template<class T_Value>
	ILINE bool GetAttr(const XmlNodeRef& node, const char* name, T_Value& value)
	{
		XmlStrCmpFunc pPrevCmpFunc = g_pXmlStrCmp;
		g_pXmlStrCmp = &strcmp; // Do case-sensitive compare
		bool bReturn = node->getAttr(name, value);
		g_pXmlStrCmp = pPrevCmpFunc;
		return bReturn;
	}
	ILINE bool GetAttr(const XmlNodeRef& node, const char* name, SSerializeString& value)
	{
		XmlStrCmpFunc pPrevCmpFunc = g_pXmlStrCmp;
		g_pXmlStrCmp = &strcmp; // Do case-sensitive compare
		bool bReturn = node->haveAttr(name);
		if (bReturn)
		{
			value = node->getAttr(name);
		}
		g_pXmlStrCmp = pPrevCmpFunc;
		return bReturn;
	}
	ILINE bool GetAttr(XmlNodeRef& node, const char* name, const string& value)
	{
		return false;
	}
	ILINE bool GetAttr(const XmlNodeRef& node, const char* name, SNetObjectID& value)
	{
		return false;
	}

	template<class T_Value>
	bool Value(const char* name, T_Value& value)
	{
		DefaultValue(value); // Set input value to default.
		if (m_nErrors)
			return false;

		if (!GetAttr(CurNode(), name, value))
		{
			//CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"Unable to read attribute %s (invalid type?)", name);
			//Failed();
			return false;
		}
		return true;
	}

	bool Value(const char* name, ScriptAnyValue& value);
	bool Value(const char* name, int8& value);
	bool Value(const char* name, string& value);
	bool Value(const char* name, CTimeValue& value);
	bool Value(const char* name, XmlNodeRef& value);

	template<class T_Value, class T_Policy>
	bool Value(const char* name, T_Value& value, const T_Policy& policy)
	{
		return Value(name, value);
	}

	void        BeginGroup(const char* szName);
	bool        BeginOptionalGroup(const char* szName, bool condition);
	void        EndGroup();
	const char* GetStackInfo() const;

	void        GetMemoryUsage(ICrySizer* pSizer) const;

private:
	//CTimeValue m_curTime;
	XmlNodeRef CurNode() { return m_nodeStack.back().m_node; }
	XmlNodeRef NextOf(const char* name)
	{
		XmlStrCmpFunc pPrevCmpFunc = g_pXmlStrCmp;
		g_pXmlStrCmp = &strcmp; // Do case-sensitive compare
		assert(!m_nodeStack.empty());
		CParseState& ps = m_nodeStack.back();
		XmlNodeRef node = ps.GetNext(name);
		g_pXmlStrCmp = pPrevCmpFunc;
		return node;
	}

	class CParseState
	{
	public:
		CParseState() : m_nCurrent(0) {}
		void Init(const XmlNodeRef& node)
		{
			m_node = node;
			m_nCurrent = 0;
		}

		XmlNodeRef GetNext(const char* name)
		{
			int i;
			int num = m_node->getChildCount();
			for (i = m_nCurrent; i < num; i++)
			{
				XmlNodeRef child = m_node->getChild(i);
				if (strcmp(child->getTag(), name) == 0)
				{
					m_nCurrent = i + 1;
					return child;
				}
			}
			int ncount = min(m_nCurrent, num);
			// Try searching from begining.
			for (i = 0; i < ncount; i++)
			{
				XmlNodeRef child = m_node->getChild(i);
				if (strcmp(child->getTag(), name) == 0)
				{
					m_nCurrent = i + 1;
					return child;
				}
			}
			return XmlNodeRef();
		}

	public:
		// TODO: make this much more efficient
		int        m_nCurrent;
		XmlNodeRef m_node;
	};

	int                      m_nErrors;
	std::vector<CParseState> m_nodeStack;

	bool ReadScript(XmlNodeRef node, ScriptAnyValue& value);

	//////////////////////////////////////////////////////////////////////////
	// Set Defaults.
	//////////////////////////////////////////////////////////////////////////
	void DefaultValue(bool& v) const               { v = false; }
	void DefaultValue(float& v) const              { v = 0; }
	void DefaultValue(int8& v) const               { v = 0; }
	void DefaultValue(uint8& v) const              { v = 0; }
	void DefaultValue(int16& v) const              { v = 0; }
	void DefaultValue(uint16& v) const             { v = 0; }
	void DefaultValue(int32& v) const              { v = 0; }
	void DefaultValue(uint32& v) const             { v = 0; }
	void DefaultValue(int64& v) const              { v = 0; }
	void DefaultValue(uint64& v) const             { v = 0; }
	void DefaultValue(Vec2& v) const               { v.x = 0; v.y = 0; }
	void DefaultValue(Vec3& v) const               { v.x = 0; v.y = 0; v.z = 0; }
	void DefaultValue(Ang3& v) const               { v.x = 0; v.y = 0; v.z = 0; }
	void DefaultValue(Quat& v) const               { v.w = 1.0f; v.v.x = 0; v.v.y = 0; v.v.z = 0; }
	void DefaultValue(ScriptAnyValue& v) const     {}
	void DefaultValue(CTimeValue& v) const         { v.SetValue(0); }
	//void DefaultValue( char *str ) const { if (str) str[0] = 0; }
	void DefaultValue(string& str) const           { str = ""; }
	void DefaultValue(const string& str) const     {}
	void DefaultValue(SNetObjectID& id) const      {}
	void DefaultValue(SSerializeString& str) const {}
	void DefaultValue(XmlNodeRef& ref) const       { ref = NULL; }
	//////////////////////////////////////////////////////////////////////////
};

#endif
