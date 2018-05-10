// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLTexture2D.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11Texture2D
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLTexture2D.hpp"
#include "CCryDXGLDevice.hpp"
#include "../Implementation/GLResource.hpp"
#include "../Implementation/GLDevice.hpp"

CCryDXGLTexture2D::CCryDXGLTexture2D(const D3D11_TEXTURE2D_DESC& kDesc, NCryOpenGL::STexture* pGLTexture, CCryDXGLDevice* pDevice)
	: CCryDXGLTextureBase(D3D11_RESOURCE_DIMENSION_TEXTURE2D, pGLTexture, pDevice)
	, m_kDesc(kDesc)
{
	DXGL_INITIALIZE_INTERFACE(D3D11Texture2D)
}

#if OGL_SINGLE_CONTEXT

CCryDXGLTexture2D::CCryDXGLTexture2D(const D3D11_TEXTURE2D_DESC& kDesc, NCryOpenGL::SInitialDataCopy* pInitialData, CCryDXGLDevice* pDevice)
	: CCryDXGLTextureBase(D3D11_RESOURCE_DIMENSION_TEXTURE2D, pInitialData, pDevice)
	, m_kDesc(kDesc)
{
	DXGL_INITIALIZE_INTERFACE(D3D11Texture2D)
}

#endif

CCryDXGLTexture2D::~CCryDXGLTexture2D()
{
}

#if OGL_SINGLE_CONTEXT

void CCryDXGLTexture2D::Initialize()
{
	NCryOpenGL::CContext* pContext = m_pDevice->GetGLDevice()->GetCurrentContext();
	if (m_spInitialDataCopy)
	{
		m_spGLResource = NCryOpenGL::CreateTexture2D(m_kDesc, m_spInitialDataCopy->m_akSubresourceData, pContext);
		m_spInitialDataCopy = NULL;
	}
	else
		m_spGLResource = NCryOpenGL::CreateTexture2D(m_kDesc, NULL, pContext);

	if (m_spGLResource == NULL)
		DXGL_FATAL("Deferred 2D texture creation failed");
}

#endif

////////////////////////////////////////////////////////////////////////////////
// Implementation of ID3D11Texture2D
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLTexture2D::GetDesc(D3D11_TEXTURE2D_DESC* pDesc)
{
	*pDesc = m_kDesc;
}
