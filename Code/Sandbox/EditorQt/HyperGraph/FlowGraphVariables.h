// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __FLOWGRAPHVARIABLES_H__
#define __FLOWGRAPHVARIABLES_H__
#pragma once

#include <Util/Variable.h>
#include "FlowGraphNode.h"

#define FGVARIABLES_REAL_CLONE
// #undef  FGVARIABLES_REAL_CLONE  // !!! Not working without real cloning currently !!!

// REMARKS:
// FlowGraph uses its own set of CVariables
// These Variables handle the IGetCustomItems stored in the UserData of the
// Variables
// If FGVARIABLES_REAL_CLONE is defined every CVariableFlowNode uses its own instance of CFlowNodeGetCustomItemBase
// If FGVARIABLES_REAL_CLONE is not set, there is only one instance of CFlowNodeGetCustomItemBase per port type
// which is shared and pointers are adjusted in CFlowNode::GetInputsVarBlock()
// The later is not working correctly with Undo/Redo.

class CFlowNodeGetCustomItemsBase;

namespace FlowGraphVariables
{
template<typename T> CFlowNodeGetCustomItemsBase* CreateIt() { return new T(); };

// A simple struct to map prefixes to data types
struct MapEntry
{
	const char*                  sPrefix;   // prefix string (including '_' character)
	IVariable::EDataType         eDataType;
	CFlowNodeGetCustomItemsBase* (* pGetCustomItemsCreator) ();
};

const MapEntry* FindEntry(const char* sPrefix);
};

//////////////////////////////////////////////////////////////////////////
class CFlowNodeGetCustomItemsBase : public IVariable::IGetCustomItems
{
public:
	virtual CFlowNodeGetCustomItemsBase* Clone() const
	{
#ifdef FGVARIABLES_REAL_CLONE
		CFlowNodeGetCustomItemsBase* pNew = new CFlowNodeGetCustomItemsBase();
		pNew->m_pFlowNode = m_pFlowNode;
		pNew->m_config = m_config;
		return pNew;
#else
		return const_cast<CFlowNodeGetCustomItemsBase*>(this);
#endif
	}

	CFlowNodeGetCustomItemsBase() : m_pFlowNode(0)
	{
		// CryLogAlways("CFlowNodeGetCustomItemsBase::ctor 0x%p", this);
	}

	virtual ~CFlowNodeGetCustomItemsBase()
	{
		// CryLogAlways("CFlowNodeGetCustomItemsBase::dtor 0x%p", this);
	}

	virtual bool GetItems(IVariable* /* pVar */, std::vector<SItem>& /* items */, string& /* outDialogTitle */)
	{
		return false;
	}

	virtual bool UseTree()
	{
		return false;
	}

	virtual const char* GetTreeSeparator()
	{
		return 0;
	}

	void SetFlowNode(CFlowNode* pFlowNode)
	{
		m_pFlowNode = pFlowNode;
	}

	CFlowNode* GetFlowNode() const
	{
		// assert (m_pFlowNode != 0);
		return m_pFlowNode;
	}

	void SetUIConfig(const char* sUIConfig)
	{
		m_config = sUIConfig;
	}

	const string& GetUIConfig() const
	{
		return m_config;
	}
protected:
	CFlowNode* m_pFlowNode;
	string    m_config;
private:
	CFlowNodeGetCustomItemsBase(const CFlowNodeGetCustomItemsBase&) {}
};

//////////////////////////////////////////////////////////////////////////
template<class T>
class CVariableFlowNode : public CVariable<T>
{
public:
	typedef CVariableFlowNode<T> Self;

	CVariableFlowNode() : CVariable<T>() {}

	IVariable* Clone(bool bRecursive) const
	{
		Self* var = new Self(*this);
		return var;
	}

protected:
	CVariableFlowNode(const Self& other)
		: CVariable<T>(other)
	{
		if (other.m_pUserData)
		{
			CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(other.m_pUserData);
			pGetCustomItems = pGetCustomItems->Clone();
			pGetCustomItems->AddRef();
			m_pUserData = pGetCustomItems;
		}
	}

	virtual ~CVariableFlowNode()
	{
		if (m_pUserData)
		{
			CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(m_pUserData);
			pGetCustomItems->Release();
		}
	}

	virtual string GetDisplayValue() const
	{
		return CVariable<T>::GetDisplayValue();
	}

	virtual void SetDisplayValue(const string& value)
	{
		CVariable<T>::SetDisplayValue(value);
	}
};

// explicit member template specialization in case of Vec3
// if it is a color, return values in range [0,255] instead of [0,1] (just like PropertyItem)
template<> string CVariableFlowNode<Vec3 >::GetDisplayValue() const
{
	if (m_dataType != DT_COLOR)
		return CVariable<Vec3>::GetDisplayValue();
	else
	{
		Vec3 v;
		Get(v);
		int r = v.x * 255;
		int g = v.y * 255;
		int b = v.z * 255;
		r = max(0, min(r, 255));
		g = max(0, min(g, 255));
		b = max(0, min(b, 255));
		string retVal;
		retVal.Format("%d,%d,%d", r, g, b);
		return retVal;
	}
}

template<> void CVariableFlowNode<Vec3 >::SetDisplayValue(const string& value)
{
	if (m_dataType != DT_COLOR)
		CVariable<Vec3>::Set(value);
	else
	{
		float r, g, b;
		int res = sscanf(value, "%f,%f,%f", &r, &g, &b);
		if (res == 3)
		{
			r /= 255.0f;
			g /= 255.0f;
			b /= 255.0f;
			Set(Vec3(r, g, b));
		}
		else CVariable<Vec3>::Set(value);
	}
}

template<class T>
class CVariableFlowNodeEnum : public CVariableEnum<T>
{
public:
	typedef CVariableFlowNodeEnum<T> Self;

	CVariableFlowNodeEnum() : CVariableEnum<T>()
	{
	}

	IVariable* Clone(bool bRecursive) const
	{
		Self* var = new Self(*this);
		return var;
	}

protected:
	CVariableFlowNodeEnum(const Self& other) : CVariableEnum<T>(other)
	{
		if (other.m_pUserData)
		{
			CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(other.m_pUserData);
			pGetCustomItems = pGetCustomItems->Clone();
			pGetCustomItems->AddRef();
			m_pUserData = pGetCustomItems;
		}
	}

	virtual ~CVariableFlowNodeEnum()
	{
		if (m_pUserData)
		{
			CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(m_pUserData);
			pGetCustomItems->Release();
		}
	}

};

class CVariableFlowNodeVoid : public CVariableVoid
{
public:
	typedef CVariableFlowNodeVoid Self;

	CVariableFlowNodeVoid() : CVariableVoid()
	{
	}

	IVariable* Clone(bool bRecursive) const
	{
		Self* var = new Self(*this);
		return var;
	}

protected:
	CVariableFlowNodeVoid(const Self& other) : CVariableVoid(other)
	{
		if (other.m_pUserData)
		{
			CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(other.m_pUserData);
			pGetCustomItems = pGetCustomItems->Clone();
			pGetCustomItems->AddRef();
			m_pUserData = pGetCustomItems;
		}
	}

	virtual ~CVariableFlowNodeVoid()
	{
		if (m_pUserData)
		{
			CFlowNodeGetCustomItemsBase* pGetCustomItems = static_cast<CFlowNodeGetCustomItemsBase*>(m_pUserData);
			pGetCustomItems->Release();
		}
	}

};

struct CUIEnumsDatabase_SEnum;

class CVariableFlowNodeCustomEnumBase : public CVariableFlowNode<string>
{
public:
	CVariableFlowNodeCustomEnumBase()
		: CVariableFlowNode<string>()
	{
		m_pDynEnumList = new CDynamicEnumList(this);
	}

	IVarEnumList* GetEnumList() const
	{
		return m_pDynEnumList;
	}

	string GetDisplayValue() const
	{
		if (m_pDynEnumList)
			return m_pDynEnumList->ValueToName(m_valueDef);
		else
			return CVariableFlowNode<string>::GetDisplayValue();
	}

	void SetDisplayValue(const string& value)
	{
		if (m_pDynEnumList)
			CVariableFlowNode<string>::Set(m_pDynEnumList->NameToValue(value));
		else
			CVariableFlowNode<string>::Set(value);
	}

protected:
	CVariableFlowNodeCustomEnumBase(const CVariableFlowNodeCustomEnumBase& other)
		: CVariableFlowNode<string>(other)
	{
		m_pDynEnumList = new CDynamicEnumList(this);
	}

	virtual CUIEnumsDatabase_SEnum* GetSEnum() = 0;

	// some inner class
	class CDynamicEnumList : public CVarEnumListBase<string>
	{
	public:
		CDynamicEnumList(CVariableFlowNodeCustomEnumBase* pDynEnum);
		virtual ~CDynamicEnumList();

		//! Get the name of specified value in enumeration, or NULL if out of range.
		virtual const char* GetItemName(uint index);

		virtual string     NameToValue(const string& name);
		virtual string     ValueToName(const string& value);

		//! Don't add anything to a global enum database
		virtual void AddItem(const string& name, const string& value) {}

	private:
		CVariableFlowNodeCustomEnumBase* m_pDynEnum;
	};

protected:
	TSmartPtr<CDynamicEnumList> m_pDynEnumList;
	string                     m_refPort;
	string                     m_refFormatString;
};

// looks up value in a different variable[port] and provides enum accordingly
class CVariableFlowNodeDynamicEnum : public CVariableFlowNodeCustomEnumBase
{
public:
	CVariableFlowNodeDynamicEnum(const string& refFormatString, const string& refPort)
		: CVariableFlowNodeCustomEnumBase()
	{
		m_refPort = refPort;
		m_refFormatString = refFormatString;
	}

	IVariable* Clone(bool bRecursive) const
	{
		CVariableFlowNodeDynamicEnum* var = new CVariableFlowNodeDynamicEnum(*this);
		return var;
	}

protected:
	CVariableFlowNodeDynamicEnum(const CVariableFlowNodeDynamicEnum& other)
		: CVariableFlowNodeCustomEnumBase(other)
	{
		m_refPort = other.m_refPort;
		m_refFormatString = other.m_refFormatString;
	}

	virtual CUIEnumsDatabase_SEnum* GetSEnum();

protected:
	string m_refPort;
	string m_refFormatString;
};

// looks up value via a define given through a callback and provides enum accordingly
class CVariableFlowNodeDefinedEnum : public CVariableFlowNodeCustomEnumBase
{
public:
	CVariableFlowNodeDefinedEnum(const string& refFormatString, uint32 portId)
		: CVariableFlowNodeCustomEnumBase()
	{
		m_refFormatString = refFormatString;
		m_portId = portId;
	}

	IVariable* Clone(bool bRecursive) const
	{
		CVariableFlowNodeDefinedEnum* var = new CVariableFlowNodeDefinedEnum(*this);
		return var;
	}

protected:
	CVariableFlowNodeDefinedEnum(const CVariableFlowNodeDefinedEnum& other)
		: CVariableFlowNodeCustomEnumBase(other)
	{
		m_refFormatString = other.m_refFormatString;
		m_portId = other.m_portId;
	}

	virtual CUIEnumsDatabase_SEnum* GetSEnum();

protected:
	string m_refFormatString;
	uint32  m_portId;
};

#endif

