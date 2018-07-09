// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include <CrySerialization/ClassFactory.h>
#include <Serialization/PropertyTree/IDrawContext.h>
#include <Serialization/PropertyTree/PropertyRow.h>
#include <Serialization/PropertyTree/PropertyTree.h>
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include <Serialization/PropertyTree/Unicode.h>
#include <CrySerialization/Color.h>
#include "Serialization.h"
#include "Serialization/Decorators/ToggleButton.h"
using Serialization::ToggleButton;
using Serialization::RadioButton;

class PropertyRowToggleButton : public PropertyRow
{
public:
	PropertyRowToggleButton()
		: underMouse_(false)
		, value_(false)
	{
	}

	bool isLeaf() const       { return true; }
	bool isStatic() const     { return false; }
	bool isSelectable() const { return true; }

	void setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override
	{
		ToggleButton* value = (ToggleButton*)(ser.pointer());
		value_ = *value->value;
		const char* icon = value->iconOn;
		iconOn_ = icon && icon[0] ? property_tree::Icon(icon) : property_tree::Icon();

		icon = value->iconOff;
		iconOff_ = icon && icon[0] ? property_tree::Icon(icon) : property_tree::Icon();
	}

	bool assignTo(const Serialization::SStruct& ser) const override
	{
		ToggleButton* value = (ToggleButton*)(ser.pointer());
		*value->value = value_;
		return true;
	}
	wstring         valueAsWString() const override                        { return L""; }
	WidgetPlacement widgetPlacement() const override                       { return WIDGET_INSTEAD_OF_TEXT; }
	void            serializeValue(Serialization::IArchive& ar)            {}
	int             widgetSizeMin(const PropertyTree* tree) const override { return 36; }

	int             widgetSizeMax(const PropertyTree* tree) const override
	{
		if ((!iconOn_.filename.empty() || !iconOff_.filename.empty()) && (!labelUndecorated_ || !*labelUndecorated_))
			return tree->_defaultRowHeight();

		return PropertyRow::widgetSizeMax(tree);
	}

	bool onActivate(const PropertyActivationEvent& e) override
	{
		if (e.reason == PropertyActivationEvent::REASON_KEYBOARD)
			value_;
		return true;
	}

	bool onMouseDown(PropertyTree* tree, Point point, bool& changed) override
	{
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
		bool underMouse = widgetRect(e.tree).contains(e.pos);
		if (underMouse != underMouse_)
		{
			underMouse_ = underMouse;
			e.tree->repaint();
		}
	}

	void onMouseUp(PropertyTree* tree, Point point) override
	{
		if (widgetRect(tree).contains(point))
		{
			tree->model()->rowAboutToBeChanged(this);
			pressed_ = false;
			value_ = !value_;
			tree->model()->rowChanged(this);
		}
	}

	void redraw(property_tree::IDrawContext& context) override
	{
		using namespace property_tree;
		Rect rect = context.widgetRect.adjusted(-1, -1, 1, 1);

		int buttonFlags = 0;
		if ((value_ || pressed_) && underMouse_)
			buttonFlags |= BUTTON_PRESSED;
		if (selected() || pressed_)
			buttonFlags |= BUTTON_FOCUSED;
		if (userReadOnly())
			buttonFlags |= BUTTON_DISABLED;

		if ((value_ && iconOn_.isNull()) || (!value_ && iconOff_.isNull()))
		{
			buttonFlags |= BUTTON_CENTER_TEXT;
			context.drawButton(rect, labelUndecorated(), buttonFlags, property_tree::FONT_NORMAL);
		}
		else
		{
			buttonFlags |= BUTTON_NO_FRAME;
			context.drawButtonWithIcon(value_ ? iconOn_ : iconOff_, rect, labelUndecorated(), buttonFlags, property_tree::FONT_NORMAL);
		}
	}
protected:
	bool                pressed_    : 1;
	bool                underMouse_ : 1;
	bool                value_      : 1;
	property_tree::Icon iconOn_;
	property_tree::Icon iconOff_;
};

class PropertyRowRadioButton : public PropertyRow
{
public:

	bool isLeaf() const override       { return true; }
	bool isStatic() const override     { return false; }
	bool isSelectable() const override { return false; }

	bool onActivate(const PropertyActivationEvent& e) override
	{
		if (!m_justSet)
		{
			e.tree->model()->rowAboutToBeChanged(this);
			m_justSet = true;
			e.tree->model()->rowChanged(this);
		}
		return true;
	}
	void setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override
	{
		RadioButton* value = (RadioButton*)(ser.pointer());
		m_value = value->buttonValue;
		m_toggled = m_value == *value->value;
		m_justSet = false;
	}
	bool assignTo(const Serialization::SStruct& ser) const override
	{
		if (m_justSet)
			*((RadioButton*)ser.pointer())->value = m_value;
		return true;
	}
	wstring         valueAsWString() const override  { return L""; }
	WidgetPlacement widgetPlacement() const override { return WIDGET_INSTEAD_OF_TEXT; }
	void            serializeValue(Serialization::IArchive& ar) override
	{
		bool oldToggled = m_toggled;
		ar(m_toggled, "toggled");
		if (m_toggled && !oldToggled)
			m_justSet = true;
		ar(m_value, "value");
	}
	int  widgetSizeMin(const PropertyTree* tree) const override { return 40; }

	void redraw(property_tree::IDrawContext& context)
	{
		Rect rect = context.widgetRect;
		bool pressed = context.pressed || m_toggled || m_justSet;

		int buttonFlags = BUTTON_CENTER_TEXT;
		if (pressed)
			buttonFlags |= BUTTON_PRESSED;
		if (selected())
			buttonFlags |= BUTTON_FOCUSED;
		if (userReadOnly())
			buttonFlags |= BUTTON_DISABLED;
		context.drawButton(rect, labelUndecorated(), buttonFlags, property_tree::FONT_NORMAL);
	}
protected:
	bool m_toggled;
	bool m_justSet;
	int  m_value;
};

REGISTER_PROPERTY_ROW(ToggleButton, PropertyRowToggleButton);
REGISTER_PROPERTY_ROW(RadioButton, PropertyRowRadioButton);

