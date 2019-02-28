// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLShader.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for D3D11 shader interfaces
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLShader.hpp"
#include "../Implementation/GLShader.hpp"

CCryDXGLShader::CCryDXGLShader(NCryOpenGL::SShader* pGLShader, CCryDXGLDevice* pDevice)
	: CCryDXGLDeviceChild(pDevice)
	, m_spGLShader(pGLShader)
{
}

CCryDXGLShader::~CCryDXGLShader()
{
}

NCryOpenGL::SShader* CCryDXGLShader::GetGLShader()
{
	return m_spGLShader.get();
}
