// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLResource.cpp
//  Version:     v1.00
//  Created:     30/04/2013 by Valerio Guagliumi.
//  Description: Implements the resource related functions
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "GLResource.hpp"
#include "GLContext.hpp"
#include "GLDevice.hpp"
#include "GLView.hpp"

#define DXGL_USE_IMMUTABLE_TEXTURES                         !DXGL_SUPPORT_NSIGHT_SINCE(3_0)
#define DXGL_CHECK_TEXTURE_UPLOAD_READ_BOUNDS               0
#define DXGL_LOG_TEXTURE_SIZES                              0
#define DXGL_SERVER_SIDE_SHARED_OBJECT_SYNCHRONIZATION      1
#define DXGL_PREVENT_NVIDIA_SUB_BUFFER_DATA_SYNCHRONIZATION 1

namespace NCryOpenGL
{

enum
{
	MIN_MAPPED_RESOURCE_ALIGNMENT = 64, // DX10+ mapped resources are 16-aligned but GL_ARB_map_buffer_alignment ensures 64-alignment for AVX
	MAX_UNPACK_ALIGNMENT          = 8,
	MAX_PACK_ALIGNMENT            = 8
};

uint32 GetRowPitch(uint32 uWidth, uint32 uRowBytes, const SGIFormatInfo* pFormatInfo)
{
	uint32 uNumElementsPerRow(uRowBytes * pFormatInfo->m_pTexture->m_uBlockWidth / pFormatInfo->m_pTexture->m_uNumBlockBytes);
	return uNumElementsPerRow == uWidth ? 0 : uNumElementsPerRow;
}

uint32 GetImagePitch(uint32 uHeight, uint32 uImageBytes, uint32 uRowBytes)
{
	uint32 uNumRowsPerImage(uImageBytes / uRowBytes);
	return uNumRowsPerImage == uHeight ? 0 : uNumRowsPerImage;
}

struct STexBox
{
	STexPos  m_kOffset;
	STexSize m_kSize;
};

struct SPackedLayout
{
	uint32 m_uRowPitch;
	uint32 m_uImagePitch;
	uint32 m_uTextureSize;
};

static GLint GetMaxMipLevels(const D3D11_TEXTURE1D_DESC& kTexDesc)
{
	return (GLint)IntegerLog2(kTexDesc.Width);
}

static GLint GetMaxMipLevels(const D3D11_TEXTURE2D_DESC& kTexDesc)
{
	return (GLint)max(IntegerLog2(kTexDesc.Width), IntegerLog2(kTexDesc.Height));
}

static GLint GetMaxMipLevels(const D3D11_TEXTURE3D_DESC& kTexDesc)
{
	return (GLint)max(max(IntegerLog2(kTexDesc.Width), IntegerLog2(kTexDesc.Height)), IntegerLog2(kTexDesc.Depth));
}

template<typename TextureDesc>
static GLint GetNumMipLevels(const TextureDesc& kTexDesc)
{
	return kTexDesc.MipLevels != 0 ? kTexDesc.MipLevels : GetMaxMipLevels(kTexDesc);
}

static STexSize GetMipSize(STexture* pTexture, GLint iLevel, const SGIFormatInfo* pFormat, bool bClampToBlockSize)
{
	STexSize kMinSize(1, 1, 1);
	if (bClampToBlockSize && pFormat->m_pTexture->m_bCompressed)
	{
		kMinSize = STexSize(
		  pFormat->m_pTexture->m_uBlockWidth,
		  pFormat->m_pTexture->m_uBlockHeight,
		  pFormat->m_pTexture->m_uBlockDepth);
	}

	return STexSize(
	  max(kMinSize.x, pTexture->m_iWidth >> iLevel),
	  max(kMinSize.y, pTexture->m_iHeight >> iLevel),
	  max(kMinSize.z, pTexture->m_iDepth >> iLevel));
}

void GetTextureBox(STexBox& kTexBox, STexture* pTexture, GLint iLevel, const SGIFormatInfo* pFormat, bool bClampToBlockSize)
{
	kTexBox.m_kOffset = STexPos(0, 0, 0);
	kTexBox.m_kSize = GetMipSize(pTexture, iLevel, pFormat, bClampToBlockSize);
}

void GetTextureBox(STexBox& kTexBox, STexture* pTexture, GLint iLevel, const D3D11_BOX* pBox, const SGIFormatInfo* pFormat, bool bClampToBlockSize)
{
	if (pBox != NULL)
	{
		kTexBox.m_kOffset = STexPos(pBox->left, pBox->top, pBox->front);
		kTexBox.m_kSize = STexSize(pBox->right - pBox->left, pBox->bottom - pBox->top, pBox->back - pBox->front);
	}
	else
		GetTextureBox(kTexBox, pTexture, iLevel, pFormat, bClampToBlockSize);
}

#if !DXGL_SUPPORT_COPY_IMAGE || !DXGL_SUPPORT_GETTEXIMAGE

SOutputMergerTextureViewPtr GetCopyOutputMergerView(STexture* pTexture, STexSubresourceID kSubID, CContext* pContext, const SGIFormatInfo* pFormatInfo)
{
	uint32 uSubresource(D3D11CalcSubresource(kSubID.m_iMipLevel, kSubID.m_uElement, pTexture->m_uNumMipLevels));

	DXGL_TODO("This is not thread-safe, as multiple threads can use the same texture as source/destination of copies. Add synchronization primitive.");

	if (pTexture->m_kCopySubTextureViews.size() <= uSubresource)
		pTexture->m_kCopySubTextureViews.resize(uSubresource + 1);
	SOutputMergerTextureViewPtr spOMView(pTexture->m_kCopySubTextureViews.at(uSubresource));
	if (spOMView == NULL)
	{
		spOMView = pTexture->GetCompatibleOutputMergerView(
		  SOutputMergerTextureViewConfiguration(pTexture->m_eFormat, kSubID.m_iMipLevel, kSubID.m_uElement, 1),
		  pContext);
		pTexture->m_kCopySubTextureViews.at(uSubresource) = spOMView;
	}

	return spOMView;
}

#endif //!DXGL_SUPPORT_COPY_IMAGE || !DXGL_SUPPORT_GETTEXIMAGE

struct STex1DBase
{
	static GLsizei GetBCImageSize(STexSize kSize, const STextureFormat* pTexFormat)
	{
		return pTexFormat->m_uNumBlockBytes *
		       ((kSize.x + pTexFormat->m_uBlockWidth - 1) / pTexFormat->m_uBlockWidth);
	}
};

struct STex2DBase
{
	static GLsizei GetBCImageSize(STexSize kSize, const STextureFormat* pTexFormat)
	{
		return pTexFormat->m_uNumBlockBytes *
		       ((kSize.x + pTexFormat->m_uBlockWidth - 1) / pTexFormat->m_uBlockWidth) *
		       ((kSize.y + pTexFormat->m_uBlockHeight - 1) / pTexFormat->m_uBlockHeight);
	}
};

struct STex3DBase
{
	static GLsizei GetBCImageSize(STexSize kSize, const STextureFormat* pTexFormat)
	{
		return pTexFormat->m_uNumBlockBytes *
		       ((kSize.x + pTexFormat->m_uBlockWidth - 1) / pTexFormat->m_uBlockWidth) *
		       ((kSize.y + pTexFormat->m_uBlockHeight - 1) / pTexFormat->m_uBlockHeight) *
		       ((kSize.z + pTexFormat->m_uBlockDepth - 1) / pTexFormat->m_uBlockDepth);
	}
};

struct SDefaultTexBase
{
	static void AllocateResource(STexture* pTexture, CDevice* pDevice)
	{
		GLuint uName;
		glGenTextures(1, &uName);
		pTexture->m_kName = pDevice->GetTextureNamePool().Create(uName);
	}
};

struct SDefaultTex1DBase : SDefaultTexBase, STex1DBase
{
	static void TexStorage(STexture* pTexture, STexSize kSize, GLsizei iLevels, const STextureFormat* pTexFormat)
	{
		glTextureStorage1DEXT(pTexture->m_kName.GetName(), pTexture->m_eTarget, iLevels, pTexFormat->m_iInternalFormat, kSize.x);
	}

	static void ApplyUnpackState(CContext* pContext, STexSize kSize, uint32 uLogDataAlignment, uint32 uRowPitch, uint32, const SGIFormatInfo* pFormat)
	{
		pContext->SetUnpackAlignment(std::min<int>(MAX_UNPACK_ALIGNMENT, 1 << uLogDataAlignment));
	}

	static void ApplyPackState(CContext* pContext, STexSize kSize, uint32 uLogDataAlignment, uint32 uRowPitch, uint32, const SGIFormatInfo* pFormat)
	{
		pContext->SetPackAlignment(std::min<int>(MAX_PACK_ALIGNMENT, 1 << uLogDataAlignment));
	}
};

struct SDefaultTex2DBase : SDefaultTexBase, STex2DBase
{
	typedef SDefaultTex1DBase TArrayElement;

	static void TexStorage(STexture* pTexture, STexSize kSize, GLsizei iLevels, const STextureFormat* pTexFormat)
	{
		glTextureStorage2DEXT(pTexture->m_kName.GetName(), pTexture->m_eTarget, iLevels, pTexFormat->m_iInternalFormat, kSize.x, kSize.y);
	}

	static void ApplyUnpackState(CContext* pContext, STexSize kSize, uint32 uLogDataAlignment, uint32 uRowPitch, uint32, const SGIFormatInfo* pFormat)
	{
		pContext->SetUnpackRowLength(GetRowPitch(kSize.x, uRowPitch, pFormat));
		pContext->SetUnpackAlignment(std::min<int>(MAX_UNPACK_ALIGNMENT, 1 << std::min<int>(uLogDataAlignment, countTrailingZeros32(uRowPitch))));
	}

	static void ApplyPackState(CContext* pContext, STexSize kSize, uint32 uLogDataAlignment, uint32 uRowPitch, uint32, const SGIFormatInfo* pFormat)
	{
		pContext->SetPackRowLength(GetRowPitch(kSize.x, uRowPitch, pFormat));
		pContext->SetPackAlignment(std::min<int>(MAX_PACK_ALIGNMENT, 1 << std::min<int>(uLogDataAlignment, countTrailingZeros32(uRowPitch))));
	}

	template<typename T>
	static void SetLayerComponent(STexVec<T>& kVec, T kLayer)
	{
		kVec.y = kLayer;
	}
};

struct SDefaultTex3DBase : SDefaultTexBase, STex3DBase
{
	typedef SDefaultTex2DBase TArrayElement;

	static void TexStorage(STexture* pTexture, STexSize kSize, GLsizei iLevels, const STextureFormat* pTexFormat)
	{
		glTextureStorage3DEXT(pTexture->m_kName.GetName(), pTexture->m_eTarget, iLevels, pTexFormat->m_iInternalFormat, kSize.x, kSize.y, kSize.z);
	}

	static void ApplyUnpackState(CContext* pContext, STexSize kSize, uint32 uLogDataAlignment, uint32 uRowPitch, uint32 uImagePitch, const SGIFormatInfo* pFormat)
	{
		pContext->SetUnpackRowLength(GetRowPitch(kSize.x, uRowPitch, pFormat));
		pContext->SetUnpackImageHeight(GetImagePitch(kSize.y, uImagePitch, uRowPitch));
		pContext->SetUnpackAlignment(std::min<int>(MAX_UNPACK_ALIGNMENT, 1 << std::min<int>(uLogDataAlignment, std::min<int>(countTrailingZeros32(uRowPitch), countTrailingZeros32(uImagePitch)))));
	}

	static void ApplyPackState(CContext* pContext, STexSize kSize, uint32 uLogDataAlignment, uint32 uRowPitch, uint32 uImagePitch, const SGIFormatInfo* pFormat)
	{
		pContext->SetPackRowLength(GetRowPitch(kSize.x, uRowPitch, pFormat));
		pContext->SetPackImageHeight(GetImagePitch(kSize.y, uImagePitch, uRowPitch));
		pContext->SetPackAlignment(std::min<int>(MAX_PACK_ALIGNMENT, 1 << std::min<int>(uLogDataAlignment, std::min<int>(countTrailingZeros32(uRowPitch), countTrailingZeros32(uImagePitch)))));
	}

	template<typename T>
	static void SetLayerComponent(STexVec<T>& kVec, T kLayer)
	{
		kVec.z = kLayer;
	}
};

struct STexCompressed
{
#if DXGL_SUPPORT_GETTEXIMAGE
	static void GetTexImage(STexture* pTexture, GLenum eTarget, GLint iLevel, GLvoid* pData, CContext*, const SGIFormatInfo*)
	{
		glGetCompressedTextureImageEXT(pTexture->m_kName.GetName(), eTarget, iLevel, pData);
	}
#endif //DXGL_SUPPORT_GETTEXIMAGE

	static bool GetPackedRange(const STexBox& kPixels, STexBox* pPackedRange, const SGIFormatInfo* pFormat)
	{
		const STextureFormat* pTexFormat(pFormat->m_pTexture);
		if ((kPixels.m_kOffset.x % pTexFormat->m_uBlockWidth) != 0 ||
		    (kPixels.m_kOffset.y % pTexFormat->m_uBlockHeight) != 0 ||
		    (kPixels.m_kOffset.z % pTexFormat->m_uBlockDepth) != 0 ||
		    (kPixels.m_kSize.x % pTexFormat->m_uBlockWidth) != 0 ||
		    (kPixels.m_kSize.y % pTexFormat->m_uBlockHeight) != 0 ||
		    (kPixels.m_kSize.z % pTexFormat->m_uBlockDepth) != 0)
			return false;

		pPackedRange->m_kOffset.x = pTexFormat->m_uNumBlockBytes * kPixels.m_kOffset.x / pTexFormat->m_uBlockWidth;
		pPackedRange->m_kSize.x = pTexFormat->m_uNumBlockBytes * kPixels.m_kSize.x / pTexFormat->m_uBlockWidth;

		pPackedRange->m_kOffset.y = kPixels.m_kOffset.y / pTexFormat->m_uBlockHeight;
		pPackedRange->m_kSize.y = kPixels.m_kSize.y / pTexFormat->m_uBlockHeight;

		pPackedRange->m_kOffset.z = kPixels.m_kOffset.z / pTexFormat->m_uBlockDepth;
		pPackedRange->m_kSize.z = kPixels.m_kSize.z / pTexFormat->m_uBlockDepth;
		return true;
	}
};

struct STexUncompressed
{
#if DXGL_SUPPORT_GETTEXIMAGE
	static void GetTexImage(STexture* pTexture, GLenum eTarget, GLint iLevel, GLvoid* pData, CContext* pContext, const SGIFormatInfo* pFormatInfo)
	{
		glGetTextureImageEXT(pTexture->m_kName.GetName(), eTarget, iLevel, pFormatInfo->m_pTexture->m_eBaseFormat, pFormatInfo->m_pTexture->m_eDataType, pData);
	}
#endif //DXGL_SUPPORT_GETTEXIMAGE

	static bool GetPackedRange(const STexBox& kPixels, STexBox* pPackedRange, const SGIFormatInfo* pFormat)
	{
		uint32 uPixelBytes(pFormat->m_pTexture->m_uNumBlockBytes);

		pPackedRange->m_kOffset.x = kPixels.m_kOffset.x * uPixelBytes;
		pPackedRange->m_kSize.x = kPixels.m_kSize.x * uPixelBytes;

		pPackedRange->m_kOffset.y = kPixels.m_kOffset.y;
		pPackedRange->m_kSize.y = kPixels.m_kSize.y;

		pPackedRange->m_kOffset.z = kPixels.m_kOffset.z;
		pPackedRange->m_kSize.z = kPixels.m_kSize.z;
		return true;
	}
};

struct STex1DUncompressed : SDefaultTex1DBase, STexUncompressed
{
	static void TexImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexSize kSize, GLint iBorder, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		glTextureImage1DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, pTexFormat->m_iInternalFormat, kSize.x, iBorder, pTexFormat->m_eBaseFormat, pTexFormat->m_eDataType, pData);
	}

	static void TexSubImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexBox kBox, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		glTextureSubImage1DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, kBox.m_kOffset.x, kBox.m_kSize.x, pTexFormat->m_eBaseFormat, pTexFormat->m_eDataType, pData);
	}

	static void GetPackedLayout(STexSize kRect, const SGIFormatInfo* pFormat, SPackedLayout* pLayout)
	{
		pLayout->m_uRowPitch = kRect.x * pFormat->m_pTexture->m_uNumBlockBytes;
		pLayout->m_uImagePitch = pLayout->m_uRowPitch;
		pLayout->m_uTextureSize = pLayout->m_uRowPitch;
	}
};

struct STex2DUncompressed : SDefaultTex2DBase, STexUncompressed
{
	static void TexImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexSize kSize, GLint iBorder, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		glTextureImage2DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, pTexFormat->m_iInternalFormat, kSize.x, kSize.y, iBorder, pTexFormat->m_eBaseFormat, pTexFormat->m_eDataType, pData);
	}

	static void TexSubImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexBox kBox, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		glTextureSubImage2DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, kBox.m_kOffset.x, kBox.m_kOffset.y, kBox.m_kSize.x, kBox.m_kSize.y, pTexFormat->m_eBaseFormat, pTexFormat->m_eDataType, pData);
	}

	static void GetPackedLayout(STexSize kRect, const SGIFormatInfo* pFormat, SPackedLayout* pLayout)
	{
		pLayout->m_uRowPitch = kRect.x * pFormat->m_pTexture->m_uNumBlockBytes;
		pLayout->m_uImagePitch = kRect.y * pLayout->m_uRowPitch;
		pLayout->m_uTextureSize = pLayout->m_uImagePitch;
	}
};

struct STex3DUncompressed : SDefaultTex3DBase, STexUncompressed
{
	static void TexImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexSize kSize, GLint iBorder, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		glTextureImage3DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, pTexFormat->m_iInternalFormat, kSize.x, kSize.y, kSize.z, iBorder, pTexFormat->m_eBaseFormat, pTexFormat->m_eDataType, pData);
	}

	static void TexSubImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexBox kBox, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		glTextureSubImage3DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, kBox.m_kOffset.x, kBox.m_kOffset.y, kBox.m_kOffset.z, kBox.m_kSize.x, kBox.m_kSize.y, kBox.m_kSize.z, pTexFormat->m_eBaseFormat, pTexFormat->m_eDataType, pData);
	}

	static void GetPackedLayout(STexSize kRect, const SGIFormatInfo* pFormat, SPackedLayout* pLayout)
	{
		pLayout->m_uRowPitch = kRect.x * pFormat->m_pTexture->m_uNumBlockBytes;
		pLayout->m_uImagePitch = kRect.y * pLayout->m_uRowPitch;
		pLayout->m_uTextureSize = kRect.z * pLayout->m_uImagePitch;
	}
};

struct STex1DCompressed : SDefaultTex1DBase, STexCompressed
{
	static void TexImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexSize kSize, GLint iBorder, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		glCompressedTextureImage1DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, pTexFormat->m_iInternalFormat, kSize.x, iBorder, GetBCImageSize(kSize, pTexFormat), pData);
	}

	static void TexSubImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexBox kBox, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		// Note: According to the specification this takes the internal format and not the base format
		DXGL_TODO("Verify the format parameter");
		glCompressedTextureSubImage1DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, kBox.m_kOffset.x, kBox.m_kSize.x, pTexFormat->m_iInternalFormat, GetBCImageSize(kBox.m_kSize, pTexFormat), pData);
	}

	static void GetPackedLayout(STexSize kRect, const SGIFormatInfo* pFormat, SPackedLayout* pLayout)
	{
		pLayout->m_uRowPitch = GetBCImageSize(kRect, pFormat->m_pTexture);
		pLayout->m_uImagePitch = pLayout->m_uRowPitch;
		pLayout->m_uTextureSize = pLayout->m_uRowPitch;
	}
};

struct STex2DCompressed : SDefaultTex2DBase, STexCompressed
{
	static void TexImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexSize kSize, GLint iBorder, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		glCompressedTextureImage2DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, pTexFormat->m_iInternalFormat, kSize.x, kSize.y, iBorder, GetBCImageSize(kSize, pTexFormat), pData);
	}

	static void TexSubImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexBox kBox, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		// Note: According to the specification this takes the internal format and not the base format
		DXGL_TODO("Verify the format parameter");
		glCompressedTextureSubImage2DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, kBox.m_kOffset.x, kBox.m_kOffset.y, kBox.m_kSize.x, kBox.m_kSize.y, pTexFormat->m_iInternalFormat, GetBCImageSize(kBox.m_kSize, pTexFormat), pData);
	}

	static void GetPackedLayout(STexSize kRect, const SGIFormatInfo* pFormat, SPackedLayout* pLayout)
	{
		pLayout->m_uRowPitch = GetBCImageSize(STexSize(kRect.x, 1, 1), pFormat->m_pTexture);
		pLayout->m_uImagePitch = GetBCImageSize(kRect, pFormat->m_pTexture);
		pLayout->m_uTextureSize = pLayout->m_uImagePitch;
	}
};

struct STex3DCompressed : SDefaultTex3DBase, STexCompressed
{
	static void TexImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexSize kSize, GLint iBorder, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		glCompressedTextureImage3DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, pTexFormat->m_iInternalFormat, kSize.x, kSize.y, kSize.z, iBorder, GetBCImageSize(kSize, pTexFormat), pData);
	}

	static void TexSubImage(STexture* pTexture, GLenum eTarget, GLint iLevel, STexBox kBox, const STextureFormat* pTexFormat, const GLvoid* pData)
	{
		// Note: According to the specification this takes the internal format and not the base format
		DXGL_TODO("Verify the format parameter");
		glCompressedTextureSubImage3DEXT(pTexture->m_kName.GetName(), eTarget, iLevel, kBox.m_kOffset.x, kBox.m_kOffset.y, kBox.m_kOffset.z, kBox.m_kSize.x, kBox.m_kSize.y, kBox.m_kSize.z, pTexFormat->m_iInternalFormat, GetBCImageSize(kBox.m_kSize, pTexFormat), pData);
	}

	static void GetPackedLayout(STexSize kRect, const SGIFormatInfo* pFormat, SPackedLayout* pLayout)
	{
		pLayout->m_uRowPitch = GetBCImageSize(STexSize(kRect.x, 1, 1), pFormat->m_pTexture);
		pLayout->m_uImagePitch = GetBCImageSize(STexSize(kRect.x, kRect.y, 1), pFormat->m_pTexture);
		pLayout->m_uTextureSize = GetBCImageSize(kRect, pFormat->m_pTexture);
	}
};

struct STexPartition
{
	static void GetSubTargetAndLayer(STexture* pTexture, uint32 uElement, GLenum& eTarget, GLint& iLayer)
	{
		eTarget = pTexture->m_eTarget;
		iLayer = (GLint)uElement;
	}

	static uint32 GetNumLayers(STexture* pTexture)
	{
		return pTexture->m_uNumElements;
	}

#if DXGL_SUPPORT_COPY_IMAGE
	static void InitializeCopyImageView(STexture* pTexture, const SGIFormatInfo* pFormat, CContext* pContext)
	{
		pTexture->m_kCopyImageView = pTexture->m_kName;
		pTexture->m_eCopyImageTarget = pTexture->m_eTarget;
	}
#endif //DXGL_SUPPORT_COPY_IMAGE
};

struct SCubePartition
{
	static void GetSubTargetAndLayer(STexture* pTexture, uint32 uElement, GLenum& eTarget, GLint& iLayer)
	{
		static const GLenum s_aeFaceTargets[] =
		{
			GL_TEXTURE_CUBE_MAP_POSITIVE_X, // 0
			GL_TEXTURE_CUBE_MAP_NEGATIVE_X, // 1
			GL_TEXTURE_CUBE_MAP_POSITIVE_Y, // 2
			GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, // 3
			GL_TEXTURE_CUBE_MAP_POSITIVE_Z, // 4
			GL_TEXTURE_CUBE_MAP_NEGATIVE_Z  // 5
		};

		eTarget = s_aeFaceTargets[uElement % 6];
		iLayer = uElement / 6;
	}

	static uint32 GetNumLayers(STexture* pTexture)
	{
		return pTexture->m_uNumElements / 6;
	}

#if DXGL_SUPPORT_COPY_IMAGE
	static void InitializeCopyImageView(STexture* pTexture, const SGIFormatInfo* pFormat, CContext* pContext)
	{
		CDevice* pDevice(pContext->GetDevice());
		if (!pDevice->GetAdapter()->m_kCapabilities.m_bCopyImageWorksOnCubeMapFaces && SupportTextureViews())
		{
			GLuint uCopyViewName;
			glGenTextures(1, &uCopyViewName);
			glTextureViewEXT(uCopyViewName, GL_TEXTURE_2D_ARRAY, pTexture->m_kName.GetName(), pFormat->m_pTexture->m_iInternalFormat, 0, pTexture->m_uNumMipLevels, 0, pTexture->m_uNumElements);
			pTexture->m_kCopyImageView = pDevice->GetTextureNamePool().Create(uCopyViewName);
			pTexture->m_eCopyImageTarget = GL_TEXTURE_2D_ARRAY;
		}
		else
		{
			pTexture->m_kCopyImageView = pTexture->m_kName;
			pTexture->m_eCopyImageTarget = pTexture->m_eTarget;
		}
	}
#endif //DXGL_SUPPORT_COPY_IMAGE
};

#if DXGL_CHECK_TEXTURE_UPLOAD_READ_BOUNDS

uint32 GetPageSize()
{
	#if CRY_PLATFORM_WINDOWS
	SYSTEM_INFO kInfo;
	::GetSystemInfo(&kInfo);
	return kInfo.dwPageSize;
	#else
	return getpagesize();
	#endif
}

template<typename Interface>
void TestUploadBounds(STexture* pTexture, GLenum eTarget, GLint iLevel, STexBox kBox, const SGIFormatInfo* pFormat, const GLvoid* pData, uint32 uSrcRowPitch, uint32 uSrcDepthPitch)
{
	static uint32 s_uPageSize = GetPageSize();
	static std::map<uint32, char*> s_kBuffers;
	static uint32 s_uNumBufferPages = 0;

	uint32 uSize;
	if (pFormat->m_pTexture->m_bCompressed)
		uSize = Interface::GetBCImageSize(kBox.m_kSize, pFormat->m_pTexture);
	else if (kBox.m_kSize.z > 1)
		uSize = kBox.m_kSize.z * uSrcDepthPitch;
	else if (kBox.m_kSize.y > 1)
		uSize = kBox.m_kSize.y * uSrcRowPitch;
	else
		uSize = kBox.m_kSize.x * pFormat->m_pTexture->m_uNumBlockBytes;
	uint32 uRequiredPages(2 + (uSize + s_uPageSize) / s_uPageSize);

	char* pBuffer;
	std::map<uint32, char*>::iterator kFound(s_kBuffers.find(uRequiredPages));
	if (kFound != s_kBuffers.end())
		pBuffer = kFound->second;
	else
	{
	#if CRY_PLATFORM_WINDOWS
		pBuffer = reinterpret_cast<char*>(VirtualAlloc(NULL, uRequiredPages * s_uPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		DWORD uUnused;
		VirtualProtect(pBuffer, s_uPageSize, PAGE_NOACCESS, &uUnused);
		VirtualProtect(pBuffer + s_uPageSize * (uRequiredPages - 1), s_uPageSize, PAGE_NOACCESS, &uUnused);
	#else
		pBuffer = reinterpret_cast<char*>(mmap(NULL, uRequiredPages * s_uPageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
		mprotect(pBuffer, s_uPageSize, PROT_NONE);
		mprotect(pBuffer + s_uPageSize * (uRequiredPages - 1), s_uPageSize, PROT_NONE);
	#endif
	}

	Interface::TexSubImage(pTexture, eTarget, iLevel, kBox, pFormat->m_pTexture, pBuffer + s_uPageSize);
	Interface::TexSubImage(pTexture, eTarget, iLevel, kBox, pFormat->m_pTexture, pBuffer + (uRequiredPages - 1) * s_uPageSize - uSize);
}

#else

template<typename Interface>
void TestUploadBounds(STexture*, GLenum, GLint, STexBox, const SGIFormatInfo*, const GLvoid*, uint32, uint32) {}

#endif

template<typename Interface, typename Partition>
void LogTextureSize(STexture* pTexture)
{
#if DXGL_LOG_TEXTURE_SIZES
	const char* szFormatName = "(invalid)";
	switch (pTexture->m_eFormat)
	{
	#define _GI_FORMAT_ENUM_FUNC(_FormatID, ...) case DXGL_GI_FORMAT(_FormatID): \
	  szFormatName = DXGL_QUOTE(_FormatID); break;
		DXGL_GI_FORMATS(_GI_FORMAT_ENUM_FUNC)
	#undef _GI_FORMAT_ENUM_FUNC
	}
	GLint iLayer;
	GLenum eTarget;
	Partition::GetSubTargetAndLayer(pTexture, 0, eTarget, iLayer);

	uint32 uLevel;
	for (uLevel = 0; uLevel < pTexture->m_uNumMipLevels; ++uLevel)
	{
		GLint iWidth, iHeight, iDepth;
		glGetTextureLevelParameterivEXT(pTexture->m_kName.GetName(), eTarget, uLevel, GL_TEXTURE_WIDTH, &iWidth);
		glGetTextureLevelParameterivEXT(pTexture->m_kName.GetName(), eTarget, uLevel, GL_TEXTURE_HEIGHT, &iHeight);
		glGetTextureLevelParameterivEXT(pTexture->m_kName.GetName(), eTarget, uLevel, GL_TEXTURE_DEPTH, &iDepth);
		CryLogAlways(" TEXTURE SIZE name=%d format=%s level=%d size=[%d, %d, %d]\n", pTexture->m_kName.GetName(), szFormatName, uLevel, iWidth, iHeight, iDepth);
	}
#endif
}

template<typename Interface, typename Partition = STexPartition>
struct SSingleTexImpl : Interface
{
	static void InitializeStorage(STexture* pTexture, uint32, const SGIFormatInfo* pFormat, CContext* pContext)
	{
#if DXGL_USE_IMMUTABLE_TEXTURES
		Interface::TexStorage(pTexture, GetMipSize(pTexture, 0, pFormat, false), pTexture->m_uNumMipLevels, pFormat->m_pTexture);
#else
		GLint iLevel;
		uint32 uElement;
		for (uElement = 0; uElement < pTexture->m_uNumElements; ++uElement)
		{
			GLenum eSubTarget;
			GLint iLayer;
			Partition::GetSubTargetAndLayer(pTexture, uElement, eSubTarget, iLayer);
			assert(iLayer == 0);

			for (iLevel = 0; iLevel < (GLint)pTexture->m_uNumMipLevels; ++iLevel)
				Interface::TexImage(pTexture, eSubTarget, iLevel, GetMipSize(pTexture, iLevel, pFormat, false), 0, pFormat->m_pTexture, NULL);
		}
#endif

#if DXGL_SUPPORT_COPY_IMAGE
		Partition::InitializeCopyImageView(pTexture, pFormat, pContext);
#endif //DXGL_SUPPORT_COPY_IMAGE

		LogTextureSize<Interface, Partition>(pTexture);
	}

	static void UploadImage(STexture* pTexture, STexSubresourceID kSubID, STexBox kBox, const void* pSrcData, uint32 uSrcRowPitch, uint32 uSrcDepthPitch, CContext* pContext, const SGIFormatInfo* pFormat)
	{
		GLenum eSubTarget;
		GLint iLayer;
		Partition::GetSubTargetAndLayer(pTexture, kSubID.m_uElement, eSubTarget, iLayer);
		assert(iLayer == 0);

		Interface::ApplyUnpackState(pContext, kBox.m_kSize, countTrailingZeros32((unsigned int)reinterpret_cast<uintptr_t>(pSrcData)), uSrcRowPitch, uSrcDepthPitch, pFormat);
		TestUploadBounds<Interface>(pTexture, eSubTarget, kSubID.m_iMipLevel, kBox, pFormat, pSrcData, uSrcRowPitch, uSrcDepthPitch);
		Interface::TexSubImage(pTexture, eSubTarget, kSubID.m_iMipLevel, kBox, pFormat->m_pTexture, pSrcData);
	}

	static void DownloadImage(STexture* pTexture, STexSubresourceID kSubID, STexBox kBox, void* pDstData, uint32 uDstRowPitch, uint32 uDstDepthPitch, CContext* pContext, const SGIFormatInfo* pFormat)
	{
		Interface::ApplyPackState(pContext, kBox.m_kSize, countTrailingZeros32((unsigned int)reinterpret_cast<uintptr_t>(pDstData)), uDstRowPitch, uDstDepthPitch, pFormat);

#if DXGL_SUPPORT_GETTEXIMAGE
		GLenum eSubTarget;
		GLint iLayer;
		Partition::GetSubTargetAndLayer(pTexture, kSubID.m_uElement, eSubTarget, iLayer);
		assert(iLayer == 0);

		Interface::GetTexImage(pTexture, eSubTarget, kSubID.m_iMipLevel, pDstData, pContext, pFormat);
#else
		if (kBox.m_kOffset.z > 0 || kBox.m_kSize.z > 1)
		{
			DXGL_NOT_IMPLEMENTED
		}

		SOutputMergerTextureViewPtr spView(GetCopyOutputMergerView(pTexture, kSubID, pContext, pFormat));
		if (spView == NULL)
		{
			DXGL_ERROR("Could not create an output merger view for texture readback");
			return;
		}

		if (!pContext->ReadBackOutputMergerView(spView, kBox.m_kOffset.x, kBox.m_kOffset.y, kBox.m_kSize.x, kBox.m_kSize.y, pDstData))
		{
			DXGL_ERROR("Texture readback failed");
			return;
		}
#endif
	}

	static void Map(STexture* pTexture, STexSubresourceID kSubID, bool bDownload, SMappedSubTexture& kMappedSubTex, CContext* pContext, const SGIFormatInfo* pFormat)
	{
		STexBox kBox;
		GetTextureBox(kBox, pTexture, kSubID.m_iMipLevel, pFormat, true);
		SPackedLayout kPackedLayout;
		Interface::GetPackedLayout(kBox.m_kSize, pFormat, &kPackedLayout);

		DXGL_TODO("Check if it's worth to keep an allocation pool");
		kMappedSubTex.m_pBuffer = static_cast<uint8*>(Memalign(kPackedLayout.m_uTextureSize, MIN_MAPPED_RESOURCE_ALIGNMENT));
		kMappedSubTex.m_uRowPitch = kPackedLayout.m_uRowPitch;
		kMappedSubTex.m_uImagePitch = kPackedLayout.m_uImagePitch;
		kMappedSubTex.m_uDataOffset = 0;

		if (bDownload)
			DownloadImage(pTexture, kSubID, kBox, kMappedSubTex.m_pBuffer, kPackedLayout.m_uRowPitch, kPackedLayout.m_uImagePitch, pContext, pFormat);
	}

	static void Unmap(STexture* pTexture, STexSubresourceID kSubID, const SMappedSubTexture& kMappedSubTex, CContext* pContext, const SGIFormatInfo* pFormat)
	{
		if (kMappedSubTex.m_bUpload)
		{
			STexBox kBox;
			GetTextureBox(kBox, pTexture, kSubID.m_iMipLevel, pFormat, true);

			UploadImage(pTexture, kSubID, kBox, kMappedSubTex.m_pBuffer, kMappedSubTex.m_uRowPitch, kMappedSubTex.m_uImagePitch, pContext, pFormat);
		}

		MemalignFree(kMappedSubTex.m_pBuffer);
	}
};

template<typename Interface, typename Partition = STexPartition>
struct SArrayTexImpl : Interface
{
	static void InitializeStorage(STexture* pTexture, uint32, const SGIFormatInfo* pFormat, CContext* pContext)
	{
#if DXGL_USE_IMMUTABLE_TEXTURES
		STexSize kTexSize(GetMipSize(pTexture, 0, pFormat, false));
		Interface::SetLayerComponent(kTexSize, (GLsizei)pTexture->m_uNumElements);
		Interface::TexStorage(pTexture, kTexSize, pTexture->m_uNumMipLevels, pFormat->m_pTexture);
#else
		GLint iLevel;
		for (iLevel = 0; iLevel < (GLint)pTexture->m_uNumMipLevels; ++iLevel)
		{
			STexSize kMipSize(GetMipSize(pTexture, iLevel, pFormat, false));
			Interface::SetLayerComponent(kMipSize, (GLsizei)pTexture->m_uNumElements);
			Interface::TexImage(pTexture, pTexture->m_eTarget, iLevel, kMipSize, 0, pFormat->m_pTexture, NULL);
		}
#endif

#if DXGL_SUPPORT_COPY_IMAGE
		Partition::InitializeCopyImageView(pTexture, pFormat, pContext);
#endif //DXGL_SUPPORT_COPY_IMAGE

		LogTextureSize<Interface, Partition>(pTexture);
	}

	static void UploadImage(STexture* pTexture, STexSubresourceID kSubID, STexBox kBox, const void* pSrcData, uint32 uSrcRowPitch, uint32 uSrcDepthPitch, CContext* pContext, const SGIFormatInfo* pFormat)
	{
		GLenum eSubTarget;
		GLint iLayer;
		Partition::GetSubTargetAndLayer(pTexture, kSubID.m_uElement, eSubTarget, iLayer);

		Interface::SetLayerComponent(kBox.m_kOffset, iLayer);
		Interface::SetLayerComponent(kBox.m_kSize, 1);

		Interface::ApplyUnpackState(pContext, kBox.m_kSize, countTrailingZeros32((unsigned int)reinterpret_cast<uintptr_t>(pSrcData)), uSrcRowPitch, uSrcDepthPitch, pFormat);
		TestUploadBounds<Interface>(pTexture, eSubTarget, kSubID.m_iMipLevel, kBox, pFormat, pSrcData, uSrcRowPitch, uSrcDepthPitch);
		Interface::TexSubImage(pTexture, eSubTarget, kSubID.m_iMipLevel, kBox, pFormat->m_pTexture, pSrcData);
	}

	static void DownloadImage(STexture* pTexture, STexSubresourceID kSubID, STexBox kBox, void* pDstData, uint32 uDstRowPitch, uint32 uDstDepthPitch, CContext* pContext, const SGIFormatInfo* pFormat)
	{
		Interface::ApplyPackState(pContext, kBox.m_kSize, countTrailingZeros32((unsigned int)reinterpret_cast<uintptr_t>(pDstData)), uDstRowPitch, uDstDepthPitch, pFormat);

#if DXGL_SUPPORT_GETTEXIMAGE
		GLenum eSubTarget;
		GLint iLayer;
		Partition::GetSubTargetAndLayer(pTexture, kSubID.m_uElement, eSubTarget, iLayer);

		Interface::GetTexImage(pTexture, eSubTarget, kSubID.m_iMipLevel, pDstData, pContext, pFormat);
#else
		DXGL_NOT_IMPLEMENTED
#endif
	}

	static void Map(STexture* pTexture, STexSubresourceID kSubID, bool bDownload, SMappedSubTexture& kMappedSubTex, CContext* pContext, const SGIFormatInfo* pFormat)
	{
		GLenum eSubTarget;
		GLint iLayer;
		Partition::GetSubTargetAndLayer(pTexture, kSubID.m_uElement, eSubTarget, iLayer);

		STexBox kBox;
		GetTextureBox(kBox, pTexture, kSubID.m_iMipLevel, pFormat, true);

		uint32 uNumTextures(1);
		uint32 uTextureIndex(0);

		if (bDownload)
		{
			// The box must include all layers since glGetTexImage can only copy all layers at once.
			// This is quite a waste, but as of 4.3 there's no way to only download a single layer
			// (excluding glReadPixels that only works for renderable formats, i.e. not compressed formats).
			Interface::SetLayerComponent(kBox.m_kSize, (GLsizei)Partition::GetNumLayers(pTexture));
		}

		SPackedLayout kPackedLayout;
		Interface::GetPackedLayout(kBox.m_kSize, pFormat, &kPackedLayout);

		DXGL_TODO("Check if it's worth to keep an allocation pool");
		kMappedSubTex.m_pBuffer = static_cast<uint8*>(Memalign(kPackedLayout.m_uTextureSize, MIN_MAPPED_RESOURCE_ALIGNMENT));
		kMappedSubTex.m_uRowPitch = kPackedLayout.m_uRowPitch;
		kMappedSubTex.m_uImagePitch = kPackedLayout.m_uImagePitch;
		kMappedSubTex.m_uDataOffset = 0;
		if (bDownload)
		{
			kMappedSubTex.m_uDataOffset = iLayer * kPackedLayout.m_uImagePitch;
			DownloadImage(pTexture, kSubID, kBox, kMappedSubTex.m_pBuffer, kPackedLayout.m_uRowPitch, kPackedLayout.m_uImagePitch, pContext, pFormat);
		}
	}

	static void Unmap(STexture* pTexture, STexSubresourceID kSubID, const SMappedSubTexture& kMappedSubTex, CContext* pContext, const SGIFormatInfo* pFormat)
	{
		if (kMappedSubTex.m_bUpload)
		{
			GLenum eSubTarget;
			GLint iLayer;
			Partition::GetSubTargetAndLayer(pTexture, kSubID.m_uElement, eSubTarget, iLayer);

			STexBox kBox;
			GetTextureBox(kBox, pTexture, kSubID.m_iMipLevel, pFormat, true);

			GLvoid* pData(kMappedSubTex.m_pBuffer + kMappedSubTex.m_uDataOffset);
			UploadImage(pTexture, kSubID, kBox, pData, kMappedSubTex.m_uRowPitch, kMappedSubTex.m_uImagePitch, pContext, pFormat);
		}

		MemalignFree(kMappedSubTex.m_pBuffer);
	}
};

template<typename Interface>
uint32 GetSystemMemoryTextureOffset(STexture* pTexture, const SGIFormatInfo* pFormat, STexSubresourceID kID)
{
	uint32 uOffset(0);
	uint32 uTotSize(0);
	uint32 uLevel;
	for (uLevel = 0; uLevel < pTexture->m_uNumMipLevels; ++uLevel)
	{
		STexSize kLevelSize(GetMipSize(pTexture, (GLint)uLevel, pFormat, true));

		SPackedLayout kPackedLayout;
		Interface::GetPackedLayout(kLevelSize, pFormat, &kPackedLayout);

		uTotSize += kPackedLayout.m_uTextureSize;

		// Keep every subresource aligned so that it can be directly mapped
		uTotSize += MIN_MAPPED_RESOURCE_ALIGNMENT - 1;
		uTotSize -= (uTotSize % MIN_MAPPED_RESOURCE_ALIGNMENT);
		if (uLevel < kID.m_iMipLevel)
			uOffset = uTotSize;
	}

	return uTotSize * kID.m_uElement + uOffset;
}

template<typename Interface>
struct SStagingTexImpl : Interface
{
	static void AllocateResource(STexture* pTexture, CDevice* pDevice)
	{
#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
		uint32 uNumPixelBuffers(pTexture->m_uNumMipLevels * pTexture->m_uNumElements);
		pTexture->m_akPixelBuffers = new CResourceName[uNumPixelBuffers];
		for (uint32 uPixelBuffer = 0; uPixelBuffer < uNumPixelBuffers; ++uPixelBuffer)
		{
			GLuint uPixelBufferName;
			glGenBuffers(1, &uPixelBufferName);
			pTexture->m_akPixelBuffers[uPixelBuffer] = pDevice->GetBufferNamePool().Create(uPixelBufferName);
		}
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES
	}

	static void InitializeStorage(STexture* pTexture, uint32 uCPUAccess, const SGIFormatInfo* pFormat, CContext* pContext)
	{
#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
		GLenum eUsage;
		if ((uCPUAccess & D3D11_CPU_ACCESS_WRITE) == 0)
			eUsage = GL_STREAM_READ;
		else if ((uCPUAccess & D3D11_CPU_ACCESS_READ) == 0)
			eUsage = GL_STREAM_DRAW;
		else
			eUsage = GL_STREAM_COPY;

		STexSubresourceID kSubID;
		for (kSubID.m_iMipLevel = 0; kSubID.m_iMipLevel < pTexture->m_uNumMipLevels; ++kSubID.m_iMipLevel)
		{
			STexSize kLevelSize(GetMipSize(pTexture, kSubID.m_iMipLevel, pFormat, true));

			SPackedLayout kPackedLayout;
			Interface::GetPackedLayout(kLevelSize, pFormat, &kPackedLayout);

			for (kSubID.m_uElement = 0; kSubID.m_uElement < pTexture->m_uNumElements; ++kSubID.m_uElement)
			{
				uint32 uSubResource(D3D11CalcSubresource(kSubID.m_iMipLevel, kSubID.m_uElement, pTexture->m_uNumMipLevels));
				glNamedBufferDataEXT(pTexture->m_akPixelBuffers[uSubResource].GetName(), kPackedLayout.m_uTextureSize, NULL, eUsage);
			}
		}
#else
		STexSubresourceID kEndID = { pTexture->m_uNumMipLevels, pTexture->m_uNumElements };
		uint32 uMappedSize(GetSystemMemoryTextureOffset<Interface>(pTexture, pFormat, kEndID));
		pTexture->m_pSystemMemoryCopy = static_cast<uint8*>(Memalign(uMappedSize, MIN_MAPPED_RESOURCE_ALIGNMENT));
#endif
	}

	static void UploadImage(STexture* pTexture, STexSubresourceID kSubID, STexBox kBox, const void* pSrcData, uint32 uSrcRowPitch, uint32 uSrcDepthPitch, CContext* pContext, const SGIFormatInfo* pFormat)
	{
		STexSize kDstSize(GetMipSize(pTexture, kSubID.m_iMipLevel, pFormat, true));
		SPackedLayout kDstLayout;
		Interface::GetPackedLayout(kDstSize, pFormat, &kDstLayout);

		STexBox kPackedRange;
		if (!Interface::GetPackedRange(kBox, &kPackedRange, pFormat))
		{
			DXGL_ERROR("Texture upload box is not compatible with the format of the texture");
			return;
		}

#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
		uint32 uSubResource(D3D11CalcSubresource(kSubID.m_iMipLevel, kSubID.m_uElement, pTexture->m_uNumMipLevels));
		GLint iPixelBufferName(pTexture->m_akPixelBuffers[uSubResource].GetName());
		GLvoid* pvMappedData(glMapNamedBufferRangeEXT(iPixelBufferName, 0, (GLsizei)kDstLayout.m_uTextureSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT));
		uint8* puDstData(static_cast<uint8*>(pvMappedData) + kPackedRange.m_kOffset.x);
#else
		uint8* puDstData(
		  pTexture->m_pSystemMemoryCopy +
		  GetSystemMemoryTextureOffset<Interface>(pTexture, pFormat, kSubID) +
		  kPackedRange.m_kOffset.x);
#endif

		const uint8* puSrcData(reinterpret_cast<const uint8*>(pSrcData));

		uint32 uImage, uRow;
		for (uImage = kPackedRange.m_kOffset.z; uImage < kPackedRange.m_kSize.z; ++uImage)
		{
			for (uRow = kPackedRange.m_kOffset.y; uRow < kPackedRange.m_kSize.y; ++uRow)
			{
				NCryOpenGL::Memcpy(
				  puDstData + uImage * kDstLayout.m_uImagePitch + uRow * kDstLayout.m_uRowPitch + kPackedRange.m_kOffset.x,
				  puSrcData + uImage * uSrcDepthPitch + uRow * uSrcRowPitch,
				  kPackedRange.m_kSize.x);
			}
		}

#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
		glUnmapNamedBufferEXT(pTexture->m_akPixelBuffers[uSubResource].GetName());
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES
	}

#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
	static void DownloadImage(STexture* pTexture, STexSubresourceID kSubID, STexBox kBox, const void* pDstData, uint32 uSrcRowPitch, uint32 uSrcDepthPitch, CContext* pContext, const SGIFormatInfo* pFormat)
	{
		DXGL_NOT_IMPLEMENTED
	}
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES

	static void Map(STexture* pTexture, STexSubresourceID kSubID, bool bDownload, SMappedSubTexture& kMappedSubTex, CContext* pContext, const SGIFormatInfo* pFormat)
	{
		STexSize kSubSize(GetMipSize(pTexture, kSubID.m_iMipLevel, pFormat, true));
		SPackedLayout kPackedLayout;
		Interface::GetPackedLayout(kSubSize, pFormat, &kPackedLayout);

#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
		uint32 uSubResource(D3D11CalcSubresource(kSubID.m_iMipLevel, kSubID.m_uElement, pTexture->m_uNumMipLevels));
		GLint iPixelBufferName(pTexture->m_akPixelBuffers[uSubResource].GetName());
		if (bDownload)
			kMappedSubTex.m_pBuffer = static_cast<uint8*>(glMapNamedBufferRangeEXT(iPixelBufferName, 0, kPackedLayout.m_uTextureSize, GL_MAP_READ_BIT));
		else
			kMappedSubTex.m_pBuffer = static_cast<uint8*>(glMapNamedBufferRangeEXT(iPixelBufferName, 0, kPackedLayout.m_uTextureSize, GL_MAP_WRITE_BIT));
#else
		kMappedSubTex.m_pBuffer = pTexture->m_pSystemMemoryCopy + GetSystemMemoryTextureOffset<Interface>(pTexture, pFormat, kSubID);
#endif

		kMappedSubTex.m_uRowPitch = kPackedLayout.m_uRowPitch;
		kMappedSubTex.m_uImagePitch = kPackedLayout.m_uImagePitch;
		kMappedSubTex.m_uDataOffset = 0;
	}

	static void Unmap(STexture* pTexture, STexSubresourceID kSubID, const SMappedSubTexture& kMappedSubTex, CContext*, const SGIFormatInfo*)
	{
#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
		uint32 uSubResource(D3D11CalcSubresource(kSubID.m_iMipLevel, kSubID.m_uElement, pTexture->m_uNumMipLevels));
		glUnmapNamedBufferEXT(pTexture->m_akPixelBuffers[uSubResource].GetName());
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES
	}
};

inline STexSubresourceID GetTexSubresourceID(STexture* pTexture, uint32 uSubresource)
{
	STexSubresourceID kID;
	kID.m_iMipLevel = uSubresource % pTexture->m_uNumMipLevels;
	kID.m_uElement = uSubresource / pTexture->m_uNumMipLevels;
	assert(kID.m_uElement < pTexture->m_uNumElements);
	return kID;
}

template<typename Impl>
void UpdateTexSubresource(SResource* pResource, uint32 uSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, uint32 uSrcRowPitch, uint32 uSrcDepthPitch, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("UpdateTexSubresource")

	STexture * pTexture(static_cast<STexture*>(pResource));
	pTexture->m_kCreationFence.IssueWait(pContext);

	const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(pTexture->m_eFormat));
	assert(pFormatInfo != NULL);
	assert(pFormatInfo->m_pTexture != NULL);

	STexSubresourceID kSubID(GetTexSubresourceID(pTexture, uSubresource));

	STexBox kTexBox;
	GetTextureBox(kTexBox, pTexture, kSubID.m_iMipLevel, pDstBox, pFormatInfo, false);
	Impl::UploadImage(pTexture, kSubID, kTexBox, pSrcData, uSrcRowPitch, uSrcDepthPitch, pContext, pFormatInfo);
}

template<typename Impl>
bool MapTexSubresource(SResource* pResource, uint32 uSubresource, D3D11_MAP eMapType, uint32 uMapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("MapTexSubresource")

	STexture * pTexture(static_cast<STexture*>(pResource));
	pTexture->m_kCreationFence.IssueWait(pContext);

	const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(pTexture->m_eFormat));
	assert(pFormatInfo != NULL);
	assert(pFormatInfo->m_pTexture != NULL);

	if (uSubresource >= pTexture->m_kMappedSubTextures.size())
		pTexture->m_kMappedSubTextures.resize(uSubresource + 1);
	SMappedSubTexture& kMappedSubTexture(pTexture->m_kMappedSubTextures.at(uSubresource));

	if (kMappedSubTexture.m_pBuffer != NULL)
	{
		DXGL_ERROR("Texture subresource is already mapped");
		return false;
	}

	bool bDownload(eMapType == D3D11_MAP_READ || eMapType == D3D11_MAP_WRITE || eMapType == D3D11_MAP_READ_WRITE);
	Impl::Map(pTexture, GetTexSubresourceID(pTexture, uSubresource), bDownload, kMappedSubTexture, pContext, pFormatInfo);
	kMappedSubTexture.m_bUpload = (eMapType != D3D11_MAP_READ);

	pMappedResource->pData = kMappedSubTexture.m_pBuffer + kMappedSubTexture.m_uDataOffset;
	pMappedResource->RowPitch = kMappedSubTexture.m_uRowPitch;
	pMappedResource->DepthPitch = kMappedSubTexture.m_uImagePitch;

	return kMappedSubTexture.m_pBuffer != NULL;
}

template<typename Impl>
void UnmapTexSubresource(SResource* pResource, UINT uSubresource, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("UnmapTexSubresource")

	STexture * pTexture(static_cast<STexture*>(pResource));

	const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(pTexture->m_eFormat));
	assert(pFormatInfo != NULL);
	assert(pFormatInfo->m_pTexture != NULL);

	if (uSubresource >= pTexture->m_kMappedSubTextures.size())
		pTexture->m_kMappedSubTextures.resize(uSubresource + 1);
	SMappedSubTexture& kMappedSubTexture(pTexture->m_kMappedSubTextures.at(uSubresource));

	if (kMappedSubTexture.m_pBuffer == NULL)
	{
		DXGL_ERROR("Texture subresource is not mapped");
		return;
	}

	Impl::Unmap(pTexture, GetTexSubresourceID(pTexture, uSubresource), kMappedSubTexture, pContext, pFormatInfo);
	kMappedSubTexture.m_pBuffer = NULL;
}

template<typename Impl>
void UnpackTexData(STexture* pTexture, STexSubresourceID kSubID, STexPos kOffset, STexSize kSize, const SMappedSubTexture& kDataLocation, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("UnpackTexData")

	STexBox kBox;
	kBox.m_kOffset = kOffset;
	kBox.m_kSize = kSize;
	const SGIFormatInfo* pFormat(GetGIFormatInfo(pTexture->m_eFormat));

	Impl::UploadImage(pTexture, kSubID, kBox, kDataLocation.m_pBuffer + kDataLocation.m_uDataOffset, kDataLocation.m_uRowPitch, kDataLocation.m_uImagePitch, pContext, pFormat);
}

template<typename Impl>
void PackTexData(STexture* pTexture, STexSubresourceID kSubID, STexPos kOffset, STexSize kSize, const SMappedSubTexture& kDataLocation, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("PackTexData")

	STexBox kBox;
	kBox.m_kOffset = kOffset;
	kBox.m_kSize = kSize;
	const SGIFormatInfo* pFormat(GetGIFormatInfo(pTexture->m_eFormat));

	Impl::DownloadImage(pTexture, kSubID, kBox, kDataLocation.m_pBuffer + kDataLocation.m_uDataOffset, kDataLocation.m_uRowPitch, kDataLocation.m_uImagePitch, pContext, pFormat);
}

#if DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE

template<typename Impl>
uint32 LocateTexPackedData(STexture* pTexture, STexSubresourceID kSubID, STexPos kOffset, SMappedSubTexture& kDataLocation)
{
	const SGIFormatInfo* pFormat(GetGIFormatInfo(pTexture->m_eFormat));

	STexBox kBox;
	GetTextureBox(kBox, pTexture, kSubID.m_iMipLevel, pFormat, true);
	SPackedLayout kPackedLayout;
	Impl::GetPackedLayout(kBox.m_kSize, pFormat, &kPackedLayout);

	SPackedLayout kPackedOffset;
	Impl::GetPackedLayout(STexSize((GLsizei)kOffset.x, (GLsizei)kOffset.y, (GLsizei)kOffset.z), pFormat, &kPackedOffset);

	kDataLocation.m_pBuffer = NULL;
	kDataLocation.m_uDataOffset = kPackedOffset.m_uTextureSize;
	kDataLocation.m_uRowPitch = kPackedLayout.m_uRowPitch;
	kDataLocation.m_uImagePitch = kPackedLayout.m_uImagePitch;

	return kPackedLayout.m_uTextureSize;
}

#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE

template<typename Impl>
void InitializeTexture(STexture* pTexture, const D3D11_SUBRESOURCE_DATA* pInitialData, uint32 uCPUAccess, CContext* pContext, const SGIFormatInfo* pFormatInfo)
{
	pTexture->m_pfUpdateSubresource = &UpdateTexSubresource<Impl>;
	pTexture->m_pfMapSubresource = &MapTexSubresource<Impl>;
	pTexture->m_pfUnmapSubresource = &UnmapTexSubresource<Impl>;
#if DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE
	pTexture->m_pfUnpackData = &UnpackTexData<Impl>;
	pTexture->m_pfPackData = &PackTexData<Impl>;
	pTexture->m_pfLocatePackedDataFunc = &LocateTexPackedData<Impl>;
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE

	Impl::AllocateResource(pTexture, pContext->GetDevice());

	Impl::InitializeStorage(pTexture, uCPUAccess, pFormatInfo, pContext);

	if (pInitialData)
	{
		STexBox kMipBox;
		kMipBox.m_kOffset = STexPos(0, 0, 0);

		STexSubresourceID kSubID;
		for (kSubID.m_uElement = 0; kSubID.m_uElement < pTexture->m_uNumElements; ++kSubID.m_uElement)
		{
			for (kSubID.m_iMipLevel = 0; kSubID.m_iMipLevel < (GLint)pTexture->m_uNumMipLevels; ++kSubID.m_iMipLevel)
			{
				kMipBox.m_kSize = GetMipSize(pTexture, kSubID.m_iMipLevel, pFormatInfo, false);

				Impl::UploadImage(pTexture, kSubID, kMipBox, pInitialData->pSysMem, pInitialData->SysMemPitch, pInitialData->SysMemSlicePitch, pContext, pFormatInfo);

				++pInitialData;
			}
		}
	}

	pTexture->m_kCreationFence.IssueFence(pContext->GetDevice());
}

GLenum SFrameBufferConfiguration::AttachmentIndexToID(uint32 uIndex)
{
	if (uIndex >= FIRST_COLOR_ATTACHMENT_INDEX &&
	    uIndex < FIRST_COLOR_ATTACHMENT_INDEX + MAX_COLOR_ATTACHMENTS)
		return GL_COLOR_ATTACHMENT0 + uIndex - FIRST_COLOR_ATTACHMENT_INDEX;

	if (uIndex == DEPTH_ATTACHMENT_INDEX)
		return GL_DEPTH_ATTACHMENT;

	if (uIndex == STENCIL_ATTACHMENT_INDEX)
		return GL_STENCIL_ATTACHMENT;

	return GL_NONE;
}

uint32 SFrameBufferConfiguration::AttachmentIDToIndex(GLenum eID)
{
	if (eID >= GL_COLOR_ATTACHMENT0 &&
	    eID < GL_COLOR_ATTACHMENT0 + MAX_COLOR_ATTACHMENTS)
		return FIRST_COLOR_ATTACHMENT_INDEX + eID - GL_COLOR_ATTACHMENT0;

	if (eID == GL_DEPTH_ATTACHMENT)
		return DEPTH_ATTACHMENT_INDEX;

	if (eID == GL_STENCIL_ATTACHMENT)
		return STENCIL_ATTACHMENT_INDEX;

	return MAX_ATTACHMENTS;
}

SSharingFence::SSharingFence()
#if DXGL_SHARED_OBJECT_SYNCHRONIZATION
	: m_pFence(NULL)
#endif //DXGL_SHARED_OBJECT_SYNCHRONIZATION
{
}

SSharingFence::~SSharingFence()
{
#if DXGL_SHARED_OBJECT_SYNCHRONIZATION
	if (m_pFence != NULL)
		glDeleteSync(m_pFence);
#endif //DXGL_SHARED_OBJECT_SYNCHRONIZATION
}

void SSharingFence::IssueFence(CDevice*, bool bAvoidFlushing)
{
#if DXGL_SHARED_OBJECT_SYNCHRONIZATION
	assert(m_pFence == NULL);
	m_akWaitIssued.SetZero();
	m_pFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	DXGL_TODO(
	  "Check whether flushing here has performance implications, and if so find "
	  "a place that would make it less frequent. It can be avoided on drivers "
	  "that prehemptively flush every now and then, otherwise IssueWait could wait "
	  "indefinitely (or expire on server side, failing to synchronize).");
	if (!bAvoidFlushing)
		glFlush();
#endif //DXGL_SHARED_OBJECT_SYNCHRONIZATION
}

void SSharingFence::IssueWait(CContext* pContext)
{
#if DXGL_SHARED_OBJECT_SYNCHRONIZATION
	if (m_pFence && !m_akWaitIssued.Get(pContext->GetIndex()))
	{
	#if DXGL_SERVER_SIDE_SHARED_OBJECT_SYNCHRONIZATION
		glWaitSync(m_pFence, 0, GL_TIMEOUT_IGNORED);
		m_akWaitIssued.Set(pContext->GetIndex(), true);
	#else
		while (glClientWaitSync(m_pFence, 0, 100) == GL_TIMEOUT_EXPIRED)
		{
		}
		m_akWaitIssued.SetOne();
	#endif
	}
#endif //DXGL_SHARED_OBJECT_SYNCHRONIZATION
}

SResourceNamePool::SResourceNamePool()
	: m_pBlocksHead(NULL)
{
}

SResourceNamePool::~SResourceNamePool()
{
	LockCriticalSection(&m_kBlockSection);

	while (m_pBlocksHead != NULL)
	{
		TBlockHeader* pBlockHeader(alias_cast<TBlockHeader*>(m_pBlocksHead));
		TSlot* pNextBlock(pBlockHeader->m_pNext);
		if (DecrementResourceNameRefCount(pBlockHeader->m_uRefCount))
		{
			TSlot* pSlot(m_pBlocksHead + NUM_HEADER_SLOTS + 1);
			TSlot* pEnd(pSlot + pBlockHeader->m_uSize);
			while (pSlot < pEnd)
			{
				pSlot->~TSlot();
				++pSlot;
			}

			pBlockHeader->~TBlockHeader();
			Free(m_pBlocksHead);
		}
		m_pBlocksHead = pNextBlock;
	}

	UnlockCriticalSection(&m_kBlockSection);
}

CResourceName SResourceNamePool::Create(GLuint uName)
{
	TSlot* pBlock(m_pBlocksHead);
	while (pBlock != NULL)
	{
		TBlockHeader* pBlockHeader(alias_cast<TBlockHeader*>(pBlock));
		TSlot* pNextBlock(pBlockHeader->m_pNext);
		TSlot*        pFreeSlot(alias_cast<TSlot*>(pBlockHeader->m_kFreeList.Pop()));
		if (pFreeSlot != NULL)
		{
			pFreeSlot->m_kUsed.m_pBlock = pBlock;
			pFreeSlot->m_kUsed.m_uName = uName;
			pFreeSlot->m_kUsed.m_uRefCount = 0;
			return CResourceName(pFreeSlot);
		}
		pBlock = pNextBlock;
	}

	LockCriticalSection(&m_kBlockSection);

	uint32 uNewBlockSize(m_pBlocksHead == NULL ? MIN_BLOCK_SIZE : alias_cast<TBlockHeader*>(m_pBlocksHead)->m_uSize * GROWTH_FACTOR);
	TSlot* pNewBlock(static_cast<TSlot*>(Malloc(sizeof(TSlot) * (NUM_HEADER_SLOTS + max(1u, uNewBlockSize)))));

	TBlockHeader* pNewBlockHeader(alias_cast<TBlockHeader*>(pNewBlock));
	new(pNewBlockHeader) TBlockHeader();
	pNewBlockHeader->m_pNext = m_pBlocksHead;
	pNewBlockHeader->m_pPool = this;
	pNewBlockHeader->m_uRefCount = 1;
	pNewBlockHeader->m_uSize = uNewBlockSize;
	m_pBlocksHead = pNewBlock;

	UnlockCriticalSection(&m_kBlockSection);

	TSlot* pFreeSlot(pNewBlock + NUM_HEADER_SLOTS + 1);
	TSlot* pFreeSlotsEnd(pNewBlock + uNewBlockSize);
	while (pFreeSlot != pFreeSlotsEnd)
	{
		new(pFreeSlot) TSlot;
		pNewBlockHeader->m_kFreeList.Push(pFreeSlot->m_kNextFree);
		++pFreeSlot;
	}

	TSlot* pUsedSlot(pNewBlock + NUM_HEADER_SLOTS);
	new(pUsedSlot) TSlot;
	pUsedSlot->m_kUsed.m_pBlock = pNewBlock;
	pUsedSlot->m_kUsed.m_uName = uName;
	pUsedSlot->m_kUsed.m_uRefCount = 0;
	return CResourceName(pUsedSlot);
}

SResource::SResource()
	: m_pfUpdateSubresource(NULL)
	, m_pfMapSubresource(NULL)
	, m_pfUnmapSubresource(NULL)
{
}

SResource::SResource(const SResource& kOther)
	: m_kName(kOther.m_kName)
	, m_pfUpdateSubresource(kOther.m_pfUpdateSubresource)
	, m_pfMapSubresource(kOther.m_pfMapSubresource)
	, m_pfUnmapSubresource(kOther.m_pfUnmapSubresource)
{

}

SResource::~SResource()
{
}

SFrameBufferObject::SFrameBufferObject()
	: m_bUsesSRGB(false)
{
}

SFrameBufferObject::~SFrameBufferObject()
{
}

SFrameBuffer::SFrameBuffer(const SFrameBufferConfiguration& kConfiguration)
	: m_kConfiguration(kConfiguration)
	, m_pContext(nullptr)
	, m_uNumDrawBuffers(0)
{
	ZeroArray(m_aeDrawBuffers);
}

SFrameBuffer::~SFrameBuffer()
{
	if (m_kObject.m_kName.IsValid() &&
	    m_kObject.m_kName.GetName() != 0) // Index 0 is reserved for default FBO
	{
		GLuint uName(m_kObject.m_kName.GetName());
		glDeleteFramebuffers(1, &uName);
	}
}

void PrintFBOInfo(SFrameBuffer* pFrameBuffer, bool error = true)
{
	static FILE* s_pFile(fopen("fbo_info.txt", "w"));
	GLuint uFBOName(pFrameBuffer->m_kObject.m_kName.GetName());
	fprintf(s_pFile, "Frame buffer %u Error: %d\n", uFBOName, error);
	uint32 uAttachment;
	for (uAttachment = 0; uAttachment < SFrameBufferConfiguration::MAX_ATTACHMENTS; ++uAttachment)
	{
		if (pFrameBuffer->m_kConfiguration.m_akAttachments[uAttachment] != NULL)
		{
			GLenum eAttachmentID(SFrameBufferConfiguration::AttachmentIndexToID(uAttachment));
			fprintf(s_pFile, "\tAttachment #%u (0x%x) ", uFBOName, eAttachmentID);
			GLint iParams[16], iTexName, iTexLevel;
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, iParams);
			fprintf(s_pFile, "OBJECT_TYPE=0x%x ", iParams[0]);
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, iParams);
			fprintf(s_pFile, "OBJECT_NAME=0x%x ", iParams[0]);
			iTexName = iParams[0];
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, iParams);
			fprintf(s_pFile, "TEXTURE_LEVEL=0x%x ", iParams[0]);
			iTexLevel = iParams[0];
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, iParams);
			fprintf(s_pFile, "TEXTURE_CUBE_MAP_FACE=0x%x ", iParams[0]);
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, iParams);
#if !DXGLES
			fprintf(s_pFile, "TEXTURE_LAYER=0x%x ", iParams[0]);
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_LAYERED, iParams);
#endif
			fprintf(s_pFile, "LAYERED=0x%x ", iParams[0]);
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, iParams);
			fprintf(s_pFile, "COLOR_ENCODING=0x%x ", iParams[0]);
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, iParams);
			fprintf(s_pFile, "COMPONENT_TYPE=0x%x ", iParams[0]);
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, iParams);
			fprintf(s_pFile, "R=%d ", iParams[0]);
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, iParams);
			fprintf(s_pFile, "G=%d ", iParams[0]);
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, iParams);
			fprintf(s_pFile, "B=%d ", iParams[0]);
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, iParams);
			fprintf(s_pFile, "A=%d ", iParams[0]);
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, iParams);
			fprintf(s_pFile, "D=%d ", iParams[0]);
			glGetNamedFramebufferAttachmentParameterivEXT(uFBOName, eAttachmentID, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, iParams);
			fprintf(s_pFile, "S=%d ", iParams[0]);

			fprintf(s_pFile, "\n\t\tTexture: ");
#if !DXGLES
			// These are not available on GL ES
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_WIDTH, iParams);
			fprintf(s_pFile, "WIDTH=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_HEIGHT, iParams);
			fprintf(s_pFile, "HEIGHT=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_DEPTH, iParams);
			fprintf(s_pFile, "DEPTH=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_SAMPLES, iParams);
			fprintf(s_pFile, "SAMPLES=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_FIXED_SAMPLE_LOCATIONS, iParams);
			fprintf(s_pFile, "FIXED_SAMPLE_LOCATIONS=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_INTERNAL_FORMAT, iParams);
			fprintf(s_pFile, "FIXED_SAMPLE_LOCATIONS=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_RED_SIZE, iParams);
			fprintf(s_pFile, "R=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_GREEN_SIZE, iParams);
			fprintf(s_pFile, "G=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_BLUE_SIZE, iParams);
			fprintf(s_pFile, "B=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_ALPHA_SIZE, iParams);
			fprintf(s_pFile, "A=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_DEPTH_SIZE, iParams);
			fprintf(s_pFile, "D=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_STENCIL_SIZE, iParams);
			fprintf(s_pFile, "S=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_SHARED_SIZE, iParams);
			fprintf(s_pFile, "SHR=%d ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_RED_TYPE, iParams);
			fprintf(s_pFile, "RT=0x%x ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_GREEN_TYPE, iParams);
			fprintf(s_pFile, "GT=0x%x ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_BLUE_TYPE, iParams);
			fprintf(s_pFile, "BT=0x%x ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_ALPHA_TYPE, iParams);
			fprintf(s_pFile, "AT=0x%x ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_DEPTH_TYPE, iParams);
			fprintf(s_pFile, "DT=0x%x ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_COMPRESSED, iParams);
			fprintf(s_pFile, "COMP=0x%x ", iParams[0]);
			glGetTextureLevelParameterivEXT(iTexName, GL_TEXTURE_2D, iTexLevel, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, iParams);
#endif
			fprintf(s_pFile, "COMP_SIZE=%d ", iParams[0]);
		}
		fprintf(s_pFile, "\n");
		fflush(s_pFile);
	}
}

bool SFrameBuffer::Validate()
{
	GLenum eStatus(glCheckNamedFramebufferStatusEXT(m_kObject.m_kName.GetName(), GL_DRAW_FRAMEBUFFER));
	if (eStatus != GL_FRAMEBUFFER_COMPLETE)
	{
#if DXGLES
		switch (eStatus)
		{
		case GL_FRAMEBUFFER_UNDEFINED:
			DXGL_ERROR("Framebuffer is not complete: Default framebuffer does not exist");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			DXGL_ERROR("Framebuffer is not complete: Any of the framebuffer attachment points are framebuffer incomplete");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			DXGL_ERROR("Framebuffer is not complete: Does not have at least one image attached to it");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			DXGL_ERROR("Framebuffer is not complete: Unsupported framebuffer setup, check settings.");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
			DXGL_ERROR("Framebuffer is not complete: Mismatch in multisample setup.");
			break;
		default:
			DXGL_ERROR("Framebuffer is not complete (unknown status = %X)", eStatus);
		}
#else
		DXGL_ERROR("Framebuffer is not complete (status = %X)", eStatus);
#endif
		return false;
	}
	return true;
}

void STextureState::ApplyFormatMode(GLuint uTexture, GLenum eTarget)
{
	GLint aiSwizzleRGBA[4];
	SwizzleMaskToRGBA(m_uSwizzleMask, aiSwizzleRGBA);
#if !DXGLES
	glTextureParameterivEXT(uTexture, eTarget, GL_TEXTURE_SWIZZLE_RGBA, aiSwizzleRGBA);
#else
	glTextureParameterivEXT(uTexture, eTarget, GL_TEXTURE_SWIZZLE_R, &aiSwizzleRGBA[0]);
	glTextureParameterivEXT(uTexture, eTarget, GL_TEXTURE_SWIZZLE_G, &aiSwizzleRGBA[1]);
	glTextureParameterivEXT(uTexture, eTarget, GL_TEXTURE_SWIZZLE_B, &aiSwizzleRGBA[2]);
	glTextureParameterivEXT(uTexture, eTarget, GL_TEXTURE_SWIZZLE_A, &aiSwizzleRGBA[3]);
#endif

#if DXGL_SUPPORT_STENCIL_TEXTURES
	if (m_iDepthStencilTextureMode)
		glTextureParameteriEXT(uTexture, eTarget, GL_DEPTH_STENCIL_TEXTURE_MODE, m_iDepthStencilTextureMode);
#endif //DXGL_SUPPORT_STENCIL_TEXTURES
}

void STextureState::Apply(GLuint uTexture, GLenum eTarget)
{
	ApplyFormatMode(uTexture, eTarget);

	glTextureParameteriEXT(uTexture, eTarget, GL_TEXTURE_BASE_LEVEL, m_iBaseMipLevel);
	glTextureParameteriEXT(uTexture, eTarget, GL_TEXTURE_MAX_LEVEL, m_iMaxMipLevel);
}

STexture::STexture(GLsizei iWidth, GLsizei iHeight, GLsizei iDepth, GLenum eTarget, EGIFormat eFormat, uint32 uNumMipLevels, uint32 uNumElements)
	: m_iWidth(iWidth)
	, m_iHeight(iHeight)
	, m_iDepth(iDepth)
	, m_eTarget(eTarget)
	, m_eFormat(eFormat)
	, m_uNumMipLevels(uNumMipLevels)
	, m_uNumElements(uNumElements)
#if DXGL_SUPPORT_COPY_IMAGE
	, m_eCopyImageTarget(0)
#endif //DXGL_SUPPORT_COPY_IMAGE
#if DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE
	, m_pfUnpackData(nullptr)
	, m_pfPackData(nullptr)
	, m_pfLocatePackedDataFunc(nullptr)
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE
#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
	, m_akPixelBuffers(nullptr)
#else
	, m_pSystemMemoryCopy(nullptr)
#endif
	, m_pShaderViewsHead(nullptr)
	, m_pOutputMergerViewsHead(nullptr)
	, m_pBoundModifier(nullptr)
{
	ZeroStruct(m_kCache);

#if DXGL_FULL_EMULATION
	m_uNumElements = max(1u, m_uNumElements);
#endif //DXGL_FULL_EMULATION
}

STexture::~STexture()
{
#if DXGL_SUPPORT_COPY_IMAGE
	if (m_kCopyImageView.IsValid())
	{
		GLuint uViewName(m_kCopyImageView.GetName());
		if (uViewName != m_kName.GetName())
			glDeleteTextures(1, &uViewName);
	}
#endif //DXGL_SUPPORT_COPY_IMAGE

	if (m_kName.IsValid())
	{
		GLuint uName(m_kName.GetName());
		glDeleteTextures(1, &uName);
	}

#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
	if (m_akPixelBuffers != NULL)
	{
		CResourceName* pPixelBuffersEnd(m_akPixelBuffers + m_uNumElements * m_uNumMipLevels);
		for (CResourceName* pPixelBuffer = m_akPixelBuffers; pPixelBuffer < pPixelBuffersEnd; ++pPixelBuffer)
		{
			GLuint uName(pPixelBuffer->GetName());
			glDeleteBuffers(1, &uName);
		}
		delete[] m_akPixelBuffers;
	}
#else
	if (m_pSystemMemoryCopy != NULL)
		MemalignFree(m_pSystemMemoryCopy);
#endif
}

SShaderTextureViewPtr STexture::CreateShaderResourceView(const SShaderTextureViewConfiguration& kConfiguration, CContext* pContext)
{
	DXGL_TODO("This is not thread-safe, as multiple threads can create shader views for the same texture. Add synchronization primitive.");

	SShaderTextureViewPtr spView(new SShaderTextureView(kConfiguration));
	if (!spView->Init(this, pContext))
		return NULL;
	return spView;
}

SShaderImageViewPtr STexture::CreateUnorderedAccessView(const SShaderImageViewConfiguration& kConfiguration, CContext* pContext)
{
	DXGL_TODO("This is not thread-safe, as multiple threads can create unordered access view for the same texture. Add synchronization primitive.");

	SShaderImageViewPtr spView(new SShaderImageView(kConfiguration));
	if (!spView->Init(this, pContext))
		return NULL;
	return spView;
}

SOutputMergerTextureViewPtr STexture::CreateOutputMergerView(const SOutputMergerTextureViewConfiguration& kConfiguration, CContext* pContext)
{
	SOutputMergerTextureViewPtr spView(new SOutputMergerTextureView(this, kConfiguration));
	if (!spView->Init(pContext))
		return NULL;
	return spView;
}

SOutputMergerTextureViewPtr STexture::GetCompatibleOutputMergerView(const SOutputMergerTextureViewConfiguration& kConfiguration, CContext* pContext)
{
	DXGL_TODO("This is not thread-safe, as multiple threads can create output merger views for the same texture. Add synchronization primitive.");

	SOutputMergerTextureView* pExistingView(m_pOutputMergerViewsHead);
	while (pExistingView != NULL)
	{
		if (pExistingView->m_kConfiguration == kConfiguration)
			return pExistingView;
		pExistingView = pExistingView->m_pNextView;
	}

	return CreateOutputMergerView(kConfiguration, pContext);
}

#if DXGL_SUPPORT_APITRACE && CRY_PLATFORM_WINDOWS

	#if CRY_PLATFORM_WINDOWS
uint32 SDynamicBufferCopy::ms_uPageSize = 0;
	#endif // CRY_PLATFORM_WINDOWS

SDynamicBufferCopy::SDynamicBufferCopy()
	: m_pData(NULL)
	, m_uSize(0)
	#if CRY_PLATFORM_WINDOWS
	, m_apDirtyPages(NULL)
	, m_uNumPages(0)
	#endif // CRY_PLATFORM_WINDOWS
{
}

SDynamicBufferCopy::~SDynamicBufferCopy()
{
	Free();
}

uint8* SDynamicBufferCopy::Allocate(uint32 uSize)
{
	m_uSize = uSize;
	#if CRY_PLATFORM_WINDOWS
	if (ms_uPageSize == 0)
	{
		SYSTEM_INFO kInfo;
		GetSystemInfo(&kInfo);
		ms_uPageSize = kInfo.dwPageSize;
	}

	m_uNumPages = (uSize + ms_uPageSize - 1) / ms_uPageSize;
	m_pData = reinterpret_cast<uint8*>(VirtualAlloc(NULL, m_uNumPages * ms_uPageSize, MEM_COMMIT | MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE));
	m_apDirtyPages = new void*[m_uNumPages];
	return m_pData;
	#else
		#error "Not implemented on this platform"
	#endif
}

void SDynamicBufferCopy::Free()
{
	#if CRY_PLATFORM_WINDOWS
	if (m_pData != NULL)
		VirtualFree(m_pData, 0, MEM_RELEASE);
	delete[] m_apDirtyPages;
	m_apDirtyPages = NULL;
	#else
		#error "Not implemented on this platform"
	#endif
	m_pData = NULL;
}

void SDynamicBufferCopy::ResetDirtyRegion()
{
	#if CRY_PLATFORM_WINDOWS
	ResetWriteWatch(m_pData, m_uNumPages * ms_uPageSize);
	#else
		#error "Not implemented on this platform"
	#endif
}

bool SDynamicBufferCopy::GetDirtyRegion(uint32& uOffset, uint32& uSize)
{
	#if CRY_PLATFORM_WINDOWS
	ULONG uGranularity;
	ULONG_PTR uCount(m_uNumPages);
	if (GetWriteWatch(0, m_pData, m_uNumPages * ms_uPageSize, m_apDirtyPages, &uCount, &uGranularity) != 0)
		return false;

	uint32 uMin(0xFFFFFFFF), uMax(0);
	uint32 uChange;
	for (uChange = 0; uChange < uCount; ++uChange)
	{
		assert(m_apDirtyPages[uChange] >= m_pData);
		uint32 uOffset((uint32)(reinterpret_cast<char*>(m_apDirtyPages[uChange]) - reinterpret_cast<char*>(m_pData)));
		uMin = std::min(uMin, uOffset);
		uMax = std::max(uMax, uOffset);
	}

	if (uMin > uMax)
	{
		uSize = 0;
		uOffset = 0;
	}
	else
	{
		uMax = std::min(uMax + ms_uPageSize, m_uSize) - 1;

		uOffset = uMin;
		uSize = uMax - uMin + 1;
	}

	return true;
	#else
		#error "Not implemented on this platform"
	#endif
}

#endif //DXGL_SUPPORT_APITRACE

SBuffer::SBuffer()
	: m_pfMapBufferRange(nullptr)
	, m_eUsage(0)
	, m_uSize(0)
	, m_uElementSize(0)
	, m_pSystemMemoryCopy(nullptr)
	, m_bMapped(false)
	, m_uMapOffset(0)
	, m_uMapSize(0)
	, m_pTextureBuffersHead(nullptr)
#if DXGL_SUPPORT_APITRACE
	, m_bSystemMemoryCopyOwner(true)
#endif //DXGL_SUPPORT_APITRACE
#if DXGL_STREAMING_CONSTANT_BUFFERS
	#if OGL_SINGLE_CONTEXT
	, m_bContextCacheValid(false)
	#endif
	, m_bStreaming(false)
#endif //DXGL_STREAMING_CONSTANT_BUFFERS
{
}

SBuffer::~SBuffer()
{
	if (m_kName.IsValid())
	{
		GLuint uName(m_kName.GetName());
		glDeleteBuffers(1, &uName);
	}
	if (
#if DXGL_SUPPORT_APITRACE
	  m_bSystemMemoryCopyOwner &&
#endif //DXGL_SUPPORT_APITRACE
	  m_pSystemMemoryCopy != NULL)
		MemalignFree(m_pSystemMemoryCopy);
}

SShaderViewPtr SBuffer::CreateShaderResourceView(EGIFormat eFormat, uint32 uFirstElement, uint32 uNumElements, uint32 uFlags, CContext* pContext)
{
	DXGL_TODO("Consider uFlags as well if required")

	if (m_uElementSize > 0) // Structured buffer
		return CreateBufferView(uFirstElement, uNumElements, pContext);

	SShaderTextureBufferViewConfiguration kTextureBufferConfiguration(eFormat, uFirstElement, uNumElements);
	SShaderTextureBufferViewPtr spTextureBufferView(GetTextureBuffer(kTextureBufferConfiguration, pContext));

	return spTextureBufferView;
}

SShaderViewPtr SBuffer::CreateUnorderedAccessView(EGIFormat eFormat, uint32 uFirstElement, uint32 uNumElements, uint32 uFlags, CContext* pContext)
{
	DXGL_TODO("Consider uFlags as well if required")

	if (m_uElementSize > 0) // Structured buffer
		return CreateBufferView(uFirstElement, uNumElements, pContext);

	SShaderTextureBufferViewConfiguration kTextureBufferConfiguration(eFormat, uFirstElement, uNumElements);
	SShaderTextureBufferViewPtr spTextureBufferView(GetTextureBuffer(kTextureBufferConfiguration, pContext));

	GLenum eImageFormat(GL_NONE);
	if (!GetImageFormat(eFormat, &eImageFormat))
	{
		DXGL_ERROR("Unsupported format for unordered access view");
		return NULL;
	}

	if (spTextureBufferView == nullptr) 
	{
		return nullptr;
	}

	SShaderImageViewPtr spImageView(new SShaderImageView(SShaderImageViewConfiguration(eImageFormat, 0, -1, GL_READ_WRITE)));
	if (!spImageView->Init(spTextureBufferView, pContext))
		return NULL;

	return spImageView;
}

SShaderTextureBufferViewPtr SBuffer::GetTextureBuffer(SShaderTextureBufferViewConfiguration& kConfiguration, CContext* pContext)
{
	SShaderTextureBufferView* pExistingTextureBuffer(m_pTextureBuffersHead);
	while (pExistingTextureBuffer != NULL)
	{
		if (pExistingTextureBuffer->m_kConfiguration == kConfiguration)
			return pExistingTextureBuffer;
		pExistingTextureBuffer = pExistingTextureBuffer->m_pNextView;
	}

	SShaderTextureBufferViewPtr spNewView(new SShaderTextureBufferView(kConfiguration));
	if (!spNewView->Init(this, pContext))
		return NULL;

	return spNewView;
}

SShaderBufferViewPtr SBuffer::CreateBufferView(uint32 uFirstElement, uint32 uNumElements, CContext* pContext)
{
	SBufferRange kRange(uFirstElement * m_uElementSize, uNumElements * m_uElementSize);
	SShaderBufferView* pBufferView;
	if (kRange.m_uOffset == 0 && kRange.m_uSize == m_uSize)
		pBufferView = new SShaderBufferView(SBufferRange());
	else
		pBufferView = new SShaderBufferView(kRange);
	pBufferView->Init(this, pContext);
	return pBufferView;
}

SQuery::~SQuery()
{
}

void SQuery::Begin()
{
}

void SQuery::End()
{
}

uint32 SQuery::GetDataSize()
{
	return 0;
}

SPlainQuery::SPlainQuery(GLenum eTarget)
	: m_uName(0)
	, m_eTarget(eTarget)
	, m_bBegun(false)
{
}

SPlainQuery::~SPlainQuery()
{
	glDeleteQueries(1, &m_uName);
}

void SPlainQuery::Begin()
{
	glBeginQuery(m_eTarget, m_uName);
	m_bBegun = true;
}

void SPlainQuery::End()
{
	glEndQuery(m_eTarget);
}

SOcclusionQuery::SOcclusionQuery()
#if DXGLES
// GLES does not have GL_SAMPLES_PASSED
	: SPlainQuery(GL_ANY_SAMPLES_PASSED)
#else
	: SPlainQuery(GL_SAMPLES_PASSED)
#endif
{
}

bool SOcclusionQuery::GetData(void* pData, uint32 uDataSize, bool bFlush)
{
	if (m_bBegun) // Query objects are only defined after the first glBeginQuery call
	{
		GLuint uResultAvailable(GL_FALSE);
		glGetQueryObjectuiv(m_uName, GL_QUERY_RESULT_AVAILABLE, &uResultAvailable);
		// Prevent garbage reads https://plus.google.com/+RomainGuy/posts/DgemC8aa7qH
		if ((uResultAvailable & GL_TRUE) == GL_TRUE)
		{
#if DXGLES
			GLuint uResult;
			glGetQueryObjectuiv(m_uName, GL_QUERY_RESULT, &uResult);
#else
			GLuint64 uResult;
			glGetQueryObjectui64v(m_uName, GL_QUERY_RESULT, &uResult);
#endif
			*reinterpret_cast<UINT64*>(pData) = uResult;
			return true;
		}
	}
	return false;
}

uint32 SOcclusionQuery::GetDataSize()
{
	return (uint32)sizeof(UINT64);
}

SFenceSync::SFenceSync()
	: m_pFence(NULL)
{
}

SFenceSync::~SFenceSync()
{
	if (m_pFence)
		glDeleteSync(m_pFence);
}

void SFenceSync::End()
{
	if (m_pFence)
		glDeleteSync(m_pFence);
	m_pFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

bool SFenceSync::GetData(void* pData, uint32 uDataSize, bool bFlush)
{
	assert(m_pFence != NULL);
	GLenum eResult(glClientWaitSync(m_pFence, bFlush ? GL_SYNC_FLUSH_COMMANDS_BIT : 0, 0));
	if (eResult != GL_TIMEOUT_EXPIRED)
	{
		*reinterpret_cast<BOOL*>(pData) = eResult == GL_WAIT_FAILED ? FALSE : TRUE;
		return true;
	}
	return false;
}

uint32 SFenceSync::GetDataSize()
{
	return (uint32)sizeof(BOOL);
}

#if DXGL_SUPPORT_TIMER_QUERIES

bool                                   STimestampDisjointQuery::GetData(void* pData, uint32 uDataSize, bool bFlush)
{
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT* pTimestampDisjoint(alias_cast<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT*>(pData));
	pTimestampDisjoint->Frequency = 1000000000; // Timestamps are expressed in nanoseconds
	DXGL_TODO("Check if there's a way to detect the reliability of timestamps/time elapsed");
	pTimestampDisjoint->Disjoint = FALSE;
	return true;
}

uint32 STimestampDisjointQuery::GetDataSize()
{
	return (uint32)sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT);
}

STimestampQuery::STimestampQuery()
{
	glGenQueries(1, &m_uName);
}

STimestampQuery::~STimestampQuery()
{
	glDeleteQueries(1, &m_uName);
}

void STimestampQuery::End()
{
	glQueryCounter(m_uName, GL_TIMESTAMP);
}

bool STimestampQuery::GetData(void* pData, uint32 uDataSize, bool bFlush)
{
	GLint iAvailable;
	glGetQueryObjectiv(m_uName, GL_QUERY_RESULT_AVAILABLE, &iAvailable);
	if (iAvailable == 0 && bFlush)
	{
		glFlush();
		glGetQueryObjectiv(m_uName, GL_QUERY_RESULT_AVAILABLE, &iAvailable);
	}
	if (iAvailable != 0)
	{
		GLuint64 uResult;
		glGetQueryObjectui64v(m_uName, GL_QUERY_RESULT, &uResult);
		UINT64* puTimestampData(alias_cast<UINT64*>(pData));
		*puTimestampData = uResult;
		return true;
	}
	return false;
}

uint32 STimestampQuery::GetDataSize()
{
	return (uint32)sizeof(UINT64);
}

#endif //DXGL_SUPPORT_TIMER_QUERIES

SDefaultFrameBufferTexture::SDefaultFrameBufferTexture(GLsizei iWidth, GLsizei iHeight, EGIFormat eFormat, GLbitfield uBufferMask)
	: STexture(iWidth, iHeight, 1, GL_TEXTURE_2D, eFormat, 1, 1)
	, m_uBufferMask(uBufferMask)
	, m_eInputColorBuffer(GL_INVALID_ENUM)
	, m_eOutputColorBuffer(GL_INVALID_ENUM)
	, m_uTextureRefCount(0)
	, m_iDefaultFBOWidth(iWidth)
	, m_iDefaultFBOHeight(iHeight)
	, m_bInputDirty(false)
	, m_bOutputDirty(false)
#if DXGL_FULL_EMULATION
	, m_kCustomWindowContext(NULL)
#endif //DXGL_FULL_EMULATION
{
	m_pfUpdateSubresource = &UpdateSubresource;
	m_pfMapSubresource = &MapSubresource;
	m_pfUnmapSubresource = &UnmapSubresource;
#if DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE
	m_pfUnpackData = &UnpackData;
	m_pfPackData = &PackData;
	m_pfLocatePackedDataFunc = &LocatePackedData;
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE

#if CRY_PLATFORM_IOS
	// On iOS there is no system frame buffer, we need to extract the framebuffer
	// from the currently bound framebuffer.
	#if defined(USE_SDL2_VIDEO) && defined(DXGL_SINGLEWINDOW)
	m_kDefaultFBO.m_uName = 1;
	SDL_GetWindowSize(SMainWindow::ms_kInstance.m_pSDLWindow, &m_iDefaultFBOWidth, &m_iDefaultFBOHeight);
	#else
		#error "Implement a method to extract iOS's default framebuffer size";
	#endif
#endif
}

SDefaultFrameBufferTexture::~SDefaultFrameBufferTexture()
{
	if (m_uTextureRefCount > 0)
	{
		ReleaseTexture();
	}
}

SShaderTextureViewPtr SDefaultFrameBufferTexture::CreateShaderResourceView(const SShaderTextureViewConfiguration& kConfiguration, CContext* pContext)
{
	SShaderTextureViewPtr spView(new SDefaultFrameBufferShaderTextureView(kConfiguration));
	if (!spView->Init(this, pContext))
		return NULL;
	return spView;
}

SShaderImageViewPtr SDefaultFrameBufferTexture::CreateUnorderedAccessView(const SShaderImageViewConfiguration& kConfiguration, CContext* pContext)
{
	SShaderImageViewPtr spView(new SShaderImageView(kConfiguration));
	if (!spView->Init(this, pContext))
		return NULL;
	return spView;
}

SOutputMergerTextureViewPtr SDefaultFrameBufferTexture::CreateOutputMergerView(const SOutputMergerTextureViewConfiguration& kConfiguration, CContext* pContext)
{
	SOutputMergerTextureViewPtr spView(new SDefaultFrameBufferOutputMergerView(this, kConfiguration));
	if (!spView->Init(pContext))
		return NULL;
	return spView;
}

void SDefaultFrameBufferTexture::OnCopyRead(CContext* pContext)
{
	EnsureTextureExists(pContext);
	OnRead(pContext);
}

void SDefaultFrameBufferTexture::OnCopyWrite(CContext* pContext)
{
	EnsureTextureExists(pContext);
	OnWrite(pContext, false);
}

void SDefaultFrameBufferTexture::IncTextureRefCount(CContext* pContext)
{
	++m_uTextureRefCount;
	if (m_uTextureRefCount == 1)
	{
		const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(m_eFormat));
		if (pFormatInfo == NULL || pFormatInfo->m_pTexture == NULL)
		{
			DXGL_ERROR("Invalid format for frame buffer texture (could not retrieve texture format info)");
			return;
		}

		GLuint uName;
		glGenTextures(1, &uName);
		m_kName = pContext->GetDevice()->GetTextureNamePool().Create(uName);
#if DXGL_SUPPORT_COPY_IMAGE
		m_kCopyImageView = m_kName;
		m_eCopyImageTarget = m_eTarget;
#endif //DXGL_SUPPORT_COPY_IMAGE
		m_kInputFBO.m_bUsesSRGB = pFormatInfo->m_pTexture->m_bSRGB;
		m_kDefaultFBO.m_bUsesSRGB = pFormatInfo->m_pTexture->m_bSRGB;

#if DXGL_USE_IMMUTABLE_TEXTURES
		glTextureStorage2DEXT(uName, m_eTarget, m_uNumMipLevels, pFormatInfo->m_pTexture->m_iInternalFormat, m_iWidth, m_iHeight);
#else
		glTextureImage2DEXT(uName, m_eTarget, 0, pFormatInfo->m_pTexture->m_iInternalFormat, m_iWidth, m_iHeight, 0, pFormatInfo->m_pTexture->m_eBaseFormat, pFormatInfo->m_pTexture->m_eDataType, NULL);
#endif
		GLuint uInputFBOName;
		glGenFramebuffers(1, &uInputFBOName);
		m_kInputFBO.m_kName = pContext->GetDevice()->GetFrameBufferNamePool().Create(uInputFBOName);

		if ((m_uBufferMask & GL_COLOR_BUFFER_BIT) != 0)
		{
			glNamedFramebufferTextureEXT(uInputFBOName, GL_COLOR_ATTACHMENT0, uName, 0);
			m_eInputColorBuffer = GL_COLOR_ATTACHMENT0;
#if !DXGLES
			m_eOutputColorBuffer = GL_BACK_LEFT;  // This has to be one of GL_BACK_LEFT, GL_BACK_RIGHT, GL_FRONT_LEFT, GL_FRONT_RIGHT or glDrawBuffer will fail
#else
	#if CRY_PLATFORM_IOS
			// There is no default framebuffer on iOS. There is only one created by
			// SDL or the UIKit API.
			m_eOutputColorBuffer = GL_COLOR_ATTACHMENT0;
	#else
			m_eOutputColorBuffer = GL_BACK;
	#endif
#endif
		}
		else
		{
			m_eInputColorBuffer = GL_NONE;
			m_eOutputColorBuffer = GL_NONE;
		}

		if ((m_uBufferMask & GL_DEPTH_BUFFER_BIT) != 0)
			glNamedFramebufferTextureEXT(uInputFBOName, GL_DEPTH_ATTACHMENT, uName, 0);

		if ((m_uBufferMask & GL_STENCIL_BUFFER_BIT) != 0)
			glNamedFramebufferTextureEXT(uInputFBOName, GL_STENCIL_ATTACHMENT, uName, 0);

		GLenum eStatus(glCheckNamedFramebufferStatusEXT(uInputFBOName, GL_DRAW_FRAMEBUFFER));
		if (eStatus != GL_FRAMEBUFFER_COMPLETE)
		{
			DXGL_ERROR("Default input framebuffer is not complete (status = %d)", eStatus);
			glDeleteFramebuffers(1, &uInputFBOName);
			m_kInputFBO.m_kName = CResourceName();
		}
	}
}

void SDefaultFrameBufferTexture::DecTextureRefCount()
{
	// Custom FBO
	--m_uTextureRefCount;
	if (m_uTextureRefCount == 0)
		ReleaseTexture();
}

void SDefaultFrameBufferTexture::EnsureTextureExists(CContext* pContext)
{
	if (m_uTextureRefCount == 0)
	{
		DXGL_WARNING("Extra default framebuffer texture was forced as a fallback for a currently missing implementation");
		IncTextureRefCount(pContext);
	}
}

void SDefaultFrameBufferTexture::ReleaseTexture()
{
	if (m_kInputFBO.m_kName.IsValid())
	{
		GLuint uInputFBOName(m_kInputFBO.m_kName.GetName());
		glDeleteFramebuffers(1, &uInputFBOName);
		m_kInputFBO.m_kName = CResourceName();
	}
	if (m_kName.IsValid())
	{
		GLuint uTextureName(m_kName.GetName());
		glDeleteTextures(1, &uTextureName);
		m_kName = CResourceName();
	}
#if DXGL_SUPPORT_COPY_IMAGE
	m_kCopyImageView = m_kName;
#endif //DXGL_SUPPORT_COPY_IMAGE
}

void SDefaultFrameBufferTexture::OnWrite(CContext* pContext, bool bDefaultFrameBuffer)
{
#if DXGL_FULL_EMULATION
	if (m_kCustomWindowContext != NULL)
		pContext->SetWindowContext(m_kCustomWindowContext);
#endif //DXGL_FULL_EMULATION
	if (bDefaultFrameBuffer)
		m_bInputDirty = true;  // Default frame buffer - write to output directly
	else
		m_bOutputDirty = true; // Custom FBO - write to input
}

void SDefaultFrameBufferTexture::OnRead(CContext* pContext)
{
	if (m_bInputDirty)
	{
		// Copy default buffer (output) to texture (input)
		pContext->BlitFrameBuffer(
		  m_kDefaultFBO, m_kInputFBO,
		  m_eOutputColorBuffer, m_eInputColorBuffer,
#if OGL_FLIP_Y
		  0, m_iDefaultFBOHeight, m_iDefaultFBOWidth, 0,
#else
		  0, 0, m_iDefaultFBOWidth, m_iDefaultFBOHeight,
#endif
		  0, 0, m_iWidth, m_iHeight,
		  m_uBufferMask, (m_iDefaultFBOWidth == m_iWidth) ? GL_NEAREST : GL_LINEAR);
	}
}

void SDefaultFrameBufferTexture::Flush(CContext* pContext)
{
	if (m_bOutputDirty)
	{
		// Copy texture (input) to default buffer (output)
		pContext->BlitFrameBuffer(
		  m_kInputFBO, m_kDefaultFBO,
		  m_eInputColorBuffer, m_eOutputColorBuffer,
#if OGL_FLIP_Y
		  0, m_iHeight, m_iWidth, 0,
#else
		  0, 0, m_iWidth, m_iHeight,
#endif
		  0, 0, m_iDefaultFBOWidth, m_iDefaultFBOHeight,
		  m_uBufferMask, (m_iDefaultFBOWidth == m_iWidth) ? GL_NEAREST : GL_LINEAR);
	}
}

#if DXGL_FULL_EMULATION

void SDefaultFrameBufferTexture::SetCustomWindowContext(const TWindowContext& kCustomWindowContext)
{
	m_kCustomWindowContext = kCustomWindowContext;
}

#endif //DXGL_FULL_EMULATION

typedef SSingleTexImpl<STex2DUncompressed, STexPartition> TDefaultFrameBufferTextureImpl;

void SDefaultFrameBufferTexture::UpdateSubresource(SResource* pResource, uint32 uSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, uint32 uSrcRowPitch, uint32 uSrcDepthPitch, CContext* pContext)
{
	SDefaultFrameBufferTexture* pDefaultFrameBufferTexture = static_cast<SDefaultFrameBufferTexture*>(pResource);

	DXGL_TODO(
	  "Currently only the texture FBO case is handled so we have to make sure that the texture "
	  "exists during this function call. The default framebuffer case should be supported too.");
	pDefaultFrameBufferTexture->EnsureTextureExists(pContext);

	if (uSubresource > 0)
	{
		DXGL_ERROR("The only valid subresource index for the default frame buffer is 0 - cannot update subresource");
		return;
	}

	UpdateTexSubresource<TDefaultFrameBufferTextureImpl>(pDefaultFrameBufferTexture, uSubresource, pDstBox, pSrcData, uSrcRowPitch, uSrcDepthPitch, pContext);
	pDefaultFrameBufferTexture->OnWrite(pContext, false);
}

bool SDefaultFrameBufferTexture::MapSubresource(SResource* pResource, uint32 uSubresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource, CContext* pContext)
{
	SDefaultFrameBufferTexture* pDefaultFrameBufferTexture = static_cast<SDefaultFrameBufferTexture*>(pResource);

	DXGL_TODO(
	  "Currently only the texture FBO case is handled so we have to make sure that the texture "
	  "exists during this function call. The default framebuffer case should be supported too.");
	pDefaultFrameBufferTexture->EnsureTextureExists(pContext);

	if (uSubresource > 0)
	{
		DXGL_ERROR("The only valid subresource index for the default frame buffer is 0 - cannot map subresource");
		return false;
	}

	switch (MapType)
	{
	case D3D11_MAP_READ:
	case D3D11_MAP_READ_WRITE:
		pDefaultFrameBufferTexture->OnRead(pContext);
		break;
	case D3D11_MAP_WRITE:
		break;
	default:
		DXGL_ERROR("Unsupported map operation type for default frame buffer");
		return false;
	}

	return MapTexSubresource<TDefaultFrameBufferTextureImpl>(pDefaultFrameBufferTexture, uSubresource, MapType, MapFlags, pMappedResource, pContext);
}

void SDefaultFrameBufferTexture::UnmapSubresource(SResource* pResource, uint32 uSubresource, CContext* pContext)
{
	SDefaultFrameBufferTexture* pDefaultFrameBufferTexture = static_cast<SDefaultFrameBufferTexture*>(pResource);

	DXGL_TODO(
	  "Currently only the texture FBO case is handled so we have to make sure that the texture "
	  "exists during this function call. The default framebuffer case should be supported too.");
	pDefaultFrameBufferTexture->EnsureTextureExists(pContext);

	if (uSubresource > 0)
	{
		DXGL_ERROR("The only valid subresource index for the default frame buffer is 0 - cannot unmap subresource");
		return;
	}

	if (pDefaultFrameBufferTexture->m_kMappedSubTextures.size() > 0 && pDefaultFrameBufferTexture->m_kMappedSubTextures.at(0).m_bUpload)
		pDefaultFrameBufferTexture->OnWrite(pContext, false);

	UnmapTexSubresource<TDefaultFrameBufferTextureImpl>(pDefaultFrameBufferTexture, uSubresource, pContext);
}

#if DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE

void SDefaultFrameBufferTexture::UnpackData(STexture* pTexture, STexSubresourceID kSubID, STexPos kOffset, STexSize kSize, const SMappedSubTexture& kDataLocation, CContext* pContext)
{
	SDefaultFrameBufferTexture* pDefaultFrameBufferTexture = static_cast<SDefaultFrameBufferTexture*>(pTexture);

	DXGL_TODO(
	  "Currently only the texture FBO case is handled so we have to make sure that the texture "
	  "exists during this function call. The default framebuffer case should be supported too.");
	pDefaultFrameBufferTexture->EnsureTextureExists(pContext);

	UnpackTexData<TDefaultFrameBufferTextureImpl>(pDefaultFrameBufferTexture, kSubID, kOffset, kSize, kDataLocation, pContext);

	pDefaultFrameBufferTexture->OnWrite(pContext, false);
}

void SDefaultFrameBufferTexture::PackData(STexture* pTexture, STexSubresourceID kSubID, STexPos kOffset, STexSize kSize, const SMappedSubTexture& kDataLocation, CContext* pContext)
{
	SDefaultFrameBufferTexture* pDefaultFrameBufferTexture = static_cast<SDefaultFrameBufferTexture*>(pTexture);

	DXGL_TODO(
	  "Currently only the texture FBO case is handled so we have to make sure that the texture "
	  "exists during this function call. The default framebuffer case should be supported too.");
	pDefaultFrameBufferTexture->EnsureTextureExists(pContext);

	pDefaultFrameBufferTexture->OnRead(pContext);

	PackTexData<TDefaultFrameBufferTextureImpl>(pDefaultFrameBufferTexture, kSubID, kOffset, kSize, kDataLocation, pContext);
}

uint32 SDefaultFrameBufferTexture::LocatePackedData(STexture* pTexture, STexSubresourceID kSubID, STexPos kOffset, SMappedSubTexture& kDataLocation)
{
	DXGL_TODO(
	  "Currently only the texture FBO case is handled so we have to make sure that the texture "
	  "exists during this function call. The default framebuffer case should be supported too.");

	return LocateTexPackedData<TDefaultFrameBufferTextureImpl>(pTexture, kSubID, kOffset, kDataLocation);
}

#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES || !DXGL_SUPPORT_COPY_IMAGE

const SGIFormatInfo*   GetCompatibleTextureFormatInfo(EGIFormat* peGIFormat)
{
	const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(*peGIFormat));
	if (pFormatInfo->m_pTexture != NULL)
		return pFormatInfo;
	if (pFormatInfo->m_eTypelessFormat != eGIF_NUM && pFormatInfo->m_eTypelessFormat != *peGIFormat)
	{
		*peGIFormat = pFormatInfo->m_eTypelessFormat;
		pFormatInfo = GetGIFormatInfo(pFormatInfo->m_eTypelessFormat);
		if (pFormatInfo->m_pTexture != NULL)
			return pFormatInfo;
	}
	*peGIFormat = eGIF_NUM;
	return NULL;
}

#if OGL_SINGLE_CONTEXT

SInitialDataCopy::SInitialDataCopy(const D3D11_SUBRESOURCE_DATA* akSubresourceData, uint32 uNumSubresources)
	: m_uNumSubresources(uNumSubresources)
{
	if (m_uNumSubresources == 0)
		m_akSubresourceData = NULL;
	else
	{
		uint32 uDataSize = uNumSubresources * sizeof(*m_akSubresourceData);
		m_akSubresourceData = static_cast<D3D11_SUBRESOURCE_DATA*>(Malloc(uDataSize));
		Memcpy(m_akSubresourceData, akSubresourceData, uDataSize);
	}
}

SInitialDataCopy::~SInitialDataCopy()
{
	for (uint32 uSubresource = 0; uSubresource < m_uNumSubresources; ++uSubresource)
		Free(const_cast<void*>(m_akSubresourceData[uSubresource].pSysMem));
	Free(m_akSubresourceData);
}

SInitialDataCopyPtr CreateTexture1DInitialDataCopy(const D3D11_TEXTURE1D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData)
{
	DXGL_SCOPED_PROFILE("CreateTexture1DInitialDataCopy")

	EGIFormat eGIFormat(GetGIFormat(kDesc.Format));
	const SGIFormatInfo* pFormatInfo(NULL);
	if (eGIFormat == eGIF_NUM ||
	    (pFormatInfo = GetCompatibleTextureFormatInfo(&eGIFormat)) == NULL)
	{
		DXGL_ERROR("Invalid format for 1D texture");
		return NULL;
	}

	uint32 uNumSubresources = kDesc.MipLevels * kDesc.ArraySize;
	SInitialDataCopyPtr spInitialDataCopy = new SInitialDataCopy(pInitialData, uNumSubresources);

	for (uint32 uSubresource = 0; uSubresource < uNumSubresources; ++uSubresource)
	{
		uint32 uMipLevel = uSubresource % kDesc.MipLevels;
		uint32 uWidth = kDesc.Width >> uMipLevel;

		uint32 uBlockWidth = pFormatInfo->m_pTexture->m_uBlockWidth;
		uint32 uBlockSize = pFormatInfo->m_pTexture->m_uNumBlockBytes;

		uint32 uNumBlocks = (uWidth + uBlockWidth - 1) / uBlockWidth;

		uint32 uRowSize = uBlockSize * uNumBlocks;

		void* pDataCopy = Malloc(uRowSize);
		Memcpy(pDataCopy, pInitialData[uSubresource].pSysMem, uRowSize);
		spInitialDataCopy->m_akSubresourceData[uSubresource].pSysMem = pDataCopy;
	}

	return spInitialDataCopy;
}

SInitialDataCopyPtr CreateTexture2DInitialDataCopy(const D3D11_TEXTURE2D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData)
{
	DXGL_SCOPED_PROFILE("CreateTexture2DInitialDataCopy")

	EGIFormat eGIFormat(GetGIFormat(kDesc.Format));
	const SGIFormatInfo* pFormatInfo(NULL);
	if (eGIFormat == eGIF_NUM ||
	    (pFormatInfo = GetCompatibleTextureFormatInfo(&eGIFormat)) == NULL)
	{
		DXGL_ERROR("Invalid format for 2D texture");
		return NULL;
	}

	uint32 uNumSubresources = kDesc.MipLevels * kDesc.ArraySize;
	SInitialDataCopyPtr spInitialDataCopy = new SInitialDataCopy(pInitialData, uNumSubresources);

	for (uint32 uSubresource = 0; uSubresource < uNumSubresources; ++uSubresource)
	{
		uint32 uMipLevel = uSubresource % kDesc.MipLevels;
		uint32 uWidth = kDesc.Width >> uMipLevel;
		uint32 uHeight = kDesc.Height >> uMipLevel;

		uint32 uBlockWidth = pFormatInfo->m_pTexture->m_uBlockWidth;
		uint32 uBlockHeight = pFormatInfo->m_pTexture->m_uBlockHeight;
		uint32 uBlockSize = pFormatInfo->m_pTexture->m_uNumBlockBytes;

		uint32 uNumBlocks = (uWidth + uBlockWidth - 1) / uBlockWidth;
		uint32 uNumRows = (uHeight + uBlockHeight - 1) / uBlockHeight;

		uint32 uRowPitch = pInitialData[uSubresource].SysMemPitch;

		uint32 uRowSize = uBlockSize * uNumBlocks;
		uint32 uAllocSize = uRowPitch * uNumRows;
		uint32 uUsedSize = uAllocSize - uRowPitch + uRowSize;

		void* pDataCopy = Malloc(uAllocSize);
		Memcpy(pDataCopy, pInitialData[uSubresource].pSysMem, uUsedSize);
		spInitialDataCopy->m_akSubresourceData[uSubresource].pSysMem = pDataCopy;
	}

	return spInitialDataCopy;
}

SInitialDataCopyPtr CreateTexture3DInitialDataCopy(const D3D11_TEXTURE3D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData)
{
	DXGL_SCOPED_PROFILE("CreateTexture3DInitialDataCopy")

	EGIFormat eGIFormat(GetGIFormat(kDesc.Format));
	const SGIFormatInfo* pFormatInfo(NULL);
	if (eGIFormat == eGIF_NUM ||
	    (pFormatInfo = GetCompatibleTextureFormatInfo(&eGIFormat)) == NULL)
	{
		DXGL_ERROR("Invalid format for 3D texture");
		return NULL;
	}

	uint32 uNumSubresources = kDesc.MipLevels;
	SInitialDataCopyPtr spInitialDataCopy = new SInitialDataCopy(pInitialData, uNumSubresources);

	for (uint32 uSubresource = 0; uSubresource < uNumSubresources; ++uSubresource)
	{
		uint32 uMipLevel = uSubresource;
		uint32 uWidth = kDesc.Width >> uMipLevel;
		uint32 uHeight = kDesc.Height >> uMipLevel;
		uint32 uDepth = kDesc.Depth >> uMipLevel;

		uint32 uBlockWidth = pFormatInfo->m_pTexture->m_uBlockWidth;
		uint32 uBlockHeight = pFormatInfo->m_pTexture->m_uBlockHeight;
		uint32 uBlockDepth = pFormatInfo->m_pTexture->m_uBlockDepth;
		uint32 uBlockSize = pFormatInfo->m_pTexture->m_uNumBlockBytes;

		uint32 uNumBlocks = (uWidth + uBlockWidth - 1) / uBlockWidth;
		uint32 uNumRows = (uHeight + uBlockHeight - 1) / uBlockHeight;
		uint32 uNumSlices = (uDepth + uBlockDepth - 1) / uBlockDepth;

		uint32 uRowPitch = pInitialData[uSubresource].SysMemPitch;
		uint32 uSlicePitch = pInitialData[uSubresource].SysMemSlicePitch;

		uint32 uRowSize = uBlockSize * uNumBlocks;
		uint32 uSliceSize = uRowPitch * uNumRows;
		uint32 uAllocSize = uSlicePitch * uNumSlices;
		uint32 uUsedSize = uAllocSize - uSlicePitch + uSliceSize;

		void* pDataCopy = Malloc(uAllocSize);
		Memcpy(pDataCopy, pInitialData[uSubresource].pSysMem, uUsedSize);
		spInitialDataCopy->m_akSubresourceData[uSubresource].pSysMem = pDataCopy;
	}

	return spInitialDataCopy;
}

SInitialDataCopyPtr CreateBufferInitialDataCopy(const D3D11_BUFFER_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData)
{
	DXGL_SCOPED_PROFILE("CreateBufferInitialDataCopy")

	SInitialDataCopyPtr spInitialDataCopy = new SInitialDataCopy(pInitialData, 1);

	void* pDataCopy = Malloc(kDesc.ByteWidth);
	Memcpy(pDataCopy, pInitialData->pSysMem, kDesc.ByteWidth);
	spInitialDataCopy->m_akSubresourceData[0].pSysMem = pDataCopy;

	return spInitialDataCopy;
}

#endif

STexturePtr CreateTexture1D(const D3D11_TEXTURE1D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CreateTexture1D")

	EGIFormat eGIFormat(GetGIFormat(kDesc.Format));
	const SGIFormatInfo* pFormatInfo(NULL);
	if (eGIFormat == eGIF_NUM ||
	    (pFormatInfo = GetCompatibleTextureFormatInfo(&eGIFormat)) == NULL)
	{
		DXGL_ERROR("Invalid format for 1D texture");
		return NULL;
	}

	uint32 uNumElements(kDesc.ArraySize);
	bool bArray(kDesc.ArraySize > 1 || (kDesc.MiscFlags & D3D11_RESOURCE_MISC_DXGL_FORCE_ARRAY));

	STexturePtr spTexture(
	  new STexture(
	    kDesc.Width, 1, 1,
	    bArray ? GL_TEXTURE_1D : GL_TEXTURE_1D_ARRAY,
	    eGIFormat, GetNumMipLevels(kDesc), kDesc.ArraySize));

	if (kDesc.Usage == D3D11_USAGE_STAGING)
	{
		if (pFormatInfo->m_pTexture->m_bCompressed)
			InitializeTexture<SStagingTexImpl<STex1DCompressed>>(spTexture, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);
		else
			InitializeTexture<SStagingTexImpl<STex1DUncompressed>>(spTexture, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);
	}
	else
	{
		if (pFormatInfo->m_pTexture->m_bCompressed)
		{
			if (bArray)
				InitializeTexture<SArrayTexImpl<STex2DCompressed>>(spTexture, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);
			else
				InitializeTexture<SSingleTexImpl<STex1DCompressed>>(spTexture, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);
		}
		else
		{
			if (bArray)
				InitializeTexture<SArrayTexImpl<STex2DUncompressed>>(spTexture, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);
			else
				InitializeTexture<SSingleTexImpl<STex1DUncompressed>>(spTexture, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);
		}
	}

	return spTexture;
}

template<typename Partition>
void InitializeTexture2D(STexture* pTexture, bool bArray, bool bStaging, const D3D11_SUBRESOURCE_DATA* pInitialData, uint32 uCPUAccess, CContext* pContext, const SGIFormatInfo* pFormatInfo)
{
	if (bStaging)
	{
		if (pFormatInfo->m_pTexture->m_bCompressed)
			InitializeTexture<SStagingTexImpl<STex2DCompressed>>(pTexture, pInitialData, uCPUAccess, pContext, pFormatInfo);
		else
			InitializeTexture<SStagingTexImpl<STex2DUncompressed>>(pTexture, pInitialData, uCPUAccess, pContext, pFormatInfo);
	}
	else
	{
		if (pFormatInfo->m_pTexture->m_bCompressed)
		{
			if (bArray)
				InitializeTexture<SArrayTexImpl<STex3DCompressed, Partition>>(pTexture, pInitialData, uCPUAccess, pContext, pFormatInfo);
			else
				InitializeTexture<SSingleTexImpl<STex2DCompressed, Partition>>(pTexture, pInitialData, uCPUAccess, pContext, pFormatInfo);
		}
		else
		{
			if (bArray)
				InitializeTexture<SArrayTexImpl<STex3DUncompressed, Partition>>(pTexture, pInitialData, uCPUAccess, pContext, pFormatInfo);
			else
				InitializeTexture<SSingleTexImpl<STex2DUncompressed, Partition>>(pTexture, pInitialData, uCPUAccess, pContext, pFormatInfo);
		}
	}
}

STexturePtr CreateTexture2D(const D3D11_TEXTURE2D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CreateTexture2D")

	EGIFormat eGIFormat(GetGIFormat(kDesc.Format));
	const SGIFormatInfo* pFormatInfo(NULL);
	if (eGIFormat == eGIF_NUM ||
	    (pFormatInfo = GetCompatibleTextureFormatInfo(&eGIFormat)) == NULL)
	{
		DXGL_ERROR("Invalid format for 2D texture");
		return NULL;
	}

	bool bStaging(kDesc.Usage == D3D11_USAGE_STAGING);

	if (kDesc.SampleDesc.Count > 1)
	{
		DXGL_NOT_IMPLEMENTED
		return NULL;
	}
	else
	{
		if ((kDesc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0)
		{
			bool bArray(kDesc.ArraySize > 6 || (kDesc.MiscFlags & D3D11_RESOURCE_MISC_DXGL_FORCE_ARRAY));

#if DXGL_FULL_EMULATION
			if (!SupportTextureViews())
			{
				// Hack to specify array of textures with a single element without using texture views
				if (kDesc.ArraySize == 0)
					bArray = true;
			}
#endif  //DXGL_FULL_EMULATION

#if !DXGL_SUPPORT_CUBEMAP_ARRAYS
			if (bArray) { DXGL_NOT_IMPLEMENTED; return NULL; }
			STexturePtr spTexture(
				new STexture(
					kDesc.Width, kDesc.Height, 1,
					GL_TEXTURE_CUBE_MAP,
					eGIFormat, GetNumMipLevels(kDesc), kDesc.ArraySize));
#elif DXGLES //HACK add cubemap array support with EXT_texture_cube_map_array & windows

	#ifdef CRY_PLATFORM_WINDOWS
			STexturePtr spTexture = STexturePtr(
				new STexture(
					kDesc.Width, kDesc.Height, 1,
					bArray ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP,
					eGIFormat, GetNumMipLevels(kDesc), kDesc.ArraySize));
	#else
			STexturePtr spTexture;
			if (DXGL_GL_EXTENSION_SUPPORTED(EXT_texture_cube_map_array))
			{
				spTexture = STexturePtr(
					new STexture(
						kDesc.Width, kDesc.Height, 1,
						bArray ? GL_TEXTURE_CUBE_MAP_ARRAY_EXT : GL_TEXTURE_CUBE_MAP,
						eGIFormat, GetNumMipLevels(kDesc), kDesc.ArraySize));
			}
			else {
				if (bArray) { DXGL_NOT_IMPLEMENTED; return NULL; }
				spTexture = (
					new STexture(
						kDesc.Width, kDesc.Height, 1,
						GL_TEXTURE_CUBE_MAP,
						eGIFormat, GetNumMipLevels(kDesc), kDesc.ArraySize));
			}
	#endif

#else 
			STexturePtr spTexture(
				new STexture(
					kDesc.Width, kDesc.Height, 1,
					bArray ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP,
					eGIFormat, GetNumMipLevels(kDesc), kDesc.ArraySize));
#endif


			InitializeTexture2D<SCubePartition>(spTexture, bArray, bStaging, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);

			return spTexture;
		}
		else
		{
			bool bArray(kDesc.ArraySize > 1 || (kDesc.MiscFlags & D3D11_RESOURCE_MISC_DXGL_FORCE_ARRAY));

			STexturePtr spTexture(
			  new STexture(
			    kDesc.Width, kDesc.Height, 1,
			    bArray ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D,
			    eGIFormat, GetNumMipLevels(kDesc), kDesc.ArraySize));

			InitializeTexture2D<STexPartition>(spTexture, bArray, bStaging, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);

			return spTexture;
		}
	}
}

STexturePtr CreateTexture3D(const D3D11_TEXTURE3D_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CreateTexture3D")

	EGIFormat eGIFormat(GetGIFormat(kDesc.Format));
	const SGIFormatInfo* pFormatInfo(NULL);
	if (eGIFormat == eGIF_NUM ||
	    (pFormatInfo = GetCompatibleTextureFormatInfo(&eGIFormat)) == NULL)
	{
		DXGL_ERROR("Invalid format for 3D texture");
		return NULL;
	}

	STexturePtr spTexture(
	  new STexture(
	    kDesc.Width, kDesc.Height, kDesc.Depth,
	    GL_TEXTURE_3D,
	    eGIFormat, GetNumMipLevels(kDesc), 1));

	if (kDesc.Usage == D3D11_USAGE_STAGING)
	{
		if (pFormatInfo->m_pTexture->m_bCompressed)
			InitializeTexture<SStagingTexImpl<STex3DCompressed>>(spTexture, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);
		else
			InitializeTexture<SStagingTexImpl<STex3DUncompressed>>(spTexture, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);
	}
	else
	{
		if (pFormatInfo->m_pTexture->m_bCompressed)
			InitializeTexture<SSingleTexImpl<STex3DCompressed>>(spTexture, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);
		else
			InitializeTexture<SSingleTexImpl<STex3DUncompressed>>(spTexture, pInitialData, kDesc.CPUAccessFlags, pContext, pFormatInfo);
	}

	return spTexture;
}

void glNamedBufferSubDataAsync(GLuint uBuffer, GLintptr iOffset, GLsizeiptr iSize, const GLvoid* pvData)
{
#if DXGL_PREVENT_NVIDIA_SUB_BUFFER_DATA_SYNCHRONIZATION
	DXGL_TODO("TODO: Move to a per-device variable that is detected on startup. This value seems to be optimal for current NVIDIA drivers. Also experiment different values on test scenarios.");
	enum {MAX_UPLOAD_SIZE = 256 * 1024};

	while (iSize > 0)
	{
		uint32 uUploadSize((uint32)min(iSize, (GLintptr)MAX_UPLOAD_SIZE));
		glNamedBufferSubDataEXT(uBuffer, iOffset, iSize, pvData);
		iOffset += uUploadSize;
		iSize -= uUploadSize;
		pvData = static_cast<const uint8*>(pvData) + uUploadSize;
	}
#else
	glNamedBufferSubDataEXT(uBuffer, iOffset, iSize, pvData);
#endif
}

struct SDefaultBufferImpl
{
	static void UpdateBufferSubresource(SResource* pResource, uint32 uSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, uint32, uint32, CContext* pContext)
	{
		DXGL_SCOPED_PROFILE("SDefaultBufferImpl::UpdateBufferSubresource")

		SBuffer * pBuffer(static_cast<SBuffer*>(pResource));
		pBuffer->m_kCreationFence.IssueWait(pContext);

		assert(uSubresource == 0);

		if (pDstBox != NULL)
			glNamedBufferSubDataAsync(pBuffer->m_kName.GetName(), pDstBox->left, pDstBox->right - pDstBox->left, pSrcData);
		else
			glNamedBufferSubDataAsync(pBuffer->m_kName.GetName(), 0, pBuffer->m_uSize, pSrcData);
	}
};

struct SDynamicBufferImpl
{
	static bool MapBufferRange(SBuffer* pBuffer, size_t uOffset, size_t uSize, D3D11_MAP eMapType, uint32 uMapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource, CContext* pContext)
	{
		DXGL_SCOPED_PROFILE("SDynamicBufferImpl::MapBufferRange")

		pBuffer->m_kCreationFence.IssueWait(pContext);
#if DXGL_SUPPORT_APITRACE
		pMappedResource->pData = pBuffer->m_kDynamicCopy.m_pData + uOffset;
		pBuffer->m_kDynamicCopy.ResetDirtyRegion();
#else
	#if DXGL_SUPPORT_BUFFER_STORAGE
		if (uMapFlags & (D3D11_MAP_FLAG_DXGL_COHERENT | D3D11_MAP_FLAG_DXGL_PERSISTENT))
		{
			if (!SupportBufferStorage())
			{
				DXGL_ERROR("Cannot map buffer with persistent/coherent access since buffer storage is not supported");
				return false;
			}

			pBuffer->m_bMapped = false;
			GLbitfield uAccess(GL_MAP_WRITE_BIT);
			if (uMapFlags & D3D11_MAP_FLAG_DXGL_COHERENT)
				uAccess |= GL_MAP_COHERENT_BIT;
			if (uMapFlags & D3D11_MAP_FLAG_DXGL_PERSISTENT)
				uAccess |= GL_MAP_PERSISTENT_BIT;
			pMappedResource->pData = glMapNamedBufferRangeEXT(pBuffer->m_kName.GetName(), (GLintptr)uOffset, (GLsizeiptr)uSize, uAccess);
		}
		else
	#endif //DXGL_SUPPORT_BUFFER_STORAGE
		if (eMapType == D3D11_MAP_WRITE_NO_OVERWRITE)
		{
			pBuffer->m_bMapped = true;
			pMappedResource->pData = glMapNamedBufferRangeEXT(pBuffer->m_kName.GetName(), (GLintptr)uOffset, (GLsizeiptr)uSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
		}
		else
		{
			// The only other possible mode for dynamic buffers is D3D11_MAP_WRITE_DISCARD
			assert(eMapType == D3D11_MAP_WRITE_DISCARD);

			if (SGlobalConfig::iBufferUploadMode == 2 ||
			    (SGlobalConfig::iBufferUploadMode == 1 && pBuffer->m_eUsage == GL_DYNAMIC_DRAW))
			{
				pBuffer->m_bMapped = false;
				pMappedResource->pData = pBuffer->m_pSystemMemoryCopy;
				glNamedBufferDataEXT(pBuffer->m_kName.GetName(), pBuffer->m_uSize, NULL, pBuffer->m_eUsage);
			}
			else
			{
				pBuffer->m_bMapped = true;
				pMappedResource->pData = glMapNamedBufferRangeEXT(pBuffer->m_kName.GetName(), (GLintptr)uOffset, (GLsizeiptr)uSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
			}
		}
#endif

		pMappedResource->RowPitch = 0;    // Meaningless for buffers
		pMappedResource->DepthPitch = 0;  // Meaningless for buffers

		pBuffer->m_uMapOffset = uOffset;
		pBuffer->m_uMapSize = uSize;

		return true;
	}

	static bool MapBuffer(SResource* pResource, uint32 uSubresource, D3D11_MAP eMapType, uint32 uMapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource, CContext* pContext)
	{
		DXGL_SCOPED_PROFILE("SDynamicBufferImpl::MapBuffer")

		SBuffer * pBuffer(static_cast<SBuffer*>(pResource));
		return MapBufferRange(pBuffer, (size_t)0, (size_t)pBuffer->m_uSize, eMapType, uMapFlags, pMappedResource, pContext);
	}

	static void UnmapBuffer(SResource* pResource, UINT Subresource, CContext*)
	{
		DXGL_SCOPED_PROFILE("SDynamicBufferImpl::UnmapBuffer")

		SBuffer * pBuffer(static_cast<SBuffer*>(pResource));

#if DXGL_SUPPORT_APITRACE
		uint32 uOffset, uSize;
		if (!pBuffer->m_kDynamicCopy.GetDirtyRegion(uOffset, uSize))
		{
			DXGL_WARNING("GetDirtyRegion failed - performing full buffer upload");
			uOffset = 0;
			uSize = pBuffer->m_uSize;
		}

		if (uSize > 0)
			glNamedBufferSubDataAsync(pBuffer->m_kName.GetName(), uOffset, uSize, pBuffer->m_kDynamicCopy.m_pData + uOffset);
#else
		if (pBuffer->m_bMapped)
		{
			glUnmapNamedBufferEXT(pBuffer->m_kName.GetName());
			pBuffer->m_bMapped = false;
		}
		else
		{
			// This was mapped with D3D11_MAP_WRITE_DISCARD - orphan the current storage and upload the system memory buffer
			glNamedBufferSubDataAsync(pBuffer->m_kName.GetName(), pBuffer->m_uMapOffset, pBuffer->m_uMapSize, pBuffer->m_pSystemMemoryCopy);
		}
#endif
	}

	static void UpdateBufferSubresource(SResource* pResource, uint32 uSubresource, const D3D11_BOX* pDstBox, const void* pSrcData, uint32, uint32, CContext* pContext)
	{
		DXGL_SCOPED_PROFILE("SDynamicBufferImpl::UpdateBufferSubresource")

		SBuffer * pBuffer(static_cast<SBuffer*>(pResource));
		pBuffer->m_kCreationFence.IssueWait(pContext);

		assert(uSubresource == 0);

		uint32 uDstOffset, uDstSize;
		if (pDstBox != NULL)
		{
			uDstOffset = pDstBox->left;
			uDstSize = pDstBox->right - pDstBox->left;
		}
		else
		{
			uDstOffset = 0;
			uDstSize = pBuffer->m_uSize;
		}
		glNamedBufferSubDataAsync(pBuffer->m_kName.GetName(), uDstOffset, uDstSize, pSrcData);
		NCryOpenGL::Memcpy(pBuffer->m_pSystemMemoryCopy, static_cast<const uint8*>(pSrcData) + uDstOffset, uDstSize);
	}
};

void MapBufferSystemMemoryRange(SBuffer* pBuffer, uint32 uOffset, D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
	pMappedResource->pData = pBuffer->m_pSystemMemoryCopy + uOffset;
	pMappedResource->RowPitch = 0;    // Meaningless for buffers
	pMappedResource->DepthPitch = 0;  // Meaningless for buffers
}

void UpdateBufferSystemMemory(SBuffer* pBuffer, const D3D11_BOX* pDstBox, const void* pSrcData)
{
	if (pDstBox != NULL)
		NCryOpenGL::Memcpy(pBuffer->m_pSystemMemoryCopy + pDstBox->left, pSrcData, pDstBox->right - pDstBox->left);
	else
		NCryOpenGL::Memcpy(pBuffer->m_pSystemMemoryCopy, pSrcData, pBuffer->m_uSize);
}

struct SStagingBufferImpl
{
	static bool MapBufferRange(SBuffer* pBuffer, size_t uOffset, size_t uSize, D3D11_MAP, uint32, D3D11_MAPPED_SUBRESOURCE* pMappedResource, CContext*)
	{
		DXGL_SCOPED_PROFILE("SStagingBufferImpl::MapBufferRange")
		MapBufferSystemMemoryRange(pBuffer, uOffset, pMappedResource);
		return true;
	}

	static bool MapBuffer(SResource* pResource, uint32, D3D11_MAP, uint32, D3D11_MAPPED_SUBRESOURCE* pMappedResource, CContext*)
	{
		DXGL_SCOPED_PROFILE("SStagingBufferImpl::MapBuffer")
		MapBufferSystemMemoryRange(static_cast<SBuffer*>(pResource), 0, pMappedResource);
		return true;
	}

	static void UnmapBuffer(SResource*, UINT, CContext*)
	{
	}

	static void UpdateBufferSubresource(SResource* pResource, uint32, const D3D11_BOX* pDstBox, const void* pSrcData, uint32, uint32, CContext*)
	{
		DXGL_SCOPED_PROFILE("SStagingBufferImpl::UpdateBufferSubresource")
		UpdateBufferSystemMemory(static_cast<SBuffer*>(pResource), pDstBox, pSrcData);
	}
};

#if DXGL_STREAMING_CONSTANT_BUFFERS

struct SStreamingBufferImpl
{
	static bool MapBufferRange(SBuffer* pBuffer, size_t uOffset, size_t uSize, D3D11_MAP eMap, uint32, D3D11_MAPPED_SUBRESOURCE* pMappedResource, CContext*)
	{
		DXGL_SCOPED_PROFILE("SStagingBufferImpl::MapBufferRange")
		MapBufferSystemMemoryRange(pBuffer, uOffset, pMappedResource);
		if (eMap == D3D11_MAP_WRITE || eMap == D3D11_MAP_READ_WRITE || eMap == D3D11_MAP_WRITE_DISCARD || eMap == D3D11_MAP_WRITE_NO_OVERWRITE)
	#if OGL_SINGLE_CONTEXT
			pBuffer->m_bContextCacheValid = false;
	#else
			pBuffer->m_kContextCachesValid.SetZero();
	#endif
		return true;
	}

	static bool MapBuffer(SResource* pResource, uint32, D3D11_MAP eMap, uint32, D3D11_MAPPED_SUBRESOURCE* pMappedResource, CContext*)
	{
		DXGL_SCOPED_PROFILE("SStagingBufferImpl::MapBuffer")
		SBuffer * pBuffer(static_cast<SBuffer*>(pResource));
		MapBufferSystemMemoryRange(pBuffer, 0, pMappedResource);
		if (eMap == D3D11_MAP_WRITE || eMap == D3D11_MAP_READ_WRITE || eMap == D3D11_MAP_WRITE_DISCARD || eMap == D3D11_MAP_WRITE_NO_OVERWRITE)
	#if OGL_SINGLE_CONTEXT
			pBuffer->m_bContextCacheValid = false;
	#else
			pBuffer->m_kContextCachesValid.SetZero();
	#endif
		return true;
	}

	static void UnmapBuffer(SResource*, UINT, CContext*)
	{
	}

	static void UpdateBufferSubresource(SResource* pResource, uint32, const D3D11_BOX* pDstBox, const void* pSrcData, uint32, uint32, CContext*)
	{
		DXGL_SCOPED_PROFILE("SStagingBufferImpl::UpdateBufferSubresource")
		SBuffer * pBuffer(static_cast<SBuffer*>(pResource));
		UpdateBufferSystemMemory(pBuffer, pDstBox, pSrcData);
	#if OGL_SINGLE_CONTEXT
		pBuffer->m_bContextCacheValid = false;
	#else
		pBuffer->m_kContextCachesValid.SetZero();
	#endif
	}
};

#endif //DXGL_STREAMING_CONSTANT_BUFFERS

SBufferPtr CreateBuffer(const D3D11_BUFFER_DESC& kDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CreateBuffer")

	SBufferPtr spBuffer(new SBuffer());
	spBuffer->m_uSize = kDesc.ByteWidth;
	spBuffer->m_uElementSize = 0;
	spBuffer->m_kBindings.SetZero();

	for (UINT uBindMask = 1; uBindMask != 0; uBindMask <<= 1)
	{
		switch (kDesc.BindFlags & uBindMask)
		{
		case 0:
			break;
		case D3D11_BIND_VERTEX_BUFFER:
			spBuffer->m_kBindings.Set(eBB_Array, true);
			break;
		case D3D11_BIND_INDEX_BUFFER:
			spBuffer->m_kBindings.Set(eBB_ElementArray, true);
			break;
		case D3D11_BIND_CONSTANT_BUFFER:
			spBuffer->m_kBindings.Set(eBB_UniformBuffer, true);
			break;
		case D3D11_BIND_SHADER_RESOURCE:
		case D3D11_BIND_UNORDERED_ACCESS:
			if ((kDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) != 0)
			{
				spBuffer->m_uElementSize = kDesc.StructureByteStride;
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
				spBuffer->m_kBindings.Set(eBB_ShaderStorage, true);
#endif    //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
			}
			break;
		default:
			DXGL_TODO("Support more buffer bindings");
			DXGL_ERROR("Buffer binding not supported");
			return NULL;
		}
	}

	bool bVideoMemory, bAllocateSystemMemory;
	GLbitfield uStorageFlags(0);
#if DXGL_STREAMING_CONSTANT_BUFFERS
	if (SGlobalConfig::iStreamingConstantBuffersMode > 0 &&
	    (kDesc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) != 0 &&
	    (kDesc.MiscFlags & D3D11_RESOURCE_MISC_DXGL_NO_STREAMING) == 0)
	{
		spBuffer->m_pfMapSubresource = &SStreamingBufferImpl::MapBuffer;
		spBuffer->m_pfUnmapSubresource = &SStreamingBufferImpl::UnmapBuffer;
		spBuffer->m_pfUpdateSubresource = &SStreamingBufferImpl::UpdateBufferSubresource;
		spBuffer->m_pfMapBufferRange = &SStreamingBufferImpl::MapBufferRange;
		spBuffer->m_bStreaming = true;
		bVideoMemory = true;
		bAllocateSystemMemory = true;
		uStorageFlags = GL_MAP_WRITE_BIT;

		switch (kDesc.Usage)
		{
		case D3D11_USAGE_IMMUTABLE:
			spBuffer->m_eUsage = GL_STATIC_DRAW;
			break;
		case D3D11_USAGE_DEFAULT:
			spBuffer->m_eUsage = GL_STREAM_DRAW;
			break;
		case D3D11_USAGE_DYNAMIC:
			spBuffer->m_eUsage = GL_DYNAMIC_DRAW;
			break;
		default:
			DXGL_ERROR("Buffer usage not supported");
			return NULL;
		}
	}
	else
	{
#endif //DXGL_STREAMING_CONSTANT_BUFFERS
	switch (kDesc.Usage)
	{
	case D3D11_USAGE_DEFAULT:
		spBuffer->m_pfUpdateSubresource = &SDefaultBufferImpl::UpdateBufferSubresource;
	case D3D11_USAGE_IMMUTABLE:
		if ((kDesc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) == 0)
			spBuffer->m_eUsage = GL_STATIC_READ;
		else if ((kDesc.CPUAccessFlags & D3D11_CPU_ACCESS_READ) == 0)
			spBuffer->m_eUsage = GL_STATIC_DRAW;
		else
			spBuffer->m_eUsage = GL_STATIC_COPY;
		bVideoMemory = true;
		bAllocateSystemMemory = false;
		break;
	case D3D11_USAGE_DYNAMIC:
		spBuffer->m_pfMapSubresource = &SDynamicBufferImpl::MapBuffer;
		spBuffer->m_pfUnmapSubresource = &SDynamicBufferImpl::UnmapBuffer;
		spBuffer->m_pfUpdateSubresource = &SDynamicBufferImpl::UpdateBufferSubresource;
		spBuffer->m_pfMapBufferRange = &SDynamicBufferImpl::MapBufferRange;
		if ((kDesc.CPUAccessFlags & D3D11_CPU_ACCESS_READ) != 0)
		{
			DXGL_ERROR("Cannot create a buffer with dynamic usage that is CPU readable");
			return NULL;
		}
		if ((kDesc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) != 0)
		{
			// Assuming that constant buffers are accessed more frequently and usually discarded on updates
			spBuffer->m_eUsage = GL_DYNAMIC_DRAW;
		}
		else
			spBuffer->m_eUsage = GL_STREAM_DRAW;
		bVideoMemory = true;
#if DXGL_SUPPORT_BUFFER_STORAGE
		uStorageFlags = GL_MAP_WRITE_BIT;
		if (kDesc.MiscFlags & D3D11_RESOURCE_MISC_DXGL_MAP_COHERENT)
			uStorageFlags |= GL_MAP_COHERENT_BIT;
		if (kDesc.MiscFlags & D3D11_RESOURCE_MISC_DXGL_MAP_PERSISTENT)
			uStorageFlags |= GL_MAP_PERSISTENT_BIT;
#endif //DXGL_SUPPORT_BUFFER_STORAGE
#if DXGL_SUPPORT_APITRACE
		spBuffer->m_pSystemMemoryCopy = spBuffer->m_kDynamicCopy.Allocate(kDesc.ByteWidth);
		spBuffer->m_bSystemMemoryCopyOwner = false;
		bAllocateSystemMemory = false;
#else
		bAllocateSystemMemory = true;
#endif
		break;
	case D3D11_USAGE_STAGING:
		spBuffer->m_pfMapSubresource = &SStagingBufferImpl::MapBuffer;
		spBuffer->m_pfUnmapSubresource = &SStagingBufferImpl::UnmapBuffer;
		spBuffer->m_pfUpdateSubresource = &SStagingBufferImpl::UpdateBufferSubresource;
		spBuffer->m_pfMapBufferRange = &SStagingBufferImpl::MapBufferRange;
		spBuffer->m_eUsage = GL_NONE;
		bVideoMemory = false;
		bAllocateSystemMemory = true;
		break;
	default:
		DXGL_ERROR("Buffer usage not supported");
		return NULL;
	}
#if DXGL_STREAMING_CONSTANT_BUFFERS
}
#endif //DXGL_STREAMING_CONSTANT_BUFFERS

	if (bVideoMemory)
	{
		GLuint uName;
		glGenBuffers(1, &uName);
		spBuffer->m_kName = pContext->GetDevice()->GetBufferNamePool().Create(uName);

		DXGL_TODO("Handle all required conversions");

		const GLvoid* pData(pInitialData == NULL ? NULL : pInitialData->pSysMem);
#if DXGL_SUPPORT_BUFFER_STORAGE
		if ((uStorageFlags & (GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT)))
		{
			if (SupportBufferStorage())
				glNamedBufferStorageEXT(uName, kDesc.ByteWidth, pData, uStorageFlags);
			else
			{
				DXGL_ERROR("Cannot create a persistent/coherent-mappable buffer since buffer storage is not supported");
				return NULL;
			}
		}
		else
#endif //DXGL_SUPPORT_BUFFER_STORAGE
		glNamedBufferDataEXT(uName, kDesc.ByteWidth, pData, spBuffer->m_eUsage);
		spBuffer->m_kCreationFence.IssueFence(pContext->GetDevice());
	}

	if (bAllocateSystemMemory)
		spBuffer->m_pSystemMemoryCopy = static_cast<uint8*>(Memalign(kDesc.ByteWidth, MIN_MAPPED_RESOURCE_ALIGNMENT));

	if (spBuffer->m_pSystemMemoryCopy != NULL && pInitialData != NULL)
		NCryOpenGL::Memcpy(spBuffer->m_pSystemMemoryCopy, pInitialData->pSysMem, kDesc.ByteWidth);

	return spBuffer;
}

SQueryPtr CreateQuery(const D3D11_QUERY_DESC& kDesc, CContext*)
{
	DXGL_SCOPED_PROFILE("CreateQuery")

	SQueryPtr spQuery(NULL);
	switch (kDesc.Query)
	{
	case D3D11_QUERY_OCCLUSION:
		{
			SOcclusionQuery* pOcclusionQuery(new SOcclusionQuery());
			spQuery = pOcclusionQuery;
			glGenQueries(1, &pOcclusionQuery->m_uName);
		}
		break;
	case D3D11_QUERY_EVENT:
		spQuery = new SFenceSync();
		break;
#if DXGL_SUPPORT_TIMER_QUERIES
	case D3D11_QUERY_TIMESTAMP:
		spQuery = new STimestampQuery();
		break;
	case D3D11_QUERY_TIMESTAMP_DISJOINT:
		spQuery = new STimestampDisjointQuery();
		break;
#endif //DXGL_SUPPORT_TIMER_QUERIES
	default:
		//DXGL_NOT_IMPLEMENTED
		break;
	}

	return spQuery;
}

SDefaultFrameBufferTexturePtr CreateBackBufferTexture(const D3D11_TEXTURE2D_DESC& kDesc)
{
	DXGL_SCOPED_PROFILE("CreateBackBufferTexture")

	EGIFormat eGIFormat(GetGIFormat(kDesc.Format));
	if (eGIFormat == eGIF_NUM)
		return NULL;

	return new SDefaultFrameBufferTexture(kDesc.Width, kDesc.Height, eGIFormat, GL_COLOR_BUFFER_BIT);
}

typedef void (* CopyTextureBoxFunc)(STexture*, STexPos, STexSubresourceID, STexture*, STexPos, STexSubresourceID, STexSize, CContext*);

#if DXGL_SUPPORT_COPY_IMAGE

void CopyVideoTextureBoxWithCopyImage(
  STexture* pDstTexture, STexPos kDstPos, STexSubresourceID kDstSubID,
  STexture* pSrcTexture, STexPos kSrcPos, STexSubresourceID kSrcSubID,
  STexSize kBoxSize, CContext* pContext)
{
	glCopyImageSubData(
	  pSrcTexture->m_kCopyImageView.GetName(), pSrcTexture->m_eCopyImageTarget, kSrcSubID.m_iMipLevel, kSrcPos.x, kSrcPos.y, kSrcPos.z + kSrcSubID.m_uElement,
	  pDstTexture->m_kCopyImageView.GetName(), pDstTexture->m_eCopyImageTarget, kDstSubID.m_iMipLevel, kDstPos.x, kDstPos.y, kDstPos.z + kDstSubID.m_uElement,
	  kBoxSize.x, kBoxSize.y, kBoxSize.z);
}

#else

void CopyVideoTextureBoxWithBlitFrameBuffer(
  STexture* pDstTexture, STexPos kDstPos, STexSubresourceID kDstSubID,
  STexture* pSrcTexture, STexPos kSrcPos, STexSubresourceID kSrcSubID,
  STexSize kBoxSize, CContext* pContext)
{
	SOutputMergerTextureViewPtr spSrcView(GetCopyOutputMergerView(pSrcTexture, kSrcSubID, pContext, GetGIFormatInfo(pSrcTexture->m_eFormat)));
	SOutputMergerTextureViewPtr spDstView(GetCopyOutputMergerView(pDstTexture, kDstSubID, pContext, GetGIFormatInfo(pDstTexture->m_eFormat)));

	if (!pContext->BlitOutputMergerView(
	      spSrcView, spDstView,
	      kSrcPos.x, kSrcPos.y, kSrcPos.x + kBoxSize.x, kSrcPos.y + kBoxSize.y,
	      kDstPos.x, kDstPos.y, kDstPos.x + kBoxSize.x, kDstPos.y + kBoxSize.y,
	      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
	      GL_NEAREST))
	{
		DXGL_ERROR("Error copying texture box with BlitFrameBuffer");
	}
}

void CopyVideoTextureBoxWithPixelBuffer(
  STexture* pDstTexture, STexPos kDstPos, STexSubresourceID kDstSubID,
  STexture* pSrcTexture, STexPos kSrcPos, STexSubresourceID kSrcSubID,
  STexSize kBoxSize, CContext* pContext)
{
	SMappedSubTexture kDataLocation;
	uint32 uPackedSize(pSrcTexture->m_pfLocatePackedDataFunc(pSrcTexture, kSrcSubID, kSrcPos, kDataLocation));

	const CResourceName& kCopyPixelBuffer(pContext->GetCopyPixelBuffer());
	glNamedBufferDataEXT(kCopyPixelBuffer.GetName(), uPackedSize, NULL, GL_STREAM_COPY);

	pContext->BindBuffer(kCopyPixelBuffer, eBB_PixelPack);
	pSrcTexture->m_pfPackData(pSrcTexture, kSrcSubID, kSrcPos, kBoxSize, kDataLocation, pContext);
	pContext->BindBuffer(CResourceName(), eBB_PixelPack);

	pContext->BindBuffer(kCopyPixelBuffer, eBB_PixelUnpack);
	pDstTexture->m_pfUnpackData(pDstTexture, kDstSubID, kDstPos, kBoxSize, kDataLocation, pContext);
	pContext->BindBuffer(CResourceName(), eBB_PixelUnpack);
}

#endif

#if DXGL_USE_PBO_FOR_STAGING_TEXTURES

void CopyVideoToSystemTextureBox(
  STexture* pDstTexture, STexPos kDstPos, STexSubresourceID kDstSubID,
  STexture* pSrcTexture, STexPos kSrcPos, STexSubresourceID kSrcSubID,
  STexSize kBoxSize, CContext* pContext)
{
	uint32 uDstSubResource(D3D11CalcSubresource(kDstSubID.m_iMipLevel, kDstSubID.m_uElement, pDstTexture->m_uNumMipLevels));
	if (pContext->BindBuffer(pDstTexture->m_akPixelBuffers[uDstSubResource], eBB_PixelPack) == 0)
		return;

	SMappedSubTexture kDataLocation;
	pDstTexture->m_pfLocatePackedDataFunc(pDstTexture, kDstSubID, kDstPos, kDataLocation);
	pSrcTexture->m_pfPackData(pSrcTexture, kSrcSubID, kSrcPos, kBoxSize, kDataLocation, pContext);
	pContext->BindBuffer(CResourceName(), eBB_PixelPack);
}

void CopySystemToVideoTextureBox(
  STexture* pDstTexture, STexPos kDstPos, STexSubresourceID kDstSubID,
  STexture* pSrcTexture, STexPos kSrcPos, STexSubresourceID kSrcSubID,
  STexSize kBoxSize, CContext* pContext)
{
	uint32 uSrcSubResource(D3D11CalcSubresource(kSrcSubID.m_iMipLevel, kSrcSubID.m_uElement, pSrcTexture->m_uNumMipLevels));
	if (pContext->BindBuffer(pSrcTexture->m_akPixelBuffers[uSrcSubResource], eBB_PixelUnpack) == 0)
		return;

	SMappedSubTexture kDataLocation;
	pSrcTexture->m_pfLocatePackedDataFunc(pSrcTexture, kSrcSubID, kSrcPos, kDataLocation);
	pDstTexture->m_pfUnpackData(pDstTexture, kDstSubID, kDstPos, kBoxSize, kDataLocation, pContext);
	pContext->BindBuffer(CResourceName(), eBB_PixelUnpack);
}

#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES

void CopySystemTextureBox(
  STexture* pDstTexture, STexPos kDstPos, STexSubresourceID kDstSubID,
  STexture* pSrcTexture, STexPos kSrcPos, STexSubresourceID kSrcSubID,
  STexSize kBoxSize, CContext* pContext)
{
	UINT uDstSubresource(D3D11CalcSubresource(kDstSubID.m_iMipLevel, kDstSubID.m_uElement, pDstTexture->m_uNumMipLevels));
	UINT uSrcSubresource(D3D11CalcSubresource(kSrcSubID.m_iMipLevel, kSrcSubID.m_uElement, pSrcTexture->m_uNumMipLevels));

	D3D11_BOX kDstBox;
	kDstBox.left = kDstPos.x;
	kDstBox.top = kDstPos.y;
	kDstBox.front = kDstPos.z;
	kDstBox.right = kDstPos.x + kBoxSize.x;
	kDstBox.bottom = kDstPos.y + kBoxSize.y;
	kDstBox.back = kDstPos.z + kBoxSize.z;

	D3D11_MAPPED_SUBRESOURCE kSrcMapped;
	pSrcTexture->m_pfMapSubresource(pSrcTexture, uSrcSubresource, D3D11_MAP_READ, 0, &kSrcMapped, pContext);
	pDstTexture->m_pfUpdateSubresource(pDstTexture, uDstSubresource, &kDstBox, kSrcMapped.pData, kSrcMapped.RowPitch, kSrcMapped.DepthPitch, pContext);
	pSrcTexture->m_pfUnmapSubresource(pSrcTexture, uSrcSubresource, pContext);
}

template<CopyTextureBoxFunc pfCopyTextureBox>
void CopyTextureImpl(STexture* pDstTexture, STexture* pSrcTexture, CContext* pContext)
{
	DXGL_TODO("Check if there's a better way to do this");
	STexSubresourceID kSubID;
	for (kSubID.m_iMipLevel = 0; kSubID.m_iMipLevel < (GLint)pDstTexture->m_uNumMipLevels; ++kSubID.m_iMipLevel)
	{
		STexBox kBox;
		GetTextureBox(kBox, pDstTexture, kSubID.m_iMipLevel, GetGIFormatInfo(pDstTexture->m_eFormat), true);

		for (kSubID.m_uElement = 0; kSubID.m_uElement < pDstTexture->m_uNumElements; ++kSubID.m_uElement)
		{
			pfCopyTextureBox(
			  pDstTexture, kBox.m_kOffset, kSubID,
			  pSrcTexture, kBox.m_kOffset, kSubID,
			  kBox.m_kSize, pContext);
		}
	}
}

void CopyTexture(STexture* pDstTexture, STexture* pSrcTexture, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CopyTexture")

	if (pSrcTexture->m_uNumMipLevels != pDstTexture->m_uNumMipLevels ||
	    pSrcTexture->m_uNumElements != pDstTexture->m_uNumElements ||
	    pSrcTexture->m_iWidth != pDstTexture->m_iWidth ||
	    pSrcTexture->m_iHeight != pDstTexture->m_iHeight ||
	    pSrcTexture->m_iDepth != pDstTexture->m_iDepth)
	{
		DXGL_ERROR("Source and destination textures to copy don't match");
		return;
	}

	pDstTexture->m_kCreationFence.IssueWait(pContext);
	pSrcTexture->m_kCreationFence.IssueWait(pContext);

	// Handle the default framebuffer texture case
	DXGL_TODO("This is not the most efficient way to copy from or to the default frame buffer - we are forcing the copy texture to exist. Use glReadPixels and glBlitFrameBuffer when possible.")
	pDstTexture->OnCopyWrite(pContext);
	pSrcTexture->OnCopyRead(pContext);

	if (pDstTexture->m_kName.IsValid() && pSrcTexture->m_kName.IsValid())
	{
#if DXGL_SUPPORT_COPY_IMAGE
		CopyTextureImpl<CopyVideoTextureBoxWithCopyImage>(pDstTexture, pSrcTexture, pContext);
#else
		const SGIFormatInfo* pSrcFormatInfo(GetGIFormatInfo(pSrcTexture->m_eFormat));
		if (pSrcFormatInfo->m_pUncompressed != NULL && pSrcTexture->m_iDepth == 1)
		{
			const SGIFormatInfo* pDstFormatInfo(GetGIFormatInfo(pDstTexture->m_eFormat));
			if (pDstFormatInfo->m_pUncompressed != NULL && pDstTexture->m_iDepth == 1)
				CopyTextureImpl<CopyVideoTextureBoxWithBlitFrameBuffer>(pDstTexture, pSrcTexture, pContext);
			else
				CopyTextureImpl<CopyVideoTextureBoxWithPixelBuffer>(pDstTexture, pSrcTexture, pContext);
		}
		else
			CopyTextureImpl<CopySystemTextureBox>(pDstTexture, pSrcTexture, pContext);
#endif
	}
#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
	else if (pDstTexture->m_kName.IsValid())
		CopyTextureImpl<CopySystemToVideoTextureBox>(pDstTexture, pSrcTexture, pContext);
	else if (pSrcTexture->m_kName.IsValid())
		CopyTextureImpl<CopyVideoToSystemTextureBox>(pDstTexture, pSrcTexture, pContext);
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES
	else
		CopyTextureImpl<CopySystemTextureBox>(pDstTexture, pSrcTexture, pContext);
}

template<CopyTextureBoxFunc pfCopyTextureBox>
void CopySubTextureImpl(
  STexture* pDstTexture, uint32 uDstSubresource, uint32 uDstX, uint32 uDstY, uint32 uDstZ,
  STexture* pSrcTexture, uint32 uSrcSubresource, const D3D11_BOX* pSrcBox, CContext* pContext)
{
	STexSubresourceID kDstSubID;
	kDstSubID.m_iMipLevel = uDstSubresource % pDstTexture->m_uNumMipLevels;
	kDstSubID.m_uElement = uDstSubresource / pDstTexture->m_uNumMipLevels;

	STexSubresourceID kSrcSubID;
	kSrcSubID.m_iMipLevel = uSrcSubresource % pSrcTexture->m_uNumMipLevels;
	kSrcSubID.m_uElement = uSrcSubresource / pSrcTexture->m_uNumMipLevels;

	STexBox kSrcBox;
	GetTextureBox(kSrcBox, pSrcTexture, kSrcSubID.m_iMipLevel, pSrcBox, GetGIFormatInfo(pSrcTexture->m_eFormat), true);

	STexPos kDstPos(uDstX, uDstY, uDstZ);

	pfCopyTextureBox(
	  pDstTexture, kDstPos, kDstSubID,
	  pSrcTexture, kSrcBox.m_kOffset, kSrcSubID,
	  kSrcBox.m_kSize, pContext);
}

void CopySubTexture(
  STexture* pDstTexture, uint32 uDstSubresource, uint32 uDstX, uint32 uDstY, uint32 uDstZ,
  STexture* pSrcTexture, uint32 uSrcSubresource, const D3D11_BOX* pSrcBox, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CopySubTexture")

	pDstTexture->m_kCreationFence.IssueWait(pContext);
	pSrcTexture->m_kCreationFence.IssueWait(pContext);

	// Handle the default framebuffer texture case
	DXGL_TODO("This is not the most efficient way to copy from or to the default frame buffer - we are forcing the copy texture to exist. Use glReadPixels and glBlitFrameBuffer when possible.")
	pDstTexture->OnCopyWrite(pContext);
	pSrcTexture->OnCopyRead(pContext);

	if (pDstTexture->m_kName.IsValid() && pSrcTexture->m_kName.IsValid())
	{
#if DXGL_SUPPORT_COPY_IMAGE
		CopySubTextureImpl<CopyVideoTextureBoxWithCopyImage>(pDstTexture, uDstSubresource, uDstX, uDstY, uDstZ, pSrcTexture, uSrcSubresource, pSrcBox, pContext);
#else
		const SGIFormatInfo* pSrcFormatInfo(GetGIFormatInfo(pSrcTexture->m_eFormat));
		if (pSrcFormatInfo->m_pUncompressed != NULL && pSrcTexture->m_iDepth == 1)
		{
			const SGIFormatInfo* pDstFormatInfo(GetGIFormatInfo(pDstTexture->m_eFormat));
			if (pDstFormatInfo->m_pUncompressed != NULL && pDstTexture->m_iDepth == 1)
				CopySubTextureImpl<CopyVideoTextureBoxWithBlitFrameBuffer>(pDstTexture, uDstSubresource, uDstX, uDstY, uDstZ, pSrcTexture, uSrcSubresource, pSrcBox, pContext);
			else
				CopySubTextureImpl<CopyVideoTextureBoxWithPixelBuffer>(pDstTexture, uDstSubresource, uDstX, uDstY, uDstZ, pSrcTexture, uSrcSubresource, pSrcBox, pContext);
		}
		else
			CopySubTextureImpl<CopySystemTextureBox>(pDstTexture, uDstSubresource, uDstX, uDstY, uDstZ, pSrcTexture, uSrcSubresource, pSrcBox, pContext);
#endif
	}
#if DXGL_USE_PBO_FOR_STAGING_TEXTURES
	else if (pDstTexture->m_kName.IsValid())
		CopySubTextureImpl<CopySystemToVideoTextureBox>(pDstTexture, uDstSubresource, uDstX, uDstY, uDstZ, pSrcTexture, uSrcSubresource, pSrcBox, pContext);
	else if (pSrcTexture->m_kName.IsValid())
		CopySubTextureImpl<CopyVideoToSystemTextureBox>(pDstTexture, uDstSubresource, uDstX, uDstY, uDstZ, pSrcTexture, uSrcSubresource, pSrcBox, pContext);
#endif //DXGL_USE_PBO_FOR_STAGING_TEXTURES
	else
		CopySubTextureImpl<CopySystemTextureBox>(pDstTexture, uDstSubresource, uDstX, uDstY, uDstZ, pSrcTexture, uSrcSubresource, pSrcBox, pContext);
}

void CopySubBufferInternal(SBuffer* pDstBuffer, SBuffer* pSrcBuffer, uint32 uDstOffset, uint32 uSrcOffset, uint32 uSize)
{
	if (pSrcBuffer->m_pSystemMemoryCopy != NULL && pDstBuffer->m_pSystemMemoryCopy != NULL)
		NCryOpenGL::Memcpy(pDstBuffer->m_pSystemMemoryCopy + uDstOffset, pSrcBuffer->m_pSystemMemoryCopy + uSrcOffset, uSize);

	if (pDstBuffer->m_kName.IsValid())
	{
		if (pSrcBuffer->m_kName.IsValid())
			glNamedCopyBufferSubDataEXT(pSrcBuffer->m_kName.GetName(), pDstBuffer->m_kName.GetName(), uSrcOffset, uDstOffset, uSize);
		else
			glNamedBufferSubDataAsync(pDstBuffer->m_kName.GetName(), uDstOffset, uSize, pSrcBuffer->m_pSystemMemoryCopy + uSrcOffset);
	}
	else if (pSrcBuffer->m_kName.IsValid() && pSrcBuffer->m_pSystemMemoryCopy == NULL)
	{
#if DXGLES
		GLvoid* pvData(glMapNamedBufferRangeEXT(pSrcBuffer->m_kName.GetName(), uSrcOffset, uSize, GL_MAP_READ_BIT));
		if (pvData == NULL)
		{
			DXGL_ERROR("CopySubBufferInternal - could not map buffer for reading");
			return;
		}
		NCryOpenGL::Memcpy(pDstBuffer->m_pSystemMemoryCopy + uDstOffset, pvData, uSize);
		glUnmapBuffer(pSrcBuffer->m_kName.GetName());
#else
		glGetNamedBufferSubDataEXT(pSrcBuffer->m_kName.GetName(), uSrcOffset, uSize, pDstBuffer->m_pSystemMemoryCopy + uDstOffset);
#endif
	}
}

void CopyBuffer(SBuffer* pDstBuffer, SBuffer* pSrcBuffer, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CopyBuffer")

	if (pSrcBuffer->m_uSize != pDstBuffer->m_uSize)
	{
		DXGL_ERROR("Source and destination buffers to copy don't match");
		return;
	}

	pDstBuffer->m_kCreationFence.IssueWait(pContext);
	pSrcBuffer->m_kCreationFence.IssueWait(pContext);

	CopySubBufferInternal(pDstBuffer, pSrcBuffer, 0, 0, pSrcBuffer->m_uSize);
}

void CopySubBuffer(
  SBuffer* pDstBuffer, uint32 uDstSubresource, uint32 uDstX, uint32, uint32,
  SBuffer* pSrcBuffer, uint32 uSrcSubresource, const D3D11_BOX* pSrcBox, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CopySubBuffer")

	uint32 uSrcBegin, uSrcEnd;
	if (pSrcBox != NULL)
	{
		uSrcBegin = pSrcBox->left;
		uSrcEnd = pSrcBox->right;
	}
	else
	{
		uSrcBegin = 0;
		uSrcEnd = pSrcBuffer->m_uSize;
	}
	uint32 uSize(uSrcEnd - uSrcBegin);

	if (uSrcBegin > pSrcBuffer->m_uSize ||
	    uSrcEnd > pSrcBuffer->m_uSize ||
	    uDstX + uSize > pDstBuffer->m_uSize)
	{
		DXGL_ERROR("Source or destination range out of bounds");
		return;
	}

	pDstBuffer->m_kCreationFence.IssueWait(pContext);
	pSrcBuffer->m_kCreationFence.IssueWait(pContext);

	CopySubBufferInternal(pDstBuffer, pSrcBuffer, uDstX, uSrcBegin, uSize);
}

GLenum GetBufferBindingTarget(EBufferBinding eBinding)
{
	switch (eBinding)
	{
	case eBB_Array:
		return GL_ARRAY_BUFFER;
	case eBB_CopyRead:
		return GL_COPY_READ_BUFFER;
	case eBB_CopyWrite:
		return GL_COPY_WRITE_BUFFER;
	case eBB_ElementArray:
		return GL_ELEMENT_ARRAY_BUFFER;
	case eBB_PixelPack:
		return GL_PIXEL_PACK_BUFFER;
	case eBB_PixelUnpack:
		return GL_PIXEL_UNPACK_BUFFER;
	case eBB_TransformFeedback:
		return GL_TRANSFORM_FEEDBACK_BUFFER;
	case eBB_UniformBuffer:
		return GL_UNIFORM_BUFFER;
#if DXGL_SUPPORT_DRAW_INDIRECT
	case eBB_DrawIndirect:
		return GL_DRAW_INDIRECT_BUFFER;
#endif //DXGL_SUPPORT_DRAW_INDIRECT
#if DXGL_SUPPORT_ATOMIC_COUNTERS
	case eBB_AtomicCounter:
		return GL_ATOMIC_COUNTER_BUFFER;
#endif //DXGL_SUPPORT_ATOMIC_COUNTERS
#if DXGL_SUPPORT_DISPATCH_INDIRECT
	case eBB_DispachIndirect:
		return GL_DISPATCH_INDIRECT_BUFFER;
#endif //DXGL_SUPPORT_DISPATCH_INDIRECT
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	case eBB_ShaderStorage:
		return GL_SHADER_STORAGE_BUFFER;
#endif //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	}
	DXGL_ERROR("Invalid buffer binding");
	return 0;
}

GLenum GetBufferBindingPoint(EBufferBinding eBinding)
{
	switch (eBinding)
	{
	case eBB_Array:
		return GL_ARRAY_BUFFER_BINDING;
	case eBB_CopyRead:
		return GL_COPY_READ_BUFFER_BINDING;
	case eBB_CopyWrite:
		return GL_COPY_WRITE_BUFFER_BINDING;
	case eBB_ElementArray:
		return GL_ELEMENT_ARRAY_BUFFER_BINDING;
	case eBB_PixelPack:
		return GL_PIXEL_PACK_BUFFER_BINDING;
	case eBB_PixelUnpack:
		return GL_PIXEL_UNPACK_BUFFER_BINDING;
	case eBB_TransformFeedback:
		return GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
	case eBB_UniformBuffer:
		return GL_UNIFORM_BUFFER_BINDING;
#if DXGL_SUPPORT_DRAW_INDIRECT
	case eBB_DrawIndirect:
		return GL_DRAW_INDIRECT_BUFFER_BINDING;
#endif //DXGL_SUPPORT_DRAW_INDIRECT
#if DXGL_SUPPORT_ATOMIC_COUNTERS
	case eBB_AtomicCounter:
		return GL_ATOMIC_COUNTER_BUFFER_BINDING;
#endif //DXGL_SUPPORT_ATOMIC_COUNTERS
#if DXGL_SUPPORT_DISPATCH_INDIRECT
	case eBB_DispachIndirect:
		return GL_DISPATCH_INDIRECT_BUFFER_BINDING;
#endif //DXGL_SUPPORT_DISPATCH_INDIRECT
#if DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	case eBB_ShaderStorage:
		return GL_SHADER_STORAGE_BUFFER_BINDING;
#endif //DXGL_SUPPORT_SHADER_STORAGE_BLOCKS
	}
	DXGL_ERROR("Invalid buffer binding");
	return 0;
}

}
