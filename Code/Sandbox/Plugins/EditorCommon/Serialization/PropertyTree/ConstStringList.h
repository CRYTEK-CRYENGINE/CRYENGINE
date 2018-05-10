/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <list>
#include <CrySerialization/yasli/Config.h>

class ConstStringWrapper;

namespace yasli { class Archive; }

bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, ConstStringWrapper &wrapper, const char* name, const char* label);

class ConstStringList{
public:
	const char* findOrAdd(const char* string);
protected:
	typedef std::list<yasli::string> Strings;
	Strings strings_;
};

class ConstStringWrapper{
public:
	ConstStringWrapper(ConstStringList* list, const char*& string);
protected:
	ConstStringList* list_;
	const char*& string_;
	friend bool ::YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, ConstStringWrapper &wrapper, const char* name, const char* label);
};

extern ConstStringList globalConstStringList;


