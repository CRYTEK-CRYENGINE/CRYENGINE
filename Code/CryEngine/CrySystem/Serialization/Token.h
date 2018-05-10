// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <string.h>

#include "Serialization/Strings.h"

namespace Serialization{

struct Token
{
	Token(const char* _str = 0)
		: start(_str)
		, end(_str ? _str + strlen(_str) : 0)
	{
	}

	Token(const char* _str, size_t _len) : start(_str), end(_str + _len) {}
	Token(const char* _start, const char* _end) : start(_start), end(_end) {}

	void        set(const char* _start, const char* _end) { start = _start; end = _end; }
	std::size_t length() const                            { return end - start; }

	bool        operator==(const Token& rhs) const
	{
		if (length() != rhs.length())
			return false;
		return memcmp(start, rhs.start, length()) == 0;
	}
	bool operator==(const string& rhs) const
	{
		if (length() != rhs.size())
			return false;
		return memcmp(start, rhs.c_str(), length()) == 0;
	}

	bool operator==(const char* text) const
	{
		if (strncmp(text, start, length()) == 0)
			return text[length()] == '\0';
		return false;
	}
	bool operator!=(const char* text) const
	{
		if (strncmp(text, start, length()) == 0)
			return text[length()] != '\0';
		return true;
	}
	bool operator==(char c) const
	{
		return length() == 1 && *start == c;
	}
	bool operator!=(char c) const
	{
		return length() != 1 || *start != c;
	}

	operator bool() const { return start != end; }
	string str() const { return string(start, end); }

	const char* start;
	const char* end;
};

}
