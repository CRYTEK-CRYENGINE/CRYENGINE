/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "PropertyTree.h"
#include "PropertyTreeModel.h"
#include "PropertyRowNumberField.h"
#include "IDrawContext.h"
#include "PropertyTree.h"
#include "IUIFacade.h"
#include <float.h>

PropertyRowNumberField::PropertyRowNumberField()
: pressed_(false)
, dragStarted_(false)
, valueAdded_(false)
{
}

property_tree::InplaceWidget* PropertyRowNumberField::createWidget(PropertyTree* tree)
{
	return tree->ui()->createNumberWidget(this);
}

void PropertyRowNumberField::redraw(IDrawContext& context)
{
	if (multiValue())
	{
		context.drawEntry(context.widgetRect, " ... ", false, userReadOnly(), 0);
	}
	else 
	{
		int flags = 0;
		if (context.captured)
			flags |= FIELD_SELECTED;
		if (pressed_)
			flags |= FIELD_PRESSED;
		if (userReadOnly())
			flags |= FIELD_DISABLED;
		context.drawNumberEntry(valueAsString().c_str(), context.widgetRect, flags, minValue(), maxValue());
	}
}

void PropertyRowNumberField::onMouseDrag(const PropertyDragEvent& e)
{
	if (!dragStarted_) {
		e.tree->model()->rowAboutToBeChanged(this);
		dragStarted_ = true;
	}
	increment(e.tree, e.pos.x() - lastMouseMove_.x(), e.modifier);
	lastMouseMove_ = e.pos;
	setMultiValue(false);
}

bool PropertyRowNumberField::getHoverInfo(PropertyHoverInfo* hit, const Point& cursorPos, const PropertyTree* tree) const
{
	if (pressed_ && !userReadOnly())
		hit->cursor = property_tree::CURSOR_BLANK;
	else if (widgetRect(tree).contains(cursorPos) && !userReadOnly())
		hit->cursor = property_tree::CURSOR_SLIDE;
	hit->toolTip = tooltip_;
	return true;
}

void PropertyRowNumberField::onMouseStill(const PropertyDragEvent& e)
{
	e.tree->model()->callRowCallback(this);
	e.tree->apply(true);
}

bool PropertyRowNumberField::onMouseDown(PropertyTree* tree, Point point, bool& changed)
{
	lastMouseMove_ = point;
	valueAdded_ = false;
	changed = false;
	Rect rect = widgetRect(tree);
	if (rect.contains(point) && !userReadOnly())
	{
		rect = rect.adjusted(0, 0, -rect.height(), 0);
		if (rect.contains(point))
		{
			startIncrement();
			pressed_ = true;
			return true;
		}
		else
		{
			if (point.y() - rect.top() < rect.height() / 2)
			{
				addValue(tree, singlestep());
			}
			else
			{
				addValue(tree, -singlestep());
			}
			valueAdded_ = true;
		}
	}
	return false;
}

void PropertyRowNumberField::onMouseUp(PropertyTree* tree, Point point) 
{
	tree->ui()->unsetCursor();
	pressed_ = false;
	dragStarted_ = false;

	// endIncrement() can cause PropertyRow to be destroy, 
	// so no "this" members should be accessed after the call.
	endIncrement(tree);
}

bool PropertyRowNumberField::onActivate(const PropertyActivationEvent& e)
{
	if (valueAdded_)
	{
		valueAdded_ = false;
		return false;
	}

	if (e.reason == e.REASON_RELEASE || e.reason == e.REASON_KEYBOARD)
		return e.tree->spawnWidget(this, false);
	return false;
}

int PropertyRowNumberField::widgetSizeMin(const PropertyTree* tree) const
{ 
	if (userWidgetSize() >= 0)
		return userWidgetSize();

	if (userWidgetToContent())
		return widthCache_.getOrUpdate(tree, this, 20);
	else
		return 40;
}

// ---------------------------------------------------------------------------


