// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ReadWriteXMLSink.h"

#include <stack>

typedef std::map<string, XmlNodeRef> IdTable;

static bool IsOptionalWriteXML(XmlNodeRef& definition);

static bool SaveTableInner(const IdTable&, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource);
static bool SaveReferencedId(const IdTable&, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource);
static bool SaveSomething(const IdTable&, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource);
static bool SaveArray(const IdTable&, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource);
static bool SaveProperty(const IdTable&, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource);
static bool SaveTable(const IdTable&, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource);
static bool SaveArraySetValueTable(const IdTable&, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource, int elem);

typedef bool (* SaveArraySetValue)(const IdTable&, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource, int elem);
typedef bool (* SaveDefinitionFunction)(const IdTable&, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource);

template<class T> struct WritePropertyTyped;

template<class T>
struct WritePropertyTyped
{
	static bool Save(const char* name, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource)
	{
		IWriteXMLSource::TValue vvalue((T()));
		if (!pSource->GetValue(name, vvalue, definition))
			return false;
		T* pValue = stl::get_if<T>(&vvalue);
		if (!pValue)
			return false;
		data->setAttr(name, *pValue);
		return true;
	}
	static bool SaveArray(const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource, int elem)
	{
		IWriteXMLSource::TValue vvalue((T()));
		if (!pSource->GetAt(elem, vvalue, definition))
			return false;
		T* pValue = stl::get_if<T>(&vvalue);
		if (!pValue)
			return false;
		data->setAttr("value", *pValue);
		return true;
	}
};

template<>
struct WritePropertyTyped<string> : public WritePropertyTyped<const char*>
{
};

bool IsOptionalWriteXML(XmlNodeRef& definition)
{
	bool optional = false;
	definition->getAttr("optional", optional);
	return optional;
}

bool SaveProperty(const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource)
{
	const char* name = definition->getAttr("name");
	if (0 == strlen(name))
	{
		CryLog("Property has no name");
		return false;
	}

	const char* type = definition->getAttr("type");
	if (0 == strlen(type))
	{
		CryLog("Property '%s' has no type", type);
		return false;
	}

	if (IsOptionalWriteXML(definition) && !pSource->HaveValue(name))
		return true;

	bool ok = false;
#define SAVE_PROPERTY(whichType) else if (0 == strcmp(type, # whichType)) \
  ok = WritePropertyTyped<whichType>::Save(name, definition, data, pSource)
	XML_SET_PROPERTY_HELPER(SAVE_PROPERTY);
#undef SAVE_PROPERTY

	if (!ok)
		CryLog("Failed loading attribute %s of type %s", name, type);

	return ok;
}

bool SaveArraySetValueTable(const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource, int elem)
{
	IWriteXMLSourcePtr pChildSource = pSource->BeginTableAt(elem);
	if (!pChildSource)
	{
		CryLog("Failed to find source table at %d", elem);
		return false;
	}

	if (!SaveTableInner(idTable, definition, data, &*pChildSource))
		return false;

	if (!pChildSource->EndTableAt(elem))
	{
		CryLog("Failed to finish table at element %d", elem);
		return false;
	}

	return true;
}

bool SaveArray(const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource)
{
	const char* name = definition->getAttr("name");
	if (0 == strlen(name))
	{
		CryLog("Array has no name");
		return false;
	}

	const char* elementName = definition->getAttr("elementName");
	if (0 == strlen(elementName))
		elementName = "element";

	bool validateArray = true;
	if (definition->haveAttr(elementName))
		definition->getAttr("validate", validateArray);

	size_t numElems = 0;
	IWriteXMLSourcePtr childSource = pSource->BeginArray(name, &numElems, definition);
	if (!childSource)
	{
		bool ok = IsOptionalWriteXML(definition);
		if (!ok)
			CryLog("Failed to begin array named %s", name);
		return ok;
	}

	XmlNodeRef childData = data->createNode(name);

	SaveArraySetValue setter = NULL;
	if (definition->haveAttr("type"))
	{
		setter = NULL;
		const char* type = definition->getAttr("type");
#define SETTER_PROPERTY(whichType) else if (0 == strcmp(type, # whichType)) \
  setter = WritePropertyTyped<whichType>::SaveArray
		XML_SET_PROPERTY_HELPER(SETTER_PROPERTY);
#undef SETTER_PROPERTY
		if (!setter)
		{
			CryLog("Unknown type %s in array %s", type, name);
			return false;
		}
	}
	else
	{
		setter = SaveArraySetValueTable;
	}

	bool needIndex = false;
	for (size_t i = 1; i <= numElems; i++)
	{
		if (!childSource->HaveElemAt(i))
			needIndex = true;
		else
		{
			XmlNodeRef elemData = childData->createNode(elementName);
			if (needIndex)
				elemData->setAttr("_index", i);
			needIndex = false;

			if (!setter(idTable, definition, elemData, &*childSource, i))
			{
				CryLog("Failed saving element %d of array %s", int(i), name);
				return false;
			}

			childData->addChild(elemData);
		}
	}

	if (!pSource->EndArray(name))
	{
		CryLog("Failed to finish array named %s", name);
		return false;
	}

	data->addChild(childData);

	return true;
}

bool SaveTable(const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource)
{
	const char* name = definition->getAttr("name");
	if (0 == strlen(name))
	{
		CryLog("Child-table has no name");
		return false;
	}

	IWriteXMLSourcePtr childSource = pSource->BeginTable(name);
	if (!childSource)
	{
		bool ok = IsOptionalWriteXML(definition);
		if (!ok)
			CryLog("Source creation failed for table %s", name);
		return ok;
	}

	XmlNodeRef childData = data->createNode(name);

	if (!SaveTableInner(idTable, definition, childData, childSource))
	{
		CryLog("Failed to load data for child table %s", name);
		return false;
	}

	if (!pSource->EndTable(name))
	{
		CryLog("Table %s failed to complete in sink", name);
		return false;
	}

	data->addChild(childData);

	return true;
}

bool SaveSomething(const IdTable& idTable, XmlNodeRef& nodeDefinition, XmlNodeRef& data, IWriteXMLSource* pSource)
{
	static struct
	{
		const char*            name;
		SaveDefinitionFunction saver;
	} saverTypes[] = {
		{ "Property", &SaveProperty     },
		{ "Array",    &SaveArray        },
		{ "Table",    &SaveTable        },
		{ "Use",      &SaveReferencedId },
	};
	static const int numSaverTypes = sizeof(saverTypes) / sizeof(*saverTypes);

	const char* nodeDefinitionTag = nodeDefinition->getTag();
	bool ok = false;
	int i;
	for (i = 0; i < numSaverTypes; i++)
	{
		if (0 == strcmp(saverTypes[i].name, nodeDefinitionTag))
		{
			ok = saverTypes[i].saver(idTable, nodeDefinition, data, pSource);
			break;
		}
	}
	if (!ok)
	{
		if (i == numSaverTypes)
			CryLog("Invalid definition node type %s", nodeDefinitionTag);
	}
	return ok;
}

bool SaveReferencedId(const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource)
{
	IdTable::const_iterator iter = idTable.find(definition->getAttr("id"));
	if (iter == idTable.end())
	{
		CryLog("No definition with id '%s'", definition->getAttr("id"));
		return false;
	}
	XmlNodeRef useDefinition = iter->second;
	useDefinition = useDefinition->clone();
	int numAttrs = definition->getNumAttributes();
	for (int i = 0; i < numAttrs; i++)
	{
		const char* key, * value;
		definition->getAttributeByIndex(i, &key, &value);
		useDefinition->setAttr(key, value);
	}
	return SaveSomething(idTable, useDefinition, data, pSource);
}

bool SaveTableInner(const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IWriteXMLSource* pSource)
{
	const int nChildrenDefinition = definition->getChildCount();
	for (int nChildDefinition = 0; nChildDefinition < nChildrenDefinition; nChildDefinition++)
	{
		XmlNodeRef nodeDefinition = definition->getChild(nChildDefinition);

		if (!SaveSomething(idTable, nodeDefinition, data, pSource))
			return false;
	}
	return true;
}

XmlNodeRef CReadWriteXMLSink::CreateXMLFromSource(const char* definitionFile, IWriteXMLSource* pSource)
{
	XmlNodeRef rootDefinition = GetISystem()->LoadXmlFromFile(definitionFile);
	if (!rootDefinition)
	{
		CryLog("Unable to load XML-Lua definition file: %s", definitionFile);
		return 0;
	}
	if (0 != strcmp(rootDefinition->getTag(), "Definition"))
	{
		CryLog("Root tag of definition file was %s; expected Definition", rootDefinition->getTag());
		return 0;
	}
	const char* rootNode = "Root";
	if (rootDefinition->haveAttr("root"))
		rootNode = rootDefinition->getAttr("root");
	XmlNodeRef rootData = GetISystem()->CreateXmlNode(rootNode);

	XmlNodeRef allowAlways = rootDefinition->findChild("AllowAlways");
	if (allowAlways != 0)
		rootDefinition->removeChild(allowAlways);

	XmlNodeRef settingsParams = rootDefinition->findChild("Settings");
	if (settingsParams != 0)
		rootDefinition->removeChild(settingsParams);

	// scan for id's in the structure (for the Use member)
	IdTable idTable;
	std::stack<XmlNodeRef> scanStack;
	scanStack.push(rootDefinition);
	while (!scanStack.empty())
	{
		XmlNodeRef refNode = scanStack.top();
		scanStack.pop();

		int numChildren = refNode->getChildCount();
		const char* tag = refNode->getTag();

		for (int i = 0; i < numChildren; i++)
			scanStack.push(refNode->getChild(i));

		if (refNode->haveAttr("id") && 0 != strcmp("Use", tag))
			idTable[refNode->getAttr("id")] = refNode;

		if (allowAlways != 0 && (!strcmp("Table", tag) || !strcmp("Array", tag)))
			for (int i = 0; i < allowAlways->getChildCount(); ++i)
				refNode->addChild(allowAlways->getChild(i)->clone());
	}

	if (!SaveTableInner(idTable, rootDefinition, rootData, pSource))
	{
		CryLog("Error createing xml using definition %s", definitionFile);
		return 0;
	}

	bool ok = pSource->Complete();
	if (!ok)
	{
		CryLog("Warning: sink failed to complete writing");
		return 0;
	}

	return rootData;
}

bool CReadWriteXMLSink::WriteXML(const char* definitionFile, const char* dataFile, IWriteXMLSource* pSource)
{
	XmlNodeRef data = CreateXMLFromSource(definitionFile, pSource);
	if (!data)
	{
		CryLog("Failed creating %s", dataFile);
		return false;
	}
	if (!data->saveToFile(dataFile))
	{
		CryLog("Failed saving %s", dataFile);
		return false;
	}
	return true;
}
