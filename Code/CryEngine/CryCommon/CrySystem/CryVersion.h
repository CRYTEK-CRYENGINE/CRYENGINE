// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   cryversion.h
//  Version:     v1.00
//  Created:     27/8/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Defines File version structure.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __cryversion_h__
#define __cryversion_h__

//////////////////////////////////////////////////////////////////////////
/** This class keeps file version information.
 */
struct SFileVersion
{
	int v[4];

	SFileVersion()
	{
		v[0] = v[1] = v[2] = v[3] = 0;
	}
	SFileVersion(const int vers[])
	{
		v[0] = vers[0];
		v[1] = vers[1];
		v[2] = vers[2];
		v[3] = 1;
	}

	void Set(const char* s)
	{
		v[0] = v[1] = v[2] = v[3] = 0;

		char t[50];
		const size_t len = (std::min)(strlen(s), sizeof(t) - 1);
		memcpy(t, s, len);
		t[len] = 0;

		char* p;
		if (!(p = strtok(t, "."))) return;
		v[3] = atoi(p);
		if (!(p = strtok(NULL, "."))) return;
		v[2] = atoi(p);
		if (!(p = strtok(NULL, "."))) return;
		v[1] = atoi(p);
		if (!(p = strtok(NULL, "."))) return;
		v[0] = atoi(p);
	}

	explicit SFileVersion(const char* s)
	{
		Set(s);
	}

	bool operator<(const SFileVersion& v2) const
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
	bool operator==(const SFileVersion& v1) const
	{
		if (v[0] == v1.v[0] && v[1] == v1.v[1] &&
		    v[2] == v1.v[2] && v[3] == v1.v[3]) return true;
		return false;
	}
	bool operator>(const SFileVersion& v1) const
	{
		return !(*this < v1);
	}
	bool operator>=(const SFileVersion& v1) const
	{
		return (*this == v1) || (*this > v1);
	}
	bool operator<=(const SFileVersion& v1) const
	{
		return (*this == v1) || (*this < v1);
	}

	int& operator[](int i)       { return v[i]; }
	int  operator[](int i) const { return v[i]; }

	void ToShortString(char* s) const
	{
		sprintf(s, "%d.%d.%d", v[2], v[1], v[0]);
	}

	void ToString(char* s) const
	{
		sprintf(s, "%d.%d.%d.%d", v[3], v[2], v[1], v[0]);
	}
};

#endif // __cryversion_h__
