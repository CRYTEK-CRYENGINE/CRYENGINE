// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLTexture3D.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11Texture3D
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLTexture3D.hpp"
#include "CCryDXGLDevice.hpp"
#include "../Implementation/GLResource.hpp"
#include "../Implementation/GLDevice.hpp"

CCryDXGLTexture3D::CCryDXGLTexture3D(const D3D11_TEXTURE3D_DESC& kDesc, NCryOpenGL::STexture* pGLTexture, CCryDXGLDevice* pDevice)
	: CCryDXGLTextureBase(D3D11_RESOURCE_DIMENSION_TEXTURE3D, pGLTexture, pDevice)
	, m_kDesc(kDesc)
{
	DXGL_INITIALIZE_INTERFACE(D3D11Texture3D)
}

#if OGL_SINGLE_CONTEXT

CCryDXGLTexture3D::CCryDXGLTexture3D(const D3D11_TEXTURE3D_DESC& kDesc, NCryOpenGL::SInitialDataCopy* pInitialData, CCryDXGLDevice* pDevice)
	: CCryDXGLTextureBase(D3D11_RESOURCE_DIMENSION_TEXTURE3D, pInitialData, pDevice)
	, m_kDesc(kDesc)
{
	DXGL_INITIALIZE_INTERFACE(D3D11Texture3D)
}

#endif

CCryDXGLTexture3D::~CCryDXGLTexture3D()
{
}

#if OGL_SINGLE_CONTEXT

void CCryDXGLTexture3D::Initialize()
{
	NCryOpenGL::CContext* pContext = m_pDevice->GetGLDevice()->GetCurrentContext();
	if (m_spInitialDataCopy)
	{
		m_spGLResource = NCryOpenGL::CreateTexture3D(m_kDesc, m_spInitialDataCopy->m_akSubresourceData, pContext);
		m_spInitialDataCopy = NULL;
	}
	else
		m_spGLResource = NCryOpenGL::CreateTexture3D(m_kDesc, NULL, pContext);

	if (m_spGLResource == NULL)
		DXGL_FATAL("Deferred 3D texture creation failed");
}

#endif

////////////////////////////////////////////////////////////////////////////////
// Implementation of ID3D11Texture3D
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLTexture3D::GetDesc(D3D11_TEXTURE3D_DESC* pDesc)
{
	*pDesc = m_kDesc;
}
