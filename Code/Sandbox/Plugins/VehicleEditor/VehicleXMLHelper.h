// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __VEHICLE_XML_HELPER__H__
#define __VEHICLE_XML_HELPER__H__

struct IVariable;

namespace VehicleXml
{

bool        IsUseNode(const XmlNodeRef& node);
bool        IsArrayNode(const XmlNodeRef& node);
bool        IsArrayElementNode(const XmlNodeRef& node, const char* name);
bool        IsArrayParentNode(const XmlNodeRef& node, const char* name);
bool        IsPropertyNode(const XmlNodeRef& node);
bool        IsOptionalNode(const XmlNodeRef& node);
bool        IsDeprecatedNode(const XmlNodeRef& node);

bool        HasElementNameEqualTo(const XmlNodeRef& node, const char* name);
bool        HasNodeIdEqualTo(const XmlNodeRef& node, const char* id);
bool        HasNodeNameEqualTo(const XmlNodeRef& node, const char* name);

const char* GetNodeId(const XmlNodeRef& node);
const char* GetNodeElementName(const XmlNodeRef& node);
const char* GetNodeName(const XmlNodeRef& node);

XmlNodeRef  GetXmlNodeDefinitionById(const XmlNodeRef& definitionRoot, const char* id);
XmlNodeRef  GetXmlNodeDefinitionByVariable(const XmlNodeRef& definitionRoot, IVariable* pRootVar, IVariable* pVar);

bool        GetVariableHierarchy(IVariable* pRootVar, IVariable* pVar, std::vector<IVariable*>& varHierarchyOut);

void        GetXmlNodeFromVariable(IVariable* pVar, XmlNodeRef& xmlNode);
void        GetXmlNodeChildDefinitions(const XmlNodeRef& definitionRoot, const XmlNodeRef& definition, std::vector<XmlNodeRef>& childDefinitionsOut);
void        GetXmlNodeChildDefinitionsByVariable(const XmlNodeRef& definitionRoot, IVariable* pRootVar, IVariable* pVar, std::vector<XmlNodeRef>& childDefinitionsOut);
XmlNodeRef  GetXmlNodeChildDefinitionsByName(const XmlNodeRef& definitionRoot, const char* childName, IVariable* pRootVar, IVariable* pVar);

IVariable*  CreateDefaultVar(const XmlNodeRef& definitionRoot, const XmlNodeRef& definition, const char* varName);

void        SetExtendedVarProperties(IVariable* pVar, const XmlNodeRef& node);

template<class T>
IVariable* CreateVar(T value, const XmlNodeRef& definition)
{
	return new CVariable<T>;
}

IVariable* CreateVar(const char* value, const XmlNodeRef& definition);

IVariable* CreateSimpleVar(const char* attributeName, const char* attributeValue, const XmlNodeRef& definition);

void       CleanUp(IVariable* pVar);
}

/*
   =========================================================
   DefinitionTable
   =========================================================
 */

struct DefinitionTable
{
	// Build a list of definitions against their name for easy searching
	typedef std::map<string, XmlNodeRef> TXmlNodeMap;

	static TXmlNodeMap g_useDefinitionMap;

	TXmlNodeMap        m_definitionList;

	static void ReloadUseReferenceTables(XmlNodeRef definition);
	void        Create(XmlNodeRef definitionNode);
	XmlNodeRef  GetDefinition(const char* definitionName);
	XmlNodeRef  GetPropertyDefinition(const char* attributeName);
	bool        IsArray(XmlNodeRef def);
	bool        IsTable(XmlNodeRef def);
	void        Dump();

private:
	static void GetUseReferenceTables(XmlNodeRef definition);
};

#endif

