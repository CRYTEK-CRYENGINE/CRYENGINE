/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include "Config.h"
#if !YASLI_YASLI_INLINE_IMPLEMENTATION
//#include "StdAfx.h"
#endif
#include "StringList.h"
#include "STL.h"
#include "Archive.h"
#include "STLImpl.h"

namespace yasli{

// ---------------------------------------------------------------------------
YASLI_INLINE void splitStringList(StringList* result, const char *str, char delimeter)
{
    result->clear();

    const char* ptr = str;
    for(; *ptr; ++ptr)
	{
        if(*ptr == delimeter){
			result->push_back(string(str, ptr));
            str = ptr + 1;
        }
	}
	result->push_back(string(str, ptr));
}

YASLI_INLINE void joinStringList(string* result, const StringList& stringList, char sep)
{
    YASLI_ESCAPE(result != 0, return);
    result->clear();
    for(StringList::const_iterator it = stringList.begin(); it != stringList.end(); ++it)
    {
        if(!result->empty())
            result += sep;
        result->append(*it);
    }
}

YASLI_INLINE void joinStringList(string* result, const StringListStatic& stringList, char sep)
{
    YASLI_ESCAPE(result != 0, return);
    result->clear();
    for(StringListStatic::const_iterator it = stringList.begin(); it != stringList.end(); ++it)
    {
        if(!result->empty())
            (*result) += sep;
        YASLI_ESCAPE(*it != 0, continue);
        result->append(*it);
    }
}

YASLI_INLINE bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, StringList& value, const char* name, const char* label)
{
	return ar(static_cast<StringListBase&>(value), name, label);
}

YASLI_INLINE bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, StringListValue& value, const char* name, const char* label)
{
    if(ar.isEdit()){
			Serializer ser = Serializer::forEdit(value);
			return ar(ser, name, label);
    }
		else{
			string str;
			if(ar.isOutput())
				str = value.c_str();
			if(!ar(str, name, label))
				return false;
			if(ar.isInput()){
				int index = value.stringList().find(str.c_str());
				if(index >= 0){
					value = index;
				}
			}
			return true;
		}
}

YASLI_INLINE bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, StringListStaticValue& value, const char* name, const char* label)
{
    if(ar.isEdit()) {
			Serializer ser = Serializer::forEdit(value);
			return ar(ser, name, label);
		}
		else{
			string str;
			if(ar.isOutput())
				str = value.c_str();
			if(!ar(str, name, label))
				return false;
			if(ar.isInput()){
				int index = value.stringList().find(str.c_str());
				if(index != StringListStatic::npos){
					value = index;
				}
			}
			return true;
		}
}

}
