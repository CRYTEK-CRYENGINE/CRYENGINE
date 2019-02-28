// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PROCEDURAL_CLIP_CONVERSION__H__
#define __PROCEDURAL_CLIP_CONVERSION__H__
#pragma once

#include <vector>
#include <map>

struct SProcClipConversionEntry
{
	typedef std::vector<string> ParamConversionStringList;

	XmlNodeRef Convert(const XmlNodeRef& pOldNode) const;

	XmlNodeRef CreateNodeStructure(XmlNodeRef pNewNode, const ParamConversionStringList& paramList) const;

	void       CreateAttribute(XmlNodeRef pNewNode, const ParamConversionStringList& newAttributeName, const char* const newAttributeValue) const;
	void       ConvertAttribute(const XmlNodeRef& pOldNode, XmlNodeRef pNewNode, const char* const oldAttributeName, const ParamConversionStringList& newAttributeName) const;

	ParamConversionStringList animRef;
	ParamConversionStringList crcString;
	ParamConversionStringList dataString;

	typedef std::vector<ParamConversionStringList> ParameterConversionVector;
	ParameterConversionVector parameters;
};

class CProcClipConversionHelper
{
public:
	CProcClipConversionHelper();
	XmlNodeRef Convert(const XmlNodeRef& pOldXmlNode);

private:
	void LoadEntry(const XmlNodeRef& pXmlEntryNode);

	typedef std::map<string, SProcClipConversionEntry> ConversionEntryMap;
	ConversionEntryMap conversionMap;
};

#endif
