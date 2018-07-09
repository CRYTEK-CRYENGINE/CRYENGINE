// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLResource.cpp
//  Version:     v1.00
//  Created:     26/03/2015 by Valerio Guagliumi.
//  Description: Implements the resource view related functions
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>

#include "GLView.hpp"
#include "GLDevice.hpp"

namespace NCryOpenGL
{

CResourceName CreateTextureView(STexture* pTexture, const SGIFormatInfo* pFormatInfo, GLenum eTarget, GLuint uMinMipLevel, GLuint uNumMipLevels, GLuint uMinLayer, GLuint uNumLayers, CContext* pContext)
{
#if DXGL_SUPPORT_TEXTURE_VIEWS
	if (SupportTextureViews())
	{
		GLint iInternalFormat;
		if (pFormatInfo->m_eTypelessConversion != eGIFC_TEXTURE_VIEW)
		{
			// This can happen if lazy texture view creation is disabled or two views with different modifiers
			// are bound at the same time (for example one view as DEPTH_TO_RED, one as STENCIL_TO_GREEN).
			// In this case create a view with same format as the original texture, so that it will have its own
			// separate format modifier.
			iInternalFormat = GetGIFormatInfo(pTexture->m_eFormat)->m_pTexture->m_iInternalFormat;
		}
		else
		{
			if (pFormatInfo->m_pTexture == NULL)
			{
				DXGL_ERROR("Invalid format for texture view");
				return CResourceName();
			}

			iInternalFormat = pFormatInfo->m_pTexture->m_iInternalFormat;
		}

		GLuint uView;
		glGenTextures(1, &uView);
		glTextureViewEXT(uView, eTarget, pTexture->m_kName.GetName(), iInternalFormat, uMinMipLevel, uNumMipLevels, uMinLayer, uNumLayers);
		return pContext->GetDevice()->GetTextureNamePool().Create(uView);
	}
	else
#endif
	{
		DXGL_ERROR("glTextureView not supported - cannot create the requested unique view for the texture");
		return CResourceName();
	}
}

CResourceName CreateTextureBuffer(SBuffer* pBuffer, const SGIFormatInfo* pFormatInfo, uint32 uFirstElement, uint32 uNumElements, CContext* pContext)
{
#if DXGL_SUPPORT_TEXTURE_BUFFERS
	uint32 uElementSize(pFormatInfo->m_pTexture->m_uNumBlockBytes);
	uint32 uViewSize(uNumElements * uElementSize);
	bool bEntireBuffer(uFirstElement == 0 && uViewSize == pBuffer->m_uSize);

	#if !defined(glTexBufferRange)
	if (!bEntireBuffer)
	{
		DXGL_ERROR("glTexBufferRange is not supported - can't create an arbitrary texture buffer");
		return CResourceName();
	}
	#endif //!defined(glTexBufferRange)

	GLuint uView;
	glGenTextures(1, &uView);

	#if defined(glTexBufferRange)
	if (!bEntireBuffer)
		glTextureBufferRangeEXT(uView, GL_TEXTURE_BUFFER, pFormatInfo->m_pTexture->m_iInternalFormat, pBuffer->m_kName.GetName(), uFirstElement * uElementSize, uViewSize);
	else
	#endif //defined(glTexBufferRange)
	glTextureBufferEXT(uView, GL_TEXTURE_BUFFER, pFormatInfo->m_pTexture->m_iInternalFormat, pBuffer->m_kName.GetName());

	return pContext->GetDevice()->GetTextureNamePool().Create(uView);
#else
	DXGL_ERROR("Buffer shader resource views are not supported on this configuration");
	return CResourceName();
#endif
}

SShaderView::SShaderView(Type eType)
	: m_eType(eType)
{
}

SShaderView::~SShaderView()
{
}

SShaderBufferView::SShaderBufferView(const SBufferRange& kRange)
	: SShaderView(eSVT_Buffer)
	, m_kRange(kRange)
{
}

SShaderBufferView::~SShaderBufferView()
{
}

void SShaderBufferView::Init(SBuffer* pBuffer, CContext* pContext)
{
	pBuffer->m_kCreationFence.IssueWait(pContext);
	m_kName = pBuffer->m_kName;
	m_kCreationFence.IssueFence(pContext->GetDevice());
}

SShaderTextureBasedView::SShaderTextureBasedView(Type eType)
	: SShaderView(eType)
	, m_bTextureOwned(false)
{
}

SShaderTextureBasedView::~SShaderTextureBasedView()
{
	if (m_bTextureOwned)
	{
		GLuint uTextureName(m_kName.GetName());
		glDeleteTextures(1, &uTextureName);
	}
}

bool SShaderTextureBasedView::BindTextureUnit(SSamplerState*, STextureUnitContext&, CContext*)
{
	DXGL_ERROR("Cannot bind this type of texture based view to a texture unit");
	return false;
}

bool SShaderTextureBasedView::GenerateMipmaps(CContext*)
{
	DXGL_ERROR("Cannot create mipmaps from this type of texture based view");
	return false;
}

SShaderTextureBufferView::SShaderTextureBufferView(const SShaderTextureBufferViewConfiguration& kConfiguration)
	: SShaderTextureBasedView(eSVT_Texture)
	, m_pBuffer(NULL)
	, m_kConfiguration(kConfiguration)
	, m_pNextView(NULL)
{
}

SShaderTextureBufferView::~SShaderTextureBufferView()
{
	if (m_pBuffer != NULL)
	{
		SShaderTextureBufferView** pLink = &m_pBuffer->m_pTextureBuffersHead;
		while (*pLink != NULL)
		{
			if (*pLink == this)
			{
				*pLink = m_pNextView;
				break;
			}
			pLink = &((*pLink)->m_pNextView);
		}
	}
}

bool SShaderTextureBufferView::Init(SBuffer* pBuffer, CContext* pContext)
{
	const SGIFormatInfo* pFormatInfo;
	if ((pFormatInfo = GetGIFormatInfo(m_kConfiguration.m_eFormat)) == NULL ||
	    pFormatInfo->m_pTexture == NULL)
	{
		DXGL_ERROR("Invalid format for buffer view");
		return false;
	}

	pBuffer->m_kCreationFence.IssueWait(pContext);

	CResourceName kTextureBufferName(CreateTextureBuffer(pBuffer, pFormatInfo, m_kConfiguration.m_uFirstElement, m_kConfiguration.m_uNumElements, pContext));
	if (!kTextureBufferName.IsValid())
		return false;

	m_kName = kTextureBufferName;
	m_kCreationFence.IssueFence(pContext->GetDevice());
	m_bTextureOwned = true;

	m_pBuffer = pBuffer;
	m_pNextView = pBuffer->m_pTextureBuffersHead;
	pBuffer->m_pTextureBuffersHead = this;

	return true;
}

bool SShaderTextureBufferView::BindTextureUnit(SSamplerState* pSamplerState, STextureUnitContext& kContext, CContext* pContext)
{
	kContext.m_kCurrentUnitState.m_kTextureName = m_kName;
	kContext.m_kCurrentUnitState.m_uSampler = pSamplerState->m_uSamplerObjectNoMip;
#if DXGLES
	// OpenGL ES does not have texture buffers
	DXGL_TODO("Find alternatives for TEXTURE BUFFER")
	kContext.m_kCurrentUnitState.m_eTextureTarget = GL_INVALID_ENUM;
#else
	kContext.m_kCurrentUnitState.m_eTextureTarget = GL_TEXTURE_BUFFER;
#endif
	assert(m_kName.IsValid());
	return true;
}

SShaderTextureView::SShaderTextureView(const SShaderTextureViewConfiguration& kConfiguration)
	: SShaderTextureBasedView(eSVT_Texture)
	, m_pTexture(nullptr)
	, m_kConfiguration(kConfiguration)
	, m_pNextView(nullptr)
{
	ZeroStruct(m_kViewState);
}

SShaderTextureView::~SShaderTextureView()
{
	if (m_pTexture != NULL)
	{
		SShaderTextureView** pLink = &m_pTexture->m_pShaderViewsHead;
		while (*pLink != NULL)
		{
			if (*pLink == this)
			{
				*pLink = m_pNextView;
				break;
			}
			pLink = &((*pLink)->m_pNextView);
		}
	}
}

bool SShaderTextureView::Init(STexture* pTexture, CContext* pContext)
{
	const SGIFormatInfo* pFormatInfo;
	if (m_kConfiguration.m_eFormat == eGIF_NUM ||
	    (pFormatInfo = GetGIFormatInfo(m_kConfiguration.m_eFormat)) == NULL)
	{
		DXGL_ERROR("Invalid format for shader resource view");
		return false;
	}

	m_kViewState.m_iBaseMipLevel = (GLint)m_kConfiguration.m_uMinMipLevel;
	m_kViewState.m_iMaxMipLevel = (GLint)(m_kConfiguration.m_uMinMipLevel + m_kConfiguration.m_uNumMipLevels - 1);
	m_kViewState.m_uSwizzleMask = pFormatInfo->m_uDXGISwizzleMask;
#if DXGL_SUPPORT_STENCIL_TEXTURES
	m_kViewState.m_iDepthStencilTextureMode = 0;
#endif //DXGL_SUPPORT_STENCIL_TEXTURES
	m_pTexture = pTexture;

	bool bFormatRequiresUniqueView(false);
	if (m_kConfiguration.m_eFormat != pTexture->m_eFormat)
	{
		if (pFormatInfo->m_eTypelessFormat != pTexture->m_eFormat)
		{
			DXGL_ERROR("Shader resource view format is not compatible with texture format");
			return false;
		}

		switch (pFormatInfo->m_eTypelessConversion)
		{
		case eGIFC_DEPTH_TO_RED:
#if DXGL_SUPPORT_STENCIL_TEXTURES
			m_kViewState.m_iDepthStencilTextureMode = GL_DEPTH_COMPONENT;
#endif  //DXGL_SUPPORT_STENCIL_TEXTURES
			break;
		case eGIFC_STENCIL_TO_RED:
#if DXGL_SUPPORT_STENCIL_TEXTURES
			m_kViewState.m_iDepthStencilTextureMode = GL_STENCIL_INDEX;
#else
			DXGL_ERROR("Stencil texturing not supported - cannot create shader resource view for stencil format");
			return false;
#endif
			break;
		case eGIFC_TEXTURE_VIEW:
			bFormatRequiresUniqueView = true;
			break;
		case eGIFC_UNSUPPORTED:
			DXGL_ERROR("Shader resource view conversion not supported for the requested format");
			return false;
		}
	}

	if (m_kConfiguration.m_eTarget != pTexture->m_eTarget)
		bFormatRequiresUniqueView = true;

	if (bFormatRequiresUniqueView ||
	    m_kConfiguration.m_uMinLayer > 0 ||
	    m_kConfiguration.m_uNumLayers != pTexture->m_uNumElements)
	{
		if (!CreateUniqueView(pContext))
			return false;
	}
	else
	{
		m_kName = pTexture->m_kName;
		pTexture->m_kCreationFence.IssueWait(pContext);
		m_kCreationFence.IssueFence(pContext->GetDevice());
		m_bTextureOwned = false;
	}

	m_pNextView = pTexture->m_pShaderViewsHead;
	pTexture->m_pShaderViewsHead = this;

	return true;
}

bool SShaderTextureView::CreateUniqueView(CContext* pContext)
{
	const SGIFormatInfo* pFormatInfo(GetGIFormatInfo(m_kConfiguration.m_eFormat));

	m_pTexture->m_kCreationFence.IssueWait(pContext);

	CResourceName kUniqueView = CreateTextureView(
	  m_pTexture, pFormatInfo, m_kConfiguration.m_eTarget,
	  m_kConfiguration.m_uMinMipLevel, m_kConfiguration.m_uNumMipLevels,
	  m_kConfiguration.m_uMinLayer, m_kConfiguration.m_uNumLayers,
	  pContext);

	if (kUniqueView.IsValid())
	{
		m_kName = kUniqueView;
		m_bTextureOwned = true;

		m_kViewState.ApplyFormatMode(kUniqueView.GetName(), m_kConfiguration.m_eTarget);
		m_kCreationFence.IssueFence(pContext->GetDevice());

		return true;
	}
	return false;
}

bool SShaderTextureView::BindTextureUnit(SSamplerState* pSamplerState, STextureUnitContext& kContext, CContext* pContext)
{
	kContext.m_kCurrentUnitState.m_uSampler = m_kConfiguration.m_uNumMipLevels > 1 ?
	                                          pSamplerState->m_uSamplerObjectMip :
	                                          pSamplerState->m_uSamplerObjectNoMip;
	kContext.m_kCurrentUnitState.m_eTextureTarget = m_kConfiguration.m_eTarget;

	if (!m_bTextureOwned)
	{
		if (m_kConfiguration.m_eTarget == m_pTexture->m_eTarget)
		{
			if (m_pTexture->m_pBoundModifier == this)
			{
				kContext.m_kCurrentUnitState.m_kTextureName = m_pTexture->m_kName;
				return true;
			}

			if (m_pTexture->m_pBoundModifier == NULL)
			{
				if (m_pTexture->m_kCache != m_kViewState)
				{
					m_kViewState.Apply(m_pTexture->m_kName.GetName(), m_kConfiguration.m_eTarget);
					m_pTexture->m_kCache = m_kViewState;
				}
				m_pTexture->m_pBoundModifier = this;
				kContext.m_kModifiedTextures.push_back(m_pTexture);
				kContext.m_kCurrentUnitState.m_kTextureName = m_pTexture->m_kName;
				return true;
			}
		}

		if (!CreateUniqueView(pContext))
			return false;
	}

	kContext.m_kCurrentUnitState.m_kTextureName = m_kName;
	return true;
}

bool SShaderTextureView::GenerateMipmaps(CContext* pContext)
{
	DXGL_TODO("Verify the expected behavior when the view has non-default parameters (currently ignored)")
	glGenerateTextureMipmapEXT(m_pTexture->m_kName.GetName(), m_kConfiguration.m_eTarget);
	m_kCreationFence.IssueWait(pContext);
	return true;
}

SShaderImageView::SShaderImageView(const SShaderImageViewConfiguration& kConfiguration)
	: SShaderTextureBasedView(eSVT_Image)
	, m_kConfiguration(kConfiguration)
{
}

SShaderImageView::~SShaderImageView()
{
}

bool SShaderImageView::Init(STexture* pTexture, CContext* pContext)
{
	m_kName = pTexture->m_kName;
	pTexture->m_kCreationFence.IssueWait(pContext);
	m_kCreationFence.IssueFence(pContext->GetDevice());
	m_bTextureOwned = false;
	return true;
}

bool SShaderImageView::Init(SShaderTextureBasedView* pTextureBufferView, CContext* pContext)
{
	m_spAuxiliaryView = pTextureBufferView;
	m_kName = pTextureBufferView->m_kName;
	pTextureBufferView->m_kCreationFence.IssueWait(pContext);
	m_kCreationFence.IssueFence(pContext->GetDevice());
	m_bTextureOwned = false;
	return true;
}

SOutputMergerView::SOutputMergerView(const CResourceName& kUniqueView, EGIFormat eFormat)
	: m_kUniqueView(kUniqueView)
	, m_eFormat(eFormat)
{
#if !OGL_SINGLE_CONTEXT
	memset(m_kContextMap, 0, sizeof(m_kContextMap));
#endif
}

SOutputMergerView::~SOutputMergerView()
{
#if OGL_SINGLE_CONTEXT
	SContextData* pContextData(&m_kContextData);
	{
#else
	uint32 uContext;
	for (uContext = 0; uContext < DXGL_ARRAY_SIZE(m_kContextMap); ++uContext)
	{
		SContextData* pContextData(m_kContextMap[uContext]);
		if (pContextData != NULL)
#endif
		{
			// All frame buffers with this view bound must be removed from the cache and deleted
			TFrameBufferRefs::iterator kFrameBufferRefIter(pContextData->m_kBoundFrameBuffers.begin());
			const TFrameBufferRefs::iterator kFrameBufferRefEnd(pContextData->m_kBoundFrameBuffers.end());
			while (kFrameBufferRefIter != kFrameBufferRefEnd)
			{
				if (--kFrameBufferRefIter->m_uNumBindings == 0)
				{
					SFrameBuffer* pFrameBuffer(kFrameBufferRefIter->m_spFrameBuffer);
					pFrameBuffer->m_pContext->RemoveFrameBuffer(pFrameBuffer, this);
				}
				++kFrameBufferRefIter;
			}

#if !OGL_SINGLE_CONTEXT
			delete pContextData;
#endif
		}
	}

	if (m_kUniqueView.IsValid())
	{
		GLuint uUniqueViewName(m_kUniqueView.GetName());
		glDeleteTextures(1, &uUniqueViewName);
	}
}

bool SOutputMergerView::AttachFrameBuffer(SFrameBuffer* pFrameBuffer, GLenum eAttachmentID, CContext* pContext)
{
#if OGL_SINGLE_CONTEXT
	SContextData* pContextData(&m_kContextData);
#else
	SContextData* pContextData(m_kContextMap[pFrameBuffer->m_pContext->GetIndex()]);
	if (pContextData == NULL)
	{
		pContextData = new SContextData();
		m_kContextMap[pFrameBuffer->m_pContext->GetIndex()] = pContextData;
	}
#endif

	TFrameBufferRefs::iterator kFound(std::find(pContextData->m_kBoundFrameBuffers.begin(), pContextData->m_kBoundFrameBuffers.end(), pFrameBuffer));
	if (kFound == pContextData->m_kBoundFrameBuffers.end())
		pContextData->m_kBoundFrameBuffers.push_back(SFrameBufferRef(pFrameBuffer, 1));
	else
		++kFound->m_uNumBindings;

	return FrameBufferTexture(pFrameBuffer->m_kObject.m_kName.GetName(), eAttachmentID);
}

void SOutputMergerView::DetachFrameBuffer(SFrameBuffer* pFrameBuffer)
{
#if OGL_SINGLE_CONTEXT
	SContextData* pContextData(&m_kContextData);
#else
	SContextData* pContextData(m_kContextMap[pFrameBuffer->m_pContext->GetIndex()]);
	if (pContextData == NULL)
	{
		DXGL_ERROR("Frame buffer context has not been registered to this output merger view");
		return;
	}
#endif

	TFrameBufferRefs::iterator kFound(std::find(pContextData->m_kBoundFrameBuffers.begin(), pContextData->m_kBoundFrameBuffers.end(), pFrameBuffer));
	if (kFound != pContextData->m_kBoundFrameBuffers.end())
	{
		if (--kFound->m_uNumBindings == 0)
			pContextData->m_kBoundFrameBuffers.erase(kFound);
	}
	else
		DXGL_ERROR("Could not find the frame buffer to be detached from the view");
}

bool SOutputMergerView::FrameBufferTexture(GLuint uFrameBuffer, GLenum eAttachmentID)
{
	if (m_kUniqueView.IsValid())
	{
		glNamedFramebufferTextureEXT(uFrameBuffer, eAttachmentID, m_kUniqueView.GetName(), 0);
		return true;
	}
	return false;
}

void SOutputMergerView::Bind(const SFrameBuffer&, CContext*)
{
}

const GLint SOutputMergerTextureView::INVALID_LAYER = -1;

SOutputMergerTextureView::SOutputMergerTextureView(STexture* pTexture, const SOutputMergerTextureViewConfiguration& kConfiguration)
	: SOutputMergerView(CResourceName(), kConfiguration.m_eFormat)
	, m_pTexture(pTexture)
	, m_kConfiguration(kConfiguration)
	, m_pNextView(pTexture->m_pOutputMergerViewsHead)
	, m_iMipLevel(0)
	, m_iLayer(0)
{
	pTexture->m_pOutputMergerViewsHead = this;
}

SOutputMergerTextureView::~SOutputMergerTextureView()
{
	SOutputMergerTextureView*& pLink = m_pTexture->m_pOutputMergerViewsHead;
	while (pLink != NULL)
	{
		if (pLink == this)
			pLink = m_pNextView;
		else
			pLink = pLink->m_pNextView;
	}
}

bool SOutputMergerTextureView::Init(CContext* pContext)
{
	m_iMipLevel = (GLint)m_kConfiguration.m_uMipLevel;
	if (m_kConfiguration.m_uMinLayer == 0 && m_kConfiguration.m_uNumLayers == m_pTexture->m_uNumElements)
		m_iLayer = INVALID_LAYER;
	else if (m_kConfiguration.m_uNumLayers == 1)
		m_iLayer = m_kConfiguration.m_uMinLayer;
	else
	{
		DXGL_NOT_IMPLEMENTED;
	}

	if (m_kConfiguration.m_eFormat != m_pTexture->m_eFormat)
	{
		const SGIFormatInfo* pFormatInfo;
		if (m_kConfiguration.m_eFormat == eGIF_NUM ||
		    (pFormatInfo = GetGIFormatInfo(m_kConfiguration.m_eFormat)) == NULL ||
		    pFormatInfo->m_pTexture == NULL)
		{
			DXGL_ERROR("Invalid format for output merger view");
			return false;
		}

		if (pFormatInfo->m_pTexture->m_iInternalFormat == GetGIFormatInfo(m_pTexture->m_eFormat)->m_pTexture->m_iInternalFormat)
			return true;

		// Frame buffer attachment does not support any kind of in-place conversion - texture view is required unless no conversion is needed at all
		return CreateUniqueView(pFormatInfo, pContext);
	}

	return true;
}

bool SOutputMergerTextureView::FrameBufferTexture(GLuint uFrameBuffer, GLenum eAttachmentID)
{
	GLuint uTexture(m_kUniqueView.IsValid() ? m_kUniqueView.GetName() : m_pTexture->m_kName.GetName());
	if (m_iLayer == INVALID_LAYER)
		glNamedFramebufferTextureEXT(uFrameBuffer, eAttachmentID, uTexture, m_iMipLevel);
	else
		glNamedFramebufferTextureLayerEXT(uFrameBuffer, eAttachmentID, uTexture, m_iMipLevel, m_iLayer);

	return true;
}

bool SOutputMergerTextureView::CreateUniqueView(const SGIFormatInfo* pFormatInfo, CContext* pContext)
{
	if (pFormatInfo->m_eTypelessFormat != m_pTexture->m_eFormat)
	{
		DXGL_ERROR("Output merger view format is not compatible with texture format");
		return false;
	}

	m_kUniqueView = CreateTextureView(m_pTexture, pFormatInfo, m_pTexture->m_eTarget, 0, m_pTexture->m_uNumMipLevels, 0, m_pTexture->m_uNumElements, pContext);
	m_pTexture->m_kCreationFence.IssueWait(pContext);
	m_kCreationFence.IssueFence(pContext->GetDevice());

	return true;
}

SDefaultFrameBufferOutputMergerView::SDefaultFrameBufferOutputMergerView(SDefaultFrameBufferTexture* pTexture, const SOutputMergerTextureViewConfiguration& kConfiguration)
	: SOutputMergerTextureView(pTexture, kConfiguration)
	, m_bUsesTexture(false)
{
}

SDefaultFrameBufferOutputMergerView::~SDefaultFrameBufferOutputMergerView()
{
	if (m_bUsesTexture)
		static_cast<SDefaultFrameBufferTexture*>(m_pTexture)->DecTextureRefCount();
}

bool SDefaultFrameBufferOutputMergerView::AttachFrameBuffer(SFrameBuffer* pFrameBuffer, GLenum eAttachmentID, CContext* pContext)
{
	if (!m_bUsesTexture)
	{
		static_cast<SDefaultFrameBufferTexture*>(m_pTexture)->IncTextureRefCount(pContext);
		m_bUsesTexture = true;
	}
	return SOutputMergerTextureView::AttachFrameBuffer(pFrameBuffer, eAttachmentID, pContext);
}

void SDefaultFrameBufferOutputMergerView::Bind(const SFrameBuffer& kFrameBuffer, CContext* pContext)
{
	static_cast<SDefaultFrameBufferTexture*>(m_pTexture)->OnWrite(pContext, kFrameBuffer.m_kObject.m_kName.GetName() == 0);
	SOutputMergerTextureView::Bind(kFrameBuffer, pContext);
}

bool SDefaultFrameBufferOutputMergerView::CreateUniqueView(const SGIFormatInfo* pFormatInfo, CContext* pContext)
{
	if (!m_bUsesTexture)
	{
		static_cast<SDefaultFrameBufferTexture*>(m_pTexture)->IncTextureRefCount(pContext);
		m_bUsesTexture = true;
	}
	return SOutputMergerTextureView::CreateUniqueView(pFormatInfo, pContext);
}

SDefaultFrameBufferShaderTextureView::SDefaultFrameBufferShaderTextureView(const SShaderTextureViewConfiguration& kConfiguration)
	: SShaderTextureView(kConfiguration)
	, m_bUsesTexture(false)
{
}

SDefaultFrameBufferShaderTextureView::~SDefaultFrameBufferShaderTextureView()
{
}

bool SDefaultFrameBufferShaderTextureView::BindTextureUnit(SSamplerState* pSamplerState, STextureUnitContext& kContext, CContext* pContext)
{
	SDefaultFrameBufferTexture* pDefaultFrameBufferTexture(static_cast<SDefaultFrameBufferTexture*>(m_pTexture));
	if (!m_bUsesTexture)
	{
		pDefaultFrameBufferTexture->IncTextureRefCount(pContext);
		m_bUsesTexture = true;
	}
	pDefaultFrameBufferTexture->OnRead(pContext);
	return SShaderTextureBasedView::BindTextureUnit(pSamplerState, kContext, pContext);
}

template<typename ViewDesc, typename View>
struct SResourceViewBaseImpl
{
	typedef ViewDesc          TViewDesc;
	typedef View              TView;
	typedef _smart_ptr<TView> TViewPtr;
};

struct SShaderResourceViewImpl : SResourceViewBaseImpl<D3D11_SHADER_RESOURCE_VIEW_DESC, SShaderView>
{
	enum EViewDimension
	{
		DIMENSION_BUFFER           = D3D11_SRV_DIMENSION_BUFFER,
		DIMENSION_TEXTURE1D        = D3D11_SRV_DIMENSION_TEXTURE1D,
		DIMENSION_TEXTURE1DARRAY   = D3D11_SRV_DIMENSION_TEXTURE1DARRAY,
		DIMENSION_TEXTURE2D        = D3D11_SRV_DIMENSION_TEXTURE2D,
		DIMENSION_TEXTURE2DARRAY   = D3D11_SRV_DIMENSION_TEXTURE2DARRAY,
		DIMENSION_TEXTURE2DMS      = D3D11_SRV_DIMENSION_TEXTURE2DMS,
		DIMENSION_TEXTURE2DMSARRAY = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY,
		DIMENSION_TEXTURE3D        = D3D11_SRV_DIMENSION_TEXTURE3D,
		DIMENSION_TEXTURECUBE      = D3D11_SRV_DIMENSION_TEXTURECUBE,
		DIMENSION_TEXTURECUBEARRAY = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY,
		DIMENSION_BUFFEREX         = D3D11_SRV_DIMENSION_BUFFEREX
	};

	static SResourceViewBaseImpl::TViewPtr GetView(STexture* pTexture, DXGI_FORMAT eDXGIFormat, GLenum eTarget, uint32 uMinLevel, uint32 uNumLevels, uint32 uMinElement, uint32 uNumElements, CContext* pContext)
	{
		SShaderTextureViewConfiguration kConfiguration(GetGIFormat(eDXGIFormat), eTarget, uMinLevel, uNumLevels, uMinElement, uNumElements);

		SShaderTextureView* pExistingView(pTexture->m_pShaderViewsHead);
		while (pExistingView != NULL)
		{
			if (pExistingView->m_kConfiguration == kConfiguration)
				return pExistingView;
			pExistingView = pExistingView->m_pNextView;
		}

		return pTexture->CreateShaderResourceView(kConfiguration, pContext);
	}

	template<typename DimDesc>
	static TViewPtr GetViewMip(STexture* pTexture, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, GLenum eTarget, uint32 uMinElement, uint32 uNumElements, CContext* pContext)
	{
		uint32 uNumMipLevels(kDimDesc.MipLevels == -1 ? (pTexture->m_uNumMipLevels - kDimDesc.MostDetailedMip) : kDimDesc.MipLevels);
		return GetView(pTexture, eDXGIFormat, eTarget, kDimDesc.MostDetailedMip, uNumMipLevels, uMinElement, uNumElements, pContext);
	}

	template<typename DimDesc>
	static TViewPtr GetViewLayers(STexture* pTexture, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, uint32 uMinLevel, uint32 uNumLevels, GLenum eTarget, CContext* pContext)
	{
		uint32 uNumElements(kDimDesc.ArraySize == -1 ? (pTexture->m_uNumElements - kDimDesc.FirstArraySlice) : kDimDesc.ArraySize);
		return GetView(pTexture, eDXGIFormat, eTarget, uMinLevel, uNumLevels, kDimDesc.FirstArraySlice, uNumElements, pContext);
	}

	template<typename DimDesc>
	static TViewPtr GetViewMipLayers(STexture* pTexture, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, GLenum eTarget, CContext* pContext)
	{
		uint32 uNumElements(kDimDesc.ArraySize == -1 ? (pTexture->m_uNumElements - kDimDesc.FirstArraySlice) : kDimDesc.ArraySize);
		return GetViewMip(pTexture, kDimDesc, eDXGIFormat, eTarget, kDimDesc.FirstArraySlice, uNumElements, pContext);
	}

	template<typename DimDesc>
	static TViewPtr GetView(SBuffer* pBuffer, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, UINT uFlags, CContext* pContext)
	{
		return pBuffer->CreateShaderResourceView(GetGIFormat(eDXGIFormat), kDimDesc.FirstElement, kDimDesc.NumElements, uFlags, pContext);
	}

	template<typename DimDesc>
	static TViewPtr GetViewEx(SBuffer* pBuffer, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, CContext* pContext)
	{
		return GetView(pBuffer, kDimDesc, eDXGIFormat, kDimDesc.Flags, pContext);
	}
};

struct SUnorderedAccessViewImpl : SResourceViewBaseImpl<D3D11_UNORDERED_ACCESS_VIEW_DESC, SShaderView>
{
	enum EViewDimension
	{
		DIMENSION_BUFFER         = D3D11_UAV_DIMENSION_BUFFER,
		DIMENSION_TEXTURE1D      = D3D11_UAV_DIMENSION_TEXTURE1D,
		DIMENSION_TEXTURE1DARRAY = D3D11_UAV_DIMENSION_TEXTURE1DARRAY,
		DIMENSION_TEXTURE2D      = D3D11_UAV_DIMENSION_TEXTURE2D,
		DIMENSION_TEXTURE2DARRAY = D3D11_UAV_DIMENSION_TEXTURE2DARRAY,
		DIMENSION_TEXTURE3D      = D3D11_UAV_DIMENSION_TEXTURE3D,
	};

	static SResourceViewBaseImpl::TViewPtr GetView(STexture* pTexture, DXGI_FORMAT eDXGIFormat, GLenum eTarget, uint32 uMipSlice, uint32 uMinElement, uint32 uNumElements, CContext* pContext)
	{
		EGIFormat eGIFormat(GetGIFormat(eDXGIFormat));
		GLenum eImageFormat(GL_NONE);
		if (!GetImageFormat(eGIFormat, &eImageFormat))
		{
			DXGL_ERROR("Unsupported format for unordered access view");
			return NULL;
		}

		if (uMinElement == 0 && uNumElements == pTexture->m_uNumElements)
		{
			SShaderImageViewConfiguration kConfiguration(eImageFormat, (GLint)uMipSlice, -1, GL_READ_WRITE);
			return pTexture->CreateUnorderedAccessView(kConfiguration, pContext);
		}
		else if (uNumElements == 1)
		{
			SShaderImageViewConfiguration kConfiguration(eImageFormat, (GLint)uMipSlice, uMinElement, GL_READ_WRITE);
			return pTexture->CreateUnorderedAccessView(kConfiguration, pContext);
		}
		else
		{
			// Need to first create a proxy view containing only the selected layers
			SShaderTextureViewConfiguration kProxyConfiguration(pTexture->m_eFormat, eTarget, uMipSlice, 1, uMinElement, uNumElements);
			SShaderTextureViewPtr spProxyTextureView(pTexture->CreateShaderResourceView(kProxyConfiguration, pContext));
			if (spProxyTextureView == NULL)
				return NULL;

			SShaderImageViewConfiguration kConfiguration(eImageFormat, (GLint)uMipSlice, -1, GL_READ_WRITE);
			SShaderImageViewPtr spImageView(new SShaderImageView(kConfiguration));
			if (!spImageView->Init(spProxyTextureView, pContext))
				return NULL;

			return spImageView;
		}
	}

	template<typename DimDesc>
	static TViewPtr GetViewMip(STexture* pTexture, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, GLenum eTarget, uint32 uMinElement, uint32 uNumElements, CContext* pContext)
	{
		return GetView(pTexture, eDXGIFormat, eTarget, kDimDesc.MipSlice, uMinElement, uNumElements, pContext);
	}

	template<typename DimDesc>
	static TViewPtr GetViewLayers(STexture* pTexture, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, uint32 uMipSlice, GLenum eTarget, CContext* pContext)
	{
		uint32 uNumElements(kDimDesc.ArraySize == -1 ? (pTexture->m_uNumElements - kDimDesc.FirstArraySlice) : kDimDesc.ArraySize);
		return GetView(pTexture, eDXGIFormat, eTarget, uMipSlice, kDimDesc.FirstArraySlice, uNumElements, pContext);
	}

	template<typename DimDesc>
	static TViewPtr GetViewMipLayers(STexture* pTexture, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, GLenum eTarget, CContext* pContext)
	{
		uint32 uNumElements(kDimDesc.ArraySize == -1 ? (pTexture->m_uNumElements - kDimDesc.FirstArraySlice) : kDimDesc.ArraySize);
		return GetViewMip(pTexture, kDimDesc, eDXGIFormat, eTarget, kDimDesc.FirstArraySlice, uNumElements, pContext);
	}

	template<typename DimDesc>
	static TViewPtr GetView(SBuffer* pBuffer, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, UINT uFlags, CContext* pContext)
	{
		return pBuffer->CreateUnorderedAccessView(GetGIFormat(eDXGIFormat), kDimDesc.FirstElement, kDimDesc.NumElements, uFlags, pContext);
	}

	template<typename DimDesc>
	static TViewPtr GetViewEx(SBuffer* pBuffer, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, CContext* pContext)
	{
		return GetView(pBuffer, kDimDesc, eDXGIFormat, kDimDesc, kDimDesc.Flags);
	}
};

template<typename ViewDesc>
struct SOutputMergerViewImpl : SResourceViewBaseImpl<ViewDesc, SOutputMergerView>
{
	static typename SOutputMergerViewImpl::TViewPtr GetView(STexture* pTexture, DXGI_FORMAT eDXGIFormat, GLenum, uint32 uMipLevel, uint32, uint32 uMinElement, uint32 uNumElements, CContext* pContext)
	{
		return pTexture->GetCompatibleOutputMergerView(SOutputMergerTextureViewConfiguration(GetGIFormat(eDXGIFormat), uMipLevel, uMinElement, uNumElements), pContext);
	}

	template<typename DimDesc>
	static typename SOutputMergerViewImpl::TViewPtr GetViewMip(STexture* pTexture, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, GLenum eTarget, uint32 uMinElement, uint32 uNumElements, CContext* pContext)
	{
		return GetView(pTexture, eDXGIFormat, eTarget, kDimDesc.MipSlice, 1, uMinElement, uNumElements, pContext);
	}

	template<typename DimDesc>
	static typename SOutputMergerViewImpl::TViewPtr GetViewLayers(STexture* pTexture, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, GLenum eTarget, uint32 uMinLevel, uint32 uNumLevels, CContext* pContext)
	{
		uint32 uNumElements(kDimDesc.ArraySize == -1 ? (pTexture->m_uNumElements - kDimDesc.FirstArraySlice) : kDimDesc.ArraySize);
		return GetView(pTexture, eDXGIFormat, eTarget, uMinLevel, uNumLevels, kDimDesc.FirstArraySlice, uNumElements, pContext);
	}

	template<typename DimDesc>
	static typename SOutputMergerViewImpl::TViewPtr GetViewMipLayers(STexture* pTexture, const DimDesc& kDimDesc, DXGI_FORMAT eDXGIFormat, GLenum eTarget, CContext* pContext)
	{
		uint32 uNumElements(kDimDesc.ArraySize == -1 ? (pTexture->m_uNumElements - kDimDesc.FirstArraySlice) : kDimDesc.ArraySize);
		return GetViewMip(pTexture, kDimDesc, eDXGIFormat, eTarget, kDimDesc.FirstArraySlice, uNumElements, pContext);
	}
};

struct SRenderTargetViewImpl : SOutputMergerViewImpl<D3D11_RENDER_TARGET_VIEW_DESC>
{
	enum EViewDimension
	{
		DIMENSION_BUFFER           = D3D11_RTV_DIMENSION_BUFFER,
		DIMENSION_TEXTURE1D        = D3D11_RTV_DIMENSION_TEXTURE1D,
		DIMENSION_TEXTURE1DARRAY   = D3D11_RTV_DIMENSION_TEXTURE1DARRAY,
		DIMENSION_TEXTURE2D        = D3D11_RTV_DIMENSION_TEXTURE2D,
		DIMENSION_TEXTURE2DARRAY   = D3D11_RTV_DIMENSION_TEXTURE2DARRAY,
		DIMENSION_TEXTURE2DMS      = D3D11_RTV_DIMENSION_TEXTURE2DMS,
		DIMENSION_TEXTURE2DMSARRAY = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY,
		DIMENSION_TEXTURE3D        = D3D11_RTV_DIMENSION_TEXTURE3D
	};
};

struct SDepthStencilViewImpl : SOutputMergerViewImpl<D3D11_DEPTH_STENCIL_VIEW_DESC>
{
	enum EViewDimension
	{
		DIMENSION_TEXTURE1D        = D3D11_DSV_DIMENSION_TEXTURE1D,
		DIMENSION_TEXTURE1DARRAY   = D3D11_DSV_DIMENSION_TEXTURE1DARRAY,
		DIMENSION_TEXTURE2D        = D3D11_DSV_DIMENSION_TEXTURE2D,
		DIMENSION_TEXTURE2DARRAY   = D3D11_DSV_DIMENSION_TEXTURE2DARRAY,
		DIMENSION_TEXTURE2DMS      = D3D11_DSV_DIMENSION_TEXTURE2DMS,
		DIMENSION_TEXTURE2DMSARRAY = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY,
	};
};

template<typename Impl>
typename Impl::TViewPtr GetTexture1DView(STexture* pTexture, const typename Impl::TViewDesc& kViewDesc, CContext* pContext)
{
	switch (kViewDesc.ViewDimension)
	{
	case Impl::DIMENSION_TEXTURE1D:
		return Impl::GetViewMip(pTexture, kViewDesc.Texture1D, kViewDesc.Format, GL_TEXTURE_1D, 0, 1, pContext);
	case Impl::DIMENSION_TEXTURE1DARRAY:
		return Impl::GetViewMipLayers(pTexture, kViewDesc.Texture1DArray, kViewDesc.Format, GL_TEXTURE_1D_ARRAY, pContext);
	}
	return NULL;
}

template<typename Impl>
typename Impl::TViewPtr GetTexture2DView(STexture* pTexture, const typename Impl::TViewDesc& kViewDesc, CContext* pContext)
{
	switch (kViewDesc.ViewDimension)
	{
	case Impl::DIMENSION_TEXTURE2D:
		return Impl::GetViewMip(pTexture, kViewDesc.Texture2D, kViewDesc.Format, GL_TEXTURE_2D, 0, 1, pContext);
	case Impl::DIMENSION_TEXTURE2DARRAY:
		return Impl::GetViewMipLayers(pTexture, kViewDesc.Texture2DArray, kViewDesc.Format, GL_TEXTURE_2D_ARRAY, pContext);
	}
	return NULL;
}

template<typename Impl>
typename Impl::TViewPtr GetTexture2DMSView(STexture* pTexture, const typename Impl::TViewDesc& kViewDesc, CContext* pContext)
{
	switch (kViewDesc.ViewDimension)
	{
#if DXGL_SUPPORT_MULTISAMPLED_TEXTURES
	case Impl::DIMENSION_TEXTURE2DMS:
		return Impl::GetView(pTexture, kViewDesc.Format, GL_TEXTURE_2D_MULTISAMPLE, 0, 1, 0, 1, pContext);
	#if !DXGLES
	case Impl::DIMENSION_TEXTURE2DMSARRAY:
		return Impl::GetViewLayers(pTexture, kViewDesc.Texture2DMSArray, kViewDesc.Format, GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0, 1, pContext);
	#endif //!DXGLES
#endif   //DXGL_SUPPORT_MULTISAMPLED_TEXTURES
	}
	return NULL;
}

template<typename Impl>
typename Impl::TViewPtr GetTextureCubeView(STexture* pTexture, const typename Impl::TViewDesc& kViewDesc, CContext* pContext)
{
	switch (kViewDesc.ViewDimension)
	{
	case D3D11_SRV_DIMENSION_TEXTURECUBE:
		return Impl::GetViewMip(pTexture, kViewDesc.TextureCube, kViewDesc.Format, GL_TEXTURE_CUBE_MAP, 0, 6, pContext);
#if DXGL_SUPPORT_CUBEMAP_ARRAYS
	case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
		return Impl::GetViewMip(pTexture, kViewDesc.TextureCubeArray, kViewDesc.Format, GL_TEXTURE_CUBE_MAP_ARRAY, kViewDesc.TextureCubeArray.First2DArrayFace, 6 * kViewDesc.TextureCubeArray.NumCubes - kViewDesc.TextureCubeArray.First2DArrayFace, pContext);
#endif //DXGL_SUPPORT_CUBEMAP_ARRAYS
	}
	return NULL;
}

template<typename Impl>
typename Impl::TViewPtr GetTexture3DView(STexture* pTexture, const typename Impl::TViewDesc& kViewDesc, CContext* pContext)
{
	if ((uint32)kViewDesc.ViewDimension == (uint32)Impl::DIMENSION_TEXTURE3D)
		return Impl::GetViewMip(pTexture, kViewDesc.Texture3D, kViewDesc.Format, GL_TEXTURE_3D, 0, 1, pContext);
	return NULL;
}

template<typename Impl>
typename Impl::TViewPtr GetBufferView(SBuffer* pBuffer, const typename Impl::TViewDesc& kViewDesc, CContext* pContext)
{
	if ((uint32)kViewDesc.ViewDimension == (uint32)Impl::DIMENSION_BUFFER)
		return Impl::GetView(pBuffer, kViewDesc.Buffer, kViewDesc.Format, 0, pContext);
	return NULL;
}

template<typename Impl>
typename Impl::TViewPtr GetBufferViewEx(SBuffer* pBuffer, const typename Impl::TViewDesc& kViewDesc, CContext* pContext)
{
	if ((uint32)kViewDesc.ViewDimension == (uint32)Impl::DIMENSION_BUFFEREX)
		return Impl::GetViewEx(pBuffer, kViewDesc.BufferEx, kViewDesc.Format, pContext);
	return NULL;
}

SShaderViewPtr CreateShaderResourceView(SResource* pResource, D3D11_RESOURCE_DIMENSION eDimension, const D3D11_SHADER_RESOURCE_VIEW_DESC& kViewDesc, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CreateShaderResourceView")

	typedef SShaderResourceViewImpl TImpl;
	SShaderViewPtr spView;
	switch (eDimension)
	{
	case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		spView = GetTexture1DView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		spView = GetTexture2DView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		if (spView == NULL)
			spView = GetTexture2DMSView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		if (spView == NULL)
			spView = GetTextureCubeView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
		spView = GetTexture3DView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		break;
	case D3D11_RESOURCE_DIMENSION_BUFFER:
		spView = GetBufferView<TImpl>(static_cast<SBuffer*>(pResource), kViewDesc, pContext);
		if (spView == NULL)
			spView = GetBufferViewEx<TImpl>(static_cast<SBuffer*>(pResource), kViewDesc, pContext);
		break;
	default:
		DXGL_ERROR("Invalid resource dimension for shader resource");
		return NULL;
	}

	if (spView == NULL)
		DXGL_ERROR("Invalid shader resource view parameters");
	return spView;
}

SShaderViewPtr CreateUnorderedAccessView(SResource* pResource, D3D11_RESOURCE_DIMENSION eDimension, const D3D11_UNORDERED_ACCESS_VIEW_DESC& kViewDesc, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CreateShaderResourceView")

	typedef SUnorderedAccessViewImpl TImpl;
	SShaderViewPtr spView;
	switch (eDimension)
	{
	case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		spView = GetTexture1DView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		spView = GetTexture2DView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
		spView = GetTexture3DView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		break;
	case D3D11_RESOURCE_DIMENSION_BUFFER:
		spView = GetBufferView<TImpl>(static_cast<SBuffer*>(pResource), kViewDesc, pContext);
		break;
	default:
		DXGL_ERROR("Invalid resource dimension for shader resource");
		return NULL;
	}

	if (spView == NULL)
		DXGL_ERROR("Invalid shader resource view parameters");
	return spView;
}

SOutputMergerViewPtr CreateRenderTargetView(SResource* pResource, D3D11_RESOURCE_DIMENSION eDimension, const D3D11_RENDER_TARGET_VIEW_DESC& kViewDesc, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CreateRenderTargetView")

	typedef SRenderTargetViewImpl TImpl;
	SOutputMergerViewPtr spView;
	switch (eDimension)
	{
	case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		spView = GetTexture1DView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		spView = GetTexture2DView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		if (spView == NULL)
			spView = GetTexture2DMSView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
		spView = GetTexture3DView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		break;
	default:
		DXGL_ERROR("Invalid resource dimension for render target");
		return NULL;
	}
	if (spView == NULL)
		DXGL_ERROR("Invalid resource dimension for render target");
	return spView;
}

SOutputMergerViewPtr CreateDepthStencilView(SResource* pResource, D3D11_RESOURCE_DIMENSION eDimension, const D3D11_DEPTH_STENCIL_VIEW_DESC& kViewDesc, CContext* pContext)
{
	DXGL_SCOPED_PROFILE("CreateDepthStencilView")

	typedef SDepthStencilViewImpl TImpl;
	SOutputMergerViewPtr spView;
	switch (eDimension)
	{
	case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		spView = GetTexture1DView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		spView = GetTexture2DView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		if (spView == NULL)
			spView = GetTexture2DMSView<TImpl>(static_cast<STexture*>(pResource), kViewDesc, pContext);
		break;
	default:
		DXGL_ERROR("Invalid resource dimension for depth stencil");
		return NULL;
	}
	if (spView == NULL)
		DXGL_ERROR("Invalid resource dimension for depth stencil");
	return spView;
}

}
