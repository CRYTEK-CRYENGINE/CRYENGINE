// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __XMLSERIALIZER_H__
#define __XMLSERIALIZER_H__

#include "IXMLSerializer.h"

class XMLSerializer : public IXMLSerializer
{
public:
	virtual XmlNodeRef CreateNode(const char *tag);
	virtual bool Write(XmlNodeRef root, const char* szFileName);

	virtual XmlNodeRef Read(const IXmlBufferSource& source, bool bRemoveNonessentialSpacesFromContent, int nErrorBufferSize, char* szErrorBuffer);
};

#endif //__XMLSERIALIZER_H__
