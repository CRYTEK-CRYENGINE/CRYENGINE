/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "ConstStringList.h"
#include <algorithm>
#include <CrySerialization/yasli/STL.h>
#include <CrySerialization/yasli/Archive.h>

ConstStringList globalConstStringList;

const char* ConstStringList::findOrAdd(const char* string)
{
	// TODO: try sorted vector of const char*
	Strings::iterator it = std::find(strings_.begin(), strings_.end(), string);
	if(it == strings_.end()){
		strings_.push_back(string);
		return strings_.back().c_str();
	}
	else{
		return it->c_str();
	}
}


ConstStringWrapper::ConstStringWrapper(ConstStringList* list, const char*& string)
: list_(list ? list : &globalConstStringList)
, string_(string)
{
	YASLI_ASSERT(string_);
}


bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, ConstStringWrapper& value, const char* name, const char* label)
{
	if(ar.isOutput()){
        YASLI_ASSERT(value.string_);
        string out = value.string_;
		return ar(out, name, label);
	}
	else{
		yasli::string in;
		bool result = ar(in, name, label);

        value.string_ = value.list_->findOrAdd(in.c_str());
		return result;
	}
}

