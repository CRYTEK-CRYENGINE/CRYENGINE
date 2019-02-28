// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __simplestring_h__
#define __simplestring_h__
#pragma once

#include <cstring>      // memcpy()

// Implementation note: we cannot use name CSimpleString because it is already used by ATL
class SimpleString
{
public:
	SimpleString() 
		: m_str((char*)0)
		, m_length(0)
	{
	}

	explicit SimpleString(const char* s) 
		: m_str((char*)0)
		, m_length(0)
	{ 
		*this = s;
	}

	SimpleString(const SimpleString& str)
		: m_str((char*)0)
		, m_length(0)
	{
		*this = str;
	}

	~SimpleString() 
	{ 
		if (m_str) 
		{
			delete [] m_str;
		}
	}

	SimpleString& operator=(const SimpleString& str)
	{ 
		if (this != &str)
		{
			*this = str.m_str; 
		}
		return *this;
	}

	SimpleString& operator=(const char* s)
	{
		char* const oldStr = m_str;
		m_str = (char*)0;
		m_length = 0;
		if (s && s[0])
		{
			m_length = strlen(s);
			m_str = new char[m_length + 1];
			memcpy(m_str, s, m_length + 1);
		}
		if (oldStr) 
		{
			delete [] oldStr;
		}
		return *this;
	}

	operator const char*() const
	{ 
		return c_str();
	}

	const char* c_str() const
	{
		return (m_str) ? m_str : ""; 
	}

	size_t length() const
	{
		return m_length;
	}

private:
	char* m_str;
	size_t m_length;
};

#endif
