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

class PropertyRowContainer;
struct ContainerMenuHandler : PropertyRowMenuHandler
{
public:

	PropertyTree* tree;
	PropertyRowContainer* container;
	PropertyRow* element;
	int pointerIndex;

	ContainerMenuHandler(PropertyTree* tree, PropertyRowContainer* container);

public:
	void onMenuInsertElement();
	void onMenuAppendElement();
	void onMenuAppendPointerByIndex();
	void onMenuRemoveAll();
	void onMenuChildInsertBefore();
	void onMenuChildRemove();
};

class PropertyRowContainer : public PropertyRow
{
public:
	PropertyRowContainer();
	bool isContainer() const override{ return true; }
	bool onActivate(const PropertyActivationEvent& e) override;
	bool onContextMenu(IMenu& item, PropertyTree* tree) override;
	void redraw(IDrawContext& context) override;
	bool onKeyDownContainer(PropertyTree* tree, const KeyEvent* key);
	bool onKeyDown(PropertyTree* tree, const KeyEvent* key) override;

	void labelChanged() override;
	bool isStatic() const override{ return false; }
	bool isSelectable() const override{ return userWidgetSize() == 0 ? false : true; }
	PropertyRow* addElement(PropertyTree* tree, bool append);
	void setInlined(bool inlined) { inlined_ = inlined; }
	bool isInlined() const{ return inlined_; }

	PropertyRow* defaultRow(PropertyTreeModel* model);
	const PropertyRow* defaultRow(const PropertyTreeModel* model) const;
	void serializeValue(yasli::Archive& ar) override;

	const char* elementTypeName() const{ return elementTypeName_; }
	virtual void setValueAndContext(const yasli::ContainerInterface& value, yasli::Archive& ar) {
		fixedSize_ = value.isFixedSize();
		elementTypeName_ = value.elementType().name();
		serializer_.setPointer(value.pointer());
		serializer_.setType(value.containerType());
	}
	const char* typeNameForFilter(PropertyTree* tree) const override;
	yasli::string valueAsString() const override;
	// C-array is an example of fixed size container
	bool isFixedSize() const{ return fixedSize_; }
	WidgetPlacement widgetPlacement() const override{ return inlined_ ? WIDGET_NONE : WIDGET_AFTER_NAME; }
	int widgetSizeMin(const PropertyTree*) const override;

protected:
	virtual void generateMenu(property_tree::IMenu& menu, PropertyTree* tree, bool addActions);

	const char* elementTypeName_;
	char buttonLabel_[8];
	bool fixedSize_;
	bool inlined_;
};

