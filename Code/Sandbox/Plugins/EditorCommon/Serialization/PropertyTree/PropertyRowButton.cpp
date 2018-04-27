/**
 *  wWidgets - Lightweight UI Toolkit.
 *  Copyright (C) 2009-2011 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "PropertyRowImpl.h"
#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/ClassFactory.h>
#include <CrySerialization/yasli/decorators/Button.h>
#include "PropertyTree.h"
#include "IDrawContext.h"
#include "PropertyTreeModel.h"
#include "Unicode.h"

using yasli::Button;
class PropertyRowButton : public PropertyRowImpl<Button>{
public:
	PropertyRowButton();
	void redraw(IDrawContext& context) override;
	bool onMouseDown(PropertyTree* tree, Point point, bool& changed) override;
	void onMouseDrag(const PropertyDragEvent& e) override;
	void onMouseUp(PropertyTree* tree, Point point) override;
	bool onActivate(const PropertyActivationEvent& e) override;
	int floorHeight() const override{ return 3; }
	yasli::string valueAsString() const override{ return value_ ? value_.text : ""; }
	int widgetSizeMin(const PropertyTree* tree) const override{ 
		if (userWidgetSize() >= 0)
			return userWidgetSize();
		else
			return 60; 
	}
protected:
	bool underMouse_;
};

PropertyRowButton::PropertyRowButton()
: underMouse_(false)
{
}
	

void PropertyRowButton::redraw(IDrawContext& context)
{
	Rect buttonRect(context.widgetRect.adjusted(-1, 0, 1, 1));

	yasli::string text(value().text ? value().text : labelUndecorated());
	bool pressed = underMouse_ && value();
	using namespace property_tree;
	int buttonFlags = (pressed ? BUTTON_PRESSED : 0) |
		((selected() & context.tree->hasFocusOrInplaceHasFocus()) ? BUTTON_FOCUSED : 0) |
		(userReadOnly() ? BUTTON_DISABLED : 0) | BUTTON_CENTER_TEXT;
	context.drawButton(buttonRect, text.c_str(), buttonFlags, rowFont(context.tree));
}

bool PropertyRowButton::onMouseDown(PropertyTree* tree, Point point, bool& changed)
{
	if(widgetRect(tree).contains(point)){
		value().pressed = !value().pressed;
		underMouse_ = true;
		tree->repaint();
		return true;
	}
	return false;
}

void PropertyRowButton::onMouseDrag(const PropertyDragEvent& e)
{
	bool underMouse = widgetRect(e.tree).contains(e.pos);
	if(underMouse != underMouse_){
		underMouse_ = underMouse;
		e.tree->repaint();
	}
}

void PropertyRowButton::onMouseUp(PropertyTree* tree, Point point)
{
	PropertyActivationEvent ev;
	ev.clickPoint = point;
	ev.tree = tree;
	if(widgetRect(tree).contains(point)){
		onActivate(ev);
    }
	else{
        tree->model()->rowAboutToBeChanged(this);
		value().pressed = false;
		tree->repaint();
	}
}

bool PropertyRowButton::onActivate(const PropertyActivationEvent& ev)
{
	if (ev.reason != ev.REASON_RELEASE) {
		value().pressed = true;
		ev.tree->model()->rowChanged(this); // Row is recreated here, so don't unlock
	}
	return true;
}

DECLARE_SEGMENT(PropertyRowButton)
REGISTER_PROPERTY_ROW(Button, PropertyRowButton);

