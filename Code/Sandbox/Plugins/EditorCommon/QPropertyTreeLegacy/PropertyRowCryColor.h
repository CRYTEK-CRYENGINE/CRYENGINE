// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Serialization/PropertyTreeLegacy/IDrawContext.h"
#include "Serialization/PropertyTreeLegacy/PropertyRow.h"

#include <Serialization.h>

#include <QColor>

struct IPropertyRowCryColor
{
	virtual bool   pickColor(PropertyTreeLegacy* tree) = 0;
	virtual void   setColor(const QColor& color) = 0;
	virtual QColor getColor() = 0;

	IPropertyRowCryColor() : changed(false) {}
	bool changed;
};

template<class ColorClass>
class PropertyRowCryColor : public PropertyRow, public IPropertyRowCryColor
{
public:
	PropertyRowCryColor() : pPicker(nullptr) {}
	~PropertyRowCryColor();

	bool            isLeaf() const override                                { return true; }
	bool            isStatic() const override                              { return false; }
	WidgetPlacement widgetPlacement() const override                       { return WIDGET_AFTER_PULLED; }
	int             widgetSizeMin(const PropertyTreeLegacy* tree) const override { return userWidgetSize() >= 0 ? userWidgetSize() : 40; }
	void            handleChildrenChange() override;

	void            setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override;
	bool            assignTo(const Serialization::SStruct& ser) const override;
	void            closeNonLeaf(const Serialization::SStruct& ser, Serialization::IArchive& ar);
	void            serializeValue(yasli::Archive& ar) override;

	bool            onActivate(const PropertyActivationEvent& e);
	yasli::string   valueAsString() const;
	void            redraw(property_tree::IDrawContext& context);

	bool            onContextMenu(property_tree::IMenu& menu, PropertyTreeLegacy* tree) override;

	bool            pickColor(PropertyTreeLegacy* tree) override;
	void            setColor(const QColor& color) override;
	QColor          getColor() override { return color_; }
	void            resetPicker()       { pPicker = nullptr; }

private:
	QColor   color_;
	QWidget* pPicker;
};

struct CryColorMenuHandler : PropertyRowMenuHandler
{
	PropertyTreeLegacy*         tree;
	IPropertyRowCryColor* propertyRowColor;

	CryColorMenuHandler(PropertyTreeLegacy* tree, IPropertyRowCryColor* propertyRowColor);
	~CryColorMenuHandler(){}

	void onMenuPickColor();
};
