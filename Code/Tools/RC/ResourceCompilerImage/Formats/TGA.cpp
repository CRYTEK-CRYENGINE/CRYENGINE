// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Util.h"										// Util

namespace ImageTGA
{

///////////////////////////////////////////////////////////////////////////////////

struct TgaFileHeader
{
	uint8  idLength;
	uint8  colorMapType;
	uint8  imageType;
	uint8  colorMapInfo[5];
	uint16 imageOriginX;
	uint16 imageOriginY;
	uint16 imageWidth;
	uint16 imageHeight;
	uint8  pixelBitCount;
	struct
	{
		// FIXME: will these bitfields work with *both* little-endian and big-endian?
		unsigned char alphaBitCount        : 4;
		unsigned char pixelsAreRightToLeft : 1;
		unsigned char pixelsAreTopToBottom : 1;
		unsigned char _padding_            : 2;
	} imageDescription;
	// ubyte id[idLength];
	// ubyte colorMap[someFunction(colorMapInfo)];

	TgaFileHeader()
	{
		memset(&imageDescription, 0, sizeof(imageDescription));
	}

	void setTrueColor24(int wd, int hg) 
	{
		setTrueColor(wd, hg);
		imageDescription.alphaBitCount = 0;
		pixelBitCount = 24;
	}

	void setTrueColor32(int wd, int hg) 
	{
		setTrueColor(wd, hg);
		imageDescription.alphaBitCount = 8;
		pixelBitCount = 32;
	}

	void setBw8(int wd, int hg) 
	{
		idLength = 0;
		colorMapType = 0;
		imageType = 3; // uncompressed black&white
		colorMapInfo[0] = 0;
		colorMapInfo[1] = 0;
		colorMapInfo[2] = 0;
		colorMapInfo[3] = 0;
		colorMapInfo[4] = 0;
		imageOriginX = 0;
		imageOriginY = 0;
		imageWidth = wd;
		imageHeight = hg;
		pixelBitCount = 8;
		imageDescription.alphaBitCount = 0;
		imageDescription.pixelsAreRightToLeft = 0;
		imageDescription.pixelsAreTopToBottom = 1;
		imageDescription._padding_ = 0;
	}

	uint32 getFileOffsetOfPixels() const
	{
		return idLength + sizeof(*this);  // + colorMap[xxx]
	}

	bool isSimple24() const
	{
		return 
			(imageWidth > 0) && 
			(imageHeight > 0) &&
			(colorMapType == 0) &&
			(imageType == 2) &&
			(pixelBitCount == 24) &&
			(imageDescription.pixelsAreRightToLeft == 0);
	}

	bool isSimple32() const
	{
		return 
			(imageWidth > 0) && 
			(imageHeight > 0) &&
			(colorMapType == 0) &&
			(imageType == 2) &&
			(pixelBitCount == 32) &&
			(imageDescription.pixelsAreRightToLeft == 0);
	}

private:
	void setTrueColor(int wd, int hg) 
	{
		idLength = 0;
		colorMapType = 0;
		imageType = 2; // uncompressed true-color
		colorMapInfo[0] = 0;
		colorMapInfo[1] = 0;
		colorMapInfo[2] = 0;
		colorMapInfo[3] = 0;
		colorMapInfo[4] = 0;
		imageOriginX = 0;
		imageOriginY = 0;
		imageWidth = wd;
		imageHeight = hg;
		pixelBitCount = 0;
		imageDescription.alphaBitCount = 0;
		imageDescription.pixelsAreRightToLeft = 0;
		imageDescription.pixelsAreTopToBottom = 1;
		imageDescription._padding_ = 0;
	}
};


void WriteTga(const char* fname, int width, int height, const float* p, int step)
{
	FILE* f = fopen(fname, "wb");

	TgaFileHeader h;
	h.setBw8(width, height);

	fwrite(&h, sizeof(h), 1, f);

	const int pixelCount = width * height;


	float minV, maxV;
	minV = p[0];
	maxV = p[0];
	for (int i = 0; i < pixelCount; ++i)
	{
		if (p[i * step] < minV)
		{
			minV = p[i * step];
		}
		if (p[i * step] > maxV)
		{
			maxV = p[i * step];
		}
	}

	for (int i = 0; i < pixelCount; ++i)
	{
		const float v = p[i * step];

		uint b = 0;

		if (minV >= maxV)
		{
			assert(0);
		}
		else
		{
			const float norm = (v - minV) / (maxV - minV);
			b = Util::getClamped(int(norm * 255.0f), 0, 255);
		}

		fwrite(&b, 1, 1, f);
	}
	fclose(f);
}

///////////////////////////////////////////////////////////////////////////////////

};
