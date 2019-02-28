// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLRasterizerState.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11RasterizerState
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLRASTERIZERSTATE__
#define __CRYDXGLRASTERIZERSTATE__

#include "CCryDXGLDeviceChild.hpp"

namespace NCryOpenGL
{
struct SRasterizerState;
class CContext;
};

class CCryDXGLRasterizerState : public CCryDXGLDeviceChild
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLRasterizerState, D3D11RasterizerState)

	CCryDXGLRasterizerState(const D3D11_RASTERIZER_DESC& kDesc, CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLRasterizerState();

	bool Initialize(CCryDXGLDevice* pDevice, NCryOpenGL::CContext* pContext);
	bool Apply(NCryOpenGL::CContext* pContext);

	// Implementation of ID3D11RasterizerState
	void GetDesc(D3D11_RASTERIZER_DESC* pDesc);
protected:
	D3D11_RASTERIZER_DESC         m_kDesc;
	NCryOpenGL::SRasterizerState* m_pGLState;
};

#endif //__CRYDXGLRASTERIZERSTATE__
