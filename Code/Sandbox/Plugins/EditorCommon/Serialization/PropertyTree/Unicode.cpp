/**
 *  yasli - Serialization Library.
 *  Copyright (C) 2007-2013 Evgeny Andreeshchev <eugene.andreeshchev@gmail.com>
 *                          Alexander Kotliar <alexander.kotliar@gmail.com>
 * 
 *  This code is distributed under the MIT License:
 *                          http://www.opensource.org/licenses/MIT
 */

#include "StdAfx.h"
#include "Unicode.h"

#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#else
# include <vector>
# include <QString>
#endif

namespace property_tree {

yasli::string fromWideChar(const wchar_t* wstr)
{
  // We have different implementation for windows as Qt for windows 
  // is built with wchar_t of diferent size (4 bytes, as on linux).
  // Therefore we avoid calling any wchar_t functions in Qt.
#ifdef _WIN32
#ifdef WW_DISABLE_UTF8
	const unsigned int codepage = CP_ACP;
#else
	const unsigned int codepage = CP_UTF8;
#endif
	int len = WideCharToMultiByte(codepage, 0, wstr, -1, NULL, 0, 0, 0);
	char* buf = (char*)alloca(len);
	if(len > 1){
		WideCharToMultiByte(codepage, 0, wstr, -1, buf, len, 0, 0);
		return yasli::string(buf, len - 1);
	}
	return yasli::string();
#else
    return QString::fromWCharArray(wstr).toUtf8().data();
#endif
}

yasli::wstring toWideChar(const char* str)
{
#ifdef _WIN32
#ifdef WW_DISABLE_UTF8
	const unsigned int codepage = CP_ACP;
#else
	const unsigned int codepage = CP_UTF8;
#endif
    int len = MultiByteToWideChar(codepage, 0, str, -1, NULL, 0);
	wchar_t* buf = (wchar_t*)alloca(len * sizeof(wchar_t));
    if(len > 1){ 
        MultiByteToWideChar(codepage, 0, str, -1, buf, len);
		return yasli::wstring(buf, len - 1);
    }
	return yasli::wstring();
#else
    QString s = QString::fromUtf8(str);

    std::vector<wchar_t> result(s.size()+1);
    s.toWCharArray(&result[0]);
    return &result[0];
#endif
}

}

