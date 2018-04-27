// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLRasterizerState.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11RasterizerState
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLRasterizerState.hpp"
#include "CCryDXGLDevice.hpp"
#include "../Implementation/GLState.hpp"
#include "../Implementation/GLDevice.hpp"

CCryDXGLRasterizerState::CCryDXGLRasterizerState(const D3D11_RASTERIZER_DESC& kDesc, CCryDXGLDevice* pDevice)
	: CCryDXGLDeviceChild(pDevice)
	, m_kDesc(kDesc)
	, m_pGLState(new NCryOpenGL::SRasterizerState)
{
	DXGL_INITIALIZE_INTERFACE(D3D11RasterizerState)
}

CCryDXGLRasterizerState::~CCryDXGLRasterizerState()
{
	delete m_pGLState;
}

bool CCryDXGLRasterizerState::Initialize(CCryDXGLDevice* pDevice, NCryOpenGL::CContext* pContext)
{
	return NCryOpenGL::InitializeRasterizerState(m_kDesc, *m_pGLState, pContext);
}

bool CCryDXGLRasterizerState::Apply(NCryOpenGL::CContext* pContext)
{
	return pContext->SetRasterizerState(*m_pGLState);
}

////////////////////////////////////////////////////////////////////////////////
// Implementation of ID3D11RasterizerState
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLRasterizerState::GetDesc(D3D11_RASTERIZER_DESC* pDesc)
{
	(*pDesc) = m_kDesc;
}
