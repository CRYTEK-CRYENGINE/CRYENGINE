/**
 *  yasli - Serialization Library
 *  Copyright (C) 2009-2015 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

namespace yasli{
class Archive;

struct HorizontalLine{
	void YASLI_SERIALIZE_METHOD(yasli::Archive& ar) {}
};

inline bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, HorizontalLine& value, const char* name, const char* label){		
    if(ar.isEdit())
        return ar(yasli::Serializer(value), name, label);
    return false;
}
}
