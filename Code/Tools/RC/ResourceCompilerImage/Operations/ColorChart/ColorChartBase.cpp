// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "../../ImageObject.h"

#include "ColorChartBase.h"

//////////////////////////////////////////////////////////////////////////
// helper

static bool CheckImageFormat(const ImageObject* pImg)
{
	const EPixelFormat format = pImg->GetPixelFormat();
	return format == ePixelFormat_A8R8G8B8 || format == ePixelFormat_X8R8G8B8; // || format == ePixelFormat_R8G8B8;
}


namespace
{
struct Color
{
private:
	int c[3];

public:
	Color(uint32 x, uint32 y, void* pPixels, uint32 pitch)
	{
		const uint8* p = (const uint8*)pPixels + pitch * y + x * 4;
		c[0] = p[0];
		c[1] = p[1];
		c[2] = p[2];
	}

	bool isSimilar(const Color& a, int maxDiff) const
	{
		return
			abs(a.c[0] - c[0]) <= maxDiff &&
			abs(a.c[1] - c[1]) <= maxDiff &&
			abs(a.c[2] - c[2]) <= maxDiff;
	}
};
}

static bool ScanColorChartAt(uint32 x, uint32 y, void* pData, uint32 pitch)
{
	const Color colorRef[2] =
	{
		Color(x, y, pData, pitch),
		Color(x + 2, y, pData, pitch)
	};

	// We require two colors of the border to be at least a bit different
	if (colorRef[0].isSimilar(colorRef[1], 15))
	{
		return false;
	}

	static const int kMaxDiff = 3;

	static_assert((CHART_IMAGE_WIDTH % 2) == 0, "Invalid image width!");
	static_assert((CHART_IMAGE_HEIGHT % 2) == 0, "Invalid image height!");

	int refIdx = 0;
	for (int i = 0; i < CHART_IMAGE_WIDTH; i += 2)
	{
		if (!colorRef[refIdx].isSimilar(Color(x + i,     y, pData, pitch), kMaxDiff) ||
			!colorRef[refIdx].isSimilar(Color(x + i + 1, y, pData, pitch), kMaxDiff))
		{
			return false;
		}
		refIdx ^= 1;
	}

	refIdx = 0;
	for (int i = 0; i < CHART_IMAGE_HEIGHT; i += 2)
	{
		if (!colorRef[refIdx].isSimilar(Color(x, y + i,     pData, pitch), kMaxDiff) ||
			!colorRef[refIdx].isSimilar(Color(x, y + i + 1, pData, pitch), kMaxDiff))
		{
			return false;
		}
		refIdx ^= 1;
	}

	refIdx = 0;
	for (int i = 0; i < CHART_IMAGE_HEIGHT; i += 2)
	{
		if (!colorRef[refIdx].isSimilar(Color(x + CHART_IMAGE_WIDTH - 1, y + i,     pData, pitch), kMaxDiff) ||
			!colorRef[refIdx].isSimilar(Color(x + CHART_IMAGE_WIDTH - 1, y + i + 1, pData, pitch), kMaxDiff))
		{
			return false;
		}
		refIdx ^= 1;
	}

	refIdx = 0;
	for (int i = 0; i < CHART_IMAGE_WIDTH; i += 2)
	{
		if (!colorRef[refIdx].isSimilar(Color(x + i,     y + CHART_IMAGE_HEIGHT - 1, pData, pitch), kMaxDiff) ||
			!colorRef[refIdx].isSimilar(Color(x + i + 1, y + CHART_IMAGE_HEIGHT - 1, pData, pitch), kMaxDiff))
		{
			return false;
		}
		refIdx ^= 1;
	}

	return true;
}


static int ScanColorChart(uint32 width, uint32 height, void* pData, uint32 pitch, uint32& resx, uint32& resy)
{
	bool bFound = false;;

	for (uint32 y = 0; y <= height - CHART_IMAGE_HEIGHT; ++y)
	{
		for (uint32 x = 0; x <= width - CHART_IMAGE_WIDTH; ++x)
		{
			if (ScanColorChartAt(x, y, pData, pitch))
			{
				if (bFound)
				{
					return 2;
				}
				bFound = true;
				resx = x;
				resy = y;
				x += CHART_IMAGE_WIDTH - 1;
			}
		}
	}

	return bFound ? 1 : 0;
}


static int FindColorChart(const ImageObject* pImg, uint32& x, uint32& y)
{
	const uint32 w = pImg->GetWidth(0);
	const uint32 h = pImg->GetHeight(0);

	if (w < CHART_IMAGE_WIDTH || h < CHART_IMAGE_HEIGHT)
	{
		return 0;
	}

	char* pData;
	uint32 pitch;
	pImg->GetImagePointer(0, pData, pitch);

	return ScanColorChart(w, h, pData, pitch, x, y);
}

//////////////////////////////////////////////////////////////////////////
// CColorChartBase impl

CColorChartBase::CColorChartBase()
{
}


CColorChartBase::~CColorChartBase()
{
}


bool CColorChartBase::GenerateFromInput(ImageObject* pImg)
{
	if (!pImg)
	{
		return false;
	}

	if (!CheckImageFormat(pImg))
	{
		RCLogError("Cannot generate color chart from input image. Input image format unsupported.");
		return false;
	}

	if (!ExtractFromImage(pImg))
	{
		return false;
	}

	return true;
}


bool CColorChartBase::ExtractFromImage(ImageObject* pImg)
{
	assert(pImg);
	assert(CheckImageFormat(pImg));

	uint32 x = 0;
	uint32 y = 0;

	const int count = FindColorChart(pImg, x, y);
	if (count != 1)
	{
		RCLogError("Cannot extract color chart from input image: %s",
			(count == 0 ? "the color chart wasn't detected." : "there is more than one color chart included."));
		return false;
	}

	return ExtractFromImageAt(pImg, x, y);
}
