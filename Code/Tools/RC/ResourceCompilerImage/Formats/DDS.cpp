// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"

#include "../ImageObject.h"  // ImageToProcess

///////////////////////////////////////////////////////////////////////////////////

namespace ImageDDS
{

ImageObject* LoadByUsingDDSLoader(const char* filenameRead, CImageProperties* pProps, string& res_specialInstructions)
{
	res_specialInstructions.clear();

	std::auto_ptr<ImageObject> pRet;

	{
		std::auto_ptr<ImageObject> pLoad(new ImageObject(128, 128, 1, ePixelFormat_A32B32G32R32F, ImageObject::eCubemap_UnknownYet));
		if (pLoad->LoadImage(filenameRead, false))
		{
			pRet.reset(pLoad.release());
		}
	}

	return pRet.release();
}

bool SaveByUsingDDSSaver(const char* filenameWrite, const CImageProperties* pProps, const ImageObject* pImageObject)
{
	return pImageObject->SaveImage(filenameWrite, false);
}

}
