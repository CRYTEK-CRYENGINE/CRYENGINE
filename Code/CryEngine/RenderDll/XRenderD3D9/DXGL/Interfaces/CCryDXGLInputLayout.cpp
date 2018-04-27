// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLInputLayout.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11InputLayout
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLInputLayout.hpp"
#include "../Implementation/GLShader.hpp"

CCryDXGLInputLayout::CCryDXGLInputLayout(NCryOpenGL::SInputLayout* pGLLayout, CCryDXGLDevice* pDevice)
	: CCryDXGLDeviceChild(pDevice)
	, m_spGLLayout(pGLLayout)
{
	DXGL_INITIALIZE_INTERFACE(D3D11InputLayout)
}

CCryDXGLInputLayout::~CCryDXGLInputLayout()
{
}

NCryOpenGL::SInputLayout* CCryDXGLInputLayout::GetGLLayout()
{
	return m_spGLLayout;
}
