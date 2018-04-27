// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/************************************************************************
   This class initiates from a given string, and stores the hashed Representation of that string
   Plus: It can store a copy of the original string for debug purposes
************************************************************************/

//! \cond INTERNAL

#pragma once

#include <CryCore/CryCrc32.h>
#include <CrySerialization/Forward.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>

#if !defined(_RELEASE)
	#define HASHEDSTRING_STORES_SOURCE_STRING      //!< For debug purposes, it is often interesting to see from which string the hash was created.
	#define HASHEDSTRING_CHECKS_FOR_HASH_CONFLICTS //!< Will compare the strings if the hash-values are equal, and assert if the hash-values match but the strings dont.
#endif

class CHashedString
{
public:
	static const uint32 INVALID_HASH = 0;
	static const CHashedString& GetEmpty()
	{
		const static CHashedString emptyInstance;
		return emptyInstance;
	}

	CHashedString() : m_hash(INVALID_HASH) {}
	CHashedString(const CHashedString& other);
	explicit CHashedString(const uint32 hash);
	CHashedString(const char* szText); //!< Remark: If the string starts with '0x' then we assume it is already a string representation of a hash, we wont rehash this string, but instead just convert it into a hash.
	CHashedString(const string& text); //!< Remark: If the string starts with '0x' then we assume it is already a string representation of a hash, we wont rehash this string, but instead just convert it into a hash.

	void   Clear();
	bool   IsValid() const { return m_hash != CHashedString::INVALID_HASH; } //!< Remark: Valid means the Hash != INVALID_HASH. Therefore Empty strings (and the string "0x0") will be treated as a invalid.
	string GetText() const;                                                  //!< Remark: only if HASHEDSTRING_STORES_SOURCE_STRING is defined you will receive the original string, otherwise you will get the hash value converted to an string (starting with '0x')
	uint32 GetHash() const { return m_hash; }
	string GetHashAsString() const;

	operator uint32() const { return m_hash; }
	CHashedString& operator=(const char* szText);
	CHashedString& operator=(const string& text);
	CHashedString& operator=(const CHashedString& other);
	bool           operator==(const char* szText) const;
	bool           operator==(const CHashedString& other) const;
	bool           operator!=(const CHashedString& other) const { return m_hash != other.m_hash; }
	bool           operator<(const CHashedString& other) const  { return m_hash < other.m_hash; }

protected:
	uint32      CreateHash(const char* szStringToHash) const;
	friend bool Serialize(Serialization::IArchive& ar, CHashedString& str, const char* name, const char* label);

	uint32       m_hash;
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
public: string m_textCopy;    //!< Remark: This member is only public, so that we can serialize it easier.
#endif
};

//////////////////////////////////////////////////////////////////////////
inline CHashedString::CHashedString(const uint32 hash)
{
	m_hash = hash;
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (m_hash != INVALID_HASH)
		m_textCopy = GetHashAsString();
#endif
}

//////////////////////////////////////////////////////////////////////////
inline CHashedString::CHashedString(const char* szText)
{
	m_hash = CreateHash(szText);
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (m_hash != INVALID_HASH)
		m_textCopy = szText;
#endif
}

//////////////////////////////////////////////////////////////////////////
inline CHashedString::CHashedString(const string& text)
{
	m_hash = CreateHash(text.c_str());
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (m_hash != INVALID_HASH)
		m_textCopy = text;
#endif
}

//////////////////////////////////////////////////////////////////////////
inline CHashedString::CHashedString(const CHashedString& other)
{
	m_hash = other.m_hash;
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	m_textCopy = other.m_textCopy;
#endif
}

//////////////////////////////////////////////////////////////////////////
inline void CHashedString::Clear()
{
	m_hash = INVALID_HASH;
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	m_textCopy.clear();
#endif
}

//////////////////////////////////////////////////////////////////////////
inline CHashedString& CHashedString::operator=(const char* szText)
{
	m_hash = CreateHash(szText);
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (m_hash != INVALID_HASH)
		m_textCopy = szText;
	else
		m_textCopy.clear();
#endif
	return *this;
}

//////////////////////////////////////////////////////////////////////////
inline CHashedString& CHashedString::operator=(const string& text)
{
	m_hash = CreateHash(text.c_str());
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (m_hash != INVALID_HASH)
		m_textCopy = text;
	else
		m_textCopy.clear();
#endif
	return *this;
}

//////////////////////////////////////////////////////////////////////////
inline CHashedString& CHashedString::operator=(const CHashedString& other)
{
	m_hash = other.m_hash;
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	m_textCopy = other.m_textCopy;
#endif
	return *this;
}

//////////////////////////////////////////////////////////////////////////
inline string CHashedString::GetText() const
{
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	return m_textCopy;
#else
	return GetHashAsString();
#endif
}

//////////////////////////////////////////////////////////////////////////
inline string CHashedString::GetHashAsString() const
{
	char buffer[16];
	cry_sprintf(buffer, "0x%08x", m_hash);
	return buffer;
}

//////////////////////////////////////////////////////////////////////////
inline bool CHashedString::operator==(const char* szText) const
{
#if defined(HASHEDSTRING_CHECKS_FOR_HASH_CONFLICTS)
	return *this == CHashedString(szText);
#endif
	return m_hash == CreateHash(szText);
}

//////////////////////////////////////////////////////////////////////////
inline bool CHashedString::operator==(const CHashedString& other) const
{
#if defined(HASHEDSTRING_CHECKS_FOR_HASH_CONFLICTS)
	if (m_hash == other.m_hash)
	{
		CRY_ASSERT_MESSAGE(m_textCopy.compareNoCase(other.m_textCopy) == 0 || m_textCopy == other.GetHashAsString() || GetHashAsString() == other.m_textCopy, "HashCollision occured! The same hash was used for two different strings");
		return true;
	}
	else
	{
		return false;
	}
#endif
	return m_hash == other.m_hash;
}

//////////////////////////////////////////////////////////////////////////
inline uint32 CHashedString::CreateHash(const char* szStringToHash) const
{
	if (szStringToHash == nullptr)
	{
		return CHashedString::INVALID_HASH;
	}
	if (szStringToHash[0] == '0' && szStringToHash[1] == 'x')  //!< If the given string already starts with 0x... we assume that it is already a hashCode, stored in a string. We therefore simply convert it back.
	{
		return strtoul(szStringToHash + 2, 0, 16);
	}
	return CCrc32::ComputeLowercase(szStringToHash);
}

//////////////////////////////////////////////////////////////////////////
namespace std
{
//! Hash computation for std::unordered_map etc.
template<> struct hash<CHashedString>
{
	size_t operator()(const CHashedString& s) const { return s.GetHash(); }
};
}

//////////////////////////////////////////////////////////////////////////
inline bool Serialize(Serialization::IArchive& ar, CHashedString& str, const char* name, const char* label)
{
#if defined(HASHEDSTRING_STORES_SOURCE_STRING)
	if (ar.caps(Serialization::IArchive::BINARY))
	{
		if (!ar(str.m_hash, name, label))
			return false;
		if (ar.isInput())
		{
			if (str.m_hash != CHashedString::INVALID_HASH)
				str.m_textCopy = str.GetHashAsString();
			else
				str.m_textCopy.clear();
		}
		return true;
	}
	else
	{
		if (!ar(str.m_textCopy, name, label))
			return false;
		if (ar.isInput())
			str.m_hash = str.CreateHash(str.m_textCopy.c_str());
	}
#else
	if (ar.caps(Serialization::IArchive::BINARY))
	{
		return ar(str.m_hash, name, label);
	}
	else
	{
		string temp;
		if (ar.isOutput())
			temp = str.GetText();  //we serialize our hash code as a string
		if (!ar(temp, name, label))
			return false;
		if (ar.isInput())
			str.m_hash = str.CreateHash(temp.c_str());
	}
#endif
	return true;
}

//! \endcond
