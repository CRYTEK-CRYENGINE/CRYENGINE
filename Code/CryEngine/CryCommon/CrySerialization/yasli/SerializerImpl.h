/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */


#pragma once

#include <CrySerialization/yasli/Config.h>
#include "ClassFactory.h"

// Archive.h is supposed to be pre-included

namespace yasli {

inline bool Serializer::operator()(Archive& ar) const{
	YASLI_ESCAPE(serializeFunc_ && object_, return false);
	return serializeFunc_(object_, ar);
}

inline bool Serializer::operator()(Archive& ar, const char* name, const char* label) const{
	return ar(*this, name, label);
}

inline void PointerInterface::YASLI_SERIALIZE_METHOD(Archive& ar) const
{
	const bool noEmptyNames = ar.caps(Archive::NO_EMPTY_NAMES);
	const char* const typePropertyName = noEmptyNames ? "type" : "";
	const char* const dataPropertyName = noEmptyNames ? "data" : "";

	TypeID baseTypeID = baseType();
	const char* oldRegisteredName = registeredTypeName();
	if (!oldRegisteredName)
		oldRegisteredName = "";
	ClassFactoryBase* factory = this->factory();

	if(ar.isOutput()){
		if(oldRegisteredName[0] != '\0'){
			TypeNameWithFactory pair(oldRegisteredName, factory);
			if(ar(pair, typePropertyName)){
#if YASLI_NO_EXTRA_BLOCK_FOR_POINTERS
                serializer()(ar);
#else
                ar(serializer(), dataPropertyName);
#endif
            }
			else
				ar.warning(pair, "Unable to write typeID!");
		}
	}
	else{
		TypeNameWithFactory pair("", factory);
		if(!ar(pair, typePropertyName)){
			if(oldRegisteredName[0] != '\0'){
				create(""); // 0
			}
			return;
		}

		if(oldRegisteredName[0] != '\0' && (pair.registeredName.empty() || (pair.registeredName != oldRegisteredName)))
			create(""); // 0

		if(!pair.registeredName.empty()){
			if(!get())
				create(pair.registeredName.c_str());
#if YASLI_NO_EXTRA_BLOCK_FOR_POINTERS
			serializer()(ar);
#else
			ar(serializer(), dataPropertyName);
#endif
		}
	}	
}

}
// vim:sw=4 ts=4:
