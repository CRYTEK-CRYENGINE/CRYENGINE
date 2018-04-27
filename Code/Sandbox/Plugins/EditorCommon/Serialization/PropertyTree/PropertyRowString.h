/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include "PropertyRowField.h"
#include "PropertyTree.h"
#include "PropertyTreeModel.h"
#include "Unicode.h"

class PropertyRowString : public PropertyRowField
{
public:
	bool isLeaf() const override{ return true; }
	bool isStatic() const override{ return false; }
	bool assignTo(yasli::string& str) const;
	bool assignTo(yasli::wstring& str) const;
	bool assignToByPointer(void* instance, const yasli::TypeID& type) const override;
	void setValue(const char* str, const void* handle, const yasli::TypeID& typeId);
	void setValue(const wchar_t* str, const void* handle, const yasli::TypeID& typeId);
	property_tree::InplaceWidget* createWidget(PropertyTree* tree) override;
	yasli::string valueAsString() const override;
	yasli::wstring valueAsWString() const override { return value_; }
	WidgetPlacement widgetPlacement() const override{ return WIDGET_VALUE; }
	void serializeValue(yasli::Archive& ar) override;
	const yasli::wstring& value() const{ return value_; }
private:
	yasli::wstring value_;
};


