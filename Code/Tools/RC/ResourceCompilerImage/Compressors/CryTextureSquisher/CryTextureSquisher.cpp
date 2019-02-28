// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ThreadUtils.h"
#include "MathHelpers.h"
#include "ColorBlockRGBA4x4c.h"
#include "ColorBlockRGBA4x4s.h"
#include "ColorBlockRGBA4x4f.h"
#include "CryTextureSquisher.h"

// we can't build debug-builds with the concurrency-runtime,
// as _CRT_DBG_MALLOC interferes with concurrency-runtime's alloca/freea
#if	!defined(_DEBUG) && !defined(_CRT_DBG_MALLOC)
#pragma warning(push)
#pragma warning(disable: 4355)  // warning C4355: 'this': used in base member initializer list
#include <ppl.h>
#pragma warning(pop)
#define PROCESS_IN_PARALLEL
#endif

static ThreadUtils::CriticalSection s_squishLock;

// preserve the ability to link with the old squish (which is in NvTT)
#define squish	squishccr
#define	SQUISH_USE_CPP
#define	SQUISH_USE_SSE	3
#define SQUISH_USE_XSSE	0
#define	SQUISH_USE_CCR
#include "../../SDKs/squish-ccr/squish.h"
#include "../../SDKs/squish-ccr/squish.inl"

// number of bytes per block per type
#define BLOCKSIZE_BC1   8
#define BLOCKSIZE_BC2   16
#define BLOCKSIZE_BC3   16
#define BLOCKSIZE_BC4   8
#define BLOCKSIZE_BC5   16
#define BLOCKSIZE_BC6   16
#define BLOCKSIZE_BC7   16
#define BLOCKSIZE_CTX1  8
#define BLOCKSIZE_LIMIT 16

#define PTROFFSET_R			0
#define PTROFFSET_G			1
#define PTROFFSET_B			2
#define PTROFFSET_A			3

/* -------------------------------------------------------------------------------------------------------------
 * internal presets
 */
static struct ParameterMatrix
{
	int flagsBaseline;
	int flagsUniform;
	int flagsPerceptual;
	int flagsQuality[CryTextureSquisher::EQualityProfile::eQualityProfile_Num];

	size_t offset;
	bool alphaOnly;
} P2P[] =
{
	// eCompressorPreset_BC1U,
	{
		squish::kBtc1 + squish::kExcludeAlphaFromPalette,
		squish::kColourMetricUniform,
		squish::kColourMetricPerceptual,
		{ squish::kColourRangeFit, squish::kColourClusterFit, squish::kColourIterativeClusterFit, squish::kColourIterativeClusterFit },

		0, false
	},
	// eCompressorPreset_BC2U,
	{
		squish::kBtc2,
		squish::kColourMetricUniform,
		squish::kColourMetricPerceptual,
		{ squish::kColourRangeFit, squish::kColourClusterFit, squish::kColourIterativeClusterFit, squish::kColourIterativeClusterFit },

		0, false
	},
	// eCompressorPreset_BC3U,
	{
		squish::kBtc3,
		squish::kColourMetricUniform,
		squish::kColourMetricPerceptual,
		{ squish::kColourRangeFit, squish::kColourClusterFit + squish::kAlphaIterativeFit, squish::kColourIterativeClusterFit + squish::kAlphaIterativeFit, squish::kColourIterativeClusterFit + squish::kAlphaIterativeFit },

		0, false
	},
	// eCompressorPreset_BC4U,
	{
		squish::kBtc4,
		squish::kColourMetricUniform,
		squish::kColourMetricUniform,
		{ 0, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit },

		PTROFFSET_R, false
	},
	// eCompressorPreset_BC5U,
	{
		squish::kBtc5,
		squish::kColourMetricUniform,
		squish::kColourMetricPerceptual,
		{ 0, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit },

		PTROFFSET_R, false
	},
	// eCompressorPreset_BC6UH,
	{
		squish::kBtc6,
		squish::kColourMetricUniform,
		squish::kColourMetricPerceptual,
		{ squish::kColourRangeFit, squish::kColourRangeFit, squish::kColourRangeFit, squish::kColourRangeFit },

		0, false
	},
	// eCompressorPreset_BC7U,
	{
		squish::kBtc7,
		squish::kColourMetricUniform,
		squish::kColourMetricPerceptual,
		{ squish::kColourRangeFit, squish::kColourRangeFit, squish::kColourClusterFit, squish::kColourIterativeClusterFit },

		0, false
	},

	// eCompressorPreset_BC4S,
	{
		squish::kBtc4 + squish::kSignedInternal,
		squish::kColourMetricUniform,
		squish::kColourMetricUniform,
		{ 0, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit },

		PTROFFSET_R, false
	},
	// eCompressorPreset_BC5S,
	{
		squish::kBtc5 + squish::kSignedInternal,
		squish::kColourMetricUniform,
		squish::kColourMetricPerceptual,
		{ 0, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit },

		PTROFFSET_R, false
	},

	// eCompressorPreset_BC1Un,
	{
		squish::kBtc1 + squish::kExcludeAlphaFromPalette,
		squish::kColourMetricUnit,
		squish::kColourMetricUnit,
		{ squish::kNormalRangeFit, squish::kNormalRangeFit, squish::kNormalRangeFit, squish::kNormalRangeFit },

		0, false
	},
	// eCompressorPreset_BC2Un,
	{
		squish::kBtc2,
		squish::kColourMetricUnit,
		squish::kColourMetricUnit,
		{ squish::kNormalRangeFit, squish::kNormalRangeFit, squish::kNormalRangeFit, squish::kNormalRangeFit },

		0, false
	},
	// eCompressorPreset_BC3Un,
	{
		squish::kBtc3,
		squish::kColourMetricUnit,
		squish::kColourMetricUnit,
		{ squish::kNormalRangeFit, squish::kNormalRangeFit + squish::kAlphaIterativeFit, squish::kNormalRangeFit + squish::kAlphaIterativeFit, squish::kNormalRangeFit + squish::kAlphaIterativeFit },

		0, false
	},
	// eCompressorPreset_BC4Un,
	{
		squish::kBtc4,
		squish::kColourMetricUniform,
		squish::kColourMetricUniform,
		{ 0, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit },

		PTROFFSET_B, false
	},
	// eCompressorPreset_BC5Un,
	{
		squish::kBtc5,
		squish::kColourMetricUnit,
		squish::kColourMetricUnit,
		{ 0, 0, squish::kNormalIterativeFit, squish::kNormalIterativeFit },

		PTROFFSET_R, false
	},
	// eCompressorPreset_BC6UHn,
	{
		squish::kBtc6,
		squish::kColourMetricUnit,
		squish::kColourMetricUnit,
		{ squish::kNormalRangeFit, squish::kNormalRangeFit, squish::kNormalRangeFit, squish::kNormalRangeFit },

		0, false
	},
	// eCompressorPreset_BC7Un,
	{
		squish::kBtc7,
		squish::kColourMetricUnit,
		squish::kColourMetricUnit,
		{ squish::kColourRangeFit, squish::kColourRangeFit, squish::kColourClusterFit, squish::kColourIterativeClusterFit },

		0, false
	},

	// eCompressorPreset_BC4Sn,
	{
		squish::kBtc4 + squish::kSignedInternal,
		squish::kColourMetricUniform,
		squish::kColourMetricUniform,
		{ 0, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit },

		PTROFFSET_B, false
	},
	// eCompressorPreset_BC5Sn,
	{
		squish::kBtc5 + squish::kSignedInternal,
		squish::kColourMetricUnit,
		squish::kColourMetricUnit,
		{ 0, 0, squish::kNormalIterativeFit, squish::kNormalIterativeFit },

		PTROFFSET_R, false
	},

	// eCompressorPreset_BC1Ua,
	{
		squish::kBtc1 + squish::kWeightColourByAlpha,
		squish::kColourMetricUniform,
		squish::kColourMetricPerceptual,
		{ squish::kColourRangeFit, squish::kColourClusterFit, squish::kColourIterativeClusterFit, squish::kColourIterativeClusterFit },

		0, false
	},
	// eCompressorPreset_BC2Ut,
	{
		squish::kBtc2 + squish::kWeightColourByAlpha,
		squish::kColourMetricUniform,
		squish::kColourMetricPerceptual,
		{ squish::kColourRangeFit, squish::kColourClusterFit, squish::kColourIterativeClusterFit, squish::kColourIterativeClusterFit },

		0, false
	},
	// eCompressorPreset_BC3Ut,
	{
		squish::kBtc3 + squish::kWeightColourByAlpha,
		squish::kColourMetricUniform,
		squish::kColourMetricPerceptual,
		{ squish::kColourRangeFit, squish::kColourClusterFit + squish::kAlphaIterativeFit, squish::kColourIterativeClusterFit + squish::kAlphaIterativeFit, squish::kColourIterativeClusterFit + squish::kAlphaIterativeFit },

		0, false
	},
	// eCompressorPreset_BC4Ua,
	{
		squish::kBtc4,
		squish::kColourMetricUniform,
		squish::kColourMetricUniform,
		{ 0, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit },

		PTROFFSET_A, true
	},
	// eCompressorPreset_BC7Ut
	{
		squish::kBtc7 + squish::kWeightColourByAlpha,
		squish::kColourMetricUniform,
		squish::kColourMetricPerceptual,
		{ squish::kColourRangeFit, squish::kColourRangeFit, squish::kColourClusterFit, squish::kColourIterativeClusterFit },

		0, false
	},

	// eCompressorPreset_BC4Sa,
	{
		squish::kBtc4 + squish::kSignedInternal,
		squish::kColourMetricUniform,
		squish::kColourMetricUniform,
		{ 0, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit, squish::kAlphaIterativeFit },

		PTROFFSET_A, true
	},

	// eCompressorPreset_BC7Ug
	{
		squish::kBtc7,
		squish::kColourMetricUniform,
		squish::kColourMetricUniform,
		{ squish::kColourRangeFit, squish::kColourClusterFit, squish::kColourClusterFit * 15, squish::kColourClusterFit * 15 },

		0, false
	},

	// eCompressorPreset_CTX1U
	{
		squish::kCtx1,
		squish::kColourMetricUniform,
		squish::kColourMetricUniform,
		{ squish::kColourRangeFit, squish::kColourClusterFit, squish::kColourIterativeClusterFit, squish::kColourIterativeClusterFit },

		0, false
	},
	// eCompressorPreset_CTX1Un
	{
		squish::kCtx1,
		squish::kColourMetricUnit,
		squish::kColourMetricUnit,
		{ squish::kNormalRangeFit, squish::kNormalRangeFit, squish::kNormalRangeFit, squish::kNormalRangeFit },

		0, false
	},
};

/* -------------------------------------------------------------------------------------------------------------
 * compression functions
 */
void CryTextureSquisher::Compress(const CryTextureSquisher::CompressorParameters& compress)
{
	const unsigned int w = compress.width;
	const unsigned int h = compress.height;
	const size_t offset = P2P[compress.preset].offset;
	int flags = P2P[compress.preset].flagsBaseline + P2P[compress.preset].flagsQuality[compress.quality] +
		(!compress.perceptual ? P2P[compress.preset].flagsUniform : P2P[compress.preset].flagsPerceptual);
	const bool bAlphaOnly = P2P[compress.preset].alphaOnly;
		
	squish::sqio::dtp datatype;
	switch (compress.srcType)
	{
	case eBufferType_sint8:  flags   += squish::kSignedExternal;
	case eBufferType_uint8:  datatype = squish::sqio::dtp::DT_U8;  break;
	case eBufferType_sint16: flags   += squish::kSignedExternal;
	case eBufferType_uint16: datatype = squish::sqio::dtp::DT_U16; break;
	case eBufferType_sfloat: flags   += squish::kSignedExternal;
	case eBufferType_ufloat: datatype = squish::sqio::dtp::DT_F23; break;
	default: __assume(0); break;
	}

	if (compress.perceptual && (flags & squish::kColourMetricPerceptual))
		flags |= squish::kColourMetricCustom;

	const struct squish::sqio sqio = squish::GetSquishIO(w, h, datatype, flags);

	if (compress.perceptual && (flags & squish::kColourMetricPerceptual))
		s_squishLock.Lock();
	if (compress.perceptual && (flags & squish::kColourMetricPerceptual))
		squish::SetWeights(sqio.flags, &compress.weights[0]);

	const unsigned int savedFpeMask = MathHelpers::EnableFloatingPointExceptions(~(_EM_INEXACT | _EM_UNDERFLOW | _EM_OVERFLOW | _EM_INVALID));

	switch (compress.srcType)
	{
	// compress an unsigned 8bit texture --------------------------------------------------
	// compress a signed 8bit texture -----------------------------------------------------
	case eBufferType_uint8:
	case eBufferType_sint8:
		{
#ifdef PROCESS_IN_PARALLEL
			Concurrency::parallel_for(0U, h, 4U, [&,w,h,flags](unsigned int y)
#else
			for (unsigned int y = 0U; y < h; y += 4U)
#endif
			{
				ColorBlockRGBA4x4c srcBlock;
				uint8 dstBlock[BLOCKSIZE_LIMIT];

				uint8* const targetBlock = dstBlock;
				const uint8* const sourceRgba = (const uint8*)srcBlock.colors() + offset;

				for (unsigned int x = 0U; x < w; x += 4U)
				{
					if (!bAlphaOnly)
					{
						srcBlock.setBGRA8(compress.srcBuffer, w,h, compress.pitch, x,y);
					}
					else
					{
						srcBlock.setA8(compress.srcBuffer, w,h, compress.pitch, x,y);
					}

					sqio.encoder(sourceRgba, 0xFFFF, targetBlock, sqio.flags);

					if (compress.userOutputFunction)
					{
						compress.userOutputFunction(compress, targetBlock, sqio.blocksize, y >> 2, x >> 2);
					}
				}
			}
#ifdef PROCESS_IN_PARALLEL
			);
#endif
		}
		break;
	// compress an unsigned 16bit texture -------------------------------------------------
	// compress a signed 16bit texture ----------------------------------------------------
	case eBufferType_uint16:
	case eBufferType_sint16:
		{
#ifdef PROCESS_IN_PARALLEL
			Concurrency::parallel_for(0U, h, 4U, [&,w,h,flags](unsigned int y)
#else
			for (unsigned int y = 0U; y < h; y += 4U)
#endif
			{
				ColorBlockRGBA4x4s srcBlock;
				uint8 dstBlock[BLOCKSIZE_LIMIT];

				uint8* const targetBlock = dstBlock;
				const float* const sourceRgba = (const float*)srcBlock.colors() + offset;

				for (unsigned int x = 0U; x < w; x += 4U)
				{
					if (!bAlphaOnly)
					{
						srcBlock.setBGRA16(compress.srcBuffer, w,h, compress.pitch, x,y);
					}
					else
					{
						srcBlock.setA16(compress.srcBuffer, w,h, compress.pitch, x,y);
					}

					sqio.encoder(sourceRgba, 0xFFFF, targetBlock, sqio.flags);

					if (compress.userOutputFunction)
					{
						compress.userOutputFunction(compress, targetBlock, sqio.blocksize, y >> 2, x >> 2);
					}
				}
			}
#ifdef PROCESS_IN_PARALLEL
			);
#endif
		}
		break;
	// compress an unsigned floating point texture ----------------------------------------
	// compress a signed floating point texture -------------------------------------------
	case eBufferType_ufloat:
	case eBufferType_sfloat:
		{
#ifdef PROCESS_IN_PARALLEL
			Concurrency::parallel_for(0U, h, 4U, [&,w,h,flags](unsigned int y)
#else
			for (unsigned int y = 0U; y < h; y += 4U)
#endif
			{
				ColorBlockRGBA4x4f srcBlock;
				uint8 dstBlock[BLOCKSIZE_LIMIT];

				uint8* const targetBlock = dstBlock;
				const float* const sourceRgba = (const float*)srcBlock.colors() + offset;

				for (unsigned int x = 0U; x < w; x += 4U)
				{
					if (!bAlphaOnly)
					{
						srcBlock.setRGBAf(compress.srcBuffer, w,h, compress.pitch, x,y);
					}
					else
					{
						srcBlock.setAf(compress.srcBuffer, w,h, compress.pitch, x,y);
					}

					sqio.encoder(sourceRgba, 0xFFFF, targetBlock, sqio.flags);

					if (compress.userOutputFunction)
					{
						compress.userOutputFunction(compress, targetBlock, sqio.blocksize, y >> 2, x >> 2);
					}
				}
			}
#ifdef PROCESS_IN_PARALLEL
			);
#endif
		}
		break;
	default:
		assert(0);
		break;
	}

	if (compress.perceptual && (flags & squish::kColourMetricPerceptual))
		s_squishLock.Unlock();

	MathHelpers::EnableFloatingPointExceptions(savedFpeMask);
}

void CryTextureSquisher::Decompress(const DecompressorParameters& decompress)
{
	const unsigned int w = decompress.width;
	const unsigned int h = decompress.height;
	const size_t offset = P2P[decompress.preset].offset;
	int flags = P2P[decompress.preset].flagsBaseline +
		P2P[decompress.preset].flagsUniform;
	const bool bAlphaOnly = P2P[decompress.preset].alphaOnly;

	squish::sqio::dtp datatype;
	switch (decompress.dstType)
	{
	case eBufferType_sint8:  flags   += squish::kSignedExternal;
	case eBufferType_uint8:  datatype = squish::sqio::dtp::DT_U8;  break;
	case eBufferType_sint16: flags   += squish::kSignedExternal;
	case eBufferType_uint16: datatype = squish::sqio::dtp::DT_U16; break;
	case eBufferType_sfloat: flags   += squish::kSignedExternal;
	case eBufferType_ufloat: datatype = squish::sqio::dtp::DT_F23; break;
	default: __assume(0); break;
	}

	const struct squish::sqio sqio = squish::GetSquishIO(w, h, datatype, flags);

	const unsigned int savedFpeMask = MathHelpers::EnableFloatingPointExceptions(~(_EM_INEXACT | _EM_UNDERFLOW | _EM_OVERFLOW | _EM_INVALID));

	switch (decompress.dstType)
	{
	// decompress an unsigned 8bit texture --------------------------------------------------
	// decompress a signed 8bit texture -----------------------------------------------------
	case eBufferType_uint8:
	case eBufferType_sint8:
		{
#ifdef PROCESS_IN_PARALLEL
			Concurrency::parallel_for(0U, h, 4U, [&,w,h,flags](unsigned int y)
#else
			for (unsigned int y = 0U; y < h; y += 4U)
#endif
			{
				uint8 srcBlock[BLOCKSIZE_LIMIT];
				ColorBlockRGBA4x4c dstBlock;

				uint8* const sourceBlock = srcBlock;
				uint8* const targetRgba = (uint8*)dstBlock.colors() + offset;

				for (unsigned int x = 0U; x < w; x += 4U)
				{
					if (decompress.userInputFunction)
					{
						decompress.userInputFunction(decompress, sourceBlock, sqio.blocksize, y >> 2, x >> 2);
					}

					if (!bAlphaOnly)
					{
						dstBlock.setBGRA8(decompress.dstBuffer, w,h, decompress.pitch, x,y);
					}

					sqio.decoder(targetRgba, sourceBlock, sqio.flags);

					if (!bAlphaOnly)
					{
						dstBlock.getBGRA8(decompress.dstBuffer, w,h, decompress.pitch, x,y);
					}
					else
					{
						dstBlock.getA8(decompress.dstBuffer, w,h, decompress.pitch, x,y);
					}
				}
			}
#ifdef PROCESS_IN_PARALLEL
			);
#endif
		}
		break;
		// decompress an unsigned 16bit texture -------------------------------------------------
		// decompress a signed 16bit texture ----------------------------------------------------
	case eBufferType_uint16:
	case eBufferType_sint16:
		{
#ifdef PROCESS_IN_PARALLEL
			Concurrency::parallel_for(0U, h, 4U, [&,w,h,flags](unsigned int y)
#else
			for (unsigned int y = 0U; y < h; y += 4U)
#endif
			{
				uint8 srcBlock[BLOCKSIZE_LIMIT];
				ColorBlockRGBA4x4s dstBlock;

				uint8* const sourceBlock = srcBlock;
				uint16* const targetRgba = (uint16*)dstBlock.colors() + offset;

				for (unsigned int x = 0U; x < w; x += 4U)
				{
					if (decompress.userInputFunction)
					{
						decompress.userInputFunction(decompress, sourceBlock, sqio.blocksize, y >> 2, x >> 2);
					}

					if (!bAlphaOnly)
					{
						dstBlock.setBGRA16(decompress.dstBuffer, w,h, decompress.pitch, x,y);
					}

					sqio.decoder(targetRgba, sourceBlock, sqio.flags);

					if (!bAlphaOnly)
					{
						dstBlock.getBGRA16(decompress.dstBuffer, w,h, decompress.pitch, x,y);
					}
					else
					{
						dstBlock.getA16(decompress.dstBuffer, w,h, decompress.pitch, x,y);
					}
				}
			}
#ifdef PROCESS_IN_PARALLEL
			);
#endif
		}
		break;
		// decompress an unsigned floating point texture ----------------------------------------
		// decompress a signed floating point texture -------------------------------------------
	case eBufferType_ufloat:
	case eBufferType_sfloat:
		{
#ifdef PROCESS_IN_PARALLEL
			Concurrency::parallel_for(0U, h, 4U, [&,w,h,flags](unsigned int y)
#else
			for (unsigned int y = 0U; y < h; y += 4U)
#endif
			{
				uint8 srcBlock[BLOCKSIZE_LIMIT];
				ColorBlockRGBA4x4f dstBlock;

				uint8* const sourceBlock = srcBlock;
				float* const targetRgba = (float*)dstBlock.colors() + offset;

				for (unsigned int x = 0U; x < w; x += 4U)
				{
					if (decompress.userInputFunction)
					{
						decompress.userInputFunction(decompress, sourceBlock, sqio.blocksize, y >> 2, x >> 2);
					}

					if (!bAlphaOnly)
					{
						dstBlock.setRGBAf(decompress.dstBuffer, w,h, decompress.pitch, x,y);
					}

					sqio.decoder(targetRgba, sourceBlock, sqio.flags);

					if (!bAlphaOnly)
					{
						dstBlock.getRGBAf(decompress.dstBuffer, w,h, decompress.pitch, x,y);
					}
					else
					{
						dstBlock.getAf(decompress.dstBuffer, w,h, decompress.pitch, x,y);
					}
				}
			}
#ifdef PROCESS_IN_PARALLEL
			);
#endif
		}
		break;
	default:
		assert(0);
		break;
	}

	MathHelpers::EnableFloatingPointExceptions(savedFpeMask);
}
