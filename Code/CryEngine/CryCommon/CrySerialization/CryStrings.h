// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryFixedString.h>
#include <CrySerialization/Forward.h>

// Note : if you are looking for the CryStringT serialization, it is handled in Serialization/STL.h

template<size_t N>
bool Serialize(Serialization::IArchive& ar, CryFixedStringT<N>& value, const char* name, const char* label);

template<size_t N>
bool Serialize(Serialization::IArchive& ar, CryStackStringT<char, N>& value, const char* name, const char* label);

template<size_t N>
bool Serialize(Serialization::IArchive& ar, CryStackStringT<wchar_t, N>& value, const char* name, const char* label);

#include <CrySerialization/CryStringsImpl.h>
