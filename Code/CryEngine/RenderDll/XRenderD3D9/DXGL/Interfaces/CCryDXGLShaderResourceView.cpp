// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLShaderResourceView.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11ShaderResourceView
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLShaderResourceView.hpp"
#include "CCryDXGLDevice.hpp"
#include "CCryDXGLResource.hpp"
#include "../Implementation/GLView.hpp"
#include "../Implementation/GLDevice.hpp"

CCryDXGLShaderResourceView::CCryDXGLShaderResourceView(CCryDXGLResource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC& kDesc, CCryDXGLDevice* pDevice)
	: CCryDXGLView(pResource, pDevice)
	, m_kDesc(kDesc)
{
	DXGL_INITIALIZE_INTERFACE(D3D11ShaderResourceView)
}

CCryDXGLShaderResourceView::~CCryDXGLShaderResourceView()
{
}

bool CCryDXGLShaderResourceView::Initialize(NCryOpenGL::CContext* pContext)
{
	D3D11_RESOURCE_DIMENSION eDimension;
	m_spResource->GetType(&eDimension);
	m_spGLView = NCryOpenGL::CreateShaderResourceView(m_spResource->GetGLResource(), eDimension, m_kDesc, pContext);
	return m_spGLView != NULL;
}

#if OGL_SINGLE_CONTEXT

NCryOpenGL::SShaderView* CCryDXGLShaderResourceView::GetGLView(NCryOpenGL::CContext* pContext)
{
	IF_UNLIKELY (!m_spGLView)
	{
		if (!Initialize(pContext))
			DXGL_FATAL("Deferred shader resource view creation failed");
	}
	return m_spGLView;
}

#endif

////////////////////////////////////////////////////////////////////////////////
// Implementation of ID3D11ShaderResourceView
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLShaderResourceView::GetDesc(D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc)
{
	*pDesc = m_kDesc;
}
