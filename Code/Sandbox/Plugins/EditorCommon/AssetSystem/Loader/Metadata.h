// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryExtension/CryGUID.h"

class IXmlNode;
struct IChunkFile;

namespace AssetLoader
{

//! The metadata structure intended to be one for all asset types.
struct SAssetMetadata
{
	enum EVersion { eVersion_Actual = 0 };

	int32  version = eVersion_Actual;   //!< For data format control and conversion.
	string type;                        //!< Class name of the type, resolved to class desc pointer at runtime.
	CryGUID guid = CryGUID::Null();
	string sourceFile;
	std::vector<string> files;
	std::vector<std::pair<string, string>> details;
	std::vector<std::pair<string, int32>> dependencies;
};

//! Returns name of the metadata root tag.
const char* GetMetadataTag();

//! Returns true if successful, otherwise returns false.
bool ReadMetadata(const XmlNodeRef& asset, SAssetMetadata& metadata);

void WriteMetaData(const XmlNodeRef& asset, const SAssetMetadata& metadata);

}

