// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace ScreenResolution
{
  struct SScreenResolution
  {
    int iWidth;
    int iHeight;
    unsigned int nDephtPerPixel;
    string sResolution;

    SScreenResolution(unsigned int _iWidth, unsigned int _iHeight, unsigned int _nDepthPerPixel, const char* _sResolution):
        iWidth(_iWidth)
      , iHeight(_iHeight)
      , nDephtPerPixel(_nDepthPerPixel)
      , sResolution(_sResolution)
    {}

    SScreenResolution()
    {}
  };

  void InitialiseScreenResolutions();
  void ReleaseScreenResolutions();

  bool GetScreenResolutionAtIndex(unsigned int nIndex, SScreenResolution& resolution);
	bool HasResolution(const int width, const int height);
	int GetNearestResolution(const int width, const int height);
  unsigned int GetNumScreenResolutions();
}