// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __READWRITEXMLSINK_H__
#define __READWRITEXMLSINK_H__

#pragma once

#include <CrySystem/ISystem.h>
#include <CrySystem/XML/IReadWriteXMLSink.h>

class CReadWriteXMLSink : public IReadWriteXMLSink
{
public:
	bool       ReadXML(const char* definitionFile, const char* dataFile, IReadXMLSink* pSink);
	bool       ReadXML(const char* definitionFile, XmlNodeRef node, IReadXMLSink* pSink);
	bool       ReadXML(XmlNodeRef definition, const char* dataFile, IReadXMLSink* pSink);
	bool       ReadXML(XmlNodeRef definition, XmlNodeRef node, IReadXMLSink* pSink);

	XmlNodeRef CreateXMLFromSource(const char* definitionFile, IWriteXMLSource* pSource);
	bool       WriteXML(const char* definitionFile, const char* dataFile, IWriteXMLSource* pSource);
};

// helper to define the if/else chain that we need in a few locations...
// types must match IReadXMLSink::TValueTypes
#define XML_SET_PROPERTY_HELPER(ELSE_LOAD_PROPERTY) \
  if (false);                                       \
  ELSE_LOAD_PROPERTY(Vec3);                         \
  ELSE_LOAD_PROPERTY(int);                          \
  ELSE_LOAD_PROPERTY(float);                        \
  ELSE_LOAD_PROPERTY(string);                       \
  ELSE_LOAD_PROPERTY(bool);

#endif // __READWRITEXMLSINK_H__
