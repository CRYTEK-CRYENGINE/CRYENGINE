// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef CRY_PLATFORM_WINDOWS
#include <png.h>

namespace Private_WritePNG
{
class AutoCloseFile
{
	FILE *m_fp;
public:
	AutoCloseFile(FILE* fp)
		: m_fp(fp)
	{
	}

	~AutoCloseFile()
	{
		if (m_fp)
		{
			fclose(m_fp);
		}
	}
};
}
#endif

bool WritePNG(byte* data, int width, int height, const char* file_name)
{
#ifdef CRY_PLATFORM_WINDOWS
	FILE *fp = fopen(file_name, "wb+");
	if (!fp)
	{
		CryLog("[write_png_file] File %s could not be opened for writing", file_name);
		return false;
	}

	Private_WritePNG::AutoCloseFile auto_close(fp);
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr)
	{
		CryLog("[write_png_file] png_create_write_struct failed");
		return false;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		CryLog("[write_png_file] png_create_info_struct failed");
		return false;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		CryLog("[write_png_file] Error during init_io");
		return false;
	}

	png_init_io(png_ptr, fp);


	if (setjmp(png_jmpbuf(png_ptr)))
	{
		CryLog("[write_png_file] Error during writing header");
		return false;
	}

	png_byte color_type = PNG_COLOR_TYPE_RGB;
	png_byte bit_depth = 8;

	png_set_IHDR(png_ptr, info_ptr, width, height,
		bit_depth, color_type, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);


	if (setjmp(png_jmpbuf(png_ptr)))
	{
		CryLog("[write_png_file] Error during writing bytes");
		return false;
	}

	png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
	for (int i = 0; i < height; ++i)
		row_pointers[i] = &data[i * width * 3];

	png_write_image(png_ptr, row_pointers);

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		CryLog("[write_png_file] Error during end of write");
		return false;
	}

	png_write_end(png_ptr, NULL);

	free(row_pointers);
	return true;
#else
	return false;
#endif
}

