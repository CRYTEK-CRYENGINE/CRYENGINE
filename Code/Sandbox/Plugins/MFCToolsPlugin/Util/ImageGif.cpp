// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImageGif.h"
#include <CrySystem/File/CryFile.h>

//---------------------------------------------------------------------------

#define NEXTBYTE        (*ptr++)
#define IMAGESEP        0x2c
#define GRAPHIC_EXT     0xf9
#define PLAINTEXT_EXT   0x01
#define APPLICATION_EXT 0xff
#define COMMENT_EXT     0xfe
#define START_EXTENSION 0x21
#define INTERLACEMASK   0x40
#define COLORMAPMASK    0x80
#define CHK(x) x

#pragma pack(push,1)
struct SGIFRGBcolor
{
	uint8 red, green, blue;
};
struct SGIFRGBPixel
{
	uint8 red, green, blue, alpha;
};
#pragma pack(pop)

static int BitOffset = 0,            /* Bit Offset of next code */
           XC = 0, YC = 0,           /* Output X and Y coords of current pixel */
           Pass = 0,                 /* Used by output routine if interlaced pic */
           OutCount = 0,             /* Decompressor output 'stack count' */
           RWidth, RHeight,          /* screen dimensions */
           Width, Height,            /* image dimensions */
           LeftOfs, TopOfs,          /* image offset */
           BitsPerPixel,             /* Bits per pixel, read from GIF header */
           BytesPerScanline,         /* bytes per scanline in output raster */
           ColorMapSize,             /* number of colors */
           Background,               /* background color */
           CodeSize,                 /* Code size, read from GIF header */
           InitCodeSize,             /* Starting code size, used during Clear */
           Code,                     /* Value returned by ReadCode */
           MaxCode,                  /* limiting value for current code size */
           ClearCode,                /* GIF clear code */
           EOFCode,                  /* GIF end-of-information code */
           CurCode, OldCode, InCode, /* Decompressor variables */
           FirstFree,                /* First free code, generated per GIF spec */
           FreeCode,                 /* Decompressor, next free slot in hash table*/
           FinChar,                  /* Decompressor variable */
           BitMask,                  /* AND mask for data size */
           ReadMask;                 /* Code AND mask for current code size */

static bool Interlace, HasColormap;

static SGIFRGBPixel* Image;   /* The result array */
static SGIFRGBcolor* Palette; /* The palette that is used */
static uint8* IndexImage;

static uint8* Raster;     /* The raster data stream, unblocked */

static uint8 used[256];
static int numused;

const char* id87 = "GIF87a";
const char* id89 = "GIF89a";

static int log2(int);

/* Fetch the next code from the raster data stream.  The codes can be
 * any length from 3 to 12 bits, packed into 8-bit bytes, so we have to
 * maintain our location in the Raster array as a BIT Offset.  We compute
 * the uint8 Offset into the raster array by dividing this by 8, pick up
 * three bytes, compute the bit Offset into our 24-bit chunk, shift to
 * bring the desired code to the bottom, then mask it off and return it.
 */
inline int ReadCode(void)
{
	int RawCode, ByteOffset;

	ByteOffset = BitOffset / 8;
	RawCode = Raster[ByteOffset] + (0x100 * Raster[ByteOffset + 1]);

	if (CodeSize >= 8)
		RawCode += (0x10000 * Raster[ByteOffset + 2]);

	RawCode >>= (BitOffset % 8);
	BitOffset += CodeSize;

	return RawCode & ReadMask;
}

inline void AddToPixel(uint8 Index)
{
	if (YC < Height)
	{
		SGIFRGBPixel* p = Image + YC * BytesPerScanline + XC;
		p->red = Palette[Index].red;
		p->green = Palette[Index].green;
		p->blue = Palette[Index].blue;
		p->alpha = 0;
		IndexImage[YC * BytesPerScanline + XC] = Index;
	}

	if (!used[Index]) { used[Index] = 1;  numused++; }

	/* Update the X-coordinate, and if it overflows, update the Y-coordinate */

	if (++XC == Width)
	{

		/* If a non-interlaced picture, just increment YC to the next scan line.
		 * If it's interlaced, deal with the interlace as described in the GIF
		 * spec.  Put the decoded scan line out to the screen if we haven't gone
		 * past the bottom of it
		 */

		XC = 0;
		if (!Interlace) YC++;
		else
		{
			switch (Pass)
			{
			case 0:
				YC += 8;
				if (YC >= Height)
				{
					Pass++;
					YC = 4;
				}
				break;
			case 1:
				YC += 8;
				if (YC >= Height)
				{
					Pass++;
					YC = 2;
				}
				break;
			case 2:
				YC += 4;
				if (YC >= Height)
				{
					Pass++;
					YC = 1;
				}
				break;
			case 3:
				YC += 2;
				break;
			default:
				break;
			}
		}
	}
}

bool CImageGif::Load(const string& fileName, CImageEx& outImage)
{
	bool ret = false;

	std::vector<uint8> data;
	CCryFile file;
	if (!file.Open(fileName, "rb"))
	{
		CryLog("File not found %s", (const char*)fileName);
		return false;
	}
	long filesize = file.GetLength();

	data.resize(filesize);
	uint8* ptr = &data[0];

	file.ReadRaw(ptr, filesize);

	/* Detect if this is a GIF file */
	if (strncmp((char*)ptr, "GIF87a", 6) && strncmp((char*)ptr, "GIF89a", 6))
	{
		CryLog("Bad GIF file format %s", (const char*)fileName);
		return false;
	}

	int numcols;
	unsigned char ch, ch1;
	uint8* ptr1;
	int i;
	short transparency = -1;

	TImage<uint8> outImageIndex;

	/* The hash table used by the decompressor */
	int* Prefix;
	int* Suffix;

	/* An output array used by the decompressor */
	int* OutCode;

	CHK(Prefix = new int[4096]);
	CHK(Suffix = new int[4096]);
	CHK(OutCode = new int[1025]);

	BitOffset = 0;
	XC = YC = 0;
	Pass = 0;
	OutCount = 0;

	Palette = NULL;
	CHK(Raster = new uint8[filesize]);

	if (strncmp((char*) ptr, id87, 6))
		if (strncmp((char*) ptr, id89, 6))
		{
			CryLog("Bad GIF file format %s", (const char*)fileName);
			goto cleanup;
		}

	ptr += 6;

	/* Get variables from the GIF screen descriptor */

	ch = NEXTBYTE;
	RWidth = ch + 0x100 * NEXTBYTE;         /* screen dimensions... not used. */
	ch = NEXTBYTE;
	RHeight = ch + 0x100 * NEXTBYTE;

	ch = NEXTBYTE;
	HasColormap = ((ch & COLORMAPMASK) ? true : false);

	BitsPerPixel = (ch & 7) + 1;
	numcols = ColorMapSize = 1 << BitsPerPixel;
	BitMask = ColorMapSize - 1;

	Background = NEXTBYTE;        /* background color... not used. */

	if (NEXTBYTE)     /* supposed to be NULL */
	{
		CryLog("Bad GIF file format %s", (const char*)fileName);
		goto cleanup;
	}

	/* Read in global colormap. */
	SGIFRGBcolor mspPal[1024];

	if (HasColormap)
	{
		for (i = 0; i < ColorMapSize; i++)
		{
			mspPal[i].red = NEXTBYTE;
			mspPal[i].green = NEXTBYTE;
			mspPal[i].blue = NEXTBYTE;
			used[i] = 0;
		}
		Palette = mspPal;

		numused = 0;
	}   /* else no colormap in GIF file */

	/* look for image separator */

	for (ch = NEXTBYTE; ch != IMAGESEP; ch = NEXTBYTE)
	{
		i = ch;
		if (ch != START_EXTENSION)
		{
			CryLog("Bad GIF file format %s", (const char*)fileName);
			goto cleanup;
		}

		/* handle image extensions */
		switch (ch = NEXTBYTE)
		{
		case GRAPHIC_EXT:
			ch = NEXTBYTE;
			if (ptr[0] & 0x1)
				transparency = ptr[3];     /* transparent color index */
			ptr += ch;
			break;
		case PLAINTEXT_EXT:
			break;
		case APPLICATION_EXT:
			break;
		case COMMENT_EXT:
			break;
		default:
			{
				CryLog("Invalid GIF89 extension %s", (const char*)fileName);
				goto cleanup;
			}
		}

		while ((ch = NEXTBYTE))
			ptr += ch;
	}

	//if (transparency >= 0)
	//mfSet_transparency(transparency);

	/* Now read in values from the image descriptor */

	ch = NEXTBYTE;
	LeftOfs = ch + 0x100 * NEXTBYTE;
	ch = NEXTBYTE;
	TopOfs = ch + 0x100 * NEXTBYTE;
	ch = NEXTBYTE;
	Width = ch + 0x100 * NEXTBYTE;
	ch = NEXTBYTE;
	Height = ch + 0x100 * NEXTBYTE;
	Interlace = ((NEXTBYTE & INTERLACEMASK) ? true : false);

	// Set the dimensions which will also allocate the image data
	// buffer.
	outImage.Allocate(Width, Height);
	//mfSet_dimensions (Width, Height);
	Image = (SGIFRGBPixel*)outImage.GetData();
	outImageIndex.Allocate(Width, Height);
	IndexImage = outImageIndex.GetData();

	/* Note that I ignore the possible existence of a local color map.
	 * I'm told there aren't many files around that use them, and the spec
	 * says it's defined for future use.  This could lead to an error
	 * reading some files.
	 */

	/* Start reading the raster data. First we get the intial code size
	 * and compute decompressor constant values, based on this code size.
	 */

	CodeSize = NEXTBYTE;
	ClearCode = (1 << CodeSize);
	EOFCode = ClearCode + 1;
	FreeCode = FirstFree = ClearCode + 2;

	/* The GIF spec has it that the code size is the code size used to
	 * compute the above values is the code size given in the file, but the
	 * code size used in compression/decompression is the code size given in
	 * the file plus one. (thus the ++).
	 */

	CodeSize++;
	InitCodeSize = CodeSize;
	MaxCode = (1 << CodeSize);
	ReadMask = MaxCode - 1;

	/* Read the raster data.  Here we just transpose it from the GIF array
	 * to the Raster array, turning it from a series of blocks into one long
	 * data stream, which makes life much easier for ReadCode().
	 */

	ptr1 = Raster;
	do
	{
		ch = ch1 = NEXTBYTE;
		while (ch--)
			*ptr1++ = NEXTBYTE;
		if ((ptr1 - Raster) > filesize)
		{
			CryLog("Corrupted GIF file (unblock) %s", (const char*)fileName);
			goto cleanup;
		}
	}
	while (ch1);

	BytesPerScanline = Width;

	/* Decompress the file, continuing until you see the GIF EOF code.
	 * One obvious enhancement is to add checking for corrupt files here.
	 */

	Code = ReadCode();
	while (Code != EOFCode)
	{

		/* Clear code sets everything back to its initial value, then reads the
		 * immediately subsequent code as uncompressed data.
		 */

		if (Code == ClearCode)
		{
			CodeSize = InitCodeSize;
			MaxCode = (1 << CodeSize);
			ReadMask = MaxCode - 1;
			FreeCode = FirstFree;
			CurCode = OldCode = Code = ReadCode();
			FinChar = CurCode & BitMask;
			AddToPixel(FinChar);
		}
		else
		{
			/* If not a clear code, then must be data: save same as CurCode and InCode */
			CurCode = InCode = Code;

			/* If greater or equal to FreeCode, not in the hash table yet;
			 * repeat the last character decoded
			 */

			if (CurCode >= FreeCode)
			{
				CurCode = OldCode;
				OutCode[OutCount++] = FinChar;
			}

			/* Unless this code is raw data, pursue the chain pointed to by CurCode
			 * through the hash table to its end; each code in the chain puts its
			 * associated output code on the output queue.
			 */

			while (CurCode > BitMask)
			{
				if (OutCount > 1024)
				{
					CryLog("Corrupted GIF file (OutCount) %s", (const char*)fileName);
					goto cleanup;
				}
				OutCode[OutCount++] = Suffix[CurCode];
				CurCode = Prefix[CurCode];
			}

			/* The last code in the chain is treated as raw data. */

			FinChar = CurCode & BitMask;
			OutCode[OutCount++] = FinChar;

			/* Now we put the data out to the Output routine.
			 * It's been stacked LIFO, so deal with it that way...
			 */

			for (i = OutCount - 1; i >= 0; i--)
				AddToPixel(OutCode[i]);
			OutCount = 0;

			/* Build the hash table on-the-fly. No table is stored in the file. */

			Prefix[FreeCode] = OldCode;
			Suffix[FreeCode] = FinChar;
			OldCode = InCode;

			/* Point to the next slot in the table.  If we exceed the current
			 * MaxCode value, increment the code size unless it's already 12.  If it
			 * is, do nothing: the next code decompressed better be CLEAR
			 */

			FreeCode++;
			if (FreeCode >= MaxCode)
			{
				if (CodeSize < 12)
				{
					CodeSize++;
					MaxCode *= 2;
					ReadMask = (1 << CodeSize) - 1;
				}
			}
		}
		Code = ReadCode();
	}

	ret = true;

cleanup:
	if (Raster) { CHK(delete[] Raster); }
	if (Prefix) { CHK(delete[] Prefix); }
	if (Suffix) { CHK(delete[] Suffix); }
	if (OutCode) { CHK(delete[] OutCode); }
	return ret;
}

