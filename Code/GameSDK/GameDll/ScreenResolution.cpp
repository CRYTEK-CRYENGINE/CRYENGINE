// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ScreenResolution.h"
#include <vector>

namespace
{
  std::vector<ScreenResolution::SScreenResolution> s_ScreenResolutions;
}

namespace ScreenResolution
{
  //////////////////////////////////////////////////////////////////////////
  void InitialiseScreenResolutions()
  {
    #if CRY_PLATFORM_DESKTOP
		
		CryFixedStringT<16> format;

		SDispFormat *formats = NULL;
		int numFormats = gEnv->pRenderer->EnumDisplayFormats(NULL);
		if(numFormats)
		{
			formats = new SDispFormat[numFormats];
			gEnv->pRenderer->EnumDisplayFormats(formats);
		}

		int lastWidth, lastHeight;
		lastHeight = lastWidth = -1;

		for(int i = 0; i < numFormats; ++i)
		{

			if(HasResolution(formats[i].m_Width, formats[i].m_Height))
			{
				continue;
			}

			if(formats[i].m_Width < 800)
				continue;

			format.Format("%i X %i", formats[i].m_Width, formats[i].m_Height);

			SScreenResolution resolution(formats[i].m_Width, formats[i].m_Height, formats[i].m_BPP, format.c_str());
			s_ScreenResolutions.push_back(resolution);

			lastHeight = formats[i].m_Height;
			lastWidth = formats[i].m_Width;
		}

		if(formats)
			delete[] formats;

    #endif
  }
  //////////////////////////////////////////////////////////////////////////
  void ReleaseScreenResolutions()
  {
    s_ScreenResolutions.clear();
  }
	//////////////////////////////////////////////////////////////////////////
	bool GetScreenResolutionAtIndex(unsigned int nIndex, SScreenResolution& resolution)
	{
		assert(!(nIndex >= s_ScreenResolutions.size()));

		if (nIndex < s_ScreenResolutions.size())
		{
			resolution = s_ScreenResolutions.at(nIndex);
			return true;
		}
		return false;
	}
	//////////////////////////////////////////////////////////////////////////
	bool HasResolution(const int width, const int height)
	{
		const uint32 size = GetNumScreenResolutions();
		for(uint32 i=0; i<size; ++i)
		{
			if(s_ScreenResolutions[i].iWidth == width && s_ScreenResolutions[i].iHeight == height)
			{
				return true;
			}
		}
		return false;
	}
	//////////////////////////////////////////////////////////////////////////
	int GetNearestResolution(const int width, const int height)
	{
		const uint32 size = GetNumScreenResolutions();
		float minDifference = 1.0f;
		int nearestIndex = -1;
		for(uint32 i=0; i<size; ++i)
		{

			if(s_ScreenResolutions[i].iWidth == width && s_ScreenResolutions[i].iHeight == height)
			{
				return (int)i;
			}

			float difference = 0.0f;

			difference += abs(1.0f - (float)s_ScreenResolutions[i].iWidth / (float)width);
			difference += abs(1.0f - (float)s_ScreenResolutions[i].iHeight / (float)height);

			if(difference < minDifference)
			{
				nearestIndex = (int)i;
				minDifference = difference;
			}
		}

		return nearestIndex;
	}
  //////////////////////////////////////////////////////////////////////////
  unsigned int GetNumScreenResolutions()
  {
    return s_ScreenResolutions.size();
  }
}
