// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryStringUtils.h"  // cry_vsprintf(), CryStringUtils_Internal::compute_length_formatted_va
#include "CryString.h"

#include <string.h>
#include <wchar.h>
#include <limits.h>
#include <stdarg.h>
#include <ctype.h>
#include <wctype.h>
#include <type_traits>  // std::is_same<>

//! \cond INTERNAL
#ifndef CRY_STRING_DEBUG
	#define CRY_STRING_DEBUG(s)
#endif

template<class T> class CharTraits;

//! Traits for char.
template<>
class CharTraits<char>
{
public:
	typedef size_t            size_type;
	typedef char              value_type;
	typedef const value_type* const_str;
	typedef value_type*       pointer;
	typedef const value_type* const_pointer;
	typedef value_type&       reference;
	typedef const value_type& const_reference;
	typedef pointer           iterator;
	typedef const_pointer     const_iterator;

	ILINE int _isspace(value_type c) const
	{
		return ::isspace((int)c);
	}

	ILINE value_type _traits_toupper(value_type c) const
	{
		return toupper(c);
	}

	ILINE value_type _traits_tolower(value_type c) const
	{
		return tolower(c);
	}

	ILINE value_type _ascii_tolower(value_type c) const
	{
		return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
	}

	ILINE value_type _ascii_toupper(value_type c) const
	{
		return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
	}

	ILINE int _strcmp(const_str a, const_str b) const
	{
		return ::strcmp(a, b);
	}

	ILINE int _strncmp(const_str a, const_str b, size_type n) const
	{
		return ::strncmp(a, b, n);
	}

	ILINE int _stricmp(const_str a, const_str b) const
	{
		PREFAST_SUPPRESS_WARNING(4996); // 'function': was declared deprecated
		return ::stricmp(a, b);
	}

	ILINE int _strnicmp(const_str a, const_str b, size_type n) const
	{
		PREFAST_SUPPRESS_WARNING(4996); // 'function': was declared deprecated
		return ::strnicmp(a, b, n);
	}

	ILINE size_type _strspn(const_str str, const_str strCharSet) const
	{
		return (str == NULL) ? 0 : (size_type)::strspn(str, strCharSet);
	}

	ILINE size_type _strcspn(const_str str, const_str strCharSet) const
	{
		return (str == NULL) ? 0 : (size_type)::strcspn(str, strCharSet);
	}

	static ILINE size_type _strlen(const_str str)
	{
		return (str == NULL) ? 0 : (size_type)::strlen(str);
	}

	ILINE const_str _strchr(const_str str, value_type c) const
	{
		return (str == NULL) ? 0 : ::strchr(str, c);
	}

	ILINE value_type* _strstr(value_type* str, const_str strSearch) const
	{
		return (str == NULL) ? 0 : (value_type*)::strstr(str, strSearch);
	}

	ILINE void _copy(value_type* dest, const value_type* src, size_type count)
	{
		memcpy(dest, src, count * sizeof(value_type));
	}

	ILINE void _move(value_type* dest, const value_type* src, size_type count)
	{
		memmove(dest, src, count * sizeof(value_type));
	}

	ILINE void _set(value_type* dest, value_type ch, size_type count)
	{
		memset(dest, ch, count * sizeof(value_type));
	}
};

//! Traits for wchar_t.
template<>
class CharTraits<wchar_t>
{
public:
	typedef size_t            size_type;
	typedef wchar_t           value_type;
	typedef const value_type* const_str;
	typedef value_type*       pointer;
	typedef const value_type* const_pointer;
	typedef value_type&       reference;
	typedef const value_type& const_reference;
	typedef pointer           iterator;
	typedef const_pointer     const_iterator;

	ILINE int _isspace(value_type c) const
	{
		return iswspace(c);
	}

	ILINE value_type _traits_toupper(value_type c) const
	{
		return towupper(c);
	}

	ILINE value_type _traits_tolower(value_type c) const
	{
		return towlower(c);
	}

	ILINE value_type _ascii_tolower(value_type c) const
	{
		return ((((c) >= 'A') && ((c) <= 'Z')) ? ((c) - 'A' + 'a') : (c));
	}

	ILINE value_type _ascii_toupper(value_type c) const
	{
		return ((((c) >= 'a') && ((c) <= 'z')) ? ((c) - 'a' + 'A') : (c));
	}

	ILINE int _strcmp(const_str a, const_str b) const
	{
		return wcscmp(a, b);
	}

	ILINE int _strncmp(const_str a, const_str b, size_type n) const
	{
		return wcsncmp(a, b, n);
	}

	ILINE int _stricmp(const_str a, const_str b) const
	{
		PREFAST_SUPPRESS_WARNING(4996); // 'function': was declared deprecated
		return wcsicmp(a, b);
	}

	ILINE int _strnicmp(const_str a, const_str b, size_type n) const
	{
		PREFAST_SUPPRESS_WARNING(4996); // 'function': was declared deprecated
		return wcsnicmp(a, b, n);
	}

	ILINE size_type _strspn(const_str str, const_str strCharSet) const
	{
		return (str == NULL) ? 0 : (size_type)::wcsspn(str, strCharSet);
	}

	ILINE size_type _strcspn(const_str str, const_str strCharSet) const
	{
		return (str == NULL) ? 0 : (size_type)::wcscspn(str, strCharSet);
	}

	static ILINE size_type _strlen(const_str str)
	{
		return (str == NULL) ? 0 : (size_type)::wcslen(str);
	}

	ILINE const_str _strchr(const_str str, value_type c) const
	{
		return (str == NULL) ? 0 : ::wcschr(str, c);
	}

	ILINE value_type* _strstr(value_type* str, const_str strSearch) const
	{
		return (str == NULL) ? 0 : ::wcsstr(str, strSearch);
	}

	ILINE void _copy(value_type* dest, const value_type* src, size_type count)
	{
		memcpy(dest, src, count * sizeof(value_type));
	}

	ILINE void _move(value_type* dest, const value_type* src, size_type count)
	{
		memmove(dest, src, count * sizeof(value_type));
	}

	ILINE void _set(value_type* dest, value_type ch, size_type count)
	{
		wmemset(dest, ch, count);
	}
};
//! \endcond

template<class T, size_t S>
class CryStackStringT : public CharTraits<T>
{
public:
	//////////////////////////////////////////////////////////////////////////
	// Types compatible with STL string.
	//////////////////////////////////////////////////////////////////////////
	typedef CryStackStringT<T, S> _Self;
	typedef size_t                size_type;
	typedef T                     value_type;
	typedef const value_type*     const_str;
	typedef value_type*           pointer;
	typedef const value_type*     const_pointer;
	typedef value_type&           reference;
	typedef const value_type&     const_reference;
	typedef pointer               iterator;
	typedef const_pointer         const_iterator;
	static const size_type MAX_SIZE = S;

	enum _npos_type : size_t
	{
		npos = (size_type) ~0
	};

	//////////////////////////////////////////////////////////////////////////
	// Constructors
	//////////////////////////////////////////////////////////////////////////
	CryStackStringT();

public:

	CryStackStringT(const _Self& str);
	CryStackStringT(const _Self& str, size_type nOff, size_type nCount);
	CryStackStringT(size_type nRepeat, value_type ch);

private:
	//! The reason of making this constructor unavailable (for a while) is explained in comments
	//! in front of the declaration of CryStringT::CryStringT(size_type nRepeat, value_type ch), see CryString.h.
	CryStackStringT(value_type ch, size_type nRepeat);

public:

	CryStackStringT(const_str str);
	CryStackStringT(const CryStringT<T>& str);
	CryStackStringT(const_str str, size_type nLength);
	CryStackStringT(const_iterator _First, const_iterator _Last);
	~CryStackStringT();

	//////////////////////////////////////////////////////////////////////////
	// STL string like interface.
	//////////////////////////////////////////////////////////////////////////
	//! Operators.
	size_type length() const;
	size_type size() const;
	bool      empty() const;
	void      clear(); //!< Free up the data.

	//! Returns the storage currently allocated to hold the string, a value at least as large as length().
	//! Also: capacity is always >= (MAX_SIZE-1).
	size_type capacity() const;

	//! Sets the capacity of the string to a number at least as great as a specified number.
	//! Shrink to fit post-cond: capacity() >= (MAX_SIZE-1).
	//! \param nCount = 0 is shrinking string to fit number of characters in it.
	void       reserve(size_type nCount = 0);

	_Self&     append(const value_type* _Ptr);
	_Self&     append(const value_type* _Ptr, size_type nCount);
	_Self&     append(const _Self& _Str, size_type nOff, size_type nCount);
	_Self&     append(const _Self& _Str);
	_Self&     append(size_type nCount, value_type _Ch);
	_Self&     append(const_iterator _First, const_iterator _Last);

	_Self&     assign(const_str _Ptr);
	_Self&     assign(const_str _Ptr, size_type nCount);
	_Self&     assign(const _Self& _Str, size_type off, size_type nCount);
	_Self&     assign(const _Self& _Str);
	_Self&     assign(size_type nCount, value_type _Ch);
	_Self&     assign(const_iterator _First, const_iterator _Last);

	value_type at(size_type index) const;
	//value_type& at( size_type index );

	const_iterator begin() const { return m_str; };
	const_iterator end() const   { return m_str + length(); };

	iterator       begin()       { return m_str; };
	iterator       end()         { return m_str + length(); };

	//! cast to C string operator.
	operator const_str() const { return m_str; }

	//! cast to C string.
	const value_type* c_str() const { return m_str; }
	const value_type* data() const  { return m_str; };

	//////////////////////////////////////////////////////////////////////////
	// string comparison.
	//////////////////////////////////////////////////////////////////////////
	int compare(const _Self& _Str) const;
	int compare(size_type _Pos1, size_type _Num1, const _Self& _Str) const;
	int compare(size_type _Pos1, size_type _Num1, const _Self& _Str, size_type nOff, size_type nCount) const;
	int compare(const value_type* _Ptr) const;
	int compare(size_type _Pos1, size_type _Num1, const value_type* _Ptr, size_type _Num2 = npos) const;

	// Case insensitive comparison
	int compareNoCase(const _Self& _Str) const;
	int compareNoCase(size_type _Pos1, size_type _Num1, const _Self& _Str) const;
	int compareNoCase(size_type _Pos1, size_type _Num1, const _Self& _Str, size_type nOff, size_type nCount) const;
	int compareNoCase(const value_type* _Ptr) const;
	int compareNoCase(size_type _Pos1, size_type _Num1, const value_type* _Ptr, size_type _Num2 = npos) const;

	//! Copies at most a specified number of characters from an indexed position in a source string to a target character array.
	size_type copy(value_type* _Ptr, size_type nCount, size_type nOff = 0) const;

	void      push_back(value_type _Ch) { _ConcatenateInPlace(&_Ch, 1); }
	void      resize(size_type nCount, value_type _Ch = ' ');

	//! Simple sub-string extraction.
	_Self substr(size_type pos, size_type count = npos) const;

	// Replace part of string.
	_Self& replace(value_type chOld, value_type chNew);
	_Self& replace(const_str strOld, const_str strNew);
	_Self& replace(size_type pos, size_type count, const_str strNew);
	_Self& replace(size_type pos, size_type count, const_str strNew, size_type count2);
	_Self& replace(size_type pos, size_type count, size_type nNumChars, value_type chNew);

	// Insert new elements to string.
	_Self& insert(size_type nIndex, value_type ch);
	_Self& insert(size_type nIndex, size_type nCount, value_type ch);
	_Self& insert(size_type nIndex, const_str pstr);
	_Self& insert(size_type nIndex, const_str pstr, size_type nCount);

	//! Delete count characters starting at zero-based index.
	_Self& erase(size_type nIndex, size_type count = npos);

	//! Search (return starting index, or -1 if not found) look for a single character match like "C" strchr.
	size_type find(value_type ch, size_type pos = 0) const;

	//! Look for a specific sub-string like "C" strstr.
	size_type find(const_str subs, size_type pos = 0) const;
	size_type rfind(value_type ch, size_type pos = npos) const;

	size_type find_first_of(value_type _Ch, size_type nOff = 0) const;
	size_type find_first_of(const_str charSet, size_type nOff = 0) const;
	size_type find_first_of(const _Self& _Str, size_type _Off = 0) const;

	size_type find_first_not_of(value_type _Ch, size_type _Off = 0) const;
	size_type find_first_not_of(const value_type* _Ptr, size_type _Off = 0) const;
	size_type find_first_not_of(const value_type* _Ptr, size_type _Off, size_type _Count) const;
	size_type find_first_not_of(const _Self& _Str, size_type _Off = 0) const;

	void      swap(_Self& _Str);

	//////////////////////////////////////////////////////////////////////////
	// overloaded operators.
	//////////////////////////////////////////////////////////////////////////

	value_type operator[](size_type index) const;
	//	value_type&	operator[]( size_type index ); // same as at()

	_Self& operator=(const _Self& str);
	_Self& operator=(value_type ch);
	_Self& operator=(const_str str);
	_Self& operator=(const CryStringT<T>& str);

	void   move(_Self& src);

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	template<size_t AnySize>
	_Self& operator=(const CryStackStringT<T, AnySize>& str)
	{
		_Assign(str.c_str(), str.size());
		return *this;
	}
#endif

	// string concatenation
	_Self& operator+=(const _Self& str);
	_Self& operator+=(value_type ch);
	_Self& operator+=(const_str str);
	_Self& operator+=(const CryStringT<T>& str);

	size_t GetAllocatedMemory() const
	{
		size_t size = sizeof(*this);
		if (m_str != m_strBuf)
			size += (m_nAllocSize + 1) * sizeof(value_type);
		return size;
	}

	//////////////////////////////////////////////////////////////////////////
	//! Extended functions.
	//! This functions are not in the STL string.
	//! They have an ATL CString interface.
	//////////////////////////////////////////////////////////////////////////
	//! Format string printf-like.
	//! This function exists in 'char' version of the class only: it's
	//! almost impossible to implement it robustly for wchar_t - different
	//! compilers handle wchar_t formatting differently.
	_Self& Format(const value_type* format, ...);

	//! Format string printf-like. va_list version.
	//! This function exists in 'char' version of the class only: it's
	//! almost impossible to implement it robustly for wchar_t - different
	//! compilers handle wchar_t formatting differently.
	_Self& FormatV(const value_type* format, va_list args);

	//! Converts the string to lower-case.
	//! This function uses the "C" locale for case-conversion (ie, A-Z only).
	_Self& MakeLower();

	//! Converts the string to upper-case.
	//! This function uses the "C" locale for case-conversion (ie, A-Z only).
	_Self& MakeUpper();

	_Self& Trim();
	_Self& Trim(value_type ch);
	_Self& Trim(const value_type* sCharSet);

	_Self& TrimLeft();
	_Self& TrimLeft(value_type ch);
	_Self& TrimLeft(const value_type* sCharSet);

	_Self& TrimRight();
	_Self& TrimRight(value_type ch);
	_Self& TrimRight(const value_type* sCharSet);

	_Self  SpanIncluding(const_str charSet) const;
	_Self  SpanExcluding(const_str charSet) const;
	_Self  Tokenize(const_str charSet, int& nStart) const;
	_Self  Mid(size_type nFirst, size_type nCount = npos) const { return substr(nFirst, nCount); };

	_Self  Left(size_type count) const;
	_Self  Right(size_type count) const;
	//////////////////////////////////////////////////////////////////////////

	// public utilities.
	static bool _IsValidString(const_str str);

public:
	//! Only used for debugging statistics.
	static size_t _usedMemory(size_t size)
	{
		static size_t s_used_memory = 0;
		s_used_memory += size;
		return s_used_memory;
	}

public:
	size_type   m_nLength;
	size_type   m_nAllocSize;
	value_type* m_str;                //!< Pointer to ref counted string data.
	value_type  m_strBuf[MAX_SIZE];

	// implementation helpers
	void _AllocData(size_type nLen);
	void _FreeData(value_type* pData);
	void _Free();
	void _Initialize();

	void _Concatenate(const_str sStr1, size_type nLen1, const_str sStr2, size_type nLen2);
	void _ConcatenateInPlace(const_str sStr, size_type nLen);
	void _Assign(const_str sStr, size_type nLen);
	void _MakeUnique();
};

/////////////////////////////////////////////////////////////////////////////
// CryStackStringT<T,S> Implementation
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline bool CryStackStringT<T, S >::_IsValidString(const_str)
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline void CryStackStringT<T, S >::_Assign(const_str sStr, size_type nLen)
{
	// Check if this string is shared (reference count greater then 1) or not enough capacity to store new string.
	// Then allocate new string buffer.
	if (nLen > capacity())
	{
		_Free();
		_AllocData(nLen);
	}
	// Copy characters from new string to this buffer.
	CharTraits<T>::_copy(m_str, sStr, nLen);

	// Set new length.
	m_nLength = nLen;

	// Make null terminated string.
	m_str[nLen] = 0;
	CRY_STRING_DEBUG(m_str)
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline void CryStackStringT<T, S >::_Concatenate(const_str sStr1, size_type nLen1, const_str sStr2, size_type nLen2)
{
	size_type nLen = nLen1 + nLen2;
	if (nLen1 * 2 > nLen)
		nLen = nLen1 * 2;
	if (nLen != 0)
	{
		if (nLen < 8)
			nLen = 8;
		_AllocData(nLen);
		CharTraits<T>::_copy(m_str, sStr1, nLen1);
		CharTraits<T>::_copy(m_str + nLen1, sStr2, nLen2);
		m_nLength = nLen1 + nLen2;
		m_str[nLen1 + nLen2] = 0;
	}
	CRY_STRING_DEBUG(m_str)
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline void CryStackStringT<T, S >::_ConcatenateInPlace(const_str sStr, size_type nLen)
{
	if (nLen != 0)
	{
		// Check if this string is shared (reference count greater then 1) or not enough capacity to store new string.
		// Then allocate new string buffer.
		if (length() + nLen > capacity())
		{
			value_type* pOldData = m_str;
			_Concatenate(m_str, length(), sStr, nLen);
			_FreeData(pOldData);
		}
		else
		{
			CharTraits<T>::_copy(m_str + length(), sStr, nLen);
			m_nLength += nLen;
			m_str[m_nLength] = 0; // Make null terminated string.
		}
	}
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline void CryStackStringT<T, S >::_MakeUnique()
{
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline void CryStackStringT<T, S >::_Initialize()
{
	m_strBuf[0] = 0;
	m_str = m_strBuf;
	m_nLength = 0;
	m_nAllocSize = MAX_SIZE - 1; // 0 termination not counted!
}

//! Always allocate one extra character for '\0' termination.
//! Assumes (optimistically) that data length will equal allocation length.
template<class T, size_t S>
inline void CryStackStringT<T, S >::_AllocData(size_type nLen)
{
	assert(nLen >= 0);
	assert(nLen <= INT_MAX - 1);    // max size (enough room for 1 extra)

	if (nLen == 0)
		_Initialize();
	else
	{
		size_type allocLen = (nLen + 1) * sizeof(value_type);
		value_type* pData = m_strBuf;
		if (allocLen > MAX_SIZE)
		{
			pData = (value_type*)CryModuleMalloc(allocLen);
			_usedMemory(allocLen);   // For statistics.
			m_nAllocSize = nLen;
		}
		else
		{
			m_nAllocSize = MAX_SIZE - 1;
		}
		m_nLength = nLen;
		m_str = pData;
		m_str[nLen] = 0; // null terminated string.
	}
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline void CryStackStringT<T, S >::_Free()
{
	_FreeData(m_str);
	_Initialize();
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline void CryStackStringT<T, S >::_FreeData(value_type* pData)
{
	if (pData != m_strBuf)
	{
		size_t allocLen = (m_nAllocSize + 1) * sizeof(value_type);
		_usedMemory(-check_cast<ptrdiff_t>(allocLen));   // For statistics.
		CryModuleFree(pData);
	}
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>::CryStackStringT()
{
	_Initialize();
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>::CryStackStringT(const CryStackStringT<T, S>& str)
{
	_Initialize();
	_Assign(str.c_str(), str.length());
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>::CryStackStringT(const CryStackStringT<T, S>& str, size_type nOff, size_type nCount)
{
	_Initialize();
	assign(str, nOff, nCount);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>::CryStackStringT(const_str str)
{
	_Initialize();
	// Make a copy of C string.
	size_type nLen = this->_strlen(str);
	if (nLen != 0)
	{
		_AllocData(nLen);
		CharTraits<T>::_copy(m_str, str, nLen);
		CRY_STRING_DEBUG(m_str)
	}
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>::CryStackStringT(const CryStringT<T>& str)
{
	_Initialize();
	size_t nLength = str.size();
	if (nLength > 0)
	{
		_AllocData(nLength);
		CharTraits<T>::_copy(m_str, str.c_str(), nLength);
		CRY_STRING_DEBUG(m_str)
	}
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>::CryStackStringT(const_str str, size_type nLength)
{
	_Initialize();
	if (nLength > 0)
	{
		_AllocData(nLength);
		CharTraits<T>::_copy(m_str, str, nLength);
		CRY_STRING_DEBUG(m_str)
	}
}

//////////////////////////////////////////////////////////////////////////
//! The reason for making this constructor unavailable (for a while) is explained in comments
//! in front of the declaration of CryStringT::CryStringT(size_type nRepeat, value_type ch)
//! (see CryString.h).
template<class T, size_t S>
inline CryStackStringT<T, S>::CryStackStringT(size_type nRepeat, value_type ch)
{
	_Initialize();
	if (nRepeat > 0)
	{
		_AllocData(nRepeat);
		CharTraits<T>::_set(m_str, ch, nRepeat);
		CRY_STRING_DEBUG(m_str)
	}
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>::CryStackStringT(const_iterator _First, const_iterator _Last)
{
	_Initialize();
	size_type nLength = (size_type)(_Last - _First);
	if (nLength > 0)
	{
		_AllocData(nLength);
		CharTraits<T>::_copy(m_str, _First, nLength);
		CRY_STRING_DEBUG(m_str)
	}
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>::~CryStackStringT()
{
	_FreeData(m_str);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::length() const
{
	return m_nLength;
}
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::size() const
{
	return m_nLength;
}
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::capacity() const
{
	return m_nAllocSize;
}

template<class T, size_t S>
inline bool CryStackStringT<T, S >::empty() const
{
	return length() == 0;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline void CryStackStringT<T, S >::clear()
{
	if (length() == 0)
		return;
	_Free();
	assert(length() == 0);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline void CryStackStringT<T, S >::reserve(size_type nCount)
{
	// Reserve of 0 is shrinking container to fit number of characters in it..
	if (nCount > capacity())
	{
		value_type* pOldData = m_str;
		size_type nOldLength = m_nLength;
		_AllocData(nCount);
		CharTraits<T>::_copy(m_str, pOldData, nOldLength);
		m_nLength = nOldLength;
		m_str[m_nLength] = 0;
		_FreeData(pOldData);
	}
	else if (nCount == 0)
	{
		if (length() != capacity())
		{
			value_type* pOldData = m_str;
			if (pOldData != m_strBuf)  // in case we fit into our static buffer, we cannot shrink anyway
			{
				size_type nOldLength = m_nLength;
				_AllocData(m_nLength);
				// if (pOldData != m_strBuf || m_str != m_strBuf) // actually always true
				// {
				CharTraits<T>::_copy(m_str, pOldData, nOldLength); // we can safely use CharTraits<T>::_copy, as we never overlap here (in case of static buffer, we don't shrink anyway)
				_FreeData(pOldData);
				// }
			}
		}
	}
	CRY_STRING_DEBUG(m_str)
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::append(const_str _Ptr)
{
	*this += _Ptr;
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::append(const_str _Ptr, size_type nCount)
{
	_ConcatenateInPlace(_Ptr, nCount);
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::append(const CryStackStringT<T, S>& _Str, size_type off, size_type nCount)
{
	size_type len = _Str.length();
	if (off > len)
		return *this;
	if (off + nCount > len)
		nCount = len - off;
	_ConcatenateInPlace(_Str.m_str + off, nCount);
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::append(const CryStackStringT<T, S>& _Str)
{
	*this += _Str;
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::append(size_type nCount, value_type _Ch)
{
	if (nCount > 0)
	{
		if (length() + nCount >= capacity())
		{
			value_type* pOldData = m_str;
			size_type nOldLength = m_nLength;
			_AllocData(length() + nCount);
			CharTraits<T>::_copy(m_str, pOldData, nOldLength);
			CharTraits<T>::_set(m_str + nOldLength, _Ch, nCount);
			_FreeData(pOldData);
		}
		else
		{
			size_type nOldLength = length();
			CharTraits<T>::_set(m_str + nOldLength, _Ch, nCount);
			m_nLength = nOldLength + nCount;
			m_str[length()] = 0; // Make null terminated string.
		}
	}
	CRY_STRING_DEBUG(m_str)
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::append(const_iterator _First, const_iterator _Last)
{
	append(_First, (size_type)(_Last - _First));
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::assign(const_str _Ptr)
{
	*this = _Ptr;
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::assign(const_str _Ptr, size_type nCount)
{
	size_type len = this->_strlen(_Ptr);
	_Assign(_Ptr, (nCount < len) ? nCount : len);
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::assign(const CryStackStringT<T, S>& _Str, size_type off, size_type nCount)
{
	size_type len = _Str.length();
	if (off > len)
		return *this;
	if (off + nCount > len)
		nCount = len - off;
	_Assign(_Str.m_str + off, nCount);
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::assign(const CryStackStringT<T, S>& _Str)
{
	*this = _Str;
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::assign(size_type nCount, value_type _Ch)
{
	if (nCount >= 1)
	{
		_AllocData(nCount);
		CharTraits<T>::_set(m_str, _Ch, nCount);
		CRY_STRING_DEBUG(m_str)
	}
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::assign(const_iterator _First, const_iterator _Last)
{
	assign(_First, (size_type)(_Last - _First));
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline typename CryStackStringT<T, S>::value_type CryStackStringT<T, S >::at(size_type index) const
{
	assert(index >= 0 && index < length());
	return m_str[index];
}

/*
   inline value_type& CryStackStringT<T,S>::at( size_type index )
   {
   // same as GetAt
   assert( index >= 0 && index < length() );
   return m_str[index];
   }
 */

template<class T, size_t S>
inline typename CryStackStringT<T, S>::value_type CryStackStringT<T, S >::operator[](size_type index) const
{
	assert(index < length());
	return m_str[index];
}

/*
   inline value_type& CryStackStringT<T,S>::operator[]( size_type index )
   {
   // same as GetAt
   assert( index >= 0 && index < length() );
   return m_str[index];
   }
 */

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline int CryStackStringT<T, S >::compare(const CryStackStringT<T, S>& _Str) const
{
	return CharTraits<T>::_strcmp(m_str, _Str.m_str);
}

template<class T, size_t S>
inline int CryStackStringT<T, S >::compare(size_type _Pos1, size_type _Num1, const CryStackStringT<T, S>& _Str) const
{
	return compare(_Pos1, _Num1, _Str.m_str, npos);
}

template<class T, size_t S>
inline int CryStackStringT<T, S >::compare(size_type _Pos1, size_type _Num1, const CryStackStringT<T, S>& _Str, size_type nOff, size_type nCount) const
{
	assert(nOff < _Str.length());
	return compare(_Pos1, _Num1, _Str.m_str + nOff, nCount);
}

template<class T, size_t S>
inline int CryStackStringT<T, S >::compare(const value_type* _Ptr) const
{
	return CharTraits<T>::_strcmp(m_str, _Ptr);
}

template<class T, size_t S>
inline int CryStackStringT<T, S >::compare(size_type _Pos1, size_type _Num1, const value_type* _Ptr, size_type _Num2) const
{
	assert(_Pos1 < length());
	if (length() - _Pos1 < _Num1)
		_Num1 = length() - _Pos1; // trim to size

	int res = _Num1 == 0 ? 0 : CharTraits<T>::_strncmp(m_str + _Pos1, _Ptr, (_Num1 < _Num2) ? _Num1 : _Num2);
	return (res != 0 ? res : _Num2 == npos && _Ptr[_Num1] == 0 ? 0 : _Num1 < _Num2 ? -1 : _Num1 == _Num2 ? 0 : +1);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline int CryStackStringT<T, S >::compareNoCase(const CryStackStringT<T, S>& _Str) const
{
	return CharTraits<T>::_stricmp(m_str, _Str.m_str);
}

template<class T, size_t S>
inline int CryStackStringT<T, S >::compareNoCase(size_type _Pos1, size_type _Num1, const CryStackStringT<T, S>& _Str) const
{
	return compareNoCase(_Pos1, _Num1, _Str.m_str, npos);
}

template<class T, size_t S>
inline int CryStackStringT<T, S >::compareNoCase(size_type _Pos1, size_type _Num1, const CryStackStringT<T, S>& _Str, size_type nOff, size_type nCount) const
{
	assert(nOff < _Str.length());
	return compareNoCase(_Pos1, _Num1, _Str.m_str + nOff, nCount);
}

template<class T, size_t S>
inline int CryStackStringT<T, S >::compareNoCase(const value_type* _Ptr) const
{
	return CharTraits<T>::_stricmp(m_str, _Ptr);
}

template<class T, size_t S>
inline int CryStackStringT<T, S >::compareNoCase(size_type _Pos1, size_type _Num1, const value_type* _Ptr, size_type _Num2) const
{
	assert(_Pos1 < length());
	if (length() - _Pos1 < _Num1)
		_Num1 = length() - _Pos1; // trim to size

	int res = _Num1 == 0 ? 0 : CharTraits<T>::_strnicmp(m_str + _Pos1, _Ptr, (_Num1 < _Num2) ? _Num1 : _Num2);
	return (res != 0 ? res : _Num2 == npos && _Ptr[_Num1] == 0 ? 0 : _Num1 < _Num2 ? -1 : _Num1 == _Num2 ? 0 : +1);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::copy(value_type* _Ptr, size_type nCount, size_type nOff) const
{
	assert(nOff < length());
	if (nOff + nCount > length()) // trim to offset.
		nCount = length() - nOff;

	CharTraits<T>::_copy(_Ptr, m_str + nOff, nCount);
	return nCount;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline void CryStackStringT<T, S >::resize(size_type nCount, value_type _Ch)
{
	_MakeUnique();
	if (nCount > length())
	{
		size_type numToAdd = nCount - length();
		append(numToAdd, _Ch);
	}
	else if (nCount < length())
	{
		m_nLength = nCount;
		m_str[length()] = 0; // Make null terminated string.
	}
}

//////////////////////////////////////////////////////////////////////////
//! compare helpers
template<class T, size_t S> inline bool operator==(const CryStackStringT<T, S>& s1, const CryStackStringT<T, S>& s2)
{ return s1.compare(s2) == 0; }
template<class T, size_t S> inline bool operator==(const CryStackStringT<T, S>& s1, const typename CryStackStringT<T, S>::value_type* s2)
{ return s1.compare(s2) == 0; }
template<class T, size_t S> inline bool operator==(const typename CryStackStringT<T, S>::value_type* s1, const CryStackStringT<T, S>& s2)
{ return s2.compare(s1) == 0; }
template<class T, size_t S> inline bool operator==(const CryStackStringT<T, S>& s1, const CryStringT<T>& s2)
{ return s1 == s2.c_str(); }
template<class T, size_t S> inline bool operator==(const CryStringT<T>& s1, const CryStackStringT<T, S>& s2)
{ return s1.c_str() == s2; }
template<class T, size_t S> inline bool operator!=(const CryStackStringT<T, S>& s1, const CryStackStringT<T, S>& s2)
{ return s1.compare(s2) != 0; }
template<class T, size_t S> inline bool operator!=(const CryStackStringT<T, S>& s1, const typename CryStackStringT<T, S>::value_type* s2)
{ return s1.compare(s2) != 0; }
template<class T, size_t S> inline bool operator!=(const typename CryStackStringT<T, S>::value_type* s1, const CryStackStringT<T, S>& s2)
{ return s2.compare(s1) != 0; }
template<class T, size_t S> inline bool operator!=(const CryStackStringT<T, S>& s1, const CryStringT<T>& s2)
{ return s1 != s2.c_str(); }
template<class T, size_t S> inline bool operator!=(const CryStringT<T>& s1, const CryStackStringT<T, S>& s2)
{ return s1.c_str() != s2; }
template<class T, size_t S> inline bool operator<(const CryStackStringT<T, S>& s1, const CryStackStringT<T, S>& s2)
{ return s1.compare(s2) < 0; }
template<class T, size_t S> inline bool operator<(const CryStackStringT<T, S>& s1, const typename CryStackStringT<T, S>::value_type* s2)
{ return s1.compare(s2) < 0; }
template<class T, size_t S> inline bool operator<(const typename CryStackStringT<T, S>::value_type* s1, const CryStackStringT<T, S>& s2)
{ return s2.compare(s1) > 0; }
template<class T, size_t S> inline bool operator>(const CryStackStringT<T, S>& s1, const CryStackStringT<T, S>& s2)
{ return s1.compare(s2) > 0; }
template<class T, size_t S> inline bool operator>(const CryStackStringT<T, S>& s1, const typename CryStackStringT<T, S>::value_type* s2)
{ return s1.compare(s2) > 0; }
template<class T, size_t S> inline bool operator>(const typename CryStackStringT<T, S>::value_type* s1, const CryStackStringT<T, S>& s2)
{ return s2.compare(s1) < 0; }
template<class T, size_t S> inline bool operator<=(const CryStackStringT<T, S>& s1, const CryStackStringT<T, S>& s2)
{ return s1.compare(s2) <= 0; }
template<class T, size_t S> inline bool operator<=(const CryStackStringT<T, S>& s1, const typename CryStackStringT<T, S>::value_type* s2)
{ return s1.compare(s2) <= 0; }
template<class T, size_t S> inline bool operator<=(const typename CryStackStringT<T, S>::value_type* s1, const CryStackStringT<T, S>& s2)
{ return s2.compare(s1) >= 0; }
template<class T, size_t S> inline bool operator>=(const CryStackStringT<T, S>& s1, const CryStackStringT<T, S>& s2)
{ return s1.compare(s2) >= 0; }
template<class T, size_t S> inline bool operator>=(const CryStackStringT<T, S>& s1, const typename CryStackStringT<T, S>::value_type* s2)
{ return s1.compare(s2) >= 0; }
template<class T, size_t S> inline bool operator>=(const typename CryStackStringT<T, S>::value_type* s1, const CryStackStringT<T, S>& s2)
{ return s2.compare(s1) <= 0; }

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::operator=(value_type ch)
{
	_Assign(&ch, 1);
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S> operator+(const CryStackStringT<T, S>& string1, typename CryStackStringT<T, S>::value_type ch)
{
	CryStackStringT<T, S> s(string1);
	s.append(1, ch);
	return s;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S> operator+(typename CryStackStringT<T, S>::value_type ch, const CryStackStringT<T, S>& str)
{
	CryStackStringT<T, S> s;
	s.reserve(str.size() + 1);
	s.append(1, ch);
	s.append(str);
	return s;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S> operator+(const CryStackStringT<T, S>& string1, const CryStackStringT<T, S>& string2)
{
	CryStackStringT<T, S> s(string1);
	s.append(string2);
	return s;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S> operator+(const CryStackStringT<T, S>& str1, const typename CryStackStringT<T, S>::value_type* str2)
{
	CryStackStringT<T, S> s(str1);
	s.append(str2);
	return s;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S> operator+(const typename CryStackStringT<T, S>::value_type* str1, const CryStackStringT<T, S>& str2)
{
	assert(str1 == NULL || (CryStackStringT<T, S>::_IsValidString(str1)));
	CryStackStringT<T, S> s;
	s.reserve(CharTraits<T>::_strlen(str1) + str2.size());
	s.append(str1);
	s.append(str2);
	return s;
}

template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::operator=(const CryStackStringT<T, S>& str)
{
	_Assign(str.c_str(), str.length());
	return *this;
}

template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::operator=(const_str str)
{
	assert(str == NULL || _IsValidString(str));
	_Assign(str, this->_strlen(str));
	return *this;
}

template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::operator=(const CryStringT<T>& str)
{
	_Assign(str.c_str(), str.size());
	return *this;
}

template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::operator+=(const CryStringT<T>& str)
{
	_ConcatenateInPlace(str.c_str(), str.size());
	return *this;
}

template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::operator+=(const_str str)
{
	assert(str == NULL || _IsValidString(str));
	_ConcatenateInPlace(str, this->_strlen(str));
	return *this;
}

template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::operator+=(value_type ch)
{
	_ConcatenateInPlace(&ch, 1);
	return *this;
}

template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::operator+=(const CryStackStringT<T, S>& str)
{
	_ConcatenateInPlace(str.m_str, str.length());
	return *this;
}

//! Find first single character.
//! return npos if not found and index otherwise.
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::find(value_type ch, size_type pos) const
{
	if (pos > length())
	{
		return (typename CryStackStringT<T, S>::size_type)npos;
	}
	const_str str = CharTraits<T>::_strchr(m_str + pos, ch);
	// return npos if not found and index otherwise
	return (str == NULL) ? npos : (size_type)(str - m_str);
}

//! Find a sub-string (like strstr).
//! \return npos for not found, distance from beginning otherwise.
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::find(const_str subs, size_type pos) const
{
	assert(_IsValidString(subs));
	if (pos > length())
	{
		return npos;
	}

	// find first matching substring
	const_str str = CharTraits<T>::_strstr(m_str + pos, subs);

	// return npos for not found, distance from beginning otherwise
	return (str == NULL) ? npos : (size_type)(str - m_str);

}

//! Find last single character.
//! \return -1 if not found, distance from beginning otherwise.
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::rfind(value_type ch, size_type pos) const
{
	const_str str;
	if (pos == npos)
	{
		// find last single character
		str = strrchr(m_str, ch);
		// return -1 if not found, distance from beginning otherwise
		return (str == NULL) ? (size_type) - 1 : (size_type)(str - m_str);
	}
	else
	{
		if (pos > length())
		{
			return npos;
		}

		value_type tmp = m_str[pos + 1];
		m_str[pos + 1] = 0;
		str = strrchr(m_str, ch);
		m_str[pos + 1] = tmp;
	}
	// return -1 if not found, distance from beginning otherwise
	return (str == NULL) ? (size_type) - 1 : (size_type)(str - m_str);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::find_first_of(const CryStackStringT<T, S>& _Str, size_type _Off) const
{
	return find_first_of(_Str.m_str, _Off);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::find_first_of(value_type _Ch, size_type nOff) const
{
	if (nOff > length())
	{
		return npos;
	}
	value_type charSet[2] = { _Ch, 0 };
	const_str str = strpbrk(m_str + nOff, charSet);
	return (str == NULL) ? -1 : (size_type)(str - m_str);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::find_first_of(const_str charSet, size_type nOff) const
{
	assert(_IsValidString(charSet));
	if (nOff > length())
	{
		return npos;
	}
	const_str str = strpbrk(m_str + nOff, charSet);
	return (str == NULL) ? (size_type) - 1 : (size_type)(str - m_str);
}

//size_type find_first_not_of(const _Self& __s, size_type __pos = 0) const
//{ return find_first_not_of(__s._M_start, __pos, __s.size()); }

//size_type find_first_not_of(const _CharT* __s, size_type __pos = 0) const
//{ _STLP_FIX_LITERAL_BUG(__s) return find_first_not_of(__s, __pos, _Traits::length(__s)); }

//size_type find_first_not_of(const _CharT* __s, size_type __pos,	size_type __n) const;

//size_type find_first_not_of(_CharT __c, size_type __pos = 0) const;

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::find_first_not_of(const value_type* _Ptr, size_type _Off) const
{ return find_first_not_of(_Ptr, _Off, this->_strlen(_Ptr)); }

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::find_first_not_of(const CryStackStringT<T, S>& _Str, size_type _Off) const
{ return find_first_not_of(_Str.m_str, _Off); }

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::find_first_not_of(value_type _Ch, size_type _Off) const
{
	if (_Off > length())
		return npos;
	else
	{
		for (const value_type* str = begin() + _Off; str != end(); ++str)
			if (*str != _Ch)
				return size_type(str - begin());
		// Character found!
		return npos;
	}
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline typename CryStackStringT<T, S>::size_type CryStackStringT<T, S >::find_first_not_of(const value_type* _Ptr, size_type _Off, size_type _Count) const
{
	if (_Off > length())
		return npos;
	else
	{
		const value_type* charsFirst = _Ptr, * charsLast = _Ptr + _Count;
		for (const value_type* str = begin() + _Off; str != end(); ++str)
		{
			const value_type* c;
			for (c = charsFirst; c != charsLast; ++c)
			{
				if (*c == *str)
					break;
			}
			if (c == charsLast)
				return size_type(str - begin());// Current character not in char set.
		}
		return npos;
	}
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S> CryStackStringT<T, S >::substr(size_type pos, size_type count) const
{
	if (pos >= length())
	{
		return CryStackStringT<T, S>();
	}
	if (count == npos)
	{
		count = length() - pos;
	}
	if (pos + count > length())
	{
		count = length() - pos;
	}
	return CryStackStringT<T, S>(m_str + pos, count);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::erase(size_type nIndex, size_type nCount)
{
	if (nCount > length() - nIndex)
	{
		nCount = length() - nIndex;
	}
	if (nIndex < length())
	{
		_MakeUnique();
		size_type nNumToCopy = length() - (nIndex + nCount) + 1;
		CharTraits<T>::_move(m_str + nIndex, m_str + nIndex + nCount, nNumToCopy);
		m_nLength = length() - nCount;
	}

	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::insert(size_type nIndex, value_type ch)
{
	return insert(nIndex, 1, ch);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::insert(size_type nIndex, size_type nCount, value_type ch)
{
	_MakeUnique();

	size_type nNewLength = length();
	if (nIndex > nNewLength)
		nIndex = nNewLength;
	nNewLength += nCount;

	if (capacity() < nNewLength)
	{
		value_type* pOldData = m_str;
		size_type nOldLength = m_nLength;
		const_str pstr = m_str;
		_AllocData(nNewLength);
		CharTraits<T>::_copy(m_str, pstr, nOldLength + 1);
		_FreeData(pOldData);
	}

	CharTraits<T>::_move(m_str + nIndex + nCount, m_str + nIndex, (nNewLength - nIndex));
	CharTraits<T>::_set(m_str + nIndex, ch, nCount);
	m_nLength = nNewLength;
	CRY_STRING_DEBUG(m_str)

	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::insert(size_type nIndex, const_str pstr, size_type nCount)
{
	size_type nInsertLength = nCount;
	size_type nNewLength = length();
	if (nInsertLength > 0)
	{
		_MakeUnique();
		if (nIndex > nNewLength)
			nIndex = nNewLength;
		nNewLength += nInsertLength;

		if (capacity() < nNewLength)
		{
			value_type* pOldData = m_str;
			size_type nOldLength = m_nLength;
			const_str pOldStr = m_str;
			_AllocData(nNewLength);
			CharTraits<T>::_copy(m_str, pOldStr, (nOldLength + 1));
			_FreeData(pOldData);
		}

		CharTraits<T>::_move(m_str + nIndex + nInsertLength, m_str + nIndex, (nNewLength - nIndex - nInsertLength + 1));
		CharTraits<T>::_copy(m_str + nIndex, pstr, nInsertLength);
		m_nLength = nNewLength;
		m_str[length()] = 0;
	}
	CRY_STRING_DEBUG(m_str)

	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::insert(size_type nIndex, const_str pstr)
{
	return insert(nIndex, pstr, this->_strlen(pstr));
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::replace(size_type pos, size_type count, const_str strNew)
{
	return replace(pos, count, strNew, this->_strlen(strNew));
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::replace(size_type pos, size_type count, const_str strNew, size_type count2)
{
	erase(pos, count);
	insert(pos, strNew, count2);
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::replace(size_type pos, size_type count, size_type nNumChars, value_type chNew)
{
	erase(pos, count);
	insert(pos, nNumChars, chNew);
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::replace(value_type chOld, value_type chNew)
{
	if (chOld != chNew)
	{
		_MakeUnique();
		value_type* strend = m_str + length();
		for (value_type* str = m_str; str != strend; ++str)
		{
			if (*str == chOld)
			{
				*str = chNew;
			}
		}
	}
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::replace(const_str strOld, const_str strNew)
{
	size_type nSourceLen = this->_strlen(strOld);
	if (nSourceLen == 0)
		return *this;
	size_type nReplacementLen = this->_strlen(strNew);

	size_type nCount = 0;
	value_type* strStart = m_str;
	value_type* strEnd = m_str + length();
	value_type* strTarget;
	while (strStart < strEnd)
	{
		while ((strTarget = CharTraits<T>::_strstr(strStart, strOld)) != NULL)
		{
			nCount++;
			strStart = strTarget + nSourceLen;
		}
		strStart += this->_strlen(strStart) + 1;
	}

	if (nCount > 0)
	{
		_MakeUnique();

		size_type nOldLength = length();
		size_type nNewLength = nOldLength + (nReplacementLen - nSourceLen) * nCount;
		if (capacity() < nNewLength)
		{
			value_type* pOldData = m_str;
			size_type nPrevLength = m_nLength;
			const_str pstr = m_str;
			_AllocData(nNewLength);
			CharTraits<T>::_copy(m_str, pstr, nPrevLength);
			_FreeData(pOldData);
		}
		strStart = m_str;
		strEnd = m_str + length();

		while (strStart < strEnd)
		{
			while ((strTarget = CharTraits<T>::_strstr(strStart, strOld)) != NULL)
			{
				size_type nBalance = nOldLength - ((size_type)(strTarget - m_str) + nSourceLen);
				CharTraits<T>::_move(strTarget + nReplacementLen, strTarget + nSourceLen, nBalance);
				CharTraits<T>::_copy(strTarget, strNew, nReplacementLen);
				strStart = strTarget + nReplacementLen;
				strStart[nBalance] = 0;
				nOldLength += (nReplacementLen - nSourceLen);
			}
			strStart += this->_strlen(strStart) + 1;
		}
		m_nLength = nNewLength;
	}
	CRY_STRING_DEBUG(m_str)

	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline void CryStackStringT<T, S >::move(CryStackStringT<T, S>& str)
{
	memcpy(this, &str, sizeof(*this));
	if (str.m_str == str.m_strBuf)
		m_str = m_strBuf;
	str._Initialize(); // Reset input string
}

template<class T, size_t S>
inline void CryStackStringT<T, S >::swap(CryStackStringT<T, S>& _Str)
{
	CryStackStringT<T, S> temp;
	temp.move(*this);
	move(_Str);
	_Str.move(temp);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::FormatV(const T* format, va_list args)
{
	static_assert(std::is_same<T, char>::value, "This function supports 'char' only");
	assert(_IsValidString(format));

#if CRY_COMPILER_MSVC && CRY_COMPILER_VERSION <= 1700
	// Visual Studio 2012 and older don't have va_copy()
	const int n = CryStringUtils_Internal::compute_length_formatted_va(format, args);
#else
	va_list argsCopy;
	va_copy(argsCopy, args);
	const int n = CryStringUtils_Internal::compute_length_formatted_va(format, argsCopy);
	va_end(argsCopy);
#endif

	if (n < 0)
	{
		clear();
	}
	else
	{
		resize(n);
		cry_vsprintf(m_str, n + 1, format, args);
	}
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::Format(const T* format, ...)
{
	static_assert(std::is_same<T, char>::value, "This function supports 'char' only");
	va_list args;
	va_start(args, format);
	FormatV(format, args);
	va_end(args);
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::MakeLower()
{
	_MakeUnique();
	for (value_type* s = m_str; *s != 0; s++)
	{
		*s = this->_ascii_tolower(*s);
	}
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::MakeUpper()
{
	_MakeUnique();
	for (value_type* s = m_str; *s != 0; s++)
	{
		*s = this->_ascii_toupper(*s);
	}
	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::Trim()
{
	return TrimRight().TrimLeft();
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::Trim(value_type ch)
{
	_MakeUnique();
	const value_type chset[2] = { ch, 0 };
	return TrimRight(chset).TrimLeft(chset);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::Trim(const value_type* sCharSet)
{
	_MakeUnique();
	return TrimRight(sCharSet).TrimLeft(sCharSet);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::TrimRight(value_type ch)
{
	const value_type chset[2] = { ch, 0 };
	return TrimRight(chset);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::TrimRight(const value_type* sCharSet)
{
	if (!sCharSet || !(*sCharSet) || length() < 1)
		return *this;

	const value_type* last = m_str + length() - 1;
	const value_type* str = last;
	while ((str != m_str) && (CharTraits<T>::_strchr(sCharSet, *str) != 0))
		str--;

	if (str != last)
	{
		// Just shrink length of the string.
		size_type nNewLength = (size_type)(str - m_str) + 1; // m_str can change in _MakeUnique
		_MakeUnique();
		m_nLength = nNewLength;
		m_str[nNewLength] = 0;
	}

	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::TrimRight()
{
	if (length() < 1)
		return *this;

	const value_type* last = m_str + length() - 1;
	const value_type* str = last;
	while ((str != m_str) && (CharTraits<T>::_isspace(*str) != 0))
		str--;

	if (str != last)   // something changed?
	{
		// Just shrink length of the string.
		size_type nNewLength = (size_type)(str - m_str) + 1; // m_str can change in _MakeUnique
		_MakeUnique();
		m_nLength = nNewLength;
		m_str[nNewLength] = 0;
	}

	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::TrimLeft(value_type ch)
{
	const value_type chset[2] = { ch, 0 };
	return TrimLeft(chset);
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::TrimLeft(const value_type* sCharSet)
{
	if (!sCharSet || !(*sCharSet))
		return *this;

	const value_type* str = m_str;
	while ((*str != 0) && (CharTraits<T>::_strchr(sCharSet, *str) != 0))
		str++;

	if (str != m_str)
	{
		size_type nOff = (size_type)(str - m_str); // m_str can change in _MakeUnique
		_MakeUnique();
		size_type nNewLength = length() - nOff;
		CharTraits<T>::_move(m_str, m_str + nOff, nNewLength + 1);
		m_nLength = nNewLength;
		m_str[nNewLength] = 0;
	}

	return *this;
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S>& CryStackStringT<T, S >::TrimLeft()
{
	const value_type* str = m_str;
	while ((*str != 0) && (CharTraits<T>::_isspace(*str) != 0))
		str++;

	if (str != m_str)
	{
		size_type nOff = (size_type)(str - m_str); // m_str can change in _MakeUnique
		_MakeUnique();
		size_type nNewLength = length() - nOff;
		CharTraits<T>::_move(m_str, m_str + nOff, nNewLength + 1);
		m_nLength = nNewLength;
		m_str[nNewLength] = 0;
	}

	return *this;
}

template<class T, size_t S>
inline CryStackStringT<T, S> CryStackStringT<T, S >::Right(size_type count) const
{
	if (count == npos)
		return CryStackStringT<T, S>();
	else if (count > length())
		return *this;

	return CryStackStringT<T, S>(m_str + length() - count, count);
}

template<class T, size_t S>
inline CryStackStringT<T, S> CryStackStringT<T, S >::Left(size_type count) const
{
	if (count == npos)
		return CryStackStringT<T, S>();
	else if (count > length())
		count = length();

	return CryStackStringT<T, S>(m_str, count);
}

//! strspn equivalent.
template<class T, size_t S>
inline CryStackStringT<T, S> CryStackStringT<T, S >::SpanIncluding(const_str charSet) const
{
	assert(_IsValidString(charSet));
	return Left((size_type)this->_strspn(m_str, charSet));
}

//! strcspn equivalent.
template<class T, size_t S>
inline CryStackStringT<T, S> CryStackStringT<T, S >::SpanExcluding(const_str charSet) const
{
	assert(_IsValidString(charSet));
	return Left((size_type)this->_strcspn(m_str, charSet));
}

//////////////////////////////////////////////////////////////////////////
template<class T, size_t S>
inline CryStackStringT<T, S> CryStackStringT<T, S >::Tokenize(const_str charSet, int& nStart) const
{
	if (nStart < 0)
	{
		return CryStackStringT<T, S>();
	}

	if (!charSet)
		return *this;

	const_str sPlace = m_str + nStart;
	const_str sEnd = m_str + length();
	if (sPlace < sEnd)
	{
		int nIncluding = (int)this->_strspn(sPlace, charSet);

		if ((sPlace + nIncluding) < sEnd)
		{
			sPlace += nIncluding;
			int nExcluding = (int)this->_strcspn(sPlace, charSet);
			int nFrom = nStart + nIncluding;
			nStart = nFrom + nExcluding + 1;

			return substr(nFrom, nExcluding);
		}
	}
	// Return empty string.
	nStart = -1;
	return CryStackStringT<T, S>();
}

//////////////////////////////////////////////////////////////////////////
//! Specialization providing efficient move semantics for array classes.
template<class T, size_t S>
bool raw_movable(const CryStackStringT<T, S>& str)
{
	return false;
}

template<class T, size_t S>
void move_init(CryStackStringT<T, S>& dest, CryStackStringT<T, S>& source)
{
	dest.move(source);
}

#if defined(_RELEASE)
	#define ASSERT_LEN  (void)(0)
	#define ASSERT_WLEN (void)(0)
#else
	#define ASSERT_LEN  CRY_ASSERT_TRACE(this->length() <= S, ("String '%s' is %u character(s) longer than MAX_SIZE=%u", this->c_str(), this->length() - S, S))
	#define ASSERT_WLEN CRY_ASSERT_TRACE(this->length() <= S, ("Wide-char string '%ls' is %u character(s) longer than MAX_SIZE=%u", this->c_str(), this->length() - S, S))
#endif

//! Template specialization for char.
template<size_t S> class CryFixedStringT : public CryStackStringT<char, S>
{
public:
	typedef CryStackStringT<char, S> _parentType;
	typedef CryFixedStringT<S>       _Self;
	typedef size_t                   size_type;
	typedef char                     value_type;
	typedef const value_type*        const_str;
	typedef value_type*              pointer;
	typedef const value_type*        const_pointer;
	typedef value_type&              reference;
	typedef const value_type&        const_reference;
	typedef pointer                  iterator;
	typedef const_pointer            const_iterator;
	static const size_type MAX_SIZE = S;
	CryFixedStringT() : _parentType() {}
	CryFixedStringT(const _parentType& str) : _parentType(str) { ASSERT_LEN; }
	CryFixedStringT(const _parentType& str, size_type nOff, size_type nCount) : _parentType(str, nOff, nCount) { ASSERT_LEN; }
	CryFixedStringT(const _Self& str) : _parentType(str) { ASSERT_LEN; }
	CryFixedStringT(const _Self& str, size_type nOff, size_type nCount) : _parentType(str, nOff, nCount) { ASSERT_LEN; }
	CryFixedStringT(size_type nRepeat, value_type ch) : _parentType(nRepeat, ch) { ASSERT_LEN; }
	CryFixedStringT(const_str str) : _parentType(str) { ASSERT_LEN; }
	CryFixedStringT(const_str str, size_type nLength) : _parentType(str, nLength) { ASSERT_LEN; }
	CryFixedStringT(const_iterator _First, const_iterator _Last) : _parentType(_First, _Last) { ASSERT_LEN; }

	template<size_t AnySize> _Self& operator=(const CryFixedStringT<AnySize>& str)
	{
		_parentType::operator=(str);
		ASSERT_LEN;
		return *this;
	}
	template<size_t AnySize> _Self& operator=(const CryStackStringT<char, AnySize>& str)
	{
		_parentType::operator=(str);
		ASSERT_LEN;
		return *this;
	}
	_Self& operator=(value_type ch)
	{
		_parentType::operator=(ch);
		ASSERT_LEN;
		return *this;
	}

	void GetMemoryUsage(class ICrySizer* pSizer) const {}
};

//! \cond INTERNAL
//! Template specialization for a fixed list of CryFixedString.
template<size_t maxElements, size_t stringSize>
class CCryFixedStringListT
{
public:
	CCryFixedStringListT()
	{ Clear(); }

	void Clear()
	{
		m_currentAmount = -1;
		for (int a = 0; a < NUM_MAX_ELEMENTS; ++a)
			m_data[a] = "";
	}

	void Add(const char* name)
	{
		m_data[++m_currentAmount] = name;
		if (m_currentAmount == NUM_MAX_ELEMENTS)
			m_currentAmount = -1;
	}

	const char* operator[](int index)
	{
		if (index <= m_currentAmount)
			return m_data[index].c_str();
		return NULL;
	}

	ILINE int            Size() { return m_currentAmount + 1; }

	CryFixedStringT<32>* GetData(int& amount)
	{
		amount = m_currentAmount + 1;
		return m_data;
	}
private:
	static const int            NUM_MAX_ELEMENTS = maxElements;
	CryFixedStringT<stringSize> m_data[NUM_MAX_ELEMENTS];
	int32                       m_currentAmount;
};
// \endcond

//! A template specialization for wchar_t.
template<size_t S> class CryFixedWStringT : public CryStackStringT<wchar_t, S>
{
public:
	typedef CryStackStringT<wchar_t, S> _parentType;
	typedef CryFixedWStringT<S>         _Self;
	typedef size_t                      size_type;
	typedef wchar_t                     value_type;
	typedef const value_type*           const_str;
	typedef value_type*                 pointer;
	typedef const value_type*           const_pointer;
	typedef value_type&                 reference;
	typedef const value_type&           const_reference;
	typedef pointer                     iterator;
	typedef const_pointer               const_iterator;
	static const size_type MAX_SIZE = S;
	CryFixedWStringT() : _parentType() {}
	CryFixedWStringT(const _parentType& str) : _parentType(str) { ASSERT_WLEN; }
	CryFixedWStringT(const _parentType& str, size_type nOff, size_type nCount) : _parentType(str, nOff, nCount) { ASSERT_WLEN; }
	CryFixedWStringT(const _Self& str) : _parentType(str) { ASSERT_WLEN; }
	CryFixedWStringT(const _Self& str, size_type nOff, size_type nCount) : _parentType(str, nOff, nCount) { ASSERT_WLEN; }
	CryFixedWStringT(size_type nRepeat, value_type ch) : _parentType(nRepeat, ch) { ASSERT_WLEN; }
	CryFixedWStringT(const_str str) : _parentType(str) { ASSERT_WLEN; }
	CryFixedWStringT(const_str str, size_type nLength) : _parentType(str, nLength) { ASSERT_WLEN; }
	CryFixedWStringT(const_iterator _First, const_iterator _Last) : _parentType(_First, _Last) { ASSERT_WLEN; }

	template<size_t AnySize> _Self& operator=(const CryFixedWStringT<AnySize>& str)
	{
		_parentType::operator=(str);
		ASSERT_WLEN;
		return *this;
	}
	template<size_t AnySize> _Self& operator=(const CryStackStringT<wchar_t, AnySize>& str)
	{
		_parentType::operator=(str);
		ASSERT_WLEN;
		return *this;
	}
};
#undef ASSERT_LEN
#undef ASSERT_WLEN
typedef CryStackStringT<char, 512> stack_string;

//! Special string type used for specifying file paths.
typedef CryStackStringT<char, 512> CryPathString;

#if defined(USER_alexl)
struct SUnitTest_FixedString
{
	bool UnitAssert(const char* message, const char* value, const char* refValue)
	{
		int res = strcmp(value, refValue);
		CRY_ASSERT_MESSAGE(res == 0, message);
		return res == 0;
	}

	bool UnitAssert(const char* message, const wchar_t* value, const wchar_t* refValue)
	{
		int res = wcscmp(value, refValue);
		CRY_ASSERT_MESSAGE(res == 0, message);
		return res == 0;
	}

	bool UnitAssert(const char* message, bool cond)
	{
		CRY_ASSERT_MESSAGE(cond, message);
		return cond;
	}

	bool UnitAssert(const char* message, size_t a, size_t b)
	{
		CRY_ASSERT_MESSAGE(a == b, message);
		return a == b;
	}

	int UnitTest()
	{
		CryStackStringT<char, 10> str1;
		CryStackStringT<char, 10> str2;
		CryStackStringT<char, 4> str3;
		CryStackStringT<char, 10> str4;
		CryStackStringT<wchar_t, 16> wstr1;
		CryStackStringT<wchar_t, 255> wstr2;
		CryFixedStringT<100> fixedString100;
		CryFixedStringT<200> fixedString200;

		typedef CryStackStringT<char, 10> T;
		T* pStr = new T;
		* pStr = "adads";
		delete pStr;

		str1 = "abcd";
		UnitAssert("Assignment1-EnoughSpace", str1, "abcd");

		str2 = "efg";
		UnitAssert("Assignment2-EnoughSpace", str2, "efg");

		str2 = str1;
		UnitAssert("Assignment3-EnoughSpace", str2, "abcd");

		str3 = str1;
		UnitAssert("Assignment4-NotEnoughSpace", str3, "abcd");

		str1 += "XY";
		UnitAssert("Concatenate-EnoughSpace", str1, "abcdXY");

		str2 += "efghijk";
		UnitAssert("Concatenate-NotEnoughSpace", str2, "abcdefghijk");

		str1.replace("bc", "");
		UnitAssert("Replace-Shrink-EnoughSpace", str1, "adXY");

		str1.replace("XY", "1234");
		UnitAssert("Replace-Grow-EnoughSpace", str1, "ad1234");

		str1.replace("1234", "1234567890");
		UnitAssert("Replace-Grow-NotEnoughSpace", str1, "ad1234567890");

		str1.reserve(200);
		UnitAssert("Reserve200-SameString", str1, "ad1234567890");
		UnitAssert("Reserve200-Capacity", str1.capacity() == 200);

		str1.reserve(0);
		UnitAssert("Reserve0-SameString", str1, "ad1234567890");
		UnitAssert("Reserve0-Capacity==Length", str1.capacity() == str1.length());

		str1.erase(7); //!< doesn't change capacity
		UnitAssert("Erase-SameString", str1, "ad12345");

		str4.assign("abc");
		UnitAssert("Str4 Assignment", str4, "abc");
		str4.reserve(9);
		UnitAssert("Str4", str4.capacity() >= 9);  //!< capacity is always >= MAX_SIZE-1
		str4.reserve(0);
		UnitAssert("Str4-Shrink", str4.capacity() >= 9);  //!< capacity is always >= MAX_SIZE-1

		size_t idx = str1.find("123");
		UnitAssert("Str1-Find", idx == 2);

		idx = str1.find("123", 3);
		UnitAssert("Str1-Find", idx == str1.npos);

		wstr1 = L"abc";
		UnitAssert("WStr1-Assign", wstr1, L"abc");
		UnitAssert("WStr1-CompareCaseGT", wstr1.compare(L"aBc") > 0);
		UnitAssert("WStr1-CompareCaseLT", wstr1.compare(L"babc") < 0);
		UnitAssert("WStr1-CompareNoCase", wstr1.compareNoCase(L"aBc") == 0);

		str1.Format("This is a %s %S with %d params", "mixed", L"string", 3);
		str2.Format("This is a %S %s with %d params", L"mixed", "string", 3);
		UnitAssert("Str1-Format1", str1, "This is a mixed string with 3 params");
		UnitAssert("Str1-Format2", str1, str2);

		wstr1.Format(L"This is a %s %S with %d params", L"mixed", "string", 3);
		wstr2.Format(L"This is a %S %s with %d params", "mixed", L"string", 3);
		UnitAssert("WStr1-Format1", wstr1, L"This is a mixed string with 3 params");
		UnitAssert("WStr1-Format2", wstr1, wstr2);

		fixedString100 = "01234";
		str2 = fixedString200;
		fixedString200 = fixedString100;
		UnitAssert("FixedString-Test2", fixedString100, "01234");
		UnitAssert("FixedString-Test1", fixedString100, fixedString200);

		CryStackStringT<char, 10> testStr;
		CryFixedStringT<100> testStr2;
		CryFixedWStringT<100> testWStr1;
		string normalString;
		wstring normalWString;
		normalString = string(testStr);
		normalString = string(testStr2);
		normalString.assign(testStr2.c_str());

		// normalWString = testWStr1;  // <- must NOT compile, as we don't allow it!.
		normalWString = wstring(testWStr1);
		return 0;
	}
};
#endif // #if defined(USER_alexl)
