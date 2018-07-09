// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../CryCore/CryCrc32.h"
#include "../CryString/CryString.h"

#include "TypeTraits.h"
#include "TypeOperators.h"

namespace Cry {
namespace Type {
namespace Utils {

// TODO: Move this to somewhere were everyone would expect it.
class CCompileTime_String
{
public:
	template<size_t LENGTH>
	constexpr CCompileTime_String(const char (&charArray)[LENGTH])
		: m_length(LENGTH)
		, m_pData(charArray)
	{}

	constexpr CCompileTime_String(const char* szData, size_t length)
		: m_length(length)
		, m_pData(szData)
	{}

	constexpr char operator[](size_t index) const
	{
		return index < m_length ? m_pData[index] : 0;
	}

	constexpr size_t length() const
	{
		return m_length;
	}

	constexpr const char* c_str() const
	{
		return m_pData;
	}

private:
	const size_t m_length;
	const char*  m_pData;
};
// ~TODO

template<typename T>
struct Explicit {};

template<typename T>
struct SCompileTime_TypeInfo
{
	constexpr static CCompileTime_String GetName()
	{
		return TypeName();
	}

	constexpr static uint32 GetCrc32()
	{
		return ComputeCrc(TypeName());
	}

private:
	constexpr static uint32 ComputeCrc(CCompileTime_String str)
	{
		return CCrc32::Compute_CompileTime(str.c_str(), str.length());
	}

	constexpr static CCompileTime_String TypeName()
	{
#if defined (CRY_PLATFORM_ORBIS) /*|| defined(CRY_PLATFORM_ANDROID) || defined(CRY_PLATFORM_APPLE) || defined(CRY_PLATFORM_LINUX)*/
		return CCompileTime_String(__PRETTY_FUNCTION__ + GetTypeNameBegin(__PRETTY_FUNCTION__), GetTypeNameLength(__PRETTY_FUNCTION__));
#elif defined (CRY_PLATFORM_WINDOWS) || defined(CRY_PLATFORM_DURANGO)
		return CCompileTime_String(__FUNCTION__ + GetTypeNameBegin(__FUNCTION__), GetTypeNameLength(__FUNCTION__));
#else
		return CCompileTime_String(__PRETTY_FUNCTION__);
#endif
	}

	enum { Offset = sizeof("Cry::Type::Utils::CompileTime_TypeInfo<") };

	constexpr static uint32 GetTypeNameBegin(CCompileTime_String str)
	{
		// TODO: Validate that this runs on the commented out platforms.
#if defined(CRY_PLATFORM_ORBIS) /*|| defined(CRY_PLATFORM_ANDROID) || defined(CRY_PLATFORM_APPLE) || defined(CRY_PLATFORM_LINUX)*/
		return sizeof("static Cry::Type::Utils::CCompileTime_String Cry::Type::Utils::SCompileTime_TypeInfo<") - 1;
#elif defined(CRY_PLATFORM_WINDOWS) || defined(CRY_PLATFORM_DURANGO)
		return str[Offset + 0] == 'c' && str[Offset + 1] == 'l' && str[Offset + 2] == 'a' && str[Offset + 3] == 's' && str[Offset + 4] == 's' && str[Offset + 5] == ' ' ?
		       sizeof("Cry::Type::Utils::CompileTime_TypeInfo<class ")
		       : str[Offset + 0] == 's' && str[Offset + 1] == 't' && str[Offset + 2] == 'r' && str[Offset + 3] == 'u' && str[Offset + 4] == 'c' && str[Offset + 5] == 't' && str[Offset + 6] == ' ' ?
		       sizeof("Cry::Type::Utils::CompileTime_TypeInfo<struct ")
		       : str[Offset + 0] == 'e' && str[Offset + 1] == 'n' && str[Offset + 2] == 'u' && str[Offset + 3] == 'm' && str[Offset + 4] == ' ' ?
		       sizeof("Cry::Type::Utils::CompileTime_TypeInfo<enum ")
		       : Offset;
#else
		return 0;
#endif
	}

	constexpr static uint32 GetTypeNameLength(CCompileTime_String str)
	{
		return FindTypeNameEnd(CCompileTime_String(str.c_str() + GetTypeNameBegin(str), str.length() - GetTypeNameBegin(str)), 0);
	}

	constexpr static uint32 FindTypeNameEnd(CCompileTime_String str, size_t fromIndex)
	{
		// TODO: Validate that this runs on the commented out platforms.
#if defined(CRY_PLATFORM_ORBIS) /*|| defined(CRY_PLATFORM_ANDROID) || defined(CRY_PLATFORM_APPLE) || defined(CRY_PLATFORM_LINUX)*/

		return str[fromIndex + 0] == '>' &&
		       str[fromIndex + 1] == ':' &&
		       str[fromIndex + 2] == ':' &&
		       str[fromIndex + 3] == 'T' &&
		       str[fromIndex + 4] == 'y' &&
		       str[fromIndex + 5] == 'p' &&
		       str[fromIndex + 6] == 'e' &&
		       str[fromIndex + 7] == 'N' &&
		       str[fromIndex + 8] == 'a' &&
		       str[fromIndex + 9] == 'm' &&
		       str[fromIndex + 10] == 'e' &&
		       str[fromIndex + 11] == '(' &&
		       str[fromIndex + 12] == ')' &&
		       str[fromIndex + 13] == ' ' &&
		       str[fromIndex + 14] == '[' &&
		       str[fromIndex + 15] == 'T' &&
		       str[fromIndex + 16] == ' ' &&
		       str[fromIndex + 17] == '=' &&
		       str[fromIndex + 18] == ' '
		       ? str[fromIndex - 1] != ' ' ? fromIndex : fromIndex - 1
		       : FindTypeNameEnd(str, fromIndex + 1);
#elif defined (CRY_PLATFORM_WINDOWS) || defined(CRY_PLATFORM_DURANGO)
		return str.length() - (str[str.length() - sizeof(" >::TypeName")] != ' ' ? sizeof(">::TypeName") : sizeof(" >::TypeName"));
#else
		return 0;
#endif
	}
};

}   // ~Utils namespace

// TODO: Where to move this?
template<typename TYPE>
struct PureType
{
	typedef typename std::remove_cv<typename std::remove_reference<typename std::remove_pointer<TYPE>::type>::type>::type Type;
};

template<typename TYPE, bool IS_ABSTRACT = std::is_abstract<TYPE>::value>
struct AlignOf
{
	static const size_t Value = alignof(TYPE);
};

template<typename TYPE>
struct AlignOf<TYPE, true>
{
	static const size_t Value = 0;
};
// ~TODO

} // ~Reflection namespace
} // ~Cry namespace
