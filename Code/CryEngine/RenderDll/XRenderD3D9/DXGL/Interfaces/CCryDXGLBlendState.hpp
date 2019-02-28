// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLBlendState.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11BlendState
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLBLENDSTATE__
#define __CRYDXGLBLENDSTATE__

#include "CCryDXGLDeviceChild.hpp"

namespace NCryOpenGL
{
struct SBlendState;
class CContext;
}

class CCryDXGLBlendState : public CCryDXGLDeviceChild
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLBlendState, D3D11BlendState)

	CCryDXGLBlendState(const D3D11_BLEND_DESC& kDesc, CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLBlendState();

	bool Initialize(CCryDXGLDevice* pDevice, NCryOpenGL::CContext* pContext);
	bool Apply(NCryOpenGL::CContext* pContext);

	// Implementation of ID3D11BlendState
	void GetDesc(D3D11_BLEND_DESC* pDesc);
protected:
	D3D11_BLEND_DESC         m_kDesc;
	NCryOpenGL::SBlendState* m_pGLState;
};

#endif //__CRYDXGLBLENDSTATE__
