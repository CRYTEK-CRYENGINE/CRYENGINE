// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "ImageProperties.h"


static const char* s_mipDownsizingMethodNames[][2] =
{
	{ "average", "1" },	   // "1" obsolete setting, to keep compatibility with old files
	{ "nearest", "" },	   
	{ "gauss", "0" },	   // "0" obsolete setting, to keep compatibility with old files
	{ "kaiser", "" },
	{ "lanczos", "" }	
};


inline int CImageProperties::GetMipDownsizingMethodDefault()
{
	return 2;
}


inline int CImageProperties::GetMipDownsizingMethodCount()
{
	return sizeof(s_mipDownsizingMethodNames) / sizeof(s_mipDownsizingMethodNames[0]);
}


const char* CImageProperties::GetMipDownsizingMethodName(int idx)
{
	if (idx < 0 || idx >= GetMipDownsizingMethodCount())
	{
		idx = GetMipDownsizingMethodDefault();
	}
	return s_mipDownsizingMethodNames[idx][0];
}


const int CImageProperties::GetMipDownsizingMethodIndex(int platform) const
{
	if (platform < 0)
	{
		platform = m_pCC->platform;
	}

	const int defaultIndex = GetMipDownsizingMethodDefault();
	const char* const pDefaultName = GetMipDownsizingMethodName(defaultIndex);

	const string name = m_pCC->multiConfig->getConfig(platform).GetAsString("mipgentype", pDefaultName, pDefaultName);

	const int n = GetMipDownsizingMethodCount();
	for (int i = 0; i < n; ++i)
	{	
		const char* const p0 = s_mipDownsizingMethodNames[i][0];
		const char* const p1 = s_mipDownsizingMethodNames[i][1];
		if (stricmp(name.c_str(), p0) == 0 || stricmp(name.c_str(), p1) == 0)
		{
			return i;
		}
	}

	RCLogWarning("Unknown downsizing method: '%s'. Using '%s'.", name.c_str(), s_mipDownsizingMethodNames[defaultIndex][0]);

	return defaultIndex;
}

// eof
