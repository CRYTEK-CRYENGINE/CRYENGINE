// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLSwitchToRef.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for ID3D11SwitchToRef
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLSwitchToRef.hpp"
#include "../Implementation/GLCommon.hpp"

CCryDXGLSwitchToRef::CCryDXGLSwitchToRef(CCryDXGLDevice* pDevice)
	: CCryDXGLDeviceChild(pDevice)
{
	DXGL_INITIALIZE_INTERFACE(D3D11SwitchToRef)
}

CCryDXGLSwitchToRef::~CCryDXGLSwitchToRef()
{
}

////////////////////////////////////////////////////////////////////////////////
// ID3D11SwitchToRef implementation
////////////////////////////////////////////////////////////////////////////////

BOOL CCryDXGLSwitchToRef::SetUseRef(BOOL UseRef)
{
	DXGL_NOT_IMPLEMENTED
	return false;
}

BOOL CCryDXGLSwitchToRef::GetUseRef()
{
	DXGL_NOT_IMPLEMENTED
	return false;
}
