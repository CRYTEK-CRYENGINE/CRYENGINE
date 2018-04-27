// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <StdAfx.h>
#include <Serialization/PropertyTree/IDrawContext.h>
#include <Serialization/PropertyTree/PropertyRow.h>
#include "Serialization/QPropertyTree/QPropertyTree.h"
#include "Serialization.h"
#include "CrySerialization/Math.h"

class PropertyRowGap : public PropertyRow
{
public:
	bool            isLeaf() const override                                { return true; }
	bool            isStatic() const override                              { return false; }

	WidgetPlacement widgetPlacement() const override                       { return WIDGET_AFTER_PULLED; }
	int             widgetSizeMax(const PropertyTree* tree) const override { return 10; }
};

namespace Serialization
{
REGISTER_PROPERTY_ROW(SGap, PropertyRowGap);
}

