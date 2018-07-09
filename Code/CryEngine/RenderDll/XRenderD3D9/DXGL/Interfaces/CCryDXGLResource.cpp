// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLResource.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11Resource
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLResource.hpp"
#include "../Implementation/GLResource.hpp"

CCryDXGLResource::CCryDXGLResource(D3D11_RESOURCE_DIMENSION eDimension, NCryOpenGL::SResource* pGLResource, CCryDXGLDevice* pDevice)
	: CCryDXGLDeviceChild(pDevice)
	, m_spGLResource(pGLResource)
	, m_eDimension(eDimension)
{
	DXGL_INITIALIZE_INTERFACE(D3D11Resource)
}

#if OGL_SINGLE_CONTEXT

CCryDXGLResource::CCryDXGLResource(D3D11_RESOURCE_DIMENSION eDimension, NCryOpenGL::SInitialDataCopy* pInitialDataCopy, CCryDXGLDevice* pDevice)
	: CCryDXGLDeviceChild(pDevice)
	, m_spInitialDataCopy(pInitialDataCopy)
	, m_eDimension(eDimension)
{
}

#endif

CCryDXGLResource::~CCryDXGLResource()
{
}

////////////////////////////////////////////////////////////////////////////////
// Implementation of ID3D11Resource
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLResource::GetType(D3D11_RESOURCE_DIMENSION* pResourceDimension)
{
	*pResourceDimension = m_eDimension;
}

void CCryDXGLResource::SetEvictionPriority(UINT EvictionPriority)
{
	DXGL_NOT_IMPLEMENTED
}

UINT CCryDXGLResource::GetEvictionPriority(void)
{
	DXGL_NOT_IMPLEMENTED
	return 0;
}
