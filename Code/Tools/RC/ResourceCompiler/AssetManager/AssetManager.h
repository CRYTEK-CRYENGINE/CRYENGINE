// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IAssetManager.h"

struct IResourceCompiler;
class IConfig;
class XmlNodeRef;

class CAssetManager : public IAssetManager
{
public:
	CAssetManager(IResourceCompiler* pRc);
	virtual void RegisterDetailProvider(FnDetailsProvider detailsProvider, const char* szExt) override;
	virtual bool SaveCryasset(const IConfig* const pConfig, const char* szSourceFilepath, size_t filesCount, const char** pFiles, const char* szOutputFolder) const override;

private:
	bool CollectMetadataDetails(XmlNodeRef& xml, const std::vector<string>& files) const;
private:
	IResourceCompiler* m_pRc;

	VectorMap<string, FnDetailsProvider> m_providers;
};