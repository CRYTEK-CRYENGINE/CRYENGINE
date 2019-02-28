// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __XMLPAKFILESINK_H__
#define __XMLPAKFILESINK_H__

#include "XMLWriter.h"
#include "IPakSystem.h"

class XMLPakFileSink : public IXMLSink
{
public:
	XMLPakFileSink(IPakSystem* pakSystem, const string& archivePath, const string& filePath);
	~XMLPakFileSink();

	// IXMLSink
	virtual void Write(const char* text);

private:
	IPakSystem* pakSystem;
	PakSystemArchive* archive;
	string filePath;
	std::vector<char> data;
};

#endif //__XMLPAKFILESINK_H__
