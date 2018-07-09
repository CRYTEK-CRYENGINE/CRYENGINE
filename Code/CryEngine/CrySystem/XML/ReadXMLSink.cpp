// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ReadWriteXMLSink.h"

#include <CrySystem/ISystem.h>
#include <stack>

typedef std::map<string, XmlNodeRef> IdTable;
struct SParseParams
{
	IdTable    idTable;
	XmlNodeRef useAlways;
	bool       strict;

	SParseParams()
	{
		strict = true;
	}
};

static XmlNodeRef Clone(XmlNodeRef source);
static void       CopyAttributes(const XmlNodeRef& source, XmlNodeRef& dest);
static bool       IsOptionalReadXML(const SParseParams& parseParams, XmlNodeRef& definition);
static bool       CheckEnum(const SParseParams& parseParams, const char* name, XmlNodeRef& definition, XmlNodeRef& data);

static bool       LoadTableInner(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink);
static bool       LoadArray(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink);
static bool       LoadProperty(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink);
static bool       LoadTable(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink);
static bool       LoadReferencedId(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink);
static bool       LoadSomething(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink);
static bool       LoadArraySetValueTable(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink, int elem);

typedef bool (* LoadArraySetValue)(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink, int elem);
typedef bool (* LoadDefinitionFunction)(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink);

template<class T> struct ReadPropertyTyped;

template<class T>
struct ReadPropertyTyped
{
	static bool Load(const SParseParams& parseParams, const char* name, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink)
	{
		T value;
		memset(&value, 0, sizeof(T));

		if (!pSink->IsCreationMode())
		{
			if (!data->haveAttr(name))
				return false;
			if (!data->getAttr(name, value))
				return false;
			if (!CheckEnum(parseParams, name, definition, data))
				return false;
		}

		IReadXMLSink::TValue vvalue(value);
		pSink->SetValue(name, vvalue, definition);
		return true;
	}
	static bool LoadArray(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink, int elem)
	{
		T value;
		memset(&value, 0, sizeof(T));

		if (!pSink->IsCreationMode())
		{
			if (!data->haveAttr("value"))
				return false;
			if (!data->getAttr("value", value))
				return false;
		}

		IReadXMLSink::TValue vvalue(value);
		pSink->SetAt(elem, vvalue, definition);
		return true;
	}
};

template<>
struct ReadPropertyTyped<string>
{
	static bool Load(const SParseParams& parseParams, const char* name, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink)
	{
		const char* value = 0;

		if (!pSink->IsCreationMode())
		{
			if (!data->haveAttr(name))
				return false;
			if (!CheckEnum(parseParams, name, definition, data))
				return false;

			value = data->getAttr(name);
		}

		IReadXMLSink::TValue vvalue(value);
		pSink->SetValue(name, vvalue, definition);
		return true;
	}
	static bool LoadArray(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink, int elem)
	{
		const char* value = 0;

		if (!pSink->IsCreationMode())
		{
			if (!data->haveAttr("value"))
				return false;

			value = data->getAttr("value");
		}

		IReadXMLSink::TValue vvalue(value);
		pSink->SetAt(elem, vvalue, definition);
		return true;
	}
};

XmlNodeRef Clone(XmlNodeRef source)
{
	assert(source != (IXmlNode*)NULL);

	// Can't use clone() on XmlNodeRef objects since they can contain a CXMLBinaryNode, which doesn't support
	// clone(). Instead we just create a regular xml node and manually copy content, tag, attributes and children
	XmlNodeRef cloned = GetISystem()->CreateXmlNode(source->getTag());
	cloned->setContent(source->getContent());
	CopyAttributes(source, cloned);
	const int iChildCount = source->getChildCount();
	for (int i = 0; i < iChildCount; ++i)
	{
		cloned->addChild(Clone(source->getChild(i)));
	}

	return cloned;
}

void CopyAttributes(const XmlNodeRef& source, XmlNodeRef& dest)
{
	// Not as fast as CXmlNode::copyAttributes(), but that method will have undefined behavior if the XmlNodeRef contains
	// a CBinaryXmlNode object
	int nNumAttributes = source->getNumAttributes();
	for (int i = 0; i < nNumAttributes; ++i)
	{
		const char* key = NULL;
		const char* value = NULL;
		if (source->getAttributeByIndex(i, &key, &value))
			dest->setAttr(key, value);
	}
}

bool IsOptionalReadXML(const SParseParams& parseParams, XmlNodeRef& definition)
{
	// If strict mode is off, then everything is optional
	if (parseParams.strict == false)
		return true;

	bool optional = false;
	definition->getAttr("optional", optional);
	return optional;
}

bool CheckEnum(const SParseParams& parseParams, const char* name, XmlNodeRef& definition, XmlNodeRef& data)
{
	if (XmlNodeRef enumNode = definition->findChild("Enum"))
	{
		// If strict mode is off, then no need to check the enum value
		return true;

		// if restrictive attribute set to false, check always succeeds
		if (enumNode->haveAttr("restrictive"))
		{
			bool res = true;
			enumNode->getAttr("restrictive", res);
			if (!res)
				return true;
		}

		// else check enum values
		const char* val = data->getAttr(name);
		for (int i = 0; i < enumNode->getChildCount(); ++i)
		{
			if (0 == strcmp(enumNode->getChild(i)->getContent(), val))
				return true;
		}
		return false;
	}
	return true;
}

bool LoadProperty(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink)
{
	if (strlen(definition->getAttr("deprecated")))
	{
		// skip deprecated properties by not loading them and returning success
		return true;
	}

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

	XmlNodeRef dataToRead = data;
	if (!pSink->IsCreationMode())
	{
		// This check is done so the data xml can specify child elements instead of attributes if desired,
		// since the rest of the code assume only attributes
		if (XmlNodeRef childRef = data->findChild(name))
		{
			if (data->haveAttr(name))
			{
				CryLog("Duplicate definition (attribute and element) for %s", name);
				return false;
			}
			if (childRef->getChildCount())
			{
				CryLog("Property-style elements can not have children (property was %s)", name);
				return false;
			}

			dataToRead = GetISystem()->CreateXmlNode(data->getTag());

			string content = childRef->getContent();
			dataToRead->setAttr(name, content.Trim().c_str());
		}

		if (!dataToRead->haveAttr(name))
		{
			if (!IsOptionalReadXML(parseParams, definition))
			{
				CryLog("Failed to load property %s", name);
				return false;
			}
			return true;
		}
	}

	bool ok = false;
#define LOAD_PROPERTY(whichType) else if (0 == strcmp(type, # whichType)) \
  ok = ReadPropertyTyped<whichType>::Load(parseParams, name, definition, dataToRead, pSink)
	XML_SET_PROPERTY_HELPER(LOAD_PROPERTY);
#undef LOAD_PROPERTY

	if (!ok)
		CryLog("Failed loading attribute %s of type %s", name, type);

	return ok;
}

bool LoadArraySetValueTable(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink, int elem)
{
	IReadXMLSinkPtr pChildSink = pSink->BeginTableAt(elem, definition);

	if (pSink->IsCreationMode() && definition->haveAttr("type"))
	{
		if (!LoadSomething(parseParams, definition, data, &*pChildSink))
			return false;
	}
	else
	{
		if (!LoadTableInner(parseParams, definition, data, &*pChildSink))
			return false;
	}

	if (!pChildSink->EndTableAt(elem))
	{
		CryLog("Failed to finish table at element %d", elem);
		return false;
	}

	return true;
}

bool LoadArray(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink)
{
	if (strlen(definition->getAttr("deprecated")))
	{
		// skip deprecated properties by not loading them and returning success
		return true;
	}

	const char* name = definition->getAttr("name");
	if (0 == strlen(name))
	{
		CryLog("Array has no name");
		return false;
	}

	bool validateArray = true;
	const char* elementName = definition->getAttr("elementName");
	if (0 == strlen(elementName))
	{
		elementName = "element";
		validateArray = false;
	}
	else
	{
		definition->getAttr("validate", validateArray);
	}

	XmlNodeRef childData;

	if (!pSink->IsCreationMode())
	{
		childData = data->findChild(name);
		if (!childData)
		{
			bool ok = IsOptionalReadXML(parseParams, definition);
			if (!ok)
				CryLog("Failed to load child table %s", name);
			return ok;
		}
	}

	IReadXMLSinkPtr childSink = pSink->BeginArray(name, definition);
	if (!childSink)
	{
		CryLog("Failed to begin array named %s", name);
		return false;
	}

	LoadArraySetValue setter = NULL;
	if (definition->haveAttr("type"))
	{
		setter = NULL;
		const char* type = definition->getAttr("type");
#define SETTER_PROPERTY(whichType) else if (0 == strcmp(type, # whichType)) \
  setter = ReadPropertyTyped<whichType>::LoadArray
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
		setter = LoadArraySetValueTable;
	}

	if (!pSink->IsCreationMode())
	{
		int numElems = childData->getChildCount();
		int elem = 1;
		for (int i = 0; i < numElems; i++)
		{
			XmlNodeRef elemData = childData->getChild(i);
			if (0 == strcmp(elemData->getTag(), elementName))
			{
				int increment = 1;
				if (elemData->haveAttr("_index"))
				{
					if (!elemData->getAttr("_index", elem))
					{
						CryLog("_index is not an integer in array %s (pos hint=%d)", name, elem);
						return false;
					}
				}
				if (!setter(parseParams, definition, elemData, &*childSink, elem))
				{
					CryLog("Failed loading element %d of array %s", elem, name);
					return false;
				}
				elem += increment;
			}
			else if (validateArray)
			{
				CryLog("Invalid node %s in array %s", elemData->getTag(), name);
				return false;
			}
		}
	}
	else
	{
		// only process array content for the array being created
		if (0 == strcmp(name, pSink->GetCreationNode()->getAttr("name")))
		{
			if (!setter(parseParams, definition, data, &*childSink, 1))
			{
				CryLog("[ReadXML CreationMode]: Failed loading element %d of array %s", 1, name);
				return false;
			}
		}
	}

	if (!pSink->EndArray(name))
	{
		CryLog("Failed to finish array named %s", name);
		return false;
	}

	return true;
}

bool LoadTable(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink)
{
	if (strlen(definition->getAttr("deprecated")))
	{
		// skip deprecated properties by not loading them and returning success
		return true;
	}

	const char* name = definition->getAttr("name");
	if (0 == strlen(name))
	{
		CryLog("Child-table has no name");
		return false;
	}

	XmlNodeRef childData;

	if (!pSink->IsCreationMode())
	{
		childData = data->findChild(name);
		if (!childData)
		{
			bool ok = IsOptionalReadXML(parseParams, definition);
			if (!ok)
				CryLog("Failed to load child table %s", name);
			return ok;
		}
	}

	IReadXMLSinkPtr childSink = pSink->BeginTable(name, definition);
	if (!childSink)
	{
		CryLog("Sink creation failed for table %s", name);
		return false;
	}

	if (!LoadTableInner(parseParams, definition, childData, childSink))
	{
		CryLog("Failed to load data for child table %s", name);
		return false;
	}

	if (!pSink->EndTable(name))
	{
		CryLog("Table %s failed to complete in sink", name);
		return false;
	}

	return true;
}

bool LoadSomething(const SParseParams& parseParams, XmlNodeRef& nodeDefinition, XmlNodeRef& data, IReadXMLSink* pSink)
{
	// Ignore if it's the useAlways array
	if (parseParams.useAlways == nodeDefinition)
		return true;

	static struct
	{
		const char*            name;
		LoadDefinitionFunction loader;
	} loaderTypes[] = {
		{ "Property", &LoadProperty     },
		{ "Array",    &LoadArray        },
		{ "Table",    &LoadTable        },
		{ "Use",      &LoadReferencedId },
	};
	static const int numLoaderTypes = sizeof(loaderTypes) / sizeof(*loaderTypes);

	const char* nodeDefinitionTag = nodeDefinition->getTag();
	bool ok = false;
	int i;

	for (i = 0; i < numLoaderTypes; i++)
	{
		if (0 == strcmp(loaderTypes[i].name, nodeDefinitionTag))
		{
			ok = loaderTypes[i].loader(parseParams, nodeDefinition, data, pSink);
			break;
		}
	}

	if (0 == stricmp("Settings", nodeDefinitionTag))
		return true;

	if (!ok)
	{
		if (i == numLoaderTypes)
			CryLog("Invalid definition node type %s, line %d", nodeDefinitionTag, nodeDefinition->getLine());
	}
	return ok;
}

bool LoadReferencedId(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink)
{
	IdTable::const_iterator iter = parseParams.idTable.find(definition->getAttr("id"));
	if (iter == parseParams.idTable.end())
	{
		CryLog("No definition with id '%s'", definition->getAttr("id"));
		return false;
	}
	XmlNodeRef useDefinition = Clone(iter->second);
	CopyAttributes(definition, useDefinition);

	return LoadSomething(parseParams, useDefinition, data, pSink);
}

bool LoadTableInner(const SParseParams& parseParams, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink* pSink)
{
	const int nChildrenDefinition = definition->getChildCount();

	for (int nChildDefinition = 0; nChildDefinition < nChildrenDefinition; nChildDefinition++)
	{
		XmlNodeRef nodeDefinition = definition->getChild(nChildDefinition);

		if (!LoadSomething(parseParams, nodeDefinition, data, pSink))
		{
			LoadSomething(parseParams, nodeDefinition, data, pSink);
			return false;
		}
	}

	const char* tag = definition->getTag();
	if (parseParams.useAlways != (IXmlNode*)NULL)
	{
		assert(!definition->haveAttr("type"));

		const int nUseAlwaysDefCount = parseParams.useAlways->getChildCount();
		for (int i = 0; i < nUseAlwaysDefCount; ++i)
		{
			XmlNodeRef nodeDefinition = parseParams.useAlways->getChild(i);

			// Don't continue loading useAlways nodes in creation mode
			if (!pSink->IsCreationMode())
			{
				if (!LoadSomething(parseParams, nodeDefinition, data, pSink))
					return false;
			}
		}
	}

	return true;
}

bool CReadWriteXMLSink::ReadXML(XmlNodeRef rootDefinition, XmlNodeRef rootData, IReadXMLSink* pSink)
{
	if (!pSink->IsCreationMode())
	{
		if (0 == rootData)
			return false;

		if (0 != strcmp(rootDefinition->getTag(), "Definition"))
		{
			CryLog("Root tag of definition file was %s; expected Definition", rootDefinition->getTag());
			return false;
		}
		if (rootDefinition->haveAttr("root"))
		{
			if (0 != strcmp(rootDefinition->getAttr("root"), rootData->getTag()))
			{
				CryLog("Root data has wrong tag; was %s expected %s", rootData->getTag(), rootDefinition->getAttr("root"));
				return false;
			}
		}
	}

	SParseParams parseParams;
	parseParams.useAlways = rootDefinition->findChild("AllowAlways");

	if (XmlNodeRef settingsParams = rootDefinition->findChild("Settings"))
	{
		settingsParams->getAttr("strict", parseParams.strict);
	}

	// scan for id's in the structure (for the Use member)
	std::stack<XmlNodeRef> scanStack;
	scanStack.push(rootDefinition);
	while (!scanStack.empty())
	{
		XmlNodeRef refNode = scanStack.top();
		scanStack.pop();

		int numChildren = refNode->getChildCount();
		const char* tag = refNode->getTag();

		for (int i = 0; i < numChildren; i++)
		{
			const XmlNodeRef& childNodeRef = refNode->getChild(i);
			if (parseParams.useAlways != childNodeRef)
				scanStack.push(childNodeRef);
		}

		// If the element has an attribute id="" and is not a "<Use>" element add it to the idTable map
		if (refNode->haveAttr("id") && 0 != strcmp("Use", tag))
			parseParams.idTable[refNode->getAttr("id")] = refNode;
	}

	if (pSink->IsCreationMode() && rootDefinition->haveAttr("type"))
	{
		// if creating from a 0-child definition node, load itself
		if (!LoadSomething(parseParams, rootDefinition, rootData, pSink))
			return false;
	}
	else
	{
		// load content
		if (!LoadTableInner(parseParams, rootDefinition, rootData, pSink))
			return false;
	}

	bool ok = pSink->Complete();
	if (!ok)
		CryLog("Warning: sink failed to complete reading");

	return ok;
}

bool CReadWriteXMLSink::ReadXML(XmlNodeRef definition, const char* dataFile, IReadXMLSink* pSink)
{
	XmlNodeRef rootData = GetISystem()->LoadXmlFromFile(dataFile);
	if (!rootData)
	{
		CryLog("Unable to load XML-Lua data file: %s", dataFile);
		return false;
	}
	return ReadXML(definition, rootData, pSink);
}

bool CReadWriteXMLSink::ReadXML(const char* definitionFile, XmlNodeRef rootData, IReadXMLSink* pSink)
{
	XmlNodeRef rootDefinition = GetISystem()->LoadXmlFromFile(definitionFile);
	if (!rootDefinition)
	{
		CryLog("Unable to load XML-Lua definition file: %s", definitionFile);
		return false;
	}
	return ReadXML(rootDefinition, rootData, pSink);
}

bool CReadWriteXMLSink::ReadXML(const char* definitionFile, const char* dataFile, IReadXMLSink* pSink)
{
	XmlNodeRef rootData = GetISystem()->LoadXmlFromFile(dataFile);
	if (!rootData)
	{
		CryLog("Unable to load XML-Lua data file: %s", dataFile);
		return false;
	}
	if (!ReadXML(definitionFile, rootData, pSink))
	{
		CryLog("Unable to load file %s", dataFile);
		return false;
	}
	return true;
}
