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
#include <stdio.h>

namespace property_tree {

yasli::string fromWideChar(const wchar_t* wideCharString);
yasli::wstring toWideChar(const char* multiByteString);
yasli::wstring fromANSIToWide(const char* ansiString);
yasli::string toANSIFromWide(const wchar_t* wstr);

}

