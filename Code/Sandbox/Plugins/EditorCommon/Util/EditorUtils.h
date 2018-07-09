// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include <CrySystem/XML/IXml.h>
#include <CrySystem/ISystem.h>

//! Typedef for quaternion.
//typedef CryQuat Quat;

struct IDisplayViewport;

#ifndef MIN
	#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
	#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define LINE_EPS (0.00001f)

template<typename T, size_t N>
char (&ArraySizeHelper(T(&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

/// Some preprocessor utils
/// http://altdevblogaday.com/2011/07/12/abusing-the-c-preprocessor/
#define JOIN(x, y)  JOIN2(x, y)
#define JOIN2(x, y) x ## y

#define LIST_0(x)
#define LIST_1(x)    x ## 1
#define LIST_2(x)    LIST_1(x), x ## 2
#define LIST_3(x)    LIST_2(x), x ## 3
#define LIST_4(x)    LIST_3(x), x ## 4
#define LIST_5(x)    LIST_4(x), x ## 5
#define LIST_6(x)    LIST_5(x), x ## 6
#define LIST_7(x)    LIST_6(x), x ## 7
#define LIST_8(x)    LIST_7(x), x ## 8

#define LIST(cnt, x) JOIN(LIST_, cnt)(x)

//! Checks heap for errors.
struct EDITOR_COMMON_API HeapCheck
{
	//! Runs consistency checks on the heap.
	static void Check(const char* file, int line);
};

#ifdef _DEBUG
	#define HEAP_CHECK HeapCheck::Check(__FILE__, __LINE__);
#else
	#define HEAP_CHECK
#endif

#define MAKE_SURE(x, action) { if (!(x)) { ASSERT(0 && # x); action; } }

namespace EditorUtils
{
// Class to create scoped variable value.
template<typename TType>
class TScopedVariableValue
{
public:
	//Relevant for containers, should not be used manually.
	TScopedVariableValue() :
		m_pVariable(nullptr)
	{

	}

	// Main constructor.
	TScopedVariableValue(TType& tVariable, const TType& tConstructValue, const TType& tDestructValue) :
		m_pVariable(&tVariable),
		m_tConstructValue(tConstructValue),
		m_tDestructValue(tDestructValue)
	{
		*m_pVariable = m_tConstructValue;
	}

	// Transfers ownership.
	TScopedVariableValue(TScopedVariableValue& tInput) :
		m_pVariable(tInput.m_pVariable),
		m_tConstructValue(tInput.m_tConstructValue),
		m_tDestructValue(tInput.m_tDestructValue)
	{
		// I'm not sure if anyone should use this but for now I'm adding one.
		tInput.m_pVariable = nullptr;
	}

	// Move constructor: needed to use CreateScopedVariable, and transfers ownership.
	TScopedVariableValue(TScopedVariableValue&& tInput)
	{
		std::move(m_pVariable, tInput.m_tVariable);
		std::move(m_tConstructValue, tInput.m_tConstructValue);
		std::move(m_tDestructValue, tInput.m_tDestructtValue);
	}

	// Applies the scoping exit, if the variable is valid.
	virtual ~TScopedVariableValue()
	{
		if (m_pVariable)
		{
			*m_pVariable = m_tDestructValue;
		}
	}

	// Transfers ownership, if not self assignment.
	TScopedVariableValue& operator=(TScopedVariableValue& tInput)
	{
		// I'm not sure if this makes sense to exist... but for now I'm adding one.
		if (this != &tInput)
		{
			m_pVariable = tInput.m_pVariable;
			m_tConstructValue = tInput.m_tConstructValue;
			m_tDestructValue = tInput.m_tDestructValue;
			tInput.m_pVariable = nullptr;
		}

		return *this;
	}

protected:
	TType* m_pVariable;
	TType  m_tConstructValue;
	TType  m_tDestructValue;
};

// Helper function to create scoped variable.
// Ideal usage: auto tMyVariable=CreateScoped(tContainedVariable,tConstructValue,tDestructValue);
template<typename TType>
TScopedVariableValue<TType> CreateScopedVariableValue(TType& tVariable, const TType& tConstructValue, const TType& tDestructValue)
{
	return TScopedVariableValue<TType>(tVariable, tConstructValue, tDestructValue);
}

EDITOR_COMMON_API bool OpenHelpPage(const char* szName);
} // EditorUtils

//////////////////////////////////////////////////////////////////////////
// XML Helper functions.
//////////////////////////////////////////////////////////////////////////
namespace XmlHelpers
{
inline XmlNodeRef CreateXmlNode(const char* sTag)
{
	return GetISystem()->CreateXmlNode(sTag);
}

inline bool SaveXmlNode(XmlNodeRef node, const char* filename)
{
	return node->saveToFile(filename);
}

inline XmlNodeRef LoadXmlFromFile(const char* fileName)
{
	return GetISystem()->LoadXmlFromFile(fileName);
}

inline XmlNodeRef LoadXmlFromBuffer(const char* buffer, size_t size)
{
	return GetISystem()->LoadXmlFromBuffer(buffer, size);
}
}

//////////////////////////////////////////////////////////////////////////
// Tokenize string.
//////////////////////////////////////////////////////////////////////////
inline string TokenizeString(const string& str, const char* pszTokens, int& iStart)
{
	assert(iStart >= 0);

	if (pszTokens == NULL)
	{
		return str;
	}

	const char* pszPlace = (const char*)str + iStart;
	const char* pszEnd = (const char*)str + str.GetLength();
	if (pszPlace < pszEnd)
	{
		int nIncluding = (int)strspn(pszPlace, pszTokens);
		;

		if ((pszPlace + nIncluding) < pszEnd)
		{
			pszPlace += nIncluding;
			int nExcluding = (int)strcspn(pszPlace, pszTokens);

			int iFrom = iStart + nIncluding;
			int nUntil = nExcluding;
			iStart = iFrom + nUntil + 1;

			return (str.Mid(iFrom, nUntil));
		}
	}

	// return empty string, done tokenizing
	iStart = -1;
	return "";
}

// This template function will join strings from a vector into a single string, using a separator char
template<class T>
inline void JoinStrings(const std::vector<T>& rStrings, string& rDestStr, char aSeparator = ',')
{
	for (size_t i = 0, iCount = rStrings.size(); i < iCount; ++i)
	{
		rDestStr += rStrings[i];

		if (i < iCount - 1)
		{
			rDestStr += aSeparator;
		}
	}
}

// This function will split a string containing separated strings, into a vector of strings
// better version of TokenizeString
inline void SplitString(const string& rSrcStr, std::vector<string>& rDestStrings, char aSeparator = ',')
{
	int crtPos = 0, lastPos = 0;

	while (true)
	{
		crtPos = rSrcStr.Find(aSeparator, lastPos);

		if (-1 == crtPos)
		{
			crtPos = rSrcStr.GetLength();

			if (crtPos != lastPos)
			{
				rDestStrings.push_back(string((const char*)(rSrcStr.GetString() + lastPos), crtPos - lastPos));
			}

			break;
		}
		else
		{
			if (crtPos != lastPos)
			{
				rDestStrings.push_back(string((const char*)(rSrcStr.GetString() + lastPos), crtPos - lastPos));
			}
		}

		lastPos = crtPos + 1;
	}
}

// This function will split a string containing separated strings, into a vector of strings
// better version of TokenizeString
inline void SplitString(string& rSrcStr, std::vector<string>& rDestStrings, char aSeparator = ',')
{
	int crtPos = 0, lastPos = 0;

	while (true)
	{
		crtPos = rSrcStr.find(aSeparator, lastPos);

		if (-1 == crtPos)
		{
			crtPos = rSrcStr.length();

			if (crtPos != lastPos)
			{
				rDestStrings.push_back(string((const char*)(rSrcStr.c_str() + lastPos), crtPos - lastPos));
			}

			break;
		}
		else
		{
			if (crtPos != lastPos)
			{
				rDestStrings.push_back(string((const char*)(rSrcStr.c_str() + lastPos), crtPos - lastPos));
			}
		}

		lastPos = crtPos + 1;
	}
}

// Format unsigned number to string with 1000s separator
inline string FormatWithThousandsSeperator(const unsigned int number)
{
	string string;

	string.Format("%d", number);

	for (int p = string.GetLength() - 3; p > 0; p -= 3)
		string.Insert(p, ',');

	return string;
}

//! Shortens the argument so that it has at most \p maxCharCountIncludingEllipsis characters.
//! If \p maxCharCountIncludingEllipsis is at least the length of \p szEllipsis the return value
//! ends with \p szEllipsis.
//! Use this function to present a long string in a line edit, for example, when the original value
//! can be obtained in some other way.
//! Examples:
//! ShortenStringWithEllipsis("Lorem ipsum dolor sit amet, consectetur adipiscing elit", 20, "...") = "Lorem ipsum dolor..."
inline string ShortenStringWithEllipsis(string what, size_t maxCharCountIncludingEllipsis = 20, const char* szEllipsis = "...")
{
	const size_t ellipsisLen = strlen(szEllipsis);
	if (what.size() <= maxCharCountIncludingEllipsis)
	{
		return what;
	}
	if (ellipsisLen > maxCharCountIncludingEllipsis)
	{
		return what.substr(0, maxCharCountIncludingEllipsis);
	}
	what.erase(maxCharCountIncludingEllipsis - strlen(szEllipsis));
	return what + szEllipsis;
}

void EDITOR_COMMON_API FormatFloatForUI(string& str, int significantDigits, double value);

//////////////////////////////////////////////////////////////////////////
// Simply sub string searching case insensitive.
//////////////////////////////////////////////////////////////////////////
inline const char* strstri(const char* pString, const char* pSubstring)
{
	int i, j, k;
	for (i = 0; pString[i]; i++)
		for (j = i, k = 0; tolower(pString[j]) == tolower(pSubstring[k]); j++, k++)
			if (!pSubstring[k + 1])
				return (pString + i);

	return NULL;
}

//! Parses a null-terminated string of properties and calls the user provided function for each property.
//! \param szProperties Pointer to the null-terminated string of properties to be parsed: [propery_name[ = value]] [delimeter(s) [propery_name[ = value]]] etc...
//! \param szDelim Pointer to the null-terminated string of properties delimiters. eg ";\n\r"
//! \param fn Function object, to be applied to the each key/value pairs. pKey and pValue are not null-terminated and have lengths keyLength and valueLength, respectively.
EDITOR_COMMON_API void ParsePropertyString(
  const char* szProperties,
  const char* szDelim,
  const std::function<void(const char* pKey, size_t keyLength, const char* pValue, size_t valueLength)>& fn);

EDITOR_COMMON_API Vec3 ConvertToTextPos(const Vec3& pos, const Matrix34& tm, IDisplayViewport* view, bool bDisplay2D);

class CArchive;

EDITOR_COMMON_API CArchive& operator<<(CArchive& ar, const string& str); // for CString conversion
EDITOR_COMMON_API CArchive& operator>>(CArchive& ar, string& str);       // for CString conversion

