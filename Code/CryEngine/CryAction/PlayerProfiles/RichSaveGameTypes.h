// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RICH_SAVE_GAME_TYPES_H__
#define __RICH_SAVE_GAME_TYPES_H__

#pragma pack(push)
#pragma pack(1)

struct CryBitmapFileHeader         /**** BMP file header structure ****/
{
	uint16 bfType;           /* Magic number for file */
	uint32 bfSize;           /* Size of file */
	uint16 bfReserved1;      /* Reserved */
	uint16 bfReserved2;      /* ... */
	uint32 bfOffBits;        /* Offset to bitmap data */

	AUTO_STRUCT_INFO;
};

struct CryBitmapInfoHeader         /**** BMP file info structure ****/
{
	uint32 biSize;           /* Size of info header */
	int32  biWidth;          /* Width of image */
	int32  biHeight;         /* Height of image */
	uint16 biPlanes;         /* Number of color planes */
	uint16 biBitCount;       /* Number of bits per pixel */
	uint32 biCompression;    /* Type of compression to use */
	uint32 biSizeImage;      /* Size of image data */
	int32  biXPelsPerMeter;  /* X pixels per meter */
	int32  biYPelsPerMeter;  /* Y pixels per meter */
	uint32 biClrUsed;        /* Number of colors used */
	uint32 biClrImportant;   /* Number of important colors */

	AUTO_STRUCT_INFO;
};

#pragma pack(pop)

namespace RichSaveGames
{
static const int RM_MAXLENGTH = 1024;

struct GUID
{
	uint32        Data1;
	uint16        Data2;
	uint16        Data3;
	unsigned char Data4[8];

	AUTO_STRUCT_INFO;
};

#pragma pack(push)
#pragma pack(1)
struct RICH_GAME_MEDIA_HEADER
{
	uint32  dwMagicNumber;
	uint32  dwHeaderVersion;
	uint32  dwHeaderSize;
	int64   liThumbnailOffset;
	uint32  dwThumbnailSize;
	GUID    guidGameId;
	wchar_t szGameName[RM_MAXLENGTH];
	wchar_t szSaveName[RM_MAXLENGTH];
	wchar_t szLevelName[RM_MAXLENGTH];
	wchar_t szComments[RM_MAXLENGTH];

	AUTO_STRUCT_INFO;
};
#pragma pack(pop)
}
#endif
