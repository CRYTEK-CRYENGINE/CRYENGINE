// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Util/EditorUtils.h"
#include "IDisplayViewport.h"
#include <malloc.h>
#include "Controls/QuestionDialog.h"
#include "FileDialogs/SystemFileDialog.h"

#include <QDesktopServices>
#include <QJsonDocument>

#include <cctype>  // std::isspace
#include <cstring> // std::strspn

namespace EditorUtils
{
EDITOR_COMMON_API bool OpenHelpPage(const char* szName)
{
	char szEngineRootDir[_MAX_PATH];
	CryFindEngineRootFolder(CRY_ARRAY_COUNT(szEngineRootDir), szEngineRootDir);

	QString fileName = QString("%1/Editor/OnlineDocumentation.json").arg(szEngineRootDir);
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Unable to open %s", fileName.toStdString().c_str());
		return false;
	}

	QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));
	const QVariantMap variantMap = doc.toVariant().toMap();
	const QVariant entry = variantMap[szName];
	const QUrl url = QUrl(entry.isValid() ? entry.toString() : variantMap["default"].toString());
	QDesktopServices::openUrl(url);

	return true;
}
}

CArchive& operator<<(CArchive& ar, const string& str)
{
	CString cstr(str.GetString());
	ar << cstr;
	/*
	   AfxWriteStringLength(ar, str.GetLength(), false);
	   ar.Write((const void*)str.GetString(), str.GetLength());
	 */
	return ar;
}

CArchive& operator>>(CArchive& ar, string& str)
{
	/*
	   int nCharSize;  // 1 = char, 2 = wchar_t
	   UINT nLength = UINT(AfxReadStringLength(ar, nCharSize));
	   str.Empty();
	   str.insert(0, nLength + 1, '\0');

	   UINT nBytesRead = ar.Read(const_cast<char*>(str.GetBuffer()), nLength);
	   ASSERT(nBytesRead == nLength);
	 */

	CString cstr;
	ar >> cstr;
	str = cstr.GetString();
	return ar;
}

//////////////////////////////////////////////////////////////////////////
void HeapCheck::Check(const char* file, int line)
{
#ifdef _DEBUG

	_ASSERTE(_CrtCheckMemory());

	/*
	   int heapstatus = _heapchk();
	   switch( heapstatus )
	   {
	   case _HEAPOK:
	    break;
	   case _HEAPEMPTY:
	    break;
	   case _HEAPBADBEGIN:
	    {
	      string str;
	      str.Format( "Bad Start of Heap, at file %s line:%d",file,line );
	      MessageBox( NULL,str,"Heap Check",MB_OK );
	    }
	    break;
	   case _HEAPBADNODE:
	    {
	      string str;
	      str.Format( "Bad Node in Heap, at file %s line:%d",file,line );
	      MessageBox( NULL,str,"Heap Check",MB_OK );
	    }
	    break;
	   }
	 */
#endif
}

//////////////////////////////////////////////////////////////////////////-
static int TrimTrailingZeros(char* pBuf)
{
	for (int pos = strlen(pBuf) - 1; pos >= 0; --pos)
	{
		if (pBuf[pos] == '0')
			pBuf[pos] = '\0';
		else
			return pos + 1;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// This function is supposed to format float in user-friendly way,
// omitting the exponent notation.
//
// Why not using printf? Its formatting rules has following drawbacks:
//  %g   - will use exponent for small numbers;
//  %.Nf - doesn't allow to control total amount of significant numbers,
//         which exposes limited precision during binary-to-decimal fraction
//         conversion.
//////////////////////////////////////////////////////////////////////////
void FormatFloatForUI(string& str, int significantDigits, double value)
{
	str.Empty();
	str.Preallocate(significantDigits + 3);

	int point = 0;
	int sign = 0;
	char buf[_CVTBUFSIZE + 1] = { 0 };
	if (value > -1.0 && value < 1.0)
		_fcvt_s(buf, value, significantDigits, &point, &sign);
	else
		_ecvt_s(buf, value, significantDigits, &point, &sign);

	if (sign != 0)
		str.Append("-");
	if (point <= 0)
	{
		if (TrimTrailingZeros(buf) > 0)
		{
			str.Append("0.");
			while (point < 0)
			{
				str.Append("0");
				++point;
			}
			str.Append(buf);
		}
		else
			str.Append("0");
	}
	else
	{
		int len = TrimTrailingZeros(buf);
		if (point < len)
		{
			str.Append(buf, point);
			str.Append(".");
			str.Append(buf + point);
		}
		else
		{
			str.Append(buf);
			point -= len;
			while (point > 0)
			{
				str.Append("0");
				--point;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////-
Vec3 ConvertToTextPos(const Vec3& pos, const Matrix34& tm, IDisplayViewport* view, bool bDisplay2D)
{
	if (bDisplay2D)
	{
		Vec3 world_pos = tm.TransformPoint(pos);
		int width = 0;
		int height = 0;
		view->GetDimensions(&width, &height);

		POINT screen_pos = view->WorldToView(world_pos);

		IDisplayViewport::EAxis axis = IDisplayViewport::AXIS_NONE;
		bool b2D;
		view->GetPerpendicularAxis(&axis, &b2D);

		return Vec3((float)screen_pos.x, (float)screen_pos.y, 0);
	}
	else
	{
		return tm.TransformPoint(pos);
	}
}

//////////////////////////////////////////////////////////////////////////
void ParsePropertyString(const char* szProperties, const char* szDelim, const std::function<void(const char* pKey, size_t keyLength, const char* pValue, size_t valueLength)>& fn)
{
	auto trimLeft = [](const char* pBegin, const char* pEnd) -> const char*
	{
		while (pBegin < pEnd && std::isspace(static_cast<unsigned char>(*pBegin)))
		{
			++pBegin;
		}
		return pBegin;
	};

	auto trimRight = [](const char* pBegin, const char* pEnd) -> const char*
	{
		while (pBegin < pEnd && std::isspace(static_cast<unsigned char>(*(pEnd - 1))))
		{
			--pEnd;
		}
		return pEnd;
	};

	for (const char* pBegin = szProperties, * pEnd = szProperties; *pEnd; )
	{
		pEnd += std::strcspn(pEnd, szDelim);

		// parse line
		const char* pKey = trimLeft(pBegin, pEnd);
		const char* pValue = pKey;
		while (pValue < pEnd && *pValue != '=')
		{
			++pValue;
		}

		const char* pKeyEnd = trimRight(pKey, pValue);
		if (pKey < pKeyEnd)
		{
			if (pValue < pEnd)
			{
				pValue = trimLeft(pValue + 1, pEnd);
				const char* pValueEnd = trimRight(pValue, pEnd);
				if (pValue < pValueEnd)
				{
					fn(pKey, pKeyEnd - pKey, pValue, pValueEnd - pValue);
				}
			}
			else // a flag that has no value
			{
				fn(pKey, pKeyEnd - pKey, nullptr, 0);
			}
		}

		pEnd += std::strspn(pEnd, szDelim);
		pBegin = pEnd;
	}
};

