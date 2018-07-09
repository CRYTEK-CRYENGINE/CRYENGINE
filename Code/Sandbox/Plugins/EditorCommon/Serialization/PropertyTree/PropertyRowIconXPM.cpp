/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include <CrySerialization/yasli/ClassFactory.h>

#include "IDrawContext.h"
#include "PropertyRowImpl.h"
#include "PropertyTreeModel.h"
#include "PropertyTree.h"
#include "Serialization.h"
#include "Color.h"	
#include <CrySerialization/yasli/decorators/IconXPM.h>
using yasli::IconXPM;
using yasli::IconXPMToggle;

class PropertyRowIconXPM : public PropertyRow{
public:
	void redraw(IDrawContext& context) override
	{
		context.drawIcon(context.widgetRect, icon_);
	}

	bool isLeaf() const override{ return true; }
	bool isStatic() const override{ return false; }
	bool isSelectable() const override{ return false; }

	bool onActivate(const PropertyActivationEvent& e) override { return false; }
	void setValueAndContext(const yasli::Serializer& ser, yasli::Archive& ar) override {
		YASLI_ESCAPE(ser.size() == sizeof(IconXPM), return);
		icon_ = *(IconXPM*)(ser.pointer());
	}
	yasli::wstring valueAsWString() const override{ return L""; }
	WidgetPlacement widgetPlacement() const override{ return WIDGET_ICON; }
	void serializeValue(Archive& ar) override{}
	int widgetSizeMin(const PropertyTree*) const override{ return 18; }
protected:
	IconXPM icon_;
};

class PropertyRowIconToggle : public PropertyRow{
public:
	void redraw(IDrawContext& context) override
	{
		IconXPM& icon = value_ ? iconTrue_ : iconFalse_;
		context.drawIcon(context.widgetRect, icon);
	}

	void setValueAndContext(const yasli::Serializer& ser, yasli::Archive& ar) override {
		YASLI_ESCAPE(ser.size() == sizeof(IconXPMToggle), return);
		const IconXPMToggle* icon = (IconXPMToggle*)(ser.pointer());
		iconTrue_ = icon->iconTrue_;
		iconFalse_ = icon->iconFalse_;
		value_ = icon->value_;
	}

	bool assignTo(const yasli::Serializer& ser) const override
	{
		IconXPMToggle* toggle = (IconXPMToggle*)ser.pointer();
		toggle->value_ = value_;
		return true;
	}

	bool isLeaf() const override{ return true; }
	bool isStatic() const override{ return false; }
	bool isSelectable() const override{ return true; }
	bool onActivate(const PropertyActivationEvent& ev) override
	{
		if (ev.reason != ev.REASON_RELEASE) {
			ev.tree->model()->rowAboutToBeChanged(this);
			value_ = !value_;
			ev.tree->model()->rowChanged(this);
		}
		return true;
	}
	DragCheckBegin onMouseDragCheckBegin() override
	{
		if (userReadOnly())
			return DRAG_CHECK_IGNORE;
		return value_ ? DRAG_CHECK_UNSET : DRAG_CHECK_SET;
	}
	bool onMouseDragCheck(PropertyTree* tree, bool value) override
	{
		if (value_ != value) {
			tree->model()->rowAboutToBeChanged(this);
			value_ = value;
			tree->model()->rowChanged(this);
			return true;
		}
		return false;
	}
	yasli::wstring valueAsWString() const override{ return value_ ? L"true" : L"false"; }
	WidgetPlacement widgetPlacement() const override{ return WIDGET_ICON; }

	int widgetSizeMin(const PropertyTree*) const override{ return 18; }

	IconXPM iconTrue_;
	IconXPM iconFalse_;
	bool value_;
};

REGISTER_PROPERTY_ROW(IconXPM, PropertyRowIconXPM); 
REGISTER_PROPERTY_ROW(IconXPMToggle, PropertyRowIconToggle); 
DECLARE_SEGMENT(PropertyRowIconXPM)

