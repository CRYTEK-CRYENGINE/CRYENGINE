// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PAKXMLFILEBUFFERSOURCE_H__
#define __PAKXMLFILEBUFFERSOURCE_H__

#include "../CryXML/IXMLSerializer.h"
#include "IPakSystem.h"

class PakXmlFileBufferSource : public IXmlBufferSource
{
public:
	PakXmlFileBufferSource(IPakSystem* pakSystem, const char* path)
		: pakSystem(pakSystem)
	{
		file = pakSystem->Open(path, "r");
	}
	~PakXmlFileBufferSource()
	{
		if (file)
			pakSystem->Close(file);
	}

	virtual int Read(void* buffer, int size) const
	{
		return pakSystem->Read(file, buffer, size);
	};

	IPakSystem* pakSystem;
	PakSystemFile* file;
};

class PakXmlBufferSource : public IXmlBufferSource
{
public:
	PakXmlBufferSource(const char* buffer, size_t length)
	: position(buffer), end(buffer + length)
	{		
	}
	
	virtual int Read(void* output, int size) const
	{
		size_t bytesLeft = end - position;
		size_t bytesToCopy = size < bytesLeft ? size : bytesLeft;
		if (bytesToCopy > 0)
		{
			memcpy(output, position, bytesToCopy);
			position += bytesToCopy;
		}
		return bytesToCopy;
	};

	mutable const char* position;
	const char* end;
};

#endif //__PAKXMLFILEBUFFERSOURCE_H__
