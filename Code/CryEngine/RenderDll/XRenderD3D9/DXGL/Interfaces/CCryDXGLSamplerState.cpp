// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLSamplerState.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11SamplerState
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLSamplerState.hpp"
#include "CCryDXGLDevice.hpp"
#include "../Implementation/GLState.hpp"
#include "../Implementation/GLDevice.hpp"

CCryDXGLSamplerState::CCryDXGLSamplerState(const D3D11_SAMPLER_DESC& kDesc, CCryDXGLDevice* pDevice)
	: CCryDXGLDeviceChild(pDevice)
	, m_kDesc(kDesc)
	, m_pGLState(NULL)
{
	DXGL_INITIALIZE_INTERFACE(D3D11SamplerState)
}

CCryDXGLSamplerState::~CCryDXGLSamplerState()
{
	delete m_pGLState;
}

bool CCryDXGLSamplerState::Initialize(NCryOpenGL::CContext* pContext)
{
	assert(m_pGLState == NULL);
	m_pGLState = new NCryOpenGL::SSamplerState;
	return NCryOpenGL::InitializeSamplerState(m_kDesc, *m_pGLState, pContext);
}

void CCryDXGLSamplerState::Apply(uint32 uStage, uint32 uSlot, NCryOpenGL::CContext* pContext)
{
	IF_UNLIKELY (!m_pGLState)
	{
		if (!Initialize(pContext))
			DXGL_FATAL("Deferred sampler state creation failed");
	}
	pContext->SetSampler(m_pGLState, uStage, uSlot);
}

////////////////////////////////////////////////////////////////
// Implementation of ID3D11SamplerState
////////////////////////////////////////////////////////////////

void CCryDXGLSamplerState::GetDesc(D3D11_SAMPLER_DESC* pDesc)
{
	(*pDesc) = m_kDesc;
}
