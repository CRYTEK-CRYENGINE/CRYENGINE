// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IConfig.h"
#include "IRCLog.h"


bool IConfig::HasKey(const char* const key, const int ePriMask) const
{
	const char* pValue;
	return GetKeyValue(key, pValue, ePriMask);
}


IConfig::EResult IConfig::Get(const char* const key, bool& value, const int ePriMask) const 
{
	const char* pValue;
	if (!GetKeyValue(key, pValue, ePriMask))
	{
		return eResult_KeyIsNotFound;
	}

	{
		int tmpInt;
		if (sscanf(pValue, "%d", &tmpInt) == 1)
		{
			value = (tmpInt != 0);
			return eResult_Success;
		}
	}

	if (!stricmp(pValue, "true") ||
		!stricmp(pValue, "yes") ||
		!stricmp(pValue, "enable") ||
		!stricmp(pValue, "y") ||
		!stricmp(pValue, "t"))
	{
		value = true;
		return eResult_Success;
	}

	if (!stricmp(pValue, "false") ||
		!stricmp(pValue, "no") ||
		!stricmp(pValue, "disable") ||
		!stricmp(pValue, "n") ||
		!stricmp(pValue, "f"))
	{
		value = false;
		return eResult_Success;
	}

	return eResult_ValueIsEmptyOrBad;
}

IConfig::EResult IConfig::Get(const char* const key, int& value, const int ePriMask) const 
{
	const char* pValue;
	if (!GetKeyValue(key, pValue, ePriMask))
	{
		return eResult_KeyIsNotFound;
	}

	int tmpValue;
	if (sscanf(pValue, "%d", &tmpValue) == 1)
	{
		value = tmpValue;
		return eResult_Success;
	}

	return eResult_ValueIsEmptyOrBad;	
}

IConfig::EResult IConfig::Get(const char* const key, float& value, const int ePriMask) const
{
	const char* pValue;
	if (!GetKeyValue(key, pValue, ePriMask))
	{
		return eResult_KeyIsNotFound;
	}

	float tmpValue;
	if (sscanf(pValue, "%f", &tmpValue) == 1)
	{
		value = tmpValue;
		return eResult_Success;
	}

	return eResult_ValueIsEmptyOrBad;	
}

IConfig::EResult IConfig::Get(const char* const key, string& value, const int ePriMask) const
{
	const char* pValue;
	if (!GetKeyValue(key, pValue, ePriMask))
	{
		return eResult_KeyIsNotFound;
	}

	if (pValue && pValue[0])
	{
		value = pValue;
		return eResult_Success;
	}

	return eResult_ValueIsEmptyOrBad;
}


bool IConfig::GetAsBool(const char* const key, const bool keyIsNotFoundValue, const bool emptyOrBadValue, const int ePriMask) const
{
	bool value;
	switch (Get(key, value, ePriMask))
	{
	case eResult_Success:
		return value;
	case eResult_KeyIsNotFound:
		return keyIsNotFoundValue;
	default:
		return emptyOrBadValue;
	}
}

int IConfig::GetAsInt(const char* const key, const int keyIsNotFoundValue, const int emptyOrBadValue, const int ePriMask) const
{
	int value;
	switch (Get(key, value, ePriMask))
	{
	case eResult_Success:
		return value;
	case eResult_KeyIsNotFound:
		return keyIsNotFoundValue;
	default:
		return emptyOrBadValue;
	}
}

float IConfig::GetAsFloat(const char* const key, const float keyIsNotFoundValue, const float emptyOrBadValue, const int ePriMask) const
{
	float value;
	switch (Get(key, value, ePriMask))
	{
	case eResult_Success:
		return value;
	case eResult_KeyIsNotFound:
		return keyIsNotFoundValue;
	default:
		return emptyOrBadValue;
	}
}

string IConfig::GetAsString(const char* const key, const char* const keyIsNotFoundValue, const char* const emptyOrBadValue, const int ePriMask) const
{
	string value;
	switch (Get(key, value, ePriMask))
	{
	case eResult_Success:
		return value;
	case eResult_KeyIsNotFound:
		return string(keyIsNotFoundValue);
	default:
		return string(emptyOrBadValue);
	}
}


static inline void SkipWhitespace(const char* &p)
{
	while (*p && *p <= ' ')
	{
		++p;
	}
}

void IConfig::SetFromString(const EConfigPriority ePri, const char* p)
{
	assert(Util::isPowerOfTwo(ePri));

	if (p == 0)
	{
		return;
	}

	while (*p)
	{
		string sKey;
		string sValue;

		SkipWhitespace(p);

		if (*p != '/')
		{
			RCLog("Config string format is invalid ('/' expected): '%s'", p);
			break;
		}
		++p;   // jump over '/'

		while (IsValidNameChar(*p))
		{
			sKey += *p++;
		}

		SkipWhitespace(p);

		if (*p != '=')
		{
			RCLog("Config string format is invalid ('=' expected): '%s'", p);
			break;
		}
		++p;   // jump over '='

		SkipWhitespace(p);

		if (*p == '\"')   // in quotes
		{
			++p;
			while (*p && *p != '\"')   // value
			{
				sValue += *p++;
			}
			if (*p == '\"')
			{
				++p;
			}
		}
		else   // without quotes
		{
			while (IsValidNameChar(*p))   // value
			{
				sValue += *p++;
			}
		}

		SkipWhitespace(p);

		sKey.Trim();
		sValue.Trim();

		SetKeyValue(ePri, sKey, sValue);
	}
}

