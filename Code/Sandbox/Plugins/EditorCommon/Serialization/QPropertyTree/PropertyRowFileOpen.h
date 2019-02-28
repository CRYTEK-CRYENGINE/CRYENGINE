// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QObject>
#include "Serialization/PropertyTree/PropertyRow.h"

class PropertyRowFileOpen;
class PropertyTree;

struct FileOpenMenuHandler : PropertyRowMenuHandler
{
	PropertyRowFileOpen* self;
	PropertyTree* tree;

	FileOpenMenuHandler(PropertyRowFileOpen* self, PropertyTree* tree)
	: self(self)
	, tree(tree)
	{
	}

	void onMenuActivate();
	void onMenuClear();
};

yasli::string extractExtensionFromFilter(const char* fileSelectorFilter);

