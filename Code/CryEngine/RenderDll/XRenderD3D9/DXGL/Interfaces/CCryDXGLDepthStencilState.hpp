// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLDepthStencilState.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11DepthStencilState
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLDEPTHSTENCILSTATE__
#define __CRYDXGLDEPTHSTENCILSTATE__

#include "CCryDXGLDeviceChild.hpp"

namespace NCryOpenGL
{
struct SDepthStencilState;
class CContext;
}

class CCryDXGLDepthStencilState : public CCryDXGLDeviceChild
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLDepthStencilState, D3D11DepthStencilState)

	CCryDXGLDepthStencilState(const D3D11_DEPTH_STENCIL_DESC& kDesc, CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLDepthStencilState();

	bool Initialize(CCryDXGLDevice* pDevice, NCryOpenGL::CContext* pContext);
	bool Apply(uint32 uStencilReference, NCryOpenGL::CContext* pContext);

	// Implementation of ID3D11DepthStencilState
	void GetDesc(D3D11_DEPTH_STENCIL_DESC* pDesc);
protected:
	D3D11_DEPTH_STENCIL_DESC        m_kDesc;
	NCryOpenGL::SDepthStencilState* m_pGLState;
};

#endif //__CRYDXGLDEPTHSTENCILSTATE__
