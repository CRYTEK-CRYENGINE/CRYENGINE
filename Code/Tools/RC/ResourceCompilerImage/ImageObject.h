// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <assert.h>                   // assert()
#include <CryMath/CryHalf.inl>                // CryHalf
#include <CryMath/Cry_Math.h>                 // Vec4

#include "IRCLog.h"                   // IRCLog
#include "IConfig.h"                  // IConfig

class CBumpProperties;

#include "ImageProperties.h"          // CBumpProperties
#include "PixelFormats.h"             // EPixelFormat
#include "Operations/Histogram.h"

template <class T>
class Color4
{
public:
	T components[4];

public:
	Color4()
	{
	}

	Color4(const T& c0, const T& c1, const T& c2, const T& c3)
	{
		components[0] = c0;
		components[1] = c1;
		components[2] = c2;
		components[3] = c3;
	}
};

template <class T>
class Color2
{
public:
	T components[2];

public:
	Color2()
	{
	}

	Color2(const T& c0, const T& c1)
	{
		components[0] = c0;
		components[1] = c1;
	}
};

struct SHalf
{
	explicit SHalf(float f)
	{
		h = CryConvertFloatToHalf(f);
	}

	operator float() const
	{
		return CryConvertHalfToFloat(h);
	}

	CryHalf h;
};

// ImageObject allows the abstraction of different kinds of
// images generated during conversion
class ImageObject
{
public:
	enum ECubemap
	{
		eCubemap_UnknownYet,              // don't know yet
		eCubemap_Yes,                     // it is a cubemap
		eCubemap_No                       // it is not a cubemap
	};

	enum EColorNormalization
	{
		eColorNormalization_Normalize,
		eColorNormalization_PassThrough,
	};

	enum EAlphaNormalization
	{
		eAlphaNormalization_SetToZero,
		eAlphaNormalization_Normalize,
		eAlphaNormalization_PassThrough,
	};

	enum EAlphaContent
	{
		eAlphaContent_Indeterminate,      // the format may have alpha, but can't be calculated
		eAlphaContent_Absent,             // the format has no alpha
		eAlphaContent_OnlyWhite,          // alpha contains just white
		eAlphaContent_OnlyBlack,          // alpha contains just black
		eAlphaContent_OnlyBlackAndWhite,  // alpha contains just black and white
		eAlphaContent_Greyscale           // alpha contains grey tones
	};

private:
	class MipLevel
	{
	public:
		uint32 m_width;
		uint32 m_height;
		uint32 m_rowCount;  // for compressed textures m_rowCount is usually less than m_height
		uint32 m_pitch;     // row size in bytes
		uint8* m_pData;

	public:
		MipLevel() 
			: m_width(0)
			, m_height(0)
			, m_rowCount(0)
			, m_pitch(0)
			, m_pData(0)
		{
		}

		~MipLevel()
		{
			delete[] m_pData;
			m_pData = 0;
		}

		void Alloc()
		{
			assert(m_pData == 0);
			m_pData = new uint8[m_pitch * m_rowCount];
		}

		uint32 GetSize() const
		{
			assert(m_pitch);
			return m_pitch * m_rowCount;
		}
	};

private:
	EPixelFormat           m_pixelFormat;
	ECubemap               m_eCubemap;
	std::vector<MipLevel*> m_mips;        // stores *pointers* to avoid reallocations when elements are erase()'d

	Vec4         m_colMinARGB;            // ARGB will be added the properties of the DDS file
	Vec4         m_colMaxARGB;            // ARGB will be added the properties of the DDS file
	float        m_averageBrightness;     // will be added to the properties of the DDS file
	ImageObject* m_pAttachedImage;        // alpha channel in the case it was lost through format conversion
	uint32       m_imageFlags;            // combined from CImageExtensionHelper::EIF_Cubemap,...
	uint32       m_numPersistentMips;
	uint32       m_compressedBlockWidth;
	uint32       m_compressedBlockHeight;

private:
	// Prevent copying
	ImageObject(const ImageObject&);
	ImageObject& operator=(const ImageObject&);

public:
	// allocate an image with the given properties
	ImageObject(const uint32 width, const uint32 height, const uint32 maxMipCount, const EPixelFormat pixelFormat, ECubemap eCubemap, const int compressedBlockWidth = -1, const int compressedBlockHeight = -1);
	~ImageObject();

	void ResetImage(const uint32 width, const uint32 height, const uint32 maxMipCount, const EPixelFormat pixelFormat, ECubemap eCubemap, const int compressedBlockWidth = -1, const int compressedBlockHeight = -1);

	// clone this image-object's contents, but not the properties
	ImageObject* CopyImage() const;

	// Arguments:
	//   inSrc - must not be 0
	//	 offsetx - initial x position to copy pixels from
	//	 offsety - initial y position to copy pixels from
	//   width - amount of x pixels to copy and final width of destination image
	//	 height - amount of y pixels to copy and final height of destination image
	ImageObject* CopyImageArea(int offsetx, int offsety, int width, int height) const;

	void ClearImage();
	void ClearImageArea(int offsetx, int offsety, int width, int height);

	// allocate an image with the same properties as the given image and the requested format, with dropping top-mips if requested
	ImageObject* AllocateImage(const uint32 highestMip, const EPixelFormat pixelFormat, const int compressedBlockWidth = -1, const int compressedBlockHeight = -1) const;
	// allocate an image with the same properties as the given image, with dropping top-mips if requested
	ImageObject* AllocateImage(const uint32 highestMip = 0) const;
	// allocate an image with the same properties as the given image, with adding or dropping top-mips if requested
	ImageObject* AllocateImage(const CImageProperties* pProps, bool bRemoveMips, uint32& reduceResolution) const;

	// clone this image-object's alpha channel, A32B32G32R32F/A16B16G16R16F/A16B16G16R16/A8R8G8B8/X8R8G8B8
	ImageObject* CopyAnyAlphaIntoA8Image(int maxMipCount) const;
	ImageObject* CopyAnyAlphaIntoR8Image(int maxMipCount) const;
	ImageObject* CopyAnyAlphaIntoR16Image(int maxMipCount) const;
	ImageObject* CopyAnyAlphaIntoR16FImage(int maxMipCount) const;
	ImageObject* CopyAnyAlphaIntoR32FImage(int maxMipCount) const;

	// copy the alpha channel out of the given image, A32B32G32R32F/A16B16G16R16F/A16B16G16R16/A8R8G8B8
	void TakeoverAnyAlphaFromA8Image(const ImageObject* pImage);
	void TakeoverAnyAlphaFromR8Image(const ImageObject* pImage);
	void TakeoverAnyAlphaFromR16Image(const ImageObject* pImage);
	void TakeoverAnyAlphaFromR16FImage(const ImageObject* pImage);
	void TakeoverAnyAlphaFromR32FImage(const ImageObject* pImage);

	// -----------------------------------------------------------------
	// getters/setters

	EPixelFormat GetPixelFormat() const
	{
		return m_pixelFormat;
	}

	uint32 GetPixelCount(const uint32 mip) const
	{
		assert(mip < (uint32)m_mips.size());
		assert(m_mips[mip]);

		return m_mips[mip]->m_width * m_mips[mip]->m_height;
	}

	uint32 GetWidth(const uint32 mip) const
	{
		assert(mip < (uint32)m_mips.size());
		assert(m_mips[mip]);

		return m_mips[mip]->m_width;
	}

	uint32 GetHeight(const uint32 mip) const
	{
		assert(mip<(uint32)m_mips.size());
		assert(m_mips[mip]);

		return m_mips[mip]->m_height;
	}

	ECubemap GetCubemap() const
	{
		return m_eCubemap;
	}

	void SetCubemap(ECubemap eCubemap)
	{
		assert(eCubemap != eCubemap_UnknownYet);

		m_eCubemap = eCubemap;
	}

	uint32 GetMipCount() const
	{
		assert(!m_mips.empty());

		return (uint32)m_mips.size();
	}

	void GetExtent(uint32& width, uint32& height, uint32& mipCount) const
	{
		mipCount = (uint32)m_mips.size();

		assert(mipCount > 0);
		assert(m_mips[0]);

		width = m_mips[0]->m_width;
		height = m_mips[0]->m_height;
	}

	void TransformExtent(uint32& width, uint32& height, uint32& mipCount, const CImageProperties& props) const
	{
		if (props.GetCubemap() && (GetCubemap() == eCubemap_UnknownYet))
		{
			if (width * 1 == height * 6)
			{
				height = props.GetPowOf2() ? Util::getFlooredPowerOfTwo(height / 1) : height / 1;
				width = height * 6;
				return;
			}

			if (width * 2 == height * 4)
			{
				height = props.GetPowOf2() ? Util::getFlooredPowerOfTwo(height / 2) : height / 2;
				width = height * 6;
				return;
			}

			if (width * 3 == height * 4)
			{
				height = props.GetPowOf2() ? Util::getFlooredPowerOfTwo(height / 3) : height / 3;
				width = height * 6;
				return;
			}

			if (width * 4 == height * 3)
			{
				height = props.GetPowOf2() ? Util::getFlooredPowerOfTwo(height / 4) : height / 4;
				width = height * 6;
				return;
			}
		}
	}

	// Get extent of the image with all tentative changes applied
	void GetExtentTransformed(uint32& width, uint32& height, uint32& mipCount, const CImageProperties& props) const
	{
		GetExtent(width, height, mipCount);
		TransformExtent(width, height, mipCount, props);
	}

	uint32 GetMipDataSize(const uint32 mip) const
	{
		assert(mip < m_mips.size());

		return m_mips[mip]->GetSize();
	}

	template <class T>
	T* GetPixelsPointer(const uint32 mip) const
	{
		assert((m_pixelFormat >= 0) && (m_pixelFormat < ePixelFormat_Count));
		assert(!CPixelFormats::GetPixelFormatInfo(m_pixelFormat)->bCompressed);
		assert(CPixelFormats::GetPixelFormatInfo(m_pixelFormat)->bytesPerBlock == sizeof(T));
		assert(mip < (uint32)m_mips.size());
		assert(m_mips[mip]);

		return (T *)(m_mips[mip]->m_pData);
	}

	template <class T>
	T& GetPixel(const uint32 mip, uint32 x, uint32 y) const
	{
		assert((m_pixelFormat >= 0) && (m_pixelFormat < ePixelFormat_Count));
		assert(!CPixelFormats::GetPixelFormatInfo(m_pixelFormat)->bCompressed);
		assert(CPixelFormats::GetPixelFormatInfo(m_pixelFormat)->bytesPerBlock == sizeof(T));
		assert(mip < (uint32)m_mips.size());
		assert(m_mips[mip]);
		assert(x < m_mips[mip]->m_width);
		assert(x < m_mips[mip]->m_height);

		return ((T*)(m_mips[mip]->m_pData))[y * m_mips[mip]->m_width + x];
	}

	void GetImagePointer(const uint32 mip, char*& pMem, uint32& pitch) const
	{	
		assert((m_pixelFormat >= 0) && (m_pixelFormat < ePixelFormat_Count));
		assert(mip < (uint32)m_mips.size());
		assert(m_mips[mip]);

		pMem = (char *)(m_mips[mip]->m_pData);
		pitch = m_mips[mip]->m_pitch; 
	}

	// ARGB
	void GetColorRange(Vec4& minColor, Vec4& maxColor) const
	{
		minColor = m_colMinARGB;
		maxColor = m_colMaxARGB;
	}

	// ARGB
	void SetColorRange(const Vec4& minColor, const Vec4& maxColor)
	{
		m_colMinARGB = minColor;
		m_colMaxARGB = maxColor;
	}

	//! calculates the average brightness for a texture
	float CalculateAverageBrightness() const;

	float GetAverageBrightness() const
	{
		return m_averageBrightness;
	}

	void SetAverageBrightness(const float avgBrightness)
	{
		m_averageBrightness = avgBrightness;
	}

	// Return:
	//   combined from CImageExtensionHelper::EIF_Cubemap,...
	uint32 GetImageFlags() const 
	{ 
		return m_imageFlags;	
	}

	// Arguments:
	//   imageFlags - combined from CImageExtensionHelper::EIF_Cubemap,...
	void SetImageFlags(const uint32 imageFlags)
	{
		m_imageFlags = imageFlags;
	}

	// Arguments:
	//   imageFlags - combined from CImageExtensionHelper::EIF_Cubemap,...
	void AddImageFlags(const uint32 imageFlags)
	{
		m_imageFlags |= imageFlags;
	}

	// Arguments:
	//   imageFlags - combined from CImageExtensionHelper::EIF_Cubemap,...
	void RemoveImageFlags(const uint32 imageFlags) 
	{
		m_imageFlags &= ~imageFlags;
	}

	// Arguments:
	//   imageFlags - combined from CImageExtensionHelper::EIF_Cubemap,...
	bool HasImageFlags(const uint32 imageFlags) const
	{
		return (m_imageFlags & imageFlags) != 0;
	}

	uint32 GetNumPersistentMips() const
	{
		return m_numPersistentMips;
	}

	void SetNumPersistentMips(uint32 nMips)
	{
		m_numPersistentMips = nMips;
	}

	uint32 GetCompressedBlockWidth() const
	{
		return m_compressedBlockWidth;
	}

	uint32 GetCompressedBlockHeight() const
	{
		return m_compressedBlockHeight;
	}

	bool HasCubemapCompatibleSizes() const
	{
		uint32 w, h, mips;
		GetExtent(w, h, mips);

		switch (m_eCubemap)
		{
		case eCubemap_UnknownYet:
			return (w == h * 6) || (w == h * 2) || (w * 3 == h * 4) || (w * 4 == h * 3);
		case eCubemap_Yes:
			return (w == h * 6);
		case eCubemap_No:
			break;
		}

		return false;
	}

	bool HasPowerOfTwoSizes() const
	{
		uint32 w, h, mips;
		GetExtent(w, h, mips);

		switch (m_eCubemap)
		{
		case eCubemap_UnknownYet:
			if (w * 1 == h * 6)
			{
				return Util::isPowerOfTwo(h / 1);
			}
			if (w * 2 == h * 4)
			{
				return Util::isPowerOfTwo(h / 2);
			}
			if (w * 3 == h * 4)
			{
				return Util::isPowerOfTwo(h / 3);
			}
			if (w * 4 == h * 3)
			{
				return Util::isPowerOfTwo(h / 4);
			}

			break;
		case eCubemap_Yes:
			return Util::isPowerOfTwo(h);
		case eCubemap_No:
			break;
		}

		return Util::isPowerOfTwo(w) && Util::isPowerOfTwo(h);
	}

	EAlphaContent ClassifyAlphaContent() const;

	bool HasNonOpaqueAlpha() const
	{
		const EAlphaContent eAC = CPixelFormats::IsPixelFormatWithoutAlpha(m_pixelFormat) ? eAlphaContent_OnlyWhite : ClassifyAlphaContent();
		return (eAC == eAlphaContent_OnlyBlack || eAC == eAlphaContent_OnlyBlackAndWhite || eAC == eAlphaContent_Greyscale);
	}

	// use mip < 0 for "all mips"
	bool HasSingleColor(int mip, const Vec4& color, float epsilon) const;

	// use when you convert an image to another one
	void CopyPropertiesFrom(const ImageObject& src)
	{
		m_colMinARGB        = src.m_colMinARGB;
		m_colMaxARGB        = src.m_colMaxARGB;
		m_averageBrightness = src.m_averageBrightness;
		m_imageFlags        = src.m_imageFlags;
	}

	void CopyImageFlagsFrom(const ImageObject& src)
	{
		m_imageFlags        = src.m_imageFlags;
	}

	void DropTopMip()
	{
		ImageObject* const pAttached = GetAttachedImage();
		if (pAttached)
		{
			// recursive for all attached images
			pAttached->DropTopMip();
		}

		if (m_mips.size() > 1)
		{
			delete m_mips[0];
			m_mips.erase(m_mips.begin());
		}
	}

	// ---------------------------------------------------------------------------------
	// attached image management (single linked list)

	ImageObject* GetAttachedImage() const
	{
		return m_pAttachedImage;
	}

	void SetAttachedImage(ImageObject* pImage)
	{
		if (m_pAttachedImage != pImage)
		{
			assert(
				(!m_pAttachedImage) || 
				(m_pAttachedImage->GetWidth(0) == GetWidth(0) && m_pAttachedImage->GetHeight(0) == GetHeight(0)));
			delete m_pAttachedImage;
			m_pAttachedImage = pImage;
		}
	}

	void ReplaceAttachedImage(ImageObject* pImage)
	{
		m_pAttachedImage = pImage;
	}

	void ClearAttachedImage()
	{
		m_pAttachedImage = NULL;
	}

	// ---------------------------------------------------------------------------------

	// Compute histogram for BGR8, BGRA8, BGRX8, RGBA32F
	bool ComputeLuminanceHistogramForAnyRGB(Histogram<256>& histogram) const;
	// Compute vector length histogram for BGR8, BGRA8, BGRX8, RGBA32F
	bool ComputeUnitSphereDeviationHistogramForAnyRGB(Histogram<256>& histogram) const;

	// Compute sum of angle differences between original and DXT1 compressed
	float GetDXT1NormalsCompressionError(const CImageProperties* pProps) const;
	// Compute MSE of converting to the format (linear and sRGB)
	void GetGammaCompressionError(EPixelFormat format, const CImageProperties* pProps, float* pErrorLinear, float* pErrorSrgb) const;
	// Compute MSE of 565/DXT1 compressed color-space transforms
	void GetDXT1ColorspaceCompressionError(const CImageProperties* pProps, float(&pError)[20]) const;

	bool IsPerfectGreyscale(ColorF* epsilon = nullptr) const;

	// ---------------------------------------------------------------------------------
	// Tools for BGR8, BGRA8, BGRX8

	// Swizzle channels.
	// 'swizzle' - a 3- or 4-letter string, possible letters are:
	//    r - input red channel,
	//    g - input green channel,
	//    b - input blue channel,
	//    a - input alpha channel,
	//    1 - 1.0,
	//    0 - 0.0
	// Example: 'rr0g' produces image with red channel filled with the source red channel,
	// green channel filled with the source red channel, blue channel filled with
	// zeros, alpha channel filled with the source green channel.
	bool Swizzle(const char* swizzle);

	bool SetConstantAlpha(const int alphaValue);
	bool ThresholdAlpha(const int alphaThresholdValue);

	// ---------------------------------------------------------------------------------
	// Tools for A32B32G32R32F
	void ValidateFloatAlpha() const;
	void ClampMinimumAlpha(float minAlpha);

	void DirectionToAccessibility();
	void ConvertLegacyGloss();
	void GlossFromNormals(bool bHasAuthoredGloss);
	void NormalizeVectors(uint32 firstMip, uint32 maxMipCount);

	// Calculates "(pixel.rgba * scale) + bias"
	void ScaleAndBiasChannels(uint32 firstMip, uint32 maxMipCount, const Vec4& scale, const Vec4& bias);
	// Calculates "clamp(pixel.rgba, min, max)"
	void ClampChannels(uint32 firstMip, uint32 maxMipCount, const Vec4& min, const Vec4& max);

	// Computes the dynamically used range for the texture and expands it to use the
	// full range [0,2^(2^ExponentBits-1)] for better quality.
	void NormalizeImageRange(EColorNormalization eColorNorm, EAlphaNormalization eAlphaNorm, bool bMaintainBlack = false, int nExponentBits = 0);
	// Brings normalized ranges back to it's original range.
	void ExpandImageRange(EColorNormalization eColorNorm, EAlphaNormalization eAlphaNorm, int nExponentBits = 0);

	// Finds and returns max values for r, g, b, a of an A32B32G32R32F image.
	void GetComponentMaxima(ColorF& res) const;

	// Compute luminance into alpha for A32B32G32R32F.
	void GetLuminanceInAlpha();

	// Routines to measure and manipulate alpha coverage
	float ComputeAlphaCoverageScaleFactor(int iMip, const float fDesiredCoverage, const float fAlphaRef) const;
	float ComputeAlphaCoverage(int iMip, const float fAlphaRef) const;

	void TransferAlphaCoverage(const CImageProperties* pProps, const ImageObject* srcImg, int srcMip, int dstMip);

	// Image filtering
	void FilterImage(const CImageProperties* pProps, const ImageObject* srcImg, int srcMip, int dstMip, RECT* srcRect, RECT* dstRect);
	void FilterImage(int filterIndex, int filterOp, float blurH, float blurV, const ImageObject* srcImg, int srcMip, int dstMip, RECT* srcRect, RECT* dstRect);

	// ---------------------------------------------------------------------------------

	// Writes image to disk, overwrites any existing file
	bool SaveImage(const char* filename, bool bForceDX10) const;
	bool LoadImage(const char* filename, bool bForceDX10);

private:
	bool SaveImage(FILE* const out, bool bForceDX10) const;
	bool LoadImage(FILE* const out, bool bForceDX10);

	// extend the DDS format by some user data (we use a magic word to detect that a DDS file was extended)
	bool SaveExtendedData(FILE* f, bool bForceDX10) const;
	bool LoadExtendedData(FILE* f, bool bForceDX10);
};

class ImageToProcess
{
private:
	ImageObject* m_img;

public:
	explicit ImageToProcess(ImageObject* a_img)
	{
		assert(a_img);
		m_img = a_img;
	}

	~ImageToProcess()
	{
		delete m_img;
		m_img = NULL;
	}

	void forget()
	{
		m_img = NULL;
	}
			
	void invalidate()
	{
		if (m_img)
		{
			delete m_img;
			m_img = NULL;
		}
	}

	ImageObject* set(ImageObject* a_img)
	{
		assert(m_img);

		if (m_img != a_img)
		{
			delete m_img;
			m_img = a_img;
		}

		return m_img;
	}

	ImageObject* get() const
	{
		return m_img;
	}

	const ImageObject* getConst() const
	{
		return m_img;
	}

	bool valid() const
	{
		return m_img != NULL;
	}

	enum EQuality
	{
		eQuality_Preview,          // for the 256x256 preview only
		eQuality_Fast,
		eQuality_Normal,
		eQuality_Slow,
	};

private:
	// return values used by the compressor libraries
	enum EResult
	{
		eResult_Failed,            // the compressor can handle the format, but failed (failed allocation, alignment violation, ...)
		eResult_Success,           // the compressor can handle the format, and was successful
		eResult_UnsupportedFormat, // the compressor can't handle the format, another compressor has to take over if possible
	};

	// only called by ConvertFormat()
	void ConvertFormatWithSpecifiedCompressor(const CImageProperties* pProps, EPixelFormat fmtTo, EQuality quality, CImageProperties::EImageCompressor compressor);

	// Convert to/from DirectX BC (4bpp or 8bpp) format
	EResult ConvertFormatWithCTSquisher(const CImageProperties* pProps, EPixelFormat fmtDst, EQuality quality);

	// Convert to/from POWERVR PVRTC (2bpp or 4bpp) format
	EResult ConvertFormatWithPVRTCCompressor(const CImageProperties* pProps, EPixelFormat fmtDst, EQuality quality);

	// Convert to/from ASTC format
	EResult ConvertFormatWithASTCCompressor(const CImageProperties* pProps, EPixelFormat fmtDst, EQuality quality);


public:
	// ---------------------------------------------------------------------------------
	//! can be used to compress, requires a preset
	void ConvertFormat(const CImageProperties* pProps, EPixelFormat fmtTo);
	void ConvertFormat(const CImageProperties* pProps, EPixelFormat fmtTo, EQuality quality);

	void ConvertModel(const CImageProperties* pProps, CImageProperties::EColorModel modelTo);

	// ---------------------------------------------------------------------------------
	// Arguments:
	//   bDeGamma - apply de-gamma correction
	void GammaToLinearRGBA32F(bool bDeGamma);
	void LinearRGBAAnyFToGammaRGBAAnyF();

	// ---------------------------------------------------------------------------------
	// Convert between BGR8, BGRA8, BGRX8, RGBA16, RGBA16H, RGBA32F
	void ConvertBetweenAnyRGB(EPixelFormat dstFormat);
	// Convert between GR8, BGR8, BGRA8, BGRX8, RG16H, RGBA16H, RG32F, RGBA32F
	void ConvertBetweenAnyRG(EPixelFormat dstFormat);

	// Convert between RGBA32F and RGB9E5
	void ConvertBetweenRGB32FAndRGBE(EPixelFormat dstFormat);

	// Convert between RGB and CIE color-spaces, format remains RGBA32F
	void ConvertBetweenRGBA32FAndCIE32F(bool bEncode);

	// Convert between RGB and YCbCr color-spaces, format remains RGBA32F
	void ConvertBetweenRGB32FAndYCbCr32F(bool bEncode);

	// Convert between RGB and YFbFr color-spaces, format remains RGBA32F
	void ConvertBetweenRGB32FAndYFbFr32F(bool bEncode);

	// Convert between RGB and IRB color-spaces, format remains RGBA32F
	void ConvertBetweenRGBA32FAndIRB32F(bool bEncode);

	// Compress RGB to RGBK color-space, format becomes BGRA8
	//! can be used to convert HDR textures to lossy LDR RGBK representation
	void CompressRGB32FToRGBK8(const CImageProperties* pProps, int rgbkMaxValue, bool bDXTCompressedAlpha, int rgbkType);

	// ---------------------------------------------------------------------------------
	// Converters for red-only formats

	void ConvertFromR32FToRGBA32F();
	void ConvertFromRGBA32FToR32F();

	void ConvertFromR16FToRGBA16F();
	void ConvertFromRGBA16FToR16F();

	void ConvertFromR16ToRGBA16();
	void ConvertFromRGBA16ToR16();

	void ConvertFromR8ToRGBA8();
	void ConvertFromRGBA8ToR8();

	// ---------------------------------------------------------------------------------
	// Converters for 8bit formats

	void ConvertFromL8ToXRGB8();
	void ConvertFromA8ToARGB8();
	void ConvertFromAL8ToARGB8();

	// Convert from BGR8, BGRA8, BGRX8 to L8
	void ConvertFromAnyRGB8ToL8();

	// Convert from BGR8, BGRA8, BGRX8 to A8
	void ConvertFromAnyRGB8ToA8();

	// Convert from BGR8, BGRA8, BGRX8 to A8L8
	void ConvertFromAnyRGB8ToAL8();

	// ---------------------------------------------------------------------------------
	// Resizers for A32B32G32R32F
	
	// Image filtering
	void FilterImage(int dstWidth, int dstHeight);

	// Prerequisites: image width is even, ARGB32F only, no mips.
	void DownscaleTwiceHorizontally();

	// Prerequisites: height is even, ARGB32F only, no mips.
	void DownscaleTwiceVertically();

	// Prerequisites: width is pow of 2, ARGB32F only, no mips.
	void UpscalePow2TwiceHorizontally();

	// Prerequisites: height is pow of 2, ARGB32F only, no mips.
	void UpscalePow2TwiceVertically();

	// ---------------------------------------------------------------------------------
	// Tools for A32B32G32R32F

	// input needs to be in range 0..1
	void AddNormalMap(const ImageObject* pAddBump);

	// Arguments:
	//   bValueFromAlpha - true=from alpha, false=value from RGB luminance
	void BumpToNormalMap(
		const CBumpProperties& bumpProperties,
		const bool bValueFromAlpha);

	void CreateHighPass(uint32 dwMipDown);

	void ConvertProbe(bool bPow2);

#if 0
	void NormalToHeight();
#endif

private:
	ImageToProcess(const ImageToProcess&);
	ImageToProcess& operator =(const ImageToProcess&);
};
