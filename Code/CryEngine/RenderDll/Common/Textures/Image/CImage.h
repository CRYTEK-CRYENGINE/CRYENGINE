// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef CIMAGE_H
#define CIMAGE_H

#include <CryRenderer/IImage.h>

#define SH_LITTLE_ENDIAN

// The mask for extracting just R/G/B from an ulong or SRGBPixel
#ifdef SH_BIG_ENDIAN
	#define RGB_MASK 0xffffff00
#else
	#define RGB_MASK 0x00ffffff
#endif

/**
 * An RGB pixel.
 */
struct SRGBPixel
{
	uint8 blue, green, red, alpha;
	SRGBPixel()  /* : red(0), green(0), blue(0), alpha(255) {} */
	{ *(unsigned int*)this = (unsigned int)~RGB_MASK; }
	SRGBPixel(int r, int g, int b) : red(r), green(g), blue(b), alpha(255) {}
	//bool eq (const SRGBPixel& p) const { return ((*(unsigned int *)this) & RGB_MASK) == ((*(unsigned int *)&p) & RGB_MASK); }
};

class CImageFile;
namespace DDSSplitted {
struct DDSDesc;
}

struct IImageFileStreamCallback
{
	virtual void OnImageFileStreamComplete(CImageFile* pImFile) = 0;

protected:
	virtual ~IImageFileStreamCallback() {}
};

struct SImageFileStreamState
{
	enum
	{
		MaxStreams = 64
	};

	struct Request
	{
		void*  pOut;
		size_t nOffs;
		size_t nSize;
	};

	void RaiseComplete(CImageFile* pFile)
	{
		if (m_pCallback)
		{
			m_pCallback->OnImageFileStreamComplete(pFile);
			m_pCallback = NULL;
		}
	}

	volatile int              m_nPending;
	uint32                    m_nFlags;
	IImageFileStreamCallback* m_pCallback;
	IReadStreamPtr            m_pStreams[MaxStreams];
	Request                   m_requests[MaxStreams];
};

class CImageFile : public IImageFile, public IStreamCallback
{
private:
	CImageFile(const CImageFile&);
	CImageFile& operator=(const CImageFile&);

private:
	volatile int m_nRefCount;

protected:
	int    m_Width;  // Width of image.
	int    m_Height; // Height of image.
	int    m_Depth;  // Depth of image.
	int    m_Sides;  // Depth of image.

	int    m_ImgSize;

	int    m_NumMips;
	int    m_NumPersistantMips;
	int    m_Flags;       // e.g. FIM_GREYSCALE|FIM_ALPHA
	int    m_nStartSeek;
	float  m_fAvgBrightness;
	ColorF m_cMinColor;
	ColorF m_cMaxColor;

	union // The image data.
	{
		byte*      m_pByteImage[6];
		SRGBPixel* m_pPixImage[6];
	};

	EImFileError           m_eError;   // Last error code.
	string                 m_FileName; // file name

	ETEX_Format            m_eFormat;
	ETEX_TileMode          m_eTileMode;

	SImageFileStreamState* m_pStreamState;

protected:
	CImageFile(const string& filename);

	void mfSet_error(const EImFileError error, const char* detail = NULL);
	void mfSet_dimensions(const int w, const int h);
public:
	virtual ~CImageFile();

	virtual int AddRef()
	{
		return CryInterlockedIncrement(&m_nRefCount);
	}

	virtual int Release()
	{
		int nRef = CryInterlockedDecrement(&m_nRefCount);
		if (nRef == 0)
		{
			delete this;
		}
		else if (nRef < 0)
		{
			assert(0);
			CryFatalError("Deleting Reference Counted Object Twice");
		}
		return nRef;
	}

	const string&                mfGet_filename() const { return m_FileName; }

	int                          mfGet_width() const    { return m_Width; }
	int                          mfGet_height() const   { return m_Height; }
	int                          mfGet_depth() const    { return m_Depth; }
	int                          mfGet_NumSides() const { return m_Sides; }

	EImFileError                 mfGet_error() const    { return m_eError; }

	byte*                        mfGet_image(const int nSide);
	void                         mfFree_image(const int nSide);
	bool                         mfIs_image(const int nSide) const              { return m_pByteImage[nSide] != NULL; }

	int                          mfGet_StartSeek() const                        { return m_nStartSeek; }

	void                         mfSet_ImageSize(int Size)                      { m_ImgSize = Size; }
	int                          mfGet_ImageSize() const                        { return m_ImgSize; }

	ETEX_Format                  mfGetFormat() const                            { return m_eFormat; }
	ETEX_TileMode                mfGetTileMode() const                          { return m_eTileMode; }

	void                         mfSet_numMips(const int num)                   { m_NumMips = num; }
	int                          mfGet_numMips() const                          { return m_NumMips; }

	void                         mfSet_numPersistantMips(const int num)         { m_NumPersistantMips = num; }
	int                          mfGet_numPersistantMips() const                { return m_NumPersistantMips; }

	void                         mfSet_avgBrightness(const float avgBrightness) { m_fAvgBrightness = avgBrightness; }
	float                        mfGet_avgBrightness() const                    { return m_fAvgBrightness; }

	void                         mfSet_minColor(const ColorF& minColor)         { m_cMinColor = minColor; }
	const ColorF&                mfGet_minColor() const                         { return m_cMinColor; }

	void                         mfSet_maxColor(const ColorF& maxColor)         { m_cMaxColor = maxColor; }
	const ColorF&                mfGet_maxColor() const                         { return m_cMaxColor; }

	void                         mfSet_Flags(const int Flags)                   { m_Flags |= Flags; }
	int                          mfGet_Flags() const                            { return m_Flags; }

	void                         mfAbortStreaming();

	virtual DDSSplitted::DDSDesc mfGet_DDSDesc() const = 0;

public:
	enum class EResourceCompilerResult
	{
		//! Texture was already compiled, no need to run RC
		AlreadyCompiled,
		//! Texture was queued for compilation, will not be available immediately
		Queued,
		//! Texture was compiled immediately, and can be loaded right away.
		Available,
		//! Texture compilation failed
		Failed,
		//! RC invoke was disabled, or image format was not supported
		Skipped,
	};

	static _smart_ptr<CImageFile>  mfLoad_file(const string& filename, const uint32 nFlags);
	static _smart_ptr<CImageFile>  mfStream_File(const string& filename, const uint32 nFlags, IImageFileStreamCallback* pCallback);
	static EResourceCompilerResult mfInvokeRC(const string& fileToLoad, const string& filename, char* extOut, size_t extOutCapacity, bool immediate);
};

#endif