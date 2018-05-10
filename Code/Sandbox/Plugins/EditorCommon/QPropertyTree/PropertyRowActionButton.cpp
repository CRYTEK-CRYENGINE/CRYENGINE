// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryCore/Platform/platform.h>

#include <Serialization/PropertyTree/PropertyTree.h>
#include <Serialization/PropertyTree/PropertyRowImpl.h>
#include <Serialization/PropertyTree/IDrawContext.h>
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include <Serialization/PropertyTree/Unicode.h>
#include <CrySerialization/Color.h>


#include <CrySerialization/ClassFactory.h>
#include <CrySerialization/Decorators/ActionButton.h>
#include "Serialization.h"

using Serialization::IActionButton;
using Serialization::IActionButtonPtr;

class PropertyRowActionButton : public PropertyRow
{
public:
	PropertyRowActionButton() : underMouse_(), pressed_(), minimalWidth_() {}
	bool isLeaf() const override       { return true; }
	bool isStatic() const override     { return false; }
	bool isSelectable() const override { return true; }

	bool onActivate(const PropertyActivationEvent& e) override
	{
		if (e.reason == PropertyActivationEvent::REASON_KEYBOARD)
		{
			if (value_)
				value_->Callback();
		}
		return true;
	}

	bool onMouseDown(PropertyTree* tree, Point point, bool& changed) override
	{
		if (userReadOnly() || !tree->config().enableActions)
			return false;
		if (widgetRect(tree).contains(point))
		{
			underMouse_ = true;
			pressed_ = true;
			tree->repaint();
			return true;
		}
		return false;
	}

	void onMouseDrag(const PropertyDragEvent& e) override
	{
		if (userReadOnly() || !e.tree->config().enableActions)
			return;
		bool underMouse = widgetRect(e.tree).contains(e.pos);
		if (underMouse != underMouse_)
		{
			underMouse_ = underMouse;
			e.tree->repaint();
		}
	}

	void onMouseUp(PropertyTree* tree, Point point) override
	{
		if (userReadOnly() || !tree->config().enableActions)
			return;
		if (widgetRect(tree).contains(point))
		{
			pressed_ = false;
			if (value_)
				value_->Callback();
			tree->revert();
		}
	}
	void setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override
	{
		value_ = static_cast<IActionButton*>(ser.pointer())->Clone();
		const char* icon = value_->Icon();
		icon_ = icon && icon[0] ? property_tree::Icon(icon) : property_tree::Icon();
	}
	bool            assignTo(const Serialization::SStruct& ser) const override { return true; }
	wstring         valueAsWString() const override                            { return L""; }
	WidgetPlacement widgetPlacement() const override                           { return WIDGET_INSTEAD_OF_TEXT; }
	void            serializeValue(Serialization::IArchive& ar) override       {}

	int             widgetSizeMin(const PropertyTree* tree) const override
	{
		if (minimalWidth_ == 0)
			minimalWidth_ = (int)tree->ui()->textWidth(labelUndecorated(), property_tree::FONT_NORMAL) + 6 + (icon_.isNull() ? 0 : 18);
		return minimalWidth_;
	}

	int widgetSizeMax(const PropertyTree* tree) const override
	{
		if (!icon_.filename.empty() && (!labelUndecorated_ || !*labelUndecorated_))
			return tree->_defaultRowHeight();

		return PropertyRow::widgetSizeMax(tree);
	}

	void redraw(IDrawContext& context) override
	{
		using namespace property_tree;
		Rect rect = context.widgetRect.adjusted(-1, -1, 1, 1);
		bool pressed = pressed_ && underMouse_;

		int buttonFlags = 0;
		if (pressed)
			buttonFlags |= BUTTON_PRESSED;
		if (selected())
			buttonFlags |= BUTTON_FOCUSED;
		if (userReadOnly() || !context.tree->config().enableActions)
			buttonFlags |= BUTTON_DISABLED;

		if (icon_.isNull())
		{
			context.drawButton(rect, labelUndecorated(), buttonFlags | BUTTON_CENTER_TEXT, property_tree::FONT_NORMAL);
		}
		else
		{
			context.drawButtonWithIcon(icon_, rect, labelUndecorated(), buttonFlags, property_tree::FONT_NORMAL);
		}
	}
	bool isFullRow(const PropertyTree* tree) const override
	{
		if (PropertyRow::isFullRow(tree))
			return true;
		return !userFixedWidget();
	}
protected:
	mutable int         minimalWidth_;
	bool                underMouse_;
	bool                pressed_;
	property_tree::Icon icon_;
	IActionButtonPtr    value_;
};

REGISTER_PROPERTY_ROW(IActionButton, PropertyRowActionButton);

