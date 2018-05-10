// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "XMLScriptLoader.h"
#include <CrySystem/XML/IReadWriteXMLSink.h>
#include <stack>

/*
 * Load an XML file to a script table
 */

class CXmlScriptLoad : public IReadXMLSink
{
public:
	CXmlScriptLoad();

	SmartScriptTable GetTable() { CRY_ASSERT(m_tableStack.size() == 1); return m_tableStack.top(); }

	// IReadXMLSink
	virtual void            AddRef();
	virtual void            Release();

	virtual IReadXMLSinkPtr BeginTable(const char* name, const XmlNodeRef& definition);
	virtual IReadXMLSinkPtr BeginTableAt(int elem, const XmlNodeRef& definition);
	virtual bool            SetValue(const char* name, const TValue& value, const XmlNodeRef& definition);
	virtual bool            EndTableAt(int elem);
	virtual bool            EndTable(const char* name);

	virtual IReadXMLSinkPtr BeginArray(const char* name, const XmlNodeRef& definition);
	virtual bool            SetAt(int elem, const TValue& value, const XmlNodeRef& definition);
	virtual bool            EndArray(const char* name);

	virtual bool            IsCreationMode()                       { return false; }
	virtual void            SetCreationNode(XmlNodeRef definition) {}
	virtual XmlNodeRef      GetCreationNode()                      { return XmlNodeRef(0); }

	virtual bool            Complete();
	// ~IReadXMLSink

private:
	int                          m_nRefs;
	IScriptSystem*               m_pSS;
	std::stack<SmartScriptTable> m_tableStack;

	IScriptTable* CurTable() { CRY_ASSERT(!m_tableStack.empty()); return m_tableStack.top().GetPtr(); }

	class SSetValueVisitor
	{
	public:
		SSetValueVisitor(IScriptTable* pTable, const char* name) : m_pTable(pTable), m_name(name) {}

		template<class T>
		void Visit(const T& value)
		{
			m_pTable->SetValue(m_name, value);
		}

		template<class T>
		void operator()(const T& type)
		{
			Visit(type);
		}

		void operator()(const SReadWriteXMLCommon::TValue& var)
		{
			VisitVariant(var);
		}

	private:
		template<size_t I = 0>
		void VisitVariant(const SReadWriteXMLCommon::TValue& var)
		{
			if (var.index() == I)
			{
				Visit(stl::get<I>(var));
			}
			else
			{
				VisitVariant<I + 1>(var);
			}
		}

		IScriptTable* m_pTable;
		const char*   m_name;
	};
	class SSetValueAtVisitor
	{
	public:
		SSetValueAtVisitor(IScriptTable* pTable, int elem) : m_pTable(pTable), m_elem(elem) {}

		template<class T>
		void Visit(const T& value)
		{
			m_pTable->SetAt(m_elem, value);
		}

		template<class T>
		void operator()(const T& type)
		{
			Visit(type);
		}

		void operator()(const SReadWriteXMLCommon::TValue& var)
		{
			VisitVariant(var);
		}

	private:
		template<size_t I = 0>
		void VisitVariant(const SReadWriteXMLCommon::TValue& var)
		{
			if (var.index() == I)
			{
				Visit(stl::get<I>(var));
			}
			else
			{
				VisitVariant<I + 1>(var);
			}
		}

		IScriptTable* m_pTable;
		int           m_elem;
	};
};
template<>
void CXmlScriptLoad::SSetValueVisitor::VisitVariant<stl::variant_size<SReadWriteXMLCommon::TValue>::value>(const SReadWriteXMLCommon::TValue& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}
template<>
void CXmlScriptLoad::SSetValueAtVisitor::VisitVariant<stl::variant_size<SReadWriteXMLCommon::TValue>::value>(const SReadWriteXMLCommon::TValue& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}

TYPEDEF_AUTOPTR(CXmlScriptLoad);
typedef CXmlScriptLoad_AutoPtr CXmlScriptLoadPtr;

CXmlScriptLoad::CXmlScriptLoad() : m_nRefs(0), m_pSS(gEnv->pScriptSystem)
{
	m_tableStack.push(SmartScriptTable(m_pSS));
}

void CXmlScriptLoad::AddRef()
{
	++m_nRefs;
}

void CXmlScriptLoad::Release()
{
	if (0 == --m_nRefs)
		delete this;
}

IReadXMLSinkPtr CXmlScriptLoad::BeginTable(const char* name, const XmlNodeRef& definition)
{
	m_tableStack.push(SmartScriptTable(m_pSS));
	return this;
}

IReadXMLSinkPtr CXmlScriptLoad::BeginTableAt(int elem, const XmlNodeRef& definition)
{
	return BeginTable(NULL, definition);
}

bool CXmlScriptLoad::SetValue(const char* name, const TValue& value, const XmlNodeRef& definition)
{
	SSetValueVisitor visitor(CurTable(), name);
	stl::visit(visitor, value);
	return true;
}

bool CXmlScriptLoad::EndTable(const char* name)
{
	SmartScriptTable newTable = CurTable();
	m_tableStack.pop();
	CurTable()->SetValue(name, newTable);
	return true;
}

bool CXmlScriptLoad::EndTableAt(int elem)
{
	SmartScriptTable newTable = CurTable();
	m_tableStack.pop();
	CurTable()->SetAt(elem, newTable);
	return true;
}

IReadXMLSinkPtr CXmlScriptLoad::BeginArray(const char* name, const XmlNodeRef& definition)
{
	m_tableStack.push(SmartScriptTable(m_pSS));
	return this;
}

bool CXmlScriptLoad::SetAt(int elem, const TValue& value, const XmlNodeRef& definition)
{
	SSetValueAtVisitor visitor(CurTable(), elem);
	stl::visit(visitor, value);
	return true;
}

bool CXmlScriptLoad::EndArray(const char* name)
{
	return EndTable(name);
}

bool CXmlScriptLoad::Complete()
{
	return m_tableStack.size() == 1;
}

SmartScriptTable XmlScriptLoad(const char* definitionFile, XmlNodeRef data)
{
	CXmlScriptLoadPtr pLoader(new CXmlScriptLoad);
	if (GetISystem()->GetXmlUtils()->GetIReadWriteXMLSink()->ReadXML(definitionFile, data, &*pLoader))
		return pLoader->GetTable();
	else
		return NULL;
}

SmartScriptTable XmlScriptLoad(const char* definitionFile, const char* dataFile)
{
	CXmlScriptLoadPtr pLoader(new CXmlScriptLoad);
	if (GetISystem()->GetXmlUtils()->GetIReadWriteXMLSink()->ReadXML(definitionFile, dataFile, &*pLoader))
		return pLoader->GetTable();
	else
		return NULL;
}

/*
 * Save an XML file from a script table
 */

class CXmlScriptSaver : public IWriteXMLSource
{
public:
	CXmlScriptSaver(SmartScriptTable pTable);

	// IWriteXMLSource
	virtual void               AddRef();
	virtual void               Release();

	virtual IWriteXMLSourcePtr BeginTable(const char* name);
	virtual IWriteXMLSourcePtr BeginTableAt(int elem);
	virtual bool               HaveValue(const char* name);
	virtual bool               GetValue(const char* name, TValue& value, const XmlNodeRef& definition);
	virtual bool               EndTableAt(int elem);
	virtual bool               EndTable(const char* name);

	virtual IWriteXMLSourcePtr BeginArray(const char* name, size_t* numElems, const XmlNodeRef& definition);
	virtual bool               HaveElemAt(int elem);
	virtual bool               GetAt(int elem, TValue& value, const XmlNodeRef& definition);
	virtual bool               EndArray(const char* name);

	virtual bool               Complete();
	// ~IWriteXMLSource

private:
	int                          m_nRefs;
	std::stack<SmartScriptTable> m_tables;

	IScriptTable* CurTable() { return m_tables.top().GetPtr(); }
};

TYPEDEF_AUTOPTR(CXmlScriptSaver);
typedef CXmlScriptSaver_AutoPtr CXmlScriptSaverPtr;

void CXmlScriptSaver::AddRef()
{
	++m_nRefs;
}

void CXmlScriptSaver::Release()
{
	if (0 == --m_nRefs)
		delete this;
}

IWriteXMLSourcePtr CXmlScriptSaver::BeginTable(const char* name)
{
	SmartScriptTable childTable;
	if (!CurTable()->GetValue(name, childTable))
		return NULL;
	m_tables.push(childTable);
	return this;
}

IWriteXMLSourcePtr CXmlScriptSaver::BeginTableAt(int elem)
{
	SmartScriptTable childTable;
	if (!CurTable()->GetAt(elem, childTable))
		return NULL;
	m_tables.push(childTable);
	return this;
}

IWriteXMLSourcePtr CXmlScriptSaver::BeginArray(const char* name, size_t* numElems, const XmlNodeRef& definition)
{
	SmartScriptTable childTable;
	if (!CurTable()->GetValue(name, childTable))
		return NULL;
	*numElems = childTable->Count();
	m_tables.push(childTable);
	return this;
}

bool CXmlScriptSaver::EndTable(const char* name)
{
	m_tables.pop();
	return true;
}

bool CXmlScriptSaver::EndTableAt(int elem)
{
	return EndTable(NULL);
}

bool CXmlScriptSaver::EndArray(const char* name)
{
	return EndTable(name);
}

namespace
{

struct CGetValueVisitor
{
public:
	CGetValueVisitor(IScriptTable* pTable, const char* name) : m_ok(false), m_pTable(pTable), m_name(name) {}

	template<class T>
	void Visit(T& value)
	{
		m_ok = m_pTable->GetValue(m_name, value);
	}

	template<class T>
	void operator()(T& type)
	{
		Visit(type);
	}

	void operator()(SReadWriteXMLCommon::TValue& var)
	{
		VisitVariant(var);
	}

	bool Ok()
	{
		return m_ok;
	}

private:
	template<size_t I = 0>
	void VisitVariant(SReadWriteXMLCommon::TValue& var)
	{
		if (var.index() == I)
		{
			Visit(stl::get<I>(var));
		}
		else
		{
			VisitVariant<I + 1>(var);
		}
	}

	bool          m_ok;
	IScriptTable* m_pTable;
	const char*   m_name;
};
template<>
void CGetValueVisitor::VisitVariant<stl::variant_size<SReadWriteXMLCommon::TValue>::value>(SReadWriteXMLCommon::TValue& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}

struct CGetAtVisitor
{
public:
	CGetAtVisitor(IScriptTable* pTable, int elem) : m_ok(false), m_pTable(pTable), m_elem(elem) {}

	template<class T>
	void Visit(T& value)
	{
		m_ok = m_pTable->GetAt(m_elem, value);
	}

	template<class T>
	void operator()(T& type)
	{
		Visit(type);
	}

	void operator()(SReadWriteXMLCommon::TValue& var)
	{
		VisitVariant(var);
	}

	bool Ok()
	{
		return m_ok;
	}

private:
	template<size_t I = 0>
	void VisitVariant(SReadWriteXMLCommon::TValue& var)
	{
		if (var.index() == I)
		{
			Visit(stl::get<I>(var));
		}
		else
		{
			VisitVariant<I + 1>(var);
		}
	}

	bool          m_ok;
	IScriptTable* m_pTable;
	int           m_elem;
};
template<>
void CGetAtVisitor::VisitVariant<stl::variant_size<SReadWriteXMLCommon::TValue>::value>(SReadWriteXMLCommon::TValue& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}
}

bool CXmlScriptSaver::GetValue(const char* name, TValue& value, const XmlNodeRef& definition)
{
	CGetValueVisitor visitor(CurTable(), name);
	stl::visit(visitor, value);
	return visitor.Ok();
}

bool CXmlScriptSaver::GetAt(int elem, TValue& value, const XmlNodeRef& definition)
{
	CGetAtVisitor visitor(CurTable(), elem);
	stl::visit(visitor, value);
	return visitor.Ok();
}

bool CXmlScriptSaver::HaveElemAt(int elem)
{
	ScriptAnyValue value;
	if (CurTable()->GetAtAny(elem, value))
		if (value.GetVarType() != svtNull)
			return true;
	return false;
}

bool CXmlScriptSaver::HaveValue(const char* name)
{
	ScriptAnyValue value;
	if (CurTable()->GetValueAny(name, value))
		if (value.GetVarType() != svtNull)
			return true;
	return false;
}

bool CXmlScriptSaver::Complete()
{
	return true;
}

CXmlScriptSaver::CXmlScriptSaver(SmartScriptTable pTable) : m_nRefs(0)
{
	m_tables.push(pTable);
}

XmlNodeRef XmlScriptSave(const char* definitionFile, SmartScriptTable scriptTable)
{
	CXmlScriptSaverPtr pSaver(new CXmlScriptSaver(scriptTable));
	return GetISystem()->GetXmlUtils()->GetIReadWriteXMLSink()->CreateXMLFromSource(definitionFile, &*pSaver);
}

bool XmlScriptSave(const char* definitionFile, const char* dataFile, SmartScriptTable scriptTable)
{
	CXmlScriptSaverPtr pSaver(new CXmlScriptSaver(scriptTable));
	return GetISystem()->GetXmlUtils()->GetIReadWriteXMLSink()->WriteXML(definitionFile, dataFile, &*pSaver);
}
