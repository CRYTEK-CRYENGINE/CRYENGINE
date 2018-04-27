// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchive.h>
#include <CrySerialization/CryStrings.h>

namespace Serialization
{
template<class TFixedStringClass>
class CFixedStringSerializer : public IString
{
public:
	CFixedStringSerializer(TFixedStringClass& str) : str_(str) {}

	void        set(const char* value) { str_ = value; }
	const char* get() const            { return str_.c_str(); }
	const void* handle() const         { return &str_; }
	TypeID      type() const           { return TypeID::get<TFixedStringClass>(); }
private:
	TFixedStringClass& str_;
};

template<class TFixedStringClass>
class CFixedWStringSerializer : public IWString
{
public:
	CFixedWStringSerializer(TFixedStringClass& str) : str_(str) {}

	void           set(const wchar_t* value) { str_ = value; }
	const wchar_t* get() const               { return str_.c_str(); }
	const void*    handle() const            { return &str_; }
	TypeID         type() const              { return TypeID::get<TFixedStringClass>(); }
private:
	TFixedStringClass& str_;
};
}

template<size_t N>
inline bool Serialize(Serialization::IArchive& ar, CryFixedStringT<N>& value, const char* name, const char* label)
{
	Serialization::CFixedStringSerializer<CryFixedStringT<N>> str(value);
	return ar(static_cast<Serialization::IString&>(str), name, label);
}

template<size_t N>
inline bool Serialize(Serialization::IArchive& ar, CryStackStringT<char, N>& value, const char* name, const char* label)
{
	Serialization::CFixedStringSerializer<CryStackStringT<char, N>> str(value);
	return ar(static_cast<Serialization::IString&>(str), name, label);
}

template<size_t N>
inline bool Serialize(Serialization::IArchive& ar, CryStackStringT<wchar_t, N>& value, const char* name, const char* label)
{
	Serialization::CFixedWStringSerializer<CryStackStringT<wchar_t, N>> str(value);
	return ar(static_cast<Serialization::IWString&>(str), name, label);
}
