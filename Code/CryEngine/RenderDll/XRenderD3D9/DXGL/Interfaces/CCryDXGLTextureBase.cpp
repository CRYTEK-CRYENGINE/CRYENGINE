// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLTextureBase.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL common base class for textures
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLTextureBase.hpp"
#include "../Implementation/GLResource.hpp"

CCryDXGLTextureBase::CCryDXGLTextureBase(D3D11_RESOURCE_DIMENSION eDimension, NCryOpenGL::STexture* pGLTexture, CCryDXGLDevice* pDevice)
	: CCryDXGLResource(eDimension, pGLTexture, pDevice)
{
}

#if OGL_SINGLE_CONTEXT

CCryDXGLTextureBase::CCryDXGLTextureBase(D3D11_RESOURCE_DIMENSION eDimension, NCryOpenGL::SInitialDataCopy* pInitialData, CCryDXGLDevice* pDevice)
	: CCryDXGLResource(eDimension, pInitialData, pDevice)
{
}

#endif

CCryDXGLTextureBase::~CCryDXGLTextureBase()
{
}

NCryOpenGL::STexture* CCryDXGLTextureBase::GetGLTexture()
{
#if OGL_SINGLE_CONTEXT
	IF_UNLIKELY (!m_spGLResource)
		Initialize();
#endif
	return static_cast<NCryOpenGL::STexture*>(m_spGLResource.get());
}
