// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Serialization/PropertyTree/IDrawContext.h>
#include <Serialization/PropertyTree/PropertyRow.h>
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include <Serialization.h>
#include "Serialization/QPropertyTree/QPropertyTree.h"

struct IPropertyRowCryColor
{
	virtual bool   pickColor(PropertyTree* tree) = 0;
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
	int             widgetSizeMin(const PropertyTree* tree) const override { return userWidgetSize() >= 0 ? userWidgetSize() : 40; }
	void            handleChildrenChange() override;

	void            setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override;
	bool            assignTo(const Serialization::SStruct& ser) const override;
	void            closeNonLeaf(const Serialization::SStruct& ser, Serialization::IArchive& ar);
	void            serializeValue(yasli::Archive& ar) override;

	bool            onActivate(const PropertyActivationEvent& e);
	yasli::string   valueAsString() const;
	void            redraw(property_tree::IDrawContext& context);

	bool            onContextMenu(property_tree::IMenu& menu, PropertyTree* tree) override;

	bool            pickColor(PropertyTree* tree) override;
	void            setColor(const QColor& color) override;
	QColor          getColor() override { return color_; }
	void            resetPicker()       { pPicker = nullptr; }

private:
	QColor   color_;
	QWidget* pPicker;
};

struct CryColorMenuHandler : PropertyRowMenuHandler
{
	PropertyTree*         tree;
	IPropertyRowCryColor* propertyRowColor;

	CryColorMenuHandler(PropertyTree* tree, IPropertyRowCryColor* propertyRowColor);
	~CryColorMenuHandler(){};

	void onMenuPickColor();
};

