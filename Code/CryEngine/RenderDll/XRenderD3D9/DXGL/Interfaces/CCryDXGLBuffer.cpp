// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLBuffer.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11Buffer
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLBuffer.hpp"
#include "CCryDXGLDeviceContext.hpp"
#include "CCryDXGLDevice.hpp"
#include "../Implementation/GLResource.hpp"
#include "../Implementation/GLDevice.hpp"

CCryDXGLBuffer::CCryDXGLBuffer(const D3D11_BUFFER_DESC& kDesc, NCryOpenGL::SBuffer* pGLBuffer, CCryDXGLDevice* pDevice)
	: CCryDXGLResource(D3D11_RESOURCE_DIMENSION_BUFFER, pGLBuffer, pDevice)
	, m_kDesc(kDesc)
{
	DXGL_INITIALIZE_INTERFACE(D3D11Buffer)
}

#if OGL_SINGLE_CONTEXT

CCryDXGLBuffer::CCryDXGLBuffer(const D3D11_BUFFER_DESC& kDesc, NCryOpenGL::SInitialDataCopy* pInitialData, CCryDXGLDevice* pDevice)
	: CCryDXGLResource(D3D11_RESOURCE_DIMENSION_BUFFER, pInitialData, pDevice)
	, m_kDesc(kDesc)
{
	DXGL_INITIALIZE_INTERFACE(D3D11Buffer)
}

#endif

CCryDXGLBuffer::~CCryDXGLBuffer()
{
}

#if OGL_SINGLE_CONTEXT

void CCryDXGLBuffer::Initialize()
{
	NCryOpenGL::CContext* pContext = m_pDevice->GetGLDevice()->GetCurrentContext();
	if (m_spInitialDataCopy)
	{
		m_spGLResource = NCryOpenGL::CreateBuffer(m_kDesc, m_spInitialDataCopy->m_akSubresourceData, pContext);
		m_spInitialDataCopy = NULL;
	}
	else
		m_spGLResource = NCryOpenGL::CreateBuffer(m_kDesc, NULL, pContext);

	if (m_spGLResource == NULL)
		DXGL_FATAL("Deferred buffer creation failed");
}

#endif

NCryOpenGL::SBuffer* CCryDXGLBuffer::GetGLBuffer()
{
#if OGL_SINGLE_CONTEXT
	IF_UNLIKELY (!m_spGLResource)
		Initialize();
#endif
	return static_cast<NCryOpenGL::SBuffer*>(m_spGLResource.get());
}

////////////////////////////////////////////////////////////////////////////////
// ID3D11Buffer implementation
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLBuffer::GetDesc(D3D11_BUFFER_DESC* pDesc)
{
	(*pDesc) = m_kDesc;
}
