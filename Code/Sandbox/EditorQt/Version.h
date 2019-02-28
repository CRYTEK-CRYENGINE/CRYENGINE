// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/StringUtils.h>

//////////////////////////////////////////////////////////////////////////
/** This class keeps file version information.
*/
struct Version
{
	int v[4];

	Version()
	{
		v[0] = v[1] = v[2] = v[3] = 0;
	}
	Version(const int vers[])
	{
		v[0] = vers[0];
		v[1] = vers[1];
		v[2] = vers[2];
		v[3] = 1;
	}

	explicit Version(const char* s)
	{
		v[0] = v[1] = v[2] = v[3] = 0;

		char t[50];
		char* p;
		cry_strcpy(t, s);
		if (!(p = strtok(t, "."))) return;
		v[3] = atoi(p);
		if (!(p = strtok(NULL, "."))) return;
		v[2] = atoi(p);
		if (!(p = strtok(NULL, "."))) return;
		v[1] = atoi(p);
		if (!(p = strtok(NULL, "."))) return;
		v[0] = atoi(p);
	}

	bool operator<(const Version& v2) const
	{
		if (v[3] < v2.v[3]) return true;
		if (v[3] > v2.v[3]) return false;

		if (v[2] < v2.v[2]) return true;
		if (v[2] > v2.v[2]) return false;

		if (v[1] < v2.v[1]) return true;
		if (v[1] > v2.v[1]) return false;

		if (v[0] < v2.v[0]) return true;
		if (v[0] > v2.v[0]) return false;
		return false;
	}
	bool operator==(const Version& v1) const
	{
		if (v[0] == v1.v[0] && v[1] == v1.v[1] &&
			v[2] == v1.v[2] && v[3] == v1.v[3]) return true;
		return false;
	}
	bool operator>(const Version& v1) const
	{
		return !(*this < v1);
	}
	bool operator>=(const Version& v1) const
	{
		return (*this == v1) || (*this > v1);
	}
	bool operator<=(const Version& v1) const
	{
		return (*this == v1) || (*this < v1);
	}

	int&    operator[](int i) { return v[i]; }
	int     operator[](int i) const { return v[i]; }

	string ToString() const
	{
		char s[1024];
		cry_sprintf(s, "%d.%d.%d", v[2], v[1], v[0]);
		return s;
	}

	string ToFullString() const
	{
		char s[1024];
		cry_sprintf(s, "%d.%d.%d.%d", v[3], v[2], v[1], v[0]);
		return s;
	}
};


