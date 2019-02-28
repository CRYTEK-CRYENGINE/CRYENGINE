// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IXMLSERIALIZER_H__
#define __IXMLSERIALIZER_H__

#include <CrySystem/XML/IXml.h>
#include <cstdio>
class IXMLDataSink;
class IXMLDataSource;

struct IXmlBufferSource
{
	virtual int Read(void* buffer, int size) const = 0;
};

class FileXmlBufferSource : public IXmlBufferSource
{
public:
	explicit FileXmlBufferSource(const char* path)
	{
		file = std::fopen(path, "r");
	}
	explicit FileXmlBufferSource(const wchar_t* path)
	{
		file = _wfopen(path, L"r");
	}
	~FileXmlBufferSource()
	{
		if (file)
			std::fclose(file);
	}

	virtual int Read(void* buffer, int size) const
	{
		if (!file)
			return 0;
		return check_cast<int>(std::fread(buffer, 1, size, file));
	}

private:
	mutable std::FILE* file;
};

class IXMLSerializer
{
public:
	virtual XmlNodeRef CreateNode(const char *tag) = 0;
	virtual bool Write(XmlNodeRef root, const char* szFileName) = 0;

	virtual XmlNodeRef Read(const IXmlBufferSource& source, bool bRemoveNonessentialSpacesFromContent, int nErrorBufferSize, char* szErrorBuffer) = 0;
};

#endif //__IXMLSERIALIZER_H__
