// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Metadata.h"
#include <Cry3DEngine/I3DEngine.h>
#include <Cry3DEngine/CGF/IChunkFile.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

namespace AssetLoader
{

const char* GetMetadataTag() { return "AssetMetadata"; }

// DODO: FIXME: the following classes parce cryasset xml manualy: 
// Code\CryEngine\RenderDll\Common\Textures\TextureCompiler.cpp
// Code\Sandbox\EditorQt\Alembic\AlembicCompiler.cpp

bool ReadMetadata(const XmlNodeRef& container, SAssetMetadata& metadata)
{
	assert(container);

	XmlNodeRef pMetadataNode = container->isTag(GetMetadataTag()) ? container : container->findChild(GetMetadataTag());

	if (!pMetadataNode || !pMetadataNode->getAttr("version", metadata.version) || (metadata.version > SAssetMetadata::eVersion_Actual))
	{
		return false;
	}
	metadata.type = pMetadataNode->getAttr("type");
	if (pMetadataNode->haveAttr("guid"))
	{
		const string guid = pMetadataNode->getAttr("guid");
		metadata.guid = CryGUID::FromString(guid);
	}

	metadata.sourceFile = pMetadataNode->getAttr("source");
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
			string detailName;
			if (!strcmp("Detail", pDetail->getTag()) && pDetail->getAttr("name", detailName))
			{
				metadata.details.emplace_back(detailName, pDetail->getContent());
			}
		}
	}
	XmlNodeRef pDependencies = pMetadataNode->findChild("Dependencies");
	if (pDependencies)
	{
		const int count = pDependencies->getChildCount();
		metadata.dependencies.reserve(count);
		for (int i = 0; i < count; ++i)
		{
			XmlNodeRef pDependency = pDependencies->getChild(i);
			if (!strcmp("Path", pDependency->getTag()))
			{
				int32 instanceCount = 0;
				pDependency->getAttr("usageCount", instanceCount);
				metadata.dependencies.emplace_back(PathUtil::ToUnixPath(pDependency->getContent()), instanceCount);
			}
		}
	}
	return true;
}

// See also RC's version of WriteMetaData.
void WriteMetaData(const XmlNodeRef& asset, const SAssetMetadata& metadata)
{	
	CRY_ASSERT(metadata.guid != CryGUID::Null());

	XmlNodeRef pMetadataNode = asset->isTag(GetMetadataTag())? asset : asset->findChild(GetMetadataTag());
	if (!pMetadataNode)
	{
		pMetadataNode = asset->createNode(GetMetadataTag());
		asset->insertChild(0, pMetadataNode);
	}
	pMetadataNode->setAttr("version", metadata.version);
	pMetadataNode->setAttr("type", metadata.type);
	pMetadataNode->setAttr("guid", metadata.guid.ToString().c_str());
	pMetadataNode->setAttr("source", metadata.sourceFile);

	XmlNodeRef pFiles = pMetadataNode->findChild("Files");
	if (!pFiles)
	{
		pFiles = pMetadataNode->newChild("Files");
	}

	for (const string& file : metadata.files)
	{
		XmlNodeRef pFile = pFiles->newChild("File");
		pFile->setAttr("path", file);
	}

	if (!metadata.details.empty())
	{
		XmlNodeRef pDetails = pMetadataNode->findChild("Details");
		if (!pDetails)
		{
			pDetails = pMetadataNode->newChild("Details");
		}

		for (auto& detail : metadata.details)
		{
			XmlNodeRef pDetail = pDetails->newChild("Detail");
			pDetail->setAttr("name", detail.first);
			pDetail->setContent(detail.second);
		}
	}

	if (!metadata.dependencies.empty())
	{
		XmlNodeRef pFiles = pMetadataNode->findChild("Dependencies");
		if (!pFiles)
		{
			pFiles = pMetadataNode->newChild("Dependencies");
		}
		for (const auto& item : metadata.dependencies)
		{
			const string& file = item.first;
			XmlNodeRef pFile = pFiles->newChild("Path");
			if (item.second)
			{
				pFile->setAttr("usageCount", item.second);
			}
			pFile->setContent(file);
		}
	}
}

}

