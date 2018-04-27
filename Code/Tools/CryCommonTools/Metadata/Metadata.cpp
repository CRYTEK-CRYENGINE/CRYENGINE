// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Metadata.h"
#include "IConverter.h"


namespace AssetManager
{

const char* GetMetadataTag() { return "AssetMetadata"; }

bool WriteMetadata(const XmlNodeRef& asset, const SAssetMetadata& metadata)
{
	XmlNodeRef pMetadataNode = GetMetadataNode(asset);
	if (!pMetadataNode)
	{
		pMetadataNode = asset->createNode(GetMetadataTag());
		asset->insertChild(0, pMetadataNode);
	}
	pMetadataNode->setAttr("version", metadata.version);
	pMetadataNode->setAttr("type", metadata.type);

	CRY_ASSERT(metadata.guid != CryGUID::Null());
	pMetadataNode->setAttr("guid", metadata.guid.ToString().c_str());

	if (!metadata.source.empty())
	{
		pMetadataNode->setAttr("source", metadata.source);
	}
	else
	{
		pMetadataNode->delAttr("source");
	}

	XmlNodeRef pFiles = pMetadataNode->findChild("Files");
	if (!pFiles)
	{
		pFiles = pMetadataNode->newChild("Files");
	}
	else
	{
		pFiles->removeAllChilds();
	}

	for (const string& file : metadata.files)
	{
		XmlNodeRef pFile = pFiles->newChild("File");
		pFile->setAttr("path", file);
	}

	XmlNodeRef pDetails = pMetadataNode->findChild("Details");
	if (!pDetails)
	{
		pDetails = pMetadataNode->newChild("Details");
	}
	else
	{
		pDetails->removeAllChilds();
	}

	for (const auto& detail : metadata.details)
	{
		XmlNodeRef pDetail = pDetails->newChild("Detail");
		pDetail->setAttr("name", detail.first);
		pDetail->setContent(detail.second);
	}
	return true;
}

bool ReadMetadata(const XmlNodeRef& asset, SAssetMetadata& metadata)
{
	XmlNodeRef pMetadataNode = GetMetadataNode(asset);

	if (!pMetadataNode || !pMetadataNode->getAttr("version", metadata.version) || (metadata.version > SAssetMetadata::eVersion_Actual))
	{
		return false;
	}
	metadata.type = pMetadataNode->getAttr("type");
	metadata.source = pMetadataNode->getAttr("source");
	if (pMetadataNode->haveAttr("guid"))
	{
		const string guid = pMetadataNode->getAttr("guid");
		metadata.guid = CryGUID::FromString(guid);
	}

	XmlNodeRef pFiles = pMetadataNode->findChild("Files");
	if (pFiles)
	{
		metadata.files.reserve(pFiles->getChildCount());
		for (int i = 0, n = pFiles->getChildCount(); i < n; ++i)
		{
			XmlNodeRef pFile = pFiles->getChild(i);
			metadata.files.emplace_back(pFile->getAttr("path"));
		}
	}

	XmlNodeRef pDetails = pMetadataNode->findChild("Details");
	if (pDetails)
	{
		const int detailCount = pDetails->getChildCount();
		metadata.details.reserve(detailCount);
		for (int i = 0; i < detailCount; ++i)
		{
			XmlNodeRef pDetail = pDetails->getChild(i);
			const char* szDetailName;
			if (!strcmp("Detail", pDetail->getTag()) && pDetail->getAttr("name", &szDetailName))
			{
				metadata.details.emplace_back(szDetailName, pDetail->getContent());
			}
		}
	}

	return true;
}

const XmlNodeRef GetMetadataNode(const XmlNodeRef& asset)
{
	return asset->isTag(GetMetadataTag()) ? asset : asset->findChild(GetMetadataTag());
}

void AddDetails(XmlNodeRef& xml, const std::vector<std::pair<string, string>>& details)
{
	if (details.empty())
	{
		return;
	}

	XmlNodeRef pDetails = xml->findChild("Details");
	if (pDetails)
	{
		pDetails->removeAllChilds();
	}
	else
	{
		pDetails = xml->newChild("Details");
	}

	for (const auto& detail : details)
	{
		XmlNodeRef pDetail = pDetails->newChild("Detail");
		pDetail->setAttr("name", detail.first);
		pDetail->setContent(detail.second);
	}
}

void AddDependencies(XmlNodeRef & xml, const std::vector<std::pair<string,int32>>& dependencies)
{
	if (dependencies.empty())
	{
		return;
	}

	XmlNodeRef pDependencies = xml->findChild("Dependencies");
	if (pDependencies)
	{
		pDependencies->removeAllChilds();
	}
	else
	{
		pDependencies = xml->newChild("Dependencies");
	}

	for (const auto& item : dependencies)
	{
		XmlNodeRef pPath = pDependencies->newChild("Path");

		if (item.second)
		{
			pPath->setAttr("usageCount", item.second);
		}

		// There is an agreement in the sandbox dependency tracking system that local paths have to start with "./"
		if (item.first.FindOneOf("/\\") != string::npos)
		{
			pPath->setContent(item.first);
		}
		else // Folow the engine rules: if no slashes in the name assume it is in same folder as the cryasset.
		{
			pPath->setContent(string().Format("./%s", item.first.c_str()));
		}
	}
}


}
