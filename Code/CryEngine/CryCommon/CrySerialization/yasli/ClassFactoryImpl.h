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

#include "STL.h"
#include "ClassFactory.h"
#include "Archive.h"
#include "MemoryWriter.h"

namespace yasli{

YASLI_INLINE const char* TypeID::name() const{
#if YASLI_NO_RTTI
	if (typeInfo_)
		return typeInfo_->name;
	else
		return "";
#else
    if(typeInfo_)
        return typeInfo_->name();
	else
		return name_.c_str();
#endif
}

YASLI_INLINE size_t TypeID::sizeOf() const{
#if YASLI_NO_RTTI
	if (typeInfo_)
		return typeInfo_->size;
	else
		return 0;
#else
	return 0;
#endif
}

YASLI_INLINE bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, yasli::TypeNameWithFactory& value, const char* name, const char* label)
{
	if(!ar(value.registeredName, name))
		return false;

	if (ar.isInput()){
		const TypeDescription* desc = value.factory->descriptionByRegisteredName(value.registeredName.c_str());
		if(!desc){
			ar.error(value, "Unable to read TypeID: unregistered type name: \'%s\'", value.registeredName.c_str());
			value.registeredName.clear();
			return false;
		}
	}
	return true;
}

}
