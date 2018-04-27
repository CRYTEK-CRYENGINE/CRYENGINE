/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */


#pragma once
namespace yasli {

class Archive;

class KeyValueDictionaryInterface
{
public:
	virtual bool   hasElements() const = 0;
	virtual void   clear() = 0;
	virtual void   serializeKey(Archive& ar) = 0;
	virtual void   serializeValue(Archive& ar) = 0;
	virtual bool   serializeAsVector(Archive& ar, const char* name, const char* label) = 0;
	virtual TypeID keyType() const = 0;

	virtual void nextElement() = 0;
	virtual bool reachedEnd() const = 0;
};

}

