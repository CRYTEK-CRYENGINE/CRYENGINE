// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryExtension\CryGUID.h"

class IXmlNode;

namespace AssetManager
{

//! The metadata structure intended to be one for all asset types.
struct SAssetMetadata
{
	enum EVersion { eVersion_Actual = 0 };

	int32               version = eVersion_Actual; //!< For data format control and conversion.
	string              type;                      //!< Class name of the type, resolved to class desc pointer at runtime.
	CryGUID             guid = CryGUID::Null();
	std::vector<string> files;
	string              source;
	std::vector<std::pair<string, string>> details;
};

//! Returns name of the metadata root tag.
const char* GetMetadataTag();

//! Returns true if successful, otherwise returns false.
bool WriteMetadata(const XmlNodeRef& asset, const SAssetMetadata& metadata);

//! Returns true if successful, otherwise returns false.
bool ReadMetadata(const XmlNodeRef& asset, SAssetMetadata& metadata);

const XmlNodeRef GetMetadataNode(const XmlNodeRef& asset);
void AddDetails(XmlNodeRef& xml, const std::vector<std::pair<string, string>>& details);
void AddDependencies(XmlNodeRef& xml, const std::vector<std::pair<string, int32>>& dependencies);

}
