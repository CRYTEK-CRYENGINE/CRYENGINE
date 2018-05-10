// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLBuffer.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11Buffer
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLBUFFER__
#define __CRYDXGLBUFFER__

#include "CCryDXGLResource.hpp"

namespace NCryOpenGL
{
struct SBuffer;
}

class CCryDXGLBuffer : public CCryDXGLResource
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLBuffer, D3D11Buffer)

	CCryDXGLBuffer(const D3D11_BUFFER_DESC& kDesc, NCryOpenGL::SBuffer* pGLBuffer, CCryDXGLDevice* pDevice);
#if OGL_SINGLE_CONTEXT
	CCryDXGLBuffer(const D3D11_BUFFER_DESC& kDesc, NCryOpenGL::SInitialDataCopy* pInitialDataCopy, CCryDXGLDevice* pDevice);
#endif
	virtual ~CCryDXGLBuffer();

#if OGL_SINGLE_CONTEXT
	virtual void Initialize();
#endif

	NCryOpenGL::SBuffer* GetGLBuffer();

	// ID3D11Buffer implementation
	void GetDesc(D3D11_BUFFER_DESC* pDesc);
private:
	D3D11_BUFFER_DESC m_kDesc;
};

#endif //__CRYDXGLBUFFER__
