// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <assert.h>                         // assert()
#include <algorithm>                        // std::max()

#include "../ImageCompiler.h"               // CImageCompiler
#include "../ImageObject.h"                 // ImageToProcess

#include "IRCLog.h"                         // IRCLog
#include "MathHelpers.h"                    // MathHelpers

#include "CryTextureSquisher/CryTextureSquisher.h"

///////////////////////////////////////////////////////////////////////////////////

// callbacks for the CryTextureSquisher

namespace 
{

struct CrySquisherCallbackUserData
{
	ImageObject* m_pImageObject;
	char* m_dstMem;
	uint32 m_dstOffset;
};

void CrySquisherOutputCallback(const CryTextureSquisher::CompressorParameters& compress, const void* data, uint size, uint oy, uint ox)
{
	CrySquisherCallbackUserData* const pUserData = static_cast<CrySquisherCallbackUserData*>(compress.userPtr);
	uint stride = (compress.width + 3) >> 2;
	uint blocks = (compress.height + 3) >> 2;

	assert(CPixelFormats::GetPixelFormatInfo(pUserData->m_pImageObject->GetPixelFormat())->bCompressed);

	memcpy(((char*)pUserData->m_dstMem) + size * (stride * oy + ox), data, size);

	pUserData->m_dstOffset = size * (stride * blocks);
}

void CrySquisherInputCallback(const CryTextureSquisher::DecompressorParameters& decompress, void* data, uint size, uint oy, uint ox)
{
	CrySquisherCallbackUserData* const pUserData = static_cast<CrySquisherCallbackUserData*>(decompress.userPtr);
	uint stride = (decompress.width + 3) >> 2;
	uint blocks = (decompress.height + 3) >> 2;

	assert(CPixelFormats::GetPixelFormatInfo(pUserData->m_pImageObject->GetPixelFormat())->bCompressed);

	memcpy(data, ((char*)pUserData->m_dstMem) + size * (stride * oy + ox), size);

	pUserData->m_dstOffset = size * (stride * blocks);
}

} // namespace

///////////////////////////////////////////////////////////////////////////////////

ImageToProcess::EResult ImageToProcess::ConvertFormatWithCTSquisher(const CImageProperties* pProps, EPixelFormat fmtDst, EQuality quality)
{
	{
		EPixelFormat fmtSrc = get()->GetPixelFormat();

		const bool bSrcCompressed = 
			(fmtSrc == ePixelFormat_DXT1) || (fmtSrc == ePixelFormat_DXT1a) || (fmtSrc == ePixelFormat_DXT3) || (fmtSrc == ePixelFormat_DXT3t) || (fmtSrc == ePixelFormat_DXT5) || (fmtSrc == ePixelFormat_DXT5t) || 
			(fmtSrc == ePixelFormat_3DCp) || (fmtSrc == ePixelFormat_3DC) || 
			(fmtSrc == ePixelFormat_BC1) || (fmtSrc == ePixelFormat_BC1a) || (fmtSrc == ePixelFormat_BC2) || (fmtSrc == ePixelFormat_BC2t) || (fmtSrc == ePixelFormat_BC3) || (fmtSrc == ePixelFormat_BC3t) || 
			(fmtSrc == ePixelFormat_BC4) || (fmtSrc == ePixelFormat_BC4s) || (fmtSrc == ePixelFormat_BC5) || (fmtSrc == ePixelFormat_BC5s) || 
			(fmtSrc == ePixelFormat_BC6UH) || 
			(fmtSrc == ePixelFormat_BC7) || (fmtSrc == ePixelFormat_BC7t) || 
			(fmtSrc == ePixelFormat_CTX1);
		const bool bDstCompressed = 
			(fmtDst == ePixelFormat_DXT1) || (fmtDst == ePixelFormat_DXT1a) || (fmtDst == ePixelFormat_DXT3) || (fmtDst == ePixelFormat_DXT3t) || (fmtDst == ePixelFormat_DXT5) || (fmtDst == ePixelFormat_DXT5t) || 
			(fmtDst == ePixelFormat_3DCp) || (fmtDst == ePixelFormat_3DC) || 
			(fmtDst == ePixelFormat_BC1) || (fmtDst == ePixelFormat_BC1a) || (fmtDst == ePixelFormat_BC2) || (fmtDst == ePixelFormat_BC2t) || (fmtDst == ePixelFormat_BC3) || (fmtDst == ePixelFormat_BC3t) || 
			(fmtDst == ePixelFormat_BC4) || (fmtDst == ePixelFormat_BC4s) || (fmtDst == ePixelFormat_BC5) || (fmtDst == ePixelFormat_BC5s) || 
			(fmtDst == ePixelFormat_BC6UH) || 
			(fmtDst == ePixelFormat_BC7) || (fmtDst == ePixelFormat_BC7t)|| 
			(fmtDst == ePixelFormat_CTX1);

		const bool bSrcSimple = (fmtSrc == ePixelFormat_A8) || (fmtSrc == ePixelFormat_A8R8G8B8) || (fmtSrc == ePixelFormat_X8R8G8B8) || (fmtSrc == ePixelFormat_R32F) || (fmtSrc == ePixelFormat_A32B32G32R32F);
		const bool bDstSimple = (fmtDst == ePixelFormat_A8) || (fmtDst == ePixelFormat_A8R8G8B8) || (fmtDst == ePixelFormat_X8R8G8B8) || (fmtDst == ePixelFormat_R32F) || (fmtDst == ePixelFormat_A32B32G32R32F);

		const bool bDecode = bSrcCompressed && bDstSimple;
		const bool bEncode = bSrcSimple && bDstCompressed;

		assert(!(bEncode && bDecode));
		if (!(bEncode || bDecode))
		{
			return eResult_UnsupportedFormat;
		}

		std::auto_ptr<ImageObject> pRet(get()->AllocateImage(0, fmtDst));

		// CTsquish operates on native normal vectors when floating-point
		// buffers are used. Revert bias and scale when passing a normal-map.
		// Also take care range is clamped to valid values.
		if (bEncode)
		{
			if (fmtSrc == ePixelFormat_A32B32G32R32F)
			{
				const uint32 nMips = get()->GetMipCount();

				// NOTES:
				// - all incoming images are unsigned, even normal maps
				// - all mipmaps of incoming images can contain out-of-range values from mipmap filtering
				// - 3Dc/BC5 is synonymous with "is a normal map" because they are not tagged explicitly as such
				if (fmtDst == ePixelFormat_3DC || fmtDst == ePixelFormat_BC5 || fmtDst == ePixelFormat_BC5s)
				{
					get()->ScaleAndBiasChannels(0, nMips,
						Vec4( 2.0f,  2.0f,  2.0f,  1.0f),
						Vec4(-1.0f, -1.0f, -1.0f,  0.0f));
					get()->ClampChannels(0, nMips,
						Vec4(-1.0f, -1.0f, -1.0f, -1.0f),
						Vec4( 1.0f,  1.0f,  1.0f,  1.0f));
				}
				else if (fmtDst == ePixelFormat_BC6UH)
				{
					get()->ClampChannels(0, nMips,
						Vec4(   0.0f,    0.0f,    0.0f,    0.0f),
						Vec4(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX));
				}
				else
				{
					get()->ClampChannels(0, nMips,
						Vec4( 0.0f,  0.0f,  0.0f,  0.0f),
						Vec4( 1.0f,  1.0f,  1.0f,  1.0f));
				}
			}
		}

		if (bEncode)
		{
			// Compressing
			assert(bSrcSimple);      // A8R8G8B8 or X8R8G8B8
			assert(bDstCompressed);	 // DXT1, DXT1a, DXT3, DXT5, ...

			const uint32 mipCount = pRet->GetMipCount();
			for(uint32 dwMip = 0; dwMip < mipCount; ++dwMip)
			{
				uint32 dwLocalWidth = get()->GetWidth(dwMip);
				uint32 dwLocalHeight = get()->GetHeight(dwMip);

				char *pSrcMem;
				uint32 dwSrcPitch;
				get()->GetImagePointer(dwMip, pSrcMem, dwSrcPitch);

				char *pDstMem;
				uint32 dwDstPitch;
				pRet->GetImagePointer(dwMip, pDstMem, dwDstPitch);

				{
					CrySquisherCallbackUserData userData;
					userData.m_pImageObject = pRet.get();
					userData.m_dstOffset = 0;
					userData.m_dstMem = pDstMem;

					CryTextureSquisher::CompressorParameters compress;

					compress.srcBuffer = pSrcMem;
					compress.width = dwLocalWidth;
					compress.height = dwLocalHeight;
					compress.pitch = dwSrcPitch;

					compress.srcType = (CPixelFormats::IsFormatFloatingPoint(fmtSrc, true) ? CryTextureSquisher::eBufferType_ufloat : CryTextureSquisher::eBufferType_uint8);
					if (CPixelFormats::IsFormatSigned(fmtDst))
					{
						compress.srcType = (compress.srcType == CryTextureSquisher::eBufferType_ufloat ? CryTextureSquisher::eBufferType_sfloat : CryTextureSquisher::eBufferType_sint8);
					}

					const Vec3 uniform = pProps->GetUniformColorWeights();
					const Vec3 weights = pProps->GetColorWeights();
					switch (get()->GetImageFlags() & CImageExtensionHelper::EIF_Colormodel)
					{
					case CImageExtensionHelper::EIF_Colormodel_RGB:
						compress.weights[0] = weights.x;
						compress.weights[1] = weights.y;
						compress.weights[2] = weights.z;
						break;
					case CImageExtensionHelper::EIF_Colormodel_CIE:
						compress.weights[0] = weights.y + (weights.x + weights.z);
						compress.weights[1] = weights.x;
						compress.weights[2] = weights.z;
						break;
					case CImageExtensionHelper::EIF_Colormodel_IRB:
						compress.weights[0] = weights.y + max(weights.x, weights.z);
						compress.weights[1] = weights.x;
						compress.weights[2] = weights.z;
						break;
					case CImageExtensionHelper::EIF_Colormodel_YCC:
						compress.weights[0] = 0.125f;
						compress.weights[1] = 0.750f;
						compress.weights[2] = 0.125f;
						break;
					case CImageExtensionHelper::EIF_Colormodel_YFF:
						compress.weights[0] = 0.125f;
						compress.weights[1] = 0.750f;
						compress.weights[2] = 0.125f;
						break;
					}

					if (pProps->GetNormalizeRange())
					{
						Vec4 minColor, maxColor, rngColor;
						pRet->GetColorRange(minColor, maxColor);

						rngColor.x = maxColor.x - minColor.x;
						rngColor.y = maxColor.y - minColor.y;
						rngColor.z = maxColor.z - minColor.z;
						rngColor.w = std::max(rngColor.x, std::max(rngColor.y, rngColor.z));
						rngColor.x = rngColor.x / rngColor.w;
						rngColor.y = rngColor.y / rngColor.w;
						rngColor.z = rngColor.z / rngColor.w;

						compress.weights[0] *= rngColor.x;
						compress.weights[1] *= rngColor.y;
						compress.weights[2] *= rngColor.z;
					}

					compress.perceptual = 
						(compress.weights[0] != uniform.x) || 
						(compress.weights[1] != uniform.y) ||
						(compress.weights[2] != uniform.z);
					compress.quality = 
						(quality == eQuality_Preview ? CryTextureSquisher::eQualityProfile_Low : 
						(quality == eQuality_Fast    ? CryTextureSquisher::eQualityProfile_Low : 
						(quality == eQuality_Slow    ? CryTextureSquisher::eQualityProfile_High : 
						                               CryTextureSquisher::eQualityProfile_Medium)));

					compress.userPtr = &userData;
					compress.userOutputFunction = CrySquisherOutputCallback;

					switch (fmtDst)
					{
					case ePixelFormat_DXT1: 
					case ePixelFormat_BC1:
						compress.preset = CryTextureSquisher::eCompressorPreset_BC1U;
						break;
					case ePixelFormat_DXT1a: 
					case ePixelFormat_BC1a: 
						compress.preset = CryTextureSquisher::eCompressorPreset_BC1Ua;
						break;
					case ePixelFormat_DXT3: 
					case ePixelFormat_BC2: 
						compress.preset = CryTextureSquisher::eCompressorPreset_BC2U;
						break;
					case ePixelFormat_DXT3t: 
					case ePixelFormat_BC2t: 
						compress.preset = CryTextureSquisher::eCompressorPreset_BC2Ut;
						break;
					case ePixelFormat_DXT5: 
					case ePixelFormat_BC3: 
						compress.preset = CryTextureSquisher::eCompressorPreset_BC3U;
						break;
					case ePixelFormat_DXT5t: 
					case ePixelFormat_BC3t: 
						compress.preset = CryTextureSquisher::eCompressorPreset_BC3Ut;
						break;
					case ePixelFormat_3DCp: 
					case ePixelFormat_BC4:
						compress.preset = (CPixelFormats::IsFormatSingleChannel(fmtSrc)
							? CryTextureSquisher::eCompressorPreset_BC4Ua    // a-channel
							: CryTextureSquisher::eCompressorPreset_BC4U);   // r-channel
						break;
					case ePixelFormat_BC4s:
						compress.preset = (CPixelFormats::IsFormatSingleChannel(fmtSrc)
							? CryTextureSquisher::eCompressorPreset_BC4Sa    // a-channel
							: CryTextureSquisher::eCompressorPreset_BC4S);   // r-channel
						break;
					case ePixelFormat_3DC: 
					case ePixelFormat_BC5:
						compress.preset = CryTextureSquisher::eCompressorPreset_BC5Un;
						break;
					case ePixelFormat_BC5s:
						compress.preset = CryTextureSquisher::eCompressorPreset_BC5Sn;
						break;
					case ePixelFormat_BC6UH:
						compress.preset = CryTextureSquisher::eCompressorPreset_BC6UH;
						break;
					case ePixelFormat_BC7: 
						compress.preset = CryTextureSquisher::eCompressorPreset_BC7U;
						break;
					case ePixelFormat_BC7t: 
						compress.preset = CryTextureSquisher::eCompressorPreset_BC7Ut;
						break;
					default:
						RCLogError("%s: Unexpected pixel format (in compressing an image). Inform an RC programmer.", __FUNCTION__);
						set(0);
						return eResult_Failed;
					}

					CryTextureSquisher::Compress(compress); 

					assert(userData.m_dstOffset == dwDstPitch * ((dwLocalHeight + 3) / 4));
				}
			} // for: all mips
		}
		else
		{
			// Decompressing
			assert(bSrcCompressed);	 // DXT1, DXT1a, DXT3, DXT5, ...
			assert(bDstSimple);      // A8R8G8B8 or X8R8G8B8

			// Clear() before decompressing to provide valid values for skipped values
			pRet->ClearImage();

			int dwOutPlanes = -1;
			CPixelFormats::GetPixelFormatInfo(fmtDst, 0, &dwOutPlanes, 0);
			assert(dwOutPlanes == 4 || dwOutPlanes == 1);

			const uint32 mipCount = pRet->GetMipCount();
			for (uint32 dwMip = 0; dwMip < mipCount; ++dwMip)
			{
				const uint32 dwLocalWidth = pRet->GetWidth(dwMip);
				const uint32 dwLocalHeight = pRet->GetHeight(dwMip);

				const uint32 blockRows = (dwLocalHeight + 3) / 4;
				const uint32 blockColumns = (dwLocalWidth + 3) / 4;

				char *pSrcMem;
				uint32 dwSrcPitch;
				get()->GetImagePointer(dwMip, pSrcMem, dwSrcPitch);

				char *pDstMem;
				uint32 dwDstPitch;
				pRet->GetImagePointer(dwMip, pDstMem, dwDstPitch);

				{
					CrySquisherCallbackUserData userData;
					userData.m_pImageObject = get();
					userData.m_dstOffset = 0;
					userData.m_dstMem = pSrcMem;

					CryTextureSquisher::DecompressorParameters decompress;

					decompress.dstBuffer = pDstMem;
					decompress.width = dwLocalWidth;
					decompress.height = dwLocalHeight;
					decompress.pitch = dwDstPitch;

					decompress.dstType = (CPixelFormats::IsFormatFloatingPoint(fmtDst, true) ? CryTextureSquisher::eBufferType_ufloat : CryTextureSquisher::eBufferType_uint8);
					if (CPixelFormats::IsFormatSigned(fmtSrc))
					{
						decompress.dstType = (decompress.dstType == CryTextureSquisher::eBufferType_ufloat ? CryTextureSquisher::eBufferType_sfloat : CryTextureSquisher::eBufferType_sint8);
					}

					decompress.userPtr = &userData;
					decompress.userInputFunction = CrySquisherInputCallback;

					switch (fmtSrc)
					{
					case ePixelFormat_DXT1: 
					case ePixelFormat_BC1:
						decompress.preset = CryTextureSquisher::eCompressorPreset_BC1U;
						break;
					case ePixelFormat_DXT1a: 
					case ePixelFormat_BC1a: 
						decompress.preset = CryTextureSquisher::eCompressorPreset_BC1Ua;
						break;
					case ePixelFormat_DXT3: 
					case ePixelFormat_BC2: 
						decompress.preset = CryTextureSquisher::eCompressorPreset_BC2U;
						break;
					case ePixelFormat_DXT3t: 
					case ePixelFormat_BC2t: 
						decompress.preset = CryTextureSquisher::eCompressorPreset_BC2Ut;
						break;
					case ePixelFormat_DXT5: 
					case ePixelFormat_BC3: 
						decompress.preset = CryTextureSquisher::eCompressorPreset_BC3U;
						break;
					case ePixelFormat_DXT5t: 
					case ePixelFormat_BC3t: 
						decompress.preset = CryTextureSquisher::eCompressorPreset_BC3Ut;
						break;
					case ePixelFormat_3DCp: 
					case ePixelFormat_BC4:
						decompress.preset = (CPixelFormats::IsFormatSingleChannel(fmtDst)
							? CryTextureSquisher::eCompressorPreset_BC4Ua    // a-channel 
							: CryTextureSquisher::eCompressorPreset_BC4U);   // r-channel
						break;
					case ePixelFormat_BC4s:
						decompress.preset = (CPixelFormats::IsFormatSingleChannel(fmtDst)
							? CryTextureSquisher::eCompressorPreset_BC4Sa    // a-channel 
							: CryTextureSquisher::eCompressorPreset_BC4S);   // r-channel
						break;
					case ePixelFormat_3DC: 
					case ePixelFormat_BC5: 
						decompress.preset = CryTextureSquisher::eCompressorPreset_BC5Un;
						break;
					case ePixelFormat_BC5s: 
						decompress.preset = CryTextureSquisher::eCompressorPreset_BC5Sn;
						break;
					case ePixelFormat_BC6UH:
						decompress.preset = CryTextureSquisher::eCompressorPreset_BC6UH;
						break;
					case ePixelFormat_BC7: 
						decompress.preset = CryTextureSquisher::eCompressorPreset_BC7U;
						break;
					case ePixelFormat_BC7t: 
						decompress.preset = CryTextureSquisher::eCompressorPreset_BC7Ut;
						break;
					default:
						RCLogError("%s: Unexpected pixel format (in decompressing an image). Inform an RC programmer.", __FUNCTION__);
						set(0);
						return eResult_Failed;
					}

					CryTextureSquisher::Decompress(decompress); 

					assert(userData.m_dstOffset == dwSrcPitch * ((dwLocalHeight + 3) / 4));
				}
			} // for: all mips
		}

		// CTsquish operates on native normal vectors when floating-point
		// buffers are used. Apply bias and scale when returning a normal-map.
		if (!bEncode)
		{
			if (fmtSrc == ePixelFormat_3DC || fmtSrc == ePixelFormat_BC5 || fmtSrc == ePixelFormat_BC5s)
			{
				if (fmtDst == ePixelFormat_A32B32G32R32F)
				{
					pRet->ScaleAndBiasChannels(0, 100,
						Vec4(0.5f, 0.5f, 0.5f, 1.0f),
						Vec4(0.5f, 0.5f, 0.5f, 0.0f));
				}
			}
		}

		// check if we lost the alpha-channel in the conversion
		if (CPixelFormats::IsPixelFormatWithoutAlpha(fmtDst))
		{
			// save alpha channel as *attached* image, because it's impossible to store it in the 2-channel normal map image
			if (pProps->m_bPreserveAlpha && get()->HasNonOpaqueAlpha() && !pProps->GetDiscardAlpha())
			{
				// TODO: squish-ccr is able to encode BC4 directly out of the alpha-channel of
				// a joint RGBA field, try to get rid of the 2x convert()

				const EPixelFormat destinationFormat = pProps->GetDestAlphaPixelFormat();

				// we are limiting alpha mip count because A8/R32F may contain more mips than 3DC
				ImageToProcess tempImageToProcess(
					fmtSrc == ePixelFormat_A32B32G32R32F && destinationFormat != ePixelFormat_A8
					? get()->CopyAnyAlphaIntoR32FImage(pRet->GetMipCount())
					: get()->CopyAnyAlphaIntoA8Image(pRet->GetMipCount())
				);
					
				// clear alpha range from color range
				{
					Vec4 minColor, maxColor;

					pRet->GetColorRange(minColor, maxColor);
					minColor.w = 0.0f;
					maxColor.w = 0.0f;
					pRet->SetColorRange(minColor, maxColor);
				}

				const EPixelFormat currentFormat = tempImageToProcess.get()->GetPixelFormat();
				if (destinationFormat != currentFormat)
				{
					tempImageToProcess.ConvertFormat(pProps, destinationFormat);
				}

				pRet->SetAttachedImage(tempImageToProcess.get());
				tempImageToProcess.forget();
			}
		}
		else
		{
			// restore alpha channel from attached image
			ImageObject* pAttached = get()->GetAttachedImage();
			if (pAttached)
			{
				// TODO: squish-ccr is able to decode BC4 directly into the alpha-channel of
				// a joint RGBA field, try to get rid of the 2x convert()

				const EPixelFormat currentFormat = pAttached->GetPixelFormat();
				const EPixelFormat destinationFormat = (fmtDst == ePixelFormat_A32B32G32R32F ? ePixelFormat_R32F : ePixelFormat_A8);
				if (destinationFormat != currentFormat)
				{
					ImageToProcess tempImageToProcess(pAttached->CopyImage());

					tempImageToProcess.ConvertFormat(pProps, destinationFormat);

					pAttached = tempImageToProcess.get();
					tempImageToProcess.forget();
				}

				if (pAttached->GetPixelFormat() == ePixelFormat_R32F)
				{
					pRet->TakeoverAnyAlphaFromR32FImage(pAttached);
				}
				else if (pAttached->GetPixelFormat() == ePixelFormat_A8)
				{
					pRet->TakeoverAnyAlphaFromA8Image(pAttached);
				}

				if (pAttached != get()->GetAttachedImage())
				{
					delete  pAttached;
				}
			}
		}

		if (!pRet.get())
		{
			RCLogError("Unexpected failure in compression module");			
		}

		set(pRet.release());
		return eResult_Success;
	}

	return eResult_UnsupportedFormat;
}
