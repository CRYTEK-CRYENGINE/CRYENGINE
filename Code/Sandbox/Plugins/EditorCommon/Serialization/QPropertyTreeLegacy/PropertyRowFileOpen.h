// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QObject>
#include "Serialization/PropertyTreeLegacy/PropertyRow.h"

class PropertyRowFileOpen;
class PropertyTreeLegacy;

struct FileOpenMenuHandler : PropertyRowMenuHandler
{
	PropertyRowFileOpen* self;
	PropertyTreeLegacy* tree;

	FileOpenMenuHandler(PropertyRowFileOpen* self, PropertyTreeLegacy* tree)
	: self(self)
	, tree(tree)
	{
	}

	void onMenuActivate();
	void onMenuClear();
};

yasli::string extractExtensionFromFilter(const char* fileSelectorFilter);
