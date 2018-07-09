// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

///////////////////////////////////////////////////////////////////////////////////
// Image i/o using libtiff library

#include <vector>

#include <assert.h>
#include <CryString/CryString.h>

class CImageProperties;
class ImageObject;

namespace ImageTIFF
{
	bool UpdateAndSaveSettingsToTIF(const char* filenameRead, const char* filenameWrite, const string& settings, bool isCryTif2012);
	bool UpdateAndSaveSettingsToTIF(const char* filenameRead, const char* filenameWrite, const CImageProperties* pProps, const string* pOriginalSettings, bool bLogSettings);

	string LoadSpecialInstructionsByUsingTIFFLoader(const char* filenameRead);

	ImageObject* LoadByUsingTIFFLoader(const char* filenameRead, CImageProperties* pProps, string& res_specialInstructions);

	bool SaveByUsingTIFFSaver(const char* filenameWrite, const CImageProperties* pProps, const ImageObject* pImageObject);
};
