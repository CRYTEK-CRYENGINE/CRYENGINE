// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLBlendState.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11BlendState
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLBlendState.hpp"
#include "CCryDXGLDevice.hpp"
#include "../Implementation/GLState.hpp"
#include "../Implementation/GLDevice.hpp"

CCryDXGLBlendState::CCryDXGLBlendState(const D3D11_BLEND_DESC& kDesc, CCryDXGLDevice* pDevice)
	: CCryDXGLDeviceChild(pDevice)
	, m_kDesc(kDesc)
	, m_pGLState(new NCryOpenGL::SBlendState)
{
	DXGL_INITIALIZE_INTERFACE(D3D11BlendState)
}

CCryDXGLBlendState::~CCryDXGLBlendState()
{
	delete m_pGLState;
}

bool CCryDXGLBlendState::Initialize(CCryDXGLDevice* pDevice, NCryOpenGL::CContext* pContext)
{
	return NCryOpenGL::InitializeBlendState(m_kDesc, *m_pGLState, pContext);
}

bool CCryDXGLBlendState::Apply(NCryOpenGL::CContext* pContext)
{
	return pContext->SetBlendState(*m_pGLState);
}

////////////////////////////////////////////////////////////////////////////////
// Implementation of ID3D11BlendState
////////////////////////////////////////////////////////////////////////////////

void CCryDXGLBlendState::GetDesc(D3D11_BLEND_DESC* pDesc)
{
	(*pDesc) = m_kDesc;
}
