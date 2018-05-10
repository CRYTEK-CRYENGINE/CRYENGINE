/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include <math.h>

#include "Factory.h"
#include "PropertyRowStringListValue.h"
#include "PropertyTreeModel.h"
#include "IDrawContext.h"
#if YASLI_INCLUDE_PROPERTY_TREE_CONFIG_LOCAL
#include <CrySerialization/yasli/ConfigLocal.h>
#endif
#include "PropertyTree.h"
#include "ConstStringList.h"

#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/ClassFactory.h>
#include <CrySerialization/DynArray.h>
#include "IMenu.h"

using yasli::StringList;
using yasli::StringListValue;

static int widgetSizeMinHelper(const PropertyTree* tree, const PropertyRow* row, RowWidthCache& widthCache)
{
	if (row->userWidgetSize() >= 0)
	{
		return row->userWidgetSize();
	}

	const int width =
		row->userWidgetToContent()
		? widthCache.getOrUpdate(tree, row, tree->_defaultRowHeight())
		: 80;

	return width;
}

// ---------------------------------------------------------------------------
REGISTER_PROPERTY_ROW(StringListValue, PropertyRowStringListValue)

property_tree::InplaceWidget* PropertyRowStringListValue::createWidget(PropertyTree* tree)
{
	return tree->ui()->createComboBox(this);
}

int PropertyRowStringListValue::widgetSizeMin(const PropertyTree* tree) const
{
	return widgetSizeMinHelper(tree, this, widthCache_);
}

// ---------------------------------------------------------------------------
REGISTER_PROPERTY_ROW(StringListStaticValue, PropertyRowStringListStaticValue)

property_tree::InplaceWidget* PropertyRowStringListStaticValue::createWidget(PropertyTree* tree)
{
	return tree->ui()->createComboBox(this);
}

int PropertyRowStringListStaticValue::widgetSizeMin(const PropertyTree* tree) const
{
	return widgetSizeMinHelper(tree, this, widthCache_);
}

DECLARE_SEGMENT(PropertyRowStringList)

// vim:ts=4 sw=4:

