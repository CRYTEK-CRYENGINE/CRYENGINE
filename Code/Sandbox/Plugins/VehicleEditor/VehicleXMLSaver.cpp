// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "VehicleXMLSaver.h"

#include <stack>
#include <CrySystem/XML/IReadWriteXMLSink.h>
#include "VehicleXMLHelper.h"

#include "VehicleData.h"

/*
 * Implementation of IWriteXMLSource, writes IVehicleData structure
 */
class CVehicleDataSaver : public IWriteXMLSource
{
public:
	CVehicleDataSaver();
	CVehicleDataSaver(IVariablePtr pNode);

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
	int                      m_nRefs;
	std::stack<IVariablePtr> m_nodeStack;

	IVariablePtr CurNode() { assert(!m_nodeStack.empty()); return m_nodeStack.top(); }

};

TYPEDEF_AUTOPTR(CVehicleDataSaver);
typedef CVehicleDataSaver_AutoPtr CVehicleDataSaverPtr;

void CVehicleDataSaver::AddRef()
{
	++m_nRefs;
}

void CVehicleDataSaver::Release()
{
	if (0 == --m_nRefs)
		delete this;
}

IWriteXMLSourcePtr CVehicleDataSaver::BeginTable(const char* name)
{
	IVariablePtr childNode = GetChildVar(CurNode(), name);
	if (!childNode)
		return NULL;
	m_nodeStack.push(childNode);
	return this;
}

IWriteXMLSourcePtr CVehicleDataSaver::BeginTableAt(int elem)
{
	IVariablePtr childNode = CurNode()->GetVariable(elem - 1);
	if (!childNode)
		return NULL;
	m_nodeStack.push(childNode);
	return this;
}

IWriteXMLSourcePtr CVehicleDataSaver::BeginArray(const char* name, size_t* numElems, const XmlNodeRef& definition)
{
	IVariablePtr childNode = GetChildVar(CurNode(), name);
	if (!childNode)
		return NULL;
	*numElems = childNode->GetNumVariables();
	m_nodeStack.push(childNode);
	return this;
}

bool CVehicleDataSaver::EndTable(const char* name)
{
	m_nodeStack.pop();
	return true;
}

bool CVehicleDataSaver::EndTableAt(int elem)
{
	return EndTable(NULL);
}

bool CVehicleDataSaver::EndArray(const char* name)
{
	return EndTable(name);
}

namespace
{
#define VAL_MIN       0.0002f
#define VAL_PRECISION 4.0f

template<class T> void ClampValue(T& val)
{
}

//! rounding
template<> void ClampValue(float& val)
{
	const static float coeff = pow(10.0f, VAL_PRECISION);
	val = ((float)((long)(val * coeff + sgn(val) * 0.5f))) / coeff;

	if (abs(val) <= VAL_MIN)
		val = 0;
}
template<> void ClampValue(Vec3& val)
{
	for (int i = 0; i < 3; ++i)
		ClampValue<float>(val[i]);
}

struct CGetValueVisitor
{
public:
	CGetValueVisitor(IVariablePtr pNode, const char* name) : m_pNode(pNode), m_name(name), m_ok(false) {}

	void Visit(const char*& value)
	{
		static string sVal;
		if (IVariable* child = GetChildVar(m_pNode, m_name))
		{
			child->Get(sVal);
			value = sVal;
			m_ok = true;
		}
	}
	template<class T> void Visit(T& value)
	{
		if (IVariable* child = GetChildVar(m_pNode, m_name))
		{
			child->Get(value);
			ClampValue(value);
			m_ok = true;
		}
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

	bool         m_ok;
	IVariablePtr m_pNode;
	const char*  m_name;
};
template<>
void CGetValueVisitor::VisitVariant<stl::variant_size<SReadWriteXMLCommon::TValue>::value>(SReadWriteXMLCommon::TValue& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}

struct CGetAtVisitor
{
public:
	CGetAtVisitor(IVariablePtr pNode, int elem) : m_pNode(pNode), m_elem(elem), m_ok(false) {}

	template<class T> void Visit(T& value)
	{
		if (IVariable* child = m_pNode->GetVariable(m_elem - 1))
		{
			child->Get(value);
			ClampValue(value);
			m_ok = true;
		}
	}
	void Visit(const char*& value)
	{
		static string sVal;
		if (IVariable* child = m_pNode->GetVariable(m_elem - 1))
		{
			child->Get(sVal);
			value = sVal;
			m_ok = true;
		}
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

	bool         m_ok;
	IVariablePtr m_pNode;
	int          m_elem;
};
template<>
void CGetAtVisitor::VisitVariant<stl::variant_size<SReadWriteXMLCommon::TValue>::value>(SReadWriteXMLCommon::TValue& var)
{
	CRY_ASSERT_MESSAGE(false, "Invalid variant index.");
}

}

bool CVehicleDataSaver::GetValue(const char* name, TValue& value, const XmlNodeRef& definition)
{
	CGetValueVisitor visitor(CurNode(), name);
	stl::visit(visitor, value);
	return visitor.Ok();
}

bool CVehicleDataSaver::GetAt(int elem, TValue& value, const XmlNodeRef& definition)
{
	CGetAtVisitor visitor(CurNode(), elem);
	stl::visit(visitor, value);
	return visitor.Ok();
}

bool CVehicleDataSaver::HaveElemAt(int elem)
{
	if (elem <= CurNode()->GetNumVariables())
		return true;
	return false;
}

bool CVehicleDataSaver::HaveValue(const char* name)
{
	if (GetChildVar(CurNode(), name))
		return true;
	return false;
}

bool CVehicleDataSaver::Complete()
{
	return true;
}

CVehicleDataSaver::CVehicleDataSaver(IVariablePtr pNode) : m_nRefs(0)
{
	m_nodeStack.push(pNode);
}

XmlNodeRef VehicleDataSave(const char* definitionFile, IVehicleData* pData)
{
	CVehicleDataSaverPtr pSaver(new CVehicleDataSaver(pData->GetRoot()));
	return GetISystem()->GetXmlUtils()->GetIReadWriteXMLSink()->CreateXMLFromSource(definitionFile, &*pSaver);
}

bool VehicleDataSave(const char* definitionFile, const char* dataFile, IVehicleData* pData)
{
	CVehicleDataSaverPtr pSaver(new CVehicleDataSaver(pData->GetRoot()));
	return GetISystem()->GetXmlUtils()->GetIReadWriteXMLSink()->WriteXML(definitionFile, dataFile, &*pSaver);
}

////////////////////////////////////////////////////////////////////////////////////////////
static int GetRequiredPrecision(float f)
{
	int width = 4;
	const float precision = 10000.f;

	// Quantise, round, and get the fraction
	f = (float)((int)((fabsf(f) * precision) + 0.499999f)) / precision;
	int decimal = (int)f;
	int ifrac = (int)(((f - (float)decimal) * precision) + 0.499999f);

	if (ifrac == 0)
		return 0;

	while (ifrac >= 10 && (ifrac % 10) == 0)
	{
		width--;
		ifrac /= 10;
	}
	return width;
}

// Display Name formatted sensibly to minimise xml merge diffs
string GetDisplayValue(IVariablePtr var)
{
#define FFMT(x) GetRequiredPrecision(x), x
	string output;
	switch (var->GetType())
	{
	case IVariable::FLOAT:
		{
			float val;
			var->Get(val);
			output.Format("%.*f", FFMT(val));
			break;
		}
	case IVariable::VECTOR2:
		{
			Vec2 val;
			var->Get(val);
			output.Format("%.*f,%.*f", FFMT(val.x), FFMT(val.y));
			break;
		}
	case IVariable::VECTOR:
		{
			Vec3 val;
			var->Get(val);
			output.Format("%.*f,%.*f,%.*f", FFMT(val.x), FFMT(val.y), FFMT(val.z));
			break;
		}
	case IVariable::VECTOR4:
		{
			Vec4 val;
			var->Get(val);
			output.Format("%.*f,%.*f,%.*f,%.*f", FFMT(val.x), FFMT(val.y), FFMT(val.z), FFMT(val.w));
			break;
		}
	case IVariable::QUAT:
		{
			Quat val;
			var->Get(val);
			output.Format("%.*f,%.*f,%.*f,%.*f", FFMT(val.w), FFMT(val.v.x), FFMT(val.v.y), FFMT(val.v.z));
			break;
		}
	default:
		output = var->GetDisplayValue();
	}
	return output;
#undef FFMT
}

XmlNodeRef findChild(const char* name, XmlNodeRef source, int& index)
{
	for (; index < source->getChildCount(); )
	{
		XmlNodeRef child = source->getChild(index++);
		if (stricmp(name, child->getTag()) == 0)
			return child;
	}
	return 0;
}

void VehicleDataMerge_ProcessArray(XmlNodeRef source, XmlNodeRef definition, IVariable* array)
{
	if (array->GetType() != IVariable::ARRAY)
		return;

	DefinitionTable definitionList;
	definitionList.Create(definition);
	//definitionList.Dump();

	// We need to keep a search index for each param name type
	// since xmlnode->findChild() would just return the first,
	// which is incorrect
	typedef std::map<string, int> TMap;
	TMap sourceNodeIndex;

	// Loop over the definition list and initialise the start index
	DefinitionTable::TXmlNodeMap::iterator it = definitionList.m_definitionList.begin();
	for (; it != definitionList.m_definitionList.end(); ++it)
		sourceNodeIndex[it->first] = 0;

	// loop through the vehicle data
	for (int i = 0; i < array->GetNumVariables(); i++)
	{
		IVariable* var = array->GetVariable(i);
		const char* dataName = var->GetName();

		XmlNodeRef propertyDef = definitionList.GetDefinition(dataName);
		if (!propertyDef)
			continue;

		if (stricmp(propertyDef->getTag(), "Property") == 0)
		{
			// Has the var changed from the src var?
			if (IVariablePtr srcVar = VehicleXml::CreateSimpleVar(dataName, source->getAttr(dataName), propertyDef))
				if (srcVar->GetDisplayValue() == var->GetDisplayValue())
					continue;

			// Patch/add the data to the source xml
			string dataDisplayName = GetDisplayValue(var);
			source->setAttr(dataName, dataDisplayName);
		}

		// We expect the sub-variable to be an array
		if (var->GetType() != IVariable::ARRAY)
			continue;

		// Load Table
		int& index = sourceNodeIndex[dataName];
		XmlNodeRef xmlTable = findChild(dataName, source, index);
		if (!xmlTable)
			continue;

		if (definitionList.IsTable(propertyDef))
		{
			VehicleDataMerge_ProcessArray(xmlTable, propertyDef, var);
		}
		else if (definitionList.IsArray(propertyDef))
		{
			const char* elementName = propertyDef->getAttr("elementName");
			IVariable* arrayRoot = var;

			// Delete the block of elements from source and re-export it
			if (arrayRoot->GetNumVariables() != xmlTable->getChildCount())
			{
				xmlTable->removeAllChilds();
				VehicleXml::GetXmlNodeFromVariable(arrayRoot, xmlTable);
			}
			else
			{
				if (propertyDef->haveAttr("type"))
				{
					for (int j = 0; j < arrayRoot->GetNumVariables(); j++)
					{
						XmlNodeRef xmlChildElement = xmlTable->getChild(j);
						string dataDisplayName = GetDisplayValue(arrayRoot->GetVariable(j));
						xmlChildElement->setAttr("value", dataDisplayName);
					}
				}
				else
				{
					for (int j = 0; j < arrayRoot->GetNumVariables(); j++)
					{
						XmlNodeRef xmlChildElement = xmlTable->getChild(j);
						VehicleDataMerge_ProcessArray(xmlChildElement, propertyDef, arrayRoot->GetVariable(j));
					}
				}
			}
		}
		else
		{
			// Leave the node alone, no need to change the src xml
		}
	}
}

XmlNodeRef VehicleDataMergeAndSave(const char* originalXml, XmlNodeRef definition, IVehicleData* pData)
{
	XmlNodeRef root = GetISystem()->LoadXmlFromFile(originalXml);

	if (!root)
	{
		CryLog("Vehicle Editor: can't create save-data, as source file %s couldnt be opened", originalXml);
		return 0;
	}

	IVariablePtr pDataRoot = pData->GetRoot();
	if (pDataRoot->GetType() != IVariable::ARRAY)
	{
		CryLog("Vehicle Editor: can't create save-data");
		return 0;
	}

	// Recursively walk the vehicle data, and patch the source xml
	VehicleDataMerge_ProcessArray(root, definition, pDataRoot);

	return root;
}

