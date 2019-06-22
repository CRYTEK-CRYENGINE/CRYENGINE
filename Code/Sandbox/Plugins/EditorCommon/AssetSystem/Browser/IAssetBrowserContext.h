// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorFramework/WidgetActionRegistry.h"

struct EDITOR_COMMON_API IAssetBrowserContext : public IEditorContext
{
	virtual void GetSelection(std::vector<CAsset*>& assets, std::vector<string>& folders) const = 0;

	virtual std::vector<string> GetSelectedFolders() const = 0;
};

