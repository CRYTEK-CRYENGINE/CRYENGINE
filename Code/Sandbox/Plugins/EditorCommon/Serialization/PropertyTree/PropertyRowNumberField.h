/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once
#include "PropertyRow.h"

class PropertyRowNumberField;

// ---------------------------------------------------------------------------

class PropertyRowNumberField : public PropertyRow
{
public:
	PropertyRowNumberField();
	WidgetPlacement widgetPlacement() const override{ return WIDGET_VALUE; }
	int widgetSizeMin(const PropertyTree* tree) const override;

	property_tree::InplaceWidget* createWidget(PropertyTree* tree) override;
	bool isLeaf() const override{ return true; }
	bool isStatic() const override{ return false; }
	bool inlineInShortArrays() const override{ return true; }
	void redraw(IDrawContext& context) override;
	bool onActivate(const PropertyActivationEvent& e) override;
	bool onMouseDown(PropertyTree* tree, Point point, bool& changed) override;
	void onMouseUp(PropertyTree* tree, Point point) override;
	void onMouseDrag(const PropertyDragEvent& e) override;
	void onMouseStill(const PropertyDragEvent& e) override;
	bool getHoverInfo(PropertyHoverInfo* hit, const Point& cursorPos, const PropertyTree* tree) const override;

	virtual void startIncrement() = 0;
	virtual void endIncrement(PropertyTree* tree) = 0;
	virtual void incrementLog(float screenFraction, float valueFieldFraction) = 0;
	virtual void increment(PropertyTree* tree, int mouseDiff, Modifier modifier) {};
	virtual bool setValueFromString(const char* str) = 0;
	virtual double minValue() const = 0;
	virtual double maxValue() const = 0;
	virtual void addValue(PropertyTree* tree, double value) {}
	virtual double singlestep() const = 0;

	mutable RowWidthCache widthCache_;
	Point lastMouseMove_;
	bool pressed_ : 1;
	bool dragStarted_ : 1;
	bool valueAdded_ : 1;
};


