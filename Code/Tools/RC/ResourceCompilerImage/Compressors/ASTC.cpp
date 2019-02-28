// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if defined(TOOLS_SUPPORT_ASTC)

#include "MathHelpers.h"
#include "../ImageObject.h"

#undef  IGNORE // Win32 define
#define fmax	ASTCfmax // cmath has _NOEXCEPT
#define fmin	ASTCfmin // cmath has _NOEXCEPT
#include "mathlib.h"
#include "mathlib.cpp"
#include "softfloat.h"
#include "softfloat.cpp"
#define if32 ASTCif32 // two unions
#include "astc_codec_internals.h"
#include "astc_weight_quant_xfer_tables.cpp"
#include "astc_weight_align.cpp"
#include "astc_integer_sequence.cpp"
#define write_bits write_bits01 // two bodies
#define read_bits  read_bits01  // two bodies
#include "astc_compute_variance.cpp"
#include "astc_averages_and_directions.cpp"
//nclude "astc_averages_and_directions_eigenvectors.cpp"
#include "astc_quantization.cpp"
#include "astc_color_quantize.cpp"
#include "astc_color_unquantize.cpp"
#define clamp01 clamp02 // two bodies
#include "astc_partition_tables.cpp"
#include "astc_percentile_tables.cpp"
#include "astc_find_best_partitioning.cpp"
#include "astc_kmeans_partitioning.cpp"
#include "astc_ideal_endpoints_and_weights.cpp"
#include "astc_pick_best_endpoint_format.cpp"
#include "astc_block_sizes2.cpp"
#include "astc_encoding_choice_error.cpp"
#include "astc_symbolic_physical.cpp"
#include "astc_compress_symbolic.cpp"
#include "astc_decompress_symbolic.cpp"
#include "astc_image_load_store.cpp"
#include "stb_image.c"      // dependency pulled in but not needed
#include "astc_ktx_dds.cpp" // dependency pulled in but not needed
#include "astc_stb_tga.cpp" // dependency pulled in but not needed

#ifdef DEBUG_PRINT_DIAGNOSTICS
int print_diagnostics = 0;
#endif

int print_tile_errors = 0;

int perform_srgb_transform = 0;
int rgb_force_use_of_hdr = 0;
int alpha_force_use_of_hdr = 0;

int astc_codec_unlink(const char *filename)
{
	return unlink(filename);
}

void astc_codec_internal_error(const char *filename, int linenum)
{
	printf("Internal error: File=%s Line=%d\n", filename, linenum);
	exit(1);
}

static astc_codec_image* ConvertToASTCImage(int xSize, int ySize, int padding, int bitness, const char* pSrcMem)
{
	assert(bitness == 8 || bitness == 16);

	if (bitness != 8 && bitness != 16)
		return nullptr;

	astc_codec_image* pDstImage = allocate_image(bitness, xSize, ySize, 1, padding);

	if (bitness == 8)
	{
		for (int y = 0; y < ySize; ++y)
		{
			const uint8_t* pSrc = reinterpret_cast<const uint8_t*>(pSrcMem) + 4 * xSize * y;
			uint8_t* pDst = &pDstImage->imagedata8[0][y + padding][4 * padding];

			for (int x = 0; x < xSize; ++x)
			{
				pDst[4 * x + 0] = pSrc[4 * x + 2];
				pDst[4 * x + 1] = pSrc[4 * x + 1];
				pDst[4 * x + 2] = pSrc[4 * x + 0];
				pDst[4 * x + 3] = pSrc[4 * x + 3];
			}
		}
	}
	else
	{
		for (int y = 0; y < ySize; ++y)
		{
			const uint16_t* pSrc = reinterpret_cast<const uint16_t*>(pSrcMem) + 4 * xSize * y;
			uint16_t* pDst = &pDstImage->imagedata16[0][y + padding][4 * padding];

			memcpy(pDst, pSrc, 4 * xSize * sizeof(uint16_t));
		}
	}

	fill_image_padding_area(pDstImage);

	return pDstImage;
}

static bool ConvertFromASTCImage(const astc_codec_image* pSrcImage, char* pDstMem, int bitness)
{
	assert((bitness == 8 && pSrcImage->imagedata8 != nullptr) || (bitness == 16 && pSrcImage->imagedata16 != nullptr));

	if ((bitness != 8 || pSrcImage->imagedata8 == nullptr) && (bitness != 16 || pSrcImage->imagedata16 == nullptr))
		return false;

	const int xSize = pSrcImage->xsize;
	const int ySize = pSrcImage->ysize;

	if (bitness == 8)
	{
		for (int y = 0; y < ySize; ++y)
		{
			const uint8_t* pSrc = &pSrcImage->imagedata8[0][y][0];
			uint8_t* pDst = reinterpret_cast<uint8_t*>(pDstMem) + 4 * xSize * y;

			for (int x = 0; x < xSize; ++x)
			{
				pDst[4 * x + 0] = pSrc[4 * x + 2];
				pDst[4 * x + 1] = pSrc[4 * x + 1];
				pDst[4 * x + 2] = pSrc[4 * x + 0];
				pDst[4 * x + 3] = pSrc[4 * x + 3];
			}
		}
	}
	else
	{
		for (int y = 0; y < ySize; ++y)
		{
			const uint16_t* pSrc = &pSrcImage->imagedata16[0][y][0];
			uint16_t* pDst = reinterpret_cast<uint16_t*>(pDstMem) + 4 * xSize * y;

			memcpy(pDst, pSrc, 4 * xSize * sizeof(uint16_t));
		}
	}

	return true;
}

static void EncodeASTCImage(const astc_codec_image* pSrcImage, int xBlockDim, int yBlockDim, const error_weighting_params* pEwp, astc_decode_mode decodeMode, swizzlepattern swizzle, uint8_t* pDstMem)
{
	const int xSize = pSrcImage->xsize;
	const int ySize = pSrcImage->ysize;
	const int xBlockCount = (xSize + xBlockDim - 1) / xBlockDim;
	const int yBlockCount = (ySize + yBlockDim - 1) / yBlockDim;

	imageblock pb;
	for (int y = 0; y < yBlockCount; ++y)
	{
		for (int x = 0; x < xBlockCount; ++x)
		{
			const int offset = (y * xBlockCount + x) * 16;
			uint8_t* pDst = pDstMem + offset;

			fetch_imageblock(pSrcImage, &pb, xBlockDim, yBlockDim, 1, x * xBlockDim, y * yBlockDim, 0, swizzle);
			symbolic_compressed_block scb;
			compress_symbolic_block(pSrcImage, decodeMode, xBlockDim, yBlockDim, 1, pEwp, &pb, &scb);

			physical_compressed_block pcb;
			pcb = symbolic_to_physical(xBlockDim, yBlockDim, 1, &scb);
			*(physical_compressed_block*)pDst = pcb;
		}
	}
}

static astc_codec_image* DecodeASTCImage(int xSize, int ySize, int bitness, int xBlockDim, int yBlockDim, astc_decode_mode decodeMode, swizzlepattern swizzle, uint8_t* pSrcMem)
{
	assert(bitness == 8 || bitness == 16);

	if (bitness != 8 && bitness != 16)
		return nullptr;

	astc_codec_image* pDstImage = allocate_image(bitness, xSize, ySize, 1, 0);
	initialize_image(pDstImage);

	const int xBlockCount = (xSize + xBlockDim - 1) / xBlockDim;
	const int yBlockCount = (ySize + yBlockDim - 1) / yBlockDim;

	imageblock pb;
	for (int y = 0; y < yBlockCount; ++y)
	{
		for (int x = 0; x < xBlockCount; ++x)
		{
			const int offset = (y * xBlockCount + x) * 16;
			uint8_t* pSrc = pSrcMem + offset;
			physical_compressed_block pcb = *(physical_compressed_block*)pSrc;
			symbolic_compressed_block scb;
			physical_to_symbolic(xBlockDim, yBlockDim, 1, pcb, &scb);
			decompress_symbolic_block(decodeMode, xBlockDim, yBlockDim, 1, x * xBlockDim, y * yBlockDim, 0, &scb, &pb);
			write_imageblock(pDstImage, &pb, xBlockDim, yBlockDim, 1, x * xBlockDim, y * yBlockDim, 0, swizzle);
		}
	}

	return pDstImage;
}

typedef std::unique_ptr<astc_codec_image, void (*)(astc_codec_image*)> AstcImagePtr;

enum EASTCQuality
{
	eASTCQuality_VeryFast,
	eASTCQuality_Fast,
	eASTCQuality_Medium,
	eASTCQuality_Thorough,
	eASTCQuality_Exhaustive,
};

static ImageObject* ConvertFormatToASTC(const CImageProperties* pProps, const ImageObject* pSrcImage, int xBlockDim, int yBlockDim, EPixelFormat fmtDst, EASTCQuality quality)
{
	bool bSrgb = (pSrcImage->GetImageFlags() & CImageExtensionHelper::EIF_SRGBRead) ? true : false;
	bool bNormalMap = false;
	bool bHDR = false;

	std::unique_ptr<ImageObject> pDstImage(pSrcImage->AllocateImage(0, fmtDst, xBlockDim, yBlockDim));
	const uint32 mipCount = pDstImage->GetMipCount();

	const int partitionBits = 10;
	const int partitionCount = 1 << partitionBits;

	error_weighting_params ewp;

	ewp.rgb_power = 1.0f;
	ewp.alpha_power = 1.0f;
	ewp.rgb_base_weight = 1.0f;
	ewp.alpha_base_weight = 1.0f;
	ewp.rgb_mean_weight = 0.0f;
	ewp.rgb_stdev_weight = 0.0f;
	ewp.alpha_mean_weight = 0.0f;
	ewp.alpha_stdev_weight = 0.0f;

	ewp.rgb_mean_and_stdev_mixing = 0.0f;
	ewp.mean_stdev_radius = 0;
	ewp.enable_rgb_scale_with_alpha = 0;
	ewp.alpha_radius = 0;

	ewp.block_artifact_suppression = 0.0f;
	ewp.rgba_weights[0] = 1.0f;
	ewp.rgba_weights[1] = 1.0f;
	ewp.rgba_weights[2] = 1.0f;
	ewp.rgba_weights[3] = 1.0f;
	ewp.ra_normal_angular_scale = 0;

	swizzlepattern swizzle = { 0, 1, 2, 3 };
	astc_decode_mode decodeMode = DECODE_LDR;
	int bitness = 8;

	const Vec3 weights = pProps->GetColorWeights();

	switch (fmtDst)
	{
	case ePixelFormat_ASTC_LDR_L:
		ewp.rgba_weights[0] = 1.0f;
		ewp.rgba_weights[1] = 0.0f;
		ewp.rgba_weights[2] = 0.0f;
		ewp.rgba_weights[3] = 0.0f;

		swizzle = { 0, 0, 0, 5 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_A:
		ewp.rgba_weights[0] = 1.0f;
		ewp.rgba_weights[1] = 0.0f;
		ewp.rgba_weights[2] = 0.0f;
		ewp.rgba_weights[3] = 0.0f;

		swizzle = { 3, 3, 3, 5 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_LA:
		ewp.rgba_weights[0] = 1.0f;
		ewp.rgba_weights[1] = 0.0f;
		ewp.rgba_weights[2] = 0.0f;
		ewp.rgba_weights[3] = 1.0f;

		swizzle = { 0, 0, 0, 3 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_RG:
		ewp.rgba_weights[0] = weights[0];
		ewp.rgba_weights[1] = 0.0f;
		ewp.rgba_weights[2] = 0.0f;
		ewp.rgba_weights[3] = weights[1];

		swizzle = { 0, 0, 0, 1 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_N:
		ewp.rgba_weights[0] = weights[0];
		ewp.rgba_weights[1] = 0.0f;
		ewp.rgba_weights[2] = 0.0f;
		ewp.rgba_weights[3] = weights[1];

		bNormalMap = true;
		swizzle = { 0, 0, 0, 1 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_RGB:
		ewp.rgba_weights[0] = weights[0];
		ewp.rgba_weights[1] = weights[1];
		ewp.rgba_weights[2] = weights[2];
		ewp.rgba_weights[3] = 0.0f;

		swizzle = { 0, 1, 2, 5 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_RGBA:
		ewp.rgba_weights[0] = weights[0];
		ewp.rgba_weights[1] = weights[1];
		ewp.rgba_weights[2] = weights[2];
		ewp.rgba_weights[3] = (weights[0] + weights[1] + weights[2]) / 3.0f;

		swizzle = { 0, 1, 2, 3 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_HDR_L:
		ewp.rgba_weights[0] = 1.0f;
		ewp.rgba_weights[1] = 0.0f;
		ewp.rgba_weights[2] = 0.0f;
		ewp.rgba_weights[3] = 0.0f;

		bHDR = true;
		swizzle = { 0, 0, 0, 5 };
		decodeMode = DECODE_HDR;
		bitness = 16;
		break;
	case ePixelFormat_ASTC_HDR_RGB:
		ewp.rgba_weights[0] = weights[0];
		ewp.rgba_weights[1] = weights[1];
		ewp.rgba_weights[2] = weights[2];
		ewp.rgba_weights[3] = 0.0f;

		bHDR = true;
		swizzle = { 0, 1, 2, 5 };
		decodeMode = DECODE_HDR;
		bitness = 16;
		break;
	case ePixelFormat_ASTC_HDR_RGBA:
		ewp.rgba_weights[0] = weights[0];
		ewp.rgba_weights[1] = weights[1];
		ewp.rgba_weights[2] = weights[2];
		ewp.rgba_weights[3] = (weights[0] + weights[1] + weights[2]) / 3.0f;

		bHDR = true;
		swizzle = { 0, 1, 2, 3 };
		decodeMode = DECODE_HDR;
		bitness = 16;
		break;
	}

	const float log10Texels = log((float)(xBlockDim * yBlockDim)) / log(10.0f);
	float dbLimit;

	switch (quality)
	{
	case eASTCQuality_VeryFast:
		ewp.partition_search_limit = 2;
		ewp.block_mode_cutoff = 0.25f;

		dbLimit = std::max(70 - 35 * log10Texels, 53 - 19 * log10Texels);

		ewp.partition_1_to_2_limit = 1.0;
		ewp.lowest_correlation_cutoff = 0.5;
		ewp.max_refinement_iters = 1;
		break;
	case eASTCQuality_Fast:
		ewp.partition_search_limit = 4;
		ewp.block_mode_cutoff = 0.5f;

		dbLimit = std::max(85 - 35 * log10Texels, 63 - 19 * log10Texels);

		ewp.partition_1_to_2_limit = 1.0;
		ewp.lowest_correlation_cutoff = 0.5;
		ewp.max_refinement_iters = 1;
		break;
	case eASTCQuality_Medium:
		ewp.partition_search_limit = 25;
		ewp.block_mode_cutoff = 0.75f;

		dbLimit = std::max(95 - 35 * log10Texels, 70 - 19 * log10Texels);

		ewp.partition_1_to_2_limit = 1.2f;
		ewp.lowest_correlation_cutoff = 0.75f;
		ewp.max_refinement_iters = 2;
		break;
	case eASTCQuality_Thorough:
		ewp.partition_search_limit = 100;
		ewp.block_mode_cutoff = 0.95f;

		dbLimit = std::max(105 - 35 * log10Texels, 77 - 19 * log10Texels);

		ewp.partition_1_to_2_limit = 2.5f;
		ewp.lowest_correlation_cutoff = 0.95f;
		ewp.max_refinement_iters = 4;
		break;
	case eASTCQuality_Exhaustive:
		ewp.partition_search_limit = partitionCount;
		ewp.block_mode_cutoff = 1.0f;

		dbLimit = 999.0f;

		ewp.partition_1_to_2_limit = 1000.0f;
		ewp.lowest_correlation_cutoff = 0.99f;
		ewp.max_refinement_iters = 4;
		break;
	}

	if (bNormalMap)
	{
		ewp.ra_normal_angular_scale = 1;
		ewp.block_artifact_suppression = 1.8f;
		ewp.mean_stdev_radius = 3;
		ewp.rgb_mean_weight = 0;
		ewp.rgb_stdev_weight = 50;
		ewp.rgb_mean_and_stdev_mixing = 0.0;
		ewp.alpha_mean_weight = 0;
		ewp.alpha_stdev_weight = 50;

		dbLimit = 999.0f;

		ewp.partition_1_to_2_limit = 1000.0f;
		ewp.lowest_correlation_cutoff = 0.99f;
	}

	ewp.texel_avg_error_limit = pow(0.1f, dbLimit * 0.1f) * 65535.0f * 65535.0f;

	if (bHDR)
	{
		rgb_force_use_of_hdr = 1;
		alpha_force_use_of_hdr = 1;

		ewp.mean_stdev_radius = 0;
		ewp.rgb_power = 0.75;
		ewp.rgb_base_weight = 0;
		ewp.rgb_mean_weight = 1;
		ewp.alpha_power = 0.75;
		ewp.alpha_base_weight = 0;
		ewp.alpha_mean_weight = 1;
		ewp.texel_avg_error_limit = 0.0f;
	}

	// Specifying the error weight of a color component as 0 is not allowed.
	// If weights are 0, then they are instead set to a small positive value.
	float maxColorComponentWeight = std::max(std::max(ewp.rgba_weights[0], ewp.rgba_weights[1]), std::max(ewp.rgba_weights[2], ewp.rgba_weights[3]));
	ewp.rgba_weights[0] = std::max(ewp.rgba_weights[0], maxColorComponentWeight / 1000.0f);
	ewp.rgba_weights[1] = std::max(ewp.rgba_weights[1], maxColorComponentWeight / 1000.0f);
	ewp.rgba_weights[2] = std::max(ewp.rgba_weights[2], maxColorComponentWeight / 1000.0f);
	ewp.rgba_weights[3] = std::max(ewp.rgba_weights[3], maxColorComponentWeight / 1000.0f);

	expand_block_artifact_suppression(xBlockDim, yBlockDim, 1, &ewp);

	const int padding = std::max(ewp.mean_stdev_radius, ewp.alpha_radius);

	for (uint32 mip = 0; mip < mipCount; ++mip)
	{
		const uint32 mipWidth = pSrcImage->GetWidth(mip);
		const uint32 mipHeight = pSrcImage->GetHeight(mip);

		const int xBlockCount = (mipWidth + xBlockDim - 1) / xBlockDim;
		const int yBlockCount = (mipHeight + yBlockDim - 1) / yBlockDim;
		const uint32 compressedDataSize = xBlockCount * yBlockCount * 16;

		if (pDstImage->GetMipDataSize(mip) != compressedDataSize)
		{
			RCLogError("%s : Compressed image data size mismatch while using ASTC Lib. Inform an RC programmer.", __FUNCTION__);
			return nullptr;
		}

		char* pSrcMem;
		uint32 srcPitch;
		pSrcImage->GetImagePointer(mip, pSrcMem, srcPitch);

		const AstcImagePtr astcImage(ConvertToASTCImage(mipWidth, mipHeight, padding, bitness, pSrcMem), destroy_image);

		if (padding > 0 || ewp.rgb_mean_weight != 0.0f || ewp.rgb_stdev_weight != 0.0f || ewp.alpha_mean_weight != 0.0f || ewp.alpha_stdev_weight != 0.0f)
		{
			compute_averages_and_variances(astcImage.get(), ewp.rgb_power, ewp.alpha_power, ewp.mean_stdev_radius, ewp.alpha_radius, swizzle);
		}

		char* pDstMem;
		uint32 dstPitch;
		pDstImage->GetImagePointer(mip, pDstMem, dstPitch);

		EncodeASTCImage(astcImage.get(), xBlockDim, yBlockDim, &ewp, decodeMode, swizzle, reinterpret_cast<uint8_t*>(pDstMem));
	}

	return pDstImage.release();
}

static ImageObject* ConvertFormatFromASTC(const ImageObject* pSrcImage, int xBlockDim, int yBlockDim, EPixelFormat fmtDst)
{
	bool bSrgb = (pSrcImage->GetImageFlags() & CImageExtensionHelper::EIF_SRGBRead) ? true : false;

	swizzlepattern swizzle = { 0, 1, 2, 3 };
	astc_decode_mode decodeMode = DECODE_LDR;
	int bitness = 8;

	switch (pSrcImage->GetPixelFormat())
	{
	case ePixelFormat_ASTC_LDR_L:
		swizzle = { 0, 0, 0, 5 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_A:
		swizzle = { 4, 4, 4, 0 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_LA:
		swizzle = { 0, 0, 0, 3 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_RG:
		swizzle = { 0, 3, 4, 5 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_N:
		swizzle = { 0, 3, 6, 5 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_RGB:
		swizzle = { 0, 1, 2, 5 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_LDR_RGBA:
		swizzle = { 0, 1, 2, 3 };
		decodeMode = bSrgb ? DECODE_LDR_SRGB : DECODE_LDR;
		bitness = 8;
		break;
	case ePixelFormat_ASTC_HDR_L:
		swizzle = { 0, 0, 0, 5 };
		decodeMode = DECODE_HDR;
		bitness = 16;
		break;
	case ePixelFormat_ASTC_HDR_RGB:
		swizzle = { 0, 1, 2, 5 };
		decodeMode = DECODE_HDR;
		bitness = 16;
		break;
	case ePixelFormat_ASTC_HDR_RGBA:
		swizzle = { 0, 1, 2, 3 };
		decodeMode = DECODE_HDR;
		bitness = 16;
		break;
	}

	std::unique_ptr<ImageObject> pDstImage(pSrcImage->AllocateImage(0, fmtDst));
	const uint32 mipCount = pDstImage->GetMipCount();

	for (uint32 mip = 0; mip < mipCount; ++mip)
	{
		const uint32 mipWidth = pSrcImage->GetWidth(mip);
		const uint32 mipHeight = pSrcImage->GetHeight(mip);

		char* pSrcMem;
		uint32 srcPitch;
		pSrcImage->GetImagePointer(mip, pSrcMem, srcPitch);

		const AstcImagePtr astcImage(DecodeASTCImage(mipWidth, mipHeight, bitness, xBlockDim, yBlockDim, decodeMode, swizzle, reinterpret_cast<uint8_t*>(pSrcMem)), destroy_image);

		char* pDstMem;
		uint32 dstPitch;
		pDstImage->GetImagePointer(mip, pDstMem, dstPitch);

		ConvertFromASTCImage(astcImage.get(), pDstMem, bitness);
	}

	return pDstImage.release();
}

ImageToProcess::EResult ImageToProcess::ConvertFormatWithASTCCompressor(const CImageProperties* pProps, EPixelFormat fmtDst, EQuality quality)
{
	MathHelpers::AutoFloatingPointExceptions autoFpe(~(_EM_INEXACT | _EM_UNDERFLOW | _EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INVALID));

	const EPixelFormat fmtSrc = get()->GetPixelFormat();

	if (fmtSrc == fmtDst)
	{
		return eResult_Success;
	}

	const bool bSrcCompressed = (fmtSrc >= ePixelFormat_ASTC_LDR_L && fmtSrc <= ePixelFormat_ASTC_HDR_RGBA);
	const bool bDstCompressed = (fmtDst >= ePixelFormat_ASTC_LDR_L && fmtDst <= ePixelFormat_ASTC_HDR_RGBA);

	const bool bSrcSimple = (fmtSrc == ePixelFormat_A8R8G8B8) || (fmtSrc == ePixelFormat_A16B16G16R16F);
	const bool bDstSimple = (fmtDst == ePixelFormat_A8R8G8B8) || (fmtDst == ePixelFormat_A16B16G16R16F);

	const bool bDecode = bSrcCompressed && bDstSimple;
	const bool bEncode = bSrcSimple && bDstCompressed;

	assert(!(bEncode && bDecode));
	if (!(bEncode || bDecode))
	{
		return eResult_UnsupportedFormat;
	}

	int xBlockDim = 4;
	int yBlockDim = 4;

	const string blockSize = pProps->GetConfigAsString("blocksize", "", "");

	if (sscanf(blockSize.data(), "%dx%d", &xBlockDim, &yBlockDim) != 2)
	{
		xBlockDim = 4;
		yBlockDim = 4;
	}

	if (!((xBlockDim ==  4 && yBlockDim ==  4) ||
		  (xBlockDim ==  5 && yBlockDim ==  4) ||
		  (xBlockDim ==  5 && yBlockDim ==  5) ||
		  (xBlockDim ==  6 && yBlockDim ==  5) ||
		  (xBlockDim ==  6 && yBlockDim ==  6) ||
		  (xBlockDim ==  8 && yBlockDim ==  5) ||
		  (xBlockDim ==  8 && yBlockDim ==  6) ||
		  (xBlockDim ==  8 && yBlockDim ==  8) ||
		  (xBlockDim == 10 && yBlockDim ==  5) ||
		  (xBlockDim == 10 && yBlockDim ==  6) ||
		  (xBlockDim == 10 && yBlockDim ==  8) ||
		  (xBlockDim == 10 && yBlockDim == 10) ||
		  (xBlockDim == 12 && yBlockDim == 10) ||
		  (xBlockDim == 12 && yBlockDim == 12)))
	{
		xBlockDim = 4;
		yBlockDim = 4;
	}

	prepare_angular_tables();
	build_quantization_mode_table();
	get_block_size_descriptor(xBlockDim, yBlockDim, 1);
	get_partition_table(xBlockDim, yBlockDim, 1, 0);

	if (bEncode)
	{
		EASTCQuality astcQuality;

		switch (quality)
		{
		case ImageToProcess::eQuality_Preview:
			astcQuality = eASTCQuality_VeryFast;
			break;
		case ImageToProcess::eQuality_Fast:
			astcQuality = eASTCQuality_Fast;
			break;
		case ImageToProcess::eQuality_Normal:
			astcQuality = eASTCQuality_Medium;
			break;
		case ImageToProcess::eQuality_Slow:
			astcQuality = eASTCQuality_Exhaustive;
			break;
		}

		set(ConvertFormatToASTC(pProps, get(), xBlockDim, yBlockDim, fmtDst, astcQuality));

		return valid() ? eResult_Success : eResult_Failed;
	}
	else
	{
		set(ConvertFormatFromASTC(get(), xBlockDim, yBlockDim, fmtDst));

		return valid() ? eResult_Success : eResult_Failed;
	}

	return eResult_Success;
}

#endif
