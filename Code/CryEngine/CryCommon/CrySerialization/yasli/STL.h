/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#pragma once

#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include <unordered_map>

#include <CrySerialization/yasli/Config.h>
#include <CrySerialization/yasli/Serializer.h>
#include <CrySerialization/yasli/KeyValue.h>

namespace yasli{ class Archive; }

namespace std{ // not nice, but needed for argument-dependent lookup to work

template<class T, class Alloc>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::vector<T, Alloc>& container, const char* name, const char* label);

template<class T, class Alloc>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::list<T, Alloc>& container, const char* name, const char* label);

template<class K, class V, class C, class Alloc>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::map<K, V, C, Alloc>& container, const char* name, const char* label);

#if !YASLI_NO_MAP_AS_DICTIONARY
template<class V>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::pair<yasli::string, V>& pair, const char* name, const char* label);
#endif

template<class K, class V>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::pair<K, V>& pair, const char* name, const char* label);

#if YASLI_CXX11
template<class K, class V, class C, class Alloc>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::unordered_map<K, V, C, Alloc>& container, const char* name, const char* label);
template<class T>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::shared_ptr<T>& pair, const char* name, const char* label);
template<class T, class D>
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, std::unique_ptr<T, D>& pair, const char* name, const char* label);
#endif

}

YASLI_STRING_NAMESPACE_BEGIN // std by default
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, yasli::string& value, const char* name, const char* label);
bool YASLI_SERIALIZE_OVERRIDE(yasli::Archive& ar, yasli::wstring& value, const char* name, const char* label);
YASLI_STRING_NAMESPACE_END


#include "STLImpl.h"
