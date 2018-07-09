// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLUnorderedAccessView.cpp
//  Version:     v1.00
//  Created:     18/06/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11UnorderedAccessView
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLUnorderedAccessView.hpp"
#include "CCryDXGLDevice.hpp"
#include "CCryDXGLResource.hpp"
#include "../Implementation/GLView.hpp"
#include "../Implementation/GLDevice.hpp"

CCryDXGLUnorderedAccessView::CCryDXGLUnorderedAccessView(CCryDXGLResource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC& kDesc, CCryDXGLDevice* pDevice)
	: CCryDXGLView(pResource, pDevice)
	, m_kDesc(kDesc)
{
	DXGL_INITIALIZE_INTERFACE(D3D11UnorderedAccessView)
}

CCryDXGLUnorderedAccessView::~CCryDXGLUnorderedAccessView()
{
}

bool CCryDXGLUnorderedAccessView::Initialize(NCryOpenGL::CContext* pContext)
{
	D3D11_RESOURCE_DIMENSION eDimension;
	m_spResource->GetType(&eDimension);
	m_spGLView = NCryOpenGL::CreateUnorderedAccessView(m_spResource->GetGLResource(), eDimension, m_kDesc, pContext);
	return m_spGLView != NULL;
}

#if OGL_SINGLE_CONTEXT

NCryOpenGL::SShaderView* CCryDXGLUnorderedAccessView::GetGLView(NCryOpenGL::CContext* pContext)
{
	IF_UNLIKELY (!m_spGLView)
	{
		if (!Initialize(pContext))
			DXGL_FATAL("Deferred unordered access view creation failed");
	}

	return m_spGLView;
}

#else

NCryOpenGL::SShaderView* CCryDXGLUnorderedAccessView::GetGLView()
{
	return m_spGLView;
}

#endif

////////////////////////////////////////////////////////////////
// Implementation of ID3D11UnorderedAccessView
////////////////////////////////////////////////////////////////

void CCryDXGLUnorderedAccessView::GetDesc(D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc)
{
	*pDesc = m_kDesc;
}
