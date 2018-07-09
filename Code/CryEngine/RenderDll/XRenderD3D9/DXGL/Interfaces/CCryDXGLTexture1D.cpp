// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLTexture1D.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11Texture1D
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLTexture1D.hpp"
#include "CCryDXGLDevice.hpp"
#include "../Implementation/GLResource.hpp"
#include "../Implementation/GLDevice.hpp"

CCryDXGLTexture1D::CCryDXGLTexture1D(const D3D11_TEXTURE1D_DESC& kDesc, NCryOpenGL::STexture* pGLTexture, CCryDXGLDevice* pDevice)
	: CCryDXGLTextureBase(D3D11_RESOURCE_DIMENSION_TEXTURE1D, pGLTexture, pDevice)
	, m_kDesc(kDesc)
{
	DXGL_INITIALIZE_INTERFACE(D3D11Texture1D)
}

#if OGL_SINGLE_CONTEXT

CCryDXGLTexture1D::CCryDXGLTexture1D(const D3D11_TEXTURE1D_DESC& kDesc, NCryOpenGL::SInitialDataCopy* pInitialData, CCryDXGLDevice* pDevice)
	: CCryDXGLTextureBase(D3D11_RESOURCE_DIMENSION_TEXTURE1D, pInitialData, pDevice)
	, m_kDesc(kDesc)
{
	DXGL_INITIALIZE_INTERFACE(D3D11Texture1D)
}

#endif

CCryDXGLTexture1D::~CCryDXGLTexture1D()
{
}

#if OGL_SINGLE_CONTEXT

void CCryDXGLTexture1D::Initialize()
{
	NCryOpenGL::CContext* pContext = m_pDevice->GetGLDevice()->GetCurrentContext();
	if (m_spInitialDataCopy)
	{
		m_spGLResource = NCryOpenGL::CreateTexture1D(m_kDesc, m_spInitialDataCopy->m_akSubresourceData, pContext);
		m_spInitialDataCopy = NULL;
	}
	else
		m_spGLResource = NCryOpenGL::CreateTexture1D(m_kDesc, NULL, pContext);

	if (m_spGLResource == NULL)
		DXGL_FATAL("Deferred 1D texture creation failed");
}

#endif

////////////////////////////////////////////////////////////////////////////////
// Implementation of ID3D11Texture1D
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLTexture1D::GetDesc(D3D11_TEXTURE1D_DESC* pDesc)
{
	*pDesc = m_kDesc;
}
