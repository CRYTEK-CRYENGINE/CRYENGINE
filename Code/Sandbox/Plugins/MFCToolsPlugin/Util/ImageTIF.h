// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __imagetif_h__
#define __imagetif_h__

#if _MSC_VER > 1000
	#pragma once
#endif

class PLUGIN_API CImageTIF
{
public:
	bool               Load(const string& fileName, CImageEx& outImage);
	bool               SaveRAW(const string& fileName, const void* pData, int width, int height, int bytesPerChannel, int numChannels, bool bFloat, const char* preset);
	static const char* GetPreset(const string& fileName);
};

#endif // __imagetif_h__

