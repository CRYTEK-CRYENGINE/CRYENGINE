// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLSwitchToRef.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11SwitchToRef
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLSWITCHTOREF__
#define __CRYDXGLSWITCHTOREF__

#include "CCryDXGLDeviceChild.hpp"

class CCryDXGLSwitchToRef : public CCryDXGLDeviceChild
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLSwitchToRef, D3D11SwitchToRef)

	CCryDXGLSwitchToRef(CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLSwitchToRef();

	// ID3D11SwitchToRef implementation
	BOOL SetUseRef(BOOL UseRef);
	BOOL GetUseRef();
};

#endif //__CRYDXGLSWITCHTOREF__
