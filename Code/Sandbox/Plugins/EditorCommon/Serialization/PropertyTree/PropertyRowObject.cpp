/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "PropertyRowObject.h"
#include "PropertyTreeModel.h"

PropertyRowObject::PropertyRowObject()
: model_(0)
{
}

bool PropertyRowObject::assignTo(Object* obj)
{
	if (object_.type() == obj->type()) {
		*obj = object_;
		return true;
	}
	return false;
}

PropertyRowObject::~PropertyRowObject()
{
	object_ = Object();
}

void PropertyRowObject::YASLI_SERIALIZE_METHOD(Archive& ar)
{
    PropertyRow::YASLI_SERIALIZE_METHOD(ar);
}


