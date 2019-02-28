// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLSamplerState.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11SamplerState
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLSAMPLERSTATE__
#define __CRYDXGLSAMPLERSTATE__

#include "CCryDXGLDeviceChild.hpp"

namespace NCryOpenGL
{
struct SSamplerState;
class CContext;
}

class CCryDXGLSamplerState : public CCryDXGLDeviceChild
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLSamplerState, D3D11SamplerState)

	CCryDXGLSamplerState(const D3D11_SAMPLER_DESC& kDesc, CCryDXGLDevice* pDevice);
	virtual ~CCryDXGLSamplerState();

	bool Initialize(NCryOpenGL::CContext* pContext);
	void Apply(uint32 uStage, uint32 uSlot, NCryOpenGL::CContext* pContext);

	// Implementation of ID3D11SamplerState
	void GetDesc(D3D11_SAMPLER_DESC* pDesc);
protected:
	D3D11_SAMPLER_DESC         m_kDesc;
	NCryOpenGL::SSamplerState* m_pGLState;
};

#endif //__CRYDXGLSAMPLERSTATE__
