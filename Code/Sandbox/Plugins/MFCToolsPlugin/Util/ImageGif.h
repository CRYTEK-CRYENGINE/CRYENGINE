// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __imagegif_h__
#define __imagegif_h__

#if _MSC_VER > 1000
	#pragma once
#endif

class CImageGif
{
public:
	bool Load(const string& fileName, CImageEx& outImage);
};

#endif // __imagegif_h__

