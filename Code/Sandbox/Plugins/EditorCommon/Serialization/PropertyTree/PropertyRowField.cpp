/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "PropertyRowField.h"
#include "PropertyTree.h"
#include "IDrawContext.h"
#include "Rect.h"

enum { BUTTON_SIZE = 16 };

Rect PropertyRowField::fieldRect(const PropertyTree* tree) const
{
	Rect fieldRect = widgetRect(tree);
	fieldRect.w -= buttonCount() * BUTTON_SIZE;
	return fieldRect;
}

bool PropertyRowField::onActivate(const PropertyActivationEvent& e)
{
	int buttonCount = this->buttonCount();
	Rect buttonsRect = widgetRect(e.tree);
	buttonsRect.x = buttonsRect.x + buttonsRect.w - buttonCount * BUTTON_SIZE;

	if (e.reason == e.REASON_PRESS)	{

		int buttonIndex = hitButton(e.tree, e.clickPoint);
		if (buttonIndex != -1)
			if (onActivateButton(buttonIndex, e))
				return true;
	}

	if (buttonsRect.contains(e.clickPoint))
		return false;

	if (e.reason == e.REASON_RELEASE || e.reason == e.REASON_KEYBOARD)
	{
		Rect rect = widgetRect(e.tree);
		if (rect.contains(e.clickPoint) && !userReadOnly())
		{
			rect = rect.adjusted(0, 0, -rect.height(), 0);
			if (rect.contains(e.clickPoint))
			{
				return e.tree->spawnWidget(this, false);
			}
		}
	}

	return PropertyRow::onActivate(e);
}

bool PropertyRowField::onMouseDown(PropertyTree* tree, Point point, bool& changed)
{
	changed = false;
	Rect rect = widgetRect(tree);
	if (rect.contains(point) && !userReadOnly())
	{
		rect = rect.adjusted(0, 0, -rect.height(), 0);
		if (rect.contains(point))
		{
			return true;
		}
	}
	return false;
}

void PropertyRowField::onMouseUp(PropertyTree* tree, Point point)
{
	tree->ui()->unsetCursor();
}

void PropertyRowField::redraw(IDrawContext& context)
{
	int buttonCount = this->buttonCount();
	int offset = 0;
	for (int i = 0; i < buttonCount; ++i) {
		Icon icon = buttonIcon(context.tree, i);
		offset += BUTTON_SIZE;
		Rect iconRect(context.widgetRect.right() - offset, context.widgetRect.top(), BUTTON_SIZE, context.widgetRect.height());
		context.drawIcon(iconRect, icon, userReadOnly() ? ICON_DISABLED : ICON_NORMAL);
	}

	int iconSpace = offset ? offset + 2 : 0;
    if(multiValue())
		context.drawEntry(context.widgetRect, " ... ", false, userReadOnly(), iconSpace);
    else
		context.drawEntry(context.widgetRect, valueAsString().c_str(), usePathEllipsis(), userReadOnly(), iconSpace);
}

Icon PropertyRowField::buttonIcon(const PropertyTree* tree, int index) const
{
	return Icon();
}

int PropertyRowField::widgetSizeMin(const PropertyTree* tree) const
{ 
	if (userWidgetSize() >= 0)
		return userWidgetSize();

	if (userWidgetToContent_)
		return widthCache_.getOrUpdate(tree, this, 0);
	else
		return 40;
}

int PropertyRowField::hitButton(const PropertyTree* tree, const Point& point) const
{
	int buttonCount = this->buttonCount();
	Rect buttonsRect = widgetRect(tree);
	buttonsRect.x = buttonsRect.x + buttonsRect.w - buttonCount * BUTTON_SIZE;

	if (buttonsRect.contains(point)) {
		int buttonIndex = (point.x() - buttonsRect.x) / BUTTON_SIZE;
		if (buttonIndex >= 0 && buttonIndex < buttonCount)
			return (buttonCount - 1) - buttonIndex;
	}
	return -1;
}

