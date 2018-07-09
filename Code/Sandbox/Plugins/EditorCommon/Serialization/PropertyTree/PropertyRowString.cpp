/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include <math.h>

#include "PropertyRowString.h"
#include "PropertyTreeModel.h"
#include "IDrawContext.h"
#include "PropertyTree.h"

#include <CrySerialization/yasli/STL.h>
#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/ClassFactory.h>
#include "IMenu.h"
#include "IUIFacade.h"
#include "Unicode.h"

// ---------------------------------------------------------------------------
YASLI_CLASS_NAME(PropertyRow, PropertyRowString, "PropertyRowString", "string");

bool PropertyRowString::assignTo(yasli::string& str) const
{
    str = fromWideChar(value_.c_str());
    return true;
}

bool PropertyRowString::assignTo(yasli::wstring& str) const
{
    str = value_;
    return true;
}

property_tree::InplaceWidget* PropertyRowString::createWidget(PropertyTree* tree)
{
		return tree->ui()->createStringWidget(this);
}

bool PropertyRowString::assignToByPointer(void* instance, const yasli::TypeID& type) const
{
	if (type == yasli::TypeID::get<yasli::string>()) {
		assignTo(*(yasli::string*)instance);
		return true;
	}
	else if (type == yasli::TypeID::get<yasli::wstring>()) {
		assignTo(*(yasli::wstring*)instance);
		return true;
	}
	return false;
}


yasli::string PropertyRowString::valueAsString() const
{
	return fromWideChar(value_.c_str());
}

void PropertyRowString::setValue(const wchar_t* str, const void* handle, const yasli::TypeID& type)
{
	value_ = str;
	serializer_.setPointer((void*)handle);
	serializer_.setType(type);
}

void PropertyRowString::setValue(const char* str, const void* handle, const yasli::TypeID& type)
{
	value_ = toWideChar(str);
	serializer_.setPointer((void*)handle);
	serializer_.setType(type);
}

void PropertyRowString::serializeValue(yasli::Archive& ar)
{
	ar(value_, "value", "Value");
}

// vim:ts=4 sw=4:

