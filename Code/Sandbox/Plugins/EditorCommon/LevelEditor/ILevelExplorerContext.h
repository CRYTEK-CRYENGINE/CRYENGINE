// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorFramework/WidgetActionRegistry.h"

struct IObjectLayer;

struct EDITOR_COMMON_API ILevelExplorerContext : public IEditorContext
{
	virtual void GetSelection(std::vector<IObjectLayer*>& layers, std::vector<IObjectLayer*>& folders) const = 0;

	virtual std::vector<IObjectLayer*> GetSelectedIObjectLayers() const = 0;
};

