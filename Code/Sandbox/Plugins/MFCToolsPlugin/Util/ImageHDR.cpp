// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImageHDR.h"
#include <CrySystem/File/CryFile.h>

// We need globals because of the callbacks (they don't allow us to pass state)
static CryMutex globalFileMutex;
static size_t globalFileBufferOffset = 0;
static size_t globalFileBufferSize = 0;

static char* fgets(char* _Buf, int _MaxCount, CCryFile* _File)
{
	while (globalFileBufferOffset < globalFileBufferSize)
	{
		char chr;

		_File->ReadRaw(&chr, 1);
		globalFileBufferOffset++;

		*_Buf++ = chr;
		if (chr == '\n')
			break;
	}

	*_Buf = '\0';
	return _Buf;
}

static size_t fread(void* _DstBuf, size_t _ElementSize, size_t _Count, CCryFile* _File)
{
	size_t cpy = min(_ElementSize * _Count, globalFileBufferSize - globalFileBufferOffset);

	_File->ReadRaw(_DstBuf, cpy);
	globalFileBufferOffset += cpy;

	return cpy;
}

/* THIS CODE CARRIES NO GUARANTEE OF USABILITY OR FITNESS FOR ANY PURPOSE.
 * WHILE THE AUTHORS HAVE TRIED TO ENSURE THE PROGRAM WORKS CORRECTLY,
 * IT IS STRICTLY USE AT YOUR OWN RISK.  */

/* utility for reading and writing Ward's rgbe image format.
   See rgbe.txt file for more details.
 */

#include <stdio.h>

typedef struct
{
	int   valid;           /* indicate which fields are valid */
	char  programtype[16]; /* listed at beginning of file to identify it
	                        * after "#?".  defaults to "RGBE" */
	float gamma;           /* image has already been gamma corrected with
	                        * given gamma.  defaults to 1.0 (no correction) */
	float exposure;        /* a value of 1.0 in an image corresponds to
	                        * <exposure> watts/steradian/m^2.
	                        * defaults to 1.0 */
	char instructions[512];
} rgbe_header_info;

/* flags indicating which fields in an rgbe_header_info are valid */
#define RGBE_VALID_PROGRAMTYPE  0x01
#define RGBE_VALID_GAMMA        0x02
#define RGBE_VALID_EXPOSURE     0x04
#define RGBE_VALID_INSTRUCTIONS 0x08

/* return codes for rgbe routines */
#define RGBE_RETURN_SUCCESS 0
#define RGBE_RETURN_FAILURE -1

/* read or write headers */
/* you may set rgbe_header_info to null if you want to */
int RGBE_ReadHeader(CCryFile* fp, uint32* width, uint32* height, rgbe_header_info* info);

/* read or write pixels */
/* can read or write pixels in chunks of any size including single pixels*/
int RGBE_ReadPixels(CCryFile* fp, float* data, int numpixels);

/* read or write run length encoded files */
/* must be called to read or write whole scanlines */
int RGBE_ReadPixels_RLE(CCryFile* fp, float* data, uint32 scanline_width,
                        uint32 num_scanlines);

/* THIS CODE CARRIES NO GUARANTEE OF USABILITY OR FITNESS FOR ANY PURPOSE.
 * WHILE THE AUTHORS HAVE TRIED TO ENSURE THE PROGRAM WORKS CORRECTLY,
 * IT IS STRICTLY USE AT YOUR OWN RISK.  */

#include <math.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>

/* This file contains code to read and write four byte rgbe file format
   developed by Greg Ward.  It handles the conversions between rgbe and
   pixels consisting of floats.  The data is assumed to be an array of floats.
   By default there are three floats per pixel in the order red, green, blue.
   (RGBE_DATA_??? values control this.)  Only the mimimal header reading and
   writing is implemented.  Each routine does error checking and will return
   a status value as defined below.  This code is intended as a skeleton so
   feel free to modify it to suit your needs.

   (Place notice here if you modified the code.)
   posted to http://www.graphics.cornell.edu/~bjw/
   written by Bruce Walter  (bjw@graphics.cornell.edu)  5/26/95
   based on code written by Greg Ward
 */

#ifndef INLINE
	#ifdef _CPLUSPLUS
/* define if your compiler understands inline commands */
		#define INLINE inline
	#else
		#define INLINE
	#endif
#endif

/* offsets to red, green, and blue components in a data (float) pixel */
#define RGBE_DATA_RED   0
#define RGBE_DATA_GREEN 1
#define RGBE_DATA_BLUE  2
#define RGBE_DATA_ALPHA 3
/* number of floats per pixel */
#define RGBE_DATA_SIZE  4

enum rgbe_error_codes
{
	rgbe_read_error,
	rgbe_write_error,
	rgbe_format_error,
	rgbe_memory_error,
};

/* default error routine.  change this to change error handling */
static int rgbe_error(int rgbe_error_code, char* msg)
{
	switch (rgbe_error_code)
	{
	case rgbe_read_error:
		CryLog("RGBE read error");
		break;
	case rgbe_write_error:
		CryLog("RGBE write error");
		break;
	case rgbe_format_error:
		CryLog("RGBE bad file format: %s\n", msg);
		break;
	default:
	case rgbe_memory_error:
		CryLog("RGBE error: %s\n", msg);
	}
	return RGBE_RETURN_FAILURE;
}

/* standard conversion from float pixels to rgbe pixels */
/* note: you can remove the "inline"s if your compiler complains about it */
static INLINE void
float2rgbe(unsigned char rgbe[4], float red, float green, float blue)
{
	float v;
	int e;

	v = red;
	if (green > v) v = green;
	if (blue > v) v = blue;
	if (v < 1e-32f)
	{
		rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
	}
	else
	{
		v = frexp(v, &e) * 256.0f / v;
		rgbe[0] = (unsigned char) (red * v);
		rgbe[1] = (unsigned char) (green * v);
		rgbe[2] = (unsigned char) (blue * v);
		rgbe[3] = (unsigned char) (e + 128);
	}
}

/* standard conversion from rgbe to float pixels */
/* note: Ward uses ldexp(col+0.5,exp-(128+8)).  However we wanted pixels */
/*       in the range [0,1] to map back into the range [0,1].            */
static INLINE void
rgbe2type(float* red, float* green, float* blue, unsigned char rgbe[4])
{
	float f;

	if (rgbe[3])     /*nonzero pixel*/
	{
		f = ldexp(1.0f, rgbe[3] - (int)(128 + 8));
		*red = rgbe[0] * f;
		*green = rgbe[1] * f;
		*blue = rgbe[2] * f;
	}
	else
		*red = *green = *blue = 0.0;
}

static INLINE void
rgbe2type(char* red, char* green, char* blue, unsigned char rgbe[4])
{
	float f;

	if (rgbe[3])     /*nonzero pixel*/
	{
		f = ldexp(1.0f, rgbe[3] - (int)(128 + 8)) * 255.0f;
		*red = unsigned char(max(0.0f, min(rgbe[0] * f, 255.0f)));
		*green = unsigned char(max(0.0f, min(rgbe[1] * f, 255.0f)));
		*blue = unsigned char(max(0.0f, min(rgbe[2] * f, 255.0f)));
	}
	else
		*red = *green = *blue = 0;
}

/* minimal header reading.  modify if you want to parse more information */
int RGBE_ReadHeader(CCryFile* fp, uint32* width, uint32* height, rgbe_header_info* info)
{
	char buf[512];
	int found_format;
	float tempf;
	int i;

	found_format = 0;
	if (info)
	{
		info->valid = 0;
		info->programtype[0] = 0;
		info->gamma = info->exposure = 1.0;
	}
	if (fgets(buf, sizeof(buf) / sizeof(buf[0]), fp) == NULL)
		return rgbe_error(rgbe_read_error, NULL);
	if ((buf[0] != '#') || (buf[1] != '?'))
	{
		/* if you want to require the magic token then uncomment the next line */
		/*return rgbe_error(rgbe_format_error,"bad initial token"); */
	}
	else if (info)
	{
		info->valid |= RGBE_VALID_PROGRAMTYPE;
		for (i = 0; i < sizeof(info->programtype) - 1; i++)
		{
			if ((buf[i + 2] == 0) || isspace(buf[i + 2]))
				break;
			info->programtype[i] = buf[i + 2];
		}
		info->programtype[i] = 0;
		if (fgets(buf, sizeof(buf) / sizeof(buf[0]), fp) == 0)
			return rgbe_error(rgbe_read_error, NULL);
	}
	for (;; )
	{
		if ((buf[0] == 0) || (buf[0] == '\n'))
			return rgbe_error(rgbe_format_error, "no FORMAT specifier found");
		else if (strcmp(buf, "FORMAT=32-bit_rle_rgbe\n") == 0)
			break;       /* format found so break out of loop */
		else if (info && (sscanf(buf, "GAMMA=%g", &tempf) == 1))
		{
			info->gamma = tempf;
			info->valid |= RGBE_VALID_GAMMA;
		}
		else if (info && (sscanf(buf, "EXPOSURE=%g", &tempf) == 1))
		{
			info->exposure = tempf;
			info->valid |= RGBE_VALID_EXPOSURE;
		}
		else if (info && (!strncmp(buf, "INSTRUCTIONS=", 13)))
		{
			info->valid |= RGBE_VALID_INSTRUCTIONS;
			for (i = 0; i < sizeof(info->instructions) - 1; i++)
			{
				if ((buf[i + 13] == 0) || isspace(buf[i + 13]))
					break;
				info->instructions[i] = buf[i + 13];
			}
		}
		if (fgets(buf, sizeof(buf) / sizeof(buf[0]), fp) == 0)
			return rgbe_error(rgbe_read_error, NULL);
	}
	if (fgets(buf, sizeof(buf) / sizeof(buf[0]), fp) == 0)
		return rgbe_error(rgbe_read_error, NULL);
	if (strcmp(buf, "\n") != 0)
		return rgbe_error(rgbe_format_error,
		                  "missing blank line after FORMAT specifier");
	if (fgets(buf, sizeof(buf) / sizeof(buf[0]), fp) == 0)
		return rgbe_error(rgbe_read_error, NULL);
	if (sscanf(buf, "-Y %d +X %d", height, width) < 2)
		return rgbe_error(rgbe_format_error, "missing image size specifier");
	return RGBE_RETURN_SUCCESS;
}

/* simple read routine.  will not correctly handle run length encoding */
int RGBE_ReadPixels(CCryFile* fp, char* data, int numpixels)
{
	unsigned char rgbe[4];

	while (numpixels-- > 0)
	{
		if (fread(rgbe, sizeof(rgbe), 1, fp) < 1)
			return rgbe_error(rgbe_read_error, NULL);
		rgbe2type(&data[RGBE_DATA_RED], &data[RGBE_DATA_GREEN],
		          &data[RGBE_DATA_BLUE], rgbe);
		data[RGBE_DATA_ALPHA] = 0.0f;
		data += RGBE_DATA_SIZE;
	}
	return RGBE_RETURN_SUCCESS;
}

int RGBE_ReadPixels_RLE(CCryFile* fp, char* data, uint32 scanline_width,
                        uint32 num_scanlines)
{
	unsigned char rgbe[4], * scanline_buffer, * ptr, * ptr_end;
	int i, count;
	unsigned char buf[2];

	if ((scanline_width < 8) || (scanline_width > 0x7fff))
		/* run length encoding is not allowed so read flat*/
		return RGBE_ReadPixels(fp, data, scanline_width * num_scanlines);
	scanline_buffer = NULL;
	/* read in each successive scanline */
	while (num_scanlines > 0)
	{
		if (fread(rgbe, sizeof(rgbe), 1, fp) < 1)
		{
			free(scanline_buffer);
			return rgbe_error(rgbe_read_error, NULL);
		}
		if ((rgbe[0] != 2) || (rgbe[1] != 2) || (rgbe[2] & 0x80))
		{
			/* this file is not run length encoded */
			rgbe2type(&data[0], &data[1], &data[2], rgbe);
			data += RGBE_DATA_SIZE;
			free(scanline_buffer);
			return RGBE_ReadPixels(fp, data, scanline_width * num_scanlines - 1);
		}
		if ((((int)rgbe[2]) << 8 | rgbe[3]) != scanline_width)
		{
			free(scanline_buffer);
			return rgbe_error(rgbe_format_error, "wrong scanline width");
		}
		if (scanline_buffer == NULL)
			scanline_buffer = (unsigned char*)
			                  malloc(sizeof(unsigned char) * 4 * scanline_width);
		if (scanline_buffer == NULL)
			return rgbe_error(rgbe_memory_error, "unable to allocate buffer space");

		ptr = &scanline_buffer[0];
		/* read each of the four channels for the scanline into the buffer */
		for (i = 0; i < 4; i++)
		{
			ptr_end = &scanline_buffer[(i + 1) * scanline_width];
			while (ptr < ptr_end)
			{
				if (fread(buf, sizeof(buf[0]) * 2, 1, fp) < 1)
				{
					free(scanline_buffer);
					return rgbe_error(rgbe_read_error, NULL);
				}
				if (buf[0] > 128)
				{
					/* a run of the same value */
					count = buf[0] - 128;
					if ((count == 0) || (count > ptr_end - ptr))
					{
						free(scanline_buffer);
						return rgbe_error(rgbe_format_error, "bad scanline data");
					}
					while (count-- > 0)
						*ptr++ = buf[1];
				}
				else
				{
					/* a non-run */
					count = buf[0];
					if ((count == 0) || (count > ptr_end - ptr))
					{
						free(scanline_buffer);
						return rgbe_error(rgbe_format_error, "bad scanline data");
					}
					*ptr++ = buf[1];
					if (--count > 0)
					{
						if (fread(ptr, sizeof(*ptr) * count, 1, fp) < 1)
						{
							free(scanline_buffer);
							return rgbe_error(rgbe_read_error, NULL);
						}
						ptr += count;
					}
				}
			}
		}
		/* now convert data from buffer into floats */
		for (i = 0; i < scanline_width; i++)
		{
			rgbe[0] = scanline_buffer[i];
			rgbe[1] = scanline_buffer[i + scanline_width];
			rgbe[2] = scanline_buffer[i + 2 * scanline_width];
			rgbe[3] = scanline_buffer[i + 3 * scanline_width];
			rgbe2type(&data[RGBE_DATA_RED], &data[RGBE_DATA_GREEN],
			          &data[RGBE_DATA_BLUE], rgbe);
			data[RGBE_DATA_ALPHA] = 0.0f;
			data += RGBE_DATA_SIZE;
		}
		num_scanlines--;
	}
	free(scanline_buffer);
	return RGBE_RETURN_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////////

bool CImageHDR::Load(const string& fileName, CImageEx& outImage)
{
	CCryFile file;
	if (!file.Open(fileName, "rb"))
	{
		CryLog("File not found %s", (const char*)fileName);
		return false;
	}

	// We use some global variables in callbacks, so we must
	// prevent multithread access to the data
	CryAutoLock<CryMutex> tifAutoLock(globalFileMutex);

	globalFileBufferSize = file.GetLength();
	globalFileBufferOffset = 0;

	bool bRet = false;
	uint32 dwWidth, dwHeight;
	rgbe_header_info info;

	if (RGBE_RETURN_SUCCESS == RGBE_ReadHeader(&file, &dwWidth, &dwHeight, &info))
	{
		if (outImage.Allocate(dwWidth, dwHeight))
		{
			char* pDst = (char*)outImage.GetData();

			if (RGBE_RETURN_SUCCESS == RGBE_ReadPixels_RLE(&file, (char*)pDst, dwWidth, dwHeight))
			{
				bRet = true;
			}
		}
	}

	if (!bRet)
	{
		outImage.Detach();
	}

	return bRet;
}

