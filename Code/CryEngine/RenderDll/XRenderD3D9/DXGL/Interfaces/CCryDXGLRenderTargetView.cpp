// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLRenderTargetView.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11RenderTargetView
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLDevice.hpp"
#include "CCryDXGLRenderTargetView.hpp"
#include "CCryDXGLResource.hpp"
#include "../Implementation/GLDevice.hpp"
#include "../Implementation/GLView.hpp"

CCryDXGLRenderTargetView::CCryDXGLRenderTargetView(CCryDXGLResource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC& kDesc, CCryDXGLDevice* pDevice)
	: CCryDXGLView(pResource, pDevice)
	, m_kDesc(kDesc)
{
	DXGL_INITIALIZE_INTERFACE(D3D11RenderTargetView)
}

CCryDXGLRenderTargetView::~CCryDXGLRenderTargetView()
{
}

bool CCryDXGLRenderTargetView::Initialize(NCryOpenGL::CContext* pContext)
{
	D3D11_RESOURCE_DIMENSION eDimension;
	m_spResource->GetType(&eDimension);
	m_spGLView = NCryOpenGL::CreateRenderTargetView(m_spResource->GetGLResource(), eDimension, m_kDesc, pContext);
	return m_spGLView != NULL;
}

#if OGL_SINGLE_CONTEXT

NCryOpenGL::SOutputMergerView* CCryDXGLRenderTargetView::GetGLView(NCryOpenGL::CContext* pContext)
{
	IF_UNLIKELY (!m_spGLView)
	{
		if (!Initialize(pContext))
			DXGL_FATAL("Deferred output merger view creation failed");
	}
	return m_spGLView;
}

#else

NCryOpenGL::SOutputMergerView* CCryDXGLRenderTargetView::GetGLView()
{
	return m_spGLView;
}

#endif

////////////////////////////////////////////////////////////////
// Implementation of ID3D11RenderTargetView
////////////////////////////////////////////////////////////////

void CCryDXGLRenderTargetView::GetDesc(D3D11_RENDER_TARGET_VIEW_DESC* pDesc)
{
	(*pDesc) = m_kDesc;
}
