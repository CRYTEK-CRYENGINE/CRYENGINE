// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   TgaImage.cpp : TGA image file format implementation.

   Revision history:
* Created by Khonich Andrey

   =============================================================================*/

#include "StdAfx.h"

namespace ImageUtils
{
string OutputPath(const char* filename)
{
	string fullPath;
	if (strstr(filename, ":\\") == NULL)
	{
		// construct path with current folder - otherwise path would start at mastercd/game
		char szCurrDir[ICryPak::g_nMaxPath];
		CryGetCurrentDirectory(sizeof(szCurrDir), szCurrDir);

		fullPath = string(szCurrDir) + "/" + filename;
	}
	else
	{
		fullPath = filename;
	}
	return fullPath;
}
}

static FILE* sFileData;

static void bwrite(unsigned char data)
{
	byte d[2];
	d[0] = data;

	gEnv->pCryPak->FWrite(d, 1, 1, sFileData);
}

void wwrite(unsigned short data)
{
	unsigned char h, l;

	l = data & 0xFF;
	h = data >> 8;
	bwrite(l);
	bwrite(h);
}

static void WritePixel(int depth, unsigned long a, unsigned long r, unsigned long g, unsigned long b)
{
	DWORD color16;

	switch (depth)
	{
	case 32:
		bwrite((byte)b);        // b
		bwrite((byte)g);        // g
		bwrite((byte)r);        // r
		bwrite((byte)a);        // a
		break;

	case 24:
		bwrite((byte)b);        // b
		bwrite((byte)g);        // g
		bwrite((byte)r);        // r
		break;

	case 16:
		r >>= 3;
		g >>= 3;
		b >>= 3;

		r &= 0x1F;
		g &= 0x1F;
		b &= 0x1F;

		color16 = (r << 10) | (g << 5) | b;

		wwrite((unsigned short)color16);
		break;
	}
}

static void GetPixel(unsigned char* data, int depth, unsigned long& a, unsigned long& r, unsigned long& g, unsigned long& b)
{
	switch (depth)
	{
	case 32:
		r = *data++;
		g = *data++;
		b = *data++;
		a = *data++;
		break;

	case 24:
		r = *data++;
		g = *data++;
		b = *data++;
		a = 0xFF;
		break;

	default:
		assert(0);
		break;
	}
}

bool WriteTGA(byte* data, int width, int height, const char* filename, int src_bits_per_pixel, int dest_bits_per_pixel)
{
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	return false;
#else
	int i;
	unsigned long r, g, b, a;

	string fullPath = ImageUtils::OutputPath(filename);

	if ((sFileData = gEnv->pCryPak->FOpen(fullPath.c_str(), "wb")) == NULL)
		return false;

	//mdesc |= LR;   // left right
	//m_desc |= UL_TGA_BT;   // top

	int id_length = 0;
	int x_org = 0;
	int y_org = 0;
	int desc = 0x20; // origin is upper left

	// 32 bpp

	int cm_index = 0;
	int cm_length = 0;
	int cm_entry_size = 0;
	int color_map_type = 0;

	int type = 2;

	bwrite(id_length);
	bwrite(color_map_type);
	bwrite(type);

	wwrite(cm_index);
	wwrite(cm_length);

	bwrite(cm_entry_size);

	wwrite(x_org);
	wwrite(y_org);
	wwrite((unsigned short) width);
	wwrite((unsigned short) height);

	bwrite(dest_bits_per_pixel);

	bwrite(desc);

	int hxw = height * width;

	int right = 0;
	//  int top   = 1;

	DWORD* temp_dp = (DWORD*) data;     // data = input pointer

	DWORD* swap = 0;

	UINT src_bytes_per_pixel = src_bits_per_pixel / 8;

	UINT size_in_bytes = hxw * src_bytes_per_pixel;

	if (src_bits_per_pixel == dest_bits_per_pixel)
	{
	#if defined(NEED_ENDIAN_SWAP)
		byte* d = new byte[hxw * 4];
		memcpy(d, data, hxw * 4);
		SwapEndian((uint32*)d, hxw);
		gEnv->pCryPak->FWrite(d, hxw, src_bytes_per_pixel, sFileData);
		SAFE_DELETE_ARRAY(d);
	#else
		gEnv->pCryPak->FWrite(data, hxw, src_bytes_per_pixel, sFileData);
	#endif
	}
	else
	{
		for (i = 0; i < hxw; i++)
		{
			GetPixel(data, src_bits_per_pixel, a, b, g, r);
			WritePixel(dest_bits_per_pixel, a, b, g, r);
			data += src_bytes_per_pixel;
		}
	}

	gEnv->pCryPak->FClose(sFileData);

	SAFE_DELETE_ARRAY(swap);
	return true;
#endif
}
