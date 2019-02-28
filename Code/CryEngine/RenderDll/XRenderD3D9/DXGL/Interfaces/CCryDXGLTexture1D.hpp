// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLTexture1D.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Declaration of the DXGL wrapper for ID3D11Texture1D
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGLTEXTURE1D__
#define __CRYDXGLTEXTURE1D__

#include "CCryDXGLTextureBase.hpp"

class CCryDXGLTexture1D : public CCryDXGLTextureBase
{
public:
	DXGL_IMPLEMENT_INTERFACE(CCryDXGLTexture1D, D3D11Texture1D)

	CCryDXGLTexture1D(const D3D11_TEXTURE1D_DESC& kDesc, NCryOpenGL::STexture* pGLTexture, CCryDXGLDevice* pDevice);
#if OGL_SINGLE_CONTEXT
	CCryDXGLTexture1D(const D3D11_TEXTURE1D_DESC& kDesc, NCryOpenGL::SInitialDataCopy* pInitialData, CCryDXGLDevice* pDevice);
#endif
	virtual ~CCryDXGLTexture1D();

#if OGL_SINGLE_CONTEXT
	virtual void Initialize();
#endif

	// Implementation of ID3D11Texture1D
	void GetDesc(D3D11_TEXTURE1D_DESC* pDesc);

#if !DXGL_FULL_EMULATION
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		if (SingleInterface<CCryDXGLTexture1D>::Query(this, riid, ppvObject))
			return S_OK;
		return CCryDXGLTextureBase::QueryInterface(riid, ppvObject);
	}
#endif //!DXGL_FULL_EMULATION
private:
	D3D11_TEXTURE1D_DESC m_kDesc;
};

#endif //__CRYDXGLTEXTURE1D__
