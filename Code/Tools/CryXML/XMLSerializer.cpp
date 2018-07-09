// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "XMLSerializer.h"
#include "xml/xml.h"
#include "IXMLSerializer.h"
#include <CryString/StringUtils.h>

XmlNodeRef XMLSerializer::CreateNode(const char *tag)
{
	return new CXmlNode(tag);
}

bool XMLSerializer::Write(XmlNodeRef root, const char* szFileName)
{
	return root->saveToFile(szFileName);
}

XmlNodeRef XMLSerializer::Read(const IXmlBufferSource& source, bool bRemoveNonessentialSpacesFromContent, int nErrorBufferSize, char* szErrorBuffer)
{
	XmlParser parser(bRemoveNonessentialSpacesFromContent);
	XmlNodeRef root = parser.parseSource(&source);
	if (nErrorBufferSize > 0 && szErrorBuffer)
	{
		const char* const err = parser.getErrorString();
		cry_strcpy(szErrorBuffer, nErrorBufferSize, err ? err : "");
	}
	return root;
}
