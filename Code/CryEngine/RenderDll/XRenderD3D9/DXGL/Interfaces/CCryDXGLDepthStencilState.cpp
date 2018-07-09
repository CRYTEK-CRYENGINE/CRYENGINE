// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLDepthStencilState.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11DepthStencilState
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLDepthStencilState.hpp"
#include "CCryDXGLDevice.hpp"
#include "../Implementation/GLState.hpp"
#include "../Implementation/GLDevice.hpp"

CCryDXGLDepthStencilState::CCryDXGLDepthStencilState(const D3D11_DEPTH_STENCIL_DESC& kDesc, CCryDXGLDevice* pDevice)
	: CCryDXGLDeviceChild(pDevice)
	, m_kDesc(kDesc)
	, m_pGLState(new NCryOpenGL::SDepthStencilState)
{
	DXGL_INITIALIZE_INTERFACE(D3D11DepthStencilState)
}

CCryDXGLDepthStencilState::~CCryDXGLDepthStencilState()
{
	delete m_pGLState;
}

bool CCryDXGLDepthStencilState::Initialize(CCryDXGLDevice* pDevice, NCryOpenGL::CContext* pContext)
{
	return NCryOpenGL::InitializeDepthStencilState(m_kDesc, *m_pGLState, pContext);
}

bool CCryDXGLDepthStencilState::Apply(uint32 uStencilReference, NCryOpenGL::CContext* pContext)
{
	return pContext->SetDepthStencilState(*m_pGLState, static_cast<GLint>(uStencilReference));
}

////////////////////////////////////////////////////////////////////////////////
// Implementation of ID3D11DepthStencilState
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLDepthStencilState::GetDesc(D3D11_DEPTH_STENCIL_DESC* pDesc)
{
	(*pDesc) = m_kDesc;
}
